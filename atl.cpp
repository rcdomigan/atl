/**
 * @file /home/ryan/programming/atl/main.cc
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jul 01, 2013
 */

#include "./atl.hpp"
#include "./primitive_callable.hpp"

#include "./debug.hpp"

#include <fstream>

using namespace atl;

int main(int argc, char *argv[]) {
    atl::Atl interpreter;
    export_primitives(interpreter);

    auto parser = Parser(interpreter.gc, std::cin);

    while( true ) {
	    std::cout << "> ";
	    auto value = parser.parse();
	    std::cout << "Gonna eval: " << printer::print(*value) << std::endl;
	    auto rval = interpreter.eval(*value);
        std::cout << std::dec << printer::print(rval) << std::endl;
    }

    return 0;
}
