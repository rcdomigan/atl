
/**
 * @file /home/ryan/programming/atl/test/lists.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Feb 14, 2015
 */

#include "../atl.hpp"
#include "../helpers.hpp"

#include "../debug.hpp"

#include <iostream>
#include <vector>

#include <gtest/gtest.h>


using namespace atl;
using namespace std;

// Convert a wrapped Ast or AstData to an AstData const&
AstData const& datafy(Any const& input)
{
	switch(input._tag)
		{
		case tag<Ast>::value:
			return *unwrap<Ast>(input).value;
		case tag<AstData>::value:
			return unwrap<AstData>(input);
		default:
			throw WrongTypeError
				("Can only datify the Ast family.");
		}
}

struct ListTest : public ::testing::Test
{
    Atl atl;

    // Assert that two simple lists (nested lists, Symbols, and
    // Fixnums) are equivalent.
    void assert_equiv(AstData const& aa, AstData const& bb)
    {
        auto aitr = aa.begin(), bitr = bb.begin();
        for(;
            (aitr != aa.end()) && (bitr != bb.end());
            ++aitr, ++bitr)
            {
                switch((*aitr)._tag)
                    {
                    case tag<Symbol>::value:
                        ASSERT_EQ((unwrap<std::string>(*aitr)),
                                  (unwrap<std::string>(*bitr)));
                        break;
                    case tag<Ast>::value:
	                    assert_equiv(datafy(*aitr),
                                     datafy(*bitr));
                        break;
                    case tag<AstData>::value:
	                    assert_equiv(datafy(*aitr),
	                                 datafy(*bitr));
                        break;
                    default:
                        ASSERT_EQ((*aitr),
                                  (*bitr));
                        break;
                    }
            }
        ASSERT_EQ((aitr == aa.end()),
                  (bitr == bb.end()));
    }

    bool _assert_not_equiv(AstData const& aa, AstData const& bb)
    {
        auto aitr = aa.begin(), bitr = bb.begin();
        for(;
            (aitr != aa.end()) || (bitr != bb.end());
            ++aitr, ++bitr)
            {
                if((*aitr)._tag != (*bitr)._tag)
                    return true;

                switch((*aitr)._tag)
                    {
                    case tag<Symbol>::value:
                        if(unwrap<std::string>(*aitr) != unwrap<std::string>(*bitr))
                            return true;
                        break;
                    case tag<Ast>::value:
                        if(_assert_not_equiv((datafy(*aitr)),
                                             (datafy(*bitr))))
                            return true;
                        break;
                    default:
                        if((*aitr).value != (*bitr).value)
                            return true;
                        break;
                    }
            }
        return (aitr != aa.end()) && (bitr != bb.end());
    }

    void assert_not_equiv(AstData const& aa, AstData const& bb)
    { ASSERT_TRUE(_assert_not_equiv(aa, bb)); }
};


TEST_F(ListTest, test_equiv_ast)
{
    // Test manually constructed asts and my test harness's equiv
    // function.
    auto& seq = atl.gc.sequence();
    auto car = push_nested_ast(seq);

    seq.push_back(wrap<Fixnum>(1));
    seq.push_back(wrap<Fixnum>(2));
    car.end_ast();

    auto& seq3 = atl.gc.sequence();
    car = push_nested_ast(seq3);

    seq3.push_back(wrap<Fixnum>(1));
    seq3.push_back(wrap<Fixnum>(2));
    car.end_ast();

    auto& seq2 = atl.gc.sequence();
    car = push_nested_ast(seq2);

    seq2.push_back(wrap<Fixnum>(1));
    seq2.push_back(wrap<Fixnum>(2));
    seq2.push_back(wrap<Fixnum>(3));
    car.end_ast();

    assert_equiv(unwrap<AstData>(seq[0]),
                 unwrap<AstData>(seq3[0]));

    assert_not_equiv((unwrap<AstData>(seq[0])),
                     (unwrap<AstData>(seq2[0])));
}

TEST_F(ListTest, test_make_ast)
{
	using namespace make_ast;
	Arena arena;

	auto allocer = ast_alloc(arena);
	auto expr = make
		(lift(1), lift(2), lift(3))
		(allocer);

	ASSERT_EQ(3, expr.size());

	auto expected = vector<Any>{
        wrap(1),
        wrap(2),
        wrap(3)
	};

	for(auto& vv : zip(expected, expr))
		ASSERT_EQ(*get<0>(vv), *get<1>(vv));
}

TEST_F(ListTest, test_quote)
{
    // Constructing a list from function calls is complicated by the
    // need to tag the list memebers.  Make sure quoting works first.
    {
        auto rval = atl.string_("'(1 2 (a b))");

        ASSERT_EQ(tag<Pointer>::value, rval._tag);
        assert_equiv((*unwrap<Ast>(atl.parse.string_("(1 2 (a b))")).value),
                     (*unwrap<Ast>(rval).value));
    }

    {
        auto rval = atl.string_("'(1)");
        assert_equiv((*unwrap<Ast>(atl.parse.string_("(1)")).value),
                     (*unwrap<Ast>(rval).value));
    }
}

// Check that I can quote an embedded Ast
TEST_F(ListTest, test_quote_embedded)
{
	using namespace make_ast;
	Arena arena;

	auto inner = make
		(lift(1), lift(2), lift(3))
		(ast_alloc(arena));

	auto expr = make
		(lift<Quote>(),
		 lift(inner))
		(ast_alloc(arena));

	auto parsed = atl.parse.string_("'(1 2 3)");

	assert_equiv(*expr.value,
	             *unwrap<Ast>(parsed).value);
}


TEST_F(ListTest, test_indexing)
{
    {
        auto rval = atl.string_("(nth '(1 2 3) 0)");
        ASSERT_EQ((unwrap<Fixnum>(*unwrap<Pointer>(rval).value).value),
                  1);
    }

    {
        auto rval = atl.string_("(nth '(1 2 3) 1)");
        ASSERT_EQ((unwrap<Fixnum>(*unwrap<Pointer>(rval).value).value),
                  2);
    }
}

// Check that I can get the index of an embedded Ast
TEST_F(ListTest, test_index_embedded)
{
	using namespace make_ast;
	Arena arena;

	auto inner = make
		(lift<Quote>(),
		 make(lift(1), lift(2), lift(3)))
		(ast_alloc(arena));

	auto expr = make
		(sym("nth"),
		 lift(inner),
		 lift(1))
		(ast_alloc(arena));

	auto rval = atl.eval(PassByValue(expr));
	ASSERT_EQ(2, unwrap<Fixnum>(*unwrap<Pointer>(rval).value).value);
}

// TODO:
// TEST_F(ListTest, test_slice)
// {
// 	auto result = atl.string_("(slice '(1 2 3 4) 2)");

//     assert_equiv((unwrap<Slice>(result)),
//                  (unwrap<AstData>(atl.parse.string_("(3 4)").value)));
// }


// Test uses parser so I'm seperating it from test_make_ast
TEST_F(ListTest, test_make_nested_ast)
{
	using namespace make_ast;
	auto ast = make(lift(1),
	                make(lift(2), lift(3)),
	                lift(4))
		(ast_alloc(atl.env.gc));

	assert_equiv(*ast.value,
	             *unwrap<Ast>(atl.parse.string_("(1 (2 3) 4)")).value);
}

TEST_F(ListTest, test_nested_lambda)
{
	using namespace make_ast;
	Arena arena;

	auto expr = make
		(sym("lambda"),
		 make(sym("a")),
		 make(sym("lambda"),
		      make(sym("b")),
		      make(sym("a"), sym("b"))))
		(ast_alloc(arena));

	assert_equiv(*expr.value,
	             *unwrap<Ast>
	             (atl.parse.string_
	              ("(lambda (a) (lambda (b) (a b)))")).value);
}

// TODO:
/* TEST_F(ListTest, test_cons) */
/* { */
/*     auto result = atl.string_("(cons 0 '(1))"); */
/*     assert_equiv(*unwrap<Ast>(atl.parse.string_("(0 1)")).value, */
/*                  *unwrap<Ast>(result).value); */
/* } */

