#ifndef ATL_HPP
#define ATL_HPP
/**
 * @file /home/ryan/programming/atl/atl.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Feb 07, 2015
 */

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
            setup_interpreter(gc, env, parse);
        }

        void eval(Any value) {
            compile.any(value);
            vm.run(compile._wrapped.main_entry_point);
        }

        void string_(const std::string& ss) { eval(parse.string_(ss)); }
        void stream(istream& ss) { eval(parse.stream(ss)); }
    };
}

#endif
