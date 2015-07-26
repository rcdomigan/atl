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
#include "./parser.hpp"
#include "./helpers.hpp"
#include "./compile.hpp"
#include "./interface_helpers.hpp"

#include "ffi.hpp"

namespace atl
{
	Any nullP(Any a) { return is<Null>(a) ? atl_true() : atl_false();  }

	void export_recursive_defs(GC &gc, Environment &env, ParseString &parser)
	{
		using namespace primitives;

		env.define("\\", wrap<Lambda>());
		env.define("quote", wrap<Quote>());
		env.define("if", wrap<If>());
		env.define("#f", atl_false());
		env.define("#t", atl_true());
		env.define("define-value", wrap<Define>());

		auto alloc_ast_type = new abstract_type::Type(abstract_type::make_concrete({tag<Ast>::value}));
		env.define("__alloc_ast__",
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

		auto cc = primitives::Constructor(env);
		mpl::for_each<TypesVec, wrap_t_arg< mpl::placeholders::_1> >(cc);

		// (: A a) declares a to be of type A
		wrap_macro(env, ":",
		           [](Eval &eval, PrimitiveMacro::Input const& ast)
		           {
			           // requires a type-expr followed by an expr.
			           // The whole shooting match is given the
			           // Type.value as its type.
			           tag_t rval;
			           {
				           auto frame = eval.compile->save_excursion();
				           frame.enter_end();

				           // TODO: I this should actually evauate,
				           // and should return an abstract_type::Type
				           rval = eval.compile->any(ast[0]);
			           }
			           eval.compile->in_place_any(ast[1]);
			           return rval;
		           });

		static auto fn_construct_params = abstract_type::make_concrete({tag<Type>::value, tag<Type>::value});
		fn_construct_params.front().count = abstract_type::Node::CountType::at_least_one;
		auto fn_construct
			= gc.make<CxxFunctor>([&env](vm_stack::iterator begin, vm_stack::iterator end)
			                      {
				                      auto type = env.gc.make<abstract_type::Type>();
				                      for(auto& vv : make_range(begin, end))
					                      type->values.emplace_back(*reinterpret_cast<Type::value_type*>(vv));
				                      *begin = reinterpret_cast<vm_stack::value_type>(type);
			                      }, "->", &fn_construct_params);

		env.define("->", wrap(fn_construct));


		/***********************************************************/
		/**     _         _ _   _                     _   _       **/
		/**    / \   _ __(_) |_| |__  _ __ ___   __ _| |_(_) ___  **/
		/**   / _ \ | '__| | __| '_ \| '_ ` _ \ / _` | __| |/ __| **/
		/**  / ___ \| |  | | |_| | | | | | | | | (_| | |_| | (__  **/
		/** /_/   \_\_|  |_|\__|_| |_|_| |_| |_|\__,_|\__|_|\___| **/
		/***********************************************************/
		wrap_function<long (long, long)>(env, "add2", [](long a, long b) { return a + b;});
		wrap_function<long (long, long)>(env, "sub2", [](long a, long b) { return a - b;});

		wrap_function<bool (long, long)>(env, "=", [](long a, long b) { return a = b;});
		wrap_function<bool (long, long)>(env, "<", [](long a, long b) { return a < b;});
		wrap_function<bool (long, long)>(env, ">", [](long a, long b) { return a > b;});
		wrap_function<bool (long, long)>(env, "<=", [](long a, long b) { return a <= b;});
		wrap_function<bool (long, long)>(env, ">=", [](long a, long b) { return a >= b;});

		/////////////////////////////////////////////////////////////////////
		//  ___       _                                 _   _              //
		// |_ _|_ __ | |_ _ __ ___  ___ _ __   ___  ___| |_(_) ___  _ __   //
		//  | || '_ \| __| '__/ _ \/ __| '_ \ / _ \/ __| __| |/ _ \| '_ \  //
		//  | || | | | |_| | | (_) \__ \ |_) |  __/ (__| |_| | (_) | | | | //
		// |___|_| |_|\__|_|  \___/|___/ .__/ \___|\___|\__|_|\___/|_| |_| //
		//                             |_|                                 //
		/////////////////////////////////////////////////////////////////////
		// introspection

		wrap_function<long ()>(env, "print-bytecode",
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
		wrap_macro
			(env,
			 "list",
			 [&](Eval& eval, PrimitiveMacro::Input const& ast) -> tag_t
			 {
				 for(auto& vv : ast)
					 eval.compile->push_value(eval.compile->in_place_any(vv));

				 eval.compile->wrapped.std_function(&unwrap<CxxFunctor>(env.toplevel.value("__alloc_ast__")).fn,
				                                    ast.size() * 2);

				 return tag<Ast>::value;
			 });


		auto cons_ast = WrapStdFunction<AstData* (vm_stack::value_type, vm_stack::value_type, Any*)>::a
			([&gc](vm_stack::value_type value, vm_stack::value_type type, Any *raw) -> AstData*
			 {
				 using namespace ast_iterator;
				 Ast seq(raw);

				 auto output = gc.dynamic_seq();
				 auto vec = output->end();
				 output->push_back(Any(tag<AstData>::value, nullptr));

				 output->push_back(Any(type,
				                       reinterpret_cast<void*>(value)));
				 std::copy(seq.begin(), seq.end(),
				           std::back_insert_iterator<GC::DynamicVector>(*output));

				 vec->value = output->end();
				 return reinterpret_cast<AstData*>(vec);
			 }
			 , env.gc
			 , "cons-ast");

		wrap_macro
			(env,
			 "cons",
			 [&, cons_ast](Eval& eval, PrimitiveMacro::Input const& ast) -> tag_t
			 {
				 // push car's (value type)
				 eval.compile->push_value(eval.compile->in_place_any(ast[0]));
				 auto seq_type = eval.compile->in_place_any(ast[1]);

				 switch(seq_type)
					 {
					 case tag<Ast>::value:
						 eval.compile->wrapped.std_function(&cons_ast->fn, 3);
						 return tag<Ast>::value;
					 default:
						 throw WrongTypeError
							 (std::string("cons not defined for ").append(type_name(seq_type)));
					 }
			 });

		wrap_function<Slice* (Any*, long)>(env, "slice",
		                                   [&gc](Any* raw, long nn) -> Slice*
			{
				Ast ast(raw);
				auto out = gc.make<Slice>(ast.begin() + nn, ast.end());
				return out;
			});

		///////////////////////////////////////////
		//  __  __                       _       //
		// |  \/  | ___  _ __   __ _  __| |___   //
		// | |\/| |/ _ \| '_ \ / _` |/ _` / __|  //
		// | |  | | (_) | | | | (_| | (_| \__ \  //
		// |_|  |_|\___/|_| |_|\__,_|\__,_|___/  //
		///////////////////////////////////////////
		// Monads

		// env.wrap_fn<Any (Any, Any)>
		//     (">>=",
		//      [env](Any fn, Any monad) {
		//     });

		// // `return` in Haskell
		// env.wrap_fn<Any (Any)>
		//     ("unit",
		//      [env](Any vv) {
		// 	return 
		//     });
	}

	void setup_interpreter(Environment &env, ParseString &parser) {
		init_types();
		export_recursive_defs(env.gc, env, parser);
	}
}

#endif
