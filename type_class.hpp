#ifndef ATL_TYPE_CLASS_HPP
#define ATL_TYPE_CLASS_HPP
// @file /home/ryan/programming/atl/type_class.hpp
// @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
// Created on Sep 23, 2014
//
// Some primitive type_classes for the evautator to harass.


#include "./type.hpp"
#include "./gc.hpp"
namespace atl {
    template<class T>
    struct typeclass_tag;

    struct Applicable;
    template<> struct typeclass_tag<Applicable> : public std::integral_constant<tag_t, 0> {};

    struct Applicable {
	typedef typename PrimitiveRecursive::Fn Fn;
	typedef std::map<tag_t, Fn> Map;
	Map *map;

	Applicable(Environment &env) {
	    map = reinterpret_cast<Map*>(env.type_class_map[typeclass_tag<Applicable>::value]);
	}

	template<class T>
	void set(const Fn& fn) {
	    (*map)[tag<T>::value] = fn;
	}

	Fn& operator[](unsigned int key) {
	    return map->at(key);
	}
    };


    struct Sequence;
    template<> struct typeclass_tag<Sequence>
	: public std::integral_constant<tag_t, 0> {};
    struct Sequence {
	typedef std::function<Any (Any, long)> Slice;
	typedef std::function<Ast (Any, GC::DynamicVector&)> ToAst;

	struct Methods {
	    Slice slice;
	    ToAst to_ast;
	};

	typedef std::map<tag_t, Methods> Map;
	Map *map;

	Sequence(Environment &env) {
	    map = reinterpret_cast<Map*>
		(env.type_class_map[typeclass_tag<Sequence>::value]);
	}

	Methods& operator[](unsigned int key) { return (*map)[key]; }
    };


    // struct Monad;
    // template<> struct typeclass_tag<Monad> : public std::integral_constant<tag_t, 1> {};

    // struct CxxMonad {
    // 	CxxMonad bind();
    // 	CxxMonad unit();
    // };

    // struct Monad {
    // 	Map *map;
    // 	Monad(Environment &env) {
    // 	    map = reinterpret_cast<Map*>(env.type_class_map[typclass_tag<Monad>::value]);
    // 	}

    // 	MonadInstance& operator[](unsigned int )
    // };
}

#endif
