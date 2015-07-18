#ifndef ATL_INTERFACE_HELPERS_HPP
#define ATL_INTERFACE_HELPERS_HPP
/**
 * @file /home/ryan/programming/atl/interface_helpers.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jul 04, 2015
 */

#include "./helpers.hpp"
#include "./ffi.hpp"

namespace atl
{
    namespace primitives
    {
	    void wrap_macro(Environment& env, const std::string& name, CxxFunctor::Fn fn)
        {
            env.define(name, wrap(new PrimitiveMacro(fn, name)));
        }

        template<class T>
        void wrap_function(Environment& env, std::string const& name, std::function<T> fn) {
            env.define(name,
                       wrap(WrapStdFunction<T>::a(fn, env.gc, name)));
        }

        long print(Any a) {
            cout << printer::any(a);
            return 0;
        }

        struct Constructor
        {
            Environment& env;
            Constructor(Environment& ee) : env(ee) {}

            // My primitives aren't GCed so I think I'm OK allocating
            // theirabstract_type::Type object in the regular heap.
            template<class T>
            void operator()(wrap_t_arg<T>)
            {
                auto type = new abstract_type::Type(abstract_type::make_concrete({tag<T>::value}));
                env.define(type_name(tag<T>::value),
                           wrap(Type(type)));
            }
        };
    }
}

#endif
