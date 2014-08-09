#ifndef LOGIC_HPP
#define LOGIC_HPP
/**
 * @file /home/ryan/programming/atl/logic.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jul 22, 2014
 */

namespace atl {
  bool is_true(Any a) { return a._value == (void*)true; }
  bool is_false(Any a) { return a._value == (void*)false; }
}

#endif
