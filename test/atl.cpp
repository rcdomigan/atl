/**
 * @file /home/ryan/programming/atl/test/atl.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Aug 05, 2015
 *
 * Regression tests for integrated behaviour:
 * - repl like calls
 * - loading form a file
 */

#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <atl.hpp>
#include <iterator>

using namespace atl;

struct AtlTest : public ::testing::Test {
	Atl atl;
    AtlTest() : atl() {}
};

TEST_F(AtlTest, test_string_stream)
{
	std::stringstream output;
	atl.set_stdout(output);

	std::stringstream content
		(";; used to test atl.cpp\n"
		 "(define-value foo (\\ (a) (print-int (add2 a 3))))");
	atl.stream(content);
	atl.string_("(foo 2)");

	ASSERT_EQ(output.str(), "5\n");
}

TEST_F(AtlTest, test_loaded_file)
{
	std::stringstream output;
	atl.set_stdout(output);

	std::ifstream file("./simple.atl");
	atl.stream(file);
	atl.string_("(foo 2)");

	ASSERT_EQ(output.str(), "5\n");
}

TEST_F(AtlTest, test_multiline_stream)
{
	std::stringstream content
		("(define-value foo (\\ (a b) (add2 (add2 a 2) b)))\n"
		 "\n"
		 "(define-value bar (\\ (a b) (add2 (add2 a 3) b)))");
	atl.stream(content);
	ASSERT_EQ((unwrap<Fixnum>(atl.string_("(bar 2 3)")).value),
	          8);
}

TEST_F(AtlTest, test_file_contents)
{
	std::ifstream file("./simple.atl");
	std::string content((istreambuf_iterator<char>(file)),
	                    istreambuf_iterator<char>());

	atl.string_(content);
	atl.string_("(foo 2)");
}
