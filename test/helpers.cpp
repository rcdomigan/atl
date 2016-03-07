#include <gtest/gtest.h>

#include <atl.hpp>
#include <helpers.hpp>

#include <print.hpp>

using namespace atl;

struct TestHelpers : public ::testing::Test {
	Atl atl;

    TestHelpers() {}
};

TEST_F(TestHelpers, test_trivial_pattern_matcher)
{
	Arena arena;
	Any sym_expr, ast_expr;
	{
		using namespace make_ast;
		sym_expr = arena.amake<Symbol>("foo");

		ast_expr = wrap
			(make
			 (lift<Lambda>(),
			  make(sym("a")))
			 (ast_alloc(arena)));
	}

	{
		using namespace pattern_matcher;

		auto sym_matcher = match_is<Symbol>();
		ASSERT_TRUE(sym_matcher(wrap(sym_expr)));
		ASSERT_FALSE(sym_matcher(wrap(ast_expr)));

		auto ast_matcher = match_ast(match_is<Lambda>(),
		                             match_ast(match_is<Symbol>()));
		ASSERT_TRUE(ast_matcher(ast_expr).is_match);
	}
}

TEST_F(TestHelpers, test_pattern_matcher)
{
	Arena arena;
	Ast matching_expr, not_matching_expr;
	{
		using namespace make_ast;
		matching_expr = make
			(lift<Lambda>(),
			 sym("a"),
			 sym("a"))
			(ast_alloc(arena));

		not_matching_expr = make
			(sym("Lambda"),
			 sym("a"))
			(ast_alloc(arena));
	}

	{
		using namespace pattern_matcher;

		auto matcher = match_ast
			(match_is<Lambda>(),
			 capture<Symbol>(),
			 match_is<Symbol>());

		auto first = matcher(wrap(matching_expr));
		ASSERT_TRUE(first.is_match);
		ASSERT_EQ("a", unwrap<Symbol>(first[0]).name);

		ASSERT_FALSE(matcher(wrap(not_matching_expr)));
	}
}

TEST_F(TestHelpers, test_nestd_pattern_matcher)
{
	Arena arena;
	Ast matching_expr, not_matching_expr;
	{
		using namespace make_ast;
		matching_expr = make
			(lift<Lambda>(),
			 sym("a"),
			 make(sym("a"),
			      lift<Fixnum>(3)))
			(ast_alloc(arena));

		not_matching_expr = make
			(sym("Lambda"),
			 sym("a"),
			 make(sym("a")))
			(ast_alloc(arena));
	}

	{
		using namespace pattern_matcher;

		auto matcher = match_ast
			(match_is<Lambda>(),
			 whatever(),
			 match_ast(capture<Symbol>(),
			           capture<Fixnum>()));

		auto first = matcher(wrap(matching_expr));
		ASSERT_TRUE(first);
		ASSERT_EQ("a", unwrap<Symbol>(first[0]).name);
		ASSERT_EQ(3, unwrap<Fixnum>(first[1]).value);

		ASSERT_FALSE(matcher(wrap(not_matching_expr)));
	}
}
