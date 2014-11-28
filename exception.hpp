#ifndef ATL_EXCEPTION_HPP
#define ATL_EXCEPTION_HPP
/**
 * @file /home/ryan/programming/atl/exception.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Aug 31, 2014
 */

#include <exception>

namespace atl {
    struct WrongTypeError : public std::runtime_error {
	WrongTypeError(const std::string& what_arg) : std::runtime_error(what_arg) {}
	WrongTypeError(const char* what_arg) : std::runtime_error(what_arg) {}
    };

    struct UnbalancedParens : public std::runtime_error {
	UnbalancedParens(const std::string& what_arg) : std::runtime_error(what_arg) {}
	UnbalancedParens(const char* what_arg) : std::runtime_error(what_arg) {}
    };

    struct UnboundSymbolError : public std::runtime_error {
        UnboundSymbolError(const std::string& what_arg) : std::runtime_error(what_arg) {}
    };
}

#endif
