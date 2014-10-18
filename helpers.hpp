#ifndef ATL_HELPERS_HH
#define ATL_HELPERS_HH

#include <algorithm>

#include "./gc.hpp"

namespace atl {
    typedef Range<Any*> AnyRange;

    namespace flat_iterator {
	const Any* const_begin(const Any aa) {
	    switch(aa._tag) {
	    case tag<CxxArray>::value:
		return unwrap<CxxArray>(aa)._begin;
	    case tag<Slice>::value:
		return unwrap<Slice>(aa)._begin;
	    case tag<Ast>::value:
	    case tag<Data>::value:
		return &*unwrap<Data>(aa).begin();
	    }
	    throw std::string("Can only get the Ast iterators for certain types");
	}

	const Any* const_end(const Any aa) {
	    switch(aa._tag) {
	    case tag<CxxArray>::value:
		return unwrap<CxxArray>(aa)._end;
	    case tag<Slice>::value:
		return unwrap<Slice>(aa)._end;
	    case tag<Ast>::value:
	    case tag<Data>::value:
		return &*unwrap<Data>(aa).end();
	    }
	    throw std::string("Can only get the Ast iterators for certain types");
	}

	Any* begin(Any aa) { return const_cast<Any*>(const_begin(aa)); }
	Any* end(Any aa) { return const_cast<Any*>(const_end(aa)); }

	Any* begin(Data aa) { return aa._value + 1; }
	Any* end(Data aa) { return reinterpret_cast<Any*>(aa._value->_value); }


	Range<Any*> range(Any input) {
	    return Range<Any*>(begin(input), end(input));
	}

	template<class Input>
	Range<Any*> range(const Input& input) {
	    return make_range(const_cast<Any*>(&*input.begin()),
			      const_cast<Any*>(&*input.end()));
	}

	Range<const Any*> const_range(Any input) {
	    return make_range(const_begin(input),
			      const_end(input));
	}

	template<class Input>
	Range<const Any*> const_range(const Input& input) {
	    return make_range(const_cast<const Any*>(&*input.begin()),
			      const_cast<const Any*>(&*input.end()));
	}
    }

    namespace ast_iterator {
	Ast::const_iterator const_begin(const Any aa) {
	    switch(aa._tag) {
	    case tag<CxxArray>::value:
		return Ast::const_iterator(unwrap<CxxArray>(aa).begin());
	    case tag<Slice>::value:
		return unwrap<Slice>(aa).begin();
	    case tag<Ast>::value:
	    case tag<Data>::value:
		return unwrap<Data>(aa).begin();
	    }
	    throw std::string("Can only get the Ast iterators for certain types");
	}

	Ast::const_iterator const_end(const Any aa) {
	    switch(aa._tag) {
	    case tag<CxxArray>::value:
		return Ast::const_iterator(unwrap<CxxArray>(aa).end());
	    case tag<Slice>::value:
		return const_cast<const Slice&&>(unwrap<Slice>(aa)).end();
	    case tag<Ast>::value:
	    case tag<Data>::value:
		return unwrap<Data>(aa).end();
	    }
	    throw std::string("Can only get the Ast iterators for certain types");
	}

	Ast::const_iterator const_begin(const AnyRange& rr) {
	    return Ast::const_iterator(rr.begin());
	}

	Ast::const_iterator const_end(const AnyRange& rr) {
	    return Ast::const_iterator(rr.end());
	}

	Ast::const_iterator const_begin(const Range<const Any*>& rr) {
	    return Ast::const_iterator(rr.begin());
	}

	Ast::const_iterator const_end(const Range<const Any*>& rr) {
	    return Ast::const_iterator(rr.end());
	}


	template<class T>
	Ast::iterator begin(T&& vv) { return Ast::iterator(flat_iterator::begin(vv)); }
	template<class T>
	Ast::iterator end(T&& vv) { return Ast::iterator(flat_iterator::end(vv)); }

	template<class T>
	size_t size(const T& seq) {
	    return const_end(seq) - const_begin(seq);
	}
    }

    namespace deep_copy {
	template<class Seq>
	Any* _to(Range<Any*> seq, GC::DynamicVector& store) {
	    using namespace flat_iterator;


	    auto frame = store.push_seq<Seq>();
	    auto itr = seq.begin();

	    while(itr != seq.end()) {
		switch(itr->_tag) {
		case tag<Ast>::value:
		case tag<Data>::value: {
		    auto end_inner = _to<Seq>(make_range(begin(*itr),
							 end(*itr)),
					      store);
		    if((end_inner < seq.end()) &&
		          (end_inner > itr))
			itr = end_inner;
		    else
			++itr;
		    continue;
		}
		case tag<Slice>::value: {
		    _to<Seq>(make_range(begin(*itr),
					end(*itr)),
			     store);
		    break;
		}
		case tag<Undefined>::value: {
		    unwrap<Undefined>(*itr)._backtrack.push_back(itr);
		    store.push_back(*itr);
		    break;
		}
		default:
		    store.push_back(*itr);
		}
		++itr;
	    }

	    frame->end_at(store.end());
	    return itr;
	}

	template<class Seq>
	Seq& to(const Range<Any*>& input, GC& gc) {
	    auto out = gc.dynamic_vector(100);
	    deep_copy::_to<Seq>(input, *out);
	    return unwrap<Seq>(*out->begin());
	}

	template<class Seq, class Fn>
	Any* _map(Fn& fn, Range<Any*> seq, GC::DynamicVector& store) {
	    using namespace flat_iterator;

	    auto frame = store.push_seq<Seq>();
	    auto itr = seq.begin();

	    while(itr != seq.end()) {
		switch(itr->_tag) {
		case tag<Ast>::value:
		case tag<Data>::value: {
		    auto end_inner = _map<Seq>(fn,
					       make_range(begin(*itr),
							  end(*itr)),
					       store);
		    if((end_inner < seq.end()) &&
		       (end_inner > itr))
			itr = end_inner;
		    else
			++itr;
		    continue;
		}
		case tag<Slice>::value: {
		    _map<Seq>(fn,
			      make_range(begin(*itr),
					 end(*itr)),
			      store);
		    break;
		}
		default:
		    store.push_back(fn(*itr));
		}
		++itr;
	    }

	    frame->end_at(store.end());
	    return itr;
	}

	template<class Seq, class Fn>
	Seq& map(Fn& fn, Range<Any*> input, GC& gc) {
	    auto out = gc.dynamic_vector(100);
	    deep_copy::_map<Seq, Fn>(fn, input, *out);
	    return unwrap<Seq>(*out->begin());
	}
    }
}


#endif
