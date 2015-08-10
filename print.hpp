#ifndef ATL_PRINT_HH
#define ATL_PRINT_HH
/**
 * @file /home/ryan/programming/atl/print.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Aug 10, 2013
 */
#include <functional>
#include <iostream>
#include <utility>

#include "./utility.hpp"
#include "./helpers.hpp"

namespace atl {
    namespace printer {
	struct Printer {
	    virtual std::ostream& print(std::ostream&) const = 0;
	};

	template<class Container>
	struct PrintRange : public Printer {
	    Range<typename Container::const_iterator> _range;
	    char _delim_open, _delim_close;

	    PrintRange(const Container& range, char delim_open, char delim_close)
		: _range(range.begin(), range.end()),
		  _delim_open(delim_open),
		  _delim_close(delim_close) {}

	    virtual std::ostream& print(std::ostream&) const;
	};

	struct PrintAny : public Printer {
	    const Any &a;
	    PrintAny(const Any &a_) : a(a_) {}
	    virtual std::ostream& print(std::ostream&) const;
	};


	template<class Range>
	PrintRange<Range> range(const Range& rr, char open='(', char close=')') {
	    return PrintRange<Range>(rr, open, close);
	}

	PrintAny any(Any const& aa) { return PrintAny(aa); }

	void print_atom_debug(const Any& vv, std::ostream& out) {
	    out << "@" << &vv
		<< " [" << type_name(vv._tag) << ":" << vv.value << "]"
		<< endl;
	}

	std::ostream& operator<<(std::ostream& out, const printer::Printer& p) { return p.print(out); }

	template<class Container>
	std::ostream& PrintRange<Container>::print(std::ostream& out) const {
	    if(_range.empty())
		return out << _delim_open << _delim_close << endl;
	    else {
		out << _delim_open << printer::any(_range[0]) << flush;
		for(auto& vv : slice(_range, 1)) out << " " << printer::any(vv);
		return out << _delim_close;
	    }
	}

	std::ostream& PrintAny::print(std::ostream& out) const {
	    auto trim_addr = [](void *pntr) {
		return reinterpret_cast<long>(pntr) & (256 - 1);
	    };

	    using namespace std;
	    switch(a._tag) {
	    case tag<Undefined>::value:
                {
                    out << "#<Undefined ";
                    return out << ":" << hex << trim_addr(a.value) << ">";
                }

	    case tag<Symbol>::value:
                return out << "'" << unwrap<string>(a);
            case tag<Type>::value:
                {
                    out << "#{";
                    if(unwrap<Type>(a).value == nullptr)
                        return out << "???}";
                    else
                        return unwrap<Type>(a).value->print(out) << "}";
                }
	    case tag<Fixnum>::value:
		return out << value<Fixnum>(a);
	    case tag<Bool>::value:
		if(value<bool>(a))
		    return out << "True";
		else
		    return out << "False";
	    case tag<String>::value:
		return out << '"' << *reinterpret_cast<string*>(a.value) << '"';
	    case tag<CxxFunctor>::value:
		return out << unwrap<CxxFunctor>(a)._name;
	    case tag<Pointer>::value:
		if(a.value)
		    return out << "#<Pointer-" << hex << (reinterpret_cast<long>(a.value) & (256 - 1)) << ">";
		else
		    return out << "#<Pointer-NULL>";
	    case tag<Ast>::value:
		return out << range(unwrap<Ast>(a));
	    case tag<Slice>::value:
		return out << range(unwrap<Slice>(a), '[', ']');
	    case tag<Procedure>::value:
		return out << "#<\\ "
			   << ">" << flush;
	    default:
		return out << "#<" << type_name(a._tag) << ">";
	    }}
    }

    void dbg_any(Any vv)
    { cout << printer::any(vv) << endl; }

    void dbg_ast(Ast& vv)
    { cout << printer::range(vv) << endl; }

}


#endif
