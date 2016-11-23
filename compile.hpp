#ifndef ATL_BYTE_COMPILE_HPP
#define ATL_BYTE_COMPILE_HPP
/**
 * @file /home/ryan/programming/atl/compile.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Nov 22, 2014
 */

#include "./exception.hpp"
#include "./vm.hpp"
#include "./byte_code.hpp"
#include "./type.hpp"
#include "./utility.hpp"
#include "./helpers/pattern_match.hpp"

#include <set>

namespace atl
{
	struct PatchingAssembler
		: public AssembleCode
	{
		typedef std::map<Symbol*, std::vector<pcode::Offset> > Backpatches;
		Backpatches backpatches;

		PatchingAssembler() = default;
		PatchingAssembler(WrapCodeForAssembler *code_) : AssembleCode(code_) {}

		PatchingAssembler& needs_patching(Symbol& sym) override
		{
			auto position = pos_end();
			auto patch = backpatches.find(&sym);
			if(patch == backpatches.end())
				{ backpatches[&sym] = std::vector<pcode::Offset>{position}; }
			else
				{ patch->second.push_back(position); }

			if(sym.scheme.is_function())
				{ call_procedure(0xdeadbeef); }
			else
				{ constant(0xfeed); }
			return *this;
		}

		PatchingAssembler& patch(Symbol& sym) override
		{
			auto patch = backpatches.find(&sym);
			if(patch != backpatches.end())
				{
					WrapCodeItr wrapped_code(code->begin(),
					                         code->end());
					PatchingAssembler assemble(&wrapped_code);
					for(auto pos : patch->second)
						{
							wrapped_code.itr = wrapped_code._begin + pos;
							if(is<CallLambda>(sym.value))
								{ assemble.call_procedure
										(unwrap<CallLambda>(sym.value).value->body_address); }
							else
								{ throw WrongTypeError("Can only backpatch functions"); }
						}
					backpatches.erase(patch);
				}
			return *this;
		}
	};

	struct Compile
	{
		typedef AssembleCode::const_iterator iterator;
		typedef pcode::Offset Offset;

		Code code_store;
		PatchingAssembler assemble;

		Compile()
			: assemble(&code_store)
		{}

		// Setup a VM jump instruction which skips over the code
		// defined during the SkipBlock instance's lifetime.
		struct SkipBlock
		{
			pcode::Offset _skip_to;
			AssembleCode& assemble;
			SkipBlock(AssembleCode& assemble_) : assemble(assemble_)
			{
				assemble.pointer(nullptr);
				_skip_to = assemble.pos_last();
				assemble.jump();
			}
			SkipBlock(SkipBlock&& steal) : assemble(steal.assemble)
			{ _skip_to = steal._skip_to; }
			SkipBlock(SkipBlock const&) = delete;

			~SkipBlock()
			{ assemble[_skip_to] = assemble.pos_end(); }
		};


		struct Context
		{
			LambdaMetadata *closure;
			bool tail;
			bool definition;

			Context just_closure()
			{ return Context(closure, false, false); }

			Context(LambdaMetadata *cc, bool t, bool d)
				: closure(cc)
				, tail(t)
				, definition(d)
			{}
		};

		// done            : The form has been evaluated, _compile can return
		// function        : The form was a function, _compile should call
		// declare_type    : The form declares the type of a nested form
		// macro_expansion : A new expression yielded by a macro
		enum class FormTag
		{ done, function, declare_type, macro_expansion };

		// Describe the thing-to-apply in an expression
		struct _Form
		{
			// result from 'form'
			Any applicable;
			FormTag form_tag;

			_Form(Any applicable_,
			      FormTag form_tag_)
				: applicable(applicable_),
				  form_tag(form_tag_)
			{}
		};


		/// \internal
		/// Evaluates the application position of an s-expression and
		/// returns a _Form structure with information for _compile.
		_Form form(Ast ast, Context context)
		{
			using namespace std;

			Any head = ast[0];

			auto compile_tail = [&](Any const& input)
				{
					return _compile(input, Context(context.closure, context.tail, false));
				};

			switch(head._tag)
				{
				case tag<CxxMacro>::value:
					return _Form(unwrap<CxxMacro>(head).fn(slice(ast, 1)),
					             FormTag::macro_expansion);

				case tag<Lambda>::value:
					{
						auto& metadata = *unwrap<Lambda>(head).value;

						auto compile_body = [&]()
							{
								metadata.body_address = assemble.pos_end();
								auto comp_val = _compile(ast[2],
								                         Context(&metadata,
								                                 true,
								                                 context.definition));

								assemble.return_(metadata.pad_to);
								return comp_val;
							};

						if(context.definition)
							{ compile_body(); }
						else
							{
								SkipBlock my_def(assemble);
								compile_body();
							}

						return _Form(wrap<CallLambda>(&metadata),
						             FormTag::done);
					}
				case tag<Quote>::value:
					{
						// TODO: copy?  Not sure how to handle this with GC.
						assemble.pointer(ast.address(1));
						return _Form(wrap<Null>(),
						             FormTag::done);
					}
				case tag<DeclareType>::value:
					{
						Compile::any(ast[1]);
						return _Form(wrap<Null>(),
						             FormTag::declare_type);
					}
				case tag<If>::value:
					{
						auto will_jump = [&]() -> pcode::Offset {
							assemble.pointer(nullptr);
							return assemble.pos_last();
						};

						auto alt_address = will_jump();
						_compile(ast[1], context.just_closure()); // get the predicate
						assemble.if_();

						// consiquent
						compile_tail(ast[2]);

						auto after_alt = will_jump();
						assemble.jump();

						// alternate
						assemble[alt_address] = assemble.pos_end();
						compile_tail(ast[3]);

						assemble[after_alt] = assemble.pos_end();

						return _Form(wrap<Null>(), FormTag::done);
					}
				case tag<Ast>::value:
					{
						auto compiled = _compile(head, context.just_closure());
						return _Form(compiled, FormTag::function);
					}
				case tag<Define>::value:
					{
						auto& sym = modify<Symbol>(ast[1]);
						auto const& value = ast[2];

						// Just assign the function location if
						// required; for other cases symbol's value
						// should have been set by assign_free
						{
							using namespace pattern_match;
							Lambda func(nullptr);
							if(match(pattern_match::ast(capture(func), tag<Ast>::value, tag<Ast>::value),
							         value))
								{
									auto& metadata = *func.value;

									_compile(value,
									         Context(context.closure, true, true));

									sym.value = wrap<CallLambda>(&metadata);
								}
						}

						assemble.add_label(sym.name);
						assemble.patch(sym);

						return _Form(wrap<Null>(),
						             FormTag::done);
					}

				case tag<CallLambda>::value:
				case tag<CxxFunctor>::value:
					{ return _Form(head, FormTag::function); }

				case tag<Symbol>::value:
					{
						auto& sym = unwrap<Symbol>(head);

						if(is<CallLambda>(sym.value))
							{
								return _Form(sym.value,
								             FormTag::function);
							}

						throw WrongTypeError("Symbol found which is not part of a closure or recursive definition");
					}
				default:
					throw WrongTypeError
						(std::string("Dunno how to use ")
						 .append(type_name(head))
						 .append(" as a function"));
				}
		}

		/// \internal Take an input and generate byte-code.
		///
		/// @param input: the thing to compile
		/// @param assemble: the "AssembleCode" helper we're using
		/// @param context: relavent context, ie are we in a tail call
		/// @return: Type information and other things a calling _compile needs to know about.
		Any _compile(Any input, Context context)
		{
			using namespace std;

		compile_value:
			switch(input._tag) {
			case tag<Ast>::value:
				{
					auto ast = unwrap<Ast>(input);
					auto form = this->form(ast, context);

					switch(form.form_tag) {
					case FormTag::done:
						return form.applicable;

					case FormTag::macro_expansion:
						{
							input = form.applicable;
							goto compile_value;
						}
					case FormTag::declare_type:
						{
							return _compile(ast[2], context);
						}
					case FormTag::function:
						{
							auto fn = form.applicable;
							auto rest = slice(ast, 1);

							auto is_procedure = is<CallLambda>(fn);
							LambdaMetadata* metadata = nullptr;

							if(is_procedure)
								{
									metadata = unwrap<CallLambda>(fn).value;

									// pad out so a tail call can use this call's frame
									auto padding = metadata->pad_to - rest.size();
									for(size_t i=0; i < padding; ++i)
										{ assemble.constant(8); }
								}

							// Compile the args:
							for(auto vv : rest)
								{ _compile(vv, context.just_closure()); }

							if(is_procedure)
								{
									if(context.tail)
										{ assemble.tail_call(metadata->pad_to,
										                     metadata->body_address); }
									else
										{ assemble.call_procedure(metadata->body_address); }
								}
							else
								{ _compile(form.applicable, context.just_closure()); }

							// Todo: need to gather tail info for
							// return type of function returning
							// function...
							return wrap<Null>();
						}}
					assert(0);
				}

			case tag<Fixnum>::value:
				assemble.constant(unwrap<Fixnum>(input).value);
				return wrap<Null>();

			case tag<Bool>::value:
				assemble.constant(unwrap<Bool>(input).value);
				return wrap<Null>();

			case tag<Pointer>::value:
				assemble.pointer(unwrap<Pointer>(input).value);
				return wrap<Null>();

			case tag<String>::value:
				assemble.pointer(&unwrap<String>(input));
				return wrap<Null>();

			case tag<CxxFunctor>::value:
				{
					auto& fn = unwrap<CxxFunctor>(input);
					assemble.std_function(&fn.fn, fn.arity);
					return wrap<Null>();
				}
			case tag<CallLambda>::value:
				{
					auto& metadata = *unwrap<CallLambda>(input).value;

					if(metadata.closure.empty())
						{ assemble.call_procedure(metadata.body_address); }
					else
						{
							assemble.call_closure();
						}
					return input;
				}
			case tag<Parameter>::value:
				{
					/* The frame we're computing from comes after the parameters on the stack */
					assemble.argument
						(context.closure->formals.size() - unwrap<Parameter>(input).value - 1);
					return wrap<Null>();
				}
			case tag<ClosureParameter>::value:
				{
					assemble.closure_argument(unwrap<ClosureParameter>(input).value);
					return wrap<Null>();
				}
			default:
				{
					throw std::string("Illegal syntax or something; got ")
						.append(type_name(input._tag))
						.append(" where value was required.");
				}
			}
			return wrap<Null>();
		}

		void ast(Ast const& ast)
		{ _compile(ast, Context(nullptr, false, false)); }

		// For most external use.  The generated code can be passed to
		// the VM for evaluation.
		void any(Any ast)
		{
			value(ast);
		}

		// For most external use.  The generated code can be passed to
		// the VM for evaluation.
		void value(Any ast)
		{ _compile(ast, Context(nullptr, false, false)); }

		void dbg();
	};

	void Compile::dbg()
	{
		auto printer = CodePrinter(code_store);
		printer.dbg();
	}
}


#endif
