/**
 * @file /home/ryan/programming/atl/main.cc
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jul 01, 2013
 */

#include "./gc.hpp"
#include "./parser.hpp"
#include "./encode.hpp"
#include "./evaluator.hpp"
#include "./primitive_callable.hpp"
#include "./print.hpp"

#include "./debug.hpp"

#include "./gc.cpp"
#include <fstream>

using namespace atl;
using namespace std;

void verbose_eval(Any a, GC& gc, EncodeAST& encode, EvaluateAST& vm) {
    cout << " main: parsed: " << printer::any(a) << endl;
    auto ee = encode.any(a);
    cout << " main: encoded: " << printer::any(ee) << endl;
    cout << printer::any(vm.any(ee)) << endl;
}

int main(int argc, char *argv[]) {
    Any a;

    ifstream in_file;
    in_file.open("prelim.atl");

    GC gc;
    ParseString parse(gc);
    Environment env(gc);
    EncodeAST encode(env, gc);
    EvaluateAST vm(env);

    setup_interpreter(&gc, &env, &encode, &parse, &vm);

    /* while( in_file ) { */
    /*   a = p(in_file); */
    /*   vm(encode(a)); */
    /* } */

    while( true )
	verbose_eval(parse.stream(cin),
		     gc, encode, vm);

    return 0;
}
