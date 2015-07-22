// A little test of the `zip` function, and the Zipper and CountingRange class

#include <utility.hpp>
#include <iostream>
#include <vector>

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
