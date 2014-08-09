#ifndef ATL_FFI_HH
#define ATL_FFI_HH
/**
 * @file /home/ryan/programming/atl/ffi_2.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Dec 29, 2013
 */

#include <utility>
#include <type_traits>
#include <boost/mpl/vector.hpp>
#include <boost/mpl/push_back.hpp>
#include <boost/mpl/at.hpp>

#include "./type.hpp"
#include "./type_traits.hpp"
#include "./conversion.hpp"
#include "./gc.hpp"

namespace mpl = boost::mpl;
namespace atl {
    /* http://loungecpp.wikidot.com/tips-and-tricks%3Aindices
     * tricky tricky, never gets instantiated, but can get filled out by
     * the template inference engine.
     */
    template<std::size_t ...Is> struct Indexer {};

    template <std::size_t N, std::size_t... Is> struct
    BuildIndicies : BuildIndicies<N-1, N-1, Is...> {};

    template <std::size_t... Is> struct
    BuildIndicies<0, Is...> : Indexer<Is...> {};

    template<class Sig> struct WrapFn {};

    /**
     * Wraps a C++ function to unpack the argument list and perform type conversion
     *
     * @tparam R: the return type of the function I'm wrapping
     * @tparam Sig: the argument signature of the function I am wrapping
     */
    template<class R, class ... Sig>
    class WrapFn<R (Sig ...)> {
    public:
	static const size_t arity = sizeof...(Sig);

    private:
	template <std::size_t... Index>
	static Any call_packed(std::function<R (Sig...)> fn
			       , const Any *begin
			       , Indexer<Index...>) {
	    return
		wrap(fn(unwrap<Sig>(begin[Index])...));
	}

	typedef std::array<Any, arity + 1> TypeArray;
	static TypeArray parameter_types() {
	    return  TypeArray{{ nil< typename type_mapping::cxx_to_atl<Sig>::type
				     >::value()...,
			nil<typename type_mapping::cxx_to_atl<R>::type >::value()}};
	}
    public:

	/**
	 * using the 'a' for 'apply' convention, builds the wrapped version of fn
	 * @return: wrapped function
	 */
	static PrimitiveRecursive* a(const std::function<R (Sig...)>&& fn, GC *gc
				     , const char* name = "#<PrimitiveRecursive>") {
	    static const auto param_t = parameter_types();
	    return gc->make<PrimitiveRecursive*>
		(
		 [fn](const Any *vv, const Any *_) -> Any {
		     return call_packed(fn, vv, BuildIndicies<arity> {});
		 }
		 , name
		 , CxxArray(&(*param_t.begin()), &(*param_t.end()))
		 );
	}};
}

#endif
