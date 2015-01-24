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

// Stack layout the opcodes expect, bottom --> top:
//   if_                : [pc-of-alternate-branch][predicate-value]
//   foreign_2          : [arg1][arg2]
//   foreign_1          : [arg1]
//   push               : *
//   pop                : [out-going]
//   jump               : [destination address]
//   call_procedure     : [arg1]...[argN][@closure][next-instruction]<body...>
//   return_            : [return-value][number-of-arguments-to-procedure]
//   argument           : [offset]
//   nested_argument    : [offset][hops]
//   tail_call          : [arg0]..[argN][N][dst]

#define ATL_NORMAL_BYTE_CODES (nop)(pop)(if_)(foreign_1)(foreign_2)(variadic_foreign)(jump)(return_)(call_procedure)(argument)(nested_argument)(tail_call)
#define ATL_BYTE_CODES (finish)(push)ATL_NORMAL_BYTE_CODES

#define ATL_VM_SPECIAL_BYTE_CODES (finish)      // Have to be interpreted specially by the run/switch statement
#define ATL_VM_NORMAL_BYTE_CODES (push)ATL_NORMAL_BYTE_CODES

#define ATL_ASSEMBLER_SPECIAL_BYTE_CODES (push) // overloads the no argument version
#define ATL_ASSEMBLER_NORMAL_BYTE_CODES (finish)ATL_NORMAL_BYTE_CODES


namespace atl {
    typedef void (*Fn1)(uintptr_t*);
    typedef void (*Fn2)(uintptr_t*, uintptr_t*);
    typedef void (*FnN)(uintptr_t* begin, uintptr_t* end);

    namespace vm_codes {
        template<class T> struct Name;
        template<class T> struct Tag;

        const size_t number_of_instructions = BOOST_PP_SEQ_SIZE(ATL_BYTE_CODES);

        namespace values {
            enum code { BOOST_PP_SEQ_ENUM(ATL_BYTE_CODES) };
        }

#define M(r, data, i, instruction)                                      \
        struct instruction;                                             \
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


    struct AssembleVM {
        typedef uintptr_t value_type;
        typedef value_type* iterator;
        uintptr_t *_begin, *_end;

        std::vector<value_type> offset;

        AssembleVM(uintptr_t *output) : _begin(output), _end(output) {
            offset.push_back(0);
        }

#define M(r, data, instruction) AssembleVM& instruction() {             \
            *_end = vm_codes::Tag<vm_codes::instruction>::value;        \
            ++_end;                                                     \
            return *this;                                               \
        }
        BOOST_PP_SEQ_FOR_EACH(M, _, ATL_ASSEMBLER_NORMAL_BYTE_CODES)
#undef M

        AssembleVM& push() {
            *_end = vm_codes::Tag<vm_codes::push>::value;
            ++_end;

            ++offset.back();
            return *this;
        }

        AssembleVM& constant(uintptr_t cc) {
            push();
            *_end = cc;
            ++_end;
            return *this;
        }

        AssembleVM& pointer(void* cc) { return constant(reinterpret_cast<uintptr_t>(cc)); }

        std::map<std::string, std::vector<uintptr_t*> > _patches;

        uintptr_t* begin() { return _begin; }
        uintptr_t* end() { return _end; }
        uintptr_t* last() { return _end - 1; }

        AssembleVM& foreign_2(Fn2 fn) {
            constant(reinterpret_cast<uintptr_t>(fn));
            return foreign_2();
        }

        AssembleVM& call_procedure(uintptr_t* body) {
            constant(reinterpret_cast<uintptr_t>(body));
            return call_procedure();
        }

        AssembleVM& argument(uintptr_t offset) {
            constant(offset);
            return argument();
        }

        AssembleVM& nested_argument(uintptr_t offset, uintptr_t hops) {
            constant(offset);
            constant(hops);
            return nested_argument();
        }

        AssembleVM& return_(size_t num_args) {
            constant(num_args);
            return return_();
        }

        AssembleVM& tail_call(size_t num_args, iterator proc) {
            constant(num_args);
            pointer(proc);
            return tail_call();
        }

        uintptr_t& operator[](size_t offset) { return _begin[offset]; }

        void print();
    };

    void AssembleVM::print() {
        using namespace std;

        int pushing = 0;
        for(auto vv : *this) {
            auto vname = vm_codes::name_or_null(vv);

            cout << " " << hex << &vv << ": ";

            if(pushing) {
                --pushing;
                cout << "@" << hex << vv;
            }
            else {
                if(vv == vm_codes::Tag<vm_codes::push>::value)
                    ++pushing;
                cout << vname;
            }
            cout << endl;
        }}

    struct TinyVM {
        typedef uintptr_t value_type;
        typedef uintptr_t* iterator;

        iterator pc;
        iterator top;           // 1 past last value
        iterator call_stack;

        value_type stack[100];

        TinyVM() : top(stack), call_stack(stack) {}

        value_type back() { return *(top - 1); }

        void nop() { ++pc; }

        void push() { *(top++) = pc[1]; pc += 2; }
        void pop() { top--; ++pc; }

        void foreign_1() {
            top -= 1;
            (*reinterpret_cast<Fn1>(*top))(top - 1);
            ++pc;
        }

        void foreign_2() {
            top -= 2;
            (*reinterpret_cast<Fn2>(top[1]))(top - 1, top);
            ++pc;
        }

        void variadic_foreign() {
            auto fn = reinterpret_cast<FnN>(*pc);
            top -= 2;
            auto n = *top;

            auto end = top;
            top -= n;

            (*fn)(top - 1, end);

            ++pc;
        }

        void if_() {
            top -= 2;
            if(top[1] != 0) ++pc;
            else pc = reinterpret_cast<uintptr_t*>(*top);
        }

        void jump() { --top; pc = reinterpret_cast<uintptr_t*>(*top); }

        void call_procedure() {
            *top        = reinterpret_cast<value_type>(pc + 1);    // next instruction
            pc          = reinterpret_cast<uintptr_t*>(back());
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
            pc            = reinterpret_cast<iterator>(call_stack[1]);
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

            pc = reinterpret_cast<iterator>(next_pc);
            top = call_stack + 2;
        }

        bool step() {
            switch(*pc) {
#define M(r, data, instruction) case vm_codes::values::instruction: instruction(); return false;
                BOOST_PP_SEQ_FOR_EACH(M, _, ATL_VM_NORMAL_BYTE_CODES)
#undef M
            case vm_codes::values::finish:
                    return true;
            default: return false;
            }}

        void run_debug(uintptr_t *in, unsigned int max_steps = 1000) {
            pc = in;

            for(unsigned int i = 0; i < max_steps; ++i) {
                std::cout << "====================\n"
                          << "= " << vm_codes::name(*pc) << "\n"
                          << "= call stack: " << hex << call_stack << " pc: " << pc << "\n"
                          << "====================" << std::endl;
                if(step()) break;
                print_stack();
            }
            print_stack();
        }

        void run(uintptr_t *in) {
            pc = in;

            while(true) {
                switch(*pc) {
#define M(r, data, instruction) case vm_codes::values::instruction: instruction(); break;
                    BOOST_PP_SEQ_FOR_EACH(M, _, ATL_VM_NORMAL_BYTE_CODES)
#undef M
                case vm_codes::values::finish:
                        return;
                }}}

        iterator begin() { return stack; }
        iterator end() { return top; }

        void print_stack() {
            using namespace std;

            for(auto vv : pointer_range(*this))
                cout << " " << hex << vv << ": @" << *vv << endl;
        }

    };
}

// Set the flag to compile this as an app that just prints the byte
// code numbers (in hex) and the corrosponding name
#ifdef TINY_VM_PRINT_BYTE_CODES
int main() {
    using namespace std;
    using namespace atl::vm_codes;
#define M(r, data, instruction) cout << setw(15) << BOOST_PP_STRINGIZE(instruction) ": " << hex << Tag<instruction>::value << endl;
    BOOST_PP_SEQ_FOR_EACH(M, _, ATL_BYTE_CODES);
#undef M

    return 0;
}
#endif

#endif
