/**
 * Test the pre-compilation passes which build up our metadata.
 */

#include <gtest/gtest.h>

#include <passes.hpp>
#include <helpers.hpp>

#include <test/trivial_functions.hpp>

using namespace atl;

struct PassesTest
	: public ::testing::Test
{
	TrivialFunctions cxx_fns;
	Arena& store;

	PassesTest()
		: store(cxx_fns.store)
	{ init_types(); }
};

TEST_F(PassesTest, test_lambda_padding)
{
	using namespace make_ast;

	LambdaMetadata fn;

	auto expr = mk(wrap<Lambda>(&fn),
	               mk("a", "b"),
	               mk(wrap(cxx_fns.weq)))
		(AstAllocator(store));

	auto tail_count = assign_padding(ref_wrap(expr));

	ASSERT_EQ(2, tail_count);
	ASSERT_EQ(2, fn.pad_to);
}

TEST_F(PassesTest, test_if_padding)
{
	using namespace make_ast;

	LambdaMetadata pred, fn0, fn1;

	pred.pad_to = 1;
	fn0.pad_to = 2;
	fn1.pad_to = 3;

	auto expr1 = mk(wrap<If>(),
	                mk(wrap<CallLambda>(&pred)),
	                mk(wrap<CallLambda>(&fn0), 1, 2),
	                mk(wrap<CallLambda>(&fn1), 1, 2, 3))
		(AstAllocator(store));

	ASSERT_EQ(3, assign_padding(ref_wrap(expr1)));

	// checks both sides of the if
	auto expr2 = mk(wrap<If>(),
	                mk(wrap<CallLambda>(&pred)),
	                mk(wrap<CallLambda>(&fn1), 1, 2, 3),
	                mk(wrap<CallLambda>(&fn0), 1, 2))
		(AstAllocator(store));

	ASSERT_EQ(3, assign_padding(ref_wrap(expr2)));
}
