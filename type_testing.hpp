#ifndef TYPE_TESTING_HH
#define TYPE_TESTING_HH
/**
 * @file /home/ryan/programming/atl/type_testing.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jul 26, 2013
 */

#include "./type.hpp"
#include <type_traits>

namespace atl {
  template<class T> bool is(const Any&);

  template<class T>
  struct Is_pointer_to {
    static constexpr bool do_it(const Any &a) {
      return is<Pointer>(a)
	&& a._value
	  && (*reinterpret_cast<unsigned int*>(a._value)  == tag<typename std::remove_pointer<T>::type >::value);
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
			>::type {};

  template<class T>
  inline bool is(const Any &a) { return Is<T>::do_it(a); }

  template<>
  bool is<Null>(const Any &a) {
    return a._tag == tag<Null>::value
      || (is<Pointer>(a) && (a._value == nullptr));
  }
}

#endif
