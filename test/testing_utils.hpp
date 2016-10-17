#ifndef TESTING_UTILS_HPP
#define TESTING_UTILS_HPP
/**
 * @file /home/ryan/programming/atl/test/testing_utils.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on May 19, 2015
 */

#include <vm.hpp>

/** Run some code
 * @param code: Code is 'finished' then run
 */
void run_code(atl::TinyVM& vm, atl::Code& code)
{
	using namespace atl;
	AssembleCode(&code).finish();

#ifdef DEBUGGING
	auto printer = CodePrinter(code);
	printer.dbg();
	vm.run_debug(code, 100);
#else
    vm.run(code);
#endif

	code.code.pop_back();
}

#endif
