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


#define ATL_NORMAL_BYTE_CODES (nop)(pop)(if_)(std_function)(jump)(return_)(call_procedure)(argument)(nested_argument)(tail_call)
#define ATL_BYTE_CODES (finish)(push)(push_word)ATL_NORMAL_BYTE_CODES

#define ATL_VM_SPECIAL_BYTE_CODES (finish)      // Have to be interpreted specially by the run/switch statement
#define ATL_VM_NORMAL_BYTE_CODES (push)ATL_NORMAL_BYTE_CODES

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


    struct AssembleVM
    {
        typedef uintptr_t value_type;
        typedef value_type* iterator;
        typedef value_type const* const_iterator;

        iterator _begin, _end;
        iterator main_entry_point;

        // TODO: don't think I ended up using this
        std::vector<value_type> offset;

        AssembleVM(uintptr_t *output = nullptr)
            : _begin(output), _end(output), main_entry_point(output)
        {
            offset.push_back(0);
        }

        AssembleVM(AssembleVM&& other)
            : _begin(std::move(other._begin)),
              _end(std::move(other._end)),
              main_entry_point(std::move(other.main_entry_point)),
              offset(std::move(other.offset)) {}

        AssembleVM& operator=(AssembleVM&& other) {
            using namespace std;

            offset.clear();
            offset = move(other.offset);

            _begin = move(other._begin);
            _end = move(other._end);
            main_entry_point = move(other.main_entry_point);

            return *this;
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

        AssembleVM& push_tagged(const Any& aa) {
            constant(aa._tag);
            return pointer(aa.value);
        }

        iterator begin() { return _begin; }
        iterator end() { return _end; }
        const_iterator begin() const { return _begin; }
        const_iterator end() const { return _end; }

        iterator last() { return _end - 1; }
        value_type back() { return *last(); }


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

        AssembleVM& std_function(CxxFunctor::value_type* fn, size_t arity) {
            constant(arity);
            pointer(reinterpret_cast<void*>(fn));
            return std_function();
        }

        uintptr_t& operator[](size_t offset) { return _begin[offset]; }

        template<class Range>
        void print(Range const& range) const
        {
            using namespace std;

            int pushing = 0;
            for(auto& vv : range)
                {
                    auto vname = vm_codes::name_or_null(vv);

                    cout << " " << hex << &vv << ": ";

                    if(pushing)
                        {
                            --pushing;
                            cout << "@" << hex << vv;
                        }
                    else
                        {
                            if(vv == vm_codes::Tag<vm_codes::push>::value)
                                ++pushing;
                            cout << vname;
                        }
                    cout << endl;
                }
        }
        void dbg();
        void dbg_to_run();
        void dbg_explicit(iterator begin, iterator end);
    };

    void AssembleVM::dbg()
    {
        print(*this);
    }

    void AssembleVM::dbg_to_run()
    {
        print(make_range(main_entry_point, _end));
    }

    void AssembleVM::dbg_explicit(iterator begin, iterator end)
    {
        print(make_range(begin, end));
    }


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

        void run_debug(vm_stack::iterator enter, unsigned int max_steps = 1000) {
            top = stack;
            pc = enter;

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

        void run(iterator in) {
            pc = in;
            top = stack;

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
