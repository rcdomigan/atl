/**
 * @file /home/ryan/programming/atl/test/vm_test.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Dec 21, 2014
 */

#include <tiny_vm.hpp>
#include <primitive_callable.hpp>
#include <compile.hpp>
#include <parser.hpp>
#include <environment.hpp>
#include "./testing_utils.hpp"

#include <gtest/gtest.h>

using namespace atl;

struct VmTest : public ::testing::Test
{
    TinyVM::value_type code[100];
    TinyVM vm;
    AssembleVM assemble;

    VmTest() : assemble(code) {}
};

TEST_F(VmTest, TestCxxFn2)
{
    using namespace atl;
    auto fns = TrivialFunctions();

    assemble.constant(2)
        .constant(3)
        .std_function(&fns.wadd->fn, 2)
        .finish();

    run_code(vm, std::move(assemble));

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

    // include conversion to Any
    Any fn = wrap(shimmed_function);

    assemble.constant(3)
        .std_function(&unwrap<CxxFunctor>(fn).fn, 1)
        .finish();

    run_code(vm, std::move(assemble));

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

    run_code(vm, std::move(assemble));
    ASSERT_EQ(vm.stack[0], 9);

    assemble = AssembleVM(code); // steal back the PCode buffer
    multiple = 4;

    assemble.constant(3)
        .std_function(&shimmed_function->fn, 1)
        .finish();

    run_code(vm, std::move(assemble));
    ASSERT_EQ(vm.stack[0], 12);
}

TEST_F(VmTest, TestIfTrue)
{
    assemble.pointer(nullptr);
    auto alternate_ptr = assemble.last();

    assemble.constant(1)
        .if_()
        .constant(5)            // consequent
        .pointer(nullptr);
    auto end_of_alt = assemble.last();
    assemble.jump();

    *alternate_ptr = reinterpret_cast<TinyVM::value_type>(assemble.end());
    assemble.constant(9);        // alternate

    *end_of_alt = reinterpret_cast<TinyVM::value_type>(assemble.end());
    assemble.finish();

    run_code(vm, std::move(assemble));

    ASSERT_EQ(vm.stack[0], 5);
}

TEST_F(VmTest, TestIfFalse)
{
    assemble.pointer(nullptr);
    auto alternate_ptr = assemble.last();

    assemble.constant(0)
        .if_()
        .constant(5)            // consequent
        .pointer(nullptr);
    auto end_of_alt = assemble.last();
    assemble.jump();

    *alternate_ptr = reinterpret_cast<TinyVM::value_type>(assemble.end());
    assemble.constant(9);        // alternate

    *end_of_alt = reinterpret_cast<TinyVM::value_type>(assemble.end());
    assemble.finish();

    run_code(vm, std::move(assemble));

    ASSERT_EQ(vm.stack[0], 9);
}

TEST_F(VmTest, TestArguments)
{
    TrivialFunctions fns;

    assemble.constant(5)
	.constant(3)
	.pointer(nullptr);
    auto procedure_address = assemble.last();

    assemble.call_procedure()
	.finish();

    *procedure_address = reinterpret_cast<uintptr_t>(assemble.end());

    assemble.argument(2)
	.argument(1)
	.std_function(&fns.wsub->fn,
                      2)
	.constant(2) // # args
	.return_();

    run_code(vm, std::move(assemble));

    ASSERT_EQ(vm.stack[0], 2);
}

TEST_F(VmTest, SymToFunction)
{
    GC gc;
    ParseString parse(gc);
    Environment env(gc);
    TrivialFunctions fns;

    setup_interpreter(env, parse);

    env.define("add2", wrap(fns.wadd));

    auto fn = env.toplevel.value("add2");

    assemble.constant(1)
        .constant(2)
        .std_function(&unwrap<CxxFunctor>(fn).fn, 2)
        .finish();

    run_code(vm, std::move(assemble));

    ASSERT_EQ(vm.stack[0], 3);
}

TEST_F(VmTest, SMoveN)
{
    assemble.pointer(nullptr);
    auto after_defs = assemble.last(); // skip over function definitions
    assemble.jump();

    auto enter_tail_call = assemble.end();
    assemble.finish();

    auto enter_setup = assemble.end();
    assemble.constant(2)
        .constant(3)
        .constant(4)
        .tail_call(3, enter_tail_call);

    *after_defs = reinterpret_cast<uintptr_t>(assemble.end());
    assemble.constant(1)
        .constant(0)
        .constant(0)
        .constant(0)
        .call_procedure(enter_setup);

    run_code(vm, std::move(assemble));

    for(int i = 0; i < 4; ++i)
        ASSERT_EQ(vm.stack[i], i + 1);
}
