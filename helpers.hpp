#ifndef ATL_HELPERS_HH
#define ATL_HELPERS_HH

#include <algorithm>

#include "./gc.hpp"

namespace atl {
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
    }

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
    }

    namespace deep_copy {
	template<class Seq>
	void _to(Any seq, GC::DynamicVector& store, Any* root) {
	    using namespace flat_iterator;

	    auto input_root = begin(seq);
	    for(auto &vv : make_range(input_root,
				      end(seq))) {
		switch(vv._tag) {
		case tag<Ast>::value:
		case tag<Data>::value:
		case tag<Slice>::value:
		case tag<CxxArray>::value: {
		    store.push_seq<Seq>()->end_at(root + (&vv - input_root));
		    break;
		}
		case tag<Undefined>::value: {
		    // TODO: sorted _backtrack values for performance.
		    auto offset = &vv - input_root;
		    std::replace(unwrap<Undefined>(vv)._backtrack.begin(),
				 unwrap<Undefined>(vv)._backtrack.end(),
				 &vv,
				 root + offset);
		}
		default:
		    store.push_back(vv);
		}}}

	Ast& ast_from(Any seq, GC& gc) {
	    auto size = flat_iterator::const_end(seq) - flat_iterator::const_begin(seq);
	    auto out = gc.dynamic_vector(size + 2);
	    auto top = out->push_seq<Ast>();
	    deep_copy::_to<Ast>(seq,
				*out,
				out->begin());
	    top->end_at(out->end());
	    return *top;
	}

	Data& data_from(Any seq, GC& gc) {
	    auto size = flat_iterator::const_end(seq) - flat_iterator::const_begin(seq);
	    auto out = gc.dynamic_vector(size + 2);
	    auto top = out->push_seq<Data>();
	    deep_copy::_to<Data>(seq,
				 *out,
				 out->begin());
	    top->end_at(out->end());
	    return *top;
	}
    }
}


#endif
