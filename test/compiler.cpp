/**
 * @file /home/ryan/programming/atl/test/compiler.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Dec 25, 2014
 */

#include <tiny_vm.hpp>
#include <primitive_callable.hpp>
#include <compile.hpp>
#include <gc.hpp>
#include <parser.hpp>
#include <environment.hpp>
#include <compile.hpp>
#include <print.hpp>

#include <gtest/gtest.h>

using namespace atl;

long add2(long a, long b) { return a + b; }
long sub2(long a, long b) { return a - b; }
bool equal2(long a, long b) { return a == b; }

void run_code(TinyVM& vm, TinyVM::iterator input) {
#ifdef DEBUGGING
    vm.run_debug(input, 100);
#else
    vm.run(input);
#endif
}


TEST(CompilerTest, BasicLambda) {
    GC gc;
    ParseString parse(gc);
    Environment env(gc);
    Compile compile(env);

    setup_interpreter(gc, env, parse);

    env.define("equal2", WrapFn2<bool (*)(long, long), equal2>::any());
    env.define("add2", WrapFn2<long (*)(long, long), add2>::any());
    env.define("sub2", WrapFn2<long (*)(long, long), sub2>::any());

    auto ast = parse.string_("((\\ (a b) (add2 a b)) 4 7)");

    auto pcode = compile.any(ast);

    TinyVM vm;
    run_code(vm, pcode.value);

    ASSERT_EQ(vm.stack[0], 5);
}

// TEST(CompilerTest, TestLambdaWithIf) {
//     TinyVM::value_type code[100];


//     GC gc;
//     ParseString parse(gc);n
//     Environment env(gc);
//     EncodeAST encode(env, gc);
//     EvaluateAST eval(env);

//     setup_interpreter(&gc, &env, &encode, &parse, &eval);

//     encode.define("equal2", wrap(WrapFn2<bool (*)(long, long), equal2>::tagged()));
//     encode.define("add2", wrap(WrapFn2<long (*)(long, long), add2>::tagged()));
//     encode.define("sub2", wrap(WrapFn2<long (*)(long, long), sub2>::tagged()));

//     auto pcode = compile(gc, encode.any(parse.string_("((\ (a b) (if (equal2 a b) (add2 a b) (sub2 a b))) 1 2)")));

//     TinyVM vm;
//     vm.run(pcode.value, 20);
//     cout << "Result: " << vm.stack[0] << endl;
//     return 0;


//     ASSERT_EQ(vm.stack[0], 5);
// }

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
