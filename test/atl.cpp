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

using namespace atl;

struct AtlTest : public ::testing::Test {
	Atl atl;
    AtlTest() : atl() {}
};

TEST_F(AtlTest, test_file_contents)
{
	std::string content;
	std::ifstream file("./simple.atl");
	std::getline(file, content);

	atl.string_(content);
	atl.string_("(foo 2)");
}

TEST_F(AtlTest, test_loaded_file)
{
	std::ifstream file("./simple.atl");
	atl.stream(file);
	atl.string_("(foo 2)");
}
