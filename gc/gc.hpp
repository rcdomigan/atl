#ifndef ATL_GC_GC_HPP
#define ATL_GC_GC_HPP

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

#include <gc/pool.hpp>

#include "./debug.hpp"
#include "./byte_code.hpp"

#include <gc/ast_pool.hpp>
#include <gc/pool.hpp>
#include <gc/ast_builder.hpp>

namespace atl
{
	// A mark-and-sweep GC. STUB
	struct GC
	{

		void unregister(MarkType::iterator itr) { _mark.erase(itr); }

		memory_pool::Pool< LambdaMetadata > _lambda_metadata_heap;
		memory_pool::Pool< String > _string_heap;
		memory_pool::Pool< CxxFunctor > _primitive_recursive_heap;
		memory_pool::Pool< Symbol > _symbol_heap;

		template< class T,  memory_pool::Pool<T> GC::*member >
		struct MemberPtr {
			typedef memory_pool::Pool<T> GC::* PoolType;
			/* man this would be easier with inline definitions. */
			const static PoolType value;
		};

		typedef mpl::map< mpl::pair< LambdaMetadata , MemberPtr<LambdaMetadata, &GC::_lambda_metadata_heap > >
		                  , mpl::pair< String , MemberPtr<String, &GC::_string_heap > >
		                  , mpl::pair< CxxFunctor,
		                               MemberPtr<CxxFunctor, &GC::_primitive_recursive_heap > >
		                  , mpl::pair< Symbol,	MemberPtr<Symbol, &GC::_symbol_heap > >
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

		std::vector<Code> code_blocks;
	public:
		void mark_and_sweep()
		{
			for(auto i : _mark_callbacks)
				i(*this);

			_string_heap.sweep();
		}

		void mark(Any a) {}

		// Adds callbacks which will be invoked during the mark phase of the GC.
		// @param fn: the callback
		void mark_callback(function<void (GC&)> fn)
		{ _mark_callbacks.push_back(fn); }

		/*****************************/
		/**  __	 __	      _	        **/
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

		Code& alloc_pcode()
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

		AstBuilder ast_builder()
		{ return AstBuilder(_ast_pool, _ast_pool.ast_backer()); }

		AstBuilder ast_builder(size_t nn)
		{ return AstBuilder(_ast_pool, _ast_pool.ast_backer(nn)); }

		Ast raw_ast(ast_composer const& func)
		{
			auto ast = ast_builder();
			func(ast);
			return ast.root();
		}

		virtual Symbol* symbol(std::string const& name) override
		{ return make<Symbol>(name); }

		virtual LambdaMetadata* lambda_metadata() override
		{ return make<LambdaMetadata>(); }

		virtual Symbol* symbol(std::string const& name, Scheme const& type) override
		{ return make<Symbol>(name, type); }

		virtual void free(Any any) override { /* stub */ }
	};

	template< class T,	memory_pool::Pool<T> GC::*member >
	const typename GC::MemberPtr<T,member>::PoolType GC::MemberPtr<T,member>::value = member;

	typedef GC::AstBuilder AstBuilder;
	typedef ast_builder::NestAst<AstBuilder> NestAst;
	typedef GC::ast_composer ast_composer;
}

#endif
