
/**
 * @file /home/ryan/programming/atl/test/test_parser.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Dec 16, 2014
 */

#include <gtest/gtest.h>

#include "../atl.hpp"
#include "../helpers.hpp"

#include "../debug.hpp"

#include <iostream>
#include <vector>


using namespace atl;
using namespace std;

struct ParserTest : public ::testing::Test {
    Atl atl;
};

TEST_F(ParserTest, Atoms) {
    ASSERT_EQ(unwrap<Fixnum>(atl.parse.string_("2")).value,
              2);

    ASSERT_EQ(unwrap<String>(atl.parse.string_("\"Hello, world!\"")).value,
              "Hello, world!");

}

TEST_F(ParserTest, SimpleIntList) {
    auto parsed = atl.parse.string_("(1 2 3)");
    auto ast = flat_iterator::range(parsed);

    auto expected = vector<Any>{
        wrap(1),
        wrap(2),
        wrap(3)
    };

    for(auto& vv : zip(ast, expected)) {
#ifdef DEBUGGING
        cout << "parsed: " << printer::any(*get<0>(vv)) << "\nexpected: " << printer::any(*get<1>(vv)) << endl;
#endif
        ASSERT_EQ(*get<0>(vv), *get<1>(vv));
    }

}


TEST_F(ParserTest, NestedIntList) {
    auto parsed = atl.parse.string_("(1 2 (4 5) 3)");
    auto ast = flat_iterator::range(parsed);

    auto begin = unwrap<Ast>(parsed).value;

    auto expected = vector<Any>{
        wrap(1), wrap(2),
        begin[3],
        Any(tag<Any>::value, begin + 7), // the pointer to the end of '(4 5)
        wrap(4), wrap(5),
        wrap(3)
    };

    for(auto& vv : zip(ast, expected)) {
#ifdef DEBUGGING
        cout << "parsed: " << printer::any(*get<0>(vv)) << "\nexpected: " << printer::any(*get<1>(vv)) << endl;
#endif
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
        wrap(2),
        wrap(3)
    };

    for(auto& vv : zip(ast, expected)) {
#ifdef DEBUGGING
        cout << "parsed: " << printer::any(*get<0>(vv)) << "\nexpected: " << printer::any(*get<1>(vv)) << endl;
#endif
        ASSERT_EQ(*get<0>(vv), *get<1>(vv));
    }
}


TEST_F(ParserTest, TestLambda)
{
    // I'm making '[[:delim:]]\[[:delim:]]' a reserved symbol for lambda, make sure it's parsed correctly.
    auto parsed = atl.parse.string_("(\\ (a b) (c d e))");
    tag_t linkable = tag<Lambda>::value;
    ASSERT_EQ(unwrap<Ast>(parsed)[0]._tag, linkable);

    parsed = atl.parse.string_("(\\a b\\)");
    linkable = tag<Symbol>::value;
    ASSERT_EQ(unwrap<Ast>(parsed)[0]._tag, linkable);
    ASSERT_EQ(unwrap<Ast>(parsed)[1]._tag, linkable);
}

TEST_F(ParserTest, TestLet)
{
    auto parsed = atl.parse.string_("(let (a b) (a 1 2))");
    tag_t let_tag = tag<Let>::value;

    ASSERT_EQ(unwrap<Ast>(parsed)[0]._tag, let_tag);
}
