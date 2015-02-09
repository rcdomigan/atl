/**
 * @file /home/ryan/programming/atl/test/compiler.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Dec 25, 2014
 *
 * Regression tests.  There may be unit tests some day if I can be bolloxed.
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

void run_code(TinyVM& vm, AssembleVM input) {
#ifdef DEBUGGING
    input.print();
    vm.run_debug(input.main_entry_point, 100);
#else
    vm.run(input.main_entry_point);
#endif
}

struct CompilerTest : public ::testing::Test {
    GC gc;
    ParseString parse;
    Environment env;
    Compile compile;
    TinyVM vm;

    CompilerTest() : parse(gc), env(gc), compile(env) {}

    virtual void SetUp() {
        setup_interpreter(gc, env, parse);

        env.define("equal2", WrapFnPntr<bool (*)(long, long), equal2>::any());
        env.define("add2", WrapFnPntr<long (*)(long, long), add2>::any());
        env.define("sub2", WrapFnPntr<long (*)(long, long), sub2>::any());
    }
};


TEST_F(CompilerTest, BasicApplication) {
    compile.any(parse.string_("(add2 2 3)"));

    run_code(vm, compile.finish());

    ASSERT_EQ(vm.stack[0], 5);
}

TEST_F(CompilerTest, NestedApplication) {
    auto ast = parse.string_("(add2 (sub2 7 5) (add2 8 3))");
    compile.any(ast);

    run_code(vm, compile.finish());

    ASSERT_EQ(vm.stack[0], 13);
}

TEST_F(CompilerTest, BasicLambda) {
    auto ast = parse.string_("((\\ (a b) (add2 a b)) 4 7)");

    compile.any(ast);

    run_code(vm, compile.finish());

    ASSERT_EQ(vm.stack[0], 11);
}

TEST_F(CompilerTest, IfTrue) {
    auto ast = parse.string_("(if #t 3 4)");
    compile.any(ast);

    run_code(vm, compile.finish());
    ASSERT_EQ(vm.stack[0], 3);
}

TEST_F(CompilerTest, IfFalse) {
    auto ast = parse.string_("(if #f 3 4)");
    compile.any(ast);

    run_code(vm, compile.finish());
    ASSERT_EQ(vm.stack[0], 4);
}

TEST_F(CompilerTest, LambdaWithIf) {
    auto ast = parse.string_("((\\ (a b) (if (equal2 a b) (add2 a b) (sub2 a b))) 7 3)");
    compile.any(ast);

    run_code(vm, compile.finish());

    ASSERT_EQ(vm.stack[0], 4);
}

TEST_F(CompilerTest, BasicDefine) {
    // Test that defining a constant works
    auto ast = parse.string_("(define-value foo 3)");
    compile.any(ast);

    ast = parse.string_("(add2 foo foo)");
    compile.any(ast);

    run_code(vm, compile.finish());

    ASSERT_EQ(vm.stack[0], 6);
}

TEST_F(CompilerTest, Backpatch) {
    // Test that defining a constant works
    auto ast = parse.string_("(add2 foo foo)");
    compile.any(ast);

    ast = parse.string_("(define-value foo 3)");
    compile.any(ast);

    run_code(vm, compile.finish());

    ASSERT_EQ(vm.stack[0], 6);
}


TEST_F(CompilerTest, DefineLambda) {
    // Check that the appropriate number of filler values have been
    // pushed to the stack.  This is a brittle test, but so it goes
    // for the moment.
    compile.any(parse.string_("(define-value my-add3 (\\ (a b c) (add2 a b)))"));
    compile.any(parse.string_("(define-value my-add1 (\\ (a) (my-add3 a a a)))"));

    ASSERT_EQ(unwrap<Procedure>(env.toplevel._local["my-add1"]).tail_params,
              3);
}


TEST_F(CompilerTest, SimpleRecursion) {
    compile.any(parse.string_("(define-value simple-recur (\\ (a b) (if (equal2 0 a) b (simple-recur (sub2 a 1) (add2 b 1)))))"));
    compile.any(parse.string_("(simple-recur 3 2)"));

    run_code(vm, compile.finish());

    ASSERT_EQ(vm.stack[0], 5);
}


int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
