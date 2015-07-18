/**
 * @file /home/ryan/programming/atl/test/abstract_type.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on May 16, 2015
 */

#include <gtest/gtest.h>

#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include <abstract_type.hpp>

using namespace atl;
using namespace std;

std::ostream& atl::abstract_type::Node::print(std::ostream& out) const
{
    return out << tag << ":" << count << ":" << type;
}

TEST(AbstractTypeTest, equality)
{
    using namespace abstract_type;
    typedef abstract_type::Type T;

    auto t0 = make_abstract({T(0, abstract), make_abstract({1, 2})}),
        t1 = make_abstract({T(0, abstract), make_abstract({1, 2})});

    ASSERT_TRUE(t0.is_function());
    ASSERT_EQ(t0, t1);

    ASSERT_NE(t0, make_abstract({0, 1}));
    ASSERT_NE(t0, make_abstract({0, 1, 2, 3}));
    ASSERT_NE(t0, make_abstract({T(1, abstract), make_abstract({1, 2})}));
}
