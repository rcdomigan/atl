#ifndef TESTING_UTILS_HPP
#define TESTING_UTILS_HPP
/**
 * @file /home/ryan/programming/atl/test/testing_utils.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on May 19, 2015
 */

#include <gc.hpp>
#include <ffi.hpp>


long add2(long a, long b) { return a + b; }
long sub2(long a, long b) { return a - b; }
bool equal2(long a, long b) { return a == b; }

struct TrivialGC
{
    template<class T, class ... Types>
    T* make(Types ... args)
    { return new T(args...); }
};

struct TrivialFunctions
{
    TrivialGC gc;
    atl::CxxFunctor *weq, *wadd, *wsub;

    TrivialFunctions()
        : gc(),
          weq(atl::WrapStdFunction<bool (long, long)>::a(equal2, gc)),
          wadd(atl::WrapStdFunction<long (long, long)>::a(add2, gc)),
          wsub(atl::WrapStdFunction<long (long, long)>::a(sub2, gc))
    {}
};

void run_code(atl::TinyVM& vm, atl::AssembleCode& input)
{
	atl::RunnableCode code(input);
#ifdef DEBUGGING
	atl::dbg_code(*input.output);
	vm.run_debug(code, 100);
#else
    vm.run(code);
#endif
}


#endif
