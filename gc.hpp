#ifndef ATL_GC_HH
#define ATL_GC_HH

// @file /home/ryan/programming/atl/gc.hpp
// @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
// Created on Jan 08, 2014
//
// A garbage collected environment.

#include <limits>
#include <algorithm>
#include <functional>
#include <list>
#include <memory>
#include <iterator>

#include <boost/mpl/map.hpp>

#include "./debug.hpp"

namespace atl
{
	using namespace std;

	/*****************/
	/*	 ____  ____  */
	/*	/ ___|/ ___| */
	/* | |  _| |     */
	/* | |_| | |___  */
	/*	\____|\____| */
	/*****************/
	namespace memory_pool
	{
		template<class T>
		class Pool
		{
		private:
			static const size_t _size = 512;
			static const size_t num_mark_feilds = (_size / (sizeof(size_t) << 1));

			static const size_t _mask = numeric_limits<off_t>::digits >> 1;

			typedef size_t mark_t;

			// used to hold a mark/unmarked bit and a allocated/free bit
			mark_t _mark[num_mark_feilds];

			T *_begin, *_itr, *_end, *_free;
		public:
			static_assert( sizeof(T) >= sizeof(void*), "Can't build a pool _free list of types sized < void*");

			Pool() : _free(nullptr)
			{
				_itr = _begin = (T*) new char[sizeof(T) * _size];
				_end = _itr + _size;

				for(unsigned int i = 0; i < num_mark_feilds; ++i) _mark[i] = 0;
			}

			~Pool() { delete[] (char*)_begin; }

			T *begin() { return _begin; }
			T *end() { return _end; }

			// @param p: pointer to the object that needs marking
			// @return: true if the mark was needed, false if the object was already marked.
			bool mark(T *p)
			{
				off_t offset = reinterpret_cast<off_t>( p - _begin)
					, mark_bit = static_cast<off_t>(1) << ((offset & _mask) << 1);

				if( _mark[offset >> 5] &  mark_bit ) return false;

				_mark[offset >> 5] |= mark_bit;
				return true;
			}

			// allocates a T from the array, checking _free list first
			T* alloc()
			{
				T *tmp;
				if(_free != nullptr)
					{
						tmp = _free;
						_free = *reinterpret_cast<T**>(_free);
					}

				else if(_itr != _end)
					tmp = _itr++;

				else
					return nullptr;

				// mark that this as allocated for the sweep cycle
				off_t offset = reinterpret_cast<off_t>( tmp - _begin);
				_mark[offset >> 5] |= static_cast<off_t>(2) << ((offset & _mask) << 1);

				return tmp;
			}

			// delete all un-marked objects (and re-set mark flags)
			virtual unsigned int sweep()
			{
				unsigned int item = 0
					, swept = 0;

				for(unsigned int i = 0; i < num_mark_feilds; ++i)
					{
						for(unsigned int mask = 2 // allocated
							    ; mask		  // while the bits havn't shifted off
							    ; mask <<= 2
							    , ++item )
							{
								if( _mark[i] & mask ) {		  // is the element allocated
									if( _mark[i] & (mask >> 1)	) // yes; and it's marked. Just clear the bit
										_mark[i] ^= (mask >> 1);

									else {			  // allocated but not marked; collect
										_mark[i] ^= mask;

										_begin[item].~T();	  // call the destructor
										++swept;
										if((_begin + item + 1) == _itr)	 --_itr;

										else {
											*reinterpret_cast<T**>(_begin + item) = _free;
											_free = _begin + item;
										}}}
							}}
				cout << "GC: Sweep reclaimed " << swept << endl;
				return swept;
			}

			void print() {
				for(unsigned int i = 1; i <= num_mark_feilds; ++i) {
					cout << "[";
					print_binary( _mark[num_mark_feilds - i] );
					cout << "]" << endl;
				}
			}

			// test whether ptr is in this particular array
			int cmp(T *ptr) {
				if(ptr < _begin)  return -1;
				if(ptr >= _end)	 return 1;
				return 0; }
		};

		struct DynamicVector
		{
			typedef Any* iterator;
			typedef const Any* const_iterator;
			typedef Any value_type;

			Any *_buffer, *_end, *_buffer_end;

			DynamicVector(size_t initial_size)
			{
				_buffer = new Any[initial_size + 1];
				_buffer++;	// reserve for the array end pointer
				_end = _buffer;
				_buffer_end = _buffer + initial_size;
			}

			DynamicVector(const DynamicVector&) = default;

			iterator begin() { return _buffer; }
			iterator end() { return _end; }

			const_iterator begin() const { return _buffer; }
			const_iterator end() const { return _end; }

			Any& back() { return *(_end - 1); }

			template<class T>
			T* push_seq()
			{
				T *result = new (_end)T(_end + 1, _end + 1);
				_end += 2;
				return result;
			}

			void push_back(const Any& input)
			{
				*_end = input;
				++_end;
			}

			Any pop_back()
			{
				--_end;
				return _end[1];
			}
			void pop_back(size_t nn) { _end -= nn; }

			void resize(size_t n) { } // TODO

			DynamicVector& operator++()
			{
				++_end;
				return *this;
			}
			DynamicVector operator++(int)
			{
				DynamicVector vec = *this;
				++_end;
				return vec;
			}
			Any& operator*() { return *_end; }
			DynamicVector& operator+=(size_t n)
			{
				_end += n;
				return *this;
			}
			Any& operator[](size_t n) { return _end[n]; }

			std::back_insert_iterator<memory_pool::DynamicVector> back_insert_iterator()
			{
				return std::back_insert_iterator<memory_pool::DynamicVector>(*this);
			}
		};
	}


	// I would like some Asts generating functions to be able to use
	// either the GC or an Arena at runtime,
	struct AllocatorBase
	{
		virtual ~AllocatorBase() {}

		virtual memory_pool::DynamicVector* sequence() = 0;
		virtual Symbol* symbol(std::string const&) = 0;
	};


	// A mark-and-sweep GC. STUB
	class GC : public AllocatorBase
	{
	private:
		typedef std::list< Any > MarkType;
		MarkType _mark;
	private:
		vector< function<void (GC&)> > _mark_callbacks;

		void unregister(MarkType::iterator itr) { _mark.erase(itr); }

		memory_pool::Pool< Undefined > _undefined_heap;
		memory_pool::Pool< Procedure > _procedure_heap;
		memory_pool::Pool< String > _string_heap;
		memory_pool::Pool< CxxFunctor > _primitive_recursive_heap;
		memory_pool::Pool< Symbol > _symbol_heap;
		memory_pool::Pool< Parameter > _Parameter_heap;
		memory_pool::Pool< Slice > _slice_heap;
		memory_pool::Pool< abstract_type::Type > _abstract_type_heap;

		template< class T,  memory_pool::Pool<T> GC::*member >
		struct MemberPtr {
			typedef memory_pool::Pool<T> GC::* PoolType;
			/* man this would be easier with inline definitions. */
			const static PoolType value;
		};

		typedef mpl::map< mpl::pair< Undefined , MemberPtr<Undefined, &GC::_undefined_heap > >
		                  , mpl::pair< Procedure , MemberPtr<Procedure, &GC::_procedure_heap > >
		                  , mpl::pair< String , MemberPtr<String, &GC::_string_heap > >
		                  , mpl::pair< CxxFunctor,
		                               MemberPtr<CxxFunctor, &GC::_primitive_recursive_heap > >
		                  , mpl::pair< Symbol,	MemberPtr<Symbol, &GC::_symbol_heap > >
		                  , mpl::pair< Parameter,  MemberPtr<Parameter, &GC::_Parameter_heap > >
		                  , mpl::pair< Slice,  MemberPtr<Slice, &GC::_slice_heap > >
		                  , mpl::pair< abstract_type::Type, MemberPtr<abstract_type::Type, &GC::_abstract_type_heap> >
		                  > PoolMap;

		template<class T>
		T* alloc_from(memory_pool::Pool<T> &pool)
		{
			T* result = pool.alloc();

			if(result == nullptr) {
				mark_and_sweep();
				result = pool.alloc();
				if(result == nullptr)
					throw string("out of memory");
			}

			return result;
		}
	public:
		typedef std::vector<pcode::value_type> PCodeAccumulator;
		std::vector<PCodeAccumulator> code_blocks;

		void mark_and_sweep()
		{
			for(auto i : _mark_callbacks)
				i(*this);

			_undefined_heap.sweep();
			_procedure_heap.sweep();
			_string_heap.sweep();
		}

		void mark(Any a) {}

		// Adds callbacks which will be invoked during the mark phase of the GC.
		// @param fn: the callback
		void mark_callback(function<void (GC&)> fn)
		{ _mark_callbacks.push_back(fn); }

		/*****************************/
		/**  __	 __	  _	    **/
		/** |  \/  | __ _| | _____  **/
		/** | |\/| |/ _` | |/ / _ \ **/
		/** | |	 | | (_| |   <	__/ **/
		/** |_|	 |_|\__,_|_|\_\___| **/
		/*****************************/
		template<class T>
		T* alloc()
		{
			static_assert( mpl::has_key<PoolMap, T>::value,
			               "GC::Type does not have corrosponding pool." );
			return alloc_from( (this->*mpl::at<PoolMap,T>::type::value) );
		}

		PCodeAccumulator& alloc_pcode()
		{
			code_blocks.emplace_back();
			return code_blocks.back();
		}

		template<class Type, class ... Types>
		Type* make(Types ... args)
		{
			return new ( alloc<Type>() ) Type (args...);
		}

		template<class Type, class ... Types>
		Any amake(Types ... args)
		{
			return Any( tag<Type>::value , make<Type>(args...));
		}

		memory_pool::DynamicVector* dynamic_seq(size_t initial_size = 100)
		{
			using DynamicVector = memory_pool::DynamicVector;
			return new DynamicVector(initial_size);
		}

		virtual memory_pool::DynamicVector* sequence() override
		{ return dynamic_seq(); }

		virtual Symbol* symbol(std::string const& name) override
		{ return make<Symbol>(name); }
	};

	template< class T,	memory_pool::Pool<T> GC::*member >
	const typename GC::MemberPtr<T,member>::PoolType GC::MemberPtr<T,member>::value = member;

	// Uses GC to mark each argument of this function.
	//
	// @tparam Types: types of the args
	// @param args: args to be marked
	void mark_args(GC &gc) {}

	template<class T, class  ... Types>
	void mark_args(GC &gc, T a, Types ... as)
	{
		gc.mark( a );
		mark_args(gc, as...);
	}

	// A GC which collects all allocated objects when it goes out of
	// scope. <STUB>
	struct Arena : public AllocatorBase
	{
		memory_pool::DynamicVector* dynamic_seq(size_t size = 100)
		{ return new memory_pool::DynamicVector(100); }

		template<class Type, class ... Types>
		Type* make(Types ... args)
		{ return new Type (args...); }

		template<class Type, class ... Types>
		Any amake(Types ... args)
		{ return Any( tag<Type>::value , make<Type>(args...)); }

		virtual memory_pool::DynamicVector* sequence() override
		{ return dynamic_seq(); }

		virtual Symbol* symbol(std::string const& name) override
		{ return make<Symbol>(name); }
	};
}

namespace std
{
	template<>
	struct iterator_traits<atl::memory_pool::DynamicVector>
	{
		typedef size_t difference_type;
		typedef atl::Any value_type;
		typedef atl::Any* pointer;
		typedef atl::Any& reference;
		typedef output_iterator_tag iterator_category;
	};
}
#endif
