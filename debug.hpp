#ifndef DEBUG_HH
#define DEBUG_HH
/**
 * @file /home/ryan/programming/atl/debug.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jan 05, 2014
 *
 * Some explicit versions of template functions, for use while debugging
 */

#include <limits>
#include <iostream>

#include "./type.hpp"
#include "./conversion.hpp"
#include "./type_testing.hpp"

namespace atl {
    // Pair* Any2Pair(Any a) { return unwrap<Pair*>(a); }

    // Any Pointer2Any(Any a) { return *unwrap<Any*>(a); }

    bool isNull(Any a) { return is<Null>(a); }

    template<class Int>
    void print_binary(Int num) {
	static const Int last_digit = static_cast<Int>(1) << (std::numeric_limits<Int>::digits - 1);

	for(unsigned int i = 0; i < static_cast<unsigned int>( std::numeric_limits<Int>::digits ); ++i) {
	    std::cout<<( (num & last_digit) != 0 );
	    //std::cout<<field<<std::endl;
	    num <<= 1;
	}
	std::cout << std::flush;
    }

    void print_binary_long(long long num) { print_binary(num); }
}


#endif
