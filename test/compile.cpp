/**
 * @file /home/ryan/programming/atl/test/compiler.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Dec 25, 2014
 *
 * Regression tests.  There may be unit tests some day if I can be bolloxed.
 */

#include <tiny_vm.hpp>
#include <print.hpp>
#include <atl.hpp>

#include <gtest/gtest.h>
#include "./testing_utils.hpp"

using namespace atl;

struct CompilerTest : public ::testing::Test {
	Atl atl;

    CompilerTest() {}

	decltype(make_ast::lift(1))
	sym(std::string const& name)
	{ return make_ast::lift(atl.gc.amake<Symbol>(name)); }

    virtual void SetUp() {
        atl.lexical.define("equal2", WrapStdFunction<bool (long, long)>::any(equal2, atl.gc));
        atl.lexical.define("add2", WrapStdFunction<long (long, long)>::any(add2, atl.gc));
        atl.lexical.define("sub2", WrapStdFunction<long (long, long)>::any(sub2, atl.gc));
    }
};


TEST_F(CompilerTest, test_compile_atom)
{
	atl.compile.any(atl.parse.string_("5"));
	run_code(atl.vm, atl.compile.code);
	ASSERT_EQ(atl.vm.stack[0], 5);
}

TEST_F(CompilerTest, BasicApplication) {
    atl.compile.any(atl.parse.string_("(add2 5 7)"));

    run_code(atl.vm, atl.compile.code);

    ASSERT_EQ(atl.vm.stack[0], 12);
}

TEST_F(CompilerTest, NestedApplication) {
    auto ast = atl.parse.string_("(add2 (sub2 7 5) (add2 8 3))");
    atl.compile.any(ast);

    run_code(atl.vm, atl.compile.code);

    ASSERT_EQ(atl.vm.stack[0], 13);
}

TEST_F(CompilerTest, TestCxxStdFunction) {
    long multiple = 3;

    auto shimmed_function = WrapStdFunction<long (long)>::a([&](long vv) -> long {
            return vv * multiple;
        },
        atl.gc,
        "foo");

    atl.lexical.define("foo", wrap(shimmed_function));

    atl.compile.any(atl.parse.string_("(foo 3)"));

    run_code(atl.vm, atl.compile.code);
    ASSERT_EQ(atl.vm.stack[0], 9);

    atl.compile.code = AssembleVM(&atl.gc.alloc_pcode());

    multiple = 4;
    atl.compile.any(atl.parse.string_("(foo 3)"));

    run_code(atl.vm, atl.compile.code);
    ASSERT_EQ(atl.vm.stack[0], 12);
}

TEST_F(CompilerTest, IfTrue) {
    auto ast = atl.parse.string_("(if #t 3 4)");
    atl.compile.any(ast);

    run_code(atl.vm, atl.compile.code);
    ASSERT_EQ(atl.vm.stack[0], 3);
}

TEST_F(CompilerTest, IfFalse) {
    auto ast = atl.parse.string_("(if #f 3 4)");
    atl.compile.any(ast);

    run_code(atl.vm, atl.compile.code);
    ASSERT_EQ(atl.vm.stack[0], 4);
}

TEST_F(CompilerTest, BasicLambda) {
    auto ast = parse.string_("((\\ (a b) (add2 a b)) 4 7)");

    compile.any(ast);

    run_code(atl.vm, atl.compile.code);

    ASSERT_EQ(11, atl.vm.stack[0]);
}

TEST_F(CompilerTest, LambdaWithIf) {
    auto ast = parse.string_("((\\ (a b) (if (equal2 a b) (add2 a b) (sub2 a b))) 7 3)");
    compile.any(ast);

    run_code(atl.vm, atl.compile.code);

    ASSERT_EQ(vm.stack[0], 4);
}

TEST_F(CompilerTest, BasicDefine) {
    // Test that defining a constant works
    auto ast = atl.parse.string_("(define-value foo 3)");
    atl.compile.any(ast);

    ast = atl.parse.string_("(add2 foo foo)");
    atl.compile.any(ast);

    run_code(atl.vm, atl.compile.code);

    ASSERT_EQ(atl.vm.stack[0], 6);
}

TEST_F(CompilerTest, Backpatch) {
    // Test that defining a constant works
    auto ast = atl.parse.string_("(add2 foo foo)");
    atl.compile.any(ast);

    ast = atl.parse.string_("(define-value foo 3)");
    atl.compile.any(ast);

    run_code(atl.vm, atl.compile.code);

    ASSERT_EQ(atl.vm.stack[0], 6);
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
	auto no_check = compile.supress_type_check();
    compile.any(parse.string_("(define-value simple-recur (\\ (a b) (if (equal2 0 a) b (simple-recur (sub2 a 1) (add2 b 1)))))"));
    compile.any(parse.string_("(simple-recur 3 2)"));

    run_code(atl.vm, atl.compile.code);

    ASSERT_EQ(vm.stack[0], 5);
}


TEST_F(CompilerTest, multiple_functions)
{
	auto no_check = compile.supress_type_check();

	compile.any(parse.string_
	            ("(define-value simple-recur (\\ (a b) (if (equal2 0 a) b (simple-recur (sub2 a 1) (add2 b 1)))))"));
	compile.any(parse.string_
	            ("(define-value simple-recur2 (\\ (a b) (if (equal2 0 a) b (simple-recur2 (sub2 a 1) (add2 b 4 )))))"));
	compile.any(parse.string_
	            ("(add2 (simple-recur 2 3) (simple-recur2 2 0))"));

	run_code(atl.vm, atl.compile.code);
	ASSERT_EQ(13, atl.vm.stack[0]);
}

TEST_F(CompilerTest, relocate_pcode)
{
	GC::PCodeAccumulator code1(10), code2(10);
	code1.clear();

	atl.compile.code.output = &code1;
	auto no_check = atl.compile.supress_type_check();

	compile.any(parse.string_
	            ("(define-value simple-recur (\\ (a b) (if (equal2 0 a) b (simple-recur (sub2 a 1) (add2 b 1)))))"));

	code2 = code1;
	atl.compile.code.output = &code2;

	compile.any(parse.string_
	            ("(define-value simple-recur2 (\\ (a b) (if (equal2 0 a) b (simple-recur2 (sub2 a 1) (add2 b 4 )))))"));
	compile.any(parse.string_
	            ("(add2 (simple-recur 2 3) (simple-recur2 2 0))"));

	run_code(atl.vm, atl.compile.code);
	ASSERT_GT(code2.capacity(), 10);
	ASSERT_EQ(atl.vm.stack[0], 13);
}
