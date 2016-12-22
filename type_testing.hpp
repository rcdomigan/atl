#ifndef TYPE_TESTING_HH
#define TYPE_TESTING_HH
/**
 * @file /home/ryan/programming/atl/type_testing.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jul 26, 2013
 */

#include "./type.hpp"
#include <type_traits>

namespace atl
{
	/// Returns true if the input Any is tagged with T's type-tag.
    template<class T> bool is(const Any&);

    template<class T>
    struct Is_pointer_to {
        static constexpr bool do_it(const Any &a) {
            return is<Pointer>(a)
                && a.value
                && (*reinterpret_cast<unsigned int*>(a.value)  == tag<typename std::remove_pointer<T>::type >::value);
        }};

    template<class T>
    struct Is_immediate {
        static constexpr bool do_it(const Any &a) {
            return a._tag == tag<T>::value;
        }};

    template<class T> struct
    Is : public mpl::if_< std::is_pointer<T>
                          , Is_pointer_to<T>
                          , Is_immediate<T>
                          >::type
    { static_assert(is_atl_type<T>::value, "Can only test tags for ATL types."); };

	template<>
	struct Is<Any> { static constexpr bool do_it(const Any &_value) { return true; }};

    template<class T>
    inline bool is(const Any &a) { return Is<T>::do_it(a); }

    template<class T>
    inline bool is(tag_t tt) { return tt == tag<T>::value; }

    template<>
    bool is<Null>(const Any &a) {
        return a._tag == tag<Null>::value
            || (is<Pointer>(a) && (a.value == nullptr));
    }
}

#endif
