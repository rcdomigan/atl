#ifndef ATL_HELPERS_HH
#define ATL_HELPERS_HH

#include <algorithm>

#include "./gc.hpp"
#include "./type.hpp"
#include "./conversion.hpp"
#include "./pass_value.hpp"


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


	/* higher order functions for Asts */
	namespace ast_hof
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
				buffer.push_back(*value.any);
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

		MovableAstPointer copy(Slice const& input, AstAllocator store)
		{
			NestAst nest(store);
			for(auto& vv : input)
				{
					auto value = pass_value(vv);
					if(is<Slice>(vv))
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



	namespace make_ast
	{
		// Maintains one 'dynamic_seq` for use with the make_ast functions
		using ast_hof::AstAllocator;

		AstAllocator ast_alloc(AllocatorBase& aa)
		{ return AstAllocator(aa); }

		typedef std::function<void (AstAllocator)> ast_composer;

		ast_composer lift(Any tt)
		{
			return [tt](AstAllocator space)
				{ space.push_back(tt); };
		}

		template<class T, class ... Args>
		ast_composer lift(Args ... args)
		{
			return [args ...](AstAllocator space)
				{ space.push_back(wrap<T>(args ...)); };
		}

		/** Add a symbol value to the ast
		 *
		 * @param name: symbol's name
		 * @return: an ast_composer
		 */
		ast_composer sym(std::string const& name)
		{
			return [name](AstAllocator heap)
				{ heap.push_back(wrap(heap.symbol(name))); };
		}

		struct _Run
		{
			AstAllocator space;
			_Run(AstAllocator ss) : space(ss) {}

			template<class Fn>
			void operator()(Fn &fn) { fn(space); }
		};

		template<class ... Args>
		std::function<Ast (AstAllocator)>
		make(Args ... args)
		{
			auto tup = make_tuple(args...);
			return [tup](AstAllocator space) -> Ast
				{
					ast_hof::NestAst nested(space);

					_Run do_apply(space);
					foreach_tuple(do_apply, tup);

					return *nested.ast;
				};
		}

		template<class Fn>
		std::function<Ast (AstAllocator)>
		map(Fn&& fn, Ast const& input)
		{
			return [fn, input](AstAllocator store) -> Ast
				{ return ast_hof::map(fn, input, store); };
		}
	}

	namespace pattern_matcher
	{
		struct Match
		{
			bool is_match;
			typedef std::vector<Any*> Matches;
			Matches matches;

			Any& operator[](off_t pos) { return *matches[pos]; }
			operator bool() { return is_match; }

			Match(bool is_match_, Matches&& matches_)
				: is_match(is_match_), matches(matches_)
			{}

			Match(bool is_match_)
				: is_match(is_match_)
			{}

			Match(Match && other)
				: is_match(other.is_match), matches(std::move(other.matches))
			{}
		};

		typedef std::function<Match (Any&)> Matcher;

		template<class T>
		Matcher capture()
		{
			return [](Any& expr) -> Match
				{
					if(is<T>(expr))
						{ return Match(true, Match::Matches({&expr})); }
					else
						{ return Match(false); }
				};
		}

		Matcher capture_whatever()
		{
			return [](Any& expr) -> Match
				{
					return Match(true, Match::Matches({&expr}));
				};
		}

		template<class T>
		Matcher match_is()
		{
			return [](Any& expr) -> Match
				{ return Match(is<T>(expr)); };
		}

		Matcher whatever()
		{
			return [](Any& expr) -> Match
				{ return Match(true); };
		}

		template<class ... Args>
		Matcher match_ast(Args ... args)
		{
			auto tup = make_tuple(args...);
			return [tup](Any& expr) -> Match
				{
					AstData* ast;

					switch(expr._tag)
						{
						case tag<AstData>::value:
							ast = &explicit_unwrap<AstData>(expr);
							break;
						case tag<Ast>::value:
							ast = explicit_unwrap<Ast>(expr).value;
							break;
						default:
							return Match(false);
						}

					Match match(true);
					auto itr = ast->begin();

					auto accume = [&](Matcher const& fn) -> bool
						{
							if(itr == ast->end())
								{ return false; }

							auto inner_result = fn(*itr);
							if(inner_result.is_match)
								{
									++itr;
									match.matches.insert(match.matches.end(),
									                     inner_result.matches.begin(),
									                     inner_result.matches.end());
									return true;
								}
							else
								{ return false; }
						};

					match.is_match = and_tuple(accume, tup);
					if(itr != ast->end())
						{ return Match(false); }
					else
						{ return match; }
				};
		}
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


	struct AstSubscripter
	{
		PassByValue value;
		AstSubscripter(PassByValue const& in) : value(in) {}
		AstSubscripter() = delete;

		AstSubscripter operator[](off_t pos)
		{ return AstSubscripter(pass_value(unwrap<Slice>(value)[pos])); }
	};


	AstSubscripter subscripter(PassByValue const& value)
	{ return AstSubscripter(value); }

	template<class T>
	AstSubscripter subscripter(T&& ast)
	{ return AstSubscripter(pass_value(ast)); }


	template<class T>
	struct Unwrapped
	{
		template<class Input, class Fn>
		static void a(Input&& in, Fn fn)
		{ return fn(unwrap<T>(in)); };
	};
}

#endif
