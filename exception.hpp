#ifndef ATL_EXCEPTION_HPP
#define ATL_EXCEPTION_HPP
/**
 * @file /home/ryan/programming/atl/exception.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Aug 31, 2014
 */

#include <string>
#include <stdexcept>

namespace atl
{
	struct WrongTypeError : public std::runtime_error {
		WrongTypeError(const std::string& what_arg) : std::runtime_error(what_arg) {}
		WrongTypeError(const char* what_arg) : std::runtime_error(what_arg) {}
	};

	struct RangeError : public std::runtime_error {
		RangeError(const std::string& what_arg) : std::runtime_error(what_arg) {}
		RangeError(const char* what_arg) : std::runtime_error(what_arg) {}
	};

	struct ArityError : public std::runtime_error {
		ArityError(const std::string& what_arg) : std::runtime_error(what_arg) {}
		ArityError(const char* what_arg) : std::runtime_error(what_arg) {}
	};

	struct UnbalancedParens : public std::runtime_error {
		UnbalancedParens(const std::string& what_arg) : std::runtime_error(what_arg) {}
		UnbalancedParens(const char* what_arg) : std::runtime_error(what_arg) {}
	};

	struct MultiExpressionString : public std::runtime_error {
		MultiExpressionString(const std::string& what_arg) : std::runtime_error(what_arg) {}
		MultiExpressionString(const char* what_arg) : std::runtime_error(what_arg) {}
	};

	struct UnboundSymbolError : public std::runtime_error {
		UnboundSymbolError(const std::string& what_arg) : std::runtime_error(what_arg) {}
	};

	struct BadPCodeInstruction : public std::runtime_error {
		BadPCodeInstruction(const std::string& what_arg) : std::runtime_error(what_arg) {}
	};

	struct NoFunctionForScope : public std::runtime_error {
		NoFunctionForScope(const std::string& what_arg) : std::runtime_error(what_arg) {}
	};

	struct RedefinitionError : public std::runtime_error {
		RedefinitionError(const std::string& what_arg) : std::runtime_error(what_arg) {}
	};

	struct CantUnify : public std::runtime_error {
		CantUnify(const std::string& what_arg) : std::runtime_error(what_arg) {}
	};

	struct AlgorithmWFailed : public std::runtime_error {
		AlgorithmWFailed(const std::string& what_arg) : std::runtime_error(what_arg) {}
	};

	struct PatternMatchFailed : public std::runtime_error {
		PatternMatchFailed(const std::string& what_arg) : std::runtime_error(what_arg) {}
	};

	struct CxxNotImplemented : public std::runtime_error {
		CxxNotImplemented(const std::string& what_arg) : std::runtime_error(what_arg) {}
	};
}

#endif
