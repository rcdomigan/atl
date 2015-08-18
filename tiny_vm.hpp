#ifndef ATL_TINY_VM_HPP
#define ATL_TINY_VM_HPP
/**
 * @file /home/ryan/programming/atl/tiny_vm.hpp
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

// Stack layout the opcodes expect, bottom --> top:
//   if_                : [pc-of-alternate-branch][predicate-value]
//   fn_pntr            : [arg1]...[argN][N][function-pointer]
//   std_function       : [arg1]...[argN][N][function-object-pointer]
//   push               : [word]
//   pop                : [out-going]
//   jump               : [destination address]
//   call_procedure     : [arg1]...[argN][@closure][next-instruction]<body...>
//   return_            : [return-value][number-of-arguments-to-procedure]
//   argument           : [offset]
//   nested_argument    : [offset][hops]
//   tail_call          : [arg0]..[argN][N][dst]
//   finish             : -


#define ATL_NORMAL_BYTE_CODES (nop)(push)(pop)(if_)(std_function)(jump)(return_)(call_procedure)(argument)(nested_argument)(tail_call)
#define ATL_BYTE_CODES (finish)(push_word)ATL_NORMAL_BYTE_CODES

#define ATL_VM_SPECIAL_BYTE_CODES (finish)      // Have to be interpreted specially by the run/switch statement
#define ATL_VM_NORMAL_BYTE_CODES ATL_NORMAL_BYTE_CODES

#define ATL_ASSEMBLER_SPECIAL_BYTE_CODES (push) // overloads the no argument version
#define ATL_ASSEMBLER_NORMAL_BYTE_CODES (finish)ATL_NORMAL_BYTE_CODES


namespace atl {

	namespace vm_codes {
		template<class T> struct Name;
		template<class T> struct Tag;

		const size_t number_of_instructions = BOOST_PP_SEQ_SIZE(ATL_BYTE_CODES);

		namespace values {
			enum code { BOOST_PP_SEQ_ENUM(ATL_BYTE_CODES) };
		}

#define M(r, data, i, instruction)                                      \
		struct instruction; \
		template<> struct Tag<instruction> { static const tag_t value = i; }; \
		template<> struct Name<instruction> { constexpr static const char* value = BOOST_PP_STRINGIZE(instruction); };

		BOOST_PP_SEQ_FOR_EACH_I(M, _, ATL_BYTE_CODES)
#undef M

#define M(r, data, i, instruction) BOOST_PP_COMMA_IF(i) Name<instruction>::value

		static const char *instruction_names[] = { BOOST_PP_SEQ_FOR_EACH_I(M, _, ATL_BYTE_CODES) };
#undef M

		static const char* name(tag_t instruction) {
			if(instruction >= number_of_instructions)
				return "<CODE UNDEFINED>";
			return instruction_names[instruction];
		}

		static const char* name_or_null(tag_t instruction) {
			if(instruction >= number_of_instructions)
				return nullptr;
			return instruction_names[instruction];
		}
	}

	typedef GC::PCodeAccumulator Code;

	void dbg_code(Code const& code)
	{
		using namespace std;

		int pushing = 0;
		size_t pos = 0;
		for(auto& vv : code)
			{
				auto vname = vm_codes::name_or_null(vv);

				cout << " " << pos << ": ";

				if(pushing)
					{
						--pushing;
						cout << "@" << vv;
					}
				else
					{
						if(vv == vm_codes::Tag<vm_codes::push>::value)
							++pushing;
						cout << vname;
					}
				cout << endl;
				++pos;
			}
		cout << "<--- done printing code --->" << endl;
	}


	struct AssembleVM
	{
		typedef uintptr_t value_type;
		typedef Code::const_iterator const_iterator;

		Code* output;

		pcode::Offset main_entry_point;

		AssembleVM(GC::PCodeAccumulator *output_)
			: output(output_), main_entry_point(0)
		{}

#define M(r, data, instruction) AssembleVM& instruction() {             \
			_push_back(vm_codes::Tag<vm_codes::instruction>::value); \
			return *this; \
		}
		BOOST_PP_SEQ_FOR_EACH(M, _, ATL_ASSEMBLER_NORMAL_BYTE_CODES)
#undef M

		void _push_back(uintptr_t cc) { output->push_back(cc); }

		AssembleVM& constant(uintptr_t cc)
		{
			push();
			_push_back(cc);
			return *this;
		}

		AssembleVM& pointer(void* cc) { return constant(reinterpret_cast<value_type>(cc)); }

		AssembleVM& push_tagged(const Any& aa)
		{
			constant(aa._tag);
			return pointer(aa.value);
		}

		void pop_back() { output->pop_back(); }
		void clear()
		{
			main_entry_point = 0;
			output->clear();
		}

		const_iterator begin() const { return output->begin(); }
		const_iterator end() const { return output->end(); }
		bool empty() const { return output->empty(); }

		pcode::Offset pos_last() { return output->size() - 1;}
		pcode::Offset pos_end() { return output->size(); }
		value_type back() { return output->back(); }


		// Takes the offset to the body
		AssembleVM& call_procedure(pcode::Offset body)
		{
			constant(reinterpret_cast<value_type>(body));
			return call_procedure();
		}

		AssembleVM& argument(uintptr_t offset)
		{
			constant(offset);
			return argument();
		}

		AssembleVM& nested_argument(uintptr_t offset, uintptr_t hops)
		{
			constant(offset);
			constant(hops);
			return nested_argument();
		}

		AssembleVM& return_(size_t num_args)
		{
			constant(num_args);
			return return_();
		}

		AssembleVM& tail_call(size_t num_args, size_t proc)
		{
			constant(num_args);
			constant(proc);
			return tail_call();
		}

		AssembleVM& std_function(CxxFunctor::value_type* fn, size_t arity)
		{
			constant(arity);
			pointer(reinterpret_cast<void*>(fn));
			return std_function();
		}

		uintptr_t& operator[](size_t offset) { return (*output)[offset]; }
	};


	// The PCode needs to end in a 'finish' instruction to run.  The
	// compiler doesn't put in the 'finish' statement by default since
	// it may be awaiting more input.
	struct RunnableCode
	{
		AssembleVM &code;
		bool _do_pop;

		RunnableCode(AssembleVM &code_)
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

		Code const* code;
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

		bool step(Code const& code)
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
            this->code = input.code.output;

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
			top = stack;
			this->code = input.code.output;

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

		void print_stack();
	};

	void TinyVM::print_stack()
	{
		using namespace std;

		for(auto vv : pointer_range(*this))
			cout << " " << vv << ": @" << *vv << endl;
	}
}

// Set the flag to compile this as an app that just prints the byte
// code numbers (in hex) and the corrosponding name
#ifdef TINY_VM_PRINT_BYTE_CODES
int main() {
    using namespace std;
    using namespace atl::vm_codes;
#define M(r, data, instruction) cout << setw(15) << BOOST_PP_STRINGIZE(instruction) ": " << Tag<instruction>::value << endl;
    BOOST_PP_SEQ_FOR_EACH(M, _, ATL_BYTE_CODES);
#undef M

    return 0;
}
#endif

#endif
