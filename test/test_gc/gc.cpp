/**
 * @file /home/ryan/programming/atl/test/gc.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jul 25, 2015
 */

#include <iostream>
#include <vector>

#include <gc.hpp>

#include <gtest/gtest.h>

using namespace atl;
using namespace std;

Ast make_expr(AstAllocator backer)
{
	auto pntr = backer.nest_ast();
	backer.push_back(Any(tag<Fixnum>::value, reinterpret_cast<void*>(1)));
	backer.push_back(Any(tag<Fixnum>::value, reinterpret_cast<void*>(2)));
	pntr.end_ast();
	return *pntr;
}

TEST(TestGC, test_building_ast)
{
	Arena store;
	Ast ast = store(make_expr);

	ASSERT_EQ(2, ast.size());

	ASSERT_EQ(tag<Fixnum>::value, ast[0]._tag);
	ASSERT_EQ(1, reinterpret_cast<Fixnum::value_type>(ast[0].value));

	ASSERT_EQ(tag<Fixnum>::value, ast[1]._tag);
	ASSERT_EQ(2, reinterpret_cast<Fixnum::value_type>(ast[1].value));
}

TEST(TestGC, test_make_ast_gc_and_arena)
{
	// Check that an Ast allocated by an Arena an a GC are the same.
	GC gc;
	Arena arena;

	auto ast0 = gc(make_expr);

	auto ast1 = arena(make_expr);

	ASSERT_EQ(2, ast0.size());
	ASSERT_EQ(2, ast1.size());

	for(auto zz : zip(ast0, ast1))
		ASSERT_EQ((*get<0>(zz)), (*get<1>(zz)));
}
