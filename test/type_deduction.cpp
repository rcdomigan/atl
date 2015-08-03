/**
 * @file /home/ryan/programming/atl/test/type_deduction.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Feb 16, 2015
 *
 * I don't have any sort of type inference engine as yet, but I still
 * need to be able to tell a few things about types.
 *
 * TODO: This should probably be called 'deduce_return_type.cpp' or
 * something like that.
 */

#include <gtest/gtest.h>


#include "../atl.hpp"
#include "../helpers.hpp"

#include "../debug.hpp"

#include <iostream>
#include <vector>

using namespace atl;
using namespace std;

struct TypeBasics : public ::testing::Test
{
    Atl atl;
};

TEST_F(TypeBasics, Atoms)
{
    ASSERT_TRUE(is<Fixnum>(atl.string_("2")));
    ASSERT_TRUE(is<String>(atl.string_("\"Hello, world!\"")));
}


namespace cxx_wrappers
{
    long foo(bool a) { return 1; }
    bool bar(long a, long b) { return true; }
}
TEST_F(TypeBasics, CxxFunctions)
{
    typedef WrapStdFunction<long (bool)> wfoo;
    auto a = wfoo::any(&cxx_wrappers::foo, atl.gc, "wfoo");

    ASSERT_EQ(*unwrap<CxxFunctor>(a).types,
              abstract_type::make_concrete({tag<Bool>::value, tag<Fixnum>::value}));

    typedef WrapStdFunction<bool (long, long)> wbar;
    auto b = wbar::any(&cxx_wrappers::bar, atl.gc, "wbar");

    ASSERT_EQ(*unwrap<CxxFunctor>(b).types,
              abstract_type::make_concrete({tag<Fixnum>::value, tag<Fixnum>::value, tag<Bool>::value }));

    ASSERT_EQ(wfoo::arity(), 1);
    ASSERT_EQ(wbar::arity(), 2);
}

TEST_F(TypeBasics, ApplyCxx)
{
    ASSERT_TRUE(is<Fixnum>(atl.string_("(add2 1 2)")));

    //std::function
    long c = 3;

    primitives::wrap_function<bool (long, long)>
        (atl.env,
         "my-equal",
         [&](long a, long b) -> bool
        {
            return (a == b) && (b == c);
        });

    ASSERT_TRUE(is<Bool>(atl.string_("(my-equal 3 3)")));
}

TEST_F(TypeBasics, IfStatement)
{
    // function pointer
    ASSERT_TRUE(is<Fixnum>(atl.string_("(if #t 2 3)")));
    ASSERT_THROW(atl.string_("(if #t 3 \"hi\")"), WrongTypeError);
    ASSERT_TRUE(is<Fixnum>(atl.string_("(if #t (add2 1 2) (add2 3 4))")));
}

TEST_F(TypeBasics, ProcedureCall)
{
    ASSERT_TRUE(is<Fixnum>(atl.string_("((\\ (a b) (add2 a b)) 1 2)")));
}

struct DeclareType : public TypeBasics {};

TEST_F(DeclareType, deduced_tag_test)
{
    // can't actually evauate; 'a' has no value.
    ASSERT_TRUE(is<Fixnum>(atl.compile.any(atl.parse.string_("(: Fixnum a)"))));
}

TEST_F(DeclareType, test_simple)
{
    // can't actually evauate; 'a' has no value.
    ASSERT_TRUE(is<Fixnum>(atl.compile.any(atl.parse.string_("(add2 (: Fixnum 2) 3)"))));

    atl.compile.reset();
    ASSERT_EQ(unwrap<Fixnum>(atl.string_("(add2 (: Fixnum 2) 3)")).value,
              5);
}


// Test that throwing in a type declaration doesn't break simple statements.
TEST_F(DeclareType, usable_in_lambda)
{
    auto tag = atl.compile.any(atl.parse.string_("(\\ (a) (add2 (: Fixnum a) 2))"));
    ASSERT_TRUE(is<Fixnum>(tag));
    atl.compile.reset();

    tag = atl.compile.any(atl.parse.string_("((\\ (a) (add2 (: Fixnum a) 2)) 3)"));
    ASSERT_TRUE(is<Fixnum>(tag));

    ASSERT_EQ(unwrap<Fixnum>(atl.string_("((\\ (a) (add2 (: Fixnum a) 2)) 3)")).value,
              5);

}

// TEST_F(DeclareType, type_function)
// {
//     "(: (\\ (a b) (add2 a b)) (-> Fixnum Fixnum Fixnum))";
// }
