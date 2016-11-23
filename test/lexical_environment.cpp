/**
 * @file /home/ryan/programming/atl/test/lexical_environment.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Oct 25, 2015
 *
 * Test the lexical environment, in particular finding free variables.
 */


#include <lexical_environment.hpp>
#include <gc.hpp>

#include <iostream>
#include <gtest/gtest.h>
#include <print.hpp>
#include <helpers.hpp>
#include <helpers/pattern_match.hpp>

using namespace std;
using namespace atl;


struct TestToplevelMap
	: public ::testing::Test
{
	Arena store;
	SymbolMap env;
	BackPatch backpatch;

	GC gc;

	void do_assign_free(Any& thing)
	{ assign_free(env, gc, backpatch, thing); }

	Any do_assign_forms(Any thing)
	{ return assign_forms(env, AstAllocator(gc), thing); }

	template<class T>
	Any do_assign_forms(T thing)
	{ return do_assign_forms(wrap(thing)); }

	Any do_assign(Any thing)
	{
		thing = do_assign_forms(thing);
		do_assign_free(thing);
		return thing;
	}

	template<class T>
	Any do_assign(T thing)
	{ return do_assign(wrap(thing)); }

	TestToplevelMap()
		: env(store)
	{
		init_types();
		setup_basic_definitions(gc, env);
	}
};


TEST_F(TestToplevelMap, test_assign_forms)
{
	using namespace make_ast;

	auto expr = do_assign_forms
		(store
		 (mk
		  (wrap<Lambda>(),
		   mk("a", "b"),
		   mk("a", "b", "c"))));

    {
	    using namespace pattern_match;

	    Lambda fn(nullptr);

	    ASSERT_TRUE
		    (match(ast(capture(fn),
		               ast("a", "b"),
		               ast("a", "b", "c")),
		           wrap(expr)))
		    << printer::with_type(expr);

	    ASSERT_NE(nullptr, fn.value);
	    ASSERT_EQ(2, fn.value->formals.size());
    }
}

TEST_F(TestToplevelMap, test_assigning_bound_lambda)
{
	using namespace make_ast;

	auto expr = do_assign
		(store
		 (mk
		  (wrap<Lambda>(),
		   mk("a", "b"),
		   mk("a", "b", "c"))));

	/* On the toplevel, only "c" should be free */
    ASSERT_EQ(1, backpatch.size());

    {
	    using namespace pattern_match;

	    Symbol const *a, *b;
	    Symbol c;
	    Lambda fn(nullptr);
	    Parameter param_a(-1), param_b(-1);

	    ASSERT_TRUE
		    (match(ast(capture(fn),
		               ast(capture_ptr(a), capture_ptr(b)),
		               ast(capture(param_a), capture(param_b), capture(c))),
		           wrap(expr)))
		    << printer::with_type(expr);

	    ASSERT_EQ(0, param_a.value);
	    ASSERT_EQ(a, &unwrap<Symbol>(fn.value->formals[param_a.value]));

	    ASSERT_EQ(1, param_b.value);
	    ASSERT_EQ(b, &unwrap<Symbol>(fn.value->formals[param_b.value]));

	    ASSERT_TRUE(is<Undefined>(c.value));
    }
}


TEST_F(TestToplevelMap, test_toplevel_replacements)
{
	using namespace make_ast;

	auto expr = mk(wrap<Lambda>(),
	               mk("a", "b"),
	               mk("a", "b", "c"))
		(ast_alloc(gc)),

		assign = mk(wrap<Define>(), "c", 3)(ast_alloc(gc));

	do_assign_forms(ref_wrap(expr));
	do_assign_free(ref_wrap(expr));

	do_assign_forms(ref_wrap(expr));
	do_assign_free(ref_wrap(assign));

	ASSERT_EQ(0, backpatch.size());

    {
	    using namespace pattern_match;

	    Symbol const *a, *b, *c;
	    Parameter param_a(0), param_b(0);

	    ASSERT_TRUE
		    (match(ast("fn", ast(capture_ptr(a), capture_ptr(b)),
		               ast(capture(param_a), capture(param_b), capture_ptr(c))),
		           wrap(expr)))
		    << printer::with_type(expr);

	    ASSERT_EQ(0, param_a.value);
	    ASSERT_EQ(1, param_b.value);

	    ASSERT_TRUE(is<Fixnum>(c->value));
	    ASSERT_EQ(3, unwrap<Fixnum>(c->value).value);
    }
}

TEST_F(TestToplevelMap, test_define_form)
{
	using namespace make_ast;

	auto expr = do_assign_forms
		(gc
		 (mk("define",
		     "recur",
		     mk("__\\__", mk(), mk("recur")))));

	{
		using namespace pattern_match;

		ASSERT_TRUE(match(ast(tag<Define>::value, tag<Symbol>::value,
		                      ast(tag<Lambda>::value, ast(), tag<Ast>::value)),
		                  ref_wrap(expr)))
			<< "got: " << printer::print(expr) << std::endl;
	}
}

TEST_F(TestToplevelMap, test_getting_definitions)
{
	using namespace make_ast;

	auto expr = mk("define",
	               "recur",
	               mk("__\\__",
	                  mk(),
	                  mk("recur")))
		(ast_alloc(gc));
	auto wrapped = wrap(expr);

	BackPatch backpatch;
	do_assign_free(wrapped);

	{
		using namespace pattern_match;
		Symbol const* formal_recur, *body_recur;

		ASSERT_TRUE(match(ast(tag<Define>::value, capture_ptr(formal_recur),
		                      ast(tag<Lambda>::value, ast(),
		                          ast(capture_ptr(body_recur)))),
		                  wrapped))
			<< "got: " << printer::print(wrapped) << std::endl;

		ASSERT_EQ(body_recur, formal_recur);

		// No inference occured, so the symbol's type should still be 'Undefined'
		ASSERT_TRUE(match(tt<Undefined>(), formal_recur->scheme.type))
			<< "expr: " << printer::print(formal_recur->scheme.type) << std::endl;
	}
}

TEST_F(TestToplevelMap, test_lambda_with_if)
{
	using namespace make_ast;
    auto expr = do_assign
	    (store
	     (mk
	      (mk(wrap<Lambda>(),
	          mk("a", "b"),
	          mk(wrap<If>(),
	             mk("equal2", "a", "b"),
	             mk("add2", "a", "b"),
	             mk("sub2", "a", "b"))),
	       7, 3)));

    {
	    using namespace pattern_match;
	    ASSERT_TRUE
		    (match(ast(ast(tag<Lambda>::value,
		                   types<Symbol, Symbol>(),
		                   ast(tag<If>::value,
		                       types<Symbol, Parameter, Parameter>(),
		                       types<Symbol, Parameter, Parameter>(),
		                       types<Symbol, Parameter, Parameter>())),
		               wrap<Fixnum>(7), wrap<Fixnum>(3)),
		           wrap(expr)))
		    << printer::print(expr) << std::endl;
    }
}

TEST_F(TestToplevelMap, test_closure_params)
{
	using namespace make_ast;

	auto expr = do_assign
		(store
		 (mk
		  (wrap<Lambda>(),
		   mk("a"),
		   mk(wrap<Lambda>(), mk("b"), mk("a", "b")))));

    {
	    using namespace pattern_match;

	    Symbol const *a, *b;
	    ClosureParameter param_a(-1);
	    Parameter param_b(-1);

	    ASSERT_TRUE
		    (match(ast(tag<Lambda>::value,
		               ast(capture_ptr(a)),
		               ast(tag<Lambda>::value,
		                   ast(capture_ptr(b)),
		                   ast(capture(param_a), capture(param_b)))),
		           wrap(expr)))
		    << printer::print(expr);

	    ASSERT_EQ(0, param_a.value);
	    ASSERT_EQ(0, param_b.value);
    }
}
