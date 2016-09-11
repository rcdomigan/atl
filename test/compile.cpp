/**
 * @file /home/ryan/programming/atl/test/compiler.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Dec 25, 2014
 *
 * Test that we can compile somethign to byte code given an Ast where
 * the variable scopes are known and type has already been inferred.
 */

#include <gtest/gtest.h>

#include <vm.hpp>
#include <print.hpp>
#include <lexical_environment.hpp>
#include <compile.hpp>
#include <test/trivial_functions.hpp>

#include "./testing_utils.hpp"

using namespace atl;

struct CompilerTest
	: public ::testing::Test
{
	GC store;
	SymbolMap lexical;

	Compile compile;
	TinyVM vm;

	Any equal, add, sub;
	CxxFunctor::value_type *fn_equal, *fn_add, *fn_sub;

	CompilerTest()
		: lexical(store)
		, compile(store)
	{
		init_types();

		auto wrapped_equal = WrapStdFunction<bool (long, long)>::a(equal2, store);
		auto wrapped_add = WrapStdFunction<long (long, long)>::a(add2, store);
		auto wrapped_sub = WrapStdFunction<long (long, long)>::a(sub2, store);

		fn_equal = &wrapped_equal->fn;
		fn_add = &wrapped_add->fn;
		fn_sub = &wrapped_sub->fn;

		equal = wrap(wrapped_equal);
		add = wrap(wrapped_add);
		sub = wrap(wrapped_sub);

		lexical.define(*store.symbol("equal2"), pass_value(equal));
		lexical.define(*store.symbol("add2"), pass_value(add));
		lexical.define(*store.symbol("sub2"), pass_value(sub));
	}

	pcode::value_type run()
	{
		compile.assemble.finish();
		run_code(vm, compile.code_store);
		compile.code_store.pop_back();

		return vm.stack[0];
	}
};


TEST_F(CompilerTest, test_back_patch_constant)
{
	Symbol sym("foo");

	Code code_store;
	PatchingAssembler assemble(&code_store);

	assemble
		.constant(3)
		.needs_patching(sym)
		.std_function(fn_add, 2)
		.finish();

	sym.value = wrap<Fixnum>(4);

	assemble
		.patch(sym);

	run_code(vm, code_store);
	ASSERT_EQ(7, vm.stack[0]);
}

TEST_F(CompilerTest, test_back_patch_function)
{
	using namespace make_ast;

	Symbol sym("foo");
	// (-> Fixnum Fixnum)
	sym.scheme.type = wrap(fn_type::fn(tag<Fixnum>::value, tag<Fixnum>::value)(store));

	Code code;
	PatchingAssembler assemble(&code);

	// (foo 3)
	assemble
		.constant(3)
		.needs_patching(sym)
		.finish();

	// (define foo (\ (a) (add a 4)))
	LambdaMetadata metadata;
	metadata.body_address = assemble.pos_end();
	sym.value = wrap<CallLambda>(&metadata);

	assemble
		.patch(sym)
		.argument(0)
		.constant(4)
		.std_function(fn_add, 2)
		.constant(1)
		.return_();

	run_code(vm, code);
	ASSERT_EQ(7, vm.stack[0]);
}

TEST_F(CompilerTest, test_compile_atom)
{
	compile.any(wrap<Fixnum>(5));
	ASSERT_EQ(run(), 5);
}

TEST_F(CompilerTest, test_basic_application) {
	using namespace make_ast;
	compile.ast(mk(add, 5, 7)(store));

	Code code;
	AssembleCode assemble(&code);

	assemble
		.constant(5)
		.constant(7)
		.std_function(&unwrap<CxxFunctor>(add).fn, 2);

	ASSERT_EQ(code,
	          compile.code_store);
}

TEST_F(CompilerTest, test_nested_application)
{
	using namespace make_ast;
	compile.ast(mk(add, mk(sub, 7, 5), mk(add, 8, 3))
	            (store));

	Code code;
	AssembleCode assemble(&code);

	assemble
		.constant(7)
		.constant(5)
		.std_function(fn_sub, 2)
		.constant(8)
		.constant(3)
		.std_function(fn_add, 2)
		.std_function(fn_add, 2);

	ASSERT_EQ(code,
	          compile.code_store);
}

TEST_F(CompilerTest, test_if)
{
	using namespace make_ast;
	compile.ast(mk(wrap<If>(), wrap<Bool>(true), 3, 4)(store));

	Code code;
	AssembleCode assemble(&code);

	assemble.add_label("alternate")
		.constant(0xdeadbeef)
		.constant(true)
		.if_()
		.constant(3)
		.add_label("to-end")
		.constant(0xdeabdbeef)
		.jump()
		.constant_patch_label("alternate")
		.constant(4)
		.constant_patch_label("to-end");

	ASSERT_EQ(code,
	          compile.code_store);
}


TEST_F(CompilerTest, test_basic_lambda)
{
	using namespace make_ast;

	Symbol a = Symbol("a"),
		b = Symbol("b");

	Bound bound_a = Bound(&a, Bound::Subtype::is_local, 1),
		bound_b = Bound(&b, Bound::Subtype::is_local, 0);

	a.value = wrap(&bound_a);
	b.value = wrap(&bound_b);

	// note: currently the compiler just sets the LambdaMetadata body
	// position; it doesn't need much else
	LambdaMetadata metadata;
	metadata.pad_to = 2;
	metadata.return_type = wrap<Type>(tag<Fixnum>::value);

	auto expr = mk
		(mk(wrap<Lambda>(&metadata),
		    mk(wrap(&a), wrap(&b)),
		    mk(add, wrap(&bound_a), wrap(&bound_b))),
		 4, 7)
		(ast_alloc(store));

	compile.ast(expr);

	ASSERT_EQ(11, run());
}

/* Try compiling and applying a lambda with no args */
TEST_F(CompilerTest, test_thunk)
{
	using namespace make_ast;

	// note: currently the compiler just sets the LambdaMetadata body
	// position; it doesn't need much else
	LambdaMetadata metadata;
	metadata.pad_to = 2;
	metadata.return_type = wrap<Type>(tag<Fixnum>::value);

	auto expr = mk
		(mk(wrap<Lambda>(&metadata),
		    mk(),
		    mk(add, 1, 2)))
		(ast_alloc(store));

	compile.ast(expr);

	ASSERT_EQ(3, run());
}

