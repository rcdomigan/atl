#ifndef RHAS_TRIVIAL_FUNCTIONS_FOR_TEST
#define RHAS_TRIVIAL_FUNCTIONS_FOR_TEST

#include <atl/ffi.hpp>
#include <atl/lexical_environment.hpp>


long add2(long a, long b) { return a + b; }
long sub2(long a, long b) { return a - b; }
bool equal2(long a, long b) { return a == b; }
long add3(long a, long b, long c) { return a + b + c; }

namespace atl
{
	namespace unittest
	{
		struct TrivialFunctions
		{
			GC& gc;
			Marked<CxxFunctor> weq, wadd, wsub, wadd3;

			TrivialFunctions(GC& gc_) :
				gc(gc_),
				weq(WrapStdFunction<bool (long, long)>::a(equal2, gc)),
				wadd(WrapStdFunction<long (long, long)>::a(add2, gc)),
				wsub(WrapStdFunction<long (long, long)>::a(sub2, gc)),
				wadd3(WrapStdFunction<long (long, long, long)>::a(add3, gc))
			{}
		};

	}
}

#endif
