#ifndef ATL_HELPERS_HH
#define ATL_HELPERS_HH

#include <algorithm>

#include "./gc.hpp"
#include "./type.hpp"
#include "./conversion.hpp"

namespace atl
{
	typedef Range<Any*> AnyRange;

	namespace flat_iterator
	{
		const Any* const_begin(const Any aa) {
			switch(aa._tag) {
			case tag<Slice>::value:
				return unwrap<Slice>(aa)._begin;
			case tag<Ast>::value:
				return &*unwrap<Ast>(aa).begin();
			}
			throw std::string("Can only get the Ast iterators for certain types");
		}

		const Any* const_end(const Any aa) {
			switch(aa._tag) {
			case tag<Slice>::value:
				return unwrap<Slice>(aa)._end;
			case tag<Ast>::value:
				return &*unwrap<Ast>(aa).end();
			}
			throw std::string("Can only get the Ast iterators for certain types");
		}

		Any* begin(Any aa) { return const_cast<Any*>(const_begin(aa)); }
		Any* end(Any aa) { return const_cast<Any*>(const_end(aa)); }

		Any* begin(const Range<Any*>& input) { return input.begin(); }
		Any* end(const Range<Any*>& input) { return input.end(); }


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

		template<class T>
		size_t size(const T& input) { return end(input) - begin(input); }
	}

	namespace ast_iterator
	{
		typedef ::Range<Ast::iterator> Range;

		Ast::const_iterator const_begin(const Any aa) {
			switch(aa._tag) {
			case tag<Slice>::value:
				return unwrap<Slice>(aa).begin();
			case tag<Ast>::value:
				return unwrap<Ast>(aa).begin();
			}
			throw std::string("Can only get the Ast iterators for certain types");
		}

		Ast::const_iterator const_end(const Any aa) {
			switch(aa._tag) {
			case tag<Slice>::value:
				return const_cast<const Slice&&>(unwrap<Slice>(aa)).end();
			case tag<Ast>::value:
				return unwrap<Ast>(aa).end();
			}
			throw std::string("Can only get the Ast iterators for certain types");
		}

		Ast::const_iterator const_begin(const AnyRange& rr) {
			return Ast::const_iterator(rr.begin());
		}

		Ast::const_iterator const_end(const AnyRange& rr) {
			return Ast::const_iterator(rr.end());
		}

		Ast::const_iterator const_begin(const ::Range<const Any*>& rr) {
			return Ast::const_iterator(rr.begin());
		}

		Ast::const_iterator const_end(const ::Range<const Any*>& rr) {
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

		template<class T>
		Range range(T input)
		{ return make_range(begin(input), end(input)); }
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
					{
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
				case tag<Slice>::value:
					{
						_to<Seq>(make_range(begin(*itr),
						                    end(*itr)),
						         store);
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
			auto out = gc.dynamic_seq(100);
			deep_copy::_to<Seq>(input, *out);
			return unwrap<Seq>(*out->begin());
		}

		template<class Seq, class Fn>
		Any* _map(Fn& fn, Range<Any*> seq, GC::DynamicVector& store) {
			using namespace flat_iterator;

			auto frame = store.push_seq<Seq>();
			auto itr = seq.begin();

			while(itr != seq.end())
				{
					switch(itr->_tag)
						{
						case tag<Ast>::value:
							{
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
						case tag<Slice>::value:
							{
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
			auto out = gc.dynamic_seq(100);
			deep_copy::_map<Seq, Fn>(fn, input, *out);
			return unwrap<Seq>(*out->begin());
		}
	}

	namespace byte_code {
		typedef typename vm_stack::value_type value_type;
		template<class T>
		vm_stack::value_type to_bytes(T input)
		{
			return reinterpret_cast<vm_stack::value_type>(input);
		}

		// TODO: use the `std::is_integral` and static cast for all integral (and floating?) types.
		value_type to_bytes(long input)
		{
			return static_cast<value_type>(input);
		}

		value_type to_bytes(bool input)
		{
			return static_cast<value_type>(input);
		}


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
}

#endif
