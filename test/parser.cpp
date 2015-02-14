
/**
 * @file /home/ryan/programming/atl/test/test_parser.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Dec 16, 2014
 */

#include "../atl.hpp"
#include "../helpers.hpp"

#include "../debug.hpp"

#include <iostream>
#include <vector>
#include <gtest/gtest.h>

using namespace atl;
using namespace std;

struct ParserTest : public ::testing::Test {
    Atl atl;
};

TEST_F(ParserTest, SimpleIntList) {
    auto parsed = atl.parse.string_("(1 2 3)");
    auto ast = flat_iterator::range(parsed);

    auto expected = vector<Any>{
        aimm(1),
        aimm(2),
        aimm(3)
    };

    for(auto& vv : zip(ast, expected))
        ASSERT_EQ(*get<0>(vv), *get<1>(vv));
}


TEST_F(ParserTest, NestedIntList) {
    auto parsed = atl.parse.string_("(1 2 (4 5 (6 7)) 3)");
    auto ast = flat_iterator::range(parsed);

    auto begin = unwrap<Ast>(parsed).value;

    auto expected = vector<Any>{
        aimm(1), aimm(2),
        aimm(begin + 9),
        aimm(4), aimm(5),
        aimm(begin + 9),
        aimm(6),
        aimm(7),
        aimm(3)
    };

    for(auto& vv : zip(ast, expected))
        ASSERT_EQ(*get<0>(vv), *get<1>(vv));
}


int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
