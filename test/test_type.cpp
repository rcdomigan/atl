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


TEST(TestType, test_Slice)
{
	vector<Any> space;
	space.emplace_back(tag<AstData>::value, reinterpret_cast<void*>(3));

	space.emplace_back(tag<Fixnum>::value, reinterpret_cast<void*>(1));
	space.emplace_back(tag<Fixnum>::value, reinterpret_cast<void*>(2));
	space.emplace_back(tag<Fixnum>::value, reinterpret_cast<void*>(3));

	Ast ast(reinterpret_cast<AstData*>(&space.front()));

	Slice slice(ast);

	ASSERT_EQ(3, slice.size());
	ASSERT_EQ(false, ast.empty());
	ASSERT_EQ(3, reinterpret_cast<Fixnum&>(slice[2]).value);
}

TEST(TestType, test_scheme_is_function)
{
	auto make_type = [](Type::value_type inner_tag) -> Any
		{ return Any(tag<Type>::value, reinterpret_cast<void*>(Type(inner_tag)._value)); };

	vector<Any> space;
	space.emplace_back(tag<AstData>::value, reinterpret_cast<void*>(2));

	space.push_back(make_type(tag<FunctionConstructor>::value));
	space.push_back(make_type(tag<Fixnum>::value));

	Any ast(tag<Ast>::value,
	        reinterpret_cast<AstData*>(&space.front()));

	Scheme scheme;
	scheme.type = ast;

	ASSERT_TRUE(scheme.is_function());

	space[1].value = reinterpret_cast<void*>(tag<FunctionConstructor>::value);
	ASSERT_FALSE(scheme.is_function());
}
