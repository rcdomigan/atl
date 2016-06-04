/**
 * @file /home/ryan/programming/atl/test/lexical_environment.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Oct 25, 2015
 *
 * Test the lexical environment, in particular finding free variables.
 */


#include <lexical_environment.hpp>
#include <gc.hpp>

#include <iostream>
#include <gtest/gtest.h>
#include <print.hpp>
#include <helpers.hpp>

using namespace std;
using namespace atl;


struct TestToplevelMap
	: public ::testing::Test
{
	SymbolMap env;
	FreeSymbols free;
	GC gc;

	TestToplevelMap()
		: env(nullptr)
	{ init_types(); }
};


TEST_F(TestToplevelMap, test_assigning_bound_lambda)
{
	using namespace make_ast;

	auto expr = make
		(lift<Lambda>(),
		 make(sym("a")),
		 make(lift<Lambda>(),
		      make(sym("b")),
		      make(sym("a"), sym("b"), sym("c"))))
		(ast_alloc(gc));

	auto wrapped = wrap(expr);

	assign_free(env,
	            gc,
	            free,
	            wrapped);

	dbg_ast(expr);

	/* On the toplevel, only "c" should be free */
    ASSERT_EQ(1, free.size());

    /* 'a' in the body should point to the 'a' formal */
    auto bound_a = subscripter(expr)[2][2][0].value;
    ASSERT_EQ(tag<Bound>::value, bound_a._tag);
    ASSERT_EQ(reinterpret_cast<Symbol*>(subscripter(expr)[1][0].value.any.value),
              unwrap<Bound>(bound_a).value);

    /* and 'b' should point to its formal */
    auto bound_b = subscripter(expr)[2][2][1].value;
    ASSERT_EQ(tag<Bound>::value, bound_b._tag);
    ASSERT_EQ(reinterpret_cast<Symbol*>(subscripter(expr)[2][1][0].value.any.value),
              unwrap<Bound>(bound_b).value);
}


TEST_F(TestToplevelMap, test_toplevel_replacements)
{
	using namespace make_ast;

	auto expr = make
		(lift<Lambda>(),
		 make(sym("a")),
		 make(lift<Lambda>(),
		      make(sym("b")),
		      make(sym("a"), sym("b"), sym("c"))))
		(ast_alloc(gc)),

		assign = make(lift<Define>(),
		              sym("c"),
		              lift<Fixnum>(3))(ast_alloc(gc));

	auto wrapped_expr = wrap(expr),
		wrapped_assign = wrap(assign);

	assign_free(env,
	            gc,
	            free,
	            wrapped_expr);

	assign_free(env,
	            gc,
	            free,
	            wrapped_assign);

	/* 'c have been bound by the toplevel */
    ASSERT_EQ(0, free.size());

	ASSERT_EQ(tag<Bound>::value,
	          subscripter(expr)[2][2][2].value._tag);
	ASSERT_EQ(env[std::string("c")].first,
	          subscripter(expr)[2][2][2].value.any.value);

	/* Check the types while I'm at it */
	auto stype = [](AstSubscripter aa) { return unwrap<Type>(unwrap<Symbol>(aa.value).scheme.type).value; };
	ASSERT_EQ(LAST_CONCRETE_TYPE + 1, stype(subscripter(expr)[1][0]));
	ASSERT_EQ(LAST_CONCRETE_TYPE + 2, stype(subscripter(expr)[2][1][0]));
	ASSERT_EQ(LAST_CONCRETE_TYPE + 3, stype(subscripter(expr)[2][2][2]));
}
