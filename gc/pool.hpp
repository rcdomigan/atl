#ifndef ATL_GC_POOL_HPP
#define ATL_GC_POOL_HPP

#include <limits>
#include <atl/debug.hpp>

namespace atl
{
	namespace memory_pool
	{
		// I want to use this to figure out how many bits are needed
		// to represent a number (ie, 3 bits for 8)
		template<class Int>
		constexpr Int _log_of_power2(Int value)
		{ return (value == 0) ? 0 : _log_of_power2(value >> 1) + 1; }

		template<class Int>
		constexpr Int log_of_power2(Int value)
		{
			// To represent 8 values I'm asking for 0 to 7
			return _log_of_power2(value - 1);
		}

		template<class T,
		         // The allocated/marked bits are masked onto integral
		         // type Bits.  These are template parameters so small
		         // values can be used for debugging.
		         class _Bits=size_t,
		         size_t _POOL_SIZE=512>
		struct Pool
		{
			typedef _Bits Bits;
			static_assert(std::is_unsigned<Bits>::value,
			              "Pool's Bits type must be unsigned");

			// Number of Bits fields it would take to annotate the
			// array of T[_size].
			//
			// We two bits per item because we need to track if it's
			// alloated or free (so we can tell if something needs to
			// be swept/destructed) and a mark bit.
			static const size_t POOL_SIZE = _POOL_SIZE;
			static_assert
			(((POOL_SIZE % (sizeof(Bits) << 1)) == 0),
			 "THe bit-size of the Bits field should divide evenly into the Pool size");

			static const size_t FIELD_SIZE = std::numeric_limits<Bits>::digits;

			// Number of alloc/mark pairs a Bits field can hold
			static const size_t BITS_SIZE = FIELD_SIZE >> 1;

			// number of bits it takes to address each alloc/mark pair
			// of bit in Bits
			static const size_t BITS_ADDRESS_SIZE = log_of_power2(BITS_SIZE);

			static const size_t FIELDS = (POOL_SIZE / BITS_SIZE);

			// If I want the bits for a particular item in my Pool,
			// given its offset, I need to right shift the offset by
			// INDEX_PART_SHIFT to determine the appropriate index
			// into the bits array, and apply the FIELD_PART_MASK to
			// the offset to get the particular bits of interest
			// within the value at the index.
			static const Bits INDEX_PART_SHIFT = BITS_ADDRESS_SIZE;
			static const Bits FIELD_PART_MASK = std::numeric_limits<Bits>::max() >> (FIELD_SIZE - BITS_ADDRESS_SIZE);

			// Get the mask for the offset's mark bit in a field.
			static Bits _mark_bit(Bits offset)
			{ return (Bits)2 << ((FIELD_PART_MASK & offset) << (Bits)1); }

			static Bits _alloc_bit(Bits offset)
			{ return (Bits)1 << ((FIELD_PART_MASK & offset) << (Bits)1); }

			// Get the index of the field the offset's mark is in.
			static size_t _field(Bits offset)
			{ return offset >> INDEX_PART_SHIFT; }

			bool is_marked(size_t offset)
			{ return _bits[_field(offset)] & _mark_bit(offset); }

			bool is_allocated(size_t offset)
			{ return _bits[_field(offset)] & _alloc_bit(offset); }

			bool set_mark(size_t offset)
			{ return _bits[_field(offset)] |= _mark_bit(offset); }

			bool unset_mark(size_t offset)
			{ return _bits[_field(offset)] &= ~_mark_bit(offset); }

			bool set_allocated(size_t offset)
			{ return _bits[_field(offset)] |= _alloc_bit(offset); }

			bool set_allocated(T* pointer)
			{ return set_allocated(pointer - _begin); }

			bool unset_allocated(size_t offset)
			{ return _bits[_field(offset)] &= ~_alloc_bit(offset); }

			bool is_marked(T* pointer)
			{ return is_marked(pointer - _begin); }

			bool is_allocated(T* pointer)
			{ return is_allocated(pointer - _begin); }


			// used to hold a mark/unmarked bit and a allocated/free bit
			Bits _bits[FIELDS];

			T *_begin, *_itr, *_end, *_free;

			static_assert(sizeof(T) >= sizeof(void*), "Can't build a pool _free list of types sized < void*");

			Pool() : _free(nullptr)
			{
				/* allocate chars since I don't want T's constructor called on the elements */
				_itr = _begin = (T*) new char[sizeof(T) * POOL_SIZE];
				_end = _itr + POOL_SIZE;

				for(unsigned int i = 0; i < FIELDS; ++i) _bits[i] = 0;
			}

			~Pool()
			{
				// Use the 'mark' bit to indicate which cells are
				// free, and shouldn't be destructed

				// Destruct anything that's not free
				for(size_t i = 0; i < POOL_SIZE; ++i)
					{ if(is_allocated(i)) { _begin[i].~T(); } }

				delete[] (char*)_begin;
			}

			T *begin() { return _begin; }
			T *end() { return _end; }

			// @param p: pointer to the object that needs marking
			void mark(T *p)
			{ set_mark(p - _begin); }

			// allocates a T from the array, checking _free list
			// first.
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

				set_allocated(tmp);
				return tmp;
			}

			void free(T* pointer)
			{
				auto offset = pointer - _begin;
				pointer->~T();	  // call the destructor
				unset_allocated(offset);
				unset_mark(offset);

				if((pointer + 1) == _itr)
					{ --_itr; }
				else {
					*reinterpret_cast<T**>(pointer) = _free;
					_free = pointer;
				}
			}

			// delete all un-marked objects (and re-set mark flags)
			virtual unsigned int sweep()
			{
				unsigned int swept = 0;

				for(unsigned int i = 0; i < POOL_SIZE; ++i)
				{
					if(is_allocated(i))
						{
							if(is_marked(i))
								{ unset_mark(i); }
							else
								{
									free(_begin + i);
									++swept;
								}
						}
				}

				return swept;
			}

			void print() {
				for(unsigned int i=0; i < FIELDS; ++i) {
					std::cout << "[";
					print_binary( _bits[i] );
					std::cout << "]" << std::endl;
				}
			}

			// test whether ptr is in this particular array
			int cmp(T *ptr) {
				if(ptr < _begin)  return -1;
				if(ptr >= _end)	 return 1;
				return 0;
			}

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
			{ return size() - num_free(); }
		};
	}
}

#endif
