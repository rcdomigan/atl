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
	// For type inference purposes, each symbol is a distinct instance
	// of Symbol, so I'm setting a 'slot' on each to refer to the
	// actual symbol value (which may be defined after the symbol is used).
	struct Slots
		: public std::vector<Any>,
		  public MarkBase
	{
		explicit Slots(GC& gc)
			: MarkBase(gc)
		{}

		virtual void mark() override
		{
			for(auto& value : *this)
				{ manage_marking.gc->mark(value); }
		}
	};

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
		SymbolMap *up, *down;
		LambdaMetadata *closure;

		Slots& slots;

		explicit SymbolMap(GC& gc, Slots& slots_)
			: MarkBase(gc)
			, up(nullptr)
			, closure(nullptr)
			, slots(slots_)
		{}

		explicit SymbolMap(SymbolMap& up_,
		                   LambdaMetadata *closure_)
			: MarkBase(*up_.manage_marking.gc)
			, up(&up_)
			, closure(closure_)
			, slots(up_.slots)
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

		std::tuple<all_iterator, SymbolMap*> find(std::string const& k)
		{
			auto itr = local.find(k);
			if(itr == end())
				{
					if(up)
						{ return up->find(k); }
					else
						{ return std::make_tuple(all_iterator(itr), nullptr); }
				}
			return std::make_tuple(all_iterator(itr), this);
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

		// Add 'sym' to all the closures between `citr` and `from_closure`.
		Any _add_closures(Symbol& sym, SymbolMap* from_closure, SymbolMap* citr)
		{
			auto& cc = *citr->closure;
			if(cc.has_closure_parmeter(sym.name))
				{ return wrap(cc.get_closure_parmeter(sym.name)); }

			if(citr == from_closure)
				{ return from_closure->local[sym.name]; }

			auto closure_value = _add_closures(sym, from_closure, citr->up);
			return wrap(cc.new_closure_parameter(sym.name, closure_value));
		}

		Any current_value(Any const& key)
		{
			if(is<Symbol>(key))
				{
					auto& sym = unwrap<Symbol>(key);

					SymbolMap::all_iterator found(local.end());
					SymbolMap *from_closure;

					std::tie(found, from_closure) = find(sym.name);

					if(found != very_end())
						{
							auto& value = found->second;

							if(is<GlobalSlot>(value))
								{
									sym.slot = unwrap<GlobalSlot>(value).value;
									return wrap(&sym);
								}

							if(from_closure == this) { return found->second; }

							else
								{
									switch(value._tag)
										{
										case tag<ClosureParameter>::value:
										case tag<Parameter>::value:
											{
												_add_closures(sym, from_closure, this);

												return wrap<ClosureParameter>
													(closure->closure_index_map[sym.name]);
											}
										default:
											return found->second;
										}
								}
						}
					else
						{
							// Associate the symbol with a slot
							auto slot = slots.size();
							slots.push_back(wrap<Null>());
							sym.slot = slot;
							define(sym.name, wrap<GlobalSlot>(slot));
						}

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
		{ return value()._tag; }

		virtual WalkerPtr clone() const override
		{ return _clone(this); }
	};

	WalkValues walk_values(SymbolMap& env, Ast::Subex& subex)
	{ return WalkValues(env, std::get<0>(subex), std::get<1>(subex)); }

	WalkValues walk_values(SymbolMap& env, Ast::Subex&& subex)
	{ return walk_values(env, subex); }


	/** Replace symbols in `value` with their values.  This doesn't
	 * setup back-patches or lexical scope, it's just meant to define
	 * the special forms (like Lambda) before type inference occurs.
	 */
	struct AssignForms
	{
		GC& gc;
		SymbolMap& toplevel;

		AssignForms(GC& gc_, SymbolMap& toplevel_)
			: gc(gc_), toplevel(toplevel_)
		{}

		void _ast(SymbolMap& env, AstBuilder& builder, WalkValues& walker)
		{
			using namespace pattern_match;
			Symbol *sym;

			Any definition;
			if(match(rest(tag<Define>::value, capture_ptr(sym), capture(definition)),
			         walker))
				{
					auto inner = gc.ast_builder();
					{
						NestAst nest_inner(inner);

						inner.push_back(wrap<Define>());
						inner.push_back(wrap(sym));

						switch(definition._tag)
							{
							case tag<Ast>::value:
							case tag<AstData>::value:
								{
									auto walk_def = walk_values(env, subex(definition));
									_ast(env, inner, walk_def);
									break;
								}
							default:
								{ _atom(env, inner, definition); }
							}
					}
					env.slots[sym->slot] = inner.built();
					builder.push_back(inner.built());
					return;
				}
			else
				{
					NestAst nesting(builder);
					Ast::Subex formals, body;
					if(match(rest(tag<Lambda>::value, capture(formals), capture(body)),
					         walker))
						{
							auto metadata = gc.make<LambdaMetadata>(gc(ast_hof::copy(formals)));

							SymbolMap inner_map(env, metadata.pointer());

							builder.push_back(wrap<Lambda>(metadata.pointer()));
							{
								NestAst nest_formals(builder);
								size_t idx = 0;
								for(auto const& sym : make_range(formals))
									{
										assert(is<Symbol>(sym));
										inner_map.formal(unwrap<Symbol>(sym),
										                 idx++);
										builder.push_back(sym);
									}
							}
							auto walk_body = walk_values(inner_map, body);
							_ast(inner_map, builder, walk_body);
						}
					else
						{
							for(; walker.has_value(); walker.next())
								{ _dispatch(env, builder, walker); }
						}
				}
		}

		void _atom(SymbolMap& env, AstBuilder& builder, Any value)
		{ builder.push_back(value); }

		void _atom(SymbolMap& env, AstBuilder& builder, WalkValues& walker)
		{ _atom(env, builder, walker.value()); }

		void _dispatch(SymbolMap& env, AstBuilder& builder, WalkValues& walker)
		{
			if(walker.is_subex())
				{
					auto sub = walker.walk_subex();
					_ast(env, builder, walker);
				}
			else
				{ _atom(env, builder, walker); }
		}

		Marked<Ast> operator()(Ast& ast)
		{
			auto builder = gc.ast_builder();
			auto walker = walk_values(toplevel, ast.subex());

			_ast(toplevel, builder, walker);

			return gc.marked(unwrap<Ast>(builder.built()));
		}

		Marked<Ast> operator()(Marked<Ast>& ast)
		{ return (*this)(*ast); }

		Marked<Ast> operator()(Marked<Ast>&& ast)
		{ return (*this)(ast); }
	};
}
#endif
