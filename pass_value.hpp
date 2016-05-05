#ifndef ATL_PASS_VALUE_HPP
#define ATL_PASS_VALUE_HPP

namespace atl
{
	/*** Reference an Any:
	 *
	 * - For Asts, store the most generic form (Slice) so that
     *   functions dealing with PassByValue do not have to dispatch
     *   Ast, AstData, and Slice seperatly.
	 *
     * - For atoms, keep a pointer to the original atom so it can be
     *   modified in place.  This makes PassByValue is safe to
     *   pass-by-value, thus the name, but take care when returnign a
     *   PassByValue since it may be pointing to an Any which
     *   existsts on stack.
	 */
	struct PassByValue
	{
		tag_t _tag;

		Slice slice;
		Any any;

		template<class Astish>
		void _store_ast(Astish& input)
		{
			_tag = tag<Slice>::value;
			slice = Slice(input);
			any = wrap(&slice);
		}

		void _store_slice(Slice const& input)
		{
			_tag = tag<Slice>::value;
			slice = input;
			any = wrap(&slice);
		}

		template<class Thing>
		typename std::enable_if<std::is_same<
			                        Any,
			                        typename std::remove_const<Thing>::type
			                        >::type::value
		                        >::type
		_store_value(Thing& input)
		{
			switch(input._tag)
				{
				case tag<AstData>::value:
					_store_ast(explicit_unwrap<AstData>(input));
					break;
				case tag<Ast>::value:
					_store_ast(explicit_unwrap<Ast>(input));
					break;
				case tag<Slice>::value:
					_store_slice(unwrap<Slice>(input));
					break;
				default:
					{
						_tag = input._tag;
						any = input;
						break;
					}
				}
		}

		explicit PassByValue(Any& input)
		{ _store_value(input); }

		explicit PassByValue(Any const& input)
		{ _store_value(input); }

		explicit PassByValue(Ast& input)
		{ _store_ast(input); }

		explicit PassByValue(AstData& input)
		{ _store_ast(input); }

		explicit PassByValue(Slice const& input)
		{ _store_slice(input); }

		explicit PassByValue(tag_t tag_, void* value_)
			: PassByValue(Any(tag_, value_))
		{}

		PassByValue() = default;

		PassByValue& operator=(Any& input)
		{
			_store_value(input);
			return *this;
		}

		bool operator==(PassByValue const& other) const
		{ return any == other.any; }
	};

	PassByValue pass_value(Slice const& input)
	{ return PassByValue(input); }

	PassByValue pass_value(Slice& input)
	{ return PassByValue(input); }

	PassByValue pass_value(Slice&& input)
	{ return PassByValue(input); }

	PassByValue pass_value(Any&& input)
	{ return PassByValue(input); }

	PassByValue pass_value(Any& input)
	{ return PassByValue(input); }

	PassByValue pass_value(Any const& input)
	{ return PassByValue(input); }

	PassByValue pass_value(Ast& input)
	{ return PassByValue(input); }

	PassByValue pass_value(AstData& input)
	{ return PassByValue(input); }

	template<class T>
	PassByValue pass_value(T&& input)
	{
		typedef typename tmpl::Apply<std::decay,
		                             std::remove_pointer<T> >::type Type;
		static_assert(!(tag<Type>::value == tag<AstData>::value),
		              "Use overloads for AstData");
		static_assert(!(tag<Type>::value == tag<Any>::value),
		              "Use the overloads for Any");
		return PassByValue(tag<Type>::value, wrap(input).value);
	}

	template<class T>
	PassByValue pass_value()
	{ return PassByValue(tag<T>::value, nullptr); }

	namespace unwrap_PBV
	{
		template<class T>
		struct Unwrap
		{
			static_assert(!(std::is_same<AstData, T>::value || std::is_same<Ast, T>::value),
			              "PassByValue uses a Slice for all Ast ranges.");

			static inline constexpr T& a(atl::PassByValue& aa)
			{ return explicit_unwrap<T>(aa.any); }

			static inline constexpr T const& a(atl::PassByValue const& aa)
			{ return explicit_unwrap<T>(aa.any); }
		};

		template<>
		struct Unwrap<Slice>
		{
			static inline constexpr Slice& a(atl::PassByValue& aa)
			{ return aa.slice; }

			static inline constexpr Slice const& a(atl::PassByValue const& aa)
			{ return aa.slice; }
		};
	}

	template<class T>
	T& unwrap(PassByValue& input)
	{ return unwrap_PBV::Unwrap<T>::a(input); }

	template<class T>
	T const& unwrap(PassByValue const& input)
	{ return unwrap_PBV::Unwrap<T>::a(input); }

	Slice unwrap_slice(PassByValue const& input)
	{ return input.slice; }

	template<class T>
	bool is(PassByValue const& value)
	{
		static_assert((tag<T>::value != tag<Ast>::value) && (tag<T>::value != tag<AstData>::value),
		              "PassByValue coerces all Ast types to Slice!");
		return is<T>(reinterpret_cast<Any const&>(value));
	}
}


#endif
