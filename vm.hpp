#ifndef ATL_VM_HPP
#define ATL_VM_HPP
/**
 * @file /home/ryan/programming/atl/vm.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Nov 10, 2014
 */

#include <iostream>

#include <map>
#include <string>
#include <vector>

#include <boost/preprocessor/seq/enum.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/for_each_i.hpp>
#include <boost/preprocessor/punctuation/comma_if.hpp>
#include <boost/preprocessor/stringize.hpp>

#include "./type.hpp"
#include "./gc.hpp"
#include "./utility.hpp"
#include "./exception.hpp"

#include "./byte_code.hpp"


namespace atl
{
	// The PCode needs to end in a 'finish' instruction to run.  The
	// compiler doesn't put in the 'finish' statement by default since
	// it may be awaiting more input.
	struct RunnableCode
	{
		AssembleCode &code;
		bool _do_pop;

		RunnableCode(AssembleCode &code_)
			: code(code_)
		{
			if(code.empty()
			   || (code.back() != vm_codes::values::finish))
				{
					_do_pop = true;
					code.finish();
				}
			else
				_do_pop = false;
		}

		RunnableCode(RunnableCode&& other)
			: code(other.code), _do_pop(other._do_pop)
		{
			other._do_pop = false;
		}

		~RunnableCode()
		{
			if(_do_pop)
				code.pop_back();
		}
	};

	struct TinyVM
	{
		typedef uintptr_t value_type;
		typedef uintptr_t* iterator;
		static const size_t stack_size = 100;

		Code const* code_stuff;
		Code::Backer const* code;

		vm_stack::Offset pc;
		iterator top;           // 1 past last value
		iterator call_stack;

		value_type stack[stack_size];

		struct SaveState
		{
			TinyVM& vm;
			vm_stack::Offset pc;
			iterator top;           // 1 past last value
			iterator call_stack;

			SaveState(TinyVM& vm_)
				: vm(vm_),
				  pc(vm_.pc),
				  top(vm_.top),
				  call_stack(vm_.call_stack)
			{}

			~SaveState()
			{
				vm.pc = pc;
				vm.top = top;
			}
		};

		SaveState save_excursion()
		{ return SaveState(*this); }

		TinyVM() : top(stack), call_stack(stack) {}

		value_type back() { return *(top - 1); }

		void nop() { ++pc; }

		void push() { *(top++) = (*code)[pc + 1]; pc += 2; }
		void pop() { top--; ++pc; }

		template<class Fn>
		void call_cxx_function() {
			auto fn = reinterpret_cast<Fn>(*(top - 1));
			top -= 2;
			auto n = *top;

			auto end = top;
			top -= n;

			(*fn)(top, end);
			++top;
			++pc;
		}

		void std_function() { call_cxx_function<CxxFunctor::value_type*>(); }

		void if_() {
			top -= 2;
			if(top[1] != 0) ++pc;
			else pc = *top;
		}

		void jump() { --top; pc = *top; }

		void call_procedure() {
			*top        = reinterpret_cast<value_type>(pc + 1);    // next instruction
			pc          = back();
			*(top - 1)  = reinterpret_cast<uintptr_t>(call_stack); // store the enclosing frame
			call_stack  = top - 1;                                 // update the frame
			++top;
		}

		void argument() {
			--top;
			*top = *(call_stack - *top);
			++top; ++pc;
		}

		void nested_argument() {
			throw std::string("TODO");
		}

		void return_() {
			auto num_args = back();
			auto result   = *(top - 2);
			top           = call_stack - num_args;
			pc            = call_stack[1];
			call_stack    = reinterpret_cast<iterator>(call_stack[0]);

			*top = result; ++top;
		}

		void tail_call() {
			// move N arguments from the top of the stack to dst
			top -= 2;
			auto next_pc = top[1];
			auto nn = top[0];

			for(auto iitr = top - nn,
				    dest = call_stack - nn;
			    iitr != top;
			    ++iitr, ++dest)
				*dest = *iitr;

			pc = next_pc;
			top = call_stack + 2;
		}

		bool step(Code::Backer const& code)
		{
			switch(code[pc])
				{
#define M(r, data, instruction) case vm_codes::values::instruction: instruction(); return false;
					BOOST_PP_SEQ_FOR_EACH(M, _, ATL_VM_NORMAL_BYTE_CODES)
#undef M
				case vm_codes::values::finish:
						return true;
				default:
					throw BadPCodeInstruction(std::string("Unknown instruction ")
					                          .append(std::to_string(code[pc]))
					                          .append(" at @")
					                          .append(std::to_string(pc)));
				}
		}

		// Take code and run it.  Prints the stack and pc after each
		// instruction.
		//
		// @param max_steps: vm will exit after max_steps instructions
		// have been evaluated, even if we never reach a 'finish' instruction.
		void run_debug(RunnableCode const& input,
		               unsigned int max_steps = 1000)
		{
            top = stack;
            pc = input.code.main_entry_point;

			this->code_stuff = input.code.output;
			this->code = &code_stuff->code;

			for(unsigned int i = 0; i < max_steps; ++i) {
				std::cout << "====================\n"
				          << "= " << vm_codes::name((*code)[pc]) << "\n"
				          << "= call stack: " << call_stack << " pc: " << pc << "\n"
				          << "====================" << std::endl;
				if(step(*code)) break;
				print_stack();
			}
			print_stack();
		}

		void run(RunnableCode const& input)
		{
			pc = input.code.main_entry_point;

			this->code_stuff = input.code.output;
			this->code = &code_stuff->code;

			while(true) {
				switch((*code)[pc]) {
#define M(r, data, instruction) case vm_codes::values::instruction: instruction(); break;
					BOOST_PP_SEQ_FOR_EACH(M, _, ATL_VM_NORMAL_BYTE_CODES)
#undef M
				case vm_codes::values::finish:
						return;
				}}}

		iterator begin() { return stack; }
		iterator end() { return top; }

		value_type result()
		{ return *(top - 1); }

		void print_stack();
	};

	void TinyVM::print_stack()
	{
		using namespace std;

		for(auto vv : pointer_range(*this))
			cout << " " << vv << ": @" << *vv << endl;
	}
}

#endif
