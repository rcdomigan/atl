/**
 * @file /home/ryan/programming/atl/test/test_type.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Sep 12, 2015
 *
 * Test the built-in types defined in atl/type.hpp
 */


#include "../type.hpp"

#include <boost/mpl/next_prior.hpp>

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
	ASSERT_EQ(4, ast.value->flat_size());
	ASSERT_EQ(false, ast.empty());
	ASSERT_EQ(1, reinterpret_cast<Fixnum::value_type>(ast[0].value));
}

TEST(TestType, test_ast_self_iterator)
{
	Any data[] = {Any(tag<AstData>::value, reinterpret_cast<void*>(3)),
	              Any(tag<Fixnum>::value, reinterpret_cast<void*>(1)),
	              Any(tag<Fixnum>::value, reinterpret_cast<void*>(2)),
	              Any(tag<Fixnum>::value, reinterpret_cast<void*>(3))};

	Ast ast(reinterpret_cast<AstData*>(data));

	auto self_itr = ast.self_iterator();
	ASSERT_TRUE(self_itr.is<Ast>());

	auto subex = atl::subex(self_itr);
	ASSERT_EQ(3, subex.size());

	auto itr = subex.begin();

	ASSERT_EQ(Any(tag<Fixnum>::value, reinterpret_cast<void*>(1)), *itr);
	++itr;
	ASSERT_EQ(Any(tag<Fixnum>::value, reinterpret_cast<void*>(2)), *itr);
	++itr;
	ASSERT_EQ(Any(tag<Fixnum>::value, reinterpret_cast<void*>(3)), *itr);
	++itr;
	ASSERT_EQ(subex.end(), itr);
}

TEST(TestType, test_empty_ast)
{
	vector<Any> space;
	space.emplace_back(tag<AstData>::value, reinterpret_cast<void*>(0));

	Ast ast(reinterpret_cast<AstData*>(&space.front()));

	ASSERT_EQ(true, ast.empty());
	ASSERT_EQ(0, ast.size());
}

TEST(TestType, test_ast_iterator)
{
	Any data[] = {Any(tag<AstData>::value, reinterpret_cast<void*>(6)),
	              Any(tag<Fixnum>::value, reinterpret_cast<void*>(1)),
	              Any(tag<Fixnum>::value, reinterpret_cast<void*>(2)),
	              Any(tag<AstData>::value, reinterpret_cast<void*>(2)),
	              Any(tag<Fixnum>::value, reinterpret_cast<void*>(3)),
	              Any(tag<Fixnum>::value, reinterpret_cast<void*>(4)),
	              Any(tag<Fixnum>::value, reinterpret_cast<void*>(5))};

	Ast ast(reinterpret_cast<AstData*>(data));
	auto itr = ast.begin();

	ASSERT_EQ(Any(tag<Fixnum>::value, reinterpret_cast<void*>(1)), *itr);
	++itr;
	ASSERT_EQ(Any(tag<Fixnum>::value, reinterpret_cast<void*>(2)), *itr);
	++itr;
	ASSERT_EQ(tag<Ast>::value, (*itr)._tag);
	{
		auto sub = subex(itr);
		auto sitr = sub.begin();
		ASSERT_EQ(Any(tag<Fixnum>::value, reinterpret_cast<void*>(3)), *sitr);
		++sitr;
		ASSERT_EQ(Any(tag<Fixnum>::value, reinterpret_cast<void*>(4)), *sitr);
		++sitr;
		ASSERT_EQ(sub.end(), sitr);
	}
	++itr;
	ASSERT_EQ(Any(tag<Fixnum>::value, reinterpret_cast<void*>(5)), *itr);
	++itr;
	ASSERT_EQ(ast.end(), itr);
}

TEST(TestType, test_ast_subscript)
{
	Any data[] = {Any(tag<AstData>::value, reinterpret_cast<void*>(6)),
	              Any(tag<Fixnum>::value, reinterpret_cast<void*>(1)),
	              Any(tag<Fixnum>::value, reinterpret_cast<void*>(2)),
	              Any(tag<AstData>::value, reinterpret_cast<void*>(2)),
	              Any(tag<Fixnum>::value, reinterpret_cast<void*>(3)),
	              Any(tag<Fixnum>::value, reinterpret_cast<void*>(4)),
	              Any(tag<Fixnum>::value, reinterpret_cast<void*>(5))};

	Ast ast(reinterpret_cast<AstData*>(data));

	ASSERT_EQ(Any(tag<Fixnum>::value, reinterpret_cast<void*>(1)), ast[0]);
	ASSERT_EQ(Any(tag<Fixnum>::value, reinterpret_cast<void*>(2)), ast[1]);
	ASSERT_EQ(tag<Ast>::value, ast[2]._tag);
	ASSERT_EQ(Any(tag<Fixnum>::value, reinterpret_cast<void*>(5)), ast[3]);
}

TEST(TestType, test_scheme_is_function)
{
	auto make_type = [](Type::value_type inner_tag) -> Any
		{ return Any(tag<Type>::value, reinterpret_cast<void*>(Type(inner_tag)._value)); };

	vector<Any> space;
	space.emplace_back(tag<AstData>::value, reinterpret_cast<void*>(2));

	space.push_back(function_constructor());
	space.push_back(make_type(tag<Fixnum>::value));

	Any ast(tag<Ast>::value,
	        reinterpret_cast<AstData*>(&space.front()));
	Scheme scheme;
	scheme.type = ast;

	ASSERT_TRUE(scheme.is_function());

	space[1] = Any(tag<Type>::value,
	               reinterpret_cast<void*>(Type(tag<Lambda>::value).value()));

	ASSERT_FALSE(scheme.is_function());
}

TEST(TestType, test_Type)
{
	typedef mpl::deref<mpl::prior<typename mpl::end<TypesVec>::type>::type>::type LastType;

	auto captured = Type(tag<LastType>::value);

	ASSERT_TRUE(captured.is_rigid());
	ASSERT_EQ(tag<LastType>::value, captured.value());
	ASSERT_NE(tag<LastType>::value, captured._value);
}
