#ifndef ATL_TYPE_TRAITS_HPP
#define ATL_TYPE_TRAITS_HPP
/**
 * @file /home/ryan/programming/atl/type_traits.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Aug 17, 2014
 */

#include "./type.hpp"

namespace atl
{
	namespace type_mapping
	{
		template<class T>
		using Identity = tmp::Identity<T>;

		template<class T>
		struct atl_builtin
		{
			static_assert(atl::is_atl_type<T>::value, "No ATL type equivalent");
			typedef T type;
		};


		template<class Input>
		struct cxx_to_atl
			: std::conditional< std::is_integral<Input>::value,
			                    Identity<Fixnum>,
			                    atl_builtin<Input>
			                    >::type
		{};

		template<> struct cxx_to_atl<bool> : public Identity<Bool> {};
		template<> struct cxx_to_atl<int> : public Identity<Fixnum> {};
		template<> struct cxx_to_atl<std::string> : public Identity<String> {};


		template<class T> struct unwrappable_type : public is_atl_type<T> {};
		template<> struct unwrappable_type<std::string> : std::true_type {};
	}

	
}

#endif
