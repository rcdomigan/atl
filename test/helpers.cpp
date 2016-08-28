#include <gtest/gtest.h>

#include <helpers.hpp>
#include <conversion.hpp>
#include <gc.hpp>

#include <print.hpp>

using namespace atl;

struct TestHelpers : public ::testing::Test {
	TestHelpers() { init_types(); }
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

		auto sym_matcher = a<Symbol>();
		ASSERT_TRUE(sym_matcher(ref_wrap(sym_expr)));
		ASSERT_FALSE(sym_matcher(ref_wrap(ast_expr)));

		auto ast_matcher = ast(a<Lambda>(), ast(a<Symbol>()));
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

		auto matcher = ast(a<Lambda>(),
		                   a<Symbol>(),
		                   a<Symbol>());

		auto first = matcher(ref_wrap(matching_expr));
		ASSERT_TRUE(first.is_match);
		ASSERT_EQ("a", unwrap<Symbol>(first[0]).name);

		ASSERT_FALSE(matcher(ref_wrap(not_matching_expr)));
	}
}

TEST_F(TestHelpers, test_nested_pattern_matcher)
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

		auto matcher = ast
			(a<Lambda>(),
			 whatever(),
			 ast(capture<Symbol>(),
			     capture<Fixnum>()));

		auto first = matcher(ref_wrap(matching_expr));
		ASSERT_TRUE(first);
		ASSERT_EQ("a", unwrap<Symbol>(first[0]).name);
		ASSERT_EQ(3, unwrap<Fixnum>(first[1]).value);

		ASSERT_FALSE(matcher(ref_wrap(not_matching_expr)));
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
		ASSERT_EQ(make(lift<Fixnum>(1),
		               make(lift<Fixnum>(3)),
		               lift<Fixnum>(2))(ast_alloc(arena)),
		          expr);
	}
}

TEST_F(TestHelpers, test_make_ast_mk)
{
	using namespace make_ast;
	Arena store;

	auto ast = make(lift<Fixnum>(1),
	                make(lift<Fixnum>(2),
	                     lift<Fixnum>(3)),
	                lift<Fixnum>(4))(ast_alloc(store));

	ASSERT_EQ(ast,
	          mk(1, mk(2, 3), 4)(ast_alloc(store)));
}

TEST_F(TestHelpers, test_make_ast_mk_sym)
{
	using namespace make_ast;
	Arena store;

	auto ast = make(sym("a"),
	                make(sym("b"),
	                     sym("c")),
	                sym("d"))(ast_alloc(store));

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
		 Slice(pre),
		 ast_alloc(store));

	ASSERT_EQ(post, mk(3, 4)(ss()));
}

TEST_F(TestHelpers, test_ast_hof_copy)
{
	Arena store;

	Ast pre;
	{
		using namespace make_ast;
		pre = make(sym("a"),
		           sym("b"))(ast_alloc(store));
	}

	auto post = *ast_hof::copy(pre, make_ast::ast_alloc(store));

	ASSERT_EQ(pre, post);
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

	dbg_ast(pre);
	dbg_ast(post);

	ASSERT_EQ(pre, post);
}

TEST_F(TestHelpers, test_pass_value)
{
	using namespace make_ast;
	Arena store;
	auto ss = [&]() { return ast_alloc(store); };

	auto ast = mk(1, mk(2, 3), 4)(ss());
	Slice slice(ast);

	AstSubscripter sub(pass_value(slice));

	ASSERT_EQ(3, unwrap<Fixnum>(sub[1][1].value).value);
	ASSERT_EQ(4, unwrap<Fixnum>(sub[2].value).value);
}

TEST_F(TestHelpers, test_fn_type)
{
	using namespace fn_type;
	using make_ast::ast_alloc;

	Arena store;

	auto ast_store = ast_alloc(store);
	auto type = fn(tt<Bool>(), 1, 1)(ast_store);

	std::cout << printer::print(type) << std::endl;
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

TEST_F(TestHelpers, test_unwrap_slice)
{
	using namespace make_ast;
	Arena arena;

	auto expr = mk("define",
	               "main",
	               mk(wrap<Lambda>(),
	                  mk(),
	                  mk("my-add3", 2, 3, 7)))
		(ast_alloc(arena));

	auto& value = expr[2];
	auto slice = unwrap_slice(value);

	ASSERT_EQ(tag<Lambda>::value, slice[0]._tag);
	ASSERT_EQ(tag<AstData>::value, slice[1]._tag);
	ASSERT_EQ(tag<AstData>::value, slice[2]._tag);
}
