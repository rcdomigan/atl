#ifndef ATL_TYPES_HPP
#define ATL_TYPES_HPP
/**
 * @file /home/ryan/programming/atl/types.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jul 07, 2013
 */

#include <vector>
#include <set>
#include <unordered_map>
#include <type_traits>
#include <string>
#include <algorithm>
#include <iterator>
#include <functional>

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

#include "./exception.hpp"
#include "./utility.hpp"

namespace mpl = boost::mpl;

namespace atl
{
	typedef size_t tag_t;
	namespace pcode
	{
		typedef uintptr_t  value_type;
		// todo: switch iterator use to Offset so it remains valid if
		// the structure being pointed into resizes
		typedef value_type* iterator;
		typedef size_t Offset;
	}

	namespace vm_stack
	{
		typedef uintptr_t  value_type;
		typedef value_type* iterator;
		typedef size_t Offset;
	}


    /******************************/
    /*  _____          _ _        */
    /* |_   _| __ __ _(_) |_ ___  */
    /*   | || '__/ _` | | __/ __| */
    /*   | || | | (_| | | |_\__ \ */
    /*   |_||_|  \__,_|_|\__|___/ */
    /******************************/
    template<class T> struct _Tag;
    template<class T>
    struct Name { constexpr static const char* value = "#<UNKNOWN>"; };

    template<class T>
    struct is_atl_type : public std::false_type {};

    template<class T>
    struct is_reinterperable : public std::false_type {};

    template<class T>
    struct is_pimpl : public std::false_type {};


#define ATL_REINTERPERABLE_SEQ (Null)(Any)(Fixnum)(Pointer)(If)(Define)(Bool)(DefineMacro)(Quote)(Lambda)(DeclareType)(Type)(Ast)(AstData)(Parameter)(ClosureParameter)
#define ATL_PIMPL_SEQ (Slice)(String)(Symbol)(Procedure)(Macro)(Undefined)(Struct)(CxxFunctor)(CxxMacro)
#define ATL_TYPES_SEQ ATL_REINTERPERABLE_SEQ ATL_PIMPL_SEQ

#define M(r, _, i, elem)						\
	struct elem;							\
	template<> struct is_atl_type<elem> : public std::true_type {};	\
	template<> struct Name<elem> { constexpr static const char* value = BOOST_PP_STRINGIZE( elem ) ; }; \
	template<> struct _Tag<elem> : public std::integral_constant<tag_t, i> {};

    BOOST_PP_SEQ_FOR_EACH_I(M, _, ATL_TYPES_SEQ)
#undef M

#define M(r, _, i, elem) template<> struct is_reinterperable<elem> : public std::true_type {};
    BOOST_PP_SEQ_FOR_EACH_I(M, _, ATL_REINTERPERABLE_SEQ)
#undef M

#define M(r, _, i, elem) template<> struct is_pimpl<elem> : public std::true_type {};
    BOOST_PP_SEQ_FOR_EACH_I(M, _, ATL_PIMPL_SEQ)
#undef M


    typedef mpl::vector25< BOOST_PP_SEQ_ENUM( ATL_TYPES_SEQ )  > TypesVec;

    template<class T>
    struct tag : public _Tag<typename std::remove_const<T>::type> {};


    /****************************************************/
    /*  ____        __ _       _ _   _                  */
    /* |  _ \  ___ / _(_)_ __ (_) |_(_) ___  _ __  ___  */
    /* | | | |/ _ \ |_| | '_ \| | __| |/ _ \| '_ \/ __| */
    /* | |_| |  __/  _| | | | | | |_| | (_) | | | \__ \ */
    /* |____/ \___|_| |_|_| |_|_|\__|_|\___/|_| |_|___/ */
    /****************************************************/
	struct Any
	{
		tag_t _tag;
		void *value;

		Any(const Any&) = default;
		constexpr Any() : _tag(tag<Any>::value) , value(nullptr) {}
		constexpr Any(tag_t t, void *v) : _tag(t) , value(v) {}
		constexpr Any(tag_t t) : _tag(t) , value(nullptr) {}
	};

    bool operator==(const Any& aa, const Any& bb) {
	return (aa._tag == bb._tag) && (aa.value == bb.value);
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
	void *value;

	Null() : _tag(0), value(nullptr) {}
    };


    /************************************/
    /** ___   immediate                **/
    /**  | ._ _ ._ _  _  _|o _._|_ _   **/
    /** _|_| | || | |(/_(_||(_| |_(/_  **/
    /************************************/
	struct Define
	{
		tag_t _tag;
		Define() : _tag(tag<Define>::value) {}
	};


	struct Fixnum
	{
		tag_t _tag;
		long value;

		Fixnum() noexcept : _tag( tag<Fixnum>::value ) , value(0) {}
		Fixnum(long value) : _tag( tag<Fixnum>::value ), value(value) {}
	};
    template<>
    struct tag<long> : public tag<Fixnum> {};

    struct Lambda
    {
        tag_t _tag;
        long value;
        Lambda() : _tag(tag<Lambda>::value) {}
    };

    struct DeclareType
    {
        tag_t _tag;
        long value;
        DeclareType() : _tag(tag<DeclareType>::value) {}
    };


    /*************************/
    /*  ____              _  */
    /* | __ )  ___   ___ | | */
    /* |  _ \ / _ \ / _ \| | */
    /* | |_) | (_) | (_) | | */
    /* |____/ \___/ \___/|_| */
    /*************************/
    struct Bool {
	tag_t _tag;
	long value;
	Bool() : _tag(tag<Bool>::value), value(false) {}
        Bool(bool vv) : _tag(tag<Bool>::value), value(vv) {}
    };
    template<>
    struct tag<bool> : public tag<Bool> {};
    Any atl_true() { return Any(tag<Bool>::value, (void*)true); }
    Any atl_false() { return Any(tag<Bool>::value, (void*)false); }

	struct Pointer
	{
		tag_t _tag;
		Any *value;

		Pointer() : _tag(tag<Pointer>::value), value(NULL) {}
		Pointer(Any *value) : _tag(tag<Pointer>::value), value(value) {}
	};

	/*********************/
	/**  _ __     _     **/
	/** |_)| |\/||_)|   **/
	/** | _|_|  ||  |_  **/
	/**           pimpl **/
	/*********************/
	namespace ast_helper
	{
		// This is really a special iterator for when I have nested
		// arrays.  I don't know when that would happen other than in
		// generating ASTs.
		template<class Value>
		struct IteratorBase
		{
			// Should by Any or Any const
			Value *value;
			IteratorBase(Value* vv) : value(vv) {}
			IteratorBase() = default;

			Value& operator*() { return *value; }
			const Value& operator*() const { return *value; }

			IteratorBase& operator++()
			{
				using namespace std;
				if(value->_tag == tag<AstData>::value)
					value = reinterpret_cast<typename
					                         conditional<is_const<Value>::value,
					                                     add_const<AstData>,
					                                     tmpl::Identity<AstData>
					                                     >::type::type*
					                         >(value)->flat_end();
				else
					++value;
				return *this;
			}

			IteratorBase operator+(size_t n) const
			{
				IteratorBase itr = *this;
				for(size_t i = 0; i < n; ++i)
					++itr;
				return itr;
			}

			size_t operator-(IteratorBase other) const
			{
				size_t n = 0;
				for(; other != *this; ++other, ++n);
				return n;
			}

			bool operator!=(const IteratorBase<Value>& j) const {return j.value != value;}

			bool operator==(const IteratorBase<Value>& j) const {return j.value == value;}

			std::ostream& print(std::ostream& out) const { return out << value; }
		};
	}

	struct AstData
	{
		typedef ast_helper::IteratorBase<Any> iterator;
		typedef ast_helper::IteratorBase<const Any> const_iterator;

		tag_t _tag;
		// this + value is the last element of the collection.
		size_t value;
		AstData(size_t offset)
			: _tag(tag<AstData>::value), value(offset)
		{}

		Any* flat_begin() { return reinterpret_cast<Any*>(this) + 1; }
		Any* flat_end() { return flat_begin() + value; }
		const Any* flat_begin() const { return reinterpret_cast<Any const*>(this) + 1; }
		const Any* flat_end() const { return flat_begin() + value; }

		iterator begin() { return iterator(flat_begin()); }
		const_iterator begin() const { return const_iterator(flat_begin()); }

		iterator end() { return iterator(flat_end()); }
		const_iterator end() const { return const_iterator(flat_end()); }
	};

	struct Ast
	{
		tag_t _tag;
		AstData* value;

		Ast(AstData *begin_end)
			: _tag(tag<Ast>::value), value(begin_end)
		{}

		Ast() : _tag(tag<Ast>::value), value(nullptr) {}
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

		iterator begin() { return value->begin(); }
		const_iterator begin() const { return const_cast<AstData const*>(value)->begin(); }

		iterator end() { return value->end(); }
		const_iterator end() const { return const_cast<AstData const*>(value)->end(); }

		Any& back()
		{ return *(end().value - 1); }

		size_t size() const { return end() - begin(); }
		bool empty() const { return value->value == 0; }
	};

	// Return an Ast pointing to an AstData `input` which was cast to
	// Any.
	Ast AstData_to_Ast(Any &input)
	{ return Ast(&reinterpret_cast<AstData&>(input)); }


	struct Parameter
	{
		size_t offset, hops;
		Parameter(size_t offset_, size_t hops_) : offset(offset_), hops(hops_) {}
	};

    struct Symbol
    {
	    std::string name;
	    Symbol(const std::string& name_) : name(name_) {}
    };

    struct String {
	std::string value;

	String() : value() {}

	template<class T>
	String(T init) : value(init) {}
    };

    template<> struct
    Name<std::string*> : public Name<String> {};

    template<> struct
    tag<std::string*> : public tag<String> {};

	struct Undefined
	{
		typedef std::vector<off_t> Backtrack;

		// Information for the compiler ()
		enum class LexicalBinding {unbound, parameter, indirect_parameter};
		LexicalBinding binding_type;

		size_t argument_position;

		Backtrack backtrack;
	};


    /* Macro and Procedure have the same data layout, but distinct tags */
    struct Procedure
    {
	    pcode::Offset body;
        size_t tail_params;

        tag_t return_type;

	    Procedure(pcode::Offset body_, size_t padding, tag_t rtype)
            : body(body_), tail_params(padding), return_type(rtype)
        {}
    };


    struct CxxFunctor
    {
	    const std::string _name;

	    typedef std::function<void (vm_stack::iterator begin, vm_stack::iterator end)> Fn;
	    typedef Fn value_type;
	    mutable value_type fn;

	    Ast types;

	    CxxFunctor(const Fn& fn
	               , const std::string& name
	               , Ast const& tt)
		    : _name(name), fn(fn), types(tt)
	    {}
    };

	struct Eval;

    struct CxxMacro
    {
	    using Input = Range<Ast::iterator>;
	    using Fn = std::function<Any (Input const&)>;

	    std::string const name;
	    Fn fn;

        CxxMacro(Fn const& fn_,
                       const std::string& name_)
	        : name(name_), fn(fn_)
        {}
    };

    struct Type
    {
        tag_t _tag;
	    tag_t value;
    };


    /*********************/
    /**  _    Other     **/
    /** / \_|_|_  _ ._  **/
    /** \_/ |_| |(/_|   **/
    /*********************/
    struct Slice
    {
	    const Any *_begin, *_end;

	    Slice() = delete;
	    Slice(const Any *begin, const Any *end)
		    : _begin(begin), _end(end) {}

	    template<class Itr>
	    Slice(Itr begin, Itr end)
		    : _begin(&*begin), _end(&*end) {}

	    Slice(Ast& ast)
		    : _begin(&*ast.begin()), _end(&*ast.end())
	    {}

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


    /**
     * For a container wrapping Ast::iterator types, what is size
     * in Anys, counting nested Asts.
     *
     * @tparam Container:
     * @param cc: @return:
     */
    template<class Container>
    size_t flat_size(const Container& cc) { return cc->end().value - cc->value.value; }

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
	: public std::conditional< atl::is_reinterperable<T>::value,
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
