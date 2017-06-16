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
#include "./helpers/itritrs.hpp"

#include <set>

namespace atl
{
	struct Compile
	{
		typedef AssembleCode::const_iterator iterator;
		typedef pcode::Offset Offset;

		GC& gc;
		Code code_store;
		AssembleCode assemble;

		Compile(GC &gc_)
			: gc(gc_),
			  assemble(&code_store)
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
		/// @param itr: the thing to compile
		/// @param context: relavent context, ie are we in a tail call
		void _compile(Any& any, Context context)
		{
			using namespace std;

			switch(any._tag) {
			case tag<AstData>::value:
			case tag<Ast>::value:
				{
					auto subex = atl::subex(any);
					auto inner = subex.begin();

					/******************/
					/* special forms: */
					/******************/
					switch(inner.tag())
						{
						case tag<If>::value:
							{
								auto will_jump = [&]() -> pcode::Offset {
									assemble.pointer(nullptr);
									return assemble.pos_last();
								};

								auto alt_address = will_jump();

								++inner;
								_compile(inner, context.just_closure()); // get the predicate
								assemble.if_();

								// consiquent
								++inner;
								_compile(inner, context);

								auto after_alt = will_jump();
								assemble.jump();

								// alternate
								++inner;
								assemble[alt_address] = assemble.pos_end();
								_compile(inner, context);

								assemble[after_alt] = assemble.pos_end();

								return;
							}
						case tag<Define>::value:
							{
								using namespace pattern_match;

								++inner;
								auto sym = unwrap<Symbol>(*inner);

								++inner;
								if(match(rest_begins(wrap<Type>(tag<FunctionConstructor>::value)),
								         sym.scheme.type))
									{
										_compile(inner, Context(context.closure, true));

										assemble.add_label(sym.name);
										assemble.define(sym.slot);
									}
								else
									{
										// need to backpatch atoms in the source
										throw CxxNotImplemented("No support for defining constants");
									}


								return;
							}
						case tag<Lambda>::value:
							{
								auto& metadata = *unwrap<Lambda>(*inner).value;
								{
									SkipBlock my_def(assemble);

									metadata.body_address = assemble.pos_end();

									++inner; // formals were processed in assign_free
									++inner;
									_compile(inner, Context(&metadata,
									                        true));
									assemble.return_();
								}

								assemble.constant(metadata.body_address);

								for(auto& var : metadata.closure)
									{ _compile(var, context.just_closure()); }

								assemble.make_closure(metadata.formals.size(),
								                      metadata.closure.size());

								return;
							}
						case tag<Quote>::value:
							{
								// TODO: copy?  Not sure how to handle this with GC.
								++inner;
								assemble.pointer(inner.pointer());
								return;
							}
						}

					/*****************/
					/* normal order: */
					/*****************/
					// Compile the args:
					size_t arg_count = 0;
					for(auto arg : slice(itritrs(subex), 1))
						{
							++arg_count;
							_compile(arg, context.just_closure());
						}

					switch(inner.tag())
						{
						case tag<Ast>::value:
							{
								// The Ast must return a closure.
								// TODO: wrap primitive functions in a
								// closure if they're getting returned

								_compile(inner, context.just_closure());
								assemble.call_closure();
								return;
							}
						case tag<CxxFunctor>::value:
							{
								auto& fn = unwrap<CxxFunctor>(*inner);

								if(!fn.variadic && (arg_count != fn.arity))
									{
										throw ArityError
											(std::string("Bad arity; expected ") +
											 std::to_string(fn.arity) +
											 std::string(" arguments, but got ") +
											 std::to_string(arg_count));
									}

								assemble.std_function(&fn.fn, arg_count);
								return;
							}
						case tag<Symbol>::value:
							{
								assemble
									.deref_slot(unwrap<Symbol>(*inner).slot)
									.call_closure();
								return;
							}
						default:
							throw WrongTypeError
								(std::string("Dunno how to use ")
								 .append(type_name(any))
								 .append(" as a function"));
						}

					assert(0);
				}

			case tag<Fixnum>::value:
				assemble.constant(unwrap<Fixnum>(any).value);
				return;

			case tag<Bool>::value:
				assemble.constant(unwrap<Bool>(any).value);
				return;

			case tag<Pointer>::value:
				assemble.pointer(unwrap<Pointer>(any).value);
				return;

			case tag<String>::value:
				assemble.pointer(&unwrap<String>(any));
				return;

			case tag<Parameter>::value:
				{
					assemble.argument
						(context.closure->formals.size() - 1 - unwrap<Parameter>(any).value);
					return;
				}
			case tag<ClosureParameter>::value:
				{
					assemble.closure_argument(unwrap<ClosureParameter>(any).value);
					return;
				}
			default:
				{
					throw std::string("Illegal syntax or something; got ")
						.append(type_name(any._tag))
						.append(" where value was required.");
				}
			}
			return;
		}

		void _compile(Ast::iterator& itr, Context context)
		{
			auto value = *itr;
			_compile(value, context);
		}

		void compile(Ast::iterator itr)
		{ _compile(itr, Context(nullptr, false)); }

		void compile(Ast& ast)
		{
			auto itr = ast.self_iterator();
			_compile(itr, Context(nullptr, false));
		}

		void compile(Marked<Ast>& ast)
		{ return compile(*ast); }

		void compile(Marked<Ast>&& ast)
		{ compile(ast); }

		void dbg();
	};

	void Compile::dbg()
	{
		auto printer = CodePrinter(code_store);
		printer.dbg();
	}
}


#endif
