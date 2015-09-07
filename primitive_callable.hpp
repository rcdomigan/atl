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

	Any nullP(Any a) { return is<Null>(a) ? atl_true() : atl_false();  }

	void export_recursive_defs(Environment& env)
	{
		using namespace primitives;

		env.lexical.define("\\", wrap<Lambda>());
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


		auto alloc_ast_type = new abstract_type::Type(abstract_type::make_concrete({tag<Ast>::value}));
		env.lexical.define
			("__alloc_ast__",
			 wrap
			 (env.gc.make<CxxFunctor>
			  ([&](vm_stack::iterator begin, vm_stack::iterator end) -> void
			  {
				  auto range = make_range(begin, end);
				  auto stack = env.gc.dynamic_seq(range.size() + 2);
				  auto seq = stack->end();
				  stack->push_back(Any(tag<AstData>::value,
				                       0));

				  for(auto pair = begin; pair < end; pair += 2)
					  stack->push_back(Any(pair[1],
					                       reinterpret_cast<void*>(pair[0])));

				  seq->value = stack->end();
				  *begin = reinterpret_cast<vm_stack::value_type>(seq);
			  },
				  "__alloc_ast__",
				  alloc_ast_type)));

		auto cc = primitives::Constructor(env.lexical);
		mpl::for_each<TypesVec, wrap_t_arg< mpl::placeholders::_1> >(cc);

		wrap_function<Any* (Any*, long)>
			(env.lexical,
			 "nth",
			 [](Any *ast, long n) -> Any*
			{ return &unwrap<Ast>(*ast)[n]; });

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
			(env.lexical,

		auto cons_ast = WrapStdFunction<AstData* (vm_stack::value_type, vm_stack::value_type, Ast*)>::a
			([&env](vm_stack::value_type value, vm_stack::value_type type, Ast* ast) -> AstData*
			 {
				 using namespace ast_iterator;

				 auto output = env.gc.dynamic_seq();
				 auto vec = output->end();
				 output->push_back(Any(tag<AstData>::value, nullptr));

				 output->push_back(Any(type,
				                       reinterpret_cast<void*>(value)));
				 std::copy(ast->begin(), ast->end(),
				           std::back_insert_iterator<memory_pool::DynamicVector>(*output));

				 vec->value = output->end();
				 return reinterpret_cast<AstData*>(vec);
			 }
			 , env.gc
			 , "cons-ast");

		wrap_macro
			(env.lexical,
			 "cons",
			 [&, cons_ast](Eval& eval, PrimitiveMacro::Input const& ast) -> tag_t
			 {
				 // push car's (value type)
				 eval.compile->push_value(eval.compile->any(ast[0]));
				 auto seq_type = eval.compile->any(ast[1]);

				 switch(seq_type)
					 {
					 case tag<Pointer>::value:
						 eval.compile->code.std_function(&cons_ast->fn, 3);
						 return tag<Ast>::value;
					 default:
						 throw WrongTypeError
							 (std::string("cons not defined for ").append(type_name(seq_type)));
					 }
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
