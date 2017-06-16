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

	namespace byte_code
	{
		typedef typename vm_stack::value_type value_type;
		template<class T>
		vm_stack::value_type to_bytes(T input)
		{ return reinterpret_cast<vm_stack::value_type>(input); }

		// TODO: use the `std::is_integral` and static cast for all integral (and floating?) types.
		value_type to_bytes(long input)
		{ return static_cast<value_type>(input); }

		value_type to_bytes(bool input)
		{ return static_cast<value_type>(input); }

		value_type to_bytes(void* input)
		{ return reinterpret_cast<value_type>(input); }

		value_type to_bytes(Pointer input)
		{ return reinterpret_cast<value_type>(input.value); }

		template<class R>
		struct PntrCaster
		{
			typedef PntrCaster<R> type;
			static R a(value_type input)
			{ return reinterpret_cast<R>(input); }
		};

		template<class I>
		struct StaticCaster
		{
			typedef StaticCaster<I> type;
			static I a(value_type input)
			{ return static_cast<I>(input); }
		};


		template<class T>
		struct Caster
			: public std::conditional<std::is_integral<T>::value,
			                          StaticCaster<T>,
			                          PntrCaster<T>
			                          >::type
		{};

		template<class R>
		R from_bytes(value_type input) { return Caster<R>::a(input); }
	}

	namespace cxx_functions
	{
		using namespace tmpl;

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
						 return call_packed(fn, vv,
						                    typename tmpl::BuildIndicies<WrapStdFunction::arity()>::type {});
					 }
					 , name
					 , WrapStdFunction::parameter_types(gc)
					 , WrapStdFunction::arity());
			}
		};
	}

	template<class Sig>
	using WrapStdFunction = cxx_functions::WrapStdFunction<Sig>;

	namespace signature
	{
		template<class R, class ... Sig>
		struct Pack;

		template<class R, class ... Sig>
		struct Pack<R (Sig...)>
		{
			typedef Pack<R (Sig...)> type;

			typedef R Return;
			typedef std::tuple<Sig...> Args;

			static constexpr std::size_t arity = sizeof...(Sig);
		};

		template<class R, class Fn, class Args, class Indexes>
		struct _Unpack;

		template<class R, class Target, class Args, std::size_t... Index>
		struct _Unpack<R, Target, Args, tmpl::Indexer<Index...> >
		{ typedef typename Target::template type<R, typename std::tuple_element<Index, Args>::type...> type; };

		// Unpack Pack to instantiate Target
		template<class Pack, class Target>
		struct Unpack
			: public _Unpack<typename Pack::Return,
			                 Target,
			                 typename Pack::Args,
			                 typename tmpl::BuildIndicies<Pack::arity>::type >
		{};

		struct _StdFunction
		{
			template<class R, class ... Args>
			using type = std::function<R (Args...)>;
		};

		template<class Pack>
		struct StdFunction
			: public Unpack<Pack, _StdFunction>
		{};

		struct _Wrapper
		{
			template<class R, class ... Args>
			using type = cxx_functions::WrapStdFunction<R (Args...)>;
		};

		template<class Pack>
		struct Wrapper
			: public Unpack<Pack, _Wrapper>
		{};
	}
}

#endif
