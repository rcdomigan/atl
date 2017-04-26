
/**
 * @file /home/ryan/programming/atl/test/test_parser.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Dec 16, 2014
 */

#include <gtest/gtest.h>

#include <atl/parser.hpp>
#include <atl/helpers/pattern_match.hpp>
#include <atl/helpers/make_ast.hpp>

#include <iostream>
#include <vector>
#include <sstream>


using namespace atl;
using namespace std;

struct ParserTest
	: public ::testing::Test
{
	GC store;
	ParseString parser;

	ParserTest()
		: parser(store)
	{ init_types(); }
};

TEST_F(ParserTest, test_simple_int_list) {
	using namespace make_ast;
    auto parsed = parser.parse("(1 2 3)");

    auto expected = store(mk(1, 2, 3));

    for(auto& vv : zip(*parsed, *expected))
	    {
#ifdef DEBUGGING
		    cout << "parsed: " << printer::print(*get<0>(vv)) << "\nexpected: " << printer::print(*get<1>(vv)) << endl;
#endif
		    ASSERT_EQ(*get<0>(vv), *get<1>(vv));
	    }

}

TEST_F(ParserTest, test_empty_list)
{
	using namespace pattern_match;
    auto parsed = parser.parse("()");


    ASSERT_TRUE(match(ast(), parsed))
	    << printer::print(*parsed) << std::endl;
}

TEST_F(ParserTest, nested_int_list)
{
	using namespace pattern_match;
    auto parsed = parser.parse("(1 2 (4 5) 3)");

    ASSERT_TRUE
	    (literal_match
	     (ast(1, 2, ast(4, 5), 3),
	      parsed))
	    << printer::print(*parsed) << std::endl;;
}

TEST_F(ParserTest, test_parsing_int_list_from_stream)
{
	using namespace pattern_match;

	std::stringstream input{"(1 2 (4 5) 3) foo"};
    auto parsed = parser.parse(input);

    ASSERT_TRUE
	    (literal_match(ast(1, 2, ast(4, 5), 3),
	                   parsed))
	    << printer::print(*parsed) << std::endl;;
}

TEST_F(ParserTest, TestQuote) {
	using namespace pattern_match;

    auto parsed = parser.parse("'(2 3)");
    ASSERT_TRUE
	    (literal_match(ast(wrap<Quote>(), ast(2, 3)),
	                   parsed))
	    << printer::print(*parsed) << std::endl;;
}

TEST_F(ParserTest, test_nested_quote)
{
	using namespace pattern_match;
	auto parsed = parser.parse("(1 '(2 3) 4)");

	ASSERT_TRUE
		(literal_match(ast(1, ast(wrap<Quote>(), ast(2, 3)), 4),
		               parsed));
}

TEST_F(ParserTest, test_stream_parsing)
{
	string contents = "(a b c)";
	stringstream as_stream(contents);

	auto from_string = parser.parse(contents);
	auto from_stream = parser.parse(as_stream);

	for(auto& vv : zip(*from_string, *from_stream))
		{ ASSERT_EQ(unwrap<Symbol>(*get<0>(vv)).name,
		            unwrap<Symbol>(*get<1>(vv)).name); }
}

TEST_F(ParserTest, test_multi_line_stream_parsing)
{
	string contents = "(1 2 3)\n\n(4 5 6)";
	stringstream as_stream(contents);

	vector<int> expected = {1, 2, 3};
	for(auto& vv : zip(*parser.parse(as_stream),
	                   expected))
		ASSERT_EQ(unwrap<Fixnum>(*get<0>(vv)).value,
		          *get<1>(vv));

	expected = {4, 5, 6};
	for(auto& vv : zip(*parser.parse(as_stream),
	                   expected))
		{
			ASSERT_EQ(unwrap<Fixnum>(*get<0>(vv)).value,
			          *get<1>(vv));
		}

}

TEST_F(ParserTest, test_comments)
{
	string contents = ";; (a b c)\n (2 3 4)";

    auto expected = vector<Any>{
        wrap(2),
        wrap(3),
        wrap(4)
    };

    for(auto& vv : zip(*parser.parse(contents),
                       expected))
		ASSERT_EQ(unwrap<Fixnum>(*get<0>(vv)).value,
		          unwrap<Fixnum>(*get<1>(vv)).value);
}
