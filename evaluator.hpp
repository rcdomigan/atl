#ifndef ATL_EVALUATOR_HPP
#define ATL_EVALUATOR_HPP
/**
 * @file /home/ryan/programming/atl/evaluator.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jun 29, 2013
 *
 * Evaluate an AST expression
 */

#include <string>
#include <map>
#include <iostream>
#include <functional>

#include "./exception.hpp"
#include "./parser.hpp"

#include "./type_testing.hpp"
#include "./conversion.hpp"
#include "./encode.hpp"
#include "./logic.hpp"
#include "./type_class.hpp"

namespace atl {
    class EvaluateAST {
	// GC &_gc;
	Environment &_env;

	// get the value of an item without trying to apply it to anything
	Any* argument_value(Any* stack) {
	reuse_frame:
	    switch(stack->_tag) {
	    case tag<Ast>::value: {
		reduce(stack, unwrap<Ast>(*stack));
		return stack + 1;
	    }
	    case tag<Parameter>::value:
		*stack = unwrap<Parameter>(*stack).value();
		if(is<Ast>(*stack))
		    stack->_tag = tag<Data>::value;
		goto reuse_frame;
	    case tag<Pointer>::value:
		*stack = unwrap<Pointer>(*stack).value();
		goto reuse_frame;
	    default:
		return stack + 1;
	    }}

	template<class Range>
	Any* bind_args(Any* stack, const Range& ast) {
	    for(const Any& vv : ast) {
		*stack = vv;
		stack = argument_value(stack);
	    }
	    return stack;
	}

	// Deal with the application position element of an AST.
	// @tparam Range: type of the AST container
	// @param stack: output vector
	// @param ast: input AST
	template<class Range>
	void apply(Any *stack, const Range ast) {
	    using namespace std;

	    auto do_apply = [&](const Procedure &fn, Any* args_end) {
		auto old_stack = fn._stack;

		fn._stack = stack;
		*args_end = fn._body;

		argument_value(args_end);

		fn._stack = old_stack;
		*stack = *args_end;
	    };

	    switch(stack->_tag) {
	    case tag<If>::value: {
		*stack = ast[1];
		argument_value(stack);
		stack[0] = is_false(*stack) ? ast[3] : ast[2];
		return;
	    }
	    case tag<DefineMacro>::value:
		*stack = ast[2];
		argument_value(stack);
		stack[0]._tag = tag<Macro>::value;
		goto backpatch_definition;
	    case tag<Define>::value:
		*stack = ast[2];
		argument_value(stack);
	    backpatch_definition: {
		    Undefined *u = reinterpret_cast<Undefined*>(ast[1]._value);
		    for( auto i : u->_backtrack ) *i = *stack;
		    return;
		}
	    case tag<Macro>::value: {
		auto &fn = unwrap<Procedure>(*stack);
		*stack = ast[1];
		stack->_tag = tag<Data>::value;
		do_apply(fn, stack + 1);
		return;
	    }
	    case tag<Procedure>::value: {
		auto& fn = unwrap<Procedure>(*stack);
		auto args_end = bind_args(stack, slice(ast, 1));
		do_apply(fn, args_end);
		return;
	    }
	    case tag<PrimitiveRecursive>::value: {
		auto end = bind_args(stack + 1, slice(ast, 1));
		stack[0] = unwrap<PrimitiveRecursive>(*stack).call(stack + 1, end);
		return;
	    }
	    case tag<Quote>::value:
		*stack = ast[1];
		return;
	    default: {
		auto end = bind_args(stack + 1, slice(ast, 1));
		stack[0] = Applicable(_env)[stack->_tag](stack, end);
		return;
	    }}}

	template<class Range>
	void reduce(Any *stack, const Range ast) {
	    *stack = ast[0];

	    argument_value(stack);
	    apply(stack, ast);
	    argument_value(stack);
	}
    public:
	EvaluateAST(Environment& env) : _env(env) {}

	Any range(const Ast& ast) {
	    Any stack[100];

	    reduce(stack, make_range(ast));
	    return stack[0];
	}

	Any any(Any vv) {
	    if(is<Ast>(vv)) {
		return range(unwrap<Ast>(vv));
	    } else {
		Any result;
		argument_value(&result);
		return result;
	    }}

	// GC& gc() { return _gc; }
    };
}

#endif
