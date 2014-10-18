#ifndef ATL_TYPES_HH
#define ATL_TYPES_HH
/**
 * @file /home/ryan/programming/atl/types.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jul 07, 2013
 */

#include <vector>
#include <set>
#include <type_traits>
#include <string>
#include <algorithm>

#include <boost/mpl/at.hpp>
#include <boost/mpl/size.hpp>
#include <boost/mpl/set.hpp>

#include <boost/mpl/vector.hpp>
#include <boost/mpl/vector/vector30.hpp>

#include <boost/mpl/equal.hpp>
#include <boost/mpl/sizeof.hpp>
#include <boost/mpl/int.hpp>

#include <boost/mpl/for_each.hpp>
#include <boost/mpl/placeholders.hpp>

#include <boost/preprocessor/seq/seq.hpp>
#include <boost/preprocessor/seq/size.hpp>
#include <boost/preprocessor/seq/for_each_i.hpp>
#include <boost/preprocessor/seq/for_each.hpp>
#include <boost/preprocessor/seq/enum.hpp>
#include <boost/preprocessor/stringize.hpp>

#include "./utility.hpp"

namespace mpl = boost::mpl;

namespace atl {
    typedef size_t tag_t;

    /******************************/
    /*  _____          _ _        */
    /* |_   _| __ __ _(_) |_ ___  */
    /*   | || '__/ _` | | __/ __| */
    /*   | || | | (_| | | |_\__ \ */
    /*   |_||_|  \__,_|_|\__|___/ */
    /******************************/
    template<class T> struct tag;
    template<class T>
    struct Name { constexpr static const char* value = "#<UNKNOWN>"; };

    template<class T>
    struct is_atl_type : public std::false_type {};

    template<class T>
    struct is_immediate : public std::is_integral<T>::type {};

    /**
     * Trait class for types which can be derived from an Any with a
     * reinterpret_cast
     * @tparam T: type under consideration
     */
    template<class T>
    struct is_reinterperable : public std::false_type {};


#define ATL_IMMEDIATE_SEQ (Null)(Any)(Fixnum)(Pointer)(If)(Define)(Bool)(DefineMacro)(Quote)(Lambda)(Type)
#define ATL_REINTERPERABLE_SEQ (Ast)(Data)
#define ATL_PIMPL_SEQ (CxxArray)(Slice)(String)(Symbol)(Procedure)(Macro)(Undefined)(PCode)
#define ATL_TYPES_SEQ ATL_IMMEDIATE_SEQ ATL_REINTERPERABLE_SEQ ATL_PIMPL_SEQ(PrimitiveRecursive)(Mark)(Parameter)

#define M(r, _, i, elem)						\
    struct elem;							\
    template<> struct is_atl_type<elem> : public std::true_type {};	\
    template<> struct Name<elem> { constexpr static const char* value = BOOST_PP_STRINGIZE( elem ) ; }; \
    template<> struct tag<elem> {					\
	typedef tag<elem> type;						\
	typedef unsigned int value_type;				\
	const static value_type value = i;				\
	operator unsigned int () { return value; }			\
    };

    BOOST_PP_SEQ_FOR_EACH_I(M, _, ATL_TYPES_SEQ)
#undef M

#define M(r, _, i, elem) template<> struct is_immediate<elem> : public std::true_type {};
    BOOST_PP_SEQ_FOR_EACH_I(M, _, ATL_IMMEDIATE_SEQ)
#undef M

    typedef mpl::vector24< BOOST_PP_SEQ_ENUM( ATL_TYPES_SEQ )  > TypesVec;

    /****************************************************/
    /*  ____        __ _       _ _   _                  */
    /* |  _ \  ___ / _(_)_ __ (_) |_(_) ___  _ __  ___  */
    /* | | | |/ _ \ |_| | '_ \| | __| |/ _ \| '_ \/ __| */
    /* | |_| |  __/  _| | | | | | |_| | (_) | | | \__ \ */
    /* |____/ \___|_| |_|_| |_|_|\__|_|\___/|_| |_|___/ */
    /****************************************************/
    struct Any {
	tag_t _tag;
	void *_value;

	Any(const Any&) = default;
	constexpr Any() : _tag(tag<Any>::value) , _value(nullptr) {}
	constexpr Any(tag_t t, void *v) : _tag(t) , _value(v) {}
	constexpr Any(tag_t t) : _tag(t) , _value(nullptr) {}
    };
    template<> struct is_reinterperable<Any> : public std::true_type {};

    bool operator==(const Any& aa, const Any& bb) {
	return (aa._tag == bb._tag) && (aa._value == bb._value);
    }

    /**
     * A null-valued Any with the tag type of T
     * @tparam T:
     */
    template<class T>
    struct nil {
	static constexpr Any value(){ return Any(tag<T>::value, nullptr); }
    };

    struct Null {
	tag_t _tag;
	void *_value;

	Null() : _tag(0), _value(nullptr) {}
    };
    template<> struct is_reinterperable<Null> : public std::true_type {};

    /************************************/
    /** ___   immediate                **/
    /**  | ._ _ ._ _  _  _|o _._|_ _   **/
    /** _|_| | || | |(/_(_||(_| |_(/_  **/
    /************************************/
    struct Define {
	tag_t _tag;
	Define() : _tag(tag<Define>::value) {}  };

    struct Fixnum {
	tag_t _tag;
	long _value;

	Fixnum() noexcept : _tag( tag<Fixnum>::value ) , _value(0) {}
	Fixnum(long value) : _tag( tag<Fixnum>::value ), _value(value) {}
    };
    template<> struct is_reinterperable<Fixnum> : public std::true_type {};

    /*************************/
    /*  ____              _  */
    /* | __ )  ___   ___ | | */
    /* |  _ \ / _ \ / _ \| | */
    /* | |_) | (_) | (_) | | */
    /* |____/ \___/ \___/|_| */
    /*************************/
    struct Bool {
	tag_t _tag;
	void *_value;
	Bool() : _tag(tag<Bool>::value), _value(nullptr) {}
    };
    Any atl_true() { return Any(tag<Bool>::value, (void*)true); }
    Any atl_false() { return Any(tag<Bool>::value, (void*)false); }

    struct Pointer {
	tag_t _tag;
	Any *_value;

	Pointer() : _tag(tag<Pointer>::value), _value(NULL) {}
	Pointer(Any *value) : _tag(tag<Pointer>::value), _value(value) {}

	Any& value() { return *_value; }
	const Any& value() const { return *_value; }
    };
    template<> struct is_reinterperable<Pointer> : public std::true_type {};

    /*********************/
    /**  _ __     _     **/
    /** |_)| |\/||_)|   **/
    /** | _|_|  ||  |_  **/
    /**           pimpl **/
    /*********************/
    struct Symbol {
	std::string _value;

	Symbol(const std::string& value) : _value(value) {}
    };

    struct String {
	std::string _value;

	String() : _value() {}

	template<class T>
	String(T init) : _value(init) {}
    };

    template<> struct
    Name<std::string*> : public Name<String> {};

    template<> struct
    tag<std::string*> : public tag<String> {};

    struct Undefined {
	typedef std::vector<Any*> Backtrack;
	Backtrack _backtrack;
    };

    /* a bound value of a procedure */
    struct Procedure;
    struct Parameter {
	Procedure *_closure;
	size_t _offset;

	const Any& value() const;
    };

    // Wrap a C style array
    struct CxxArray {
	typedef const Any* iterator;
	typedef const Any* const_iterator;
	const Any *_begin, *_end;

	CxxArray(const Any* begin, const Any* end)
	    : _begin(begin), _end(end) {}

	CxxArray() : _begin(nullptr), _end(nullptr) {}

	const_iterator begin() const { return _begin; }
	const_iterator end() const { return _end; }

	bool empty() const { return begin() == end(); }
    };

    /* Macro and Procedure have the same data layout, but distinct tags */
    struct Procedure {
	mutable Any *_stack;	/* The Parameter's _base pointers are to this address */
	Any _body;
 	Parameter *_parameters;
	size_t _num_params;

	CxxArray _parameter_types;

	Procedure() = delete;
	Procedure(const Procedure&) = delete; /* shallow copy will invalidate Parameter pointers */

	Procedure(size_t num_params) : _stack(nullptr) {
	    _parameter_types._begin = new Any[num_params];
	    _parameter_types._end = _parameter_types.begin() + num_params;

	    _num_params = num_params;

	    _parameters = new Parameter[num_params];

	    for(size_t i = 0; i < num_params; ++i) {
		_parameters[i]._closure = this;
		_parameters[i]._offset = i;
	    }
	}

	~Procedure() { delete []_parameters; }

	struct Bind {
	    const Procedure &_proc;
	    Any *_old_args;

	    Bind(const Procedure &p, Any *new_args)
		: _proc(p), _old_args(p._stack) {
		_proc._stack = new_args;
	    }
	    ~Bind() { _proc._stack = _old_args; }
	};

	Range<Parameter*> parameters() {
	    return make_range(_parameters, _parameters + _num_params);
	}
    };

    inline const Any& Parameter::value() const { return _closure->_stack[_offset]; }

    struct Macro { Procedure _proc; };

    class EvaluateAST;

    struct PrimitiveRecursive {
	const char *_name;

	typedef std::function<Any (const Any* begin, const Any* end)> Fn;
	mutable Fn _fn;

	CxxArray _parameter_types;

	PrimitiveRecursive(const Fn& fn, const char* name)
	    : _name(name), _fn(fn) {}

	PrimitiveRecursive(const Fn& fn
			   , const char* name
			   , CxxArray range)
	    : _name(name), _fn(fn), _parameter_types(range) {}

	Any call(const Any* begin, const Any* end) const { return _fn(begin, end); }

	uintptr_t untyped_call(uintptr_t *begin, uintptr_t *end) {
	    return reinterpret_cast<uintptr_t>(*begin) + reinterpret_cast<uintptr_t>(*end);
	}
    };

    /**
     * uses an arbitrary callback if invoked during the GC's mark phase
     */
    class GC;
    struct Mark {
	std::function<void (GC&)> _mark;
	Mark(std::function<void (GC&)> mark) : _mark(mark) {}
    };

    /*********************/
    /**  _    Other     **/
    /** / \_|_|_  _ ._  **/
    /** \_/ |_| |(/_|   **/
    /*********************/
    namespace ast_helper {
	// This is really a special iterator for when I have nested
	// arrays.  I don't know when that would happen other than in
	// generating ASTs.
	template<class Value>
	struct IteratorBase {
	    Value *_value;
	    IteratorBase(Value* vv) : _value(vv) {}
	    IteratorBase() = default;

	    Value& operator*() { return *_value; }
	    const Value& operator*() const { return *_value; }

	    IteratorBase& operator++() {
		using namespace std;
		if( ((_value->_tag == tag<Ast>::value) || (_value->_tag == tag<Data>::value))
		       && (_value->_value == _value + 1))
		    _value = &*reinterpret_cast<typename
						conditional<is_const<Value>::value,
							    add_const<Ast>,
							    Identity<Ast>
							    >::type::type*
						>(_value)->end();
		else
		    ++_value;
		return *this; }

	    IteratorBase operator+(size_t n) const {
		IteratorBase itr = *this;
		for(size_t i = 0; i < n; ++i)
		    ++itr;
		return itr;
	    }

	    size_t operator-(IteratorBase other) const {
		size_t n = 0;
		for(; other != *this; ++other, ++n);
		return n;
	    }

	    bool operator!=(const IteratorBase<Value>& j) const {return j._value != _value;}

	    bool operator==(const IteratorBase<Value>& j) const {return j._value == _value;}

	    std::ostream& print(std::ostream& out) const { return out << _value; }
	};
    }

    struct Ast {
	tag_t _tag;
	Any* _value;

	Ast() = delete;
	Ast(Any *begin, Any *end)
	    : _tag(tag<Ast>::value), _value(begin)
	{
	    _value->_value = end;
	}
	Ast(const Ast&) = default;


	typedef ast_helper::IteratorBase<Any> iterator;
	typedef ast_helper::IteratorBase<const Any> const_iterator;

	friend std::ostream& operator<<(std::ostream& out, const iterator& itr) { return itr.print(out); }
	friend std::ostream& operator<<(std::ostream& out, const const_iterator& itr) { return itr.print(out); }

	Any& operator[](size_t n) {
	    auto itr = begin();
	    itr = itr + n;
	    return *itr;
	}

	const Any& operator[](size_t n) const {
	    auto itr = begin();
	    itr = itr + n;
	    return *itr;
	}

	Any* flat_begin() { return _value + 1; }
	Any* flat_end() { return reinterpret_cast<Any*>(_value->_value); }
	const Any* flat_begin() const { return _value + 1; }
	const Any* flat_end() const { return reinterpret_cast<Any*>(_value->_value); }

	iterator begin() { return iterator(flat_begin()); }
	const_iterator begin() const { return const_iterator(flat_begin()); }

	iterator end() { return iterator(flat_end()); }
	const_iterator end() const { return const_iterator(flat_end()); }


	Any* end_at(Any *pos)
	{ return reinterpret_cast<Any*>(_value->_value = pos); }

	Any* end_at(const iterator& pos)
	{ return reinterpret_cast<Any*>(_value->_value = pos._value); }

	size_t size() const { return end() - begin(); }

	bool empty() const { return _value == _value->_value; }
    };
    template<> struct is_reinterperable<Ast> : public std::true_type {};

    struct Data {
	typedef typename Ast::iterator iterator;
	typedef typename Ast::const_iterator const_iterator;

	tag_t _tag;
	Any *_value;

	iterator begin() { return iterator(_value + 1); }
	const_iterator begin() const { return const_iterator(_value + 1); }

	iterator end() { return iterator(reinterpret_cast<Any*>(_value->_value)); }
	const_iterator end() const { return const_iterator(reinterpret_cast<Any*>(_value->_value)); }

	Data(Any *begin, Any *end) : _tag(tag<Data>::value), _value(begin) { _value->_value = (void*)end; }
	Data() : _tag(tag<Data>::value), _value(nullptr) {}

	const Any& operator[](size_t n) const {
	    auto itr = begin();
	    itr = itr + n;
	    return *itr;
	}

	bool empty() const {
	    return (_value == nullptr) || (_value == _value->_value);
	}
	size_t size() const { return end() - begin(); }

	Any* end_at(Any *pos)
	{ return reinterpret_cast<Any*>(_value->_value = pos); }

	Any* end_at(const iterator& pos)
	{ return reinterpret_cast<Any*>(_value->_value = pos._value); }
    };
    template<> struct is_reinterperable<Data> : public std::true_type {};


    Ast* make_empty_ast(Any *here) {
	return new (here)Ast(here + 1, here + 1);
    }

    // A subset of an Ast or Data
    struct Slice {
	const Any *_begin, *_end;

	Slice() = delete;
	Slice(const Any *begin, const Any *end)
	    : _begin(begin), _end(end) {}

	template<class Itr>
	Slice(Itr begin, Itr end)
	    : _begin(&*begin), _end(&*end) {}


	Slice(const Slice&) = default;

	typedef typename Ast::const_iterator iterator;
	typedef typename Ast::const_iterator const_iterator;

	const Any& operator[](size_t n) const { return *(begin() + n); }

	iterator begin() { return iterator(_begin); }
	const_iterator begin() const { return const_iterator(_begin); }

	iterator end() { return iterator(_end); }
	const_iterator end() const { return const_iterator(_end); }

	size_t size() const { return end() - begin(); }
	bool empty() const { return _begin == _end; }
    };

    struct PCode {
	typedef uintptr_t  value_type;
	typedef value_type* iterator;
	tag_t tag;
	uintptr_t *value;

	PCode() = delete;
	PCode(uintptr_t *vv) : tag(atl::tag<PCode>::value), value(vv) {}
    };

    /**
     * For a container wrapping Ast::iterator types, what is size
     * in Anys, counting nested Asts.
     *
     * @tparam Container:
     * @param cc: @return:
     */
    template<class Container>
    size_t flat_size(const Container& cc) { return cc->end()._value - cc->_value._value; }

    /********************************/
    /*  ____       _                */
    /* / ___|  ___| |_ _   _ _ __   */
    /* \___ \ / _ \ __| | | | '_ \  */
    /*  ___) |  __/ |_| |_| | |_) | */
    /* |____/ \___|\__|\__,_| .__/  */
    /*                      |_|     */
    /********************************/
    static const char *primitive_type_names[mpl::size<TypesVec>::value + 1];
    size_t object_sizes[ mpl::size<TypesVec>::value ];

    template<class T>
    class size
	: public std::conditional< atl::is_immediate<T>::value,
				   mpl::sizeof_<Any>,
				   mpl::sizeof_<T>
				   >::type {};

    template <typename T> struct wrap_t_arg {};
    struct InitTypes {
	template<class T>
	void operator()( wrap_t_arg<T> ) const {
	    primitive_type_names[tag<T>::value] = Name<T>::value;
	}};

    // once_flag init_types_flag;
    void init_types() { mpl::for_each<TypesVec, wrap_t_arg< mpl::placeholders::_1> >( InitTypes() ); }

    unsigned int type_tag(Any a) { return a._tag; }

    const char* type_name(unsigned int t) {
	if (t > BOOST_PP_SEQ_SIZE(ATL_TYPES_SEQ))
	    return "TYPE_TAG_OUT_OF_RANGE";
	return primitive_type_names[t];
    }
    const char* type_name(Any a) { return type_name( type_tag(a)); }
}

namespace std {
    template<>
    struct iterator_traits<typename atl::Ast::iterator> {
	typedef size_t difference_type;
	typedef atl::Any value_type;
	typedef atl::Any* pointer;
	typedef atl::Any& reference;
	typedef forward_iterator_tag iterator_category;
    };

    template<>
    struct iterator_traits<typename atl::Ast::const_iterator> {
	typedef size_t difference_type;
	typedef atl::Any value_type;
	typedef atl::Any* pointer;
	typedef atl::Any& reference;
	typedef forward_iterator_tag iterator_category;
    };
}

#endif
