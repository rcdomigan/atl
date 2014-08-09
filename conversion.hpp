#ifndef ATL_CONVERSION_HH
#define ATL_CONVERSION_HH
/**
 * @file /home/ryan/programming/atl/conversion.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jul 10, 2013
 */

#include <string>
#include "./type.hpp"
#include "./type_traits.hpp"

namespace atl {
    namespace unwrapping {
	template<class T>
	struct Immediate {
	    static inline constexpr T& a(atl::Any& aa) {return reinterpret_cast<T&>(aa._value);}
	    static inline constexpr const T& a(const atl::Any& aa) {return reinterpret_cast<const T&>(aa._value);}};

	template<class T>
	struct Pimpl {
	    static_assert(type_mapping::unwrappable_type<T>::value, "Cannot unwrap Any to type T");
	    static inline constexpr T& a(atl::Any& aa) {return *reinterpret_cast<T*>(aa._value);}
	    static inline constexpr const T& a(const atl::Any& aa) {return *reinterpret_cast<T*>(aa._value);}};

	template<class T>
	struct Reinterpret {
	    static_assert(type_mapping::unwrappable_type<T>::value, "Cannot unwrap Any to type T");
	    static inline constexpr T& a(atl::Any& aa) {return reinterpret_cast<T&>(aa);}
	    static inline constexpr const T& a(const atl::Any& aa) {return reinterpret_cast<const T&>(aa);}};

	template<class T>
	struct Any
	    : public std::conditional< is_reinterperable<T>::value,
				       Identity< Reinterpret<T> >,
				       std::conditional<is_immediate<T>::value || std::is_integral<T>::value,
							Immediate<T>,
							Pimpl<T>
							>
				       >::type::type {};
    }

    template<class T>
    static inline T& unwrap(Any& input) { return unwrapping::Any<T>::a(input); }

    template<class T>
    static inline const T& unwrap(const Any& input) { return unwrapping::Any<T>::a(input); }

    /*********************************/
    /* __        __                  */
    /* \ \      / / __ __ _ _ __     */
    /*  \ \ /\ / / '__/ _` | '_ \    */
    /*   \ V  V /| | | (_| | |_) |   */
    /*    \_/\_/ |_|  \__,_| .__/    */
    /*                     |_|       */
    /*********************************/
    namespace wrapping {
	template<class T>
	struct Reinterpret {
	    static_assert(atl::is_reinterperable<T>::value, "Cannot wrap T");
	    static inline constexpr Any a(const T& aa) { return Any(aa._tag, (void*)aa._value); }};

	template<class T>
	struct Immediate {
	    template<class Value>
	    static inline constexpr Any a(const Value& aa) {return Any(tag<T>::value, (void*)aa);} };

	template<class T>
	struct Pimpl {
	    typedef typename std::remove_pointer<T>::type Bare;
	    static_assert(type_mapping::unwrappable_type<Bare>::value,
			  "Cannot wrap type T");
	    static inline constexpr Any a(T aa) {
		return Any(tag<Bare>::value, aa); }};

	template<class T>
	struct Any
	    : public std::conditional< is_reinterperable<T>::value,
				       Identity< Reinterpret<T> >,
				       std::conditional<is_immediate<T>::value || std::is_integral<T>::value,
							Immediate<T>,
							Pimpl<T>
							>
				       >::type::type {};
    }
    template<class T> static inline constexpr Any wrap(T* vv) { return Any(tag<T>::value, vv); }

    template<class T>
    static inline constexpr Any wrap(const T& vv) {
	return wrapping::Any<typename type_mapping::cxx_to_atl<T>::type>::a(vv);
    }


    static inline constexpr Any wrap(Any* vv) { return Any(tag<Pointer>::value, vv); }
    static inline constexpr Any wrap(const Any& vv) { return vv; }


    static inline Any wrap(const Fixnum& vv) {
	return Any(tag<Fixnum>::value,
		   (void*)vv._value);
    }
}

#endif
