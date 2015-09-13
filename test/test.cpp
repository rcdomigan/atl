/**
 * @file /home/ryan/programming/atl/test/test.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Feb 21, 2015
 */

#include <gtest/gtest.h>

#include <ffi.hpp>
#include <tiny_vm.hpp>

#include "./testing_utils.hpp"

#include "./utility.cpp"
#include "./test_type.hpp"
#include "./vm.cpp"
#include "./parser.cpp"
#include "./compile.cpp"
#include "./lists.cpp"
#include "./primitive.cpp"
#include "./macros.cpp"
#include "./gc.cpp"
#include "./type_deduction.cpp"

#include "./atl.cpp"

int main(int argc, char *argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
