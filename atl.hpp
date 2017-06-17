#ifndef ATL_HPP
#define ATL_HPP
/**
 * @file /home/ryan/programming/atl/atl.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Feb 07, 2015
 */

#include <iostream>                 // for cout, ostream
#include <atl/compile.hpp>              // for Compile
#include <atl/lexical_environment.hpp>  // for AssignForms, AssignFree, BackPatch
#include <atl/parser.hpp>               // for ParseString
#include <atl/type.hpp>                 // for init_types, Any, LAST_CONCRETE_TYPE
#include <atl/type_inference.hpp>       // for AlgorithmW, apply_substitution
#include <atl/vm.hpp>                   // for TinyVM, TinyVM::value_type
#include <atl/vm.hpp>                   // for TinyVM, TinyVM::value_type
#include "gc/gc.hpp"                // for GC, ast_composer
#include "gc/marked.hpp"            // for Marked
#include "wrap.hpp"                 // for unwrap


namespace atl
{
	struct Atl
	{
		GC gc;

		Slots slots;
		SymbolMap lexical;

		AssignForms assign_forms;

		Type::value_type new_types;
		inference::Gamma gamma;
		inference::AlgorithmW w;

		Compile compiler;
		TinyVM vm;

		std::ostream* stdout;

		Atl() :
			slots(gc)
			, lexical(gc, slots)
			, assign_forms(gc, lexical)
			, new_types(LAST_CONCRETE_TYPE)
			, gamma(gc, new_types)
			, w(gc, new_types, gamma)
			, compiler(gc)
			, vm(gc)
		{
			init_types();
			setup_basic_definitions(gc, lexical);
			stdout = &std::cout;
		}

		/**
		 * @tparam T:
		 * @param expr:
		 * @return: Return type of the expression
		 */
		Any annotate(Ast& expr)
		{
			expr = *assign_forms(expr);

			auto type_info = w.W(expr);
			inference::apply_substitution(gc, type_info.subs, ref_wrap(expr));

			return *type_info.type;
		}

		void compile(Ast& expr)
		{
			annotate(expr);
			compiler.compile(expr);
		}

		void compile(Marked<Ast>& expr)
		{
			annotate(*expr);
			compiler.compile(expr);
		}

		void compile(Marked<Ast>&& expr)
		{
			auto copy = std::move(expr);
			compile(copy);
		}

		void compile(ast_composer const& compose)
		{ compile(gc(compose)); }

		pcode::value_type run()
		{
			compiler.assemble.finish();

#ifdef DEBUGGING
			auto printer = CodePrinter(code);
			printer.dbg();
			vm.run_debug(compiler.code_store, 100);
#else
			vm.run(compiler.code_store);
#endif
			compiler.code_store.pop_back();

			return vm.stack[0];
		}

		Any eval_ast(Ast& ast)
		{
			auto type = gc.marked(annotate(ast));

			compiler.compile(ast);

			auto ran = run();

			return Any(unwrap<Type>(*type).value(),
			           reinterpret_cast<void*>(ran));
		}

		Any eval_ast(GC::ast_composer const& factory)
		{
			auto ast = gc(factory);
			return eval_ast(*ast);
		}

		Any eval(Any& any)
		{
			if(is<Ast>(any))
				{ return eval_ast(unwrap<Ast>(any)); }
			throw WrongTypeError("Not yet dealing with evaling atoms");

		}

		Any eval(Any&& any)
		{ return eval(any); }

		Any eval(std::istream& stream)
		{
			auto parsed = Parser(gc, stream).parse();
			return eval(*parsed);
		}

		Any eval(std::string const& input)
		{
			auto stream = std::istringstream(input);
			return eval(stream);
		}
	};
}

#endif
