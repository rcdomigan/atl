/**
 * @file /home/ryan/programming/atl/test/macros.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on May 05, 2015
 */

#include <gtest/gtest.h>

#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include <atl.hpp>
// #include <annotate.hpp>

using namespace std;
using namespace atl;

struct TestMacros : public ::testing::Test
{
    Atl atl;
};


TEST_F(TestMacros, testing)
{
    auto macro = atl.string_
        ("(define-macro (add3 ast) (list add2 (car ast) (list add2 (cadr ast) (caddr ast))))")
}
