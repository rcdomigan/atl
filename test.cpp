
/**
 * @file /home/ryan/programming/atl/main.cc
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jul 01, 2013
 */

#include <fstream>
#include <iostream>

#include "./gc.hpp"
#include "./parser.hpp"
#include "./encode.hpp"
#include "./evaluator.hpp"
#include "./primitive_callable.hpp"
#include "./helpers.hpp"
#include "./print.hpp"

#include "./debug.hpp"

#include "./gc.cpp"

// #include "gtest/gtest.h"

using namespace atl;
using namespace std;

// int main(int argc, char *argv[]) {
//     ifstream in_file;
//     in_file.open("prelim.atl");

//     GC gc;
//     ParseString parse(gc);
//     EncodeAST encode(gc);
//     evaluateast vm(encode);

//     export_recursive_defs(&gc, &encode, &parse, &vm);

//     auto& ast = *encode(*parse("(seq-to-ast (Seq 1 2 3))"));

//     cout << "AST: " << printer::range(ast) << endl;

//     auto result = vm(ast);

//     cout << "Result: " << printer::any(result) << endl;
//     cout << " value: " << printer::range(unwrap<Ast>(*unwrap<Pointer>(result)._value)) << endl;

//     return 0;
// }

void print_backtracks(Any uu) {
    for(auto vv : unwrap<Undefined>(uu)._backtrack)
	cout << vv << endl;
}

int main() {
    GC gc;
    ParseString parse(gc);
    Environment env(gc);
    EncodeAST encode(env, gc);
    EvaluateAST vm(env);

    setup_interpreter(&gc, &env, &encode, &parse, &vm);

    //auto ast = encode.any(parse.string("(foo 1 foo 2)"));
    //auto ast = parse.string("(foo 1 foo 2)");
    auto ast = parse.stream(std::cin);
    std::cout << printer::debug_range(ast) << endl;
    
    ast = encode.any(ast);
    std::cout << printer::debug_range(ast) << endl;

    // std::cout << printer::debug_range(ast) << endl;

    // auto uu = unwrap<Ast>(ast)[0];
    // print_backtracks(uu);

    // auto ast2 = deep_copy::ast_from(ast, gc);
    // print_backtracks(uu);

    return 0;
}


/* Test the Ast */
// int main() {
//     GC gc;
//     ParseString parse(gc);
//     EncodeAST encode(gc);

//     auto& ast = *parse("((3) 4)");

//     cout << printer::range(ast) << endl;
//     cout << printer::any(ast[0]) << endl;
//     cout << printer::any(ast[1]) << endl;
//     cout << "nested array[0]: " << printer::any(unwrap<Ast>(ast[1])[0]) << endl;

//     return 0;
// }

