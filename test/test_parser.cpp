
/**
 * @file /home/ryan/programming/atl/test/test_parser.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Dec 16, 2014
 */

#include <iostream>

#include "../gc.hpp"
#include "../parser.hpp"
#include "../encode.hpp"
#include "../primitive_callable.hpp"
#include "../helpers.hpp"
#include "../print.hpp"

#include "../debug.hpp"

using namespace atl;
using namespace std;

// Test basic parsing
int main(int argc, char *argv[]) {
    GC gc;
    Environment env(gc);
    ParseString parse(gc);
    EncodeAST encode(env);

    setup_interpreter(&gc, &env, &encode, &parse);

    auto& ast = parse.string_("(bin-add (#t 1 2) 3)");
    cout << "Parsed AST: " << printer::any(ast) << endl;

    encode.any(ast);
    cout << "Encoded AST: " << printer::any(ast) << endl;

    return 0;
}
