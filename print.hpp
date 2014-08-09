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
	PrintAny any(const Any& aa) { return PrintAny(aa); }

	void print_atom_debug(const Any& vv, std::ostream& out) {
	    out << "@" << &vv
		<< " [" << vv._tag << ":" << vv._value << "]"
		<< endl;
	}

	struct DebugRange : public Printer {
	    Range<const Any*> _rr;
	    DebugRange(Range<const Any*> rr) : _rr(rr) {}

	    virtual std::ostream& print(std::ostream& out) const {
		for(auto& vv: _rr)
		    print_atom_debug(vv, out);
		return out;
	    }
	};

	template<class Range>
	DebugRange debug_range(const Range& rr) {
	    return DebugRange(make_range(rr));
	}

	DebugRange debug_range(const Any& rr) {
	    using namespace flat_iterator;
	    return DebugRange(make_range(const_begin(rr), const_end(rr)));
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
	    using namespace std;
	    switch(a._tag) {
	    case tag<Symbol>::value:
		return out << "'" << unwrap<string>(a);
	    case tag<Fixnum>::value:
		return out << unwrap<long>(a);
	    case tag<Bool>::value:
		if(unwrap<bool>(a))
		    return out << "True";
		else
		    return out << "False";
	    case tag<String>::value:
		return out << '"' << *reinterpret_cast<string*>(a._value) << '"';
	    case tag<PrimitiveRecursive>::value:
		return out << unwrap<PrimitiveRecursive>(a)._name;
	    case tag<Pointer>::value:
		if(a._value)
		    return out << "#<Pointer-" << hex << (reinterpret_cast<long>(a._value) & (256 - 1)) << ">";
		else
		    return out << "#<Pointer-NULL>";
	    case tag<Data>::value:
		return out << "'" << range(unwrap<Data>(a));
	    case tag<Ast>::value:
		return out << range(unwrap<Ast>(a));
	    case tag<Slice>::value:
		return out << range(unwrap<Slice>(a), '[', ']');
	    case tag<CxxArray>::value:
		return out << range(unwrap<CxxArray>(a));
		return out << "#" << range(unwrap<Data>(a));
	    case tag<Procedure>::value:
		return out << "#<\\ "
			   << unwrap<Procedure>(a)._num_params
			   << ">" << flush;
	    default:
		return out << "#<" << type_name(a._tag) << ">";
	    }}
    }
}


#endif
