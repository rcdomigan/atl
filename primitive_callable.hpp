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
	void export_recursive_defs(Environment& env)
	{
		using namespace primitives;

		setup_basic_definitions(env.lexical);

		// for debugging (I still need a more general printing
		// mechanism)
		wrap_function<long (long)>
			(env.lexical,
			 env.gc,
			 "print-int",
			 [&env](long a)
			 {
				 (*env.stdout) << a << std::endl;
				 return a;
			 });

		/***********************************************************/
		/**     _         _ _   _                     _   _       **/
		/**    / \   _ __(_) |_| |__  _ __ ___   __ _| |_(_) ___  **/
		/**   / _ \ | '__| | __| '_ \| '_ ` _ \ / _` | __| |/ __| **/
		/**  / ___ \| |  | | |_| | | | | | | | | (_| | |_| | (__  **/
		/** /_/   \_\_|  |_|\__|_| |_|_| |_| |_|\__,_|\__|_|\___| **/
		/***********************************************************/
		wrap_function<long (long, long)>(env.lexical, env.gc, "add2", [](long a, long b) { return a + b;});
		wrap_function<long (long, long)>(env.lexical, env.gc, "sub2", [](long a, long b) { return a - b;});
		wrap_function<bool (long, long)>(env.lexical, env.gc, "=", [](long a, long b) { return a = b;});
		wrap_function<bool (long, long)>(env.lexical, env.gc, "<", [](long a, long b) { return a < b;});
		wrap_function<bool (long, long)>(env.lexical, env.gc, ">", [](long a, long b) { return a > b;});
		wrap_function<bool (long, long)>(env.lexical, env.gc, "<=", [](long a, long b) { return a <= b;});
		wrap_function<bool (long, long)>(env.lexical, env.gc, ">=", [](long a, long b) { return a >= b;});

		/////////////////////////////////////////////////////////////////////
		//  ___       _                                 _   _              //
		// |_ _|_ __ | |_ _ __ ___  ___ _ __   ___  ___| |_(_) ___  _ __   //
		//  | || '_ \| __| '__/ _ \/ __| '_ \ / _ \/ __| __| |/ _ \| '_ \  //
		//  | || | | | |_| | | (_) \__ \ |_) |  __/ (__| |_| | (_) | | | | //
		// |___|_| |_|\__|_|  \___/|___/ .__/ \___|\___|\__|_|\___/|_| |_| //
		//                             |_|                                 //
		/////////////////////////////////////////////////////////////////////
		// introspection

		wrap_function<long ()>(env.lexical,
		                       env.gc,
		                       "print-bytecode",
		                       [&env]() {
			                       if(env.pcode)
				                       env.pcode->output->dbg();
			                       else
				                       cout << "environment pcode is not set." << endl;
			                       return 0;
		                       });
	}


	static bool _setup_interpreter = false;
	void setup_interpreter(Environment &env)
	{
		if(!_setup_interpreter)
			{
				_setup_interpreter = true;
				init_types();
			}
		export_recursive_defs(env);
	}
}

#endif
