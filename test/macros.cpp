/**
 * @file /home/ryan/programming/atl/test/macros.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on May 05, 2015
 */

#include <gtest/gtest.h>

#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include <atl.hpp>
#include <interface_helpers.hpp>

using namespace std;
using namespace atl;

struct TestMacros : public ::testing::Test
{
    Atl atl;
};


TEST_F(TestMacros, test_primitives)
{
	using namespace primitives;
	Any first_item, second_item;

	wrap_macro(atl.env, "foo",
	           [&](Eval& eval, PrimitiveMacro::Input const& ast)
	           {
		           first_item = ast[0];
		           second_item = ast[1];
		           return tag<If>::value;
	           });
	auto rtag = atl.compile.any(atl.parse.string_("(foo bar baz)"));
	auto tmp_tag = tag<If>::value;
	ASSERT_EQ(rtag,
	          tmp_tag);
	tmp_tag = tag<Symbol>::value;
	ASSERT_EQ(first_item._tag,
	          tmp_tag);
	ASSERT_EQ((unwrap<Symbol>(first_item).name),
	          "bar");
	ASSERT_EQ((unwrap<Symbol>(second_item).name),
	          "baz");
}
