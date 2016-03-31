#ifndef TESTING_UTILS_HPP
#define TESTING_UTILS_HPP
/**
 * @file /home/ryan/programming/atl/test/testing_utils.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on May 19, 2015
 */

#include <gc.hpp>
#include <ffi.hpp>
#include "../compile.hpp"


long add2(long a, long b) { return a + b; }
long sub2(long a, long b) { return a - b; }
bool equal2(long a, long b) { return a == b; }


struct TrivialFunctions
{
	atl::Arena gc;
    atl::CxxFunctor *weq, *wadd, *wsub;

    TrivialFunctions()
        : gc(),
          weq(atl::WrapStdFunction<bool (long, long)>::a(equal2, gc)),
          wadd(atl::WrapStdFunction<long (long, long)>::a(add2, gc)),
          wsub(atl::WrapStdFunction<long (long, long)>::a(sub2, gc))
    {}
};

/** Run a block of code
 * @param vm: The vm to run on
 * @param code: The code to run
 */
void run_code(atl::TinyVM& vm, atl::Code& code)
{
#ifdef DEBUGGING
	code.dbg();
	vm.run_debug(code, 100);
#else
    vm.run(code);
#endif
}

/** Run some code
 * @param asmbl: Code is passed out of this AssembleCode then passed
 *               back in when the function finishes.
 */
void run_code(atl::TinyVM& vm, atl::AssembleCode& asmbl)
{
	auto code = asmbl.pass_code_out();
	run_code(vm, *code);
    asmbl.take_code(code);
}

/**
 * @param compiler: Code is passed out of compiler and run in vm, when
 * the function finishes the code is passed back to compiler.
 */
void run_code(atl::TinyVM& vm, atl::Compile& compiler)
{
	auto code = compiler.pass_code_out();
	run_code(vm, *code);
	compiler.take_code(code);
}



#endif
