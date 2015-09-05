
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
#include <sstream>


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
    auto ast = ast_iterator::range(parsed);

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


void _check_nested(Ast const& parsed, Ast const& expected)
{
	for(auto& vv : zip(parsed, expected))
		{
			ASSERT_EQ((*get<0>(vv))._tag,
			          (*get<1>(vv))._tag);

			if(is<Ast>(*get<0>(vv)))
				_check_nested(unwrap<Ast>(*get<0>(vv)),
				              unwrap<Ast>(*get<1>(vv)));
			else
				{
#ifdef DEBUGGING
					cout << "parsed: " << printer::any(*get<0>(vv))
					     << "\nexpected: " << printer::any(*get<1>(vv))
					     << endl;
#endif
					ASSERT_EQ(*get<0>(vv), *get<1>(vv));
				}
		}
}


TEST_F(ParserTest, nested_int_list)
{
	Arena arena;
	using namespace make_ast;
    auto parsed = atl.parse.string_("(1 2 (4 5) 3)");

    auto expected =
	    make(lift(1),
	         lift(2),
	         make(lift(4),
	              lift(5)),
	         lift(3))
	    (ast_alloc(arena));

    _check_nested(unwrap<Ast>(parsed),
                  *expected);
}


TEST_F(ParserTest, TestQuote) {
	using namespace make_ast;
    auto parsed = atl.parse.string_("'(2 3)");

    auto expected =
	    make(lift<Quote>(),
	         make(lift(2),
	              lift(3)))
	    (ast_alloc(atl.gc));

    _check_nested(unwrap<Ast>(parsed), *expected);
}


TEST_F(ParserTest, test_nested_quote)
{
	using namespace make_ast;
	auto parsed = atl.parse.string_("(1 '(2 3) 4)");

	auto expected = make
		(lift(1),
		 make(lift<Quote>(),
		      make(lift(2),
		           lift(3))),
		 lift(4))
		(ast_alloc(atl.gc));

    _check_nested(unwrap<Ast>(parsed), *expected);
}


TEST_F(ParserTest, test_stream_parsing)
{
	string contents = "(a b c)";
	stringstream as_stream(contents);

	auto a = unwrap<Ast>(atl.parse.string_(contents));
	auto b = unwrap<Ast>(atl.parse.stream(as_stream));

	for(auto& vv : zip(a, b))
		ASSERT_EQ(unwrap<Symbol>(*get<0>(vv)).name,
		          unwrap<Symbol>(*get<1>(vv)).name);
}

TEST_F(ParserTest, test_multi_line_stream_parsing)
{
	string contents = "(1 2 3)\n\n(4 5 6)";
	stringstream as_stream(contents);

	auto a = unwrap<Ast>(atl.parse.stream(as_stream));
	auto b = unwrap<Ast>(atl.parse.stream(as_stream));

	vector<int> expected = {1, 2, 3};
	for(auto& vv : zip(a, expected))
		ASSERT_EQ(unwrap<Fixnum>(*get<0>(vv)).value,
		          *get<1>(vv));

	expected = {4, 5, 6};
	for(auto& vv : zip(b, expected))
		ASSERT_EQ(unwrap<Fixnum>(*get<0>(vv)).value,
		          *get<1>(vv));

}

TEST_F(ParserTest, test_comments)
{
	string contents = ";; (a b c)\n (2 3 4)";

	auto res = unwrap<Ast>(atl.parse.string_(contents));

    auto expected = vector<Any>{
        wrap(2),
        wrap(3),
        wrap(4)
    };

	for(auto& vv : zip(res, expected))
		ASSERT_EQ(unwrap<Fixnum>(*get<0>(vv)).value,
		          unwrap<Fixnum>(*get<1>(vv)).value);
}
