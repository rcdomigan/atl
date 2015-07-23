#ifndef ATL_ENVIRONMENT_HPP
#define ATL_ENVIRONMENT_HPP
/**
 * @file /home/ryan/programming/atl/environment.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Sep 23, 2014
 */

#include <map>

#include <tiny_vm.hpp>
#include <abstract_type.hpp>

namespace atl
{
	namespace lexical
	{
		struct Value
		{
			abstract_type::Type metadata;
			Any value;
			Value() = default;
			Value(Any vv, abstract_type::Type const& mm)
				: metadata(mm), value(vv)
			{}
		};

		struct Map
		{
			typedef std::map<std::string, Value> Impl;
			typedef Impl::iterator iterator;
			Impl _local;
			Map *_prev;
			GC &_gc;

			Map(Map *m) : _prev(m), _gc(m->_gc)  {}
			Map(GC &gc) : _prev(nullptr), _gc(gc) {}
			Map() = delete;

			void define(const std::string& name, Any const& value, abstract_type::Type const& type)
			{
				_local[name] = Value(value, type);
			}

			void define(const std::string& name, Any const& value)
			{ define(name, value, abstract_type::make_abstract({value._tag})); }

			iterator find(const string& k)
			{
				Impl::iterator res = _local.find(k);
				if(res == _local.end())
					{
						if(_prev == nullptr) return end();
						if( (res = _prev->find(k)) == _prev->end() )
							return _local.end();
						return  res;
					}
				return res;
			}

			// Iterate over _this_ map (doesn't do encosing scope).
			iterator end() { return _local.end(); }
			iterator begin() { return _local.begin(); }

			Any& value(const std::string& name) {
				auto i = find(name);

				if (i == end())
					throw UnboundSymbolError(std::string("Unbound symbol: ").append(name));
				else
					return i->second.value;
			}

			Any& value(Symbol& sym) {
				return value(sym.name);
			}

			void dbg();
		};

		void Map::dbg()
		{
			for(auto& pair : _local)
				{
					std::cout << pair.first << " = (: " << printer::any(pair.second.value) << " ";
					pair.second.metadata.dbg();
					std::cout << ")" << std::endl;
				}
		}

		struct MapRAII {
			Map map;
			Map **top;
			MapRAII(Map** top_) : map(*top_), top(top_) {
				*top = &map;
			}
			~MapRAII() { *top = map._prev; }
		};
	}

	struct Environment {
		std::map<tag_t, void*> type_class_map;

		GC& gc;
		lexical::Map toplevel;

		AssembleVM *pcode;
		tag_t _struct_tag;

		Environment(GC &_gc)
			: gc(_gc), toplevel(_gc), pcode(nullptr), _struct_tag(0)
		{ init_types(); }

		Environment() = delete;

		void define(const std::string& name, Any value) {
			toplevel.define(name, value);
		}

		tag_t struct_tag() { return _struct_tag++; }
	};
}

#endif
