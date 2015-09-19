#ifndef ATL_CONVERSION_HH
#define ATL_CONVERSION_HH
/**
 * @file /home/ryan/programming/atl/conversion.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jul 10, 2013
 */

#include <string>
#include "./type.hpp"
#include "./type_traits.hpp"
#include "./utility.hpp"

#include <boost/mpl/at.hpp>
#include <boost/mpl/map.hpp>


namespace atl {
	namespace unwrapping {
		template<class T>
		struct Pimpl {
			static_assert(type_mapping::unwrappable_type<T>::value, "Cannot unwrap Any to type T");
			static inline constexpr T& a(atl::Any& aa) {return *reinterpret_cast<T*>(aa.value);}
			static inline constexpr T const& a(atl::Any const& aa) {return *reinterpret_cast<T*>(aa.value);}};

		template<class T>
		struct Reinterpret {
			static_assert(type_mapping::unwrappable_type<T>::value, "Cannot unwrap Any to type T");
			static inline constexpr T& a(atl::Any& aa) {return reinterpret_cast<T&>(aa);}
			static inline constexpr T const& a(atl::Any const& aa) {return reinterpret_cast<T const&>(aa);}};

		template<class T>
		struct Any
			: public std::conditional< is_reinterperable<typename std::decay<T>::type>::value,
			                           Reinterpret<T>,
			                           Pimpl<T>
			                           >::type
		{};
	}

	template<class T>
	static inline T& unwrap(Any& input) { return unwrapping::Any<T>::a(input); }

	template<class T>
	static inline T const& unwrap(Any const& input) { return unwrapping::Any<T>::a(input); }

	// template<class T>
	// static inline T const& unwrap(void const* input)
	// { return unwrapping::Any<T>::a(*reinterpret_cast<Any const*>(input)); }


	/*********************************/
	/* __        __                  */
	/* \ \      / / __ __ _ _ __     */
	/*  \ \ /\ / / '__/ _` | '_ \    */
	/*   \ V  V /| | | (_| | |_) |   */
	/*    \_/\_/ |_|  \__,_| .__/    */
	/*                     |_|       */
	/*********************************/
	namespace wrapping
	{
		template<class T>
		struct Reinterpret
		{
			static_assert(atl::is_reinterperable<T>::value, "Cannot wrap T");

			static inline constexpr Any a(T const& aa)
			{ return Any(aa._tag, (void*)aa.value); }
		};

		template<class T>
		struct Pimpl
		{
			typedef typename std::remove_pointer<T>::type Bare;
			static_assert(type_mapping::unwrappable_type<Bare>::value,
			              "Cannot wrap type T");

			static inline constexpr Any a(T aa)
			{ return Any(tag<Bare>::value, aa); }
		};

		template<class T>
		struct Any
			: std::conditional< is_reinterperable<T>::value,
			                    tmpl::Identity< Reinterpret<T> >,
			                    Pimpl<T>
			                    >::type::type
		{};
	}

	template<class T> static inline constexpr Any wrap(T* vv)
	{
		static_assert(is_pimpl<T>::value,
		              "Cannot wrap pointer to non-pimpl: wrapped the dereferenced value "
		              "or use some other mechanism.");
		return Any(tag<T>::value, vv);
	}

	template<class T>
	static inline constexpr Any wrap(T const& vv)
	{
		using namespace tmpl;
		return Apply<wrapping::Any,
		             Apply<type_mapping::cxx_to_atl,
		                   Identity<T> >
		             >::a(vv);
	}


	static inline constexpr Any wrap(Any* vv) { return Any(tag<Pointer>::value, vv); }
	static inline constexpr Any wrap(Any const& vv) { return vv; }


	static inline Any wrap(const Fixnum& vv) {
		return Any(tag<Fixnum>::value,
		           (void*)vv.value);
	}


	template<class T>
	static inline Any wrap() { return Any(tag<T>::value, nullptr); }


	template<class T>
	Pointer make_pointer(T* value)
	{
		static_assert(is_atl_type<T>::value,
		              "Will only wrap non-pimpl types with Pointer.  "
		              "A pimpl type may first be wrapped in an Any.");
		return Pointer(reinterpret_cast<Any*>(value));
	}

	namespace get_value
	{
		using namespace tmpl;

		// 'mpl::at' with a default (in other words, the interface the
		// boost docs advertise).
		template <typename Seq, typename Key, typename Def>
		struct AtDef
			: std::conditional<mpl::has_key<Seq, Key>::value,
			                   mpl::at<Seq, Key>,
			                   mpl::identity<Def>
			                   >::type {};


		typedef mpl::map<mpl::pair<bool, Bool>
		                 , mpl::pair<long, Fixnum>
		                 > CxxToAtlType;

		// const in c++ sucks.
		template<class T, class Input>
		struct GetConstValue
		{
			typedef typename Apply<AtDef,
			                       CxxToAtlType,
			                       Apply<std::remove_reference,
			                             std::remove_pointer<T> >,
			                       Identity<T>
			                       >::type AtlType;

			typedef decltype(AtlType::value) const& Return;

			static Return a(Input input) { return unwrap<AtlType>(input).value; }
		};


		template<class T, class Input>
		struct GetRefValue
		{
			typedef typename AtDef<CxxToAtlType, T, T>::type AtlType;

			typedef decltype(AtlType::value)& Return;

			static Return a(Input input) { return unwrap<AtlType>(input).value; }
		};

		// is the object referenced or pointed to const?
		template<class T>
		struct IsConstObject
			: Apply<std::is_const, Apply<std::remove_pointer, std::remove_reference<T> >
			        >::type {};

		template<class T, class Input>
		struct FromAtl
			: std::conditional< IsConstObject<T>::value || IsConstObject<Input>::value,
			                    GetConstValue<T, Input>,
			                    GetRefValue<T, Input>
			                    >::type
		{};

		template<class T>
		struct FromAtl<T, T>
		{
			typedef T AtlType;
			typedef T Return;

			static T& a(T& input) { return input; }
			static T a(T const& input) { return input; }
		};
	}

	template<class T, class Input>
	typename get_value::FromAtl<T, Input>::Return
	value(Input&& input) {
		return get_value::FromAtl<T, Input >::a(input);
	}
}

#endif
