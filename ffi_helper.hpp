#ifndef ATL_HELPERS_FFI_HPP
#define ATL_HELPERS_FFI_HPP
/**
 * @file /home/ryan/programming/atl/interface_helpers.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jul 04, 2015
 *
 * Some helpers reducing the boiler-plate of function definitions.
 */

#include "./ffi.hpp"
#include "./lexical_environment.hpp"

namespace atl
{
	struct PrimitiveDefiner
    {
	    GC& gc;
	    SymbolMap& env;

	    PrimitiveDefiner(GC& gc_, SymbolMap& env_)
		    : gc(gc_), env(env_)
	    {}

	    void macro(const std::string& name, CxxMacro::Fn fn)
        { env.define(name, wrap(new CxxMacro(fn, name))); }

	    template<class Sig>
	    void function(std::string const& name,
	                  typename signature::StdFunction<Sig>::type const& fn) {
            env.define(name,
                       signature::Wrapper<Sig>::type::a(fn, gc, name).any);
        }
	};
}

#endif
