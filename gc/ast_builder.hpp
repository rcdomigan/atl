#ifndef ATL_GC_AST_BUILDER_RAII_HPP
#define ATL_GC_AST_BUILDER_RAII_HPP

#include <atl/gc/ast_pool.hpp>

namespace atl
{
	namespace ast_builder
	{
		template<class AstBuilder>
		struct NestAst;

		// A RAII manager for an AstBacker.  Prevents the Array underlying
		// the Ast from being collected until it's been allocated (at
		// which point something else must be referencing it or it will be
		// collected).
		template<class AstPool>
		struct AstBuilder
		{
			typedef typename AstPool::AstBacker::value_type value_type;
			typedef typename AstPool::AstBacker::iterator iterator;

			typedef typename AstPool::AstBackers::iterator BackerIterator;

			AstPool* _pool;
			typename AstPool::AstBacker* backer;
			BackerIterator _itr; // for cleaning up the pool's reference to us
			bool _do_free;

			// note: other _must_ have the same _pool
			void _move(AstBuilder&& other)
			{
				backer = other.backer;
				_itr = other._itr;
				_do_free = true;
				_pool = other._pool;

				other._do_free = false;
				other.backer = nullptr;
				other._pool = nullptr;
			}

			AstBuilder(AstBuilder const&) = delete;

			explicit AstBuilder(AstPool& pool, BackerIterator itr)
				: _pool(&pool),
				  backer(&*itr),
				  _itr(itr),
				  _do_free(true)
			{}

			AstBuilder(AstBuilder&& other)
				: _pool(other._pool)
			{ _move(std::move(other)); }

			~AstBuilder() { if(_do_free) {_pool->free_ast_backer(_itr); } }

			AstBuilder& operator=(AstBuilder&& other)
			{
				_move(std::move(other));
				return *this;
			}

			void push_back(value_type const& item) { backer->emplace_back(item); }

			template<class ... Args>
			void emplace_back(Args&& ... args) { backer->emplace_back(args...); }

			void nest_ast() { backer->nest_ast(); }
			void end_ast() { backer->end_ast(); }

			Ast root() { return Ast(reinterpret_cast<AstData*>(backer->begin())); }
		};

		template<class AstBuilder>
		struct NestAst
		{
			AstBuilder& builder;
			bool _cleanup;

			NestAst(NestAst const&) = delete;

			NestAst(AstBuilder& builder_)
				: builder(builder_),
				  _cleanup(true)
			{ builder.nest_ast(); }

			NestAst(NestAst&& other)
				: builder(other.builder)
			{
				other._cleanup = false;
				_cleanup = true;
			}

			~NestAst() { if(_cleanup) { builder.end_ast(); } }
		};
	}
}

#endif
