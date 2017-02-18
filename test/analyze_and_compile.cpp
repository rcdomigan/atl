/**
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Nov 22, 2014
 *
 * Run an Ast of symbols through the type_inference,
 * lexical_environment, and compile.
 */

#include <gtest/gtest.h>

#include <vm.hpp>
#include <print.hpp>
#include <lexical_environment.hpp>
#include <type_inference.hpp>
#include <compile.hpp>
#include <gc.hpp>

#include "./testing_utils.hpp"
#include "./trivial_functions.hpp"


using namespace atl;


struct AnalyzeAndCompile
	: public ::testing::Test
{
	GC store;
	unittest::TrivialFunctions cxx_fns;
	SymbolMap lexical;

	BackPatch backpatch;

	AssignForms assign_forms;
	AssignFree assign_free;

	inference::Gamma gamma;
	Type::value_type new_types;
	inference::AlgorithmW w;

	Compile compiler;
	TinyVM vm;

	AnalyzeAndCompile() :
		cxx_fns(store)
		, lexical(store)
		, assign_forms(store, lexical)
		, assign_free(store, lexical, backpatch)
		, gamma(store)
		, w(store, new_types, gamma)
		, compiler(store)
		, vm(store)
	{
		init_types();

		new_types = LAST_CONCRETE_TYPE;

		setup_basic_definitions(store, lexical);

		lexical.define("equal2", cxx_fns.weq.any);
		lexical.define("add2", cxx_fns.wadd.any);
		lexical.define("sub2", cxx_fns.wsub.any);
	}

	/**
	 * @tparam T:
	 * @param expr:
	 * @return: Return type of the expression
	 */
	void annotate(Marked<Ast>& expr)
	{
		expr = assign_forms(expr);

		auto type_info = w.W(expr);

		inference::apply_substitution(store,
		                              type_info.subs,
		                              expr.any);

		assign_free(expr);
	}

	void compile(Marked<Ast>& expr)
	{
		annotate(expr);
		compiler.compile(expr);
	}

	void compile(Marked<Ast>&& expr)
	{
		auto copy = std::move(expr);
		compile(copy);
	}

	void compile(ast_composer const& compose)
	{ compile(store(compose)); }

	pcode::value_type run()
	{
		compiler.assemble.finish();
		run_code(vm, compiler.code_store);
		compiler.code_store.pop_back();

		return vm.stack[0];
	}
};

TEST_F(AnalyzeAndCompile, test_simple_statement)
{
	using namespace make_ast;
	auto expr = store(mk("add2", 1, 2));

    compile(expr);
    ASSERT_EQ(3, run());
}

TEST_F(AnalyzeAndCompile, test_if_false)
{
	using namespace make_ast;
	compile(mk("if",
	           mk("equal2", 1, 2),
	           mk("add2", 7, 3),
	           mk("sub2", 7, 3)));

    ASSERT_EQ(4, run());
}

TEST_F(AnalyzeAndCompile, test_if_true)
{
	using namespace make_ast;
	compile(mk("if",
	           mk("equal2", 2, 2),
	           mk("add2", 7, 3),
	           mk("sub2", 7, 3)));

    ASSERT_EQ(10, run());
}

TEST_F(AnalyzeAndCompile, test_lambda_with_if)
{
	using namespace make_ast;
	compile
		(mk
		 (mk(wrap<Lambda>(),
		     mk("a", "b"),
		     mk("if",
		        mk("equal2", "a", "b"),
		        mk("add2", "a", "b"),
		        mk("sub2", "a", "b"))),
		  7, 3));

	ASSERT_EQ(4, run());
}


TEST_F(AnalyzeAndCompile, test_basic_define)
{
	using namespace make_ast;

	// Test that defining a constant works
	compile(mk(wrap<Define>(), "foo", 3));
	compile(mk("add2", "foo", "foo"));

	ASSERT_EQ(6, run());
}

TEST_F(AnalyzeAndCompile, test_applying_defined_lambda)
{
	using namespace make_ast;
	compile(mk("define",
	           "my-add3",
	           mk(wrap<Lambda>(), mk("a", "b", "c"),
	              mk("add2",
	                 "a",
	                 mk("add2", "b", "c")))));

	compile(mk("my-add3", 2, 3, 7));

	ASSERT_EQ(12, run());
}

TEST_F(AnalyzeAndCompile, test_nested_functions)
{
	using namespace make_ast;
	compile(mk
	        ("define", "foo", mk(wrap<Lambda>(),
	                             mk("a"),
	                             mk("add2", "a", 3))));

	compile(mk
	        ("define", "bar",
	         mk(wrap<Lambda>(),
	            mk(),
	            mk("foo", 2))));

	compile(store(mk("bar")));

	ASSERT_EQ(5, run());
}

TEST_F(AnalyzeAndCompile, test_nested_function_results)
{
	using namespace make_ast;
	compile(store
	        (mk
	         ("define", "foo",
	          mk(wrap<Lambda>(), mk("a"), mk("add2", "a", 3)))));

	compile(store
	        (mk
	         ("define", "bar",
	          mk(wrap<Lambda>(),
	             mk("a", "b"),
	             mk("add2", "a", "b")))));

	compile(store
	        (mk
	         ("define", "main",
	          mk
	          (wrap<Lambda>(),
	           mk(),
	           mk("bar",
	              mk("foo", 2),
	              mk("foo", 3))))));

	compile(store(mk("main")));

	ASSERT_EQ(11, run());
}

TEST_F(AnalyzeAndCompile, test_simple_recursion)
{
	using namespace make_ast;
	compile(mk
	        ("define", "simple-recur",
	         mk(wrap<Lambda>(),
	            mk("a", "b"),
	            mk("if",
	               mk("equal2", 0, "a"),
	               "b",
	               mk("simple-recur",
	                  mk("sub2", "a", 1),
	                  mk("add2", "b", 1))))));

	compile(mk(wrap<Define>(), "main",
	           mk(wrap<Lambda>(),
	              mk(),
	              mk("simple-recur", 3, 2))));

	compile(mk("main"));

	ASSERT_EQ(5, run());
}

TEST_F(AnalyzeAndCompile, test_simple_recursion_results)
{
	using namespace make_ast;

	compile(mk
	        ("define", "foo",
	         mk(wrap<Lambda>(),
	            mk("a"),
	            mk("add2", "a", 3))));

	compile(mk
	        ("define", "simple-recur",
	         mk(wrap<Lambda>(),
	            mk("a", "b"),
	            mk("if",
	               mk("equal2", 0, "a"),
	               "b",
	               mk("simple-recur",
	                  mk("sub2", "a", 1),
	                  mk("foo", "b"))))));

	compile(mk(wrap<Define>(), "main",
	           mk(wrap<Lambda>(),
	              mk(),
	              mk("simple-recur", 2, 2))));

	compile(mk("main"));

	ASSERT_EQ(8, run());
}
