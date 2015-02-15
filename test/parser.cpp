
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

    for(auto& vv : zip(ast, expected)) {
        ASSERT_EQ(*get<0>(vv), *get<1>(vv));
    }

}


TEST_F(ParserTest, NestedIntList) {
    auto parsed = atl.parse.string_("(1 2 (4 5) 3)");
    auto ast = flat_iterator::range(parsed);

    auto begin = unwrap<Ast>(parsed).value;

    auto expected = vector<Any>{
        aimm(1), aimm(2),
        begin[3],
        Any(tag<Any>::value, begin + 7), // the pointer to the end of '(4 5)
        aimm(4), aimm(5),
        aimm(3)
    };

    for(auto& vv : zip(ast, expected)) {
        // cout << "parsed: " << printer::any(*get<0>(vv)) << "\nexpected: " << printer::any(*get<1>(vv)) << endl;
        ASSERT_EQ(*get<0>(vv), *get<1>(vv));
    }
}


TEST_F(ParserTest, TestQuote) {
    auto parsed = atl.parse.string_("'(2 3)");
    auto ast = flat_iterator::range(parsed);
    auto begin = unwrap<Ast>(parsed).value;

    vector<Any> expected = vector<Any>{
        Any(tag<Quote>::value),
        begin[2],               // the (pointer to point to end of) quoted list
        Any(tag<Any>::value, begin + 6), // end of quoted list
        aimm(2),
        aimm(3)
    };

    for(auto& vv : zip(ast, expected)) {
        cout << "parsed: " << printer::any(*get<0>(vv)) << "\nexpected: " << printer::any(*get<1>(vv)) << endl;
        ASSERT_EQ(*get<0>(vv), *get<1>(vv));
    }
}


int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
