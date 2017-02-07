#ifndef ATL_HELPERS_MISC_HPP
#define ATL_HELPERS_MISC_HPP

#include <utility.hpp>
#include <type.hpp>
#include <type_testing.hpp>
#include <conversion.hpp>

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

	Type typify(Type const& input)
	{ return input; }

	Type typify(Any const& input)
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

	template<class T>
	Type typify()
	{
		static_assert(is_atl_type<T>::value,
		              "Can only typify atl types.");
		return Type(tag<T>::value);
	}

	/** Assign `sym` a constant `value`. */
	void symbol_assign(Symbol& sym, Any value)
	{
		sym.subtype = Symbol::Subtype::constant;
		switch(value._tag)
			{
			case tag<CxxFunctor>::value:
				{
					auto fn = unwrap<CxxFunctor>(value);
					sym.scheme.quantified.clear();
					sym.scheme.type = wrap(fn.type);
					break;
				}

			case tag<Ast>::value:
				break;

			default:
				{
					sym.scheme.quantified.clear();
					sym.scheme.type = wrap<Type>(value._tag);
					break;
				}
			}

		sym.value = value;
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
}

#endif
