#ifndef ATL_GC_POOL_HPP
#define ATL_GC_POOL_HPP

#include <limits>
#include <debug.hpp>

namespace atl
{
	namespace memory_pool
	{
		template<class T>
		class Pool
		{
		private:
			static const size_t _size = 512;
			static const size_t num_mark_feilds = (_size / (sizeof(size_t) << 1));

			static const size_t _mask = std::numeric_limits<off_t>::digits >> 1;

			typedef size_t mark_t;

			// used to hold a mark/unmarked bit and a allocated/free bit
			mark_t _mark[num_mark_feilds];

			T *_begin, *_itr, *_end, *_free;
		public:
			static_assert( sizeof(T) >= sizeof(void*), "Can't build a pool _free list of types sized < void*");

			Pool() : _free(nullptr)
			{
				/* allocate chars since I don't want T's constructor called on the elements */
				_itr = _begin = (T*) new char[sizeof(T) * _size];
				_end = _itr + _size;

				for(unsigned int i = 0; i < num_mark_feilds; ++i) _mark[i] = 0;
			}

			~Pool() { delete[] (char*)_begin; }

			T *begin() { return _begin; }
			T *end() { return _end; }

			// @param p: pointer to the object that needs marking
			// @return: true if the mark was needed, false if the
			// object was already marked (or couldn't be marked).
			bool mark(T *p)
			{
				if((p < _begin) || (p > _end)) return false;
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
				return swept;
			}

			void print() {
				for(unsigned int i = 1; i <= num_mark_feilds; ++i) {
					std::cout << "[";
					print_binary( _mark[num_mark_feilds - i] );
					std::cout << "]" << std::endl;
				}
			}

			// test whether ptr is in this particular array
			int cmp(T *ptr) {
				if(ptr < _begin)  return -1;
				if(ptr >= _end)	 return 1;
				return 0; }

			size_t size() const { return _itr - _begin; }
			size_t num_free() const
			{
				size_t counter = 0;
				auto itr = _free;
				while(itr)
					{
						++counter;
						itr = *reinterpret_cast<T**>(itr);
					}
				return counter;
			}
			size_t num_allocated() const
			{
				return size() - num_free();
			}
		};
	}
}

#endif
