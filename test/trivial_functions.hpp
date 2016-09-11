#ifndef RHAS_TRIVIAL_FUNCTIONS_FOR_TEST
#define RHAS_TRIVIAL_FUNCTIONS_FOR_TEST

#include <ffi.hpp>


long add2(long a, long b) { return a + b; }
long sub2(long a, long b) { return a - b; }
bool equal2(long a, long b) { return a == b; }


struct TrivialFunctions
{
	atl::Arena store;
    atl::CxxFunctor *weq, *wadd, *wsub;

	TrivialFunctions() :
          weq(atl::WrapStdFunction<bool (long, long)>::a(equal2, store)),
          wadd(atl::WrapStdFunction<long (long, long)>::a(add2, store)),
          wsub(atl::WrapStdFunction<long (long, long)>::a(sub2, store))
    {}
};

#endif
