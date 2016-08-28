/**
 * @file /home/ryan/programming/atl/test/vm_test.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Dec 21, 2014
 */

#include <vm.hpp>
#include "./trivial_functions.hpp"
#include "./testing_utils.hpp"

#include <gtest/gtest.h>

using namespace atl;

struct VmTest : public ::testing::Test
{
	Code code_store;
    AssembleCode assemble;
	TinyVM vm;
	TrivialFunctions fns;

	VmTest() : assemble(&code_store) {}
};

TEST_F(VmTest, TestCxxFn2)
{
    using namespace atl;

    assemble.constant(2)
        .constant(3)
        .std_function(&fns.wadd->fn, 2)
        .finish();

    run_code(vm, code_store);

    ASSERT_EQ(vm.stack[0], 5);
}

TEST_F(VmTest, test_over_write_constant)
{
	assemble.constant(2);

	auto patch = assemble.pos_end();

	assemble
        .constant(3)
        .std_function(&fns.wadd->fn, 2)
        .finish();

    WrapCodeItr overwrite_code(*assemble.code);
    AssembleCode overwrite(&overwrite_code);

    overwrite_code.set_position(patch);

    overwrite.constant(4);

    run_code(vm, code_store);

    ASSERT_EQ(vm.stack[0], 6);
}

TEST_F(VmTest, test_over_write_function)
{

	assemble
        .constant(3)
		.constant(2);

	auto patch = assemble.pos_end();

	assemble
        .std_function(&fns.wadd->fn, 2)
        .finish();

    WrapCodeItr overwrite_code(*assemble.code);
    AssembleCode overwrite(&overwrite_code);

    overwrite_code.set_position(patch);

    overwrite.std_function(&fns.wsub->fn, 2);

    run_code(vm, code_store);

    ASSERT_EQ(vm.stack[0], 1);
}

TEST_F(VmTest, TestSimpleCxxStdFunction)
{
    GC gc;
    auto shimmed_function = WrapStdFunction<long (long)>::a([&](long vv) -> long {
            return vv * 3;
        },
        gc,
        "foo");

    // include conversion to Any
    Any fn = wrap(shimmed_function);

    assemble.constant(3)
        .std_function(&unwrap<CxxFunctor>(fn).fn, 1)
        .finish();

    run_code(vm, code_store);

    ASSERT_EQ(vm.stack[0], 9);
}


TEST_F(VmTest, TestCxxStdFunction)
{
    // Try using a std::function which has some captured state..
    GC gc;
    long multiple = 3;

    auto shimmed_function = WrapStdFunction<long (long)>::a([&](long vv) -> long {
            return vv * multiple;
        },
        gc,
        "foo");

    assemble.constant(3)
        .std_function(&shimmed_function->fn, 1)
        .finish();

    run_code(vm, code_store);
    ASSERT_EQ(vm.stack[0], 9);

    multiple = 4;

    assemble.constant(3)
        .std_function(&shimmed_function->fn, 1)
        .finish();

    run_code(vm, code_store);
    ASSERT_EQ(12, vm.result());
}

TEST_F(VmTest, TestIfTrue)
{
    assemble.pointer(nullptr);
    auto alternate_ptr = assemble.pos_last();

    assemble.constant(1)
        .if_()
        .constant(5)            // consequent
        .pointer(nullptr);
    auto end_of_alt = assemble.pos_last();
    assemble.jump();

    assemble[alternate_ptr] = assemble.pos_end();
    assemble.constant(9);        // alternate

    assemble[end_of_alt] = assemble.pos_end();
    assemble.finish();

    run_code(vm, code_store);

    ASSERT_EQ(vm.stack[0], 5);
}

TEST_F(VmTest, TestIfFalse)
{
    assemble.pointer(nullptr);
    auto alternate_ptr = assemble.pos_last();

    assemble.constant(0)
        .if_()
        .constant(5)            // consequent
        .pointer(nullptr);
    auto end_of_alt = assemble.pos_last();
    assemble.jump();

    assemble[alternate_ptr] = assemble.pos_end();
    assemble.constant(9);        // alternate

    assemble[end_of_alt] = assemble.pos_end();
    assemble.finish();

    run_code(vm, code_store);

    ASSERT_EQ(vm.stack[0], 9);
}

TEST_F(VmTest, test_simple_function)
{
	//  ((\ (a b) (sub a b)) 5 3)
	assemble.constant(5)
		.constant(3)
		.add_label("function")
		.call_procedure(0)
		.finish()
		.constant_patch_label("function")
		.argument(1)
		.argument(0)
		.std_function(&fns.wsub->fn,
		              2)
		.constant(2) // # args
		.return_();

	run_code(vm, code_store);

	ASSERT_EQ(vm.stack[0], 2);
}

/* The way a tail call works (ATM) is to place the call's arguments on
 * the top of the stack, then move them down the stack to over-write
 * its caller's stack.
 */
TEST_F(VmTest, test_move_n)
{
    assemble.pointer(nullptr);
    auto after_defs = assemble.pos_last(); // skip over function definitions
    assemble.jump();

    auto enter_tail_call = assemble.pos_end();
    assemble.finish();

    auto enter_setup = assemble.pos_end();
    assemble.constant(2)
        .constant(3)
        .constant(4)
        .tail_call(3, enter_tail_call);

    assemble[after_defs] = assemble.pos_end();
    assemble.constant(1)
        .constant(0)
        .constant(0)
        .constant(0)
        .call_procedure(enter_setup);

    run_code(vm, code_store);

    for(int i = 0; i < 4; ++i)
        ASSERT_EQ(vm.stack[i], i + 1);
}

/** Try accessing parameters from a closure. */
TEST_F(VmTest, test_bound_non_locals)
{
	Closure my_closure;
	my_closure.body = 0;
	my_closure.values = {3, 5};

	assemble
		.call_closure(my_closure)
		.finish();
	my_closure.body = assemble.pos_end();

	assemble
		.closure_argument(0)
		.closure_argument(1)
		.std_function(&fns.wadd->fn, 2)
		.return_(0);

	run_code(vm, code_store);

	ASSERT_EQ(8, vm.stack[0]);
}
