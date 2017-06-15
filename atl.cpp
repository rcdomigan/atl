/**
 * @file /home/ryan/programming/atl/main.cc
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jul 01, 2013
 */

#include "./atl.hpp"

#include "./debug.hpp"

#include <fstream>

using namespace atl;

int main(int argc, char *argv[]) {
    atl::Atl interpreter;

    while( true ) {
	    std::cout << "> ";
        auto rval = interpreter.eval(std::cin);
        std::cout << std::dec << printer::print(rval) << std::endl;
    }

    return 0;
}
