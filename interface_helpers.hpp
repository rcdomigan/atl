#ifndef ATL_INTERFACE_HELPERS_HPP
#define ATL_INTERFACE_HELPERS_HPP
/**
 * @file /home/ryan/programming/atl/interface_helpers.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jul 04, 2015
 */

#include "./helpers.hpp"
#include "./lexical_environment.hpp"
#include "./ffi.hpp"

namespace atl
{
    namespace primitives
    {
	    void wrap_macro(LexicalEnvironment& env, const std::string& name, CxxMacro::Fn fn)
        {
            env.define(name, wrap(new CxxMacro(fn, name)));
        }

        template<class T>
        void wrap_function(LexicalEnvironment& env, std::string const& name, std::function<T> fn) {
            env.define(name,
                       wrap(WrapStdFunction<T>::a(fn, env.gc, name)));
        }

        long print(Any a) {
            cout << printer::any(a);
            return 0;
        }
    }
}

#endif
