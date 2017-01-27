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
	// This is a basic vector with the copy constructor disabled so I
	// can pass it around by reference and not accidently copy by
	// value.
	struct AstSubstrate
		: public std::vector<Any>
	{
		AstSubstrate() : std::vector<Any>() {}
		AstSubstrate(AstSubstrate const&) = delete;

		Any& root() { return front(); }

		void dbg();
	};

	void AstSubstrate::dbg() {
		for(auto& vv : *this)
			{ std::cout << type_name(vv) << std::endl; }
	}

	// An iterator like thing which follows the position of an object
	// in a vector so it will remain valid after a resize.
	struct MovableAstPointer
	{
		AstSubstrate *vect;
		size_t position;

		MovableAstPointer(AstSubstrate* vect_, size_t pos)
			: vect(vect_), position(pos)
		{}

		AstData* ast_data() { return reinterpret_cast<AstData*>(&(*vect)[position]); }

		// Last + 1 element of the wrapped Ast
		void end_ast()
		{
			reinterpret_cast<AstData*>(vect->data() + position)->value
				= vect->size() - position - 1;
		}

		Ast operator*() { return Ast(ast_data()); }
	};

	MovableAstPointer push_nested_ast(AstSubstrate& substrate)
	{
		auto pos = substrate.size();
		substrate.emplace_back(tag<AstData>::value, nullptr);
		return MovableAstPointer(&substrate, pos);
	}

	struct AstAllocator;
	typedef std::function<Ast (AstAllocator)> ast_composer;

	// I would like some Asts generating functions to be able to use
	// either the GC or an Arena at runtime,
	struct AllocatorBase
	{
		virtual ~AllocatorBase() {}

		virtual AstSubstrate& sequence() = 0;
		virtual Symbol* symbol(std::string const&) = 0;
		virtual LambdaMetadata* lambda_metadata() = 0;
		virtual Symbol* symbol(std::string const&, Scheme const& type) = 0;

		virtual void free(Any any) = 0;

		Ast operator()(ast_composer const& func);

		pcode::value_type* closure(pcode::value_type body_location,
		                           size_t formals,
		                           size_t captures)
		{
			auto rval = new pcode::value_type[captures + 2];
			rval[0] = formals;
			rval[1] = body_location;
			return rval;
		}
	};

	struct AstAllocator
	{
		AllocatorBase &store;
		AstSubstrate &buffer;

		explicit AstAllocator(AllocatorBase &aa)
			: store(aa), buffer(aa.sequence())
		{}

		AstAllocator& push_back(Any value)
		{
			buffer.push_back(value);
			return *this;
		}

		MovableAstPointer nest_ast()
		{ return push_nested_ast(buffer); }

		size_t size()
		{ return buffer.size(); }
	};

	Ast AllocatorBase::operator()(ast_composer const& func)
	{
		auto backer = AstAllocator(*this);
		func(backer);
		return Ast(reinterpret_cast<AstData*>(&backer.buffer.root()));
	}

	struct NestAst
	{
		AstAllocator& store;
		MovableAstPointer ast;

		NestAst(AstAllocator& store_)
			: store(store_), ast(store.nest_ast())
		{}

		~NestAst() { ast.end_ast(); }
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

		virtual AstSubstrate& sequence() override
		{
			auto rval = new AstSubstrate();
			rval->reserve(100);
			return *rval;
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
		virtual AstSubstrate& sequence() override
		{
			auto rval = new AstSubstrate();
			rval->reserve(100);
			return *rval;
		}

		template<class T, class ... Args>
		T* make(Args ... args)
		{ return new  T(args...); }

		template<class T, class ... Args>
		Any amake(Args ... args)
		{ return Any( tag<T>::value , make<T>(args...)); }

		virtual Symbol* symbol(std::string const& name) override
		{ return make<Symbol>(name); }

		virtual LambdaMetadata* lambda_metadata() override
		{ return make<LambdaMetadata>(); }

		virtual Symbol* symbol(std::string const& name, Scheme const& type) override
		{ return make<Symbol>(name, type); }

		virtual void free(Any any) override { /* stub */ }
	};
}

#endif
