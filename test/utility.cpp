// A little test of the `zip` function, and the Zipper and CountingRange class

#include <utility.hpp>
#include <iostream>
#include <vector>
#include <tuple>
#include <sstream>

#include <gtest/gtest.h>

using namespace std;

TEST(Utilities, test_zip)
{
    vector<int> vec({0, 1, 2, 3, 4});

    for(auto zz : zip(vec,
		      CountingRange()))
	ASSERT_EQ(*get<0>(zz), *get<1>(zz));
}

TEST(Utilities, test_map_range)
{
    typedef std::vector<int> Vec;
    Vec vec1 = Vec({1, 2, 3});

    Vec vec2 = map_range<Vec>
        ([&](const int in)
         {
             return in + 5;
         },
         vec1);

    Vec expected = Vec({6, 7, 8});
    for(auto zz : zip(vec2, expected))
        ASSERT_EQ(*get<0>(zz), *get<1>(zz));
}


struct CheckIndicies
{

	template<std::size_t I, std::size_t ... Is>
	static void a(vector<size_t>& out, tmp::Indexer<I, Is...>)
	{
		out.push_back(I);
		a(out, tmp::Indexer<Is...> {});
	}

	template<std::size_t I>
	static void a(vector<size_t>& out, tmp::Indexer<I>)
	{ out.push_back(I); }
};


TEST(Utilities, test_indexer)
{
	vector<size_t> vec;
	CheckIndicies::a(vec, tmp::BuildIndicies<4> {});

	ASSERT_EQ(4, (vec.size()));
	ASSERT_EQ(0, (vec.front()));
	ASSERT_EQ(3, (vec.back()));
}




TEST(Utilities, test_apply_tuple)
{
	struct Runner
	{
		stringstream out;
		void operator()(int ii, string ss, int jj)
		{
			out << ii << ":" << ss << ":" << jj;
		}
	};

	auto tup = make_tuple(2, string("string"), 3);

	Runner out;
	apply_tuple(out, tup);

	ASSERT_EQ(out.out.str(), "2:string:3");
}
