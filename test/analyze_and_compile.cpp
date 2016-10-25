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
#include <passes.hpp>
#include <type_inference.hpp>
#include <compile.hpp>
#include <gc.hpp>

#include "./testing_utils.hpp"
#include "./trivial_functions.hpp"


using namespace atl;


struct AnalyzeAndCompile
	: public ::testing::Test
{
	TrivialFunctions cxx_fns;
	Arena& store;
	SymbolMap lexical;

	inference::Gamma gamma;
	Type::value_type new_types;


	Compile compiler;
	TinyVM vm;

	AnalyzeAndCompile()
		: store(cxx_fns.store)
		, lexical(store)
	{
		init_types();

		new_types = LAST_CONCRETE_TYPE;

		setup_basic_definitions(store, lexical);

		lexical.define(*store.symbol("equal2"), pass_value(wrap(cxx_fns.weq)));
		lexical.define(*store.symbol("add2"), pass_value(wrap(cxx_fns.wadd)));
		lexical.define(*store.symbol("sub2"), pass_value(wrap(cxx_fns.wsub)));
	}

	/**
	 * @tparam T:
	 * @param expr:
	 * @return: Return type of the expression
	 */
	Any annotate(Any& expr)
	{
		BackPatch backpatch;

		assign_forms(lexical, expr);

		auto type_info = inference::W(store, new_types, gamma, pass_value(expr));

		inference::apply_substitution(store,
		                              gamma,
		                              type_info.subs,
		                              expr);

		assign_free(lexical,
		            store,
		            backpatch,
		            expr);

		assign_padding(expr);

		return type_info.type;
	}

	void compile(Any expr)
	{
		annotate(expr);
		compiler.any(expr);

		// execute the main entry point (if applicable)
		if(compiler.code_store.has_main())
			{
				using namespace make_ast;
				compiler.assemble.add_label("__call-main__");

				auto call_main = wrap(mk("main")(ast_alloc(store)));
				annotate(call_main);
				compiler.any(call_main);
			}
	}

	template<class T>
	void compile(T expr)
	{ compile(wrap(expr)); }

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
	auto expr = mk("add2", 1, 2)(ast_alloc(store));

    compile(wrap(expr));
    ASSERT_EQ(3, run());
}

TEST_F(AnalyzeAndCompile, test_if_false)
{
	using namespace make_ast;
	auto expr = mk("if",
	               mk("equal2", 1, 2),
	               mk("add2", 7, 3),
	               mk("sub2", 7, 3))
		(ast_alloc(store));

    compile(wrap(expr));

    ASSERT_EQ(4, run());
}

TEST_F(AnalyzeAndCompile, test_if_true)
{
	using namespace make_ast;
	auto expr = mk("if",
	               mk("equal2", 2, 2),
	               mk("add2", 7, 3),
	               mk("sub2", 7, 3))
		(ast_alloc(store));

    compile(wrap(expr));

    ASSERT_EQ(10, run());
}

TEST_F(AnalyzeAndCompile, test_lambda_with_if)
{
	using namespace make_ast;
	auto expr = mk
		(mk(wrap<Lambda>(),
		    mk("a", "b"),
		    mk("if",
		       mk("equal2", "a", "b"),
		       mk("add2", "a", "b"),
		       mk("sub2", "a", "b"))),
		 7, 3)
		(ast_alloc(store));

	compile(wrap(expr));

	ASSERT_EQ(4, run());
}


TEST_F(AnalyzeAndCompile, test_basic_define)
{
	using namespace make_ast;

	auto def = mk(wrap<Define>(), "foo", 3)(ast_alloc(store)),

		expr = mk("add2", "foo", "foo")(ast_alloc(store));

	// Test that defining a constant works
	compile(wrap(def));
	compile(wrap(expr));

	ASSERT_EQ(6, run());
}

TEST_F(AnalyzeAndCompile, test_applying_defined_lambda)
{
	using namespace make_ast;
	auto define_add3 = mk("define",
	                      "my-add3",
	                      mk(wrap<Lambda>(), mk("a", "b", "c"),
	                         mk("add2",
	                            "a",
	                            mk("add2", "b", "c"))))
		(ast_alloc(store)),

		apply_add3 = mk("my-add3", 2, 3, 7)(ast_alloc(store));

	{
		Compile::SkipBlock skip_definition(compiler.assemble);
		compile(define_add3);
	}
	compile(apply_add3);

	ASSERT_EQ(12, run());
}

TEST_F(AnalyzeAndCompile, test_nested_functions)
{
	using namespace make_ast;
	auto simple_recur = mk
		("define", "foo", mk(wrap<Lambda>(),
		                     mk("a"),
		                     mk("add2", "a", 3)))
		(ast_alloc(store));

	auto atl_main = mk
		("define", "main",
		 mk(wrap<Lambda>(),
		    mk(),
		    mk("foo", 2)))
		(ast_alloc(store));

	compile(simple_recur);
	compile(atl_main);

	ASSERT_EQ(5, run());
}

TEST_F(AnalyzeAndCompile, test_nested_function_results)
{
	using namespace make_ast;
	auto foo = mk
		("define", "foo",
		 mk(wrap<Lambda>(), mk("a"), mk("add2", "a", 3)))
		(ast_alloc(store));

	auto bar = mk
		("define", "bar",
		 mk(wrap<Lambda>(),
		    mk("a", "b"),
		    mk("add2", "a", "b")))
		(ast_alloc(store));

	auto atl_main = mk
		("define", "main",
		 mk
		 (wrap<Lambda>(),
		  mk(),
		  mk("bar",
		     mk("foo", 2),
		     mk("foo", 3))))
		(ast_alloc(store));

	compile(foo);
	compile(bar);
	compile(atl_main);

	ASSERT_EQ(11, run());
}

TEST_F(AnalyzeAndCompile, test_simple_recursion)
{
	using namespace make_ast;
	auto simple_recur = mk
		("define", "simple-recur",
		 mk(wrap<Lambda>(),
		    mk("a", "b"),
		    mk("if",
		       mk("equal2", 0, "a"),
		       "b",
		       mk("simple-recur",
		          mk("sub2", "a", 1),
		          mk("add2", "b", 1)))))
		(ast_alloc(store)),

		main = mk(wrap<Define>(), "main",
		          mk(wrap<Lambda>(),
		             mk(),
		             mk("simple-recur", 3, 2)))
		(ast_alloc(store));

	compile(simple_recur);
	compile(main);

	ASSERT_EQ(5, run());
}

TEST_F(AnalyzeAndCompile, test_simple_recursion_results)
{
	using namespace make_ast;

	auto foo = mk
		("define", "foo",
		 mk(wrap<Lambda>(),
		    mk("a"),
		    mk("add2", "a", 3)))
		(ast_alloc(store)),

		simple_recur = mk
		("define", "simple-recur",
		 mk(wrap<Lambda>(),
		    mk("a", "b"),
		    mk("if",
		       mk("equal2", 0, "a"),
		       "b",
		       mk("simple-recur",
		          mk("sub2", "a", 1),
		          mk("foo", "b")))))
		(ast_alloc(store)),

		main = mk(wrap<Define>(), "main",
		          mk(wrap<Lambda>(),
		             mk(),
		             mk("simple-recur", 2, 2)))
		(ast_alloc(store));

	compile(foo);
	compile(simple_recur);
	compile(main);

	ASSERT_EQ(8, run());
}
