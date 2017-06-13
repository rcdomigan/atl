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
#include <boost/mpl/set.hpp>
#include <boost/mpl/lambda.hpp>
#include <boost/mpl/placeholders.hpp>
#include <boost/mpl/value_type.hpp>
#include <boost/mpl/fold.hpp>
#include <boost/mpl/insert.hpp>
#include <boost/mpl/apply.hpp>

#include <atl/debug.hpp>
#include <atl/byte_code.hpp>

#include <atl/gc/ast_pool.hpp>
#include <atl/gc/pool.hpp>
#include <atl/gc/ast_builder.hpp>
#include <atl/gc/marked.hpp>
#include <atl/gc/vm_closure.hpp>

namespace atl
{
	/*****************/
	/*	 ____  ____  */
	/*	/ ___|/ ___| */
	/* | |  _| |     */
	/* | |_| | |___  */
	/*	\____|\____| */
	/*****************/
	struct GC;

	namespace gc_detail
	{
		struct Counter
		{
			size_t& count;
			GC* gc;

			Counter(GC *gc_, size_t& count_) : count(count_), gc(gc_) {}

			template<class Mem>
			void operator()(Mem const&)
			{
				auto &mem = gc->*Mem::value;
				count += mem.num_allocated();
			}
		};

		struct Sweeper
		{
			GC* gc;

			Sweeper(GC *gc_) : gc(gc_) {}

			template<class Mem>
			void operator()(Mem const&)
			{
				auto &mem = gc->*Mem::value;
				mem.sweep();
			}
		};

		// Class needs to implement MarkBase to get marked by the GC
		template<class GC>
		struct MarkBase;

		// Manage the lifespan of the GC's reference to a MarkBase
		// instance.
		template<class GC>
		struct ManageMarking
		{
			MarkBase<GC>* container;
			GC* gc;
			typename GC::MarkBaseList::iterator itr;

			ManageMarking(GC* gc_, MarkBase<GC>* container_)
				: container(container_),
				  gc(gc_)
			{ itr = gc->add_mark_base(container); }

			void drop()
			{
				if(gc) { gc->remove_mark_base(itr); }
			}

			void take(ManageMarking&& other, MarkBase<GC>* container_)
			{
				container = container_;
				gc = other.gc;
				itr = other.itr;
				*itr = container;
				other.gc = nullptr;
			}

			ManageMarking(ManageMarking&& other, MarkBase<GC>* container_)
			{ take(std::move(other), container_); }

			ManageMarking()=delete;
			ManageMarking(ManageMarking const&)=delete;
			ManageMarking(ManageMarking&&)=delete;

			~ManageMarking()
			{ drop(); }
		};

		template<class GC>
		struct MarkBase
		{
			ManageMarking<GC> manage_marking;
			MarkBase(GC& gc)
				: manage_marking(&gc, this)
			{}
			MarkBase(MarkBase const& other)
				: manage_marking(other.manage_marking.gc, this)
			{}
			MarkBase(MarkBase&& other)
				: manage_marking(std::move(other.manage_marking), this)
			{}

			MarkBase& operator=(MarkBase&& other)
			{
				manage_marking.drop();
				manage_marking.take(std::move(other.manage_marking), this);
				return *this;
			}

			virtual void mark()=0;
		};
	}

	namespace pl = ::boost::mpl::placeholders;

	struct GC
	{
		typedef ::atl::gc_detail::MarkBase<GC> MarkBase;
		typedef std::function<void (GC&)> MarkCallback;
		typedef std::list<MarkCallback> RootsType;
		typedef std::list<MarkBase*> MarkBaseList;


		typedef ::atl::AstPool<GC> AstPool;
		typedef ::atl::ast_builder::AstBuilder<AstPool> AstBuilder;

		typedef std::function<void (AstBuilder&)> ast_composer;

		RootsType _roots;
		MarkBaseList _mark_bases;
		Marked<Any> *_cxx_stack;

		// Adds callbacks which will be invoked during the mark phase of the GC.
		// @param fn: the callback
		RootsType::iterator add_marker(MarkCallback const& fn)
		{
			_roots.push_front(fn);
			return _roots.begin();
		}

		void remove_marker(RootsType::iterator itr) { _roots.erase(itr); }

		MarkBaseList::iterator add_mark_base(MarkBase* item)
		{
			_mark_bases.push_front(item);
			return _mark_bases.begin();
		}

		void remove_mark_base(MarkBaseList::iterator itr) { _mark_bases.erase(itr); }

		AstPool _ast_pool;
		ClosurePool _closure_pool;

		memory_pool::Pool< LambdaMetadata > _lambda_metadata_heap;
		memory_pool::Pool< String > _string_heap;
		memory_pool::Pool< CxxFunctor > _primitive_recursive_heap;
		memory_pool::Pool< Symbol > _symbol_heap;
		memory_pool::Pool< Scheme > _scheme_heap;

		bool _gc_in_progress;

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
		                  , mpl::pair< Scheme,	MemberPtr<Scheme, &GC::_scheme_heap > >
		                  > PoolMap;

		typedef typename mpl::fold<PoolMap,
		                           mpl::set<>,
		                           mpl::lambda< mpl::insert<pl::_1, mpl::first<pl::_2> > >
		                           >::type BasicPoolTypes;

		typedef typename tmpl::Apply<mpl::insert,
		                             mpl::insert<BasicPoolTypes, Ast>,
		                             tmpl::Identity<Any> >::type MarkableTypes;




		template<class T>
		T* alloc_from(memory_pool::Pool<T> &pool)
		{
			auto result = pool.alloc();

			if(result == nullptr) {
				gc();
				result = pool.alloc();
				if(result == nullptr)
					{ throw std::string("out of memory"); }
			}

			return result;
		}

		GC() : _cxx_stack(nullptr), _ast_pool(*this), _gc_in_progress(false) {}

		// Mark everything the GC knows about.  This method was broken
		// out for testing; use the 'gc()' method.
		void _mark()
		{
			_ast_pool.gc_start();
			_ast_pool.mark();

			for(auto& i : _roots) { i(*this); }
			for(auto& i : _mark_bases) { i->mark(); }

			auto marked = _cxx_stack;
			while(marked)
				{
					mark(marked->any);
					marked = marked->_up;
				}
		}

		// Sweep marked objects.  This method was broken out for
		// testing; use the gc() method.
		void _sweep()
		{
			auto sweeper = gc_detail::Sweeper(this);
			mpl::for_each
				<PoolMap,
				 typename mpl::lambda< mpl::value_type<PoolMap, mpl::_1 > >::type
				 >(sweeper);
			_ast_pool.gc_finish();
		}

		void gc()
		{
			assert(!_gc_in_progress);
			_gc_in_progress = true;
			_mark();
			_sweep();
			_gc_in_progress = false;
		}

		void mark(String& str)
		{ _string_heap.mark(&str); }

		void mark(CxxFunctor& functor)
		{
			_primitive_recursive_heap.mark(&functor);
			mark(functor.type);
		}

		void mark(Scheme& scheme)
		{
			_scheme_heap.mark(&scheme);
			mark(scheme.type);
		}

		void mark(Symbol& sym)
		{
			_symbol_heap.mark(&sym);
			// The Scheme is on the Symbol, not the Scheme heap, so
			// just check its type part.
			mark(sym.scheme.type);
		}

		void mark(LambdaMetadata& metadata)
		{
			_lambda_metadata_heap.mark(&metadata);

			for(auto& item : metadata.closure)
				{ mark(item); }

			mark(metadata.formals);
		}

		void mark(Ast& ast)
		{ ast = _ast_pool.move(ast); }

		void mark(Any& aa)
		{
			switch(aa._tag)
				{
				case tag<Lambda>::value:
					if(aa.value) { mark(*unwrap<Lambda>(aa).value); }
					break;

				case tag<LambdaMetadata>::value:
					mark(unwrap<LambdaMetadata>(aa));
					break;

				case tag<String>::value:
					mark(unwrap<String>(aa));
					break;

				case tag<CxxFunctor>::value:
					mark(unwrap<CxxFunctor>(aa));
					break;

				case tag<Symbol>::value:
					mark(unwrap<Symbol>(aa));
					break;

				case tag<Scheme>::value:
					mark(unwrap<Scheme>(aa));
					break;

				case tag<Ast>::value:
					mark(unwrap<Ast>(aa));
					break;

				default:
					break;
				}
		}

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

		template<class Type, class ... Types>
		Type* raw_make(Types ... args)
		{ return new (alloc<Type>()) Type (args...); }

		template<class Type, class ... Types>
		Any amake(Types ... args)
		{ return Any(tag<Type>::value , raw_make<Type>(args...)); }

		// Unpacks any of the `args` which are of Marked type before
		// passing them to the constructor
		template<class Type, class ... Types>
		Marked<Type> make(Types ... args)
		{
			return Marked<Type>(_cxx_stack,
			                    Any(tag<Type>::value,
			                        raw_make<Type>(unpack_marked(args)...)));
		}

		template<class T>
		Marked<T> marked(T& thing)
		{ return Marked<T>(_cxx_stack, thing); }

		Marked<Ast> marked(Ast thing)
		{ return Marked<Ast>(_cxx_stack, wrap(thing)); }

		Marked<Any> marked(Any thing)
		{ return Marked<Any>(_cxx_stack, thing); }

		AstBuilder ast_builder()
		{ return AstBuilder(_ast_pool, _ast_pool.ast_backer()); }

		AstBuilder ast_builder(size_t nn)
		{ return AstBuilder(_ast_pool, _ast_pool.ast_backer(nn)); }

		Ast raw_ast(ast_composer const& func)
		{
			auto ast = ast_builder();
			func(ast);
			return unwrap<Ast>(ast.built());
		}

		Marked<Ast> operator()(ast_composer const& func)
		{
			auto ast = ast_builder();
			func(ast);
			return Marked<Ast>(_cxx_stack, ast.built());
		}

		size_t cells_allocated()
		{
			size_t count = 0;
			auto counter = gc_detail::Counter(this, count);
			mpl::for_each
				<PoolMap,
				 typename mpl::lambda< mpl::value_type<PoolMap, mpl::_1 > >::type
				 >(counter);

			return count + _ast_pool.size();
		}

		pcode::value_type* closure(pcode::value_type body_location,
		                           size_t formals,
		                           size_t captures)
		{ return _closure_pool.closure(body_location, formals, captures); }
	};

	template< class T,	memory_pool::Pool<T> GC::*member >
	const typename GC::MemberPtr<T,member>::PoolType GC::MemberPtr<T,member>::value = member;

	typedef GC::AstBuilder AstBuilder;
	typedef ast_builder::NestAst<AstBuilder> NestAst;
	typedef GC::ast_composer ast_composer;
	typedef GC::MarkBase MarkBase;
}

#endif
