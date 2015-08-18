/**
 * @file /home/ryan/programming/atl/test/atl.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Aug 05, 2015
 *
 * Regression tests for ATL primtives
 */

#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <atl.hpp>
#include <iterator>

using namespace atl;

struct PrimitiveTest : public ::testing::Test
{
	Atl atl;
	PrimitiveTest() : atl() {}
};


TEST_F(PrimitiveTest, test_eval)
{
	auto rval = atl.string_("(eval Fixnum '(add2 1 2))");
	ASSERT_EQ(3, unwrap<Fixnum>(rval).value);

	ASSERT_EQ(tag<Fixnum>::value, rval._tag);
}

TEST_F(PrimitiveTest, test_nested_eval)
{
	auto rval = atl.string_("(add2 (eval Fixnum '(add2 1 2)) 5)");
	ASSERT_EQ(8, unwrap<Fixnum>(rval).value);
}

TEST_F(PrimitiveTest, test_eval_nth)
{
	auto rval = atl.string_("(eval Fixnum (nth '(3 5) 0))");
	ASSERT_EQ(3, unwrap<Fixnum>(rval).value);

	rval = atl.string_("(eval Fixnum (nth '(3 5) 1))");
	ASSERT_EQ(5, unwrap<Fixnum>(rval).value);
}

TEST_F(PrimitiveTest, test_eval_nested_nth)
{
	auto rval = atl.string_("(add2 (eval Fixnum (nth '(1 2) 0)) (eval Fixnum (nth '(1 2) 1)))");
	ASSERT_EQ(3, unwrap<Fixnum>(rval).value);
}
