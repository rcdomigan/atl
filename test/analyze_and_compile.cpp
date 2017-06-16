/**
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Nov 22, 2014
 *
 * Run an Ast of symbols through the type_inference,
 * lexical_environment, and compile.
 */

#include <gtest/gtest.h>

#include <atl/atl.hpp>

#include "./testing_utils.hpp"
#include "./trivial_functions.hpp"


using namespace atl;


struct AnalyzeAndCompile
	: public ::testing::Test
{
	Atl intrp;
	unittest::TrivialFunctions fns;

	AnalyzeAndCompile()
		: fns(intrp.gc)
	{ fns.add_to_env(intrp.lexical); }
};


TEST_F(AnalyzeAndCompile, test_simple_statement)
{ ASSERT_EQ(wrap<Fixnum>(3), intrp.eval("(add2 1 2)")); }

TEST_F(AnalyzeAndCompile, test_type_inference)
{
	using namespace make_ast;
	auto expr = intrp.gc(mk("if",
	                        mk("equal2", 1, 2),
	                        mk("add2", 7, 3),
	                        mk("sub2", 7, 3)));

	auto type = intrp.annotate(*expr);

	using namespace make_type;
	ASSERT_EQ(tt<Fixnum>(), type);
}

TEST_F(AnalyzeAndCompile, test_if_false)
{
	using namespace make_ast;
	ASSERT_EQ(wrap<Fixnum>(4),
	          intrp.eval_ast(mk("if",
	                            mk("equal2", 1, 2),
	                            mk("add2", 7, 3),
	                            mk("sub2", 7, 3))));
}

TEST_F(AnalyzeAndCompile, test_if_true)
{
	using namespace make_ast;
	ASSERT_EQ(wrap<Fixnum>(10),
	          intrp.eval_ast(mk("if",
	                            mk("equal2", 2, 2),
	                            mk("add2", 7, 3),
	                            mk("sub2", 7, 3))));
}

TEST_F(AnalyzeAndCompile, test_lambda_with_if)
{
	using namespace make_ast;
	ASSERT_EQ(wrap<Fixnum>(4),
	          intrp.eval_ast(mk
	                         (mk(wrap<Lambda>(),
	                             mk("a", "b"),
	                             mk("if",
	                                mk("equal2", "a", "b"),
	                                mk("add2", "a", "b"),
	                                mk("sub2", "a", "b"))),
	                          7, 3)));
}

// TEST_F(AnalyzeAndCompile, test_basic_define)
// {
// 	using namespace make_ast;

// 	// Test that defining a constant works
// 	intrp.eval_ast(mk(wrap<Define>(), "foo", 3));
// 	ASSERT_EQ(wrap<Fixnum>(6),
// 	          intrp.eval_ast(mk("add2", "foo", "foo")));
// }

TEST_F(AnalyzeAndCompile, test_applying_defined_lambda)
{
	using namespace make_ast;
	intrp.eval_ast
		(mk("define",
		    "my-add3",
		    mk(wrap<Lambda>(), mk("a", "b", "c"),
		       mk("add2",
		          "a",
		          mk("add2", "b", "c")))));

	ASSERT_EQ(wrap<Fixnum>(12),
	          intrp.eval_ast(mk("my-add3", 2, 3, 7)));
}

TEST_F(AnalyzeAndCompile, test_applying_defined_thunk)
{
	using namespace make_ast;
	intrp.eval_ast
		(mk("define",
		    "foo",
		    mk(wrap<Lambda>(),
		       mk(),
		       mk("add2", 1, 2))));

	ASSERT_EQ
		(wrap<Fixnum>(3),
		 intrp.eval_ast(mk("foo")));
}

TEST_F(AnalyzeAndCompile, test_nested_function)
{
	using namespace make_ast;
	intrp.eval_ast(mk
	        ("define", "foo", mk(wrap<Lambda>(),
	                             mk("a"),
	                             mk("add2", "a", 3))));

	intrp.eval_ast(mk("define", "bar",
	           mk(wrap<Lambda>(),
	              mk(),
	              mk("foo", 2))));

	ASSERT_EQ(wrap<Fixnum>(5), intrp.eval_ast(mk("bar")));
}

TEST_F(AnalyzeAndCompile, test_two_nested_functions)
{
	using namespace make_ast;
	intrp.eval_ast
		(mk
		 ("define", "foo",
		  mk(wrap<Lambda>(), mk("a"), mk("add2", "a", 3))));

	intrp.eval_ast
		(mk
		 ("define", "bar",
		  mk(wrap<Lambda>(),
		     mk("a", "b"),
		     mk("add2", "a", "b"))));

	intrp.eval_ast
		(mk
		 ("define", "main",
		  mk
		  (wrap<Lambda>(),
		   mk(),
		   mk("bar",
		      mk("foo", 2),
		      mk("foo", 3)))));


	ASSERT_EQ(wrap<Fixnum>(11),
	          intrp.eval_ast(mk("main")));
}

TEST_F(AnalyzeAndCompile, test_simple_recursion)
{
	using namespace make_ast;
	intrp.eval_ast
		(mk
		 ("define", "simple-recur",
		  mk(wrap<Lambda>(),
		     mk("a", "b"),
		     mk("if",
		        mk("equal2", 0, "a"),
		        "b",
		        mk("simple-recur",
		           mk("sub2", "a", 1),
		           mk("add2", "b", 1))))));

	intrp.eval_ast
		(mk(wrap<Define>(), "main",
		    mk(wrap<Lambda>(),
		       mk(),
		       mk("simple-recur", 3, 2))));

	ASSERT_EQ(wrap<Fixnum>(5), intrp.eval_ast(mk("main")));
}

TEST_F(AnalyzeAndCompile, test_simple_recursion_results)
{
	using namespace make_ast;

	intrp.eval_ast(mk
	        ("define", "foo",
	         mk(wrap<Lambda>(),
	            mk("a"),
	            mk("add2", "a", 3))));

	intrp.eval_ast(mk
	        ("define", "simple-recur",
	         mk(wrap<Lambda>(),
	            mk("a", "b"),
	            mk("if",
	               mk("equal2", 0, "a"),
	               "b",
	               mk("simple-recur",
	                  mk("sub2", "a", 1),
	                  mk("foo", "b"))))));

	intrp.eval_ast(mk(wrap<Define>(), "main",
	           mk(wrap<Lambda>(),
	              mk(),
	              mk("simple-recur", 2, 2))));

	ASSERT_EQ(wrap<Fixnum>(8),
	          intrp.eval_ast(mk("main")));
}
