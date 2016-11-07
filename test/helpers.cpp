#include <gtest/gtest.h>

#include <helpers/pattern_match.hpp>
#include <helpers.hpp>
#include <conversion.hpp>
#include <gc.hpp>

#include <print.hpp>

using namespace atl;

struct TestHelpers : public ::testing::Test {
	TestHelpers() { init_types(); }
};

TEST_F(TestHelpers, test_nested_mk)
{
	using namespace make_ast;
	Arena arena;

	auto ast = mk(mk(1, mk(), 1))
		(ast_alloc(arena));

	ASSERT_EQ(tag<Ast>::value, ast[0]._tag);

	ASSERT_EQ(tag<Fixnum>::value,
	          unwrap<Ast>(ast[0])[0]._tag);

	ASSERT_EQ(tag<Ast>::value,
	          unwrap<Ast>(ast[0])[1]._tag);
}

TEST_F(TestHelpers, test_trivial_pattern_match)
{
	Arena arena;
	Any ast_expr;
	{
		using namespace make_ast;
		ast_expr = wrap
			(mk(wrap<Lambda>(), mk("a"))
			 (ast_alloc(arena)));
	}

	{
		using namespace pattern_match;

		ASSERT_TRUE(match(ast(tag<Lambda>::value, ast(tag<Symbol>::value)), ast_expr));
		ASSERT_FALSE(match(ast(tag<Lambda>::value, ast(tag<Lambda>::value)), ast_expr));
		ASSERT_FALSE(match(ast(tag<Lambda>::value), ast_expr));
		ASSERT_FALSE(match(ast(tag<Lambda>::value, ast(tag<Symbol>::value, tag<Symbol>::value)), ast_expr));
	}
}

TEST_F(TestHelpers, test_pattern_match_empty_ast)
{
	Arena arena;
	Any ast_expr;
	{
		using namespace make_ast;
		ast_expr = wrap(mk()(ast_alloc(arena)));
	}

	{
		using namespace pattern_match;

		ASSERT_TRUE(match(ast(), ast_expr));
		ASSERT_FALSE(match(ast("foo"), ast_expr));
	}
}

TEST_F(TestHelpers, test_pattern_match_capture)
{
	Arena arena;
	Any ast_expr;
	{
		using namespace make_ast;
		ast_expr = wrap(mk()(ast_alloc(arena)));
	}

	{
		using namespace pattern_match;

		ASSERT_TRUE(match(ast(), ast_expr));
		ASSERT_FALSE(match(ast("foo"), ast_expr));
	}
}

TEST_F(TestHelpers, test_NestAst)
{
	using namespace ast_hof;
	Arena arena;
	AstAllocator store = make_ast::ast_alloc(arena);
	Ast expr;

	{
		NestAst nest(store);
		store.push_back(wrap<Fixnum>(1));
		{
			NestAst inner(store);
			store.push_back(wrap<Fixnum>(3));
		}
		store.push_back(wrap<Fixnum>(2));
		expr = *nest.ast;
	}

	{
		using namespace make_ast;
		ASSERT_EQ(mk(1, mk(3), 2)(ast_alloc(arena)),
		          expr);
	}
}

TEST_F(TestHelpers, test_make_ast_mk)
{
	using namespace make_ast;
	Arena store;
	Ast ast;

	{
		auto backer = ast_alloc(store);
		NestAst nest(backer);
		backer.push_back(wrap<Fixnum>(1));
		{
			NestAst inner(backer);
			backer.push_back(wrap<Fixnum>(2));
			backer.push_back(wrap<Fixnum>(3));
		}
		backer.push_back(wrap<Fixnum>(4));
		ast = *nest.ast;
	}

	ASSERT_EQ(ast,
	          mk(1, mk(2, 3), 4)(ast_alloc(store)));
}

TEST_F(TestHelpers, test_make_ast_mk_sym)
{
	using namespace make_ast;
	Arena store;
	Ast ast;

	{
		auto backer = ast_alloc(store);
		NestAst nest(backer);
		backer.push_back(wrap(store.symbol("a")));
		{
			NestAst inner(backer);
			backer.push_back(wrap(store.symbol("b")));
			backer.push_back(wrap(store.symbol("c")));
		}
		backer.push_back(wrap(store.symbol("d")));
		ast = *nest.ast;
	}

	ASSERT_EQ(ast,
	          mk("a", mk("b", "c"), "d")(ast_alloc(store)));
}

TEST_F(TestHelpers, test_hof_map)
{
	using namespace make_ast;
	Arena store;
	auto ss = [&]() { return ast_alloc(store); };
	auto pre = mk(1, 2)(ss());

	auto post = *ast_hof::map
		([](Any const& vv) { return wrap<Fixnum>(unwrap<Fixnum>(vv).value + 2); },
		 pre,
		 ast_alloc(store));

	ASSERT_EQ(post, mk(3, 4)(ss()));
}

TEST_F(TestHelpers, test_ast_hof_copy)
{
	Arena store;

	Ast pre;
	{
		using namespace make_ast;
		pre = mk("a", "b")(ast_alloc(store));
	}

	auto post = *ast_hof::copy(pre, make_ast::ast_alloc(store));

	ASSERT_EQ(pre, post);
	ASSERT_NE(&pre, &post);
}

TEST_F(TestHelpers, test_ast_hof_nested_copy)
{
	Arena store;

	Ast pre;
	{
		using namespace make_ast;
		pre = mk("a",
		         "b",
		         mk("c", "d"))(ast_alloc(store));
	}

	auto post = *ast_hof::copy(pre, make_ast::ast_alloc(store));

	ASSERT_EQ(pre, post);
}

TEST_F(TestHelpers, test_fn_type)
{
	using namespace fn_type;
	using make_ast::ast_alloc;

	Arena store;

	auto ast_store = ast_alloc(store);
	auto type = fn(tt<Bool>(), tag<Any>::value, tag<Any>::value)(ast_store);

	{
		using namespace pattern_match;
		ASSERT_TRUE
			(match(fnt(tt<Bool>(), fnt(tt<Any>(), tt<Any>())),
			       wrap(type)))
			<< printer::print(type) << std::endl;
	}
}

TEST_F(TestHelpers, test_nested_fn_type)
{
	using namespace fn_type;
	using make_ast::ast_alloc;

	Arena store;

	auto ast_store = ast_alloc(store);
	auto type = fn(fn(tag<Any>::value, tag<Fixnum>::value), tag<Symbol>::value, tag<Symbol>::value)(ast_store);

	{
		using namespace pattern_match;
		ASSERT_TRUE
			(match(fnt(fnt(tt<Any>(), tt<Fixnum>()),
			           fnt(tt<Symbol>(), tt<Symbol>())),
			       wrap(type)))
			<< printer::print(type) << std::endl;
	}
}

TEST_F(TestHelpers, test_fn_type_thunk)
{
	using namespace fn_type;
	using make_ast::ast_alloc;

	Arena store;

	auto ast_store = ast_alloc(store);
	auto type = fn(tag<Fixnum>::value)(ast_store);

	{
		using namespace pattern_match;
		ASSERT_TRUE
			(match(fnt(tt<Fixnum>()),
			       wrap(type)))
			<< printer::print(type) << std::endl;
	}
}

TEST_F(TestHelpers, test_subscript)
{
	using namespace make_ast;
	Arena arena;

	auto expr = mk("define",
	               mk(wrap<Lambda>(), 1))
		(ast_alloc(arena));

	ASSERT_TRUE(is<Symbol>(subscript(expr, 0)));
	ASSERT_TRUE(is<Lambda>(subscript(expr, 1, 0)));
	ASSERT_TRUE(is<Fixnum>(subscript(expr, 1, 1)));
}
