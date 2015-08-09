#ifndef ATL_HPP
#define ATL_HPP
/**
 * @file /home/ryan/programming/atl/atl.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Feb 07, 2015
 */

#include <type.hpp>
#include <tiny_vm.hpp>
#include <primitive_callable.hpp>
#include <compile.hpp>
#include <gc.hpp>
#include <parser.hpp>
#include <environment.hpp>
#include <compile.hpp>
#include <print.hpp>

namespace atl {
    struct Atl {
        GC gc;
        ParseString parse;
        Environment env;
        Compile compile;
        TinyVM vm;

        Atl() : parse(gc), env(gc), compile(env) {
            setup_interpreter(env, parse);
        }

	    void set_stdout(std::ostream& out)
	    { env.stdout = &out; }

        Any eval(Any value)
        {
            auto tag = compile.any(value);
            compile.assert_ready();
#ifdef DEBUGGING
            compile.dbg();
            vm.run_debug(*compile.wrapped.output, compile.wrapped.main_entry_point);
#else
            vm.run(*compile.wrapped.output, compile.wrapped.main_entry_point);
#endif
            compile.repl_reset();

            return Any(tag, reinterpret_cast<void*>(vm.stack[0]));
        }

	    // Parse, compile, and run the first sexpr in a string
        Any string_(const std::string& ss) {
	        return eval(parse.string_(ss));
        }

	    // Parse, compile, and run all the sexprs in a stream
        Any stream(istream& ss)
	    {
		    Any rval;
		    while(ss.peek() != EOF)
			    rval = eval(parse.stream(ss));
		    return rval;
	    }
    };
}

#endif
