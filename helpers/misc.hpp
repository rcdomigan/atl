#ifndef ATL_HELPERS_MISC_HPP
#define ATL_HELPERS_MISC_HPP

#include <atl/utility.hpp>
#include <atl/type.hpp>
#include <atl/is.hpp>
#include <atl/wrap.hpp>

namespace atl
{
	typedef Range<Any*> AnyRange;

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
