/**
 * @file /home/ryan/programming/atl/test/test_type.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Sep 12, 2015
 *
 * Test the built-in types defined in atl/type.hpp
 */


#include "../type.hpp"

#include <iostream>
#include <vector>

#include <gtest/gtest.h>

using namespace atl;
using namespace std;

TEST(TestType, test_ast)
{
	vector<Any> space;
	space.emplace_back(tag<AstData>::value, reinterpret_cast<void*>(3));

	space.emplace_back(tag<Fixnum>::value, reinterpret_cast<void*>(1));
	space.emplace_back(tag<Fixnum>::value, reinterpret_cast<void*>(2));
	space.emplace_back(tag<Fixnum>::value, reinterpret_cast<void*>(3));

	Ast ast(reinterpret_cast<AstData*>(&space.front()));

	ASSERT_EQ(3, ast.size());
	ASSERT_EQ(false, ast.empty());
	ASSERT_EQ(1, reinterpret_cast<Fixnum&>(ast[0]).value);
}

TEST(TestType, test_empty_ast)
{
	vector<Any> space;
	space.emplace_back(tag<AstData>::value, reinterpret_cast<void*>(0));

	Ast ast(reinterpret_cast<AstData*>(&space.front()));

	ASSERT_EQ(true, ast.empty());
	ASSERT_EQ(0, ast.size());
}
