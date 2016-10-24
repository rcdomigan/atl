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

namespace atl {
    template<class Int>
    void print_binary(Int num)
    {
	    static const Int last_digit = static_cast<Int>(1) << (std::numeric_limits<Int>::digits - 1);

	    for(unsigned int i = 0; i < static_cast<unsigned int>( std::numeric_limits<Int>::digits ); ++i) {
		    std::cout<<( (num & last_digit) != 0 );
		    num <<= 1;
	    }
	    std::cout << std::flush;
    }

    void print_binary_long(long long num) { print_binary(num); }
}


#endif
