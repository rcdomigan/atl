/**
 * @file /home/ryan/programming/atl/test/vm_test.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Dec 21, 2014
 */

#include <tiny_vm.hpp>
#include <primitive_callable.hpp>
#include <compile.hpp>
#include <gc.hpp>
#include <parser.hpp>
#include <environment.hpp>
#include <gtest/gtest.h>

using namespace atl;

long add2(long a, long b) { return a + b; }
long sub2(long a, long b) { return a - b; }
bool equal2(long a, long b) { return a == b; }

void run_code(TinyVM& vm, AssembleVM& assemble) {
#ifdef DEBUGGING
    vm.run_debug(assemble.begin(), 20);
#else
    vm.run(assemble.begin());
#endif
}

TEST(VmTest, TestCxxFn2) {
    using namespace atl;

    TinyVM::value_type code[100];

    TinyVM vm;
    AssembleVM assemble(code);

    assemble.constant(2)
        .constant(3)
        .foreign_2(&WrapFn2<long (*)(long, long), add2>::wrapped)
        .finish();

    assemble.print();

    run_code(vm, assemble);

    ASSERT_EQ(vm.stack[0], 5);
}

TEST(VmTest, TestArguments) {
    TinyVM::value_type code[100];

    TinyVM vm;
    AssembleVM assemble(code);

    assemble.constant(5)
	.constant(3)
	.pointer(nullptr);
    auto procedure_address = assemble.last();

    assemble.procedure()
	.finish();

    *procedure_address = reinterpret_cast<uintptr_t>(assemble.end());

    assemble.argument(2)
	.argument(1)
	.foreign_2(&WrapFn2<long (*)(long, long), sub2>::wrapped)
	.constant(2) // # args
	.return_();

    assemble.print();

    run_code(vm, assemble);

    ASSERT_EQ(vm.stack[0], 2);
}

TEST(VmTest, SymToFunction) {
    GC gc;
    ParseString parse(gc);
    Environment env(gc);

    TinyVM::value_type code[100];
    TinyVM vm;
    AssembleVM assemble(code);

    setup_interpreter(gc, env, parse);

    auto wrapped_add2 = WrapFn2<long (*)(long, long), add2>::any();

    env.define("add2", wrapped_add2);

    auto fn = env.toplevel.value("add2");

    assemble.argument(1)
        .argument(2)
        .foreign_2(unwrap<CxxFn2>(fn).value)
        .finish();

#ifdef DEBUGGING
    cout << "Foreign fuction stored and retrieved from env" << endl;
    cout << " in Any: " << wrapped_add2.value << endl;
    cout << " wrapped: " << (reinterpret_cast<void*>(&WrapFn2<long (*)(long, long), add2>::wrapped)) << endl;
    cout << " from env: " << unwrap<CxxFn2>(fn).value << endl;

    assemble.print();
#endif

    run_code(vm, assemble);

#ifdef DEBUGGING
    cout << "Yields: " << vm.stack[0] << endl;
#endif

    ASSERT_EQ(vm.stack[0], 3);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
