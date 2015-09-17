/**
 * @file /home/ryan/programming/atl/test/test_type.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Sep 12, 2015
 *
 * Test the built-in types defined in atl/type.hpp
 */


#include "../type.hpp"
#include "../gc.hpp"
#include "../utility.hpp"

#include <iostream>
#include <vector>

#include <gtest/gtest.h>

using namespace atl;
using namespace std;

TEST(TestType, test_ast_size)
{
	AstSubstrate space;

	auto ast = push_nested_ast(space);
	space.emplace_back(tag<Fixnum>::value, reinterpret_cast<void*>(1));
	space.emplace_back(tag<Fixnum>::value, reinterpret_cast<void*>(2));
	space.emplace_back(tag<Fixnum>::value, reinterpret_cast<void*>(3));

	ast.end_ast();

	ASSERT_EQ(3, (*ast).size());
	ASSERT_EQ(1, reinterpret_cast<Fixnum&>((*ast)[0]).value);
}


TEST(TestType, test_iterte_over_ast)
{
	AstSubstrate space;

	auto ast = push_nested_ast(space);
	space.emplace_back(tag<Fixnum>::value, reinterpret_cast<void*>(1));
	space.emplace_back(tag<Fixnum>::value, reinterpret_cast<void*>(2));
	space.emplace_back(tag<Fixnum>::value, reinterpret_cast<void*>(3));

	ast.end_ast();

	std::vector<Any> vec;
	vec.emplace_back(tag<Fixnum>::value, reinterpret_cast<void*>(1));
	vec.emplace_back(tag<Fixnum>::value, reinterpret_cast<void*>(2));
	vec.emplace_back(tag<Fixnum>::value, reinterpret_cast<void*>(3));


	for(auto tup : zip(vec, *ast))
		ASSERT_EQ(*get<0>(tup),
		          *get<1>(tup));
}


TEST(TestType, test_iterte_over_nested_ast)
{
	AstSubstrate space;

	auto ast = push_nested_ast(space);
	space.emplace_back(tag<Fixnum>::value, reinterpret_cast<void*>(1));
	space.emplace_back(tag<Fixnum>::value, reinterpret_cast<void*>(2));

	auto inner = push_nested_ast(space);
	space.emplace_back(tag<Fixnum>::value, reinterpret_cast<void*>(1));
	space.emplace_back(tag<Fixnum>::value, reinterpret_cast<void*>(2));
	inner.end_ast();

	space.emplace_back(tag<Fixnum>::value, reinterpret_cast<void*>(3));
	ast.end_ast();

	auto foo = std::vector<size_t>({1, 2, 3});
	auto bar = std::vector<size_t>({1, 2});
	ASSERT_EQ(bar.size(), inner->size());

	for(auto tup : zip(foo, *ast))
		{
			if(is<Ast>(*get<1>(tup)))
				{
					for(auto tup2 : zip(bar,
					                    unwrap<Ast>(*get<1>(tup))))
						{
							ASSERT_EQ(*get<0>(tup2),
							          unwrap<Fixnum>(*get<1>(tup2)).value);
						}
				}
			else
				ASSERT_EQ(*get<0>(tup),
				          unwrap<Fixnum>(*get<1>(tup)).value);
		}
;
}
