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

TEST_F(AtlTest, test_atom)
{
	auto rval = atl.string_("5");
	ASSERT_EQ(unwrap<Fixnum>(rval).value,
	          5);
}

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


TEST_F(AtlTest, running_embedded_ast)
{
	using namespace make_ast;
	auto& gc = atl.gc;

	auto sym = [&](std::string const& name) -> Symbol*
		{
			return gc.make<Symbol>(name);
		};

	auto formals = make(lift(sym("a")), lift(sym("b")))
		(ast_alloc(gc));

	auto body = make(lift(sym("add2")),
	                 make(lift(sym("add2")),
	                      lift(sym("a")),
	                      lift(2)),
	                 lift(sym("b")))
		(ast_alloc(gc));

	auto expr =
		make(make(lift(wrap<Lambda>()),
		          lift(*formals),
		          lift(*body)),
		     lift(1),
		     lift(2))
		(ast_alloc(gc));

	auto rval = atl.eval(wrap(*expr));
	ASSERT_EQ((unwrap<Fixnum>(rval).value), 5);
}
