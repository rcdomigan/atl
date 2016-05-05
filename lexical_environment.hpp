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

#include <exception.hpp>
#include <type.hpp>
#include <helpers.hpp>
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
		typedef std::pair<Symbol*, Any> value_type;
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

		Map _local;
		SymbolMap *up;
		size_t assigned_type;

		SymbolMap(SymbolMap *up_) : up(up_) {
			if(!up_)
				{ assigned_type = 0; }
			else
				{ assigned_type = up_->assigned_type; }
		}

		SymbolMap(SymbolMap const&) = delete;
		SymbolMap() = delete;

		~SymbolMap() { if(up) { up->assigned_type = assigned_type; } }

		void formal(Symbol& sym)
		{
			sym.type = wrap<Type>(++assigned_type);
			auto rval = _local.emplace(sym.name, make_pair(&sym, wrap<Bound>(&sym)));
			if(!rval.second)
				{ throw RedefinitionError(std::string("Can't redefine ").append(sym.name)); }
		}

		all_iterator very_end()
		{
			if(up)
				{ return up->very_end(); }
			else
				{ return _local.end(); }
		}

		all_iterator find(std::string const& k)
		{
			auto itr = _local.find(k);
			if(itr == end())
				{
					if(up)
						{ return up->find(k); }
					else
						{ return all_iterator(itr); }
				}
			return itr;
		}

		iterator end() { return _local.end(); }
		iterator begin() { return _local.begin(); }

		const_iterator end() const { return _local.end(); }
		const_iterator begin() const { return _local.begin(); }


		Any& value(const std::string& name)
		{
			auto i = find(name);

			if (i == very_end())
				throw UnboundSymbolError(std::string("Unbound symbol: ")
				                         .append(name));
			else
				return i->second.second;
		}

		Symbol& symbol(std::string const& name)
		{
			auto vv = find(name);
			if(vv == very_end())
				{ throw UnboundSymbolError(std::string("Unbound symbol: ").append(name)); }
			return *vv->second.first;
		}

		size_t count(std::string const& key)
		{
			if(find(key) != very_end())
				{ return 1; }
			return 0;
		}

		value_type& operator[](std::string const& key) { return _local[key]; }

		void dbg()
		{
			for(auto& pair : _local)
				{
					std::cout << pair.first
					          << " = (: "
					          << printer::any(pair.second.second)
					          << ")"
					          << std::endl;
				}
		}
	};

	typedef std::map<std::string, std::vector<Any*> > FreeSymbols;


	/** Modify `ast` in place replacing body Symbols with Bounds
	 * pointing to their respective formal or toplevel entry
	 *
	 * @param env:
	 * @param factory:
	 * @param free:
	 * @param ast:
	 * @return:
	 */
	void assign_free(SymbolMap& env,
	                 AllocatorBase &factory,
	                 FreeSymbols &free,
	                 Slice ast)
	{

		// If symbol has a value defined in `env` set `item` to it,
		// otherwise add the symbol to the free list.
		auto handle_symbol = [&free, &env](Any& item)
			{
				auto& sym = unwrap<Symbol>(item);
				auto found_def = env.find(sym.name);
				if(found_def != env.very_end())
					{ item = found_def->second.second; }
				else
					{
						auto found_free = free.find(sym.name);
						if(found_free != free.end())
							{ found_free->second.push_back(&item); }
						else
							{ free.emplace(sym.name, std::vector<Any*>({&item})); }
					}
			};

		auto head = pass_value(ast[0]);

		switch(head._tag)
			{
			case tag<Slice>::value:
				assign_free(env,
				            factory,
				            free,
				            unwrap<Slice>(head));
				break;

			case tag<Define>::value:
				{
					if(!is<Symbol>(ast[1]))
						{
							throw WrongTypeError
								(std::string
								 ("define needs a symbol as its first arg; got a ")
								 .append(type_name(ast[1]._tag)));
						}

					auto &sym = unwrap<Symbol>(ast[1]);
					env.formal(sym);

					auto found = free.find(sym.name);
					if(found != free.end())
						{
							// back-patch
							for(auto ptr : found->second)
								{ *ptr = wrap<Bound>(&sym); }
							free.erase(found);
						}

					auto value = pass_value(ast[2]);
					switch (value._tag)
						{
						case tag<Symbol>::value:
							handle_symbol(*value.any);
							break;

						case tag<Slice>::value:
							assign_free(env,
							            factory,
							            free,
							            value.slice);
							break;
						}
					return;
				}

			case tag<Symbol>::value:
				handle_symbol(ast[0]);
				if(!is<Lambda>(ast[0]))
					{ break; }

			case tag<Lambda>::value:
				{
					SymbolMap inner_map({&env});

					for(auto& item : pass_value(ast[1]).slice)
						{
							assert(is<Symbol>(item));
							inner_map.formal(unwrap<Symbol>(item));
						}

					assign_free(inner_map,
					            factory,
					            free,
					            pass_value(ast[2]).slice);
				}
				return;
			}

		// Handle the rest (if applicable)
		if(ast.size() > 1)
			{
				for(auto& raw_item : slice(ast, 1))
					{
						auto item = pass_value(raw_item);
						switch(item._tag)
							{
							case tag<Slice>::value:
								assign_free(env,
								            factory,
								            free,
								            unwrap<Slice>(item));
								break;
							case tag<Symbol>::value:
								handle_symbol(raw_item);
								break;
							}
					}
			}
	}
}

#endif
