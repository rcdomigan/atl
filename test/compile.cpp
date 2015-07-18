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
#include "./testing_utils.hpp"

using namespace atl;

struct CompilerTest : public ::testing::Test {
    GC gc;
    ParseString parse;
    Environment env;
    Compile compile;
    TinyVM vm;

    CompilerTest() : parse(gc), env(gc), compile(env) {}

    virtual void SetUp() {
        setup_interpreter(env, parse);

        env.define("equal2", WrapStdFunction<bool (long, long)>::any(equal2, gc));
        env.define("add2", WrapStdFunction<long (long, long)>::any(add2, gc));
        env.define("sub2", WrapStdFunction<long (long, long)>::any(sub2, gc));
    }
};


TEST_F(CompilerTest, BasicApplication) {
    compile.any(parse.string_("(add2 5 7)"));

    run_code(vm, compile.finish());

    ASSERT_EQ(vm.stack[0], 12);
}

TEST_F(CompilerTest, NestedApplication) {
    auto ast = parse.string_("(add2 (sub2 7 5) (add2 8 3))");
    compile.any(ast);

    run_code(vm, compile.finish());

    ASSERT_EQ(vm.stack[0], 13);
}

TEST_F(CompilerTest, TestCxxStdFunction) {
    long multiple = 3;

    auto shimmed_function = WrapStdFunction<long (long)>::a([&](long vv) -> long {
            return vv * multiple;
        },
        gc,
        "foo");

    env.define("foo", wrap(shimmed_function));

    compile.any(parse.string_("(foo 3)"));

    run_code(vm, compile.finish());
    ASSERT_EQ(vm.stack[0], 9);

    compile.wrapped = AssembleVM(gc.alloc_pcode());

    multiple = 4;
    compile.any(parse.string_("(foo 3)"));

    run_code(vm, compile.finish());
    ASSERT_EQ(vm.stack[0], 12);
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

    ASSERT_EQ(unwrap<Procedure>(env.toplevel._local["my-add1"].value).tail_params,
              3);
}


TEST_F(CompilerTest, SimpleRecursion) {
    compile.any(parse.string_("(define-value simple-recur (\\ (a b) (if (equal2 0 a) b (simple-recur (sub2 a 1) (add2 b 1)))))"));
    compile.any(parse.string_("(simple-recur 3 2)"));

    run_code(vm, compile.finish());

    ASSERT_EQ(vm.stack[0], 5);
}
