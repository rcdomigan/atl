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
	wrap_macro(atl.lexical, "foo",
	           [&](CxxMacro::Input const& ast) -> Any
	           {
		           using namespace make_ast;
		           return wrap
			           (make
			            (lift<If>(),
			             sym("#t"),
			             make(lift<Quote>(), lift(ast[0])),
			             make(lift<Quote>(), lift(ast[1])))
			            (ast_alloc(atl.gc)));
	           });
	auto rval = atl.string_("(foo bar baz)");

	ASSERT_EQ("bar", (unwrap<Symbol>(*unwrap<Pointer>(rval).value).name));
}

// Macro yielding a function
TEST_F(TestMacros, test_applicable)
{
	using namespace primitives;
	wrap_macro(atl.lexical, "foo",
	           [&](CxxMacro::Input const& ast) -> Any
	           {
		           using namespace make_ast;
		           return wrap
			           (make
			            (make
			             (lift<Lambda>(),
			              make(sym("a"), sym("b")),
			              make(sym("add2"), sym("a"), sym("b"))),
			             lift(ast[0]),
			             lift(ast[1]))
			            (ast_alloc(atl.gc)));
	           });
	auto rval = atl.string_("(foo 2 3)");

	ASSERT_EQ(5, (unwrap<Fixnum>(rval).value));
}
