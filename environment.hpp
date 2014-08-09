#ifndef ATL_ENVIRONMENT_HPP
#define ATL_ENVIRONMENT_HPP
/**
 * @file /home/ryan/programming/atl/environment.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Sep 23, 2014
 */

namespace atl {
    namespace lexical {
    	struct Map {
	    /* todo: I am assuming I can mutate the value of a pair if I have the pointer to its .second member (the 'Any').
	       That will for sure not be true for flat containers, and I should probably start using a 'Any*' instead. */
	    typedef map<string, Any> Impl;
	    typedef Impl::iterator iterator;
	    Impl _local;
	    Map *_prev;
	    GC &_gc;

	    Map(Map *m) : _prev(m), _gc(m->_gc)  {}
	    Map(GC &gc) : _prev(nullptr), _gc(gc) {}
	    Map() = delete;


	    // defines string s in the current enviornment, but does
	    // not create an assigning S-expr.
	    // \internal
	    // @param s: the name I'm binding at the current scope.
	    // @return: the new Undefined
	    Any& predefine(const string &s) {
		auto u = _gc.make<Undefined*>();
		auto inserted = _local.insert( Impl::value_type(s, wrap(u)) );

		auto itr = inserted.first;

		/* use the _backtrack mechanism to replace the Undefined in the Map when it gets a
		   value */
		if(_prev == nullptr)
		    u->_backtrack.push_back(&itr->second);

		return itr->second; }

	    // Sets the symbol s to value
	    // \internal
	    // @param s: the (name of the) symbol to bind
	    // @param value: the expression which will bind to the symbol
	    void define(const string& s, Any value) { _local.insert( Impl::value_type(s, value) ); }

	    Impl::iterator find(string k) {
		Impl::iterator res = _local.find(k);
		if(res == _local.end()) {
		    if(_prev == nullptr) return end();
		    if( (res = _prev->find(k)) == _prev->end() )
			return _local.end();
		    return  res; }
		return res; }

	    Impl::iterator end() { return _local.end(); }
	};
    }

    struct Environment {
	std::map<tag_t, void*> type_class_map;
	lexical::Map toplevel;

	Environment(GC &gc) : toplevel(gc) {}
	Environment() = delete;
    };
}

#endif
