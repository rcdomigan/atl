#include <gtest/gtest.h>

#include <helpers/pattern_match.hpp>
#include <helpers.hpp>
#include <conversion.hpp>
#include <gc.hpp>

#include <print.hpp>

using namespace atl;

struct TestHelpers
	: public ::testing::Test
{
	GC store;

	TestHelpers() { init_types(); }
};

TEST_F(TestHelpers, test_nested_mk)
{
	using namespace make_ast;

	auto ast_raii = store(mk(mk(1, mk(), 1)));
	Ast ast = *ast_raii;

	ASSERT_EQ(tag<Ast>::value, ast[0]._tag);

	ASSERT_EQ(tag<Fixnum>::value,
	          unwrap<Ast>(ast[0])[0]._tag);

	ASSERT_EQ(tag<Ast>::value,
	          unwrap<Ast>(ast[0])[1]._tag);
}

TEST_F(TestHelpers, test_trivial_pattern_match)
{
	using namespace make_ast;
	auto ast_raii = store(mk(wrap<Lambda>(), mk("a")));
	Any ast_expr = wrap(*ast_raii);

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
	using namespace make_ast;
	auto ast = store(mk());

	{
		namespace pm = pattern_match;

		ASSERT_TRUE(match(pm::ast(), wrap(*ast)));
		ASSERT_FALSE(match(pm::ast("foo"), wrap(*ast)));
	}
}

TEST_F(TestHelpers, test_pattern_match_capture)
{
	using namespace make_ast;
	auto expr = store(mk("a"));
	Symbol sym;

	{
		using namespace pattern_match;

		ASSERT_TRUE(match(ast(capture(sym)), wrap(*expr)));
		ASSERT_EQ("a", sym.name);
	}
}



TEST_F(TestHelpers, test_make_ast_mk)
{
	using namespace make_ast;

	auto backer = store.ast_builder();

	{
		NestAst nest(backer);
		backer.push_back(wrap<Fixnum>(1));
		{
			NestAst inner(backer);
			backer.push_back(wrap<Fixnum>(2));
			backer.push_back(wrap<Fixnum>(3));
		}
		backer.push_back(wrap<Fixnum>(4));
	}

	auto expr = store(mk(1, mk(2, 3), 4));
	ASSERT_EQ(backer.root(), *expr);
}

TEST_F(TestHelpers, test_make_ast_mk_sym)
{
	using namespace make_ast;

	auto backer = store.ast_builder();

	{
		NestAst nest(backer);
		backer.push_back(wrap(store.raw_make<Symbol>("a")));
		{
			NestAst inner(backer);
			backer.push_back(wrap(store.raw_make<Symbol>("b")));
			backer.push_back(wrap(store.raw_make<Symbol>("c")));
		}
		backer.push_back(wrap(store.raw_make<Symbol>("d")));
	}

	auto expr = store(mk("a", mk("b", "c"), "d"));
	ASSERT_EQ(backer.root(), *expr);
}

TEST_F(TestHelpers, test_hof_map)
{
	using namespace make_ast;

	auto pre = store(mk(1, 2));
	auto post = store
		(ast_hof::map
		 ([](Any const& vv) { return wrap<Fixnum>(unwrap<Fixnum>(vv).value + 2); },
		  *pre));

	auto expr = store(mk(3, 4));
	ASSERT_EQ(*post, *expr);
}

TEST_F(TestHelpers, test_ast_hof_copy)
{
	using namespace make_ast;

	auto pre = store(mk("a", "b"));
	auto post = store(ast_hof::copy(*pre));

	ASSERT_EQ(*pre, *post);
	ASSERT_NE(pre.any.value, post.any.value);
}

TEST_F(TestHelpers, test_ast_hof_nested_copy)
{
	using namespace make_ast;

	auto pre = store(mk("a", "b", mk("c", "d")));

	ASSERT_EQ(*pre, *store(ast_hof::copy(*pre)));
}

TEST_F(TestHelpers, test_fn_type)
{
	using namespace fn_type;

	auto type = store(fn(tt<Bool>(), tag<Any>::value, tag<Any>::value));

	{
		using namespace pattern_match;
		ASSERT_TRUE
			(match(fnt(tt<Bool>(), fnt(tt<Any>(), tt<Any>())),
			       wrap(*type)))
			<< printer::print(*type) << std::endl;
	}
}

TEST_F(TestHelpers, test_nested_fn_type)
{
	using namespace fn_type;

	auto type = store
		(fn(fn(tag<Any>::value, tag<Fixnum>::value), tag<Symbol>::value, tag<Symbol>::value));

	{
		using namespace pattern_match;
		ASSERT_TRUE
			(match(fnt(fnt(tt<Any>(), tt<Fixnum>()),
			           fnt(tt<Symbol>(), tt<Symbol>())),
			       wrap(*type)))
			<< printer::print(*type) << std::endl;
	}
}

TEST_F(TestHelpers, test_fn_type_thunk)
{
	using namespace fn_type;

	auto ast_store = store.ast_builder();
	fn(tag<Fixnum>::value)(ast_store);

	{
		using namespace pattern_match;
		ASSERT_TRUE
			(match(fnt(tt<Fixnum>()),
			       wrap(ast_store.root())))
			<< printer::print(ast_store.root()) << std::endl;
	}
}

TEST_F(TestHelpers, test_subscript)
{
	using namespace make_ast;

	auto raii = store(mk("define",
	                     mk(wrap<Lambda>(), 1)));
	auto& expr = *raii;

	ASSERT_TRUE(is<Symbol>(subscript(expr, 0)));
	ASSERT_TRUE(is<Lambda>(subscript(expr, 1, 0)));
	ASSERT_TRUE(is<Fixnum>(subscript(expr, 1, 1)));
}
