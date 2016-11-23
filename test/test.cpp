/**
 * @file /home/ryan/programming/atl/test/test.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Feb 21, 2015
 */

#include <gtest/gtest.h>

#include "./helpers.cpp"
#include "./utility.cpp"
#include "./test_type.cpp"
#include "./vm.cpp"
#include "./parser.cpp"
#include "./lexical_environment.cpp"
#include "./compile.cpp"
#include "./type_inference.cpp"
#include "./analyze_and_compile.cpp"

// #include "./atl.cpp"

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
