#ifndef ATL_HELPERS_HH
#define ATL_HELPERS_HH

#include <algorithm>

#include "./gc.hpp"
#include "./type.hpp"
#include "./conversion.hpp"

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
		// Maintains one 'dynamic_seq` for use with the make_ast functions
		struct AstAllocator
		{
			AllocatorBase &allocator;
			AstSubstrate *seq_space;

			AstAllocator(AllocatorBase &aa)
				: allocator(aa), seq_space(&aa.sequence())
			{}

			Symbol* symbol(std::string const& name)
			{ return allocator.symbol(name); }

			AstAllocator& push_back(Any value)
			{
				seq_space->push_back(value);
				return *this;
			}

			MovableAstPointer nest_ast()
			{ return push_nested_ast(*seq_space); }

			size_t size()
			{ return seq_space->size(); }
		};

		AstAllocator ast_alloc(AllocatorBase& aa)
		{ return AstAllocator(aa); }

		typedef std::function<void (AstAllocator)> ast_composer;
		template<class T>
		ast_composer lift(T tt)
		{
			return [tt](AstAllocator space)
				{ space.push_back(wrap(tt)); };
		}

		template<class T>
		ast_composer lift()
		{
			return [](AstAllocator space)
				{ space.push_back(wrap<T>()); };
		}

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
					// ast_pos needs to be positional!! ITERATOR WON'T WORK!
					auto ast = space.nest_ast();
					_Run do_apply(space);

					foreach_tuple(do_apply, tup);
					ast.end_ast();
					return *ast;
				};
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
				{
				return Match(is<T>(expr));
				};
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
							ast = &unwrap<AstData>(expr);
							break;
						case tag<Ast>::value:
							ast = unwrap<Ast>(expr).value;
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

	/// Work-alike wrapper for Any so which is safe to pass around by
	/// value (ie wrap AstData with an Ast).
	struct PassByValue
	{
		tag_t _tag;
		void* value;

		PassByValue& _store_value(Any& input)
		{
			if(is<AstData>(input))
				{
					_tag = tag<Ast>::value;
					value = &input;
				}
			else
				{
					_tag = input._tag;
					value = input.value;
				}
			return *this;
		}

		PassByValue(tag_t tt, void* vv)
			: _tag(tt), value(vv)
		{}

		PassByValue(Any& input)
		{ _store_value(input); }

		PassByValue(Ast& input)
		{
			_tag = tag<Ast>::value;
			value = input.value;
		}

		PassByValue(AstData& input)
		{
			_tag = tag<Ast>::value;
			value = &input;
		}

		PassByValue() = default;

		PassByValue& operator=(Any& input)
		{
			_store_value(input);
			return *this;
		}

		Any& as_Any() { return *reinterpret_cast<Any*>(this); }
		Any as_Any() const { return *reinterpret_cast<Any const*>(this); }
	};

	/// If we have an Any which we know will not be an AstData, we can
	/// use this function to wrap it without any type dispatch.
	///
	/// @param input: An Any that the caller promises is not an AstData
	PassByValue pass_safe_value(Any input)
	{ return PassByValue(input._tag, input.value); }

	template<class T>
	PassByValue pass_value()
	{ return PassByValue(tag<T>::value, nullptr); }

	namespace unwrap_PBV
	{
		template<class T>
		struct Unwrap
		{
			static_assert(!std::is_same<AstData, T>::value,
			              "PassByValue should not directly contain an AstData.");

			static inline constexpr T& a(atl::PassByValue& aa)
			{ return unwrap<T>(aa.as_Any()); }

			static inline constexpr T const& a(atl::PassByValue const& aa)
			{ return unwrap<T>(aa.as_Any()); }
		};
	}

	template<class T>
	T& unwrap(PassByValue& input)
	{ return unwrap_PBV::Unwrap<T>::a(input); }

	template<class T>
	T unwrap(PassByValue const& input)
	{ return unwrap_PBV::Unwrap<T>::a(input); }

	template<class T>
	bool is(PassByValue const& value)
	{ return is<T>(reinterpret_cast<Any const&>(value)); }


	/* Pass through an ast or wrap an AstData */
	Ast pass_ast(Any& input)
	{
		if(is<AstData>(input))
			return Ast(&unwrap<AstData>(input));
		else
			return unwrap<Ast>(input);
	}


	struct AstSubscriter
	{
		PassByValue value;
		AstSubscriter(PassByValue const& in) : value(in) {}
		AstSubscriter() = delete;

		AstSubscriter operator[](off_t pos)
		{ return AstSubscriter(pass_value(unwrap<Ast>(value)[pos])); }
	};

	template<class T>
	AstSubscriter subscripter(T&& ast)
	{ return AstSubscriter(pass_value(ast)); }

	template<class T>
	AstSubscriter subscripter(PassByValue const& value)
	{ return AstSubscriter(value); }


	template<class T>
	struct Unwrapped
	{
		template<class Input, class Fn>
		static void a(Input&& in, Fn fn)
		{ return fn(unwrap<T>(in)); };
	};
}

#endif
