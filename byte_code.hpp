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
//   call_procedure     : [arg1]...[argN][procedure-address]
//   return_            : [return-value][number-of-arguments-to-procedure]
//   argument           : [offset]
//   nested_argument    : [offset][hops]
//   tail_call          : [arg0]..[argN][N][procedure-address]
//   call_closure       : [arg0]..[argN][closure-address]
//   closure_argument   : [arg-offset]
//   finish             : -


#define ATL_NORMAL_BYTE_CODES (nop)(push)(pop)(if_)(std_function)(jump)(return_)(call_procedure)(argument)(nested_argument)(tail_call)(call_closure)(closure_argument)
#define ATL_BYTE_CODES (finish)(push_word)ATL_NORMAL_BYTE_CODES

#define ATL_VM_SPECIAL_BYTE_CODES (finish)      // Have to be interpreted specially by the run/switch statement
#define ATL_VM_NORMAL_BYTE_CODES ATL_NORMAL_BYTE_CODES

#define ATL_ASSEMBLER_SPECIAL_BYTE_CODES (push) // overloads the no argument version
#define ATL_ASSEMBLER_NORMAL_BYTE_CODES (finish)ATL_NORMAL_BYTE_CODES


namespace atl
{
	struct Closure
	{
		typedef std::vector<pcode::value_type> Values;
		Values values;
		pcode::Offset body;
	};

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
	struct WrapCodeForAssembler
	{
		typedef CodeBacker::iterator iterator;
		virtual void push_back(uintptr_t cc)=0;
		virtual size_t size()=0;
		virtual iterator begin()=0;
	};


	struct Code
		: public WrapCodeForAssembler
	{
		typedef typename pcode::value_type value_type;
		typedef typename CodeBacker::iterator iterator;
		typedef typename CodeBacker::const_iterator const_iterator;


		OffsetTable offset_table;
		CodeBacker code;

		Code() = default;
		Code(size_t initial_size) : code(initial_size) {}

		/** Takes code and prepares it for appending. Strip off terminating
		 * 'finish' instructions, strip that off during and add it back
		 * when the code is passed back out with return_code.
		 *
		 * @param input: Pointer to the code we're modifying
		 */
		void take_code(Code const& input)
		{
			offset_table = input.offset_table;
			code = input.code;
			if(!code.empty() && code.back() == vm_codes::values::finish)
				code.pop_back();
		}

		bool has_main() const { return offset_table.table.count("main") != 0; }
		size_t main_entry_point() const
		{ return offset_table.table.at("__call-main__"); }

		virtual iterator begin() override { return code.begin(); }
		iterator end() { return code.end(); }

		virtual void push_back(uintptr_t cc) override
		{ code.push_back(cc); }

		virtual size_t size() override { return code.size(); }

		void dbg() const;
		void print(std::ostream &) const;
	};

	void Code::print(std::ostream &out) const
	{
		int pushing = 0;
		size_t pos = 0;

		for(auto& vv : code)
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

				auto sym_names = offset_table.symbols_at(pos);
				if(!sym_names.empty())
					{
						out << "\t\t# " << sym_names.begin()->second;
						for(auto name : make_range(++sym_names.begin(), sym_names.end()))
							out << ", " << name.second;
					}


				out << std::endl;


				++pos;
			}
		out << "<--- done printing code --->" << std::endl;
	}

	void Code::dbg() const
	{ print(std::cout); }


	struct WrapCodeIter
		: public WrapCodeForAssembler
	{
		typedef WrapCodeForAssembler::iterator iterator;
		iterator _begin, itr;

		WrapCodeIter(iterator begin_) : _begin(begin_), itr(begin_) {}

		virtual void push_back(uintptr_t cc) override
		{
			*itr = cc;
			++itr;
		}

		virtual size_t size() override { return itr - _begin; }
		virtual iterator begin() override { return _begin; }
	};

	struct AssembleCode
	{
		typedef uintptr_t value_type;
		typedef Code::const_iterator const_iterator;

		WrapCodeForAssembler *output;

		AssembleCode() = default;
		AssembleCode(WrapCodeForAssembler *output_) : output(output_) {}

#define M(r, data, instruction) AssembleCode& instruction() {             \
			_push_back(vm_codes::Tag<vm_codes::instruction>::value); \
			return *this; \
		}
		BOOST_PP_SEQ_FOR_EACH(M, _, ATL_ASSEMBLER_NORMAL_BYTE_CODES)
#undef M

		void _push_back(uintptr_t cc) { output->push_back(cc); }

		AssembleCode& constant(uintptr_t cc)
		{
			push();
			_push_back(cc);
			return *this;
		}

		AssembleCode& pointer(void* cc) { return constant(reinterpret_cast<value_type>(cc)); }

		AssembleCode& push_tagged(const Any& aa)
		{
			constant(aa._tag);
			return pointer(aa.value);
		}

		pcode::Offset pos_last() { return output->size() - 1;}
		pcode::Offset pos_end() { return output->size(); }


		// Takes the offset to the body
		AssembleCode& call_procedure(pcode::Offset body)
		{
			constant(reinterpret_cast<value_type>(body));
			return call_procedure();
		}

		AssembleCode& call_closure(Closure& closure)
		{
			constant(reinterpret_cast<value_type>(&closure));
			return call_closure();
		}

		AssembleCode& closure_argument(size_t offset)
		{
			constant(offset);
			return closure_argument();
		}

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

		AssembleCode& return_(size_t num_args)
		{
			constant(num_args);
			return return_();
		}

		AssembleCode& tail_call(size_t num_args, size_t proc)
		{
			constant(num_args);
			constant(proc);
			return tail_call();
		}

		AssembleCode& std_function(CxxFunctor::value_type* fn, size_t arity)
		{
			constant(arity);
			pointer(reinterpret_cast<void*>(fn));
			return std_function();
		}

		/* We don't yet know the value of a symbol so chuck enough data in that we can backpatch later */
		AssembleCode& pad_for_backpatch()
		{
			// For now it'll be a push and a call.
			nop();
			nop();
			nop();
			return *this;
		}

		value_type& operator[](off_t pos) { return *(output->begin() + pos); }
	};
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
