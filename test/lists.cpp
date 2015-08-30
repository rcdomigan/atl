
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

struct ListTest : public ::testing::Test
{
    Atl atl;

    // Assert that two simple lists (nested lists, Symbols, and
    // Fixnums) are equivalent.
    template<class Range0, class Range1>
    void assert_equiv(Range0 const& aa, Range1 const& bb)
    {
        auto aitr = aa.begin(), bitr = bb.begin();
        for(;
            (aitr != aa.end()) && (bitr != bb.end());
            ++aitr, ++bitr)
            {
                ASSERT_EQ(((*aitr)._tag),
                          ((*bitr)._tag));
                switch((*aitr)._tag)
                    {
                    case tag<Symbol>::value:
                        ASSERT_EQ((unwrap<std::string>(*aitr)),
                                  (unwrap<std::string>(*bitr)));
                        break;
                    case tag<Ast>::value:
                        assert_equiv((unwrap<Ast>(*aitr)),
                                     (unwrap<Ast>(*bitr)));
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

    bool _assert_not_equiv(Ast const& aa, Ast const& bb)
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
                        if(_assert_not_equiv((unwrap<Ast>(*aitr)),
                                             (unwrap<Ast>(*bitr))))
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

    void assert_not_equiv(Ast const& aa, Ast const& bb)
    { ASSERT_TRUE(_assert_not_equiv(aa, bb)); }
};


TEST_F(ListTest, test_equiv_ast)
{
    // Test manually constructed asts and my test harness's equiv
    // function.
    auto seq = atl.gc.dynamic_seq();
    auto car = seq->push_seq<Ast>();

    seq->push_back(wrap<Fixnum>(1));
    seq->push_back(wrap<Fixnum>(2));
    car->end_at(seq->end());

    auto seq3 = atl.gc.dynamic_seq();
    car = seq3->push_seq<Ast>();

    seq3->push_back(wrap<Fixnum>(1));
    seq3->push_back(wrap<Fixnum>(2));
    car->end_at(seq3->end());

    auto seq2 = atl.gc.dynamic_seq();
    car = seq2->push_seq<Ast>();

    seq2->push_back(wrap<Fixnum>(1));
    seq2->push_back(wrap<Fixnum>(2));
    seq2->push_back(wrap<Fixnum>(3));
    car->end_at(seq2->end());


    assert_not_equiv((unwrap<Ast>(*seq->begin())),
                     (unwrap<Ast>(*seq2->begin())));

    assert_equiv((unwrap<Ast>(*seq->begin())),
                 (unwrap<Ast>(*seq3->begin())));
}

TEST_F(ListTest, test_quote)
{
    // Constructing a list from function calls is complicated by the
    // need to tag the list memebers.  Make sure quoting works first.
    {
        auto rval = atl.string_("'(1 2 (a b))");

        ASSERT_EQ(tag<Pointer>::value, rval._tag);
        assert_equiv((unwrap<Ast>(atl.parse.string_("(1 2 (a b))"))),
                     (unwrap<Ast>(*unwrap<Pointer>(rval).value)));
    }

    {
        auto rval = atl.string_("'(1)");
        assert_equiv((unwrap<Ast>(atl.parse.string_("(1)"))),
                     (unwrap<Ast>(*unwrap<Pointer>(rval).value)));
    }
}

// Check that I can quote an embedded Ast
TEST_F(ListTest, test_quote_embedded)
{
	using namespace make_ast;
	Arena arena;

	auto inner = make
		(lift(1), lift(2), lift(3))
		(*arena.dynamic_seq());

	auto expr = make
		(lift<Quote>(),
		 lift(*inner))
		(*arena.dynamic_seq());

	assert_equiv(*expr,
	             unwrap<Ast>(atl.parse.string_("'(1 2 3)")));
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
		(*arena.dynamic_seq());

	auto expr = make
		(lift(arena.amake<Symbol>("nth")),
		 lift(*inner),
		 lift(1))
		(*arena.dynamic_seq());

	auto rval = atl.eval(wrap(*expr));
	ASSERT_EQ(unwrap<Fixnum>(*unwrap<Pointer>(rval).value).value,
	          2);
}

TEST_F(ListTest, test_slice)
{
	auto result = atl.string_("(slice '(1 2 3 4) 2)");

    assert_equiv((unwrap<Slice>(result)),
                 (unwrap<Ast>(atl.parse.string_("(3 4)"))));
}


TEST_F(ListTest, test_make_ast)
{
	using namespace make_ast;

	{
		auto ast = make(lift(1), lift(2), lift(3))
			(*atl.env.gc.dynamic_seq());

		assert_equiv((*ast),
		             unwrap<Ast>(atl.parse.string_("(1 2 3)")));
	}

	{
		auto ast = make(lift(1),
		                make(lift(2), lift(3)),
		                lift(4))
			(*atl.env.gc.dynamic_seq());

		assert_equiv((*ast),
		             unwrap<Ast>(atl.parse.string_("(1 (2 3) 4)")));
	}
}

TEST_F(ListTest, test_list)
{
    {
        auto rval = atl.string_("(Ast 1 2 3)");
        assert_equiv((unwrap<Ast>(rval)),
                     (unwrap<Ast>(atl.parse.string_("(1 2 3)"))));
    }

    {
        auto rval = atl.string_("(Ast)");
        assert_equiv((unwrap<Ast>(rval)),
                     (*make_ast::make()(*atl.env.gc.dynamic_seq())));
    }

    {
        auto rval = atl.string_("(Ast 1 (Ast 2 3))");
        assert_equiv((unwrap<Ast>(rval)),
                     (unwrap<Ast>(atl.parse.string_("(1 (2 3))"))));
    }

    {
        auto rval = atl.string_("(Ast 1 (Ast 2 3) 4)");
        assert_equiv((unwrap<Ast>(rval)),
                     (unwrap<Ast>(atl.parse.string_("(1 (2 3) 4)"))));
    }
}

TEST_F(ListTest, test_cons)
{
    auto result = atl.string_("(cons 0 '(1))");
    cout << printer::any(result) << endl;
}

