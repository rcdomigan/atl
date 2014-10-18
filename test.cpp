/**
 * @File /Home/ryan/programming/atl/main.cc
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

// Test basic parsing
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

// Test parsing and conversion between sequential data types and Ast
int main() {
    GC gc;
    ParseString parser(gc);
    Environment env(gc);
    EncodeAST encode(env, gc);
    EvaluateAST vm(env);

    setup_interpreter(&gc, &env, &encode, &parser, &vm);

    auto parse = [&](std::string input) { return parser.string_(input); };

    // (#<Define> #<Undefined> '(#<Lambda> [#<Undefined> #<Undefined>] '(bin-add #<Undefined> #<Undefined>)))
    auto outer_sexpr = parse("(define-value foo (\\ REPLACE (bin-add a b)))");
    auto inner_sexpr = parse("(a b)");

    env.toplevel.predefine("a");
    env.toplevel.predefine("b");

    // Encodes the input range (via mutation) without doing anything
    // in particular for special forms.
    auto flat_encode = [&](Any sexpr) {
	for(auto &vv : flat_iterator::range(sexpr)) {
	    if(is<Symbol>(vv)) {
		vv = env.toplevel.look_up(unwrap<std::string>(vv));
		if(is<Undefined>(vv)) {
		    unwrap<Undefined>(vv)._backtrack.push_back(&vv);
		}
	    }
	}};

    outer_sexpr = wrap(deep_copy::to<Data>(flat_iterator::range(outer_sexpr), gc));
    inner_sexpr = wrap(gc.make<Slice*>(flat_iterator::const_begin(inner_sexpr),
				       flat_iterator::const_end(inner_sexpr)));

    flat_encode(outer_sexpr);
    flat_encode(inner_sexpr);

    flat_iterator::begin(outer_sexpr)[5] = inner_sexpr;

    auto print_info = [&](Any sexpr) {
	std::cout << printer::any(sexpr) << std::endl;
	std::cout << printer::debug_range(sexpr, true) << endl;

	std::cout << "Backtrack for 'a':" << std::endl;
	print_backtracks(flat_iterator::begin(inner_sexpr)[0]);

	std::cout << "\nBacktrack for 'b':" << std::endl;
	print_backtracks(flat_iterator::begin(inner_sexpr)[1]);
    };

    std::cout << "\n\n";

    std::cout << "Input: ";
    print_info(outer_sexpr);

    auto output = deep_copy::to<Ast>(flat_iterator::range(outer_sexpr),
				     gc);

    std::cout << "Output: ";
    print_info(wrap(output));

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

