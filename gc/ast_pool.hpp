#ifndef ATL_GC_AST_POOL_HPP
#define ATL_GC_AST_POOL_HPP

#include <atl/helpers/ast_access.hpp>

#include <list>

namespace atl
{
	namespace ast_pool_detail
	{
		/** \internal basic array wrapper which stores the pool of
		 *   Anys we will allocate Asts on top of. */
		struct Backer
		{
			Any *_begin, *_end, *_itr;
			Backer(size_t size)
			{
				_itr = _begin = new Any[size];
				_end = _begin + size;
			}

			~Backer()
			{ delete[] _begin; }

			bool is_contained(void* pointer)
			{ return (pointer >= _begin) && (pointer < _end); }

			size_t size() const { return _itr - _begin; }
			size_t reserved() const { return _end - _begin; }
			size_t remaining() const { return _end - _itr; }
		};

		// Copies the src range to the 'end' pointer of some
		// container, then moves the 'end' pointer the end of the copy
		void copy_to_end(Range<Ast::flat_iterator>& src, Any*& end)
		{
			std::memcpy(end, src.begin(), src.size() * sizeof(Any));
			end += src.size();
		}

		void copy_to_end(Range<Ast::flat_iterator>&& src, Any*& end)
		{ copy_to_end(src, end); }

		/* Descrives the portion of some array which is being used to build an Ast */
		template<class AstPool>  // template so I can mock
		struct AstBacker
		{
			typedef Any value_type;
			typedef Any* iterator;

			AstPool& _pool;
			iterator _begin, _end, _itr;
			size_t up_ast;

			AstBacker(AstPool& pool, iterator begin, iterator end)
				: _pool(pool), _begin(begin), _end(end), _itr(begin), up_ast(0)
			{}

			iterator begin() { return _begin; }
			iterator end() { return _itr; }

			size_t size() { return end() - begin(); }
			size_t reserved() { return _end - _begin; }

			template<class ... Args>
			AstBacker& emplace_back(Args&&... args)
			{
				new (_itr)value_type(args...);
				_inc_itr();
				return *this;
			}

			/* Nest an AstData.
			   Note: the AstData::value field holds the position of
			   the enclosing AstData, rather than the end of the Ast,
			   until end_ast is called.  */
			void nest_ast()
			{
				emplace_back(tag<AstData>::value, reinterpret_cast<void*>(up_ast));
				up_ast = size() - 1;
			}

			void end_ast()
			{
				auto& ast = explicit_unwrap<AstData>(_begin[up_ast]);
				up_ast = ast.value;
				ast.value = _itr - reinterpret_cast<Any*>(&ast) - 1;
			}

			/* \internal Increment the _itr, resizing the backing store if
			   required. */
			void _inc_itr()
			{
				++_itr;
				if(_itr >= _end) { _pool.grow_ast_backer(*this); }
			}

			Ast ast() { return Ast(reinterpret_cast<AstData*>(_begin)); }
		};
	}

	template<class Store> // template for mocking (requires 'mock(Any&)')
	struct AstPool
	{
		const static size_t START_SIZE = 512;
		const static size_t RESIZE_FACTOR = 2;

		// Enough space for the AstData and three other items.  Very
		// few expressions in my tests are smaller than that, although
		// this is not very scientific.
		const static size_t DEFAULT_INITIAL_SIZE = 4;

		// Fraction of pool which must be filled before we resize
		constexpr static float RESIZE_THRESHOLD = 0.9;

		ast_pool_detail::Backer *backer, *temp;
		Store& store;
		bool resize;

		typedef ast_pool_detail::AstBacker<AstPool> AstBacker;

		// A list is the simplest way to keep pointers valid through add/remove
		typedef std::list<AstBacker> AstBackers;
		AstBackers _backers;

		AstPool(Store& store_)
			: store(store_),
			  resize(false)
		{
			backer = new ast_pool_detail::Backer(START_SIZE);
			temp = nullptr;
		}

		~AstPool()
		{
			delete backer;
			if(temp) { delete temp; }
		}

		void gc_start()
		{
			temp = backer;
			if(resize)
				{
					backer = new ast_pool_detail::Backer(temp->reserved() * RESIZE_FACTOR);
					resize = false;
				}
			else
				{ backer = new ast_pool_detail::Backer(temp->reserved()); }
		}

		void gc_finish()
		{
			// If we're occupying more than the RESIZE_THRESHOLD of the
			// reserved pool, we'll grow the pool next time around.
			resize = (backer->reserved() * RESIZE_THRESHOLD) < backer->size();
			delete temp;
			temp = nullptr;
		}

		// Move an AstBacker onto the 'backer' array, allocating
		// `reserve` cells for it.
		void move_ast_backer(AstBacker& building, size_t reserve)
		{
			using namespace ast_pool_detail;

			while(reserve > backer->remaining())
				{ store.gc(); }

			auto old = building;

			building._begin = backer->_itr;
			backer->_itr += reserve;

			building._itr = building._begin;
			building._end = backer->_itr;

			copy_to_end(make_range(old), building._itr);
		}

		// Mark the non-AstData stuff for the Ast's I'm building
		void mark()
		{
			for(auto& building : _backers)
				{
					move_ast_backer(building, building.reserved());

					for(auto& any : building)
						{
							switch(any._tag)
								{
								case tag<AstData>::value:
								case tag<MovedAstData>::value:
									break;
								default:
									store.mark(any);
								}
						}
				}
		}

		AstBacker unmarked_ast_backer(size_t initial_size)
		{
			backer->_itr += initial_size;

			return AstBacker(*this,
			                 backer->_itr - initial_size,
			                 backer->_itr);
		}

		typename AstBackers::iterator ast_backer(size_t initial_size)
		{
			while(initial_size > backer->remaining())
				{ store.gc(); }

			_backers.emplace_front(unmarked_ast_backer(initial_size));
			return _backers.begin();
		}

		typename AstBackers::iterator ast_backer()
		{ return ast_backer(DEFAULT_INITIAL_SIZE); }

		void free_ast_backer(typename AstBackers::iterator itr)
		{ _backers.erase(itr); }

		void grow_ast_backer(AstBacker& ast)
		{ move_ast_backer(ast, ast.reserved() * RESIZE_FACTOR); }

		// Copy ast over to a new backer, but don't mark the the input
		// as moved
		Ast just_move(Ast& ast)
		{
			auto building = unmarked_ast_backer(ast.flat_size());

			Ast new_range(reinterpret_cast<AstData*>(building._itr));
			ast_pool_detail::copy_to_end(flat_ast(ast), building._itr);
			return new_range;
		}


		// Copy 'ast' over to the new 'backer' array, and mark it's
		// containedp items.  Ast's original AstData is invalidated,
		// and used to store a pointer to its new location until the
		// garbage collection finishes.
		Ast move(Ast ast)
		{
			if(backer->is_contained(ast.value))
				{ return ast; }

			if(ast.value->_tag == tag<MovedAstData>::value)
				{ return Ast(reinterpret_cast<AstData*>(ast.value->value)); }

			else
				{
					auto new_ast = just_move(ast);

					// store the location of the new copy to avoid re-copying
					reinterpret_cast<Any*>(ast.value)->value = new_ast.value;
					ast.value->_tag = tag<MovedAstData>::value;

					for(auto& item : slice(flat_ast(new_ast), 1))
						{
							if(is<Ast>(item))
								{ item = wrap(just_move(unwrap<Ast>(item))); }
							else
								{ store.mark(item); }
						}

					return new_ast;
				}
		}

		size_t size() const { return backer->size(); }
	};
}

#endif
