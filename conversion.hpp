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

#include <boost/mpl/at.hpp>
#include <boost/mpl/map.hpp>


namespace atl {
    namespace unwrapping {
	template<class T>
	struct Pimpl {
	    static_assert(type_mapping::unwrappable_type<T>::value, "Cannot unwrap Any to type T");
	    static inline constexpr T& a(atl::Any& aa) {return *reinterpret_cast<T*>(aa.value);}
	    static inline constexpr T const& a(atl::Any const& aa) {return *reinterpret_cast<T*>(aa.value);}};

	template<class T>
	struct Reinterpret {
	    static_assert(type_mapping::unwrappable_type<T>::value, "Cannot unwrap Any to type T");
	    static inline constexpr T& a(atl::Any& aa) {return reinterpret_cast<T&>(aa);}
	    static inline constexpr T const& a(atl::Any const& aa) {return reinterpret_cast<T const&>(aa);}};

	template<class T>
	struct Any
	    : public std::conditional< is_reinterperable<typename std::decay<T>::type>::value,
				       Reinterpret<T>,
                                       Pimpl<T>
				       >::type {};
    }

    template<class T>
    static inline T& unwrap(Any& input) { return unwrapping::Any<T>::a(input); }

    template<class T>
    static inline T const& unwrap(Any const& input) { return unwrapping::Any<T>::a(input); }

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
	    static inline constexpr Any a(T const& aa) { return Any(aa._tag, (void*)aa.value); }};

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
                                       Pimpl<T>
				       >::type::type {};
    }
    template<class T> static inline constexpr Any wrap(T* vv) { return Any(tag<T>::value, vv); }

    template<class T>
    static inline constexpr Any wrap(T const& vv) {
	return wrapping::Any<typename type_mapping::cxx_to_atl<T>::type>::a(vv);
    }


    static inline constexpr Any wrap(Any* vv) { return Any(tag<Pointer>::value, vv); }
    static inline constexpr Any wrap(Any const& vv) { return vv; }


    static inline Any wrap(const Fixnum& vv) {
	return Any(tag<Fixnum>::value,
		   (void*)vv.value);
    }


    namespace get_value {
        template <typename Seq, typename Key, typename Def>
        struct AtDef
            : std::conditional<mpl::has_key<Seq, Key>::value,
                               mpl::at<Seq, Key>,
                               mpl::identity<Def>
                               >::type {};


        typedef mpl::map<mpl::pair<bool, Bool>
                         , mpl::pair<long, Fixnum>
                         > CxxToAtlType;

        // const in c++ sucks.
        struct GetConstValue {
            template<class T, class Input>
            struct apply {
                typedef typename std::remove_reference<typename std::remove_pointer<T>::type
                                                       >::type U;

                typedef typename AtDef<CxxToAtlType, U, T>::type Mapped;

                typedef decltype(Mapped::value) Return;

                static Return a(Input input) {
                    return unwrap<Mapped>(input).value;
                }
            };};

        struct GetRefValue {
            template<class T, class Input>
            struct apply {
                typedef typename AtDef<CxxToAtlType, T, T>::type Mapped;

                typedef decltype(Mapped::value)& Return;

                static Return a(Input input) {
                    return unwrap<Mapped>(input).value;
                }
            };};

        // is the object referenced or pointed to const?
        template<class T>
        struct IsConstObject
            : std::is_const<typename std::remove_pointer<typename std::remove_reference<T>::type>::type
                            >::type {};

        template<class T, class Input>
        struct GetValue
            : std::conditional< IsConstObject<T>::value || IsConstObject<Input>::value,
                                GetConstValue,
                                GetRefValue
                                >::type::template apply<T, Input> {};
    }

    template<class T, class Input>
    typename get_value::GetValue<T, Input>::Return
    value(Input&& input) {
        return get_value::GetValue<T, Input>::a(input);
    }
}

#endif
