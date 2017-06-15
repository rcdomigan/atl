/**
 * @file /home/ryan/programming/atl/test/atl.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Aug 05, 2015
 *
 * Regression tests for integrated behaviour:
 * - repl like calls
 * - loading form a file
 */

#include <atl/atl.hpp>
#include <atl/primitive_callable.hpp>

#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <iterator>

struct AtlTest
	: public ::testing::Test
{
	atl::Atl atl;

    AtlTest()
	{ atl::export_primitives(atl); }
};

TEST_F(AtlTest, test_basic_string)
{
	using namespace atl;
	std::stringstream output;
	atl.stdout = &output;

	ASSERT_EQ(wrap<Fixnum>(3), atl.eval("(add2 1 2)"));
}

TEST_F(AtlTest, test_basic_stream)
{
	using namespace atl;
	std::stringstream content("(add2 1 2)");
	ASSERT_EQ(wrap<Fixnum>(3), atl.eval(content));
}

TEST_F(AtlTest, test_defining_function)
{
	using namespace atl;
	std::stringstream output;
	atl.stdout = &output;

	std::stringstream content("(define foo (__\\__ (a) (add2 a 3)))\n");

	atl.eval(content);
	ASSERT_EQ(wrap<Fixnum>(5), atl.eval("(foo 2)"));
}

TEST_F(AtlTest, test_string_stream)
{
	using namespace atl;
	std::stringstream output;
	atl.stdout = &output;

	std::stringstream content
		(";; used to test atl.cpp\n"
		 "(define foo (__\\__ (a) (print-int (add2 a 3))))\n"
		 "(define main (__\\__ () (foo 2)))\n"
		 "(main)");

	atl.eval(content);
	atl.eval(content);
	atl.eval(content);

	ASSERT_EQ(output.str(), "5\n");
}

TEST_F(AtlTest, test_loaded_file)
{
	using namespace atl;
	std::stringstream output;
	atl.stdout = &output;

	std::ifstream file("./simple.atl");
	atl.eval(file);
	atl.eval("(foo 2)");

	ASSERT_EQ(output.str(), "5\n");
}

TEST_F(AtlTest, test_multiline_stream)
{
	using namespace atl;
	std::stringstream content
		("(define foo (__\\__ (a b) (add2 (add2 a 2) b)))\n"
		 "\n"
		 "(define bar (__\\__ (a b) (add2 (add2 a 3) b)))\n"
		 "(define main (__\\__ () (bar 2 3)))");
	atl.eval(content);
	atl.eval(content);
	atl.eval(content);

	ASSERT_EQ(wrap<Fixnum>(8), atl.eval("(main)"));
}

TEST_F(AtlTest, test_file_contents)
{
	using namespace atl;
	std::ifstream file("./simple.atl");
	std::string content((std::istreambuf_iterator<char>(file)),
	                    std::istreambuf_iterator<char>());

	atl.eval(content);
	atl.eval("(foo 2)");
}
