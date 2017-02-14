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
#include <helpers/pattern_match.hpp>
#include <helpers/itritrs.hpp>

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
		void _compile(Ast::iterator& itr, Context context)
		{
			using namespace std;

			switch(itr.tag()) {
			case tag<Ast>::value:
				{
					auto subex = itr.subex();
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
								++inner;
								auto& sym = modify<Symbol>(*inner);

								// Just assign the function location if
								// required; for other cases symbol's value
								// should have been set by assign_free
								{
									using namespace pattern_match;
									Lambda func(nullptr);

									++inner;
									if(match(pattern_match::ast(capture(func), tag<Ast>::value, tag<Ast>::value),
									         inner))
										{
											_compile(inner, Context(context.closure, true));
											assemble.define(sym.name);
										}
								}

								assemble.add_label(sym.name);

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

								if(!metadata.closure.empty())
									{
										// The 'free' variables in our
										// closures are passed as
										// implicit arguments.  Make
										// the list of arguments so I
										// can compile those
										if(!metadata.has_closure_values)
											{
												auto builder = gc.ast_builder();
												NestAst nest(builder);
												for(auto sym : metadata.closure)
													{ builder.emplace_back(sym->value); }

												metadata.closure_values = builder.root();
												metadata.has_closure_values = true;
											}

										for(auto params_itr : itritrs(metadata.closure_values.subex()))
											{ _compile(params_itr, context.just_closure()); }
									}

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
					for(auto arg : slice(itritrs(subex), 1))
						{ _compile(arg, context.just_closure()); }

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
								assemble.std_function(&fn.fn, fn.arity);
								return;
							}
						case tag<Symbol>::value:
							{
								auto& sym = unwrap<Symbol>(*inner);
								assemble
									.deref_slot(sym.name)
									.call_closure();
								return;
							}
						default:
							throw WrongTypeError
								(std::string("Dunno how to use ")
								 .append(type_name(*itr))
								 .append(" as a function"));
						}

					assert(0);
				}

			case tag<Fixnum>::value:
				assemble.constant(unwrap<Fixnum>(*itr).value);
				return;

			case tag<Bool>::value:
				assemble.constant(unwrap<Bool>(*itr).value);
				return;

			case tag<Pointer>::value:
				assemble.pointer(unwrap<Pointer>(*itr).value);
				return;

			case tag<String>::value:
				assemble.pointer(&unwrap<String>(*itr));
				return;

			case tag<Parameter>::value:
				{
					assemble.argument
						(context.closure->formals.size() - 1 - unwrap<Parameter>(*itr).value);
					return;
				}
			case tag<ClosureParameter>::value:
				{
					assemble.closure_argument(unwrap<ClosureParameter>(*itr).value);
					return;
				}
			default:
				{
					throw std::string("Illegal syntax or something; got ")
						.append(type_name(itr->_tag))
						.append(" where value was required.");
				}
			}
			return;
		}

		void compile(Ast::iterator itr)
		{ _compile(itr, Context(nullptr, false)); }

		void compile(Marked<Ast>& ast)
		{
			auto itr = ast->self_iterator();
			_compile(itr, Context(nullptr, false));
		}

		void compile(Marked<Ast>&& ast)
		{
			auto marked = std::move(ast);
			compile(marked);
		}

		void dbg();
	};

	void Compile::dbg()
	{
		auto printer = CodePrinter(code_store);
		printer.dbg();
	}
}


#endif
