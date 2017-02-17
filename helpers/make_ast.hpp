#ifndef ATL_HELPERS_MAKE_AST_HH
#define ATL_HELPERS_MAKE_AST_HH

#include <gc/gc.hpp>
#include <helpers/misc.hpp>
#include <helpers/itritrs.hpp>

/**
 * Make Asts along various patterns
 */

namespace atl
{
	namespace make_ast
	{
		template<class T,
		         bool is_atl=tmpl::Apply<is_atl_type,
		                                 tmpl::Apply<std::remove_pointer,
		                                             RemoveConstReference<T> >
		                                 >::type::value
		         >
		struct _TypeDispatched;

		/* _Run which automatically wraps a number and assumes a bare string is a sym. */
		struct _SRun
		{
			AstBuilder& space;
			_SRun(AstBuilder& ss) : space(ss) {}

			void operator()(std::string const& ss)
			{ space.push_back(wrap(space._pool->store.raw_make<Symbol>(ss))); }

			void operator()(char const* ss)
			{ space.push_back(wrap(space._pool->store.raw_make<Symbol>(std::string(ss)))); }

			void operator()(long ss)
			{ space.push_back(wrap<Fixnum>(ss)); }

			void operator()(int ss)
			{ space.push_back(wrap<Fixnum>(ss)); }

			void operator()(Any const& value) { space.push_back(value); }

			template<class Thing>
			void operator()(Thing const& thing) { _TypeDispatched<Thing>::a(*this, thing); }
		};

		template<class Type>
		struct _TypeDispatched <Type, true>
		{
			static void a(_SRun& runner, Type const& thing)
			{ runner(wrap(thing)); }
		};

		template<class Fn>
		struct _TypeDispatched <Fn, false>
		{
			static void a(_SRun& runner, Fn const& fn)
			{ fn(runner.space); }
		};


		template<class ... Args>
		ast_composer mk(Args ... args)
		{
			auto tup = std::make_tuple(args...);
			return [tup](AstBuilder& space) -> void
				{
					NestAst nested(space);

					_SRun do_apply(space);
					foreach_tuple(do_apply, tup);
				};
		}
	}

	/* higher order functions for Asts */
	namespace ast_hof
	{
		/** Builds a new Ast in `store` by mapping `fn` over `ast`
		 * @tparam Fn: -> Any Any
		 * @param store: an AstBuilder
		 * @param fn: the Fn to apply
		 * @param ast: the ast to apply over
		 * @return: A MovableAstPointer which will be updated if store
		 *   is resized.
		 */
		template<class Fn>
		ast_composer map(Fn&& fn, Ast const& input)
		{
			return [fn, input](AstBuilder& store) -> void
				{
					NestAst nest(store);
					for(auto vv : input)
						{ store.push_back(fn(vv)); }
				};
		}


		ast_composer copy(Ast::Subex const& input)
		{
			return [input](AstBuilder& store) -> void
				{
					NestAst nest(store);
					for(auto& itr : itritrs(input))
						{
							if(itr.is<Ast>())
								{ copy(itr.subex())(store); }
							else
								{ store.push_back(*itr); }
						}
				};
		}
	}

	namespace fn_type
	{
		using make_type::tt;

		/** Make a function declartion like (-> a (-> b c)) from arguments like (a b c).
		 */
		struct FnBuilder
		{
			AstBuilder& space;
			std::vector<NestAst> nesting;

			bool is_last;

			FnBuilder(AstBuilder& ss)
				: space(ss)
				, is_last(false)
			{}

			void _push_back(Any& tt)
			{
				switch(tt._tag)
   				{
				case tag<AstData>::value:
					throw WrongTypeError
						("Can't FnBuilder._push_back a raw AstData; Ast's memory might move.");
				case tag<Ast>::value:
					ast_hof::copy(unwrap<Ast>(tt).subex())(space);
					break;
				default:
					space.push_back(tt);
				}
			}

			void _nest()
			{
				nesting.push_back(NestAst(space));
				space.push_back(wrap<Type>(tag<FunctionConstructor>::value));
			}

			void last_arg(Any& type)
			{ _push_back(type); }

			void other_arg(Any& type)
			{
				_nest();
				_push_back(type);
			}

			void arg_type(Any type)
			{
				if(is_last)
					{ last_arg(type); }
				else
					{ other_arg(type); }
			}

			/* Note: this is useful for the "shape_check" used in testing, but won't produce a proper type. */
			void operator()(std::string const& ss)
			{ arg_type(wrap(space._pool->store.raw_make<Symbol>(ss))); }

			void operator()(long ss)
			{ arg_type(wrap<Type>(ss)); }

			void operator()(unsigned long ss)
			{ arg_type(wrap<Type>(ss)); }

			void operator()(int ss)
			{ arg_type(wrap<Type>(ss)); }

			void operator()(Any& value) { arg_type(value); }

			void operator()(Marked<Ast>& ast)
			{
				if(!is_last) { _nest(); }
				// assume it's a nested function:
				ast_hof::copy(ast->subex())(space);
			}

			void operator()(ast_composer const& fn) {
				if(!is_last)
					{ _nest(); }

				fn(space);
			}

			Ast root() { return space.root(); }
		};

		template<typename Tuple,
		         size_t size = std::tuple_size<Tuple>::value>
		struct _DispatchRunner
		{
			static Ast a(Tuple& tup, AstBuilder& space)
			{
				FnBuilder runner(space);

				ForeachTuple<Tuple, 0, std::tuple_size<Tuple>::value - 2>::apply(runner, tup);

				runner(std::get<std::tuple_size<Tuple>::value - 2>(tup));

				runner.is_last = true;
				runner(std::get<std::tuple_size<Tuple>::value - 1>(tup));

				return runner.root();
			}
		};

		// thunk:
		template<class Tuple>
		struct _DispatchRunner<Tuple, 1>
		{
			static Ast a(Tuple& tup, AstBuilder& space)
			{
				FnBuilder runner(space);
				runner(std::get<0>(tup));
				return runner.root();
			}
		};

		template<class ... Args>
		ast_composer fn(Args ... args)
		{
			typedef std::tuple<Args...> Tuple;

			auto tup = Tuple(std::forward<Args>(args)...);

			return [tup](AstBuilder& space) mutable -> void
				{
					static_assert(std::tuple_size<Tuple>::value > 0,
					              "Some arguments required");
					_DispatchRunner<Tuple>::a(tup, space);
				};
		}
	}
}

#endif
