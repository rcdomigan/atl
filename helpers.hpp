#ifndef ATL_HELPERS_HH
#define ATL_HELPERS_HH

#include <algorithm>

#include "./gc.hpp"
#include "./type.hpp"
#include "./conversion.hpp"

namespace atl
{
	typedef Range<Any*> AnyRange;

	namespace ast_iterator
	{
		typedef ::Range<Ast::iterator> Range;

		Ast::iterator begin(AnyRange& rr)
		{ return Ast::iterator(rr.begin()); }

		Ast::iterator end(AnyRange& rr)
		{ return Ast::iterator(rr.end()); }

		Ast::iterator begin(Range& rr)
		{ return Ast::iterator(rr.begin()); }

		Ast::iterator end(Range& rr)
		{ return Ast::iterator(rr.end()); }

		Ast::iterator begin(Ast& rr)
		{ return rr.begin(); }

		Ast::iterator end(Ast& rr)
		{ return rr.end(); }

		template<class T>
		size_t size(T const& seq)
		{ return seq.size(); }

		template<class T>
		Range range(T input)
		{ return make_range(begin(input), end(input)); }
	}


	namespace byte_code
	{
		typedef typename vm_stack::value_type value_type;
		template<class T>
		vm_stack::value_type to_bytes(T input)
		{ return reinterpret_cast<vm_stack::value_type>(input); }

		// TODO: use the `std::is_integral` and static cast for all integral (and floating?) types.
		value_type to_bytes(long input)
		{ return static_cast<value_type>(input); }

		value_type to_bytes(bool input)
		{ return static_cast<value_type>(input); }

		value_type to_bytes(void* input)
		{ return reinterpret_cast<value_type>(input); }

		value_type to_bytes(Pointer input)
		{ return reinterpret_cast<value_type>(input.value); }

		template<class R>
		struct PntrCaster
		{
			typedef PntrCaster<R> type;
			static R a(value_type input)
			{ return reinterpret_cast<R>(input); }
		};

		template<class I>
		struct StaticCaster
		{
			typedef StaticCaster<I> type;
			static I a(value_type input)
			{ return static_cast<I>(input); }
		};


		template<class T>
		struct Caster
			: public std::conditional<std::is_integral<T>::value,
			                          StaticCaster<T>,
			                          PntrCaster<T>
			                          >::type
		{};

		template<class R>
		R from_bytes(value_type input) { return Caster<R>::a(input); }
	}


	namespace make_ast
	{
		// Maintains one 'dynamic_seq` for use with the make_ast functions
		struct AstAllocator
		{
			AllocatorBase &allocator;
			AstSubstrate *seq_space;

			AstAllocator(AllocatorBase &aa)
				: allocator(aa), seq_space(&aa.sequence())
			{}

			Symbol* symbol(std::string const& name)
			{ return allocator.symbol(name); }

			AstAllocator& push_back(Any value)
			{
				seq_space->push_back(value);
				return *this;
			}

			MovableAstPointer nest_ast()
			{ return push_nested_ast(*seq_space); }

			size_t size()
			{ return seq_space->size(); }
		};

		AstAllocator ast_alloc(AllocatorBase& aa)
		{ return AstAllocator(aa); }

		typedef std::function<void (AstAllocator)> ast_composer;
		template<class T>
		ast_composer lift(T tt)
		{
			return [tt](AstAllocator space)
				{ space.push_back(wrap(tt)); };
		}

		template<class T>
		ast_composer lift()
		{
			return [](AstAllocator space)
				{ space.push_back(wrap<T>()); };
		}

		ast_composer sym(std::string const& name)
		{
			return [name](AstAllocator heap)
				{ heap.push_back(wrap(heap.symbol(name))); };
		}


		struct _Run
		{
			AstAllocator space;
			_Run(AstAllocator ss) : space(ss) {}

			template<class Fn>
			void operator()(Fn &fn) { fn(space); }
		};

		template<class ... Args>
		std::function<Ast* (AstAllocator)>
		make(Args ... args)
		{
			auto tup = make_tuple(args...);
			return [tup](AstAllocator space) -> Ast*
				{
					// ast_pos needs to be positional!! ITERATOR WON'T WORK!
					auto ast = space.nest_ast();
					_Run do_apply(space);

					foreach_tuple(do_apply, tup);
					ast.end_ast();
					return ast.pointer();
				};
		}
	}
}

#endif
