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
	GC store;

	TinyVM vm;
	unittest::TrivialFunctions fns;

	VmTest()
		: assemble(&code_store),
		  vm(store),
		  fns(store)
	{}
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

TEST_F(VmTest, TestSimpleCxxStdFunction)
{
    GC gc;
    auto shimmed_function = WrapStdFunction<long (long)>::a([&](long vv) -> long {
            return vv * 3;
        },
        gc,
        "foo");

    assemble.constant(3)
        .std_function(&shimmed_function->fn, 1)
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
	auto closure = store.closure(0, 2, 0);

	//  ((\ (a b) (sub a b)) 5 3)
	assemble.constant(5)
		.constant(3)
		.pointer(closure)
		.call_closure()
		.finish()
		.add_label("function")
		.argument(1)
		.argument(0)
		.std_function(&fns.wsub->fn, 2)
		.return_();

	reinterpret_cast<Closure*>(closure)->body = assemble.label_pos("function");

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

    auto tail_closure = store.closure(assemble.pos_end(), 3, 0);
    assemble.finish();

    auto enter_setup = assemble.pos_end();
    assemble.constant(2)
        .constant(3)
        .constant(4)
	    .pointer(tail_closure)
	    .tail_call();

    assemble[after_defs] = assemble.pos_end();
    assemble.constant(1)
	    .constant(2)
	    .pointer(store.closure(enter_setup, 1, 0))
	    .call_closure();

    run_code(vm, code_store);

    for(int i = 0; i < 4; ++i)
        ASSERT_EQ(vm.stack[i], i + 1);
}

/** Try accessing parameters from a closure. */
TEST_F(VmTest, test_bound_non_locals)
{
	auto* closure_pointer = store.closure(0, 0, 2);
	Closure& closure = *reinterpret_cast<Closure*>(closure_pointer);
	closure.captured()[0] = 3;
	closure.captured()[1] = 5;

	assemble
		.pointer(closure_pointer)
		.call_closure()
		.finish();
	closure.body = assemble.pos_end();

	assemble
		.closure_argument(0)
		.closure_argument(1)
		.std_function(&fns.wadd->fn, 2)
		.return_();

	run_code(vm, code_store);

	ASSERT_EQ(8, vm.stack[0]);
}

TEST_F(VmTest, test_make_closure)
{
	assemble
		.constant(7) // body
		.constant(3)
		.constant(5)
		.make_closure(0, 2)
		.finish();
	assemble.dbg();

	run_code(vm, code_store);

	ASSERT_EQ(1, vm.size());

	auto closure = reinterpret_cast<TinyVM::value_type*>(vm.stack[0]);
	ASSERT_EQ(0, closure[0]); // number of formals
	ASSERT_EQ(7, closure[1]); // body address
	ASSERT_EQ(3, closure[2]); // captured arg
	ASSERT_EQ(5, closure[3]); // captured arg
}

TEST_F(VmTest, test_define)
{
	auto closure = store.closure(0, 2, 0);

	// (function 1 2)
	// (define function (\ (a b) (sub a b)))
	assemble
		.add_label("function-end")
		.constant(0)
		.jump()

		.add_label("function")
		.argument(1)
		.argument(0)
		.std_function(&fns.wsub->fn, 2)
		.return_()
		.constant_patch_label("function-end")

		.get_label("function")
		.make_closure(2, 0)
		.define("foo")

		.constant(3)
		.constant(1)
		.deref_slot("foo")
		.call_closure()
		.finish();

	reinterpret_cast<Closure*>(closure)->body = assemble.label_pos("function");

	run_code(vm, code_store);

	ASSERT_EQ(vm.stack[0], 2);
}
