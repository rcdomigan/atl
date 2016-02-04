#ifndef ATL_HPP
#define ATL_HPP
/**
 * @file /home/ryan/programming/atl/atl.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Feb 07, 2015
 */

#include <type.hpp>
#include <vm.hpp>
#include <primitive_callable.hpp>
#include <compile.hpp>
#include <gc.hpp>
#include <parser.hpp>
#include <lexical_environment.hpp>
#include <environment.hpp>
#include <compile.hpp>
#include <print.hpp>

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
