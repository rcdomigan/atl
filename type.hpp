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
#include "./abstract_type.hpp"

namespace mpl = boost::mpl;

namespace atl
{
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


#define ATL_REINTERPERABLE_SEQ (Null)(Any)(Fixnum)(Pointer)(If)(Define)(Bool)(DefineMacro)(Quote)(Lambda)(Let)(Type)(Ast)(AstData)
#define ATL_PIMPL_SEQ (Slice)(String)(Symbol)(Procedure)(Macro)(Undefined)(Parameter)(Method)(Struct)(CxxFunctor)(PrimitiveMacro)
#define ATL_TYPES_SEQ ATL_REINTERPERABLE_SEQ ATL_PIMPL_SEQ(Mark)

#define M(r, _, i, elem)						\
	struct elem;							\
	template<> struct is_atl_type<elem> : public std::true_type {};	\
	template<> struct Name<elem> { constexpr static const char* value = BOOST_PP_STRINGIZE( elem ) ; }; \
	template<> struct _Tag<elem> { \
		typedef _Tag<elem> type; \
		typedef unsigned int value_type; \
		const static value_type value = i; \
		operator tag_t () { return value; } \
	};

    BOOST_PP_SEQ_FOR_EACH_I(M, _, ATL_TYPES_SEQ)
#undef M

#define M(r, _, i, elem) template<> struct is_reinterperable<elem> : public std::true_type {};
    BOOST_PP_SEQ_FOR_EACH_I(M, _, ATL_REINTERPERABLE_SEQ)
#undef M

#define M(r, _, i, elem) template<> struct is_pimpl<elem> : public std::true_type {};
    BOOST_PP_SEQ_FOR_EACH_I(M, _, ATL_PIMPL_SEQ)
#undef M


    typedef mpl::vector26< BOOST_PP_SEQ_ENUM( ATL_TYPES_SEQ )  > TypesVec;

    template<class T>
    struct tag : public _Tag<typename std::remove_const<T>::type> {};


    /****************************************************/
    /*  ____        __ _       _ _   _                  */
    /* |  _ \  ___ / _(_)_ __ (_) |_(_) ___  _ __  ___  */
    /* | | | |/ _ \ |_| | '_ \| | __| |/ _ \| '_ \/ __| */
    /* | |_| |  __/  _| | | | | | |_| | (_) | | | \__ \ */
    /* |____/ \___|_| |_|_| |_|_|\__|_|\___/|_| |_|___/ */
    /****************************************************/
    struct Any {
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

    struct Let
    {
        tag_t _tag;
        long value;
        Let() : _tag(tag<Let>::value) {}
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

    struct Pointer {
	tag_t _tag;
	void *value;

	Pointer() : _tag(tag<Pointer>::value), value(NULL) {}
	Pointer(void *value) : _tag(tag<Pointer>::value), value(value) {}
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
			Value *value;
			IteratorBase(Value* vv) : value(vv) {}
			IteratorBase() = default;

			Value& operator*() { return *value; }
			const Value& operator*() const { return *value; }

			IteratorBase& operator++()
			{
				using namespace std;
				if( ((value->_tag == tag<Ast>::value))
				    && (value->value == value + 1))
					value = &*reinterpret_cast<typename
					                           conditional<is_const<Value>::value,
					                                       add_const<Ast>,
					                                       tmpl::Identity<Ast>
					                                       >::type::type*
					                           >(value)->end();
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
		tag_t _tag;
		Any* value;
		AstData(Any* end)
			: _tag(tag<AstData>::value), value(end)
		{}
	};

	struct Ast
	{
		typedef Any* value_type;
		tag_t _tag;
		value_type value;

		Ast() = delete;

		Ast(Any *begin_end)
			: _tag(tag<Ast>::value), value(begin_end)
		{}

		Ast(Any *begin, Any *end)
			: _tag(tag<Ast>::value), value(begin)
		{
			value->value = end;
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

		Any* flat_begin() { return value + 1; }
		Any* flat_end() { return reinterpret_cast<Any*>(value->value); }
		const Any* flat_begin() const { return value + 1; }
		const Any* flat_end() const { return reinterpret_cast<Any*>(value->value); }

		iterator begin() { return iterator(flat_begin()); }
		const_iterator begin() const { return const_iterator(flat_begin()); }

		iterator end() { return iterator(flat_end()); }
		const_iterator end() const { return const_iterator(flat_end()); }


		Any* end_at(Any *pos)
		{ return reinterpret_cast<Any*>(value->value = pos); }

		Any* end_at(const iterator& pos)
		{ return reinterpret_cast<Any*>(value->value = pos.value); }

		size_t size() const { return end() - begin(); }

		bool empty() const { return value == value->value; }
	};

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
		typedef std::vector<pcode::Offset> Backtrack;

		Backtrack backtrack;
		abstract_type::Type *type;

		Undefined(abstract_type::Type *tt)
			: type(tt)
		{}
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

	    abstract_type::Type const* types;

	    CxxFunctor(const Fn& fn
	               , const std::string& name
	               , abstract_type::Type const* tt)
		    : _name(name), fn(fn), types(tt)
	    {}
    };

	struct Eval;

    struct PrimitiveMacro
    {
	    using Input = Range<Ast::iterator>;
	    using Fn = std::function<tag_t (Eval&, Input const&)>;

	    std::string const name;
	    Fn fn;

        PrimitiveMacro(Fn const& fn_,
                       const std::string& name_)
	        : name(name_), fn(fn_)
        {}
    };

    struct Method {
        typedef std::function<Any (Range<tag_t*>)> Dispatch;
        Dispatch value;

        Method(Dispatch vv)
            : value(vv)
        {}
    };


    struct Type
    {
        typedef abstract_type::Type value_type;
        tag_t _tag;
        value_type *value;

        Type(abstract_type::Type* type_)
            : _tag(tag<Type>::value), value(type_)
        {}
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
		    : _begin(ast.flat_begin()), _end(ast.flat_end())
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


    ///////////////////////////////////////
    //  ____       _     _               //
    // | __ ) _ __(_) __| | __ _  ___ _  //
    // |  _ \| '__| |/ _` |/ _` |/ _ (_) //
    // | |_) | |  | | (_| | (_| |  __/_  //
    // |____/|_|  |_|\__,_|\__, |\___(_) //
    //                     |___/         //
    ///////////////////////////////////////
    // Bridge abstract and concrete types:

    std::ostream& abstract_type::Node::print(std::ostream& out) const
    {
        switch(tag)
            {
            case SubType::function:
                type_seq->print(out);
                break;
            case SubType::concrete:
                out << type_name(type);
                break;
            case SubType::abstract:
                out << 'a' << type;
                break;
            default:
                out << "what?";
                break;
            }

        switch(count)
            {
            case one:
                break;
            case at_least_one:
                out << "+"; break;
            case zero_or_more:
                out << "*"; break;
            }

        return out;
    }

    // The tag of the returned type of an expression
    namespace abstract_type
    {
        tag_t return_tag(Type const& tt)
        {
            if(tt.back().is_concrete())
                return tt.back().type;
            else if(tt.back().is_function())
                return tag<Procedure>::value;
            else
                return tag<Undefined>::value;
        }
    }
}

namespace std
{
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
