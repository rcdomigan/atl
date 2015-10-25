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
	struct TinyVM
	{
		typedef uintptr_t value_type;
		typedef uintptr_t* iterator;
		static const size_t stack_size = 100;

		Code::Backer const* code;	// just the byte code

		vm_stack::Offset pc;
		iterator top;           // 1 past last value
		iterator call_stack;	// points to the pointer to the enclosing frame

		value_type stack[stack_size]; // the function argument and adress stack

		TinyVM() : top(stack), call_stack(stack) {}

		value_type back() { return *(top - 1); }

		void nop() { ++pc; }

		void push() { *(top++) = (*code)[pc + 1]; pc += 2; }
		void pop() { top--; ++pc; }

		template<class Fn>
		void call_cxx_function()
		{
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

		void if_()
		{
			top -= 2;
			if(top[1] != 0) ++pc;
			else pc = *top;
		}

		void jump() { --top; pc = *top; }

		/** Use the top of the stack as a procedure address and call it.
		 * Pre call stack:
		 *   [arg1]...[argN][next-instruction]
		 *                                    ^- top
		 * Post call:
		 *   [arg1]...[argN][old-call-stack][return-address]
		 *                   ^                              ^- top
		 *                   ^- call_stack
		 */
		void call_procedure()
		{
			*top        = reinterpret_cast<value_type>(pc + 1);
			pc          = back();
			*(top - 1)  = reinterpret_cast<uintptr_t>(call_stack);
			call_stack  = top - 1;
			++top;
		}

		/** Assume the top of the stack is a closure and call it
		 * Pre call stack:
		 *   [arg1]...[argN][closure]
		 *                           ^- top
		 * Post call:
		 *   [arg1]...[argN][old-call-stack][return-address][bound-vars]
		 *                   ^                                     top -^
		 *                   ^- call_stack
		 */
		void call_closure()
		{
			--top;
			auto closure	= reinterpret_cast<Closure*>(top[0]);
			top[0]			= reinterpret_cast<value_type>(call_stack);
			top[1]			= reinterpret_cast<value_type>(pc + 1);
			top[2]			= reinterpret_cast<value_type>(&closure->values);
			pc				= closure->body;
			*top			= reinterpret_cast<uintptr_t>(call_stack);
			call_stack		= top;
			top += 3;
		}

		/** Gets the `arg-offset` value from this frame's closure.
		 * pre: [arg-offset]<- top
		 * post: [value]<-top
		 * */
		void closure_argument()
		{
			*(top - 1) = (*reinterpret_cast<Closure::Values*>(call_stack[2]))[*(top - 1)];
			++pc;
		}

		/** Get the nth argument counting backwards from the top frame on call_stack */
		void argument()
		{
			--top;
			*top = *(call_stack - *top);
			++top; ++pc;
		}

		/** Access the `offset`th parameter from an enclosing function
		 * `hops` frames up the call stack.
		 *
		 * Pre call stack:
		 *   [offset][hops]
		 *                 ^- top
		 * Post call stack:
		 *   [value]
		 *          ^- top
		 */
		void nested_argument()
		{
			top -= 2;
			auto hops = top[0];

			auto frame = call_stack;
			for(; hops; --hops) frame = reinterpret_cast<iterator>(*frame);

			top[0] = *(frame - top[1]);
			++top;
		}

		/** [return-value][number-of-arguments-to-procedure]
		 *                                                  ^- top
		 */
		void return_()
		{
			auto num_args = back();
			auto result   = *(top - 2);
			top           = call_stack - num_args;
			pc            = call_stack[1];
			call_stack    = reinterpret_cast<iterator>(call_stack[0]);

			*top = result; ++top;
		}

		/** Replace the current function's stack frame with arg0-argN
		 * from the top of the stack, and re-use it when calling
		 * `procedure-address`
		 *
		 * Pre call stack:
		 *
		 *  @caller's frame
		 *   [caller's args...]
		 *   [old-call-stack]	<- call_stack
		 *   [return-address]
		 *   ...
		 *   [my args...]
		 *   [N]
		 *   [procedure-address]
		 *						<- top
		 * Post call:
		 *
		 *  @caller's frame
		 *   [my-args...]
		 *   [old-call-stack]	<- call_stack
		 *   [return-address]
		 *                      <- top
		 */
		void tail_call()
		{
			top -= 2;
			auto num_args = top[0];
			pc = top[1];

			for(auto iitr = top - num_args,
				    dest = call_stack - num_args;
			    iitr != top;
			    ++iitr, ++dest)
				*dest = *iitr;

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


		/** \internal
		 * Call the 'main' function if it exists otherwise
		 * treat input as a code fragment and enter at pc = 0.
		 *
		 * @param input: code to enter
		 */
		void enter_code(Code const& input)
		{
			call_stack = nullptr;
			if(input.has_main())
				pc = input.main_entry_point();
			else
				pc = 0;
		}

		// Take code and run it.  Prints the stack and pc after each
		// instruction.
		//
		// @param max_steps: vm will exit after max_steps instructions
		// have been evaluated, even if we never reach a 'finish' instruction.
		void run_debug(Code const& input,
		               unsigned int max_steps = 2048)
		{
			enter_code(input);

			this->code = &input.code;

			for(unsigned int i = 0; ; ++i) {
				std::cout << "====================\n"
				          << "= " << vm_codes::name((*code)[pc]) << "\n"
				          << "= call stack: " << call_stack << " pc: " << pc << "\n"
				          << "====================" << std::endl;
				print_stack();
				if(step(*code)) break;
				if(i > max_steps)
					throw BadPCodeInstruction("Too many steps in debug evaluation");
			}
			print_stack();
		}

		void run(Code const& input)
		{
			enter_code(input);

			this->code = &input.code;

			while(true)
				{
					switch((*code)[pc])
						{
#define M(r, data, instruction) case vm_codes::values::instruction: instruction(); break;
							BOOST_PP_SEQ_FOR_EACH(M, _, ATL_VM_NORMAL_BYTE_CODES)
#undef M
						case vm_codes::values::finish:
								return;
						}
				}
		}

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
