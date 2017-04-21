#ifndef ATL_FFI_HPP
#define ATL_FFI_HPP
/**
 * @file /home/ryan/programming/atl/ffi_2.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Dec 29, 2013
 */

#include <array>                           // for tuple_element
#include <boost/mpl/aux_/adl_barrier.hpp>  // for mpl
#include <cstddef>                         // for size_t
#include <functional>                      // for function
#include <string>                          // for string
#include <tuple>                           // for tuple
#include <type_traits>                     // for remove_pointer

#include "./helpers/make_ast.hpp"          // for fn
#include "./type.hpp"                      // for iterator, Ast, tag, value_...
#include "./type_traits.hpp"               // for cxx_to_atl
#include "./utility.hpp"                   // for Apply, BuildIndicies, Indexer
#include "gc/marked.hpp"                   // for Marked
#include "helpers/misc.hpp"                // for from_bytes, to_bytes
#include "wrap.hpp"                        // for value


namespace atl {
	namespace mpl = boost::mpl;

	namespace cxx_functions
	{
		using namespace tmpl;

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

		template<class T>
		struct GuessTag
			: public Apply<tag, Apply<type_mapping::cxx_to_atl,
			                          std::remove_pointer<T> >
			               >::type
		{};

		struct Metadata
		{
			template<class R, class ... Rest>
			struct apply
			{
				static constexpr size_t arity()
				{ return sizeof...(Rest); }


				template<class Alloc>
				static Ast parameter_types(Alloc& gc)
				{ return *gc(fn_type::fn(GuessTag<Rest>::value..., GuessTag<R>::value)); }
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
			                        , vm_stack::iterator begin
			                        , tmpl::Indexer<Index...>)
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
			static Marked<CxxFunctor> a(std::function<R (Sig...)> const& fn, Alloc &gc
			                     , std::string const & name = "#<Unnamed-CxxFunctor>")
			{
				return gc.template make<CxxFunctor>
					([fn](vm_stack::iterator vv, vm_stack::iterator _)
					 {
						 return call_packed(fn, vv, tmpl::BuildIndicies<WrapStdFunction::arity()> {});
					 }
					 , name
					 , WrapStdFunction::parameter_types(gc)
					 , WrapStdFunction::arity());
			}
		};
	}


	template<class Sig>
	using WrapStdFunction = cxx_functions::WrapStdFunction<Sig>;
}

#endif
