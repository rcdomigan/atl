#ifndef ENVIRONMENT_HPP
#define ENVIRONMENT_HPP
/**
 * @file /home/ryan/programming/atl/environment.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Aug 14, 2015
 */

#include "./parser.hpp"
#include "./helpers.hpp"
#include "./compile.hpp"
#include "./lexical_environment.hpp"
#include "./byte_code.hpp"

namespace atl
{
	struct Environment
	{
		GC& gc;
		ParseString& parse;
		SymbolMap& lexical;
		Compile& compile;
		TinyVM& vm;


		AssembleCode *pcode;
		std::ostream* stdout;

		tag_t _struct_tag;

		Environment(GC& gc_,
		            ParseString& parse_,
		            SymbolMap& lexical_,
		            Compile& compile_,
		            TinyVM& vm_)
			: gc(gc_), parse(parse_), lexical(lexical_),
			  compile(compile_), vm(vm_)
		{
			stdout = &cout;
		}

        void eval(PassByValue value)
		{
			compile.any(value.any);
			{
				auto code = std::move(compile.code_store);
#ifdef DEBUGGING
				code.dbg();
				vm.run_debug(code);
#else
				vm.run(code);
#endif
			}
		}

		tag_t struct_tag() { return _struct_tag++; }
	};
}




#endif
