#ifndef ATL_HPP
#define ATL_HPP
/**
 * @file /home/ryan/programming/atl/atl.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Feb 07, 2015
 */

#include <iostream>                 // for cout, ostream
#include <atl/compile.hpp>              // for Compile
#include <atl/lexical_environment.hpp>  // for AssignForms, AssignFree, BackPatch
#include <atl/parser.hpp>               // for ParseString
#include <atl/type.hpp>                 // for init_types, Any, LAST_CONCRETE_TYPE
#include <atl/type_inference.hpp>       // for AlgorithmW, apply_substitution
#include <atl/vm.hpp>                   // for TinyVM, TinyVM::value_type
#include "gc/gc.hpp"                // for GC, ast_composer
#include "gc/marked.hpp"            // for Marked
#include "wrap.hpp"                 // for unwrap


namespace atl
{
    struct Atl
    {
        GC gc;
        ParseString parse;
        ToplevelMap lexical;
        Compile compile;
        TinyVM vm;

	    Environment env;

	    Atl() : parse(gc), compile(lexical, gc),
	            env(gc, parse, lexical, compile, vm)
	    { setup_interpreter(env); }

	    void set_stdout(std::ostream& out)
	    { env.stdout = &out; }

        PassByValue eval(PassByValue value)
	    { return env.eval(value); }

	    // Parse, compile, and run the first sexpr in a string
        PassByValue string_(const std::string& ss)
	    { return eval(parse.string_(ss)); }

	    // Parse, compile, and run all the sexprs in a stream
        PassByValue stream(istream& ss)
	    {
		    PassByValue rval;
		    while(ss.peek() != EOF)
			    rval = eval(parse.stream(ss));
		    return rval;
	    }
    };
}

#endif
