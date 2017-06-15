/**
 * @file /home/ryan/programming/atl/test/compiler.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Dec 25, 2014
 *
 * Test that we can compile somethign to byte code given an Ast where
 * the variable scopes are known and type has already been inferred.
 */

#include <gtest/gtest.h>

#include <atl/vm.hpp>
#include <atl/print.hpp>
#include <atl/compile.hpp>
#include <atl/test/trivial_functions.hpp>

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

	unittest::TrivialFunctions fns;

	CompilerTest()
		: compile(store),
		  vm(store),
		  fns(store)
	{
		init_types();

		fn_equal = &fns.weq->fn;
		fn_add = &fns.wadd->fn;
		fn_sub = &fns.wsub->fn;
		fn_add3 = &fns.wadd3->fn;

		equal = fns.weq.any;
		add = fns.wadd.any;
		sub = fns.wsub.any;
		add3 = fns.wadd3.any;
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
	using namespace make_ast;
	auto expr = store(mk(wrap<Fixnum>(5)));
	compile.compile(expr->begin());
	ASSERT_EQ(run(), 5);
}

TEST_F(CompilerTest, test_basic_application) {
	using namespace make_ast;
	compile.compile(store(mk(add, 5, 7)));

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
	compile.compile(store(mk(add, mk(sub, 7, 5), mk(add, 8, 3))));

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
	compile.compile(store(mk(wrap<If>(), wrap<Bool>(true), 3, 4)));

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

	auto metadata = store.make<LambdaMetadata>
		(*store(mk(wrap(&a), wrap(&b))));

	auto expr = store
		(mk(mk(wrap<Lambda>(&*metadata),
		       mk(wrap(&a), wrap(&b)),
		       mk(add, param_a, param_b)),
		    4, 7));

	compile.compile(expr);

	ASSERT_EQ(11, run());
}

/* Try compiling and applying a lambda with no args */
TEST_F(CompilerTest, test_thunk)
{
	using namespace make_ast;

	// note: currently the compiler just sets the LambdaMetadata body
	// position; it doesn't need much else
	auto metadata = store.make<LambdaMetadata>
		(store(mk()));

	auto expr = store(mk
		(mk(wrap<Lambda>(&*metadata),
		    mk(),
		    mk(add, 1, 2))));

	compile.compile(expr);

	ASSERT_EQ(3, run());
}

TEST_F(CompilerTest, test_dummy_closure)
{
	using namespace make_ast;

	auto a = store.make<Symbol>("a"),
		b = store.make<Symbol>("b");

	Parameter p_a(0), p_b(0);
	ClosureParameter cp_a(0);

	auto outer = store.make<LambdaMetadata>
		(*store(mk(a.any)));


	auto inner = store.make<LambdaMetadata>
		(*store(mk(b.any)));

	inner->closure_parameter("a", wrap<Parameter>(0));

	// (\ (a) (\ (b) (+ a b)))
	auto expr = store
		(mk(wrap<Lambda>(outer.pointer()),
		    mk(a.pointer()),
		    (mk(wrap<Lambda>(inner.pointer()),
		        mk(b.pointer()),
		        mk(add, wrap(cp_a), wrap(p_b))))));

	compile.compile(expr);

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

	auto a = store.make<Symbol>("a"),
		b = store.make<Symbol>("b");

	Parameter p_a(0), p_b(0);
	ClosureParameter cp_a(0);

	auto outer = store.make<LambdaMetadata>
		(store.raw_ast(mk(a.any)));

	auto inner = store.make<LambdaMetadata>
		(store.raw_ast(mk(b.any)));
	inner->closure_parameter("a", wrap<Parameter>(0));

	// (\ (a) (\ (b) (+ a b)))
	auto expr = store
		(mk(mk(mk(wrap<Lambda>(outer.pointer()), mk(a.any),
		          mk(wrap<Lambda>(inner.pointer()), mk(b.any),
		             mk(add, wrap(cp_a), wrap(p_b)))),
		       1),
		    2));

	compile.compile(expr);

	ASSERT_EQ(3, run());
}

TEST_F(CompilerTest, test_two_arg_closure)
{
	using namespace make_ast;

	auto a = store.make<Symbol>("a"),
		b = store.make<Symbol>("b"),
		c = store.make<Symbol>("c");

	Parameter p_a(0), p_b(1), p_c(0);
	ClosureParameter cp_a(0), cp_b(1);

	auto outer = store.make<LambdaMetadata>
		(store.raw_ast(mk(a.any, b.any)));

	auto inner = store.make<LambdaMetadata>
		(store.raw_ast(mk(c.any)));

	inner->closure_parameter("a", wrap<Parameter>(0));
	inner->closure_parameter("b", wrap<Parameter>(1));

	auto expr = store
		(mk(mk(mk(wrap<Lambda>(&*outer), mk(a.any, b.any),
		          (mk(wrap<Lambda>(inner.pointer()), mk(c.any),
		              mk(add3, wrap(cp_a), wrap(cp_b), wrap(p_c))))),
		       1, 2),
		    3));

	compile.compile(expr);

	ASSERT_EQ(6, run());
}

TEST_F(CompilerTest, test_define_first)
{
	using namespace make_ast;

	auto foo = store.make<Symbol>("foo"),
		a = store.make<Symbol>("a"),
		b = store.make<Symbol>("b"),
		c = store.make<Symbol>("c");

	Parameter p_a(0), p_b(1), p_c(2);

	foo->slot = 0;
	foo->scheme.type = wrap
		(store.raw_ast(fn_type::fn(tag<Fixnum>::value,
		                           tag<Fixnum>::value,
		                           tag<Fixnum>::value,
		                           tag<Fixnum>::value)));

	auto foo_info = store.make<LambdaMetadata>
		(store.raw_ast(mk(a.any, b.any, c.any)));

	// (define foo (\ (a b c) (+ (+ a b) c)))
	compile.compile
		(store
		 (mk(wrap<Define>(),
		     foo.any,
		     mk(wrap<Lambda>(foo_info.pointer()), mk(a.any, b.any, c.any),
		        mk(add, mk(add, p_a, p_b), p_c)))));

	compile.compile(store(mk(foo.any, 5, 3, 2)));

	ASSERT_EQ(10, run());
}

TEST_F(CompilerTest, test_define_after)
{
	using namespace make_ast;

	auto do_it = store.make<Symbol>("do_it"),
		foo = store.make<Symbol>("foo"),
		a = store.make<Symbol>("a"),
		b = store.make<Symbol>("b");
	Parameter p_a(0), p_b(1);

	do_it->slot = 0;
	do_it->scheme.type = wrap
		(store.raw_ast
		 (fn_type::fn(tag<Fixnum>::value)));

	foo->slot = 1;
	foo->scheme.type = wrap
		(store.raw_ast
		 (fn_type::fn(tag<Fixnum>::value,
		              tag<Fixnum>::value,
		              tag<Fixnum>::value)));

	auto do_it_info = store.make<LambdaMetadata>
		(store.raw_ast(mk()));

	auto foo_info = store.make<LambdaMetadata>
		(store.raw_ast(mk(a.any, b.any)));

	compile.compile
		(store
		 (mk(wrap<Define>(),
		     do_it.any,
		     mk(wrap<Lambda>(do_it_info.pointer()), mk(), mk(foo.any, 5, 2)))));

	// (define foo (\ (a b) (- a b)))
	compile.compile
		(store
		 (mk(wrap<Define>(),
		     foo.any,
		     mk(wrap<Lambda>(foo_info.pointer()), mk(a.any, b.any),
		        mk(sub, p_a, p_b)))));

	compile.compile(store(mk(do_it.any)));

	ASSERT_EQ(3, run());
}
