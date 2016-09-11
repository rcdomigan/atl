#ifndef ATL_HELPERS_HH
#define ATL_HELPERS_HH

#include <algorithm>

#include "./gc.hpp"
#include "./type.hpp"
#include "./conversion.hpp"
#include "./utility.hpp"
#include "./pass_value.hpp"
#include "./type_testing.hpp"

namespace atl
{
	typedef Range<Any*> AnyRange;

	namespace byte_code
	{
		typedef typename vm_stack::value_type value_type;
		template<class T>
		vm_stack::value_type to_bytes(T input)
		{ return reinterpret_cast<vm_stack::value_type>(input); }

		// TODO: use the `std::is_integral` and static cast for all integral (and floating?) types.
		value_type to_bytes(long input)
		{ return static_cast<value_type>(input); }

		value_type to_bytes(bool input)
		{ return static_cast<value_type>(input); }

		value_type to_bytes(void* input)
		{ return reinterpret_cast<value_type>(input); }

		value_type to_bytes(Pointer input)
		{ return reinterpret_cast<value_type>(input.value); }

		template<class R>
		struct PntrCaster
		{
			typedef PntrCaster<R> type;
			static R a(value_type input)
			{ return reinterpret_cast<R>(input); }
		};

		template<class I>
		struct StaticCaster
		{
			typedef StaticCaster<I> type;
			static I a(value_type input)
			{ return static_cast<I>(input); }
		};


		template<class T>
		struct Caster
			: public std::conditional<std::is_integral<T>::value,
			                          StaticCaster<T>,
			                          PntrCaster<T>
			                          >::type
		{};

		template<class R>
		R from_bytes(value_type input) { return Caster<R>::a(input); }
	}

	namespace make_ast
	{
		struct AstAllocator
		{
			AllocatorBase &allocator;
			AstSubstrate &buffer;

			AstAllocator(AllocatorBase &aa)
				: allocator(aa), buffer(aa.sequence())
			{}

			Symbol* symbol(std::string const& name)
			{ return allocator.symbol(name); }

			AstAllocator& push_back(Any value)
			{
				buffer.push_back(value);
				return *this;
			}

			AstAllocator& push_back(PassByValue value)
			{
				buffer.push_back(value.any);
				return *this;
			}

			MovableAstPointer nest_ast()
			{ return push_nested_ast(buffer); }

			size_t size()
			{ return buffer.size(); }
		};

		struct NestAst
		{
			AstAllocator& store;
			MovableAstPointer ast;

			NestAst(AstAllocator& store_)
				: store(store_), ast(store.nest_ast())
			{}

			~NestAst() { ast.end_ast(); }
		};

		AstAllocator ast_alloc(AllocatorBase& aa)
		{ return AstAllocator(aa); }

		typedef std::function<void (AstAllocator)> ast_composer;

		/* _Run which automatically wraps a number and assumes a bare string is a sym. */
		struct _SRun
		{
			AstAllocator space;
			_SRun(AstAllocator ss) : space(ss) {}

			void operator()(std::string const& ss)
			{ space.push_back(wrap(space.symbol(ss))); }

			void operator()(char const* ss)
			{ space.push_back(wrap(space.symbol(std::string(ss)))); }

			void operator()(long ss)
			{ space.push_back(wrap<Fixnum>(ss)); }

			void operator()(int ss)
			{ space.push_back(wrap<Fixnum>(ss)); }

			void operator()(Any const& value) { space.push_back(value); }

			template<class Fn>
			void operator()(Fn &fn) { fn(space); }
		};

		template<class ... Args>
		std::function<Ast (AstAllocator)>
		mk(Args ... args)
		{
			auto tup = make_tuple(args...);
			return [tup](AstAllocator space) -> Ast
				{
					NestAst nested(space);

					_SRun do_apply(space);
					foreach_tuple(do_apply, tup);

					return *nested.ast;
				};
		}
	}

	/* higher order functions for Asts */
	namespace ast_hof
	{
		using make_ast::AstAllocator;
		using make_ast::NestAst;

		/** Builds a new Ast in `store` by mapping `fn` over `ast`
		 * @tparam Fn: -> Any Any
		 * @param store: an AstAllocator
		 * @param fn: the Fn to apply
		 * @param ast: the ast to apply over
		 * @return: A MovableAstPointer which will be updated if store
		 *   is resized.
		 */
		template<class Fn>
		MovableAstPointer map(Fn&& fn, Slice const& input, AstAllocator store)
		{
			NestAst nest(store);
			for(auto& vv : input)
				{ nest.store.push_back(fn(vv)); }
			return nest.ast;
		}


		template<class Astish>
		MovableAstPointer copy(Astish const& input, AstAllocator store)
		{
			NestAst nest(store);
			for(auto& vv : input)
				{
					auto value = pass_value(vv);
					if(is<Slice>(value))
						{
							copy(value.slice,
							     store);
						}
					else
						{ nest.store.push_back(vv); }
				}
			return nest.ast;
		}
	}

	void copy_into(PassByValue input, ast_hof::AstAllocator store)
	{
		if(is<Slice>(input))
			{ ast_hof::copy(unwrap<Slice>(input), store); }
		else
			{ store.push_back(input); }
	}

	/* Pass through a Slice or wrap an ast or AstData */
	Slice unwrap_slice(Any& input)
	{
		switch(input._tag)
			{
			case tag<AstData>::value:
				{
					auto& ast = explicit_unwrap<AstData>(input);
					return Slice(ast.flat_begin(), ast.flat_end());
				}
			case tag<Ast>::value:
				{
					auto& ast = explicit_unwrap<Ast>(input);
					return Slice(ast.flat_begin(), ast.flat_end());
				}
			case tag<Slice>::value:
				{
					return unwrap<Slice>(input);
				}
			default:
				throw WrongTypeError
					("unwrap_slice can only deal with Ast-ish objects!");
			}
	}

	namespace make_type
	{
		/** Make a Type with T's tag.  Not much to it, but used in a bunch of tests.
		 * @tparam T: the type whose tag to use
		 * @return: a wraped Type
		 */
		template<class T>
		Any tt() { return wrap<Type>(tag<T>::value); }
	}



	namespace fn_type
	{
		using make_ast::AstAllocator;
		using make_ast::NestAst;
		using make_ast::ast_alloc;
		using make_ast::ast_composer;
		using make_type::tt;

		/** Make a function declartion like (-> a (-> b c)) from arguments like (a b c).
		 */

		struct FnBuilder
		{
			AstAllocator space;
			std::vector<NestAst> nesting;

			bool is_last;

			FnBuilder(AstAllocator ss)
				: space(ss)
				, is_last(false)
			{}

			void _push_back(Any const& tt)
			{
				switch(tt._tag)
					{
					case tag<Slice>::value:
					case tag<Ast>::value:
					case tag<AstData>::value:
						{
							// assume it's a nested function:
							ast_hof::copy(pass_value(tt).slice,
							              space);
							break;
						}
					default:
						space.push_back(tt);
					}
			}

			void _nest()
			{
				nesting.emplace_back(space);
				space.push_back(wrap<Type>(tag<FunctionConstructor>::value));
			}

			void last_arg(Any const& type)
			{ _push_back(type); }

			void other_arg(Any const& type)
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
			{ arg_type(wrap(space.symbol(ss))); }

			void operator()(long ss)
			{ arg_type(wrap<Type>(ss)); }

			void operator()(unsigned long ss)
			{ arg_type(wrap<Type>(ss)); }

			void operator()(int ss)
			{ arg_type(wrap<Type>(ss)); }

			void operator()(Any const& value) { arg_type(value); }

			void operator()(ast_composer const& fn)
			{
				if(!is_last)
					{ _nest(); }

				fn(space);
			}

			Ast root() { return Ast(&reinterpret_cast<AstData&>(space.buffer.root())); }
		};

		template<typename Tuple,
		         size_t size = std::tuple_size<Tuple>::value>
		struct _DispatchRunner
		{
			static Ast a(Tuple const& tup, AstAllocator& space)
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
			static Ast a(Tuple const& tup, AstAllocator& space)
			{
				FnBuilder runner(space);
				runner(get<0>(tup));
				return runner.root();
			}
		};

		template<class ... Args>
		std::function<Ast (make_ast::AstAllocator)>
		fn(Args ... args)
		{
			typedef std::tuple<Args...> Tuple;

			auto tup = Tuple(std::forward<Args>(args)...);

			return [tup](AstAllocator space) -> Ast
				{
					static_assert(std::tuple_size<Tuple>::value > 0,
					              "Some arguments required");
					return _DispatchRunner<Tuple>::a(tup, space);
				};
		}
	}


	struct AstSubscripter
	{
		PassByValue value;
		AstSubscripter(PassByValue const& in) : value(in) {}
		AstSubscripter(Ast& value_) : value(pass_value(value_)) {}
		AstSubscripter() = delete;

		AstSubscripter operator[](off_t pos)
		{ return AstSubscripter(pass_value(unwrap<Slice>(value)[pos])); }
	};


	AstSubscripter subscripter(PassByValue const& value)
	{ return AstSubscripter(value); }

	template<class T>
	AstSubscripter subscripter(T&& ast)
	{ return AstSubscripter(pass_value(ast)); }


	struct RefSubscripter
	{
		Any *value;
		RefSubscripter(Any& value_) : value(&value_) {}
		RefSubscripter(Ast& value_) : value(reinterpret_cast<Any*>(&value_)) {}

		RefSubscripter() = delete;

		RefSubscripter operator[](off_t pos)
		{
			switch(value->_tag)
				{
				case tag<Ast>::value:
					return RefSubscripter(explicit_unwrap<Ast>(*value)[pos]);

				case tag<AstData>::value:
					return RefSubscripter(explicit_unwrap<AstData>(*value)[pos]);

				case tag<Slice>::value:
					return RefSubscripter(unwrap<Slice>(*value)[pos]);

				default:
					throw WrongTypeError("Can't subscript given type");
				}
		}

		template<class T>
		T& a()
		{ return unwrap<T>(*value); }
	};


	template<class T>
	struct Unwrapped
	{
		template<class Input, class Fn>
		static void a(Input&& in, Fn fn)
		{ return fn(unwrap<T>(in)); };
	};

	Type typify(Type const& input)
	{ return input; }

	Type typify(Any const& input)
	{
		if(is<Type>(input)) { return unwrap<Type>(input); }
		return Type(input._tag);
	}

	Type typify(PassByValue const& input)
	{
		if(is<Type>(input)) { return unwrap<Type>(input); }
		return Type(input._tag);
	}

	template<class T>
	Type typify(T const& input)
	{
		static_assert(is_atl_type<T>::value,
		              "Can only typify atl types.");
		return Type(input._tag);
	}

	template<class Tree>
	typename std::enable_if<(std::is_same<Tree, Ast>::value
	                         || std::is_same<Tree, Slice>::value),
	                        bool>::type
	operator!=(Tree const& aa_tree, Tree const& bb_tree)
	{ return !(aa_tree == bb_tree); }

	template<class Tree>
	typename std::enable_if<(std::is_same<Tree, Ast>::value
	                         || std::is_same<Tree, Slice>::value),
	                        bool>::type
	operator==(Tree const& aa_tree, Tree const& bb_tree)
	{
		if(aa_tree.flat_size() != bb_tree.flat_size())
			{ return false; }

		auto aa = aa_tree.begin(),
			bb = bb_tree.begin();
		while((aa != aa_tree.end()) && (bb != bb_tree.end()))
			{
				if(aa->_tag != bb->_tag)
					{ return false; }

				switch(aa->_tag)
					{
					case tag<AstData>::value:
						{
							if(Ast(const_cast<AstData*>(reinterpret_cast<AstData const*>(aa.value)))
							   != Ast(const_cast<AstData*>(reinterpret_cast<AstData const*>(bb.value))))
								{ return false; }
							break;
						}
					case tag<Ast>::value:
						{
							if(reinterpret_cast<Ast const&>(aa)
							   != reinterpret_cast<Ast const&>(bb))
								{ return false; }
							break;
						}
					case tag<Slice>::value:
						{
							if(*reinterpret_cast<Slice const*>(aa.value)
							   != *reinterpret_cast<Slice const*>(bb.value))
								{ return false; }
							break;
						}
					case tag<Symbol>::value:
						{
							if(reinterpret_cast<Symbol*>(aa->value)->name
							   != reinterpret_cast<Symbol*>(bb->value)->name)
								{ return false; }
							break;
						}
					default:
						if(*bb != *aa)
							{ return false; }
					}

				++bb;
				++aa;
			}

		if((bb != bb_tree.end()) || (aa != aa_tree.end()))
			{ return false; }
		return true;
	}

	/* For Slices this means copying into an Ast, otherwise pass through the
	   existing value.  */
	Any de_slice(make_ast::AstAllocator store,
	             PassByValue value)
	{
		if(is<Slice>(value))
			{
				return wrap(*ast_hof::copy(unwrap<Slice>(value),
				                           store));
			}
		else
			{ return value.any; }
	}
}

#endif
