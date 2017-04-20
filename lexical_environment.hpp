#ifndef ATL_NEW_LEXICAL_ENVIRONMENT_HPP
#define ATL_NEW_LEXICAL_ENVIRONMENT_HPP
/**
 * @file /home/ryan/programming/atl/environment.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Sep 23, 2014
 */

#include <stddef.h>
#include <cassert>
#include <atl/exception.hpp>
#include <functional>
#include <atl/helpers/itritrs.hpp>
#include <atl/helpers/make_ast.hpp>
#include <atl/helpers/pattern_match.hpp>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <atl/type.hpp>
#include <atl/type_traits.hpp>
#include <utility>
#include <vector>
#include "./print.hpp"
#include "gc/ast_builder.hpp"
#include "gc/gc.hpp"
#include "gc/marked.hpp"
#include "helpers/misc.hpp"
#include "is.hpp"
#include "wrap.hpp"


namespace atl
{
	/**
	 * Map of toplevel definitions
	 */
	struct SymbolMap
		: public MarkBase
	{
		typedef Any value_type;
		typedef std::map<std::string, value_type> Map;
		typedef Map::iterator iterator;
		typedef Map::const_iterator const_iterator;

		struct all_iterator
		{
			SymbolMap::iterator value;
			typedef SymbolMap::Map::value_type value_type;

			value_type& operator*() { return *value; }
			SymbolMap::iterator operator->() { return value; }

			all_iterator(SymbolMap::iterator vv) : value(vv) {}
			bool operator==(all_iterator const& other) const
			{ return value == other.value; }

			bool operator!=(all_iterator const& other) const
			{ return value != other.value; }
		};

		Map local;
		SymbolMap *up;
		LambdaMetadata *closure;

		explicit SymbolMap(GC& gc)
			: MarkBase(gc)
			, up(nullptr)
			, closure(nullptr)
		{}

		explicit SymbolMap(GC& gc,
		                   SymbolMap *up_,
		                   LambdaMetadata *closure_)
			: MarkBase(gc)
			, up(up_)
			, closure(closure_)
		{}

		SymbolMap(SymbolMap const&) = delete;
		SymbolMap(SymbolMap &&) = delete;
		SymbolMap() = delete;

		virtual void mark() override
		{
			if(closure)
				{ manage_marking.gc->mark(*closure); }

			for(auto item : local)
				{ manage_marking.gc->mark(item.second); }

			if(up) { up->mark(); }
		}

		SymbolMap& top()
		{
			if(up != nullptr)
				{ return up->top(); }
			else
				{ return *this; }
		}

		void formal(Symbol& sym,
		            size_t offset)
		{
			auto rval = local.emplace(sym.name, wrap<Parameter>(offset));

			if(!rval.second)
				{ throw RedefinitionError(std::string("Can't redefine ").append(sym.name)); }
		}

		all_iterator very_end()
		{
			if(up)
				{ return up->very_end(); }
			else
				{ return local.end(); }
		}

		/* returns <found, closure, from_closure> (from_closure indicating
		   that it's from a different closure than the current 'top'.
		*/
		std::tuple<all_iterator, LambdaMetadata*, bool> find(std::string const& k)
		{
			auto itr = local.find(k);
			if(itr == end())
				{
					if(up)
						{
							auto rval = up->find(k);
							std::get<2>(rval) = true;
							return rval;
						}
					else
						{ return std::make_tuple(all_iterator(itr), nullptr, true); }
				}
			return std::make_tuple(all_iterator(itr), closure, false);
		}
		size_t size() const { return local.size(); }
		size_t count(std::string const& key) const { return local.count(key); }

		iterator end() { return local.end(); }
		iterator begin() { return local.begin(); }

		const_iterator end() const { return local.end(); }
		const_iterator begin() const { return local.begin(); }

		void define(std::string const& name, Any value) { local.emplace(name, value); }

		size_t count(std::string const& key)
		{
			if(std::get<0>(find(key)) != very_end())
				{ return 1; }
			return 0;
		}

		value_type& operator[](std::string const& key) { return local[key]; }

		void dbg()
		{
			std::cout << "Closure: " << closure << std::endl;
			for(auto& pair : local)
				{
					std::cout << pair.first
					          << " = (: "
					          << printer::print(pair.second)
					          << ")"
					          << std::endl;
				}
			if(up) up->dbg();
		}

		Any current_value(Any const& key)
		{
			if(is<Symbol>(key))
				{
					all_iterator found = std::get<0>(find(unwrap<Symbol>(key).name));
					if(found != very_end())
						{ return found->second; }
				}
			return key;
		}
	};

	void setup_basic_definitions(GC& store, SymbolMap& toplevel)
	{
		using namespace fn_type;

		toplevel.define("__\\__", wrap<Lambda>());
		toplevel.define(":", wrap<DeclareType>());
		toplevel.define("quote", wrap<Quote>());
		toplevel.define("if", wrap<If>());
		toplevel.define("#f", atl_false());
		toplevel.define("#t", atl_true());
		toplevel.define("define", wrap<Define>());
	}

	typedef std::set<std::string> closure;

	typedef std::map<std::string, std::vector<Ast::iterator> > BackPatch;

	// Ast::iteator wrapper which implicitly fetches Symbol's values
	struct WalkValues
		: public WalkAstIterator
	{
		SymbolMap& env;
		WalkValues(SymbolMap& env_, Ast::iterator const& itr_, Ast::iterator const& end_)
			: WalkAstIterator(itr_, end_), env(env_)
		{}

		virtual Any value() override
		{ return env.current_value(*itr); }

		virtual tag_t tag() override
		{ return env.current_value(*itr)._tag; }

		virtual WalkerPtr walk_subex() override
		{
			auto range = _range();
			return std::make_unique<WalkValues>(env, range.begin(), range.end());
		}

		virtual WalkerPtr clone() const override
		{ return _clone(this); }
	};

	WalkValues walk_values(SymbolMap& env, Ast::Subex& subex)
	{ return WalkValues(env, subex.begin(), subex.end()); }

	WalkValues walk_values(SymbolMap& env, Ast::Subex&& subex)
	{ return walk_values(env, subex); }

	WalkValues subex_values(WalkValues& walker)
	{ return walk_values(walker.env, subex(walker.itr)); }

	/** Replace symbols in `value` with their values.  This doesn't
	 * setup back-patches or lexical scope, it's just meant to define
	 * the special forms (like Lambda) before type inference occurs.
	 */
	struct AssignForms
	{
		GC& gc;
		SymbolMap& env;

		AssignForms(GC& gc_, SymbolMap& env_)
			: gc(gc_), env(env_)
		{}

		void _ast(AstBuilder& builder, WalkValues& walker)
		{
			NestAst nesting(builder);

			using namespace pattern_match;
			namespace pm = pattern_match;
			Ast::Subex formals, body;

			if(match(ast(tag<Lambda>::value, capture(formals), capture(body)),
			         walker))
				{
					auto metadata = gc.make<LambdaMetadata>
						(gc(ast_hof::copy(formals)));

					builder.push_back(wrap<Lambda>(metadata.pointer()));

					ast_hof::copy(formals)(builder);

					auto walk_body = walk_values(env, body);
					_ast(builder, walk_body);
				}
			else
				{
					for(; walker.has_value(); walker.next())
						{ _dispatch(builder, walker); }
				}
		}

		void _atom(AstBuilder& builder, WalkValues& walker)
		{ builder.push_back(walker.value()); }

		void _dispatch(AstBuilder& builder, WalkValues& walker)
		{
			if(walker.is_subex())
				{
					auto inner = subex_values(walker);
					_ast(builder, inner);
				}
			else
				{ _atom(builder, walker); }
		}

		Marked<Ast> operator()(Marked<Ast>& ast)
		{
			auto builder = gc.ast_builder();
			auto walker = walk_values(env, ast->subex());

			_ast(builder, walker);

			return gc.marked(builder.root());
		}

		Marked<Ast> operator()(Marked<Ast>&& ast)
		{ return (*this)(ast); }
	};

	/** setup backpatches, and keep track of what the closure will
	 * need to capture.
	 *
	 * @param closure: The closure needs to include symbol which are
	 * not in the toplevel or local lambda.
	 */
	void assign_symbol(SymbolMap& env
	                   , BackPatch& backpatch
	                   , Ast::iterator& itr)
	{
		auto& sym = unwrap<Symbol>(*itr);

		SymbolMap::all_iterator found_def(env.local.end());
		LambdaMetadata *closure;
		bool from_parent_closure;

		std::tie(found_def, closure, from_parent_closure) = env.find(sym.name);

		if(found_def != env.very_end())
			{
				itr.reference() = found_def->second;

				if(from_parent_closure && itr.is<Parameter>())
					{
						Symbol& sym = unwrap<Symbol>(closure->formals[unwrap<Parameter>(*itr).value]);
						itr.reference() = wrap(env.closure->closure_parameter(sym.name, *itr));
					}
			}
		else
			{
				/* If we haven't found the symbol at all, it must be backpatched at the toplevel later. */
				auto to_patch = backpatch.find(sym.name);
				if(to_patch != backpatch.end())
					{ to_patch->second.push_back(itr); }
				else
					{ backpatch.emplace(sym.name, std::vector<Ast::iterator>({itr})); }
			}
		return;
	}


	/** Modify `value` in place replacing body Symbols with Bounds
	 * pointing to their respective formal or toplevel entry
	 */
	struct AssignFree
	{
		GC& gc;
		SymbolMap& toplevel;
		BackPatch& backpatch;

		AssignFree(GC& gc_,
		           SymbolMap& env_,
		           BackPatch& backpatch_)
			: gc(gc_), toplevel(env_), backpatch(backpatch_)
		{}

		void operator()(Ast::iterator itr)
		{ assign_free(toplevel, itr); }

		void operator()(Marked<Ast>& ast)
		{ (*this)(ast->self_iterator()); }

		void operator()(Marked<Ast>&& ast)
		{ (*this)(ast->self_iterator()); }

		void assign_free(SymbolMap& env, Ast::iterator& outer)
		{
			switch(outer.tag())
				{
				case tag<Symbol>::value:
					assign_symbol(env, backpatch, outer);
					return;
				case tag<Ast>::value:
					{
						auto subex = atl::subex(outer);
						auto itr = subex.begin();

						if(is<Symbol>(*itr))
							{ assign_symbol(env, backpatch, itr); }

						switch(itr->_tag)
							{
							case tag<Define>::value:
								{
									++itr;
									auto& sym = unwrap<Symbol>(*itr);

									++itr; // body
									{
										namespace pm = pattern_match;
										Lambda func(nullptr);

										auto copy = itr;
										if(match(pm::ast(pm::capture(func), tag<Ast>::value, tag<Ast>::value),
										         copy))
											{
												env.define(sym.name, wrap<Function>(func.value));
												(*this)(itr);
											}
									}
								}
								return;
							case tag<Lambda>::value:
								{
									SymbolMap inner_map(gc, &env, unwrap<Lambda>(itr.reference()).value);

									++itr; // formals
									size_t idx = 0;
									for(auto const& sym : atl::subex(itr))
										{
											assert(is<Symbol>(sym));
											inner_map.formal(unwrap<Symbol>(sym),
											                 idx++);
										}

									++itr;
									assign_free(inner_map, itr);
								}
								return;
							default:
								for(auto inner : itritrs(subex))
									{ assign_free(env, inner); }
								return;
							}
						return;
					}
				}
		}
	};
}
#endif
