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

    namespace cxx_functions {
        namespace convert_value {
            template<class T>
            struct PassThrough { static T a(T input) { return input; } };

            template<class T>
            struct Convert
                : std::conditional<is_atl_type<T>::value,
                                   unwrapping::Any<T>,
                                   get_value::GetValue<T,Any>
                                   >::type {};
        }

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


	struct Metadata {
	    template<class R, class ... Sig>
	    struct apply {
		typedef std::array<Any, sizeof...(Sig) + 1> TypeArray; // args + the return value

		static constexpr TypeArray parameter_types() {
		    return  TypeArray{{ nil< typename type_mapping::cxx_to_atl<Sig>::type
					     >::value()...,
				nil<typename type_mapping::cxx_to_atl<R>::type >::value()}};
		}};
	};


        template<class Fn, Fn fn>
	struct _WrapFnPntr {

	    template<class R, class ... Arguments>
	    struct apply {

                template <std::size_t... Index>
                static void call_packed(PCode::iterator begin,
                                       Indexer<Index...>) {
                    using namespace byte_code;
                    *begin = to_bytes(fn(begin[Index]...));
                }

                static void shimmed(PCode::iterator begin, PCode::iterator end) {
                    call_packed(begin, BuildIndicies<sizeof...(Arguments)>{} );
                }

                constexpr static CxxFn impl = CxxFn(&shimmed, sizeof...(Arguments));

                static Any any() {
                    return Any(tag<CxxFn>::value,
                               const_cast<void*>(reinterpret_cast<void const*>(&impl))); }
            };

	};

        template<class Fn, Fn fn>
        template<class R, class ... Arguments>
        constexpr CxxFn _WrapFnPntr<Fn, fn>::apply<R,Arguments...>::impl;

	template<class Fn, Fn fn>
	struct WrapFnPntr :
	    public unpack_fn::Pointer<Metadata, Fn>,
	    public unpack_fn::Pointer<_WrapFnPntr<Fn, fn>, Fn>
        {};


        // wraps a c++ std::function
	template<class Sig> struct WrapStdFunction {};

	template<class R, class ... Sig>
	class WrapStdFunction<R (Sig ...)> :
	    public Metadata::template apply<R, Sig...> {
	public:
	    static const size_t arity = sizeof...(Sig);

	private:
	    template <std::size_t... Index>
	    static Any call_packed(std::function<R (Sig...)> fn
				   , const Any *begin
				   , Indexer<Index...>) {
		return
		    wrap(fn(convert_value::Convert<Sig>::a(begin[Index])...));
	    }
	public:

	    /**
	     * using the 'a' for 'apply' convention, builds the wrapped version of fn
	     * @return: wrapped function
	     */
	    static PrimitiveRecursive* a(const std::function<R (Sig...)>& fn, GC *gc
					 , const std::string& name = "#<PrimitiveRecursive>") {
		static const auto param_t = Metadata::template apply<R,Sig...>::parameter_types();
		return gc->make<PrimitiveRecursive>
		    (
		     [fn](const Any *vv, const Any *_) -> Any {
			 return call_packed(fn, vv, BuildIndicies<arity> {});
		     }
		     , name
		     , CxxArray(&(*param_t.begin()), &(*param_t.end()))
		     );
	    }};
    }


    template<class Fn, Fn fn>
    using WrapFnPntr = cxx_functions::WrapFnPntr<Fn, fn>;

    template<class Sig>
    using WrapStdFunction = cxx_functions::WrapStdFunction<Sig>;
}

#endif
