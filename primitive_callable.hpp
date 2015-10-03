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
#include "./environment.hpp"
#include "./interface_helpers.hpp"

#include "ffi.hpp"

namespace atl
{
	void export_recursive_defs(Environment& env)
	{
		using namespace primitives;

		env.lexical.define("__\\__", wrap<Lambda>());
		env.lexical.define(":", wrap<DeclareType>());
		env.lexical.define("quote", wrap<Quote>());
		env.lexical.define("if", wrap<If>());
		env.lexical.define("#f", atl_false());
		env.lexical.define("#t", atl_true());
		env.lexical.define("define-value", wrap<Define>());

		// for debugging (I still need a more general printing
		// mechanism)
		wrap_function<long (long)>
			(env.lexical,
			 "print-int",
			 [&env](long a)
			 {
				 (*env.stdout) << a << std::endl;
				 return a;
			 });


		auto cc = primitives::Constructor(env.lexical);
		mpl::for_each<TypesVec, wrap_t_arg< mpl::placeholders::_1> >(cc);

		wrap_function<Any* (AstData*, long)>
			(env.lexical,
			 "nth",
			 [](AstData *ast, long n) -> Any*
			{ return &Ast(ast)[n]; });

		static auto fn_construct_params = abstract_type::make_concrete({tag<Type>::value, tag<Type>::value});
		fn_construct_params.front().count = abstract_type::Node::CountType::at_least_one;
		auto fn_construct
			= env.gc.make<CxxFunctor>([&env](vm_stack::iterator begin, vm_stack::iterator end)
			                      {
				                      auto type = env.gc.make<abstract_type::Type>();
				                      for(auto& vv : make_range(begin, end))
					                      type->values.emplace_back(*reinterpret_cast<Type::value_type*>(vv));
				                      *begin = reinterpret_cast<vm_stack::value_type>(type);
			                      }, "->", &fn_construct_params);

		env.lexical.define("->", wrap(fn_construct));


		/***********************************************************/
		/**     _         _ _   _                     _   _       **/
		/**    / \   _ __(_) |_| |__  _ __ ___   __ _| |_(_) ___  **/
		/**   / _ \ | '__| | __| '_ \| '_ ` _ \ / _` | __| |/ __| **/
		/**  / ___ \| |  | | |_| | | | | | | | | (_| | |_| | (__  **/
		/** /_/   \_\_|  |_|\__|_| |_|_| |_| |_|\__,_|\__|_|\___| **/
		/***********************************************************/
		wrap_function<long (long, long)>(env.lexical, "add2", [](long a, long b) { return a + b;});
		wrap_function<long (long, long)>(env.lexical, "sub2", [](long a, long b) { return a - b;});
		wrap_function<bool (long, long)>(env.lexical, "=", [](long a, long b) { return a = b;});
		wrap_function<bool (long, long)>(env.lexical, "<", [](long a, long b) { return a < b;});
		wrap_function<bool (long, long)>(env.lexical, ">", [](long a, long b) { return a > b;});
		wrap_function<bool (long, long)>(env.lexical, "<=", [](long a, long b) { return a <= b;});
		wrap_function<bool (long, long)>(env.lexical, ">=", [](long a, long b) { return a >= b;});

		/////////////////////////////////////////////////////////////////////
		//  ___       _                                 _   _              //
		// |_ _|_ __ | |_ _ __ ___  ___ _ __   ___  ___| |_(_) ___  _ __   //
		//  | || '_ \| __| '__/ _ \/ __| '_ \ / _ \/ __| __| |/ _ \| '_ \  //
		//  | || | | | |_| | | (_) \__ \ |_) |  __/ (__| |_| | (_) | | | | //
		// |___|_| |_|\__|_|  \___/|___/ .__/ \___|\___|\__|_|\___/|_| |_| //
		//                             |_|                                 //
		/////////////////////////////////////////////////////////////////////
		// introspection

		wrap_function<long ()>(env.lexical, "print-bytecode",
		                       [&env]() {
			                       if(env.pcode)
				                       dbg_code(*env.pcode->output);
			                       else
				                       cout << "environment pcode is not set." << endl;
			                       return 0;
		                       });

		/***************************/
		/**  _     _     _        **/
		/** | |   (_)___| |_ ___  **/
		/** | |   | / __| __/ __| **/
		/** | |___| \__ \ |_\__ \ **/
		/** |_____|_|___/\__|___/ **/
		/***************************/
		wrap_function<AstData* (Any*, Ast*)>
			(env.lexical,
			 "cons-ast",
			 [&env](Any* car, Ast* ast)
			{
				auto& space = env.gc.sequence();
				auto output = push_nested_ast(space);

				space.push_back(*car);
				std::copy(ast->begin(), ast->end(),
				          std::back_inserter(space));

				output.end_ast();
				return output.ast_data();
			});

		wrap_macro
			(env.lexical,
			 "cons",
			 [&](CxxMacro::Input const& ast) -> Any
			 {
				 using namespace make_ast;
				 return wrap
					 (make
					  (sym("cons-ast"),
					   make(lift<Quote>(),
					        lift(ast[0])),
					   lift(ast[1]))
					  (ast_alloc(env.gc)));
			 });

		wrap_function<Slice* (Ast*, long)>(env.lexical, "slice",
		                                   [&env](Ast* ast, long nn) -> Slice*
			{
				return env.gc.make<Slice>(ast->begin() + nn, ast->end());
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
