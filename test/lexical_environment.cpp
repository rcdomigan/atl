/**
 * @file /home/ryan/programming/atl/test/lexical_environment.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Oct 25, 2015
 *
 * Test the lexical environment, in particular finding free variables.
 */


#include <atl/lexical_environment.hpp>
#include <atl/gc.hpp>

#include <iostream>
#include <gtest/gtest.h>
#include <atl/print.hpp>
#include <atl/helpers.hpp>

using namespace std;
using namespace atl;


struct TestToplevelMap
	: public ::testing::Test
{
	GC gc;
	SymbolMap env;
	BackPatch backpatch;
	AssignFree assign_free;
	AssignForms assign_forms;

	Marked<Ast> do_assign(Marked<Ast>&& thing)
	{
		thing = assign_forms(thing);
		assign_free(thing);
		return std::move(thing);
	}

	TestToplevelMap()
		: env(gc),
		  assign_free(gc, env, backpatch),
		  assign_forms(gc, env)
	{
		init_types();
		setup_basic_definitions(gc, env);
	}
};


TEST_F(TestToplevelMap, test_assign_forms)
{
	using namespace make_ast;

	auto expr = assign_forms
		(gc
		 (mk
		  (wrap<Lambda>(),
		   mk("a", "b"),
		   mk("a", "b", "c"))));

	gc.gc();

    {
	    using namespace pattern_match;

	    Lambda fn(nullptr);

	    ASSERT_TRUE
		    (match(ast(capture(fn),
		               ast("a", "b"),
		               ast("a", "b", "c")),
		           expr))
		    << printer::print(*expr);

	    ASSERT_NE(nullptr, fn.value);
	    ASSERT_EQ(2, fn.value->formals.size());
    }
}

TEST_F(TestToplevelMap, test_assign_free)
{
	using namespace make_ast;

	auto metadata = gc.make<LambdaMetadata>
		(gc(mk("a", "b")));

	auto expr = gc
		(mk
		 (wrap<Lambda>(metadata.pointer()),
		  mk("a", "b"),
		  mk("a", "b", "c")));

	assign_free(expr);

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
		           expr))
		    << printer::print(*expr);

	    ASSERT_EQ(0, param_a.value);
	    ASSERT_EQ(1, param_b.value);
    }
}

TEST_F(TestToplevelMap, test_parameters)
{
	using namespace make_ast;

	auto metadata = gc.make<LambdaMetadata>
		(gc(mk("a", "b")));

	auto expr = do_assign
		(gc
		 (mk
		  (wrap<Lambda>(metadata.pointer()),
		   mk("a", "b"),
		   mk("a", "b", "c"))));

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
		           expr))
		    << printer::print(*expr);

	    ASSERT_EQ(0, param_a.value);
	    ASSERT_EQ(a, &unwrap<Symbol>(fn.value->formals[param_a.value]));

	    ASSERT_EQ(1, param_b.value);
	    ASSERT_EQ(b, &unwrap<Symbol>(fn.value->formals[param_b.value]));
    }
}

TEST_F(TestToplevelMap, test_define_form)
{
	using namespace make_ast;

	auto expr = assign_forms
		(gc
		 (mk("define",
		     "recur",
		     mk("__\\__", mk(), mk("recur")))));

	{
		using namespace pattern_match;

		ASSERT_TRUE(match(ast(tag<Define>::value, tag<Symbol>::value,
		                      ast(tag<Lambda>::value, ast(), tag<Ast>::value)),
		                  expr))
			<< "got: " << printer::print(*expr) << std::endl;
	}
}

TEST_F(TestToplevelMap, test_getting_definitions)
{
	using namespace make_ast;

	auto expr = do_assign
		(gc(mk("define",
		       "recur",
		       mk("__\\__",
		          mk(),
		          mk("recur")))));

	{
		using namespace pattern_match;
		Symbol const *formal_recur;

		ASSERT_TRUE(match(ast(tag<Define>::value, capture_ptr(formal_recur),
		                      ast(tag<Lambda>::value, ast(),
		                          ast(tag<Function>::value))),
		                  expr))
			<< "got: " << printer::print(*expr) << std::endl;
	}
}

TEST_F(TestToplevelMap, test_lambda_with_if)
{
	using namespace make_ast;
    auto expr = do_assign
	    (gc
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
		           expr))
		    << printer::print(*expr) << std::endl;
    }
}

TEST_F(TestToplevelMap, test_closure_params)
{
	using namespace make_ast;

	auto expr = do_assign
		(gc
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
		           expr))
		    << printer::print(*expr);

	    ASSERT_EQ(0, param_a.value);
	    ASSERT_EQ(0, param_b.value);
    }
}

/* A defined sym can't generally have it's value swapped in for the
   symbol (at least not yet).
 */
TEST_F(TestToplevelMap, test_define_leaves_sym)
{
	using namespace make_ast;

	do_assign(gc
	          (mk
	           (wrap<Define>(),
	            "id",
	            mk(wrap<Lambda>(), mk("a"), mk("a")))));

	auto expr = do_assign(gc(mk(wrap<Lambda>(), mk("b"), mk("id", "b"))));

    {
	    using namespace pattern_match;

	    Function id(nullptr);
	    Parameter param_b(-1);

	    ASSERT_TRUE
		    (match(ast(tag<Lambda>::value,
		               tag<Ast>::value,
		               ast(capture(id), capture(param_b))),
		           expr))
		    << printer::print(*expr);

	    ASSERT_EQ(0, param_b.value);
    }
}



TEST_F(TestToplevelMap, test_value_walker)
{
	using namespace make_ast;

	auto expr = gc(mk
	               ("define",
	                "id",
	                mk("__\\__", mk("a"), mk("a"))));

	{
		using namespace pattern_match;

		ASSERT_TRUE
			(match(ast(tag<Define>::value,
			           tag<Symbol>::value,
			           ast(tag<Lambda>::value, tag<Ast>::value, tag<Ast>::value)),
			       walk_values(env, atl::subex(*expr))))
			<< printer::print(*expr);
	}
}

TEST_F(TestToplevelMap, test_define_lambda_in_sym)

{
	using namespace make_ast;

	auto expr = do_assign
		(gc(mk
		    ("define",
		     "id",
		     mk("__\\__", mk("a"), mk("a")))));


	{
		using namespace pattern_match;

		Lambda fn(nullptr);

		ASSERT_TRUE
			(match(ast(tag<Define>::value,
			           tag<Symbol>::value,
			           ast(capture(fn),
			               tag<Ast>::value,
			               tag<Ast>::value)),
			       expr))
			<< printer::print(*expr);

		ASSERT_NE(nullptr, fn.value);
	}
}
