#include <iostream>
#include <vector>

#include <atl/gc/ast_pool.hpp>
#include <atl/print.hpp>

#include <gtest/gtest.h>


struct MockGC
{
	void mark(atl::Any&) {}
	void gc() {}
};


struct TestAstPool
	: public ::testing::Test
{
	typedef atl::AstPool<MockGC> AstPool;

	MockGC mock_gc;
	AstPool asts;

	TestAstPool() : asts(mock_gc) {}
};

TEST_F(TestAstPool, test_copy_any_range)
{
	using namespace atl;

	Any aa[3] = {wrap<AstData>(2),
	             wrap<Fixnum>(0),
	             wrap<Fixnum>(1)};
	Any bb[3];
	Any* bb_itr = bb;

	auto aa_ast = Ast(reinterpret_cast<AstData*>(aa));

	ast_pool_detail::copy_to_end(flat_ast(aa_ast), bb_itr);

	ASSERT_EQ(aa_ast, Ast(reinterpret_cast<AstData*>(bb)));
	ASSERT_NE(reinterpret_cast<Any*>(aa_ast.value), bb);
}

TEST_F(TestAstPool, test_copy_nested_ast)
{
	using namespace atl;

	Any aa[6] = {wrap<AstData>(5),
	             wrap<Fixnum>(2),
	             wrap<AstData>(2),
	             wrap<Fixnum>(0),
	             wrap<Fixnum>(1),
	             wrap<Fixnum>(3)};
	Any bb[6];
	Any* bb_itr = bb;

	auto aa_ast = Ast(reinterpret_cast<AstData*>(aa));

	ast_pool_detail::copy_to_end(flat_ast(aa_ast), bb_itr);

	ASSERT_EQ(aa_ast, Ast(reinterpret_cast<AstData*>(bb)));
	ASSERT_NE(reinterpret_cast<Any*>(aa_ast.value), bb);
}

TEST_F(TestAstPool, test_building_asts)
{
	using namespace atl;

	{
		auto build = *asts.ast_backer(4);
		build.nest_ast();

		build.emplace_back(wrap<Fixnum>(1));
		build.emplace_back(wrap<Fixnum>(2));

		build.end_ast();
		ASSERT_EQ(2, build.ast().size());
		ASSERT_EQ(3, build.ast().flat_size());
	}

	{
		auto build = *asts.ast_backer(16);
		build.nest_ast();

		build.emplace_back(wrap<Fixnum>(1));
		build.emplace_back(wrap<Fixnum>(2));

		build.nest_ast();

		build.emplace_back(wrap<Fixnum>(3));
		build.emplace_back(wrap<Fixnum>(4));

		build.end_ast();

		build.emplace_back(wrap<Fixnum>(5));

		build.end_ast();

		ASSERT_EQ(7, build.ast().flat_size());
		ASSERT_EQ(4, build.ast().size());
	}
}

TEST_F(TestAstPool, test_growing_ast_backer)
{
	using namespace atl;
	auto initial_size = AstPool::DEFAULT_INITIAL_SIZE;

	auto build = *asts.ast_backer();
	ASSERT_EQ(initial_size, build.reserved());

	build.nest_ast();
	build.emplace_back(wrap<Fixnum>(0));
	build.emplace_back(wrap<Fixnum>(1));

	{
		build.nest_ast();
		build.emplace_back(wrap<Fixnum>(2));
		build.emplace_back(wrap<Fixnum>(3));
		build.end_ast();
	}
	build.emplace_back(wrap<Fixnum>(4));

	build.end_ast();

	ASSERT_LT(initial_size, build.size());
	ASSERT_EQ(initial_size * AstPool::RESIZE_FACTOR, build.reserved());

	auto ast = build.ast();

	ASSERT_EQ(Any(tag<Fixnum>::value, reinterpret_cast<void*>(0)), ast[0]);
	ASSERT_EQ(Any(tag<Fixnum>::value, reinterpret_cast<void*>(1)), ast[1]);
	ASSERT_EQ(tag<Ast>::value, ast[2]._tag);
	ASSERT_EQ(Any(tag<Fixnum>::value, reinterpret_cast<void*>(4)), ast[3]);
}

TEST_F(TestAstPool, test_moving_asts)
{
	using namespace atl;
	Ast ast_a, moved_a;
	AstData old_value(0);

	auto build = *asts.ast_backer();
	build.nest_ast();
	build.emplace_back(wrap<Fixnum>(1));
	build.emplace_back(wrap<Fixnum>(2));
	build.end_ast();

	ast_a = build.ast();
	old_value = *build.ast().value;

	asts.gc_start();

	moved_a = asts.move(ast_a);

	// The AstData gets tagged as 'moved' and re-purposed so it isn't
	// copied multiple times.  Reverse that change so I can treat it
	// like an actual Ast
	*ast_a.value = old_value;

	ASSERT_NE(moved_a.value, ast_a.value);
	ASSERT_EQ(moved_a, ast_a);

	asts.gc_finish();
}

TEST_F(TestAstPool, test_moving_ast_twice)
{
	using namespace atl;
	Ast ast_a, moved_a, moved_a_2;

	auto build = *asts.ast_backer();
	build.nest_ast();
	build.emplace_back(wrap<Fixnum>(1));
	build.emplace_back(wrap<Fixnum>(2));
	build.end_ast();

	ast_a = build.ast();

	asts.gc_start();

	moved_a = asts.move(ast_a);
	moved_a_2 = asts.move(ast_a);

	ASSERT_EQ(moved_a.value, moved_a_2.value);

	asts.gc_finish();
}
