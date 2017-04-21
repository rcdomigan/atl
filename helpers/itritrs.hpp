#ifndef ATL_ITRITRS_HPP
#define ATL_ITRITRS_HPP

#include <atl/utility.hpp>
#include <atl/is.hpp>
#include <atl/wrap.hpp>
#include <atl/gc/gc.hpp>

/* Small Ast helpers with minimal external dependencies */
namespace atl
{
	// An iterator like wrapper around Ast's iterator like
	// Ast::iterator.  This way I can use the for(auto foo : ast)
	// syntax and have each 'foo' be an Ast::iterator
	struct ItrItr
	{
		typedef typename Ast::iterator value_type;
		value_type itr;

		ItrItr(Ast::iterator itr_) : itr(itr_) {}

		ItrItr& operator++()
		{
			++itr;
			return *this;
		}

		ItrItr operator+(size_t n) const
		{
			ItrItr rval = *this;
			for(size_t i=0; i < n; ++i) { ++rval; }
			return rval;
		}

		bool operator==(ItrItr const& other) const
		{ return itr == other.itr; }

		bool operator!=(ItrItr const& other) const
		{ return itr != other.itr; }

		value_type& operator*() { return itr; }
		value_type* operator->() { return &itr; }
	};

	Range<ItrItr> itritrs(Ast::Subex subex)
	{ return make_range(ItrItr(subex.begin()), ItrItr(subex.end())); }

	Range<ItrItr> itritrs(Range<Ast::iterator>& ast)
	{ return make_range(ItrItr(ast.begin()), ItrItr(ast.end())); }

	Range<ItrItr> itritrs(Marked<Ast>& ast)
	{ return make_range(ItrItr(ast->begin()), ItrItr(ast->end())); }

	Range<ItrItr> itritrs(Marked<Any>& value)
	{
		auto& ast = unwrap<Ast>(value.any);
		return make_range(ItrItr(ast.begin()), ItrItr(ast.end()));
	}
}

namespace std
{
	template<>
    struct iterator_traits<atl::ItrItr>
    {
	    typedef size_t difference_type;
	    typedef atl::Ast::iterator value_type;
	    typedef atl::Ast::iterator* pointer;
	    typedef atl::Ast::iterator& reference;
	    typedef forward_iterator_tag iterator_category;
    };
}

#endif
