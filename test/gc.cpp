/**
 * @file /home/ryan/programming/atl/test/gc.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jul 25, 2015
 */

#include "../atl.hpp"
#include "../helpers.hpp"
#include "../debug.hpp"

#include <iostream>
#include <vector>

#include <gtest/gtest.h>

using namespace atl;
using namespace std;

TEST(TestGC, test_make_ast_gc_and_arena)
{
	// Check that an Ast allocated by an Arena an a GC are the same.
	GC gc;
	Arena arena;

	using namespace make_ast;
	auto ast0 = make_ast::make(lift(1), lift(2), lift(3))
		(ast_alloc(gc));

	auto ast1 = make_ast::make(lift(1), lift(2), lift(3))
		(ast_alloc(arena));

	ASSERT_EQ(ast0->size(), 3);
	ASSERT_EQ(ast1->size(), 3);

	for(auto zz : zip(*ast0, *ast1))
		ASSERT_EQ((*get<0>(zz)), (*get<1>(zz)));
}
