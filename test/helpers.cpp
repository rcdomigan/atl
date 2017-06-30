#include <gtest/gtest.h>

#include <atl/helpers/pattern_match.hpp>
#include <atl/helpers.hpp>
#include <atl/wrap.hpp>
#include <atl/gc.hpp>

#include <atl/print.hpp>

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


// The same '(1 (2 3) 4) Ast is going to be wrapped in a couple
// different walkers, so factor out the test steps
void _test_walker(WalkAst& walker)
{
	ASSERT_TRUE(walker.at_begin());
	ASSERT_TRUE(walker.has_value());
	ASSERT_EQ(wrap<Fixnum>(1), walker.value());
	walker.next();

	ASSERT_FALSE(walker.at_begin());
	ASSERT_TRUE(walker.has_value());
	ASSERT_TRUE(walker.is_subex());

	{
		auto sub = walker.walk_subex();

		ASSERT_FALSE(walker.at_begin());
		ASSERT_TRUE(walker.has_value());
		ASSERT_EQ(wrap<Fixnum>(2), walker.value());
		walker.next();

		ASSERT_TRUE(walker.has_value());
		ASSERT_EQ(wrap<Fixnum>(3), walker.value());
		walker.next();

		ASSERT_TRUE(walker.at_end());
	}
	walker.next();

	ASSERT_TRUE(walker.has_value());
	ASSERT_EQ(wrap<Fixnum>(4), walker.value())
		<< "got: " << printer::print(walker.value()) << std::endl;
	walker.next();

	ASSERT_FALSE(walker.has_value());
}


TEST_F(TestHelpers, test_itr_walker)
{
	using namespace make_ast;

	auto expr = store(mk(1, mk(2, 3), 4));

	auto walker = walk(expr);
	auto clone = walker.clone();

	_test_walker(walker);
	_test_walker(*clone);
}

TEST_F(TestHelpers, test_trivial_pattern_match)
{
	using namespace make_ast;
	auto ast_raii = store(mk(wrap<Lambda>(), mk("a")));

	{
		using namespace pattern_match;

		ASSERT_TRUE(match(rest(tag<Lambda>::value, ast(tag<Symbol>::value)),
		                  ast_raii))
			<< printer::print(*ast_raii) << std::endl;

		ASSERT_FALSE(match(rest(tag<Lambda>::value, ast(tag<Lambda>::value)), ast_raii))
			<< printer::print(*ast_raii) << std::endl;

		ASSERT_FALSE(match(rest(tag<Lambda>::value), ast_raii))
			<< printer::print(*ast_raii) << std::endl;

		ASSERT_FALSE
			(match(rest(tag<Lambda>::value, ast(tag<Symbol>::value, tag<Symbol>::value)),
			       ast_raii))
			<< printer::print(*ast_raii) << std::endl;
	}
}

TEST_F(TestHelpers, test_throwing_pattern_match)
{
	using namespace make_ast;
	auto ast_raii = store(mk(wrap<Lambda>(), mk("a")));

	{
		using namespace pattern_match;
		throwing_match(rest(tag<Lambda>::value, ast(tag<Symbol>::value)),
		               ast_raii);

		bool caught = false;
		try
			{ throwing_match(rest(tag<Lambda>::value, ast(tag<Lambda>::value)), ast_raii); }
		catch(PatternMatchFailed)
			{ caught = true; }
		ASSERT_TRUE(caught);

		caught = false;
		try
			{ throwing_match(rest(tag<Lambda>::value), ast_raii); }
		catch(PatternMatchFailed)
			{ caught = true; }
		ASSERT_TRUE(caught);

		caught = false;
		try
			{ throwing_match(rest(tag<Lambda>::value, ast(tag<Symbol>::value, tag<Symbol>::value)),
			                 ast_raii); }
		catch(PatternMatchFailed)
			{ caught = true; }
		ASSERT_TRUE(caught);
	}
}

TEST_F(TestHelpers, test_nested_pattern_match)
{
	using namespace make_ast;
	auto ast_raii = store(mk(mk("a")));

	{
		using namespace pattern_match;

		ASSERT_TRUE
			(match(rest(ast(tag<Symbol>::value)),
			       ast_raii))
			<< printer::print(*ast_raii) << std::endl;

		ASSERT_FALSE
			(match(rest(tag<Symbol>::value),
			       ast_raii))
			<< printer::print(*ast_raii) << std::endl;
	}
}

TEST_F(TestHelpers, test_pattern_match_empty_ast)
{
	using namespace make_ast;
	auto ast = store(mk());

	{
		namespace pm = pattern_match;

		ASSERT_TRUE(match(pm::rest(), ast));
		ASSERT_FALSE(match(pm::rest("foo"), ast));
	}
}

TEST_F(TestHelpers, test_pattern_match_capture)
{
	using namespace make_ast;
	auto expr = store(mk("a"));
	Symbol sym;

	{
		using namespace pattern_match;

		ASSERT_TRUE(match(rest(capture(sym)), expr));
		ASSERT_EQ("a", sym.name);
	}
}

TEST_F(TestHelpers, test_pattern_match_capture_subex)
{
	using namespace make_ast;
	auto expr = store(mk(wrap<Lambda>(),
	                     mk("a", "b"),
	                     mk("+", "a", "b")));

	Ast::Subex formals, body;

	{
		using namespace pattern_match;

		ASSERT_TRUE(match(rest(tag<Lambda>::value, capture(formals), capture(body)),
		                  expr));


		ASSERT_EQ(*std::get<0>(formals), subscript(*expr, 1, 0));
		ASSERT_EQ(*(std::get<0>(formals) + 1), subscript(*expr, 1, 1));

		ASSERT_EQ(*std::get<0>(body), subscript(*expr, 2, 0));
	}
}

TEST_F(TestHelpers, test_pattern_match_walk_past_capture_success)
{
	using namespace make_ast;
	auto expr = store(mk(1, 2));

	{
		using namespace pattern_match;

		auto walker = walk(expr);

		ASSERT_TRUE(walker.has_value());

		ASSERT_TRUE(match(rest(tag<Fixnum>::value, tag<Fixnum>::value),
		                  walker));

		ASSERT_FALSE(walker.has_value())
			<< "walker:"
			<< " " << walker.begin.position
			<< " " << walker.itr.position
			<< " " << walker.end.position;
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
	ASSERT_EQ(unwrap<Ast>(backer.built()), *expr);
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
	ASSERT_EQ(unwrap<Ast>(backer.built()), *expr);
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
	auto post = store(ast_hof::copy(pre->subex()));

	ASSERT_EQ(*pre, *post)
		<< "pre: " << printer::print(*pre) << "\n"
		<< "post: " << printer::print(*post) << std::endl;
	ASSERT_NE(pre.any.value, post.any.value);
}

TEST_F(TestHelpers, test_ast_hof_nested_copy)
{
	using namespace make_ast;

	auto pre = store(mk("a", "b", mk("c", "d")));

	ASSERT_EQ(*pre, *store(ast_hof::copy(pre->subex())));
}

TEST_F(TestHelpers, test_fn_type)
{
	using namespace fn_type;

	auto type = store(make_ast::mk
	                  (fn(tt<Bool>(), tag<Any>::value, tag<Any>::value)));

	{
		using namespace pattern_match;
		ASSERT_TRUE
			(match(ast(function_constructor(),
			           tt<Bool>(),
			           ast(function_constructor(),
			               tt<Any>(),
			               tt<Any>())),
			       type))
			<< printer::print(*type) << std::endl;

		ASSERT_TRUE
			(match(fnt(tt<Bool>(), fnt(tt<Any>(), tt<Any>())),
			       type))
			<< printer::print(*type) << std::endl;

		ASSERT_TRUE
			(match(fnt(tt<Bool>(), fnt(tt<Any>(), tt<Any>())),
			       type))
			<< printer::print(*type) << std::endl;
	}
}

TEST_F(TestHelpers, test_fnt_rest)
{
	using namespace fn_type;

	auto type = store(fn(tt<Bool>(), tag<Any>::value, tag<Any>::value));

	{
		using namespace pattern_match;
		ASSERT_TRUE
			(match(fnt_rest(tt<Bool>(), fnt(tt<Any>(), tt<Any>())),
			       type))
			<< printer::print(*type) << std::endl;
	}
}

TEST_F(TestHelpers, test_matching_beginning)
{
	using namespace fn_type;

	auto type = store(fn(tt<Bool>(), tag<Any>::value, tag<Any>::value));

	{
		using namespace pattern_match;
		ASSERT_TRUE
			(match(rest_begins(function_constructor()),
			       type))
			<< printer::print(*type) << std::endl;
	}
}

TEST_F(TestHelpers, test_application_pattern)
{
    using namespace fn_type;

    auto type = store(fn(fn("x", "y"), fn("x", "y")));

    /* Check the body: */
    {
	    using namespace pattern_match;

	    auto pattern = fnt_rest(fnt("a", "b"), fnt("a", "b"));

	    throwing_match(pattern, *type);

	    // Check throwing_match against regular match
	    ASSERT_TRUE(match(pattern, *type));
    }
}

TEST_F(TestHelpers, test_nested_fn_type)
{
	using namespace fn_type;
	using namespace make_ast;

	auto type = store
		(mk(fn(fn(tag<Any>::value, tag<Fixnum>::value), tag<Symbol>::value, tag<Symbol>::value)));

	{
		using namespace pattern_match;
		ASSERT_TRUE
			(match(fnt(fnt(tt<Any>(), tt<Fixnum>()),
			           fnt(tt<Symbol>(), tt<Symbol>())),
			       type))
			<< printer::print(*type) << std::endl;
	}
}

TEST_F(TestHelpers, test_fn_type_thunk)
{
	using namespace fn_type;
	using namespace make_ast;

	auto expr = store(mk(fn(tag<Fixnum>::value)));

	{
		using namespace pattern_match;
		ASSERT_TRUE
			(match(fnt(tt<Fixnum>()),
			       expr))
			<< printer::print(*expr) << std::endl;
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

TEST_F(TestHelpers, test_ItrItr)
{
	using namespace make_ast;

	auto ast = store(mk(1, mk(2, 3), 4));
	auto itr = ItrItr(ast->begin());

	ASSERT_EQ(wrap<Fixnum>(1), (*itr).reference());
	++itr;
	ASSERT_TRUE(is<Ast>(**itr));
	ASSERT_TRUE(is<AstData>(itr->reference()));
	++itr;
	ASSERT_EQ(wrap<Fixnum>(4), itr->reference());
	++itr;
}

TEST_F(TestHelpers, test_itritrs)
{
	using namespace make_ast;

	auto ast = store(mk(1, 2, 3));

	size_t count = 1;
	for(auto itr : itritrs(ast))
		{
			ASSERT_TRUE(is<Fixnum>(*itr));
			ASSERT_EQ(wrap<Fixnum>(count), *itr)
				<< "got " << printer::print(*itr)
				<< " rather than " << count << std::endl;
			++count;
		}
}
