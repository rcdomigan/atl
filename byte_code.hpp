#ifndef BYTE_CODE_HPP
#define BYTE_CODE_HPP
/**
 * @file /home/ryan/programming/atl/byte_code.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Oct 01, 2015
 *
 * Definition of the byte-code for my dumb assembler/linker
 */

#include <iostream>

#include <map>
#include <string>
#include <vector>
#include <cassert>

#include <boost/preprocessor/seq/enum.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/for_each_i.hpp>
#include <boost/preprocessor/punctuation/comma_if.hpp>
#include <boost/preprocessor/stringize.hpp>

#include "./type.hpp"
#include "./utility.hpp"
#include "./exception.hpp"

// Stack layout the opcodes expect, bottom --> top:
//   if_                : [pc-of-alternate-branch][predicate-value]
//   fn_pntr            : [arg1]...[argN][N][function-pointer]
//   std_function       : [arg1]...[argN][N][function-object-pointer]
//   push               : [word]
//   pop                : [out-going]
//   jump               : [destination address]
//   return_            : [return-value]
//   argument           : [offset]
//   nested_argument    : [offset][hops]
//   tail_call          : [arg0]..[argN][N][procedure-address]
//   call_closure       : [arg0]..[argN][closure-address]
//   closure_argument   : [arg-offset]
//   make_closure       : [arg1]...[argN][N]
//   finish             : -
//   define             : [closure-pointer][slot]
//   deref_slot         : [slot]


#define ATL_NORMAL_BYTE_CODES (nop)(push)(pop)(if_)(std_function)(jump)(return_)(argument)(nested_argument)(tail_call)(call_closure)(closure_argument)(make_closure)(deref_slot)(define)
#define ATL_BYTE_CODES (finish)(push_word)ATL_NORMAL_BYTE_CODES

#define ATL_VM_SPECIAL_BYTE_CODES (finish)      // Have to be interpreted specially by the run/switch statement
#define ATL_VM_NORMAL_BYTE_CODES ATL_NORMAL_BYTE_CODES

#define ATL_ASSEMBLER_SPECIAL_BYTE_CODES (push) // overloads the no argument version
#define ATL_ASSEMBLER_NORMAL_BYTE_CODES (finish)ATL_NORMAL_BYTE_CODES


namespace atl
{
	namespace vm_codes
	{
		template<class T> struct Name;
		template<class T> struct Tag;

		const size_t number_of_instructions = BOOST_PP_SEQ_SIZE(ATL_BYTE_CODES);

		namespace values
		{ enum code { BOOST_PP_SEQ_ENUM(ATL_BYTE_CODES) }; }

#define M(r, data, i, instruction)                                      \
		struct instruction; \
		template<> struct Tag<instruction> { static const tag_t value = i; }; \
		template<> struct Name<instruction> { constexpr static const char* value = BOOST_PP_STRINGIZE(instruction); };

		BOOST_PP_SEQ_FOR_EACH_I(M, _, ATL_BYTE_CODES)
#undef M

#define M(r, data, i, instruction) BOOST_PP_COMMA_IF(i) Name<instruction>::value

		static const char *instruction_names[] = { BOOST_PP_SEQ_FOR_EACH_I(M, _, ATL_BYTE_CODES) };
#undef M

		static const char* name(tag_t instruction)
		{
			if(instruction >= number_of_instructions)
				return "<CODE UNDEFINED>";
			return instruction_names[instruction];
		}

		static const char* name_or_null(tag_t instruction)
		{
			if(instruction >= number_of_instructions)
				return nullptr;
			return instruction_names[instruction];
		}
	}

	struct OffsetTable
	{
		typedef std::unordered_map<std::string, size_t> Table;
		typedef std::unordered_multimap<size_t, std::string> ReverseTable;
		Table table;
		ReverseTable reverse_table;

		/*** Create or update an entry in the offset tables
		 * @param name: name of the symbol referencing this piece of code
		 * @param pos: position in the code being referenced
		 */
		void set(std::string const& name, size_t pos)
		{
			auto old = table.find(name);
			if(old != table.end())
				{
					auto range = reverse_table.equal_range(old->second);
					for(auto itr = range.first; itr != range.second; ++itr)
					{
						if(itr->second == name)
							{
								reverse_table.erase(itr);
								break;
							}
					}
				}

			table[name] = pos;
			reverse_table.emplace(std::make_pair(pos, name));
		}

		Range<typename ReverseTable::const_iterator>
		symbols_at(size_t pos) const
		{
			auto range = reverse_table.equal_range(pos);
			return make_range(range.first, range.second);
		}

		size_t operator[](std::string const& name)
		{ return table[name]; }
	};


	/** The "AssembleCode" helper class just needs a working
	    push_back.  Add an abstract base so I can wrap byte-code in a
	    way that either extends the container or over-writes existing
	    code.
	 */
	typedef std::vector<pcode::value_type> CodeBacker;
	struct Code
	{
		typedef typename pcode::value_type value_type;
		typedef typename CodeBacker::iterator iterator;
		typedef typename CodeBacker::const_iterator const_iterator;

		OffsetTable offset_table;
		size_t num_slots;

		CodeBacker code;

		Code() : num_slots(0) {};
		Code(size_t initial_size) : num_slots(0), code(initial_size) {}

		iterator begin() { return code.begin(); }
		iterator end() { return code.end(); }

		const_iterator begin() const { return code.begin(); }
		const_iterator end() const { return code.end(); }

		void push_back(uintptr_t cc) { code.push_back(cc); }

		size_t size() const { return code.size(); }

		void pop_back() { code.pop_back(); }

		bool operator==(Code const& other) const
		{ return code == other.code; }

		bool operator==(Code& other)
		{ return code == other.code; }
	};

	struct CodePrinter
	{
		typedef CodeBacker::iterator iterator;
		typedef CodeBacker::value_type value_type;

		Code& code;

		int pushing;
		size_t pos;
		value_type vv;

		CodePrinter(Code& code_)
			: code(code_)
			, pushing(0)
			, pos(0)
		{}

		std::ostream& line(std::ostream &out)
		{
			auto vname = vm_codes::name_or_null(vv);

			out << " " << pos << ": ";

			if(pushing)
				{
					--pushing;
					out << "@" << vv;
				}
			else
				{
					if(vv == vm_codes::Tag<vm_codes::push>::value)
						++pushing;
					out << vname;
				}

			auto sym_names = code.offset_table.symbols_at(pos);
			if(!sym_names.empty())
				{
					out << "\t\t# " << sym_names.begin()->second;
					for(auto name : make_range(++sym_names.begin(), sym_names.end()))
						out << ", " << name.second;
				}
			return out;
		}

		void print(std::ostream &out)
		{
			pos = 0;
			pushing = 0;

			for(auto& vv_ : code)
				{
					vv = vv_;
					line(out) << std::endl;
					++pos;
				}
		}

		void dbg() { print(std::cout); }
	};

 	struct AssembleCode
	{
		typedef uintptr_t value_type;
		typedef Code::const_iterator const_iterator;

		Code *code;

		AssembleCode() = default;
		AssembleCode(Code *code_) : code(code_) {}

#define M(r, data, instruction) AssembleCode& instruction() {             \
			_push_back(vm_codes::Tag<vm_codes::instruction>::value); \
			return *this; \
		}
		BOOST_PP_SEQ_FOR_EACH(M, _, ATL_ASSEMBLER_NORMAL_BYTE_CODES)
#undef M

		void _push_back(uintptr_t cc) { code->push_back(cc); }

		AssembleCode& patch(pcode::Offset offset, pcode::value_type value)
		{
			*(code->begin() + offset) = value;
			return *this;
		}

		AssembleCode& constant(uintptr_t cc)
		{
			push();
			_push_back(cc);
			return *this;
		}

		AssembleCode& pointer(void const* cc) { return constant(reinterpret_cast<value_type>(cc)); }

		AssembleCode& push_tagged(const Any& aa)
		{
			constant(aa._tag);
			return pointer(aa.value);
		}

		pcode::Offset pos_last() { return code->size() - 1;}
		pcode::Offset pos_end() { return code->size(); }

		/* The `offset`th element from the end.  Last element is 0. */
		AssembleCode& closure_argument(size_t offset)
		{
			constant(offset);
			return closure_argument();
		}

		AssembleCode& define(size_t slot)
		{
			constant(slot);
			if(slot + 1 > code->num_slots)
				{ code->num_slots = slot + 1; }

			_push_back(vm_codes::Tag<vm_codes::define>::value);
			return *this;
		}

		AssembleCode& deref_slot(size_t slot)
		{
			constant(slot);
			_push_back(vm_codes::Tag<vm_codes::deref_slot>::value);
			return *this;
		}

		AssembleCode& make_closure(size_t formals, size_t captured)
		{
			constant(formals);
			constant(captured);
			return make_closure();
		}

		/* Counts back from the function. Offset 0 is the first argument. */
		AssembleCode& argument(uintptr_t offset)
		{
			constant(offset);
			return argument();
		}

		AssembleCode& nested_argument(uintptr_t offset, uintptr_t hops)
		{
			constant(offset);
			constant(hops);
			return nested_argument();
		}

		AssembleCode& std_function(CxxFunctor::value_type* fn, size_t arity)
		{
			constant(arity);
			pointer(reinterpret_cast<void*>(fn));
			return std_function();
		}

		value_type& operator[](off_t pos) { return *(code->begin() + pos); }

		size_t label_pos(std::string const& name)
		{ return code->offset_table[name]; }

		/* Add a label to the current pos_last. */
		AssembleCode& add_label(std::string const& name)
		{
			code->offset_table.set(name, pos_end());
			return *this;
		}

		/* Insert the current pos_last + `offset` at the location indicated by `name`. */
		AssembleCode& constant_patch_label(std::string const& name)
		{
			patch(code->offset_table[name] + 1,
			      pos_end());
			return *this;
		}

		/* Insert label's value as a constant */
		AssembleCode& get_label(std::string const& name)
		{ return constant(code->offset_table[name]); }

		void dbg();
	};

	// define out here to avoid inlining
	void AssembleCode::dbg()
	{
		CodePrinter printer(*code);
		printer.dbg();
	}
}

// Set the flag to compile this as an app that just prints the byte
// code numbers (in hex) and the corrosponding name
#ifdef VM_PRINT_BYTE_CODES
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
