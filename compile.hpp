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
	struct Compile
	{
		typedef AssembleCode::const_iterator iterator;
		typedef pcode::Offset Offset;

		Code code_store;
		AssembleCode assemble;

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

			Context just_closure()
			{ return Context(closure, false); }

			Context(LambdaMetadata *cc, bool t)
				: closure(cc)
				, tail(t)
			{}
		};

		/// \internal Take an input and generate byte-code.
		///
		/// @param input: the thing to compile
		/// @param assemble: the "AssembleCode" helper we're using
		/// @param context: relavent context, ie are we in a tail call
		/// @return: Type information and other things a calling _compile needs to know about.
		Any _compile(Any input, Context context)
		{
			using namespace std;

			switch(input._tag) {
			case tag<Ast>::value:
				{
					auto ast = unwrap<Ast>(input);
					auto head = ast[0];

					/******************/
					/* special forms: */
                    /******************/
					switch(head._tag)
						{
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
								_compile(ast[2], context);

								auto after_alt = will_jump();
								assemble.jump();

								// alternate
								assemble[alt_address] = assemble.pos_end();
								_compile(ast[3], context);

								assemble[after_alt] = assemble.pos_end();

								return wrap<Null>();
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
											_compile(value,
											         Context(context.closure, true));
											assemble.define(sym.name);
										}
								}

								assemble.add_label(sym.name);

								return wrap<Null>();
							}
						case tag<Lambda>::value:
							{
								auto& metadata = *unwrap<Lambda>(head).value;
								{
									SkipBlock my_def(assemble);

									metadata.body_address = assemble.pos_end();
									_compile(ast[2], Context(&metadata,
									                         true));
									assemble.return_();
								}

								assemble.constant(metadata.body_address);

								for(auto item : metadata.closure)
									{ _compile(item->value, context.just_closure()); }

								assemble.make_closure(metadata.formals.size(),
								                      metadata.closure.size());

								return head;
							}
						case tag<Quote>::value:
							{
								// TODO: copy?  Not sure how to handle this with GC.
								assemble.pointer(ast.address(1));
								return wrap<Null>();
							}
						}

					/*****************/
					/* normal order: */
                    /*****************/
					// Compile the args:
					for(auto vv : slice(ast, 1)) { _compile(vv, context.just_closure()); }

					switch(head._tag)
						{
						case tag<Ast>::value:
						{
							// The Ast must return a closure.
							// TODO: wrap primitive functions in a
							// closure if they're getting returned

							_compile(head, context.just_closure());
							assemble.call_closure();
							return wrap<Null>();
						}
						case tag<CxxFunctor>::value:
							{
								auto& fn = unwrap<CxxFunctor>(head);
								assemble.std_function(&fn.fn, fn.arity);
								return wrap<Null>();
							}
						case tag<Symbol>::value:
							{
								auto& sym = unwrap<Symbol>(head);
								assemble
									.deref_slot(sym.name)
									.call_closure();
								return sym.value;
							}
						default:
							throw WrongTypeError
								(std::string("Dunno how to use ")
								 .append(type_name(head))
								 .append(" as a function"));
						}

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

			case tag<Parameter>::value:
				{
					assemble.argument
						(context.closure->formals.size() - 1 - unwrap<Parameter>(input).value);
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

		void compile(Ast const& ast)
		{ _compile(wrap(ast), Context(nullptr, false)); }

		void compile(Marked<Ast>&& ast)
		{
			auto marked = std::move(ast);
			_compile(marked.any, Context(nullptr, false));
		}

		void compile(Marked<Ast>& ast)
		{ _compile(ast.any, Context(nullptr, false)); }


		// For most external use.  The generated code can be passed to
		// the VM for evaluation.
		void compile(Any ast)
		{ _compile(ast, Context(nullptr, false)); }

		void dbg();
	};

	void Compile::dbg()
	{
		auto printer = CodePrinter(code_store);
		printer.dbg();
	}
}


#endif
