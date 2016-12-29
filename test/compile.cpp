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
#include <compile.hpp>
#include <test/trivial_functions.hpp>

#include "./testing_utils.hpp"

using namespace atl;

struct CompilerTest
	: public ::testing::Test
{
	GC store;

	Compile compile;
	TinyVM vm;

	Any equal, add, sub, add3;
	CxxFunctor::value_type *fn_equal, *fn_add, *fn_sub, *fn_add3;

	TrivialFunctions fns;

	CompilerTest()
		: vm(store)
	{
		init_types();

		fn_equal = &fns.weq->fn;
		fn_add = &fns.wadd->fn;
		fn_sub = &fns.wsub->fn;
		fn_add3 = &fns.wadd3->fn;

		equal = wrap(fns.weq);
		add = wrap(fns.wadd);
		sub = wrap(fns.wsub);
		add3 = wrap(fns.wadd3);
	}

	pcode::value_type run()
	{
		compile.assemble.finish();
		run_code(vm, compile.code_store);
		compile.code_store.pop_back();

		return vm.stack[0];
	}
};

TEST_F(CompilerTest, test_compile_atom)
{
	compile.any(wrap<Fixnum>(5));
	ASSERT_EQ(run(), 5);
}

TEST_F(CompilerTest, test_basic_application) {
	using namespace make_ast;
	compile.ast(store(mk(add, 5, 7)));

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
	compile.ast(store(mk(add, mk(sub, 7, 5), mk(add, 8, 3))));

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
	compile.ast(store(mk(wrap<If>(), wrap<Bool>(true), 3, 4)));

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

	Parameter param_a(0), param_b(1);

	a.value = wrap(param_a);
	b.value = wrap(param_b);

	LambdaMetadata metadata;
	metadata.return_type = wrap<Type>(tag<Fixnum>::value);
	metadata.formals = mk(wrap(&a), wrap(&b))(ast_alloc(store));

	auto expr = mk
		(mk(wrap<Lambda>(&metadata),
		    mk(wrap(&a), wrap(&b)),
		    mk(add, param_a, param_b)),
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
	metadata.return_type = wrap<Type>(tag<Fixnum>::value);
	metadata.formals = store(mk());

	auto expr = mk
		(mk(wrap<Lambda>(&metadata),
		    mk(),
		    mk(add, 1, 2)))
		(ast_alloc(store));

	compile.ast(expr);

	ASSERT_EQ(3, run());
}

TEST_F(CompilerTest, test_dummy_closure)
{
	using namespace make_ast;

	Symbol a = Symbol("a"),
		b = Symbol("b");

	Parameter p_a(0), p_b(0);
	ClosureParameter cp_a(0);

	a.value = wrap(p_a);
	b.value = wrap(p_b);

	LambdaMetadata outer;
	outer.return_type = wrap<Type>(tag<Fixnum>::value);
	outer.formals = store(mk(wrap(&a)));

	LambdaMetadata inner;
	inner.return_type = wrap<Type>(tag<Fixnum>::value);
	inner.formals = store(mk(wrap(&b)));

	inner.closure.push_back(&a);

	// (\ (a) (\ (b) (+ a b)))
	auto expr = store
		(mk(wrap<Lambda>(&outer),
		    mk(wrap(&a)),
		    (mk(wrap<Lambda>(&inner),
		        mk(wrap(&b)),
		        mk(add, wrap(cp_a), wrap(p_b))))));

	compile.ast(expr);

	// what I expect to be generated
	Code code;
	AssembleCode assemble(&code);

	assemble
		.add_label("post-outer")
		.constant(0)
		.jump()

		.add_label("outer-closure")

		.add_label("post-closure")
		.constant(0)
		.jump()

		.add_label("inner-closure")
		.closure_argument(0)
		.argument(0)
		.std_function(fn_add, 2)
		.return_()

		.constant_patch_label("post-closure")

		.get_label("inner-closure")
		.argument(0)
		.make_closure(1, 1)
		.return_()
		.constant_patch_label("post-outer")  // end of outer closure body

		.get_label("outer-closure")
		.make_closure(1, 0);

#ifdef DEBUGGING
	std::cout << "Expecting:" << std::endl;
	assemble.dbg();

	std::cout << "Compiled:" << std::endl;
	compile.assemble.dbg();
#endif

	ASSERT_EQ(code.size(), compile.code_store.size());
	for(size_t idx = 0; idx < code.size(); ++idx)
		{
			ASSERT_EQ(code.code[idx], compile.code_store.code[idx])
				<< " idx: " << idx;
		}
}

TEST_F(CompilerTest, test_simple_closure)
{
	using namespace make_ast;

	Symbol a = Symbol("a"),
		b = Symbol("b");

	Parameter p_a(0), p_b(0);
	ClosureParameter cp_a(0);

	a.value = wrap(p_a);
	b.value = wrap(p_b);

	LambdaMetadata outer;
	outer.return_type = wrap<Type>(tag<Fixnum>::value);
	outer.formals = store(mk(wrap(&a)));

	LambdaMetadata inner;
	inner.return_type = wrap<Type>(tag<Fixnum>::value);
	inner.formals = store(mk(wrap(&b)));
	inner.closure.push_back(&a);

	// (\ (a) (\ (b) (+ a b)))
	auto expr = store
		(mk(mk(mk(wrap<Lambda>(&outer), mk(wrap(&a)),
		          mk(wrap<Lambda>(&inner), mk(wrap(&b)),
		             mk(add, wrap(cp_a), wrap(p_b)))),
		       1),
		    2));

	compile.ast(expr);

	ASSERT_EQ(3, run());
}

TEST_F(CompilerTest, test_two_arg_closure)
{
	using namespace make_ast;

	Symbol a = Symbol("a"),
		b = Symbol("b"),
		c = Symbol("c");

	Parameter p_a(0), p_b(1), p_c(0);
	ClosureParameter cp_a(0), cp_b(1);

	a.value = wrap(p_a);
	b.value = wrap(p_b);
	c.value = wrap(p_c);

	LambdaMetadata outer;
	outer.return_type = wrap<Type>(tag<Fixnum>::value);
	outer.formals = store(mk(wrap(&a), wrap(&b)));

	LambdaMetadata inner;
	inner.return_type = wrap<Type>(tag<Fixnum>::value);
	inner.formals = store(mk(wrap(&c)));

	inner.closure.push_back(&a);
	inner.closure.push_back(&b);

	auto expr = store
		(mk(mk(mk(wrap<Lambda>(&outer), mk(wrap(&a), wrap(&b)),
		          (mk(wrap<Lambda>(&inner), mk(wrap(&c)),
		              mk(add3, wrap(cp_a), wrap(cp_b), wrap(p_c))))),
		       1, 2),
		    3));

	compile.ast(expr);

	ASSERT_EQ(6, run());
}

TEST_F(CompilerTest, test_define_first)
{
	using namespace make_ast;

	Symbol foo = Symbol("foo"),
		a = Symbol("a"),
		b = Symbol("b"),
		c = Symbol("c");

	Parameter p_a(0), p_b(1), p_c(2);

	foo.scheme.type = wrap
		(store(fn_type::fn(tag<Fixnum>::value,
		                   tag<Fixnum>::value,
		                   tag<Fixnum>::value,
		                   tag<Fixnum>::value)));

	LambdaMetadata foo_info;
	foo_info.return_type = wrap<Type>(tag<Fixnum>::value);
	foo_info.formals = store(mk(&a, &b, &c));

	foo.value = wrap<Lambda>(&foo_info);

	// (define foo (\ (a b c) (+ (+ a b) c)))
	compile.ast
		(store
		 (mk(wrap<Define>(),
		     &foo,
		     mk(wrap<Lambda>(&foo_info), mk(&a, &b, &c),
		        mk(add, mk(add, p_a, p_b), p_c)))));

	compile.ast(store(mk(&foo, 5, 3, 2)));

	ASSERT_EQ(10, run());
}

TEST_F(CompilerTest, test_define_after)
{
	using namespace make_ast;

	Symbol do_it = Symbol("do_it"),
		foo = Symbol("foo"),
		a = Symbol("a"),
		b = Symbol("b");
	Parameter p_a(0), p_b(1);

	do_it.scheme.type = wrap
		(store(fn_type::fn(tag<Fixnum>::value)));

	foo.scheme.type = wrap
		(store(fn_type::fn(tag<Fixnum>::value,
		                   tag<Fixnum>::value,
		                   tag<Fixnum>::value)));

	LambdaMetadata do_it_info;
	do_it_info.return_type =  wrap<Type>(tag<Fixnum>::value);
	do_it_info.formals = store(mk());

	LambdaMetadata foo_info;
	foo_info.return_type = wrap<Type>(tag<Fixnum>::value);
	foo_info.formals = store(mk(&a, &b));

	foo.value = wrap<Lambda>(&foo_info);

	compile.ast
		(store
		 (mk(wrap<Define>(),
		     &do_it,
		     mk(wrap<Lambda>(&do_it_info), mk(), mk(&foo, 5, 2)))));

	// (define foo (\ (a b) (- a b)))
	compile.ast
		(store
		 (mk(wrap<Define>(),
		     &foo,
		     mk(wrap<Lambda>(&foo_info), mk(&a, &b),
		        mk(sub, p_a, p_b)))));

	compile.ast(store(mk(&do_it)));

	ASSERT_EQ(3, run());
}
