#ifndef ATL_PRIMITIVE_CALLABLE_HH
#define ATL_PRIMITIVE_CALLABLE_HH
/**
 * @file /home/ryan/programming/atl/primitive_callable.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Dec 12, 2013
 *
 * Playing around with integrating type info with the function primitives.
 */
#include <fstream>

#include "./type.hpp"
#include "./ffi_helper.hpp"

namespace atl
{
	void export_primitives(Atl& atl)
	{
		using namespace signature;
		PrimitiveDefiner definer(atl.gc, atl.lexical);

		setup_basic_definitions(atl.gc, atl.lexical);

		auto atl_pntr = &atl;

		// for debugging (I still need a more general printing
		// mechanism)
		definer.function<Pack<long (long)>>
			("print-int",
			 [atl_pntr](long a) -> long
			 {
				 (*atl_pntr->stdout) << a << std::endl;
				 return a;
			 });

		/***********************************************************/
		/**     _         _ _   _                     _   _       **/
		/**    / \   _ __(_) |_| |__  _ __ ___   __ _| |_(_) ___  **/
		/**   / _ \ | '__| | __| '_ \| '_ ` _ \ / _` | __| |/ __| **/
		/**  / ___ \| |  | | |_| | | | | | | | | (_| | |_| | (__  **/
		/** /_/   \_\_|  |_|\__|_| |_|_| |_| |_|\__,_|\__|_|\___| **/
		/***********************************************************/
		definer.function<Pack<long (long, long)> >("add2", [](long a, long b) { return a + b;});
		definer.function<Pack<long (long, long)> >("sub2", [](long a, long b) { return a - b;});
		definer.function<Pack<bool (long, long)> >("=", [](long a, long b) { return a = b;});
		definer.function<Pack<bool (long, long)> >("<", [](long a, long b) { return a < b;});
		definer.function<Pack<bool (long, long)> >(">", [](long a, long b) { return a > b;});
		definer.function<Pack<bool (long, long)> >("<=", [](long a, long b) { return a <= b;});
		definer.function<Pack<bool (long, long)> >(">=", [](long a, long b) { return a >= b;});
	}
}

#endif

