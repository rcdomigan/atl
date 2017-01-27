#ifndef ATL_TYPES_HPP
#define ATL_TYPES_HPP
/**
 * @file /home/ryan/programming/atl/types.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jul 07, 2013
 */

#include <vector>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <type_traits>
#include <string>
#include <algorithm>
#include <iterator>
#include <functional>
#include <map>

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
		typedef uintptr_t value_type;
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

#define ATL_REINTERPERABLE_SEQ (Null)(Any)(Fixnum)(Pointer)(If)(Define)(Bool)(DefineMacro)(Quote)(Lambda)(DeclareType)(Type)(Ast)(AstData)(MovedAstData)(Undefined)(FunctionConstructor)(ClosureParameter)(Parameter)
#define ATL_PIMPL_SEQ (String)(Symbol)(Struct)(CxxFunctor)(CxxMacro)(Scheme)(LambdaMetadata)
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


    typedef mpl::vector26< BOOST_PP_SEQ_ENUM( ATL_TYPES_SEQ )  > TypesVec;

	const static tag_t LAST_CONCRETE_TYPE = mpl::size<TypesVec>::value;

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
		explicit constexpr Any(tag_t t, void *v) : _tag(t) , value(v) {}
		explicit constexpr Any(tag_t t) : _tag(t) , value(nullptr) {}

		bool operator<(Any const& other) const
		{ return (value < other.value) && (_tag < other._tag); }
	};

    bool operator==(const Any& aa, const Any& bb)
    { return (aa._tag == bb._tag) && (aa.value == bb.value); }

    bool operator!=(const Any& aa, const Any& bb) { return !(aa == bb); }

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

    struct Undefined
    {
        tag_t _tag;
        Undefined() : _tag(tag<Undefined>::value) {}
    };

	struct Define
	{
		tag_t _tag;
		Define() : _tag(tag<Define>::value) {}
	};


	struct Fixnum
	{
		typedef long value_type;
		tag_t _tag;
		long value;

		Fixnum() noexcept : _tag( tag<Fixnum>::value ) , value(0) {}
		Fixnum(long value) : _tag( tag<Fixnum>::value ), value(value) {}
	};
    template<>
    struct tag<long> : public tag<Fixnum> {};

	struct LambdaMetadata;

    struct Lambda
    {
        tag_t _tag;
        LambdaMetadata *value;

        Lambda(LambdaMetadata *value_)
	        : _tag(tag<Lambda>::value),
	          value(value_)
	    {}
    };

    struct DeclareType
    {
        tag_t _tag;
        long value;
        DeclareType() : _tag(tag<DeclareType>::value) {}
    };

    struct FunctionConstructor
    {
        tag_t _tag;
        long value;
        FunctionConstructor() : _tag(tag<FunctionConstructor>::value) {}
    };


    /*************************/
    /*  ____              _  */
    /* | __ )  ___   ___ | | */
    /* |  _ \ / _ \ / _ \| | */
    /* | |_) | (_) | (_) | | */
    /* |____/ \___/ \___/|_| */
    /*************************/
    struct Bool
    {
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
		template<class Itr>
		Itr add_iters(Itr itr, size_t n)
		{
			for(size_t i = 0; i < n; ++i)
				++itr;
			return itr;
		}

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

			Any operator*() const
			{
				if(value->_tag == tag<AstData>::value)
					{ return Any(tag<Ast>::value,
					             reinterpret_cast<void*>(const_cast<Any*>(value))); }
				return *value;
			}

			Value* operator->() { return value; }
			Value const* operator->() const { return value; }

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
			{ return add_iters(*this, n); }

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


		/* Modify the values of an Ast, but not its structure.  This
		   iterator is suitable for traversal, but Ast references
		   returned by it will be invalid after an increment.
		 */
		struct ModifyDataIterator
			: public IteratorBase<Any>
		{
			typedef IteratorBase<Any> Base;
			Any _data_wrapper;

			ModifyDataIterator(Any *vv) : Base(vv) {}

			/* Returns a reference to the data of an Ast.  For nested AstData, return a reference to an instance Ast. */
			Any& operator*()
			{
				if(value->_tag == tag<AstData>::value)
					{
						_data_wrapper = Any(tag<Ast>::value,
						                    reinterpret_cast<void*>(value));
						return _data_wrapper;
					}
				return *value;
			}

			ModifyDataIterator operator+(size_t n) const
			{ return add_iters(*this, n); }
		};

		struct ModifyData
		{
			typedef ModifyDataIterator iterator;
			iterator _begin, _end, _peek;

			ModifyData(Any *bb, Any *ee)
				: _begin(bb), _end(ee), _peek(nullptr)
			{}

			iterator begin() { return _begin; }
			iterator end() { return _end; }

			Any& peek(size_t n)
			{
				_peek = begin() + n;
				return *_peek;
			}

			Any operator[](size_t n) const {
				return *(_begin + n);
			}
		};
	}

	struct AstData
	{
		typedef ast_helper::IteratorBase<Any> iterator;
		typedef ast_helper::IteratorBase<const Any> const_iterator;
		typedef Any* flat_iterator;
		typedef Any const* const_flat_iterator;

		tag_t _tag;
		// this + value is the last element of the collection.
		size_t value;
		AstData(size_t offset)
			: _tag(tag<AstData>::value), value(offset)
		{}

		// 'Flat' functions begin with _this_ and ends one past the last element
		Any* flat_begin() { return reinterpret_cast<Any*>(this); }
		Any* flat_end() { return flat_begin() + value + 1; }
		const Any* flat_begin() const { return reinterpret_cast<Any const*>(this); }
		const Any* flat_end() const { return flat_begin() + value + 1; }

		// Regular begin/end are over the elements of the "container"
		// and don't include 'this'
		iterator begin() { return iterator(flat_begin() + 1); }
		const_iterator begin() const { return const_iterator(flat_begin() + 1); }

		iterator end() { return iterator(flat_end()); }
		const_iterator end() const { return const_iterator(flat_end()); }

		bool empty() const { return value == 0; }

		Any operator[](size_t n) const {
			auto itr = begin();
			itr = itr + n;
			return *itr;
		}

		size_t flat_size() const { return flat_end() - flat_begin(); }
		size_t size() const { return end() - begin(); }
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

		typedef Any* flat_iterator;
		typedef Any const* const_flat_iterator;

		friend std::ostream& operator<<(std::ostream& out, const iterator& itr) { return itr.print(out); }
		friend std::ostream& operator<<(std::ostream& out, const const_iterator& itr) { return itr.print(out); }

		/* The operator[] behavior doesn't return a reference because
		   nested AstData should be wrapped before return so it can be
		   passed by value (and I'd like to avoid having two Ast types
		   (Ast and AstData) to deal with everywhere).  The
		   'reference' method exists if something in the Ast needs to
		   be set. */
		Any operator[](size_t n) const
		{
			auto itr = begin();
			itr = itr + n;
			return *itr;
		}

		Any const* address(size_t n) const
		{
			auto itr = begin();
			itr = itr + n;
			return itr.value;
		}

		Any& reference(size_t n)
		{
			auto itr = begin();
			itr = itr + n;
			return *itr.value;
		}

		flat_iterator flat_begin() { return value->flat_begin(); }
		flat_iterator flat_end() { return value->flat_end(); }

		iterator begin() { return value->begin(); }
		const_iterator begin() const { return const_cast<AstData const*>(value)->begin(); }

		iterator end() { return value->end(); }
		const_iterator end() const { return const_cast<AstData const*>(value)->end(); }

		Any& back()
		{ return *(end().value - 1); }

		size_t flat_size() const { return value->flat_size(); }

		size_t size() const { return value->size(); }
		bool empty() const { return value->value == 0; }

		bool operator<(Ast const& other) const
		{ return value < other.value; }

		ast_helper::ModifyData modify_data() { return ast_helper::ModifyData(flat_begin() + 1, flat_end()); }
	};

	// Return an Ast pointing to an AstData `input` which was cast to
	// Any.
	Ast AstData_to_Ast(AstData &input)
	{ return Ast(&input); }

	Ast AstData_to_Ast(Any &input)
	{ return AstData_to_Ast(reinterpret_cast<AstData&>(input)); }

	struct Type // OK, type isn't a pimpl
    {
	    typedef tag_t value_type;
	    static const tag_t RIGID_TYPE_MASK = 1;

        tag_t _tag;
	    tag_t _value;

	    Type(tag_t value_, bool rigid=false)
		    : _tag(tag<Type>::value)
		    , _value(value_ << 1)
	    {
		    if(rigid || (value_ < LAST_CONCRETE_TYPE))
			    { _value |= RIGID_TYPE_MASK; }
	    }

	    tag_t value() const { return _value >> 1; }
	    bool is_rigid() const { return _value & RIGID_TYPE_MASK; }

	    bool operator<(Type const& other) const { return _value < other._value; }
	    bool operator==(Type const& other) const { return _value == other._value; }
	    bool operator!=(Type const& other) const { return _value != other._value; }
    };

	struct Scheme
	{
		typedef std::unordered_set<Type::value_type> Quantified;
		Quantified quantified;
		Any type;

		bool is_function()
		{
			if(type._tag == tag<Ast>::value)
				{
					auto& ast = *reinterpret_cast<AstData*>(type.value);
					if(ast.empty())
						{ return false; }

					auto head = ast[0];
					return head._tag == tag<Type>::value &&
						(reinterpret_cast<Type&>(head).value() == tag<FunctionConstructor>::value);
				}
			return false;
		}

		Scheme() : type(tag<Undefined>::value, nullptr) {}

		Scheme(Quantified const& bound_, Any const& type_)
			: quantified(bound_), type(type_)
		{}
	};

    struct Symbol
    {
	    enum Subtype {variable, constant};
	    Subtype subtype;

	    std::string name;
	    Scheme scheme;

	    Any value;

	    Symbol(std::string const& name_="")
		    : subtype(Subtype::variable)
		    , name(name_)
		    , value(tag<Undefined>::value)
	    {}

	    Symbol(std::string const& name_, Scheme const& type_)
		    : subtype(Subtype::variable)
		    , name(name_)
		    , scheme(type_)
		    , value(tag<Undefined>::value)
	    {}
    };

	struct Parameter
	{
		tag_t _tag;
		size_t value;

		Parameter(size_t offset)
			: _tag(tag<Parameter>::value)
			, value(offset)
		{}
	};

	struct ClosureParameter
	{
		tag_t _tag;
		size_t value;

		ClosureParameter(size_t offset)
			: _tag(tag<ClosureParameter>::value)
			, value(offset)
		{}
	};

    struct LambdaMetadata
    {
	    typedef std::vector<Symbol const*> Closure;
	    Closure closure;
	    Ast formals;

	    bool has_slot;
	    size_t slot;

	    pcode::Offset body_address;

	    Any return_type;

	    LambdaMetadata() : has_slot(0) {}

	    bool is_closure()
	    { return !closure.empty(); }

	    ClosureParameter closure_parameter(Symbol const* sym)
	    {
		    size_t idx = 0;
		    for(auto cc : closure)
			    {
				    if(cc->name == sym->name) { return ClosureParameter(idx); }
				    ++idx;
			    }
		    closure.push_back(sym);
		    return ClosureParameter(idx);
	    }
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

    struct CxxFunctor
    {
	    const std::string name;

	    typedef std::function<void (vm_stack::iterator begin, vm_stack::iterator end)> Fn;
	    typedef Fn value_type;
	    mutable value_type fn;

	    // A function signature (-> a (-> b c)), or (-> a) for a thunk
	    Ast type;
	    size_t arity;

	    CxxFunctor(const Fn& fn
	               , const std::string& name
	               , Ast const& tt
	               , size_t arity_)
		    : name(name), fn(fn), type(tt), arity(arity_)
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


    /*********************/
    /**  _    Other     **/
    /** / \_|_|_  _ ._  **/
    /** \_/ |_| |(/_|   **/
    /*********************/

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

    // once_flag init_types_flag; the GC will do this
	static bool _type_rtti_init = false;
    void init_types()
    {
	    if(!_type_rtti_init)
		    { mpl::for_each<TypesVec, wrap_t_arg< mpl::placeholders::_1> >( InitTypes() ); }
    }

    unsigned int type_tag(Any a) { return a._tag; }

	const char* type_name(unsigned int t)
	{
		if (t > BOOST_PP_SEQ_SIZE(ATL_TYPES_SEQ))
			{ return "TYPE_TAG_OUT_OF_RANGE"; }
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
