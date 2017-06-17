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
#include "./utility.hpp"
#include "./exception.hpp"

#include "./byte_code.hpp"
#include "./gc.hpp"

namespace atl
{
	struct Closure
	{
		pcode::value_type formals_count;
		pcode::value_type body;

		pcode::iterator captured()
		{ return reinterpret_cast<pcode::iterator>(this) + 2; }
	};

	struct TinyVM
	{
		typedef uintptr_t value_type;
		typedef value_type* iterator;
		static const size_t stack_size = 100;

		GC& _gc;

		CodeBacker const* code;	// just the byte code
		value_type* slots;

		vm_stack::Offset pc;
		iterator top;           // 1 past last value
		iterator call_stack;	// points to the pointer to the enclosing frame

		value_type stack[stack_size]; // the function argument and adress stack

		TinyVM()=delete;

		void _reset_stack()
		{
			call_stack = nullptr;
			pc = 0;
			top = stack;
		}

		TinyVM(GC& gc)
			: _gc(gc),
			  slots(nullptr)
		{ _reset_stack(); }

		~TinyVM() { if(slots) { delete []slots; } }

		value_type back() { return *(top - 1); }

		void nop() { ++pc; }

		void push() { *(top++) = (*code)[pc + 1]; pc += 2; }
		void pop() { top--; ++pc; }

		inline iterator args_begin() { return call_stack - 1; }

		/** [arg1]...[argn][arity][pointer to std::function] */
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

		/* Pre call:
		 *  [value][slot]
		 * Post call:
		 */
		void define()
		{
			slots[*(top - 1)] = *(top - 2);
			top -= 2;
			++pc;
		}

		/* Pre call:
		 *  [slot]
		 *
		 * Post call:
		 *  [value-from-slot]
		 */
		void deref_slot()
		{
			*(top - 1) = slots[*(top - 1)];
			++pc;
		}

		/**
		 * Pre call:
		 *   [][alternate-instruction][predicate]
		 *                                       ^- top
		 * Post call:
		 *   []
		 *     ^- top
		 */
		void if_()
		{
			top -= 2;
			if(top[1] != 0) ++pc;
			else pc = *top;
		}

		void jump() { --top; pc = *top; }

		/** Assume the top of the stack is a closure and call it
		 * Pre call stack:
		 *   [arg1]...[argN][closure]
		 *                           ^- top
		 * Post call:
		 *   [arg1]...[argN][old-call-stack][N][return-address][closure-vars]
		 *                  ^                                           top -^
		 *                  ^- call_stack
		 */
		void call_closure()
		{
			--top;
			//   [arg1]...[argN][closure]
			//                  ^- top
			auto closure	= reinterpret_cast<Closure*>(top[0]);

			*top = reinterpret_cast<value_type>(call_stack);          // old-call-stack
			call_stack = top;
			++top;

			*top = closure->formals_count;                            // N
			++top;

			*top = reinterpret_cast<value_type>(pc + 1);              // return-address
			++top;

			*top = reinterpret_cast<value_type>(closure->captured()); // closure
			++top;

			pc = closure->body;
		}

		/**
		 * Pre call stack:
		 *   [body-addr][capture-arg1]...[capture-argN][formals-count][capture-count]
		 *                                                                      top -^
		 * Post call stack:
		 *   [pointer-to-closure]
		 *                  top -^
		 */
		void make_closure()
		{
			auto formals = *(top - 2);
			auto captured = *(top - 1);

			// the closure its self will be [formals-count][body_address][arg1]...
			value_type *closure = _gc.closure(*(top-captured-3), formals, captured);

			auto args_end = top - 2;
			auto args_begin = args_end - captured;

			for(auto itr = args_begin, out_itr = closure+2;
			    itr < args_end;
			    ++itr, ++out_itr)
				{ *out_itr = *itr; }

			top -= (captured + 2);
			*(top - 1) = reinterpret_cast<value_type>(closure);
			++pc;
		}

		/** Gets the `arg-offset` value from this frame's closure.
		 * pre: [arg-offset]<- top
		 * post: [value]<-top
		 * */
		void closure_argument()
		{
			*(top - 1) = reinterpret_cast<pcode::iterator>(call_stack[3])[*(top - 1)];
			++pc;
		}

		/** Get the nth argument counting backwards from the top frame on call_stack */
		void argument()
		{
			--top;
			*top = *(args_begin() - *top);
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

		/** [arg0]...[argN][frame][N][return-address]...[return-value]
		 *                 ^- call-stack                              ^- top
		 *  [return-value]
		 *                ^- top
		 */
		void return_()
		{
			auto num_args = call_stack[1];
			auto result   = *(top - 1);
			top           = call_stack - num_args;
			pc            = call_stack[2];
			call_stack    = reinterpret_cast<iterator>(call_stack[0]);

			*top = result;
			++top;
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
		 *   [caller's arg count]
		 *   [return-address]
		 *   ...
		 *   [my args...]
		 *   [closure]
		 *						<- top
		 * Post call:
		 *
		 *  @caller's frame
		 *   [my args...]
		 *   [old-call-stack]	<- call_stack
		 *   [my arg count]
		 *   [return-address]
		 *   [closure-vars]
		 *                      <- top
		 */
		void tail_call()
		{
			auto enclosing_frame = call_stack[0];
			auto enclosing_arg_count = call_stack[1];
			auto pc_continue = call_stack[2];

			auto closure = reinterpret_cast<Closure*>(*(top - 1));

			call_stack -= enclosing_arg_count;
			for(auto iitr = top - closure->formals_count - 1;
			    iitr != top;
			    ++iitr, ++call_stack)
				{ *call_stack = *iitr; }

			*call_stack = closure->formals_count;
			call_stack[0] = enclosing_frame;
			call_stack[1] = pc_continue;
			call_stack[2] = reinterpret_cast<value_type>(closure->captured());

			top = call_stack + 3;
			pc = closure->body;
		}

		bool step(CodeBacker const& code)
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
			if(slots) { delete []slots; }
			slots = new value_type[input.num_slots];
			_reset_stack();
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
				          << "= call stack: " << call_stack << " pc: " << pc << "\n"
				          << "= " << vm_codes::name((*code)[pc]);

				if((*code)[pc] == vm_codes::Tag<vm_codes::push>::value)
					{ std::cout << " " << (*code)[pc + 1]; }

				std::cout << "\n====================" << std::endl;

				if(step(*code)) break;

				print_stack();

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
		size_t size() { return end() - begin(); }

		value_type result()
		{ return *(top - 1); }

		void print_stack();
	};


	void TinyVM::print_stack()
	{
		using namespace std;

		auto stack = call_stack;
		size_t frame_count = 1;

		for(auto itr = end() - 1; itr >= begin(); --itr)
			{
				cout << " " << itr << ": @" << *itr;

				if(itr == stack)
					{
						cout << "   frame:" << frame_count;
						++frame_count;
						stack = reinterpret_cast<iterator>(*stack);
					}

				cout << endl;
			}
	}
}

#endif
