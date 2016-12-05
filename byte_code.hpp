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
//   call_procedure     : [arg1]...[argN][procedure-address]
//   return_            : [return-value][number-of-arguments-to-procedure]
//   argument           : [offset]
//   nested_argument    : [offset][hops]
//   tail_call          : [arg0]..[argN][N][procedure-address]
//   call_closure       : [arg0]..[argN][closure-address]
//   closure_argument   : [arg-offset]
//   make_closure       : [arg1]...[argN][N]
//   finish             : -


#define ATL_NORMAL_BYTE_CODES (nop)(push)(pop)(if_)(std_function)(jump)(return_)(call_procedure)(argument)(nested_argument)(tail_call)(call_closure)(closure_argument)(make_closure)
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

		OffsetTable offset_table;

		virtual void push_back(uintptr_t cc)=0;
		virtual size_t size() const=0;
		virtual iterator begin()=0;
		virtual iterator end()=0;
	};


	struct CodePrinter
	{
		typedef CodeBacker::iterator iterator;
		typedef CodeBacker::value_type value_type;

		WrapCodeForAssembler& code;

		int pushing;
		size_t pos;
		value_type vv;

		CodePrinter(WrapCodeForAssembler& code_)
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


	struct Code
		: public WrapCodeForAssembler
	{
		typedef typename pcode::value_type value_type;
		typedef typename CodeBacker::iterator iterator;
		typedef typename CodeBacker::const_iterator const_iterator;


		CodeBacker code;

		Code() = default;
		Code(size_t initial_size) : code(initial_size) {}

		bool has_main() const { return offset_table.table.count("main") != 0; }
		size_t main_entry_point() const
		{ return offset_table.table.at("__call-main__"); }

		virtual iterator begin() override { return code.begin(); }
		virtual iterator end() override { return code.end(); }

		const_iterator begin() const { return code.begin(); }
		const_iterator end() const { return code.end(); }

		virtual void push_back(uintptr_t cc) override
		{ code.push_back(cc); }

		virtual size_t size() const override { return code.size(); }

		void pop_back() { code.pop_back(); }

		bool operator==(Code const& other) const
		{ return code == other.code; }

		bool operator==(Code& other)
		{ return code == other.code; }
	};


	struct WrapCodeItr
		: public WrapCodeForAssembler
	{
		typedef WrapCodeForAssembler::iterator iterator;
		iterator _begin, _end, itr;

		WrapCodeItr(iterator begin_, iterator end_)
			: _begin(begin_)
			, _end(end_)
			, itr(begin_)
		{}

		WrapCodeItr(WrapCodeForAssembler& other)
			: WrapCodeItr(other.begin(), other.end())
		{}

		void set_position(size_t offset)
		{ itr = _begin + offset; }

		virtual void push_back(uintptr_t cc) override
		{
			assert(itr < _end);
			*itr = cc;
			++itr;
		}

		virtual size_t size() const override { return itr - _begin; }

		virtual iterator begin() override { return _begin; }
		virtual iterator end() override { return _end; }
	};

 	struct AssembleCode
	{
		typedef uintptr_t value_type;
		typedef Code::const_iterator const_iterator;

		WrapCodeForAssembler *code;

		AssembleCode() = default;
		AssembleCode(WrapCodeForAssembler *code_) : code(code_) {}

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


		// Takes the offset to the body
		AssembleCode& call_procedure(pcode::Offset body)
		{
			constant(reinterpret_cast<value_type>(body));
			return call_procedure();
		}

		/* The `offset`th element from the end.  Last element is 0. */
		AssembleCode& closure_argument(size_t offset)
		{
			constant(offset);
			return closure_argument();
		}

		AssembleCode& make_closure(size_t count)
		{
			constant(count);
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

		value_type& operator[](off_t pos) { return *(code->begin() + pos); }

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

		// implementation requires the compiler
		virtual AssembleCode& needs_patching(Symbol&)
		{ throw WrongTypeError("Basic AssembleCode can't patch"); }

		virtual AssembleCode& patch(Symbol&)
		{ throw WrongTypeError("Basic AssembleCode can't patch"); }

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
