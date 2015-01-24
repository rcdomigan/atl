#ifndef ATL_ENVIRONMENT_HPP
#define ATL_ENVIRONMENT_HPP
/**
 * @file /home/ryan/programming/atl/environment.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Sep 23, 2014
 */

#include <map>

namespace atl {
    namespace lexical {
    	struct Map {
	    typedef std::map<std::string, Any> Impl;
	    typedef Impl::iterator iterator;
	    Impl _local;
	    Map *_prev;
	    GC &_gc;

	    Map(Map *m) : _prev(m), _gc(m->_gc)  {}
	    Map(GC &gc) : _prev(nullptr), _gc(gc) {}
	    Map() = delete;


	    void define(const std::string& name, Any value) { _local[name] = value; }

	    Impl::iterator find(const string& k) {
		Impl::iterator res = _local.find(k);
		if(res == _local.end()) {
		    if(_prev == nullptr) return end();
		    if( (res = _prev->find(k)) == _prev->end() )
			return _local.end();
		    return  res; }
		return res; }

            // Iterate over _this_ map (doesn't do encosing scope).
	    Impl::iterator end() { return _local.end(); }
	    Impl::iterator begin() { return _local.begin(); }

            Any& value(const std::string& name) {
		auto i = find(name);

		if (i == end())
		    throw UnboundSymbolError(std::string("Unbound symbol: ").append(name));
		else
		    return i->second;
            }

	    Any& value(Symbol& sym) {
                return value(sym.name);
            }

            void print();
	};

        void Map::print() {
            for(auto& pair : _local)
                std::cout << pair.first << " = " << printer::any(pair.second) << endl;
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

	Environment(GC &_gc) : gc(_gc), toplevel(_gc) { init_types(); }
	Environment() = delete;

        void define(const std::string& name, Any value) {
            toplevel.define(name, value);
        }
    };
}

#endif
