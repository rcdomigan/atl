#ifndef ATL_NEW_LEXICAL_ENVIRONMENT_HPP
#define ATL_NEW_LEXICAL_ENVIRONMENT_HPP
/**
 * @file /home/ryan/programming/atl/environment.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Sep 23, 2014
 */

#include <map>
#include <vector>
#include <cassert>
#include <unordered_set>

#include <exception.hpp>
#include <type.hpp>
#include <helpers.hpp>
#include <helpers/pattern_match.hpp>
#include "./gc.hpp"
#include "./print.hpp"


namespace atl
{
	/**
	 * Map of toplevel definitions
	 */
	struct SymbolMap
	{
		/* The value_type::second may have literals or things computable from literals at compile time. */
		typedef Symbol* value_type;
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

		AllocatorBase& store;

		explicit SymbolMap(AllocatorBase& store_)
			: up(nullptr)
			, store(store_)
		{}

		explicit SymbolMap(SymbolMap *up_)
			: up(up_)
			, store(up->store)
		{}

		SymbolMap(SymbolMap const&) = delete;
		SymbolMap() = delete;

		SymbolMap& top()
		{
			if(up != nullptr)
				{ return up->top(); }
			else
				{ return *this; }
		}

		/* offset is from the frame pointer; reverse of the formal order (for
		   `(\ (a b) (...))` the offset of `a` would be 1 and `b` would be 0)
		*/
		void formal(Symbol& sym,
		            size_t offset)
		{
			sym.subtype = Symbol::Subtype::variable;
			sym.value = wrap(store.bound(&sym, Bound::Subtype::is_local, offset));

			auto rval = local.emplace(sym.name, &sym);

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

		all_iterator find(std::string const& k)
		{
			auto itr = local.find(k);
			if(itr == end())
				{
					if(up)
						{ return up->find(k); }
					else
						{ return all_iterator(itr); }
				}
			return itr;
		}
		size_t size() const { return local.size(); }
		size_t count(std::string const& key) const { return local.count(key); }

		iterator end() { return local.end(); }
		iterator begin() { return local.begin(); }

		const_iterator end() const { return local.end(); }
		const_iterator begin() const { return local.begin(); }

		void define(Symbol& sym,
		            Any const& value)
		{
			symbol_assign(sym, value);
			local.emplace(sym.name, &sym);
		}

		size_t count(std::string const& key)
		{
			if(find(key) != very_end())
				{ return 1; }
			return 0;
		}

		value_type& operator[](std::string const& key) { return local[key]; }

		void dbg()
		{
			for(auto& pair : local)
				{
					std::cout << pair.first
					          << " = (: "
					          << printer::print(pair.second->value)
					          << ")"
					          << std::endl;
				}
		}

		std::pair<Any, bool> current_value(Any key)
		{
			if(is<Symbol>(key))
				{
					auto& sym = unwrap<Symbol>(key);
					auto found = find(sym.name);

					if(found != very_end())
						{ return std::make_pair(found->second->value, true); }
					else
						{ return std::make_pair(key, false); }
				}
			return std::make_pair(key, false);
		}
	};

	void setup_basic_definitions(AllocatorBase& store, SymbolMap& toplevel)
	{
		using namespace fn_type;

		toplevel.define(*store.symbol("__\\__"), wrap<Lambda>());
		toplevel.define(*store.symbol(":"), wrap<DeclareType>());
		toplevel.define(*store.symbol("quote"), wrap<Quote>());

		toplevel.define
			(*store.symbol
			 ("if",
			  Scheme(std::unordered_set<Type::value_type>({0}),
			         wrap(fn(tt<Bool>(), 0, 0, 0)(ast_alloc(store))))),
			 wrap<If>());

		toplevel.define(*store.symbol("#f"), atl_false());
		toplevel.define(*store.symbol("#t"), atl_true());
		toplevel.define(*store.symbol("define"), wrap<Define>());
	}

	typedef std::map<std::string, std::vector<Any*> > BackPatch;
	typedef std::set<std::string> closure;


	/** Replace symbols in `value` with their values.  This doesn't
	 * setup back-patches or lexical scope, it's just meant to define
	 * the special forms (like Lambda) before type inference occurs.
	 */
	void assign_forms(SymbolMap& env,
	                  Any& value)
	{
		switch(value._tag)
			{
			case tag<Ast>::value:
			case tag<AstData>::value:
				{
					for(auto& item : unwrap_astish(value))
						{ assign_forms(env, item); }
				}
			case tag<Symbol>::value:
				/* Just assign forms; ints and other atoms are also safe.  Just no Asts. */
				auto sym_value = env.current_value(value);

				if(sym_value.second && !is<Ast>(sym_value.first))
					{ value = sym_value.first; }

				break;
			}
	}

	/** Replace a symbol with its value (where that makes sense),
	 * setup backpatches, and keep track of what the closure will need
	 * to capture.
	 *
	 * @param closure: The closure needs to include symbol which are
	 * not in the toplevel or local lambda.
	 */
	void assign_symbol(SymbolMap& env
	                   , BackPatch& backpatch
	                   , Any& value)
	{
		auto& sym = unwrap<Symbol>(value);

		auto found_def = env.find(sym.name);
		if(found_def != env.very_end())
			{ value = found_def->second->value; }
		else
			{
				/* If we haven't found the symbol at all, it must be backpatched at the toplevel later. */
				auto to_patch = backpatch.find(sym.name);
				if(to_patch != backpatch.end())
					{ to_patch->second.push_back(&value); }
				else
					{ backpatch.emplace(sym.name, std::vector<Any*>({&value})); }
			}
		return;
	}


	/** Modify `ast` in place replacing body Symbols with Bounds
	 * pointing to their respective formal or toplevel entry
	 */
	void assign_free(SymbolMap& env,
	                 AllocatorBase &store,
	                 BackPatch &backpatch,
	                 Any& value)
	{
		switch(value._tag)
			{
			case tag<Ast>::value:
			case tag<AstData>::value:
				{
					auto ast = unwrap_astish(value);
					auto& head = ast[0];

					if(is<Symbol>(head))
						{ assign_symbol(env, backpatch, head); }

					switch(head._tag)
						{
						case tag<Define>::value:
							{
								auto &sym = unwrap<Symbol>(ast[1]);

								{
									using namespace pattern_match;
									Lambda func;
									if(match(pattern_match::ast(capture(func), astish, astish),
									         ast[2]))
										{
											auto& metadata = *func.value;

											env.define(sym, wrap<CallLambda>(&metadata));
											assign_free(env, store, backpatch, ast[2]);
										}
									else
										{
											assign_free(env, store, backpatch, ast[2]);
											if(is_astish(ast[2]))
												{ env.define(sym, wrap<Undefined>()); }
											else
												{ env.define(sym, pass_value(ast[2])); }
										}
								}

								auto found = backpatch.find(sym.name);
								if(found != backpatch.end())
									{
										// back-patch
										for(auto ptr : found->second)
											{ *ptr = wrap(&sym); }
										backpatch.erase(found);
									}
								return;
							}

						case tag<Lambda>::value:
							{
								// function metadata should have been
								// created by the type inference
								SymbolMap inner_map(&env);

								auto formals = unwrap_astish(ast[1]);
								size_t count = formals.size();
								for(auto& sym : formals)
									{
										assert(is<Symbol>(sym));
										inner_map.formal(unwrap<Symbol>(sym),
										                 --count);
									}

								assign_free(inner_map, store, backpatch, ast[2]);
							}
							return;
						default:
							for(auto& item : ast)
								{ assign_free(env, store, backpatch, item); }
						}
					return;
				}
			case tag<Symbol>::value:
				assign_symbol(env, backpatch, value);
				return;
			}
	}
}

#endif
