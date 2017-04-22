#ifndef ATL_UTILITY_HPP
#define ATL_UTILITY_HPP
/**
 * @file /home/ryan/programming/nnet/utility.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Apr 10, 2014
 *
 * General purpose functions and classes, mostly copied from my nnet project.
 * Boost::range could almost replace this (the Range struct is mostly what I want from here) but doesn't
 * quite do what I want.  If it gets updated to handle parallell traversal of ranges (some kind of zip functionallity)
 * I can probably ditch this file.
 */
#include <array>
#include <tuple>
#include <algorithm>
#include <iostream>
#include <type_traits>
#include <utility>
#include <limits>

#include <boost/mpl/vector.hpp>

template<long Numerator, long Denominator, class Numeric = double>
struct rational_c {

    template<class NewNumeric>
    struct change_value_type { typedef rational_c<Numerator, Denominator, NewNumeric> type; };

    typedef Numeric Num;
    constexpr static Num value = (Num)Numerator / (Num)Denominator;
    constexpr static Num value_squared = value * value;
};

////////////////////////////////
//  _____ __  __ ____  _      //
// |_   _|  \/  |  _ \| |     //
//   | | | |\/| | |_) | |     //
//   | | | |  | |  __/| |___  //
//   |_| |_|  |_|_|   |_____| //
////////////////////////////////
namespace tmpl
{
	template<size_t N>
	struct intC : public std::integral_constant<size_t, N>::type {};

	/* Well, std library has some metaprogramming stuff now, but it's missing a lot. */
	struct _void
	{
		typedef _void type;
		/* eats constructor args so I can use it to terminate
		   recursive constructors */
		template<class ... T>
		_void(T...) {}
	};

	template<class Container, class Tag = typename Container::tag_type>
	struct Size;

	template<class Container, size_t Position, class Tag = typename Container::tag_type>
	struct AtC;

	template<class T> struct Identity { typedef T type; };

	template<class T, class U = typename T::type>
	struct IsVoid : std::false_type {};

	template<class T>
	struct IsVoid<T,_void> : std::true_type {};

	namespace detail
	{
		template<class T> struct GetDepth : public intC<T::depth + 1>::type {};

		template<> struct GetDepth<_void> : public intC<0>::type {};
	}


	template<class T>
	struct Eval : public T::type {};


	template<template<class ... U> class Fn, class ... T>
	struct Apply : Fn<typename T::type...> {};


	/**********************************/
	/*  __  __ ____  _     _     _    */
	/* |  \/  |  _ \| |   (_)___| |_  */
	/* | |\/| | |_) | |   | / __| __| */
	/* | |  | |  __/| |___| \__ \ |_  */
	/* |_|  |_|_|   |_____|_|___/\__| */
	/**********************************/
	/* meta-programming-list; a list of types which can be instantiated to make a sort of tuple.
	   Having said that; maybe I should generate a std::tuple and use the boost libs.  Well, I've already got this going;
	   so sticking with it for now
	*/
	struct MPList_tag { typedef MPList_tag tag_type; };

	template<class List> struct Car : public Identity<typename List::Car> {};
	template<> struct Car<_void> : public _void {};

	template<class List> struct Cdr : public Identity<typename List::Cdr> {};
	template<> struct Cdr<_void> : public _void {};

	template<class T, class Next>
	struct Cons : public MPList_tag {
		typedef Cons<T, Next> type;

		typedef T Car;
		typedef Next Cdr;

		/* I can use the instatiated list at run-time */
		T value;
		Next next;

		static const size_t depth = detail::GetDepth<Next>::value;
	};

	template<class ... List> struct MPList;

	template<class Head, class ... Tail>
	struct MPList<Head, Tail...> : public Cons<Head, typename MPList<Tail...>::type> {};

	template<> struct MPList<> : public _void {};

	template<class Fn, class Init, class InputList>
	struct Fold {
		static_assert( std::is_same<typename InputList::type::tag_type, MPList_tag>::value
		               , "Fold requires an MPList or Cons cell.");
		typedef typename InputList::type List;
		typedef typename
		Fn::template Apply<typename Car<List>::type
		                   , typename Fold<Fn, Init, typename Cdr<List>::type>::type
		                   > type;
	};

	template<class Fn, class Init>
	struct Fold<Fn,Init,_void> { typedef typename Init::type type; };

	template<class Flatten, class List>
	struct map_MPList {
		template<class Fn>
		void apply(Fn fn, List& ll) {
			Flatten::apply(fn, ll);

			map_MPList<Flatten, typename Cdr<List>::type
			           >::apply(fn, ll.next);
		}
	};


	template<class List>
	struct Size<List, MPList_tag> {
		const static size_t value = List::depth;
	};

	template<size_t N, class NNs>
	struct ConsNums {
		typedef NNs Next;
		static const size_t value = N;
		static const size_t depth = detail::GetDepth<Next>::value;

		typedef ConsNums<N, NNs> type;
	};

	template<size_t ... Rest> struct Nums;

	template<size_t NN, size_t ... Rest>
	struct Nums<NN, Rest...> {
		typedef ConsNums<NN, typename Nums<Rest...>::type > type; };

	template<> struct Nums<> : public _void {};

	template<class List>
	struct LengthMPList : public intC<List::depth> {};

	/****************************************************/
	/*  ____            _   __        __                */
	/* |  _ \ __ _  ___| | _\ \      / / __ __ _ _ __   */
	/* | |_) / _` |/ __| |/ /\ \ /\ / / '__/ _` | '_ \  */
	/* |  __/ (_| | (__|   <  \ V  V /| | | (_| | |_) | */
	/* |_|   \__,_|\___|_|\_\  \_/\_/ |_|  \__,_| .__/  */
	/*                                          |_|     */
	/****************************************************/
	struct PackWrap_tag { typedef PackWrap_tag tag_type; };

	/* don't instantiate.  I think I have to make it instantiable to use
	   inheritance, just don't do it. */
	template<class ... Args>
	struct PackWrap : public PackWrap_tag {
		typedef PackWrap<Args ...> type;
	};


	template<class ... Args>
	struct Size< PackWrap<Args...>, PackWrap_tag>
	{
		static constexpr size_t value = sizeof...(Args);
	};


	template<class ... Args> struct PackHead : public _void {};

	template<class Head, class ... Tail>
	struct PackHead<Head, Tail...> : public Identity<Head> {};


	template <class ... Args> struct PackTail : public _void {};

	template<class Head, class ... Tail>
	struct PackTail<Head, Tail...> : public PackWrap<Tail...> {};


	template<size_t N, class ... Args>
	struct NthArg;

	template<size_t N, class Head, class ... Tail>
	struct NthArg<N, Head, Tail...>
	{
		typedef NthArg<N - 1, Tail...> type;
	};

	template<class Head, class ... Tail>
	struct NthArg<0, Head, Tail...>
	{
		typedef Head type;
	};


	template<class ... Args>
	struct Pack2Array
	{
		static constexpr std::array< typename PackHead<Args...>::type::value_type
		                             , sizeof...(Args)
		                             > value{{ Args::value... }};
	};


	/* http://loungecpp.wikidot.com/tips-and-tricks%3Aindices` */
	template<std::size_t ...Is> struct Indexer {};

	template <std::size_t N, std::size_t... Is>
	struct BuildIndicies
		: BuildIndicies<N-1, N-1, Is...>
	{};

	template <std::size_t... Is>
	struct BuildIndicies<0, Is...>
	{ typedef Indexer<Is...> type; };
}


template<class Fn, class ... Args, std::size_t... Index>
static typename std::result_of<Fn (Args...)>::type _apply_tuple(Fn& fn, std::tuple<Args...>& args, tmpl::Indexer<Index...>)
{ return fn(std::get<Index>(args)...); }

template<class Fn, class ... Args>
static typename std::result_of<Fn (Args...)>::type apply_tuple(Fn& fn, std::tuple<Args...>& args)
{
	return _apply_tuple(fn, args, typename tmpl::BuildIndicies<sizeof...(Args)>::type {});
}


/**************************************/
/*     _                              */
/*    / \   _ __ _ __ __ _ _   _ ___  */
/*   / _ \ | '__| '__/ _` | | | / __| */
/*  / ___ \| |  | | | (_| | |_| \__ \ */
/* /_/   \_\_|  |_|  \__,_|\__, |___/ */
/*                         |___/      */
/**************************************/
template<int Start = 0, int End = 0>
struct Map {
    template<class FN, class T, class ... Aux>
    static void apply(FN fn , T& dst, Aux& ... aux) {
	const static size_t last =
	    End <= 0 ? std::min( {std::tuple_size< T >::value, std::tuple_size<Aux>::value ...} ) + End
	    : End;

	for(size_t i = Start; i < last ; ++i)
	    fn(dst[i], aux[i]...);
    }
};

template<class FN, class T, class ... Aux>
void map_array(FN fn , T& dst, Aux& ... aux) { Map<>::apply(fn,dst,aux...); }

template<class A>
std::ostream& print_array(const A& aa, std::ostream& out) {
    using namespace std;
    if(aa.size() == 0)
	return cout << "[]";

    cout << "[" << aa[0];
    Map<1>::apply([&](float ff) { out << ", " << ff; }, aa);
    return cout << "]";
}

template<class T, size_t N>
std::ostream& operator<<(const std::array<T, N>& aa, std::ostream& out) {
    return print_array(aa, out);
}

template<class A>
std::ostream& print_array(const A& aa) { return print_array(aa, std::cout); }

/*****************************/
/*  _____            _       */
/* |_   _|   _ _ __ | | ___  */
/*   | || | | | '_ \| |/ _ \ */
/*   | || |_| | |_) | |  __/ */
/*   |_| \__,_| .__/|_|\___| */
/*            |_|            */
/*****************************/
template<class FN, class Tup, size_t elem = 0, size_t last = std::tuple_size<Tup>::value>
struct MapTuple {
    static void apply(Tup& tup) {
	FN::apply( std::get<elem>(tup) );
	MapTuple<FN,Tup,elem + 1, last>::apply(tup);
    } };

template<class FN, class Tup, size_t elem>
struct MapTuple<FN, Tup, elem, elem> { static void apply(Tup&) {} };


template<class FN, class Tup>
void map_tuple(Tup& tup) { MapTuple<FN, typename std::remove_reference<Tup>::type >::apply(tup); }



template<class Tup, size_t elem = 0, size_t last = std::tuple_size<Tup>::value>
struct ForeachTuple
{
	template<class Fn>
	static void apply(Fn& fn, Tup const& tup)
	{
		fn( std::get<elem>(tup) );
		ForeachTuple<Tup,elem + 1, last>::apply(fn, tup);
	}

	template<class Fn>
	static void apply(Fn& fn, Tup& tup)
	{
		fn( std::get<elem>(tup) );
		ForeachTuple<Tup,elem + 1, last>::apply(fn, tup);
	}
};

template<class Tup, size_t elem>
struct ForeachTuple<Tup, elem, elem>
{
	template<class Fn>  static void apply(Fn&, Tup const&) {}
	template<class Fn>  static void apply(Fn&, Tup&) {}
};

template<class Fn, class Tup>
void foreach_tuple(Fn& fn, Tup const& tup)
{ ForeachTuple<typename std::remove_reference<Tup>::type >::apply(fn, tup); }

template<class Tup, size_t elem = 0, size_t last = std::tuple_size<Tup>::value>
struct AndTuple
{
	template<class Fn>
	static bool apply(Fn& fn, Tup& tup)
	{
		if(fn( std::get<elem>(tup) ))
			{ return AndTuple<Tup,elem + 1, last>::apply(fn, tup); }
		else
			{ return false; }
	}
};

template<class Tup, size_t elem>
struct AndTuple<Tup, elem, elem>
{
	template<class Fn>
	static bool apply(Fn& fn, Tup& tup) { return true; }
};

/** Fold `fn` across the elements of a tuple `tup` until either
 * fn(elem) produces a falsy value or all of fn(elem) produce true.
 *
 * @tparam Fn: Particular type of fn (must be elem-type -> boolish)
 * @tparam Tup: The tuple type
 * @param fn: function that takes the type of tups element and returns bool
 * @param tup: tuple to fold across
 * @return: true of fn returns true for all the elements of tup
 */
template<class Fn, class Tup>
bool and_tuple(Fn& fn, Tup& tup)
{ return AndTuple<typename std::remove_reference<Tup>::type >::apply(fn, tup); }


/******************************/
/*   ____                _    */
/*  / ___|___  _ __  ___| |_  */
/* | |   / _ \| '_ \/ __| __| */
/* | |__| (_) | | | \__ \ |_  */
/*  \____\___/|_| |_|___/\__| */
/******************************/
template<class T>
struct GetConstIterator : public tmpl::Identity<typename T::const_iterator> {};

template<class T>
struct GetMutableIterator : public tmpl::Identity<typename T::iterator> {};

/* Tools for dealing with const */
template<class Container, class NoRef = typename std::remove_reference<Container>::type>
struct GetIterator
    : public std::conditional< std::is_const<NoRef>::value
			       , GetConstIterator<NoRef>
			       , GetMutableIterator<NoRef>
			       >::type {};


/*********************************/
/*  ____                         */
/* |  _ \ __ _ _ __   __ _  ___  */
/* | |_) / _` | '_ \ / _` |/ _ \ */
/* |  _ < (_| | | | | (_| |  __/ */
/* |_| \_\__,_|_| |_|\__, |\___| */
/*                   |___/       */
/*********************************/
struct IncItr { template<class T> static void apply(T&& t) { ++t; } };


// Not very idomatic; the operator* returns the tuple containing all the _iterators_.
// TODO: can I construct a tuple of rvalues from the tuple of iterators?
template<class ... Types>
struct RangeItr
{
    typedef std::tuple< Types ... > value_type;
    value_type _itrs;

    RangeItr() = delete; // : _itrs(Types()...) {}
    RangeItr(Types const& ... input ) : _itrs(input ...) {}

    RangeItr& operator++()
    {
        map_tuple<IncItr>(_itrs);
        return *this;
    }
    value_type& operator*() { return _itrs; }

    /* note: only compairs the _first_ element (so I can have ranges of different sizes; stick the shortest range in
       the first position) */
    bool operator!=(const RangeItr<Types...>& other)
    {
        return std::get<0>(other._itrs) != std::get<0>(_itrs);
    }
};


template<class Itr>
struct Range_base {
    typedef Range_base<Itr> type;
    typedef Itr iterator;
    typedef const Itr const_iterator;
    Itr _begin, _end;

    typedef decltype(*_begin) value_type;

    iterator& begin() { return _begin; }
    iterator& end() { return _end; }
    const iterator& begin() const { return _begin; }
    const iterator& end() const { return _end; }
    bool empty() const { return _begin == _end; }

    Range_base() = delete;
    Range_base(const Range_base&) = default;
    ~Range_base() = default;

    Range_base(Itr begin, Itr end)
	: _begin(begin), _end(end) {}

    /* don't think this makes sense for zipped ranges */
    void push_back(value_type vv) { *_end = vv; ++_end; }
    value_type& operator[](size_t pos) { return *(_begin + pos); }
    const value_type& operator[](size_t pos) const { return *(_begin + pos); }
};


template<class T>
struct RemoveConstReference
    : public std::remove_const<typename std::remove_reference<T>::type>
{};


template<bool is_const, class ... Ranges>
struct Zipper
{
    typedef RangeItr<typename std::remove_const<typename GetIterator<Ranges>::type>::type ... > Itr;
    typedef Zipper<is_const, Ranges...> zipper_type;

    struct ref_type
        : public Range_base<Itr>
    {
        typedef typename zipper_type::Itr Itr;
        typedef Range_base<Itr> base_type;

        ref_type(Ranges&& ...  pp)
            : base_type(Itr(pp.begin() ...), Itr(pp.end()...))
        {}

        ref_type(ref_type const&) = default;
    };

    struct const_type
        : public Range_base<Itr>
    {
        typedef typename zipper_type::Itr Itr;
        typedef Range_base<Itr> base_type;

        const_type(Ranges const& ...  pp)
            : base_type(Itr(pp.begin() ...), Itr(pp.end()...))
        {}

        const_type(const_type const&) = default;
    };

    typedef typename std::conditional<is_const,
                                      const_type,
                                      ref_type
                                      >::type type;
};

template<class ... Ranges>
typename Zipper<false, Ranges...>::type zip(Ranges& ... rr)
{
    return typename Zipper<false, Ranges...>::type(std::forward<Ranges>(rr)...);
}

template<class ... Ranges>
typename Zipper<true, Ranges const...>::type zip(Ranges const& ... rr)
{
    return typename Zipper<true, Ranges const...>::type(std::forward<Ranges const>(rr)...);
}


template<class IteratorType>
struct Range : public Range_base<IteratorType> {

    typedef Range_base<IteratorType> base_type;
    typedef typename base_type::iterator iterator;
    typedef typename base_type::value_type value_type;

    Range(const std::pair<iterator,iterator>& pp)
	: base_type(pp.first, pp.second) {}

    Range(iterator aa, iterator bb)
	: base_type(aa, bb) {}

    Range() = default;
    Range(const Range&) = default;
    ~Range() = default;

    size_t size() const { return base_type::end() - base_type::begin(); }
	bool empty() const { return base_type::end() == base_type::begin(); }
};

template<class Itr>
Range<Itr> begin_end_to_range(Itr begin, Itr end) {
    return Range<Itr>(begin, end);
}

template<class Itr>
Range<Itr> make_range(Itr begin, Itr end) {
    return Range<Itr>(begin, end);
}

template<class Container>
Range<typename GetIterator<Container>::type > make_range(Container& cc) {
    return Range<typename GetIterator<Container>::type>(cc.begin(), cc.end());
}

template<class T>
Range<typename GetIterator<T>::type >  slice(T&& container, size_t begin) {
    return begin_end_to_range<typename GetIterator<T>::type>(container.begin() + begin, container.end());
}

template<class T>
Range<typename GetIterator<T>::type > slice(T&& tt, int begin, int end) {
    if(end < 0) return T(tt.begin() + begin, tt.end() + end);
    else return T(tt.begin() + begin, tt.begin() + end);
}

// Count up indefinitely
struct CountingRange {
    typedef size_t value_type;

    struct Counter {
	value_type _value;
	Counter(value_type vv) : _value(vv) {}
	Counter& operator++() {
	    ++_value;
	    return *this;
	}

	value_type operator*() { return _value; }

	bool operator!=(const Counter& other) const { return other._value != _value; }
    };
    typedef Counter iterator;
    typedef Counter const_iterator;

    iterator begin() const { return Counter(0); }
    iterator end() const { return Counter(std::numeric_limits<value_type>::max()); }
};


// It would be convinient to use the new for(value:container) form and
// have the value be a pointer into the container
template<class Container>
struct PointerRange {
    struct iterator {
	typedef typename Container::iterator value_type;
	value_type value;
	iterator(value_type vv) : value(vv) {}

	value_type operator*() { return value; }
	iterator& operator++() { ++value; return *this; }
	bool operator!=(const iterator& other) { return other.value != value; }
    };

    Container& container;
    PointerRange(Container& c) : container(c) {}

    iterator begin() { return iterator(container.begin()); }
    iterator end() { return iterator(container.end()); }
};

template<class Container>
PointerRange<Container> pointer_range(Container& cc) { return PointerRange<Container>(cc); }

template<class Output, class Fn, class Input>
Output map_range(Fn fn, Input in)
{
    Output out;
    for(auto& vv : in)
        out.push_back(fn(vv));

    return out;
}


namespace detail
{
	template<class T>
	struct Deref
	{ static T& a(T* item) { return *item; } };

	template<class T>
	struct Passthrough
	{
		static T&& a(T&& item) { return std::move(item); }
		static T& a(T& item) { return std::move(item); }
	};
}


template<class T, class R = typename std::remove_pointer<T>::type>
R& deref(T&& item)
{
	return std::conditional<std::is_pointer<T>::value,
	                        detail::Deref<T>,
	                        detail::Passthrough<T>
	                        >::a(item);
}


#endif
