#ifndef ATL_FFI_HPP
#define ATL_FFI_HPP
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
#include "./helpers.hpp"
#include "./utility.hpp"

namespace atl {
    namespace mpl = boost::mpl;

    namespace cxx_functions
    {
	namespace unpack_fn {
	    template<class Dest, class R, class ... Sig> struct Signature;
	    template<class Dest, class R, class ... Sig>
	    struct Signature<Dest, R (Sig...)>
		: public Dest::template apply<R, Sig...> {};

	    template<class Dest, class R, class ... Sig> struct Pointer;
	    template<class Dest, class R, class ... Sig>
	    struct Pointer<Dest, R (*)(Sig...)>
		: public Dest::template apply<R, Sig...> {};
	}

        // Assume a pointer is to a known Atl type (or at least it's
        // value part), and use that type to annotate the C++ function
        template<class T>
        struct GuessTag
            : public ApplyEval<WTag,
                               ApplyEval<type_mapping::cxx_to_atl,
                                         std::remove_pointer<T>
                                         > >::type
        {};

	struct Metadata
        {
	    template<class R, class ... Sig>
	    struct apply
            {
                static constexpr size_t arity()
                { return sizeof...(Sig); }

                template<class Alloc>
                static abstract_type::Type* parameter_types(Alloc& gc)
                {
                    return gc.template make<abstract_type::Type>(abstract_type::make_concrete({GuessTag<Sig>::value...,
                                    GuessTag<R>::value}));
                }
            };
	};


        // wraps a c++ std::function
	template<class Sig> struct WrapStdFunction {};

	template<class R, class ... Sig>
	class WrapStdFunction<R (Sig ...)> :
	    public Metadata::template apply<R, Sig...>
        {
	private:
	    template <std::size_t... Index>
	    static void call_packed(std::function<R (Sig...)> fn
                                    , PCode::iterator begin
                                    , Indexer<Index...>)
            {
                using namespace byte_code;
                *begin = to_bytes(atl::value<R>(fn(from_bytes<Sig>(begin[Index])...)));
	    }
	public:

	    /**
	     * using the 'a' for 'apply' convention, builds the wrapped version of fn
	     * @return: wrapped function
	     */
            template<class Alloc>
	    static CxxFunctor* a(std::function<R (Sig...)> const& fn, Alloc &gc
                                 , std::string const & name = "#<Unnamed-CxxFunctor>")
            {
		return gc.template make<CxxFunctor>
                    ([fn](PCode::iterator vv, PCode::iterator _)
                     {
			 return call_packed(fn, vv, BuildIndicies<WrapStdFunction::arity()> {});
		     }
		     , name
                     , WrapStdFunction::parameter_types(gc) );
	    }

            template<class Alloc>
            static Any any(std::function<R (Sig...)> const& fn, Alloc &gc,
                           std::string const& name = "#<Unnamed-CxxFunctor>")
            {
                return wrap(a(fn, gc, name));
            }
        };
    }


    template<class Sig>
    using WrapStdFunction = cxx_functions::WrapStdFunction<Sig>;
}

#endif
