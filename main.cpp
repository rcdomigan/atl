/**
 * @file /home/ryan/programming/atl/main.cc
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jul 01, 2013
 */

#include "./atl.hpp"

#include "./debug.hpp"

#include <fstream>

using namespace atl;
using namespace std;

int main(int argc, char *argv[]) {
    atl::Atl interpreter;

    while( true ) {
	interpreter.stream(cin);
        cout << interpreter.vm.stack[0] <<endl;
    }

    return 0;
}
