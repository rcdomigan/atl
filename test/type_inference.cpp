/* @file /home/ryan/programming/atl/test/inference.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Feb 28, 2015
 *
 * Test the inference engine on a simple language.
 */

#include <gtest/gtest.h>

#include <lexical_environment.hpp>
#include <type_inference.hpp>
#include <ffi.hpp>
#include <helpers/pattern_match.hpp>

using namespace atl;


struct TestSubstitution
	: public ::testing::Test
{
	TestSubstitution()
	{ init_types(); }
};


TEST_F(TestSubstitution, test_nop_type_substitution)
{
	using namespace make_ast;
    inference::SubstituteMap subst;
    Arena arena;

    subst[Type(1)] = wrap<Type>(3);

    auto make_expr = mk(wrap<Type>(4), wrap<Type>(5));

    auto pre = make_expr(ast_alloc(arena));
    auto post = inference::substitute_type(ast_alloc(arena),
                                           subst,
                                           ref_wrap(pre));

    ASSERT_EQ(pre, explicit_unwrap<Ast>(post));
}

TEST_F(TestSubstitution, test_type_substitution)
{
	using namespace make_ast;
	Arena arena;

    inference::SubstituteMap subst;
    subst[Type(1)] = wrap<Type>(3);

    auto ast = mk(wrap<Type>(1),
                  mk(wrap<Type>(1)),
                  wrap<Type>(2))
	    (ast_alloc(arena));

    auto expr = inference::substitute_type
	    (ast_alloc(arena),
	     subst,
	     ref_wrap(ast));

    auto expect = mk(wrap<Type>(3),
                       mk(wrap<Type>(3)),
                       wrap<Type>(2))(ast_alloc(arena));

    ASSERT_EQ(expect, explicit_unwrap<Ast>(expr))
	    << "Got: " << printer::with_type(expr) << std::endl;
}

/* substitution example from Ian Grant page 8 */
TEST_F(TestSubstitution, test_type_substitution_p8)
{
	using namespace make_ast;
	Arena arena;

	auto type = [](char cc)
		{ return wrap<Type>(cc + LAST_CONCRETE_TYPE); };


    inference::SubstituteMap subs;
    subs[Type('x' + LAST_CONCRETE_TYPE)] = wrap<Type>('a' + LAST_CONCRETE_TYPE);
    subs[Type('y' + LAST_CONCRETE_TYPE)] = wrap(mk(type('b'), type('a'))(ast_alloc(arena)));

    auto do_sub = [&](Ast& ast) -> Ast
	    {
		    return unwrap<Ast>
		    (inference::substitute_type(ast_alloc(arena),
		                                subs,
		                                ref_wrap(ast)));
	    };

    auto first = mk(type('x'),
                      mk(type('b'),
                           type('x')))(ast_alloc(arena));

    auto second = mk(type('a'),
                       type('y'))(ast_alloc(arena));

    auto expect = mk(type('a'), mk(type('b'), type('a')))(ast_alloc(arena));

    ASSERT_EQ(expect, do_sub(first))
	    << "Got: " << printer::with_type(first) << std::endl;

    ASSERT_EQ(expect, do_sub(second))
	    << "Got: " << printer::with_type(second) << std::endl;
}

TEST_F(TestSubstitution, example_p9_assuming_mgu)
{
	using namespace make_ast;
    using namespace inference;
    Arena arena;

	auto type = [&](char cc) { return wrap<Type>(cc); };

	auto zero = wrap<Type>(0);
    auto aalloc = [&](){ return ast_alloc(arena); };

    auto left = mk(type(1), type(2), type(3))(aalloc()),
	    right = mk(mk(type(2), type(2)),
	                 mk(type(3), type(3)),
	                 mk(type(0), type(0)))(aalloc());

    SubstituteMap subs
	    ({
		    make_pair
			    (Type(1),
			     wrap(mk(mk(mk(zero, zero), mk(zero, zero)),
			               mk(mk(zero, zero), mk(zero, zero)))(aalloc()))),

			    make_pair
			    (Type(2),
			     wrap(mk(mk(zero, zero), mk(zero, zero))(aalloc()))),

			    make_pair
			    (Type(3),
			     wrap(mk(zero, zero)(aalloc())))
			    });

    auto sub_left = substitute_type(aalloc(), subs, ref_wrap(left)),
	    sub_right = substitute_type(aalloc(), subs, ref_wrap(right));

    ASSERT_EQ(explicit_unwrap<Ast>(sub_left),
              explicit_unwrap<Ast>(sub_right));
}

TEST_F(TestSubstitution, test_substitution_composition)
{
	using namespace make_ast;
	using namespace inference;
	Arena arena;


	{
		SubstituteMap left({{Type(2), wrap<Type>(0)}}),
			right({{Type(1), wrap(mk(wrap<Type>(2), wrap<Type>(3))(ast_alloc(arena)))}});
		compose_subs(arena, left, right);

		ASSERT_EQ(2, left.size());
		ASSERT_EQ(mk(wrap<Type>(0), wrap<Type>(3))(ast_alloc(arena)),
		          explicit_unwrap<Ast>(left[Type(1)]));
	}
}


/* // I'm using 'Ian Grant (The Hindley-Milner Type Inference */
/* // Algorithm)' as the basis for the rules, I'm numbering them */
/* // (i)-(iv) after his Unification algorithm */
struct Unification
    : public ::testing::Test
{
	atl::Arena store;

	Unification()
	{ init_types(); }

	AstAllocator aalloc() { return make_ast::ast_alloc(store); }
	Any type(size_t tt) { return wrap<Type>(tt); }
};

TEST_F(Unification, rule_1)
{
	using namespace make_ast;
    // rule (i)
    auto subst = inference::most_general_unification
	    (store, wrap<Type>('x'),
	     wrap<Type>('y'));


    ASSERT_EQ(subst.size(), 1);
    ASSERT_EQ(subst.begin()->first, Type('y'));
    ASSERT_EQ(subst.begin()->second, wrap<Type>('x'));
}

TEST_F(Unification, rule_2a)
{
	using namespace make_ast;

    //rule (iia) ... should this be an error?  Am I an errorist?
	auto ast = mk(type(1),type(2))(ast_alloc(store));
    auto subst = inference::most_general_unification
	    (store,
	     wrap<Type>(0),
	     ref_wrap(ast));

    ASSERT_EQ(subst.size(), 1);
    ASSERT_EQ(tag<Ast>::value, subst[Type(0)]._tag);
    ASSERT_EQ(explicit_unwrap<Ast>(subst[Type(0)]),
              mk(wrap<Type>(1), wrap<Type>(2))(ast_alloc(store)));
}

TEST_F(Unification, rule_2aa)
{
	using namespace make_ast;

    //rule (iia)
	auto ast = mk(type(1), type(2))(ast_alloc(store));
    auto subst = inference::most_general_unification
	    (store,
	     wrap<Type>(0),
	     ref_wrap(ast));

    ASSERT_EQ(subst.size(), 1);
    ASSERT_EQ(tag<Ast>::value, subst[Type(0)]._tag);
    ASSERT_EQ(explicit_unwrap<Ast>(subst[Type(0)]),
              mk(type(1), type(2))(ast_alloc(store)));
}

TEST_F(Unification, rule_2b)
{
    using namespace make_ast;
    //rule (iib)
    auto ast = mk(type(0), type(1))(ast_alloc(store));
    auto subst = inference::most_general_unification
	    (store,
	     ref_wrap(ast),
	     wrap<Type>(2));

    ASSERT_EQ(subst.size(), 1);
    ASSERT_EQ(tag<Ast>::value, subst[Type(2)]._tag);
    ASSERT_EQ(explicit_unwrap<Ast>(subst[Type(2)]),
              mk(type(0), type(1))(ast_alloc(store)));
}

TEST_F(Unification, rule_3)
{
	using namespace make_ast;

    //rule (iii)
    auto subst = inference::most_general_unification
	    (store, wrap<Type>(0), wrap<Type>(0));
    ASSERT_EQ(subst.size(), 0);
}

TEST_F(Unification,  rule_4)
{
	using namespace make_ast;
	auto ast1 = mk(type(0), type(1))(aalloc());
	auto ast2 = mk(type(2), type(3))(aalloc());

    auto subst = inference::most_general_unification
		(store,
		 ref_wrap(ast1),
		 ref_wrap(ast2));

    ASSERT_EQ(subst.size(), 2);
    ASSERT_EQ(subst[Type(2)], wrap<Type>(0));
    ASSERT_EQ(subst[Type(3)], wrap<Type>(1));
}

TEST_F(Unification, example_p8)
{
    using namespace make_ast;
    using namespace inference;

    enum { A=0, B=1, X=10, Y=11 };

    auto left = mk(type(A), type(Y))(aalloc()),
	    right = mk(type(X), mk(type(B), type(X)))(aalloc());

    auto subs = inference::most_general_unification
	    (store, ref_wrap(left), ref_wrap(right));

    auto left_post = substitute_type(aalloc(), subs, ref_wrap(left)),
	    right_post = substitute_type(aalloc(), subs, ref_wrap(right));

    ASSERT_EQ(explicit_unwrap<Ast>(left_post),
              explicit_unwrap<Ast>(right_post));
}

TEST_F(Unification, example_p9)
{
    using namespace make_ast;
    using namespace inference;

    auto left = mk(type(1), type(2), type(3))(aalloc()),
	    right = mk(mk(type(2), type(2)),
	                 mk(type(3), type(3)),
	                 mk(type(0), type(0)))(aalloc());

    auto subs = most_general_unification(store, ref_wrap(left), ref_wrap(right));

    auto sub_left = substitute_type(aalloc(), subs, ref_wrap(left)),
	    sub_right = substitute_type(aalloc(), subs, ref_wrap(right));

    ASSERT_EQ(explicit_unwrap<Ast>(sub_left),
              explicit_unwrap<Ast>(sub_right));
}

struct SpecializeAndGeneralize
    : public ::testing::Test
{
    Arena store;
	SpecializeAndGeneralize() { init_types(); }
};

TEST_F(SpecializeAndGeneralize, test_instantiate)
{
	using namespace make_ast;
	using namespace inference;

	Type::value_type new_types = LAST_CONCRETE_TYPE;
	auto aa = ++new_types,
		bb = ++new_types,
		cc = ++new_types,
		dd = ++new_types,
		ee = new_types + 1,
		ff = new_types + 2;

	Scheme scheme(Scheme::Quantified{aa, bb},
	              wrap(mk(wrap<Type>(cc),
	                        mk(wrap<Type>(aa),
	                             wrap<Type>(bb)),
	                        wrap<Type>(dd))(ast_alloc(store))));

	auto expected = mk(wrap<Type>(cc),
	                     mk(wrap<Type>(ee),
	                          wrap<Type>(ff)),
	                     wrap<Type>(dd))(ast_alloc(store));
	Scheme::Quantified expected_subs{ee, ff};

	auto instantiated = instantiate(store, new_types, scheme);
	ASSERT_EQ(tag<Ast>::value, instantiated._tag);
	ASSERT_NE(expected, explicit_unwrap<Ast>(instantiated));

	std::cout << "Subbed: " << printer::print(instantiated) << std::endl;
	auto inner_ast = subscripter(explicit_unwrap<Ast>(instantiated))[1];

	ASSERT_TRUE(expected_subs.count(unwrap<Type>(inner_ast[0].value).value()))
		<< "got: " << unwrap<Type>(inner_ast[0].value).value();

	ASSERT_TRUE(expected_subs.count(unwrap<Type>(inner_ast[1].value).value()));
	ASSERT_NE(unwrap<Type>(inner_ast[0].value).value(),
	          unwrap<Type>(inner_ast[1].value).value());
}

TEST_F(SpecializeAndGeneralize, test_generalize)
{
	using namespace make_ast;
	using namespace inference;

	Type::value_type new_types = LAST_CONCRETE_TYPE;
	auto aa = ++new_types,
		bb = ++new_types,
		cc = ++new_types,
		dd = ++new_types,
		ee = new_types + 1,
		ff = new_types + 2;

	Scheme scheme(Scheme::Quantified{aa, bb},
	              wrap(mk(wrap<Type>(cc),
	                        mk(wrap<Type>(aa),
	                             wrap<Type>(bb)),
	                        wrap<Type>(dd))(ast_alloc(store))));

	auto expected = mk(wrap<Type>(cc),
	                     mk(wrap<Type>(ee),
	                          wrap<Type>(ff)),
	                     wrap<Type>(dd))(ast_alloc(store));
	Scheme::Quantified expected_subs{ee, ff};

	auto instantiated = instantiate(store, new_types, scheme);
	ASSERT_EQ(tag<Ast>::value, instantiated._tag);
	ASSERT_NE(expected, explicit_unwrap<Ast>(instantiated));

	auto inner_ast = subscripter(explicit_unwrap<Ast>(instantiated))[1];

	ASSERT_TRUE(expected_subs.count(unwrap<Type>(inner_ast[0].value).value()))
		<< "got: " << unwrap<Type>(inner_ast[0].value).value();

	ASSERT_TRUE(expected_subs.count(unwrap<Type>(inner_ast[1].value).value()));
	ASSERT_NE(unwrap<Type>(inner_ast[0].value).value(),
	          unwrap<Type>(inner_ast[1].value).value());
}

struct Inference
	: public Unification
{
	Type::value_type new_types;
	inference::Gamma gamma;

	SymbolMap symbols;

	Inference()
		: Unification(), new_types(LAST_CONCRETE_TYPE), symbols(store)
	{}
};



/* My plan is that the symbols will be bound to their scope by
   assign_free, so I can't really test var in isolation.

   TODO: Not sure the Symbol/Bound sharing the same type information
   during type inference will work as I hope, but press onward for
   now.*/
TEST_F(Inference, try_nop)
{
	using namespace inference;

	Symbol sym_a = Symbol("a"),
		sym_b = Symbol("b");

	sym_a.scheme.type = wrap<Type>(++new_types);
	sym_b.scheme.type = wrap<Type>(++new_types);

	auto e1 = W(store, new_types, gamma, wrap(&sym_a));
    auto e2 = W(store, new_types, gamma, wrap(&sym_b));
	auto e3 = W(store, new_types, gamma, wrap(&sym_a));

    ASSERT_NE(e1.type.value, e2.type.value);
    ASSERT_EQ(e1.type.value, e3.type.value);
}

TEST_F(Inference, test_lambda)
{
    using namespace inference;
    using namespace make_ast;

    auto e1 = mk(wrap<Lambda>(),
                   mk("a"),
                   "a")(aalloc());

    auto We1 = W(store, new_types, gamma, ref_wrap(e1));

    ASSERT_EQ(tag<Ast>::value, We1.type._tag);

    auto type = explicit_unwrap<Ast>(We1.type);
    ASSERT_EQ(type[0], wrap<Type>(tag<FunctionConstructor>::value));
    ASSERT_EQ(type[1], type[2]);
}

TEST_F(Inference, test_multi_arg_lambda)
{
    using namespace inference;
    using namespace make_ast;

    auto e1 = mk(wrap<Lambda>(),
                   mk("a", "b"),
                   "a")(aalloc());

    auto We1 = W(store, new_types, gamma, ref_wrap(e1));

    {
	    using namespace pattern_match;
	    ASSERT_TRUE
		    (match(fnt("a", "b", "a"),
		           We1.type))
		    << printer::with_type(e1);
    }
}

/* Same "simple lambda" used in analyse_and_compile */
TEST_F(Inference, test_simple_lambda)
{
	using namespace inference;
	using namespace make_ast;

	auto expr = mk(wrap<Lambda>(),
	               mk("a", "b"),
	               mk("add2", "a", "b"))
		(aalloc());

	std::cout << printer::with_type(expr) << std::endl;

	auto inferred = W(store, new_types, gamma, ref_wrap(expr));
	apply_substitution(store, gamma, inferred.subs, ref_wrap(expr));

	{
		using namespace pattern_match;
		ASSERT_TRUE
			(match(fnt("x", "y", "z"),
			       inferred.type))
			<< "\n" << printer::with_type(inferred.type);
	}
}

TEST_F(Inference, test_application)
{
    using namespace make_ast;
    using namespace inference;

    auto e1 = mk(wrap<Lambda>(),
                 mk("x", "y"),
                 mk("x", "y"))(aalloc());
    auto We1 = W(store, new_types, gamma, ref_wrap(e1));

    /* Check the body: */
    {
	    using namespace pattern_match;
	    ASSERT_TRUE
		    (match(fnt(fnt("a", "b"), fnt("a", "b")),
		           We1.type))
		    << printer::with_type(We1.type);
    }
}

TEST_F(Inference, test_define)
{
    using namespace make_ast;
    using namespace inference;

    auto e1 = mk(wrap<Define>(), "a", "b")(aalloc());
    auto wrapped = wrap(e1);

    auto We1 = W(store, new_types, gamma, ref_wrap(e1));

    apply_substitution(store, gamma, We1.subs, wrapped);

    ASSERT_TRUE(is<Scheme>(We1.type));

    auto& scheme = unwrap<Scheme>(We1.type);
    ASSERT_EQ(1, scheme.quantified.size());

    ASSERT_TRUE(is<Type>(scheme.type));
    ASSERT_EQ(1, scheme.quantified.count(unwrap<Type>(scheme.type).value()));
}

TEST_F(Inference, test_apply_defined)
{
    using namespace make_ast;
    using namespace inference;

    // id function
    auto e1 = mk(wrap<Define>(), "id",
                   mk(wrap<Lambda>(), mk("x"), "x"))(aalloc());
    auto wrapped = wrap(e1);

    auto We1 = W(store, new_types, gamma, ref_wrap(e1));

    ASSERT_TRUE(is<Scheme>(We1.type));

    auto id_scheme = unwrap<Scheme>(We1.type);
    ASSERT_EQ(1, id_scheme.quantified.size());

    ASSERT_TRUE(is<Ast>(id_scheme.type));
    ASSERT_EQ(*id_scheme.quantified.begin(),
              unwrap<Type>(explicit_unwrap<Ast>(id_scheme.type)[1]).value());
    ASSERT_EQ(*id_scheme.quantified.begin(),
              unwrap<Type>(explicit_unwrap<Ast>(id_scheme.type)[2]).value());

    ASSERT_TRUE(We1.subs.empty());

    apply_substitution(store, gamma, We1.subs, wrapped);

    // Now see if we can apply the newly defined "id"
    auto e2 = mk("id", "y")(aalloc());
    auto We2 = W(store, new_types, gamma, ref_wrap(e2));

    ASSERT_EQ(2, We2.subs.size());
    ASSERT_TRUE(is<Type>(We2.type));

    ASSERT_NE(unwrap<Type>(We2.type).value(),
              *id_scheme.quantified.begin());

    ASSERT_TRUE(is<Symbol>(e2[1]));
    ASSERT_TRUE(is<Type>(unwrap<Symbol>(e2[1]).scheme.type));

    ASSERT_NE(unwrap<Type>(We2.type).value(),
              unwrap<Type>(unwrap<Symbol>(e2[1]).scheme.type).value());

    auto wrapped2 = wrap(e2);
    apply_substitution(store, gamma, We2.subs, wrapped2);

    ASSERT_EQ(unwrap<Type>(We2.type).value(),
              unwrap<Type>(unwrap<Symbol>(e2[1]).scheme.type).value());

}

TEST_F(Inference, test_nested_application)
{
    using namespace make_ast;
    using namespace inference;

    auto e1 = mk(wrap<Lambda>(),
                 mk("x", "y", "z"),
                 mk("x", mk("y", "z")))
	    (aalloc());

    auto We1 = W(store, new_types, gamma, ref_wrap(e1));

    {
	    using namespace pattern_match;
	    ASSERT_TRUE
		    (match(fnt(fnt("a", "b"), fnt(fnt("c", "a"), fnt("c", "b"))),
		           We1.type))
		    << "\n" << printer::print(We1.type);
    }
}

TEST_F(Inference, test_multi_arg_application)
{
    using namespace make_ast;
    using namespace inference;

    auto e1 = mk(wrap<Lambda>(),
                 mk("x", "y", "z"),
                 mk("x", "y", "z"))
	    (aalloc());

    auto We1 = W(store, new_types, gamma, ref_wrap(e1));

    {
	    using namespace pattern_match;
	    ASSERT_TRUE
		    (match(fnt(fnt("a", "b", "c"), "a", "b", "c"),
		           We1.type))
		    << "\n" << printer::print(We1.type);
    }
    auto wrapped = wrap(e1);
    apply_substitution(store, gamma, We1.subs, wrapped);

    // z should be a non-function type
    {
	    auto z = subscript(e1, 1, 2);
	    ASSERT_EQ(tag<Symbol>::value, z._tag);
	    ASSERT_EQ(tag<Type>::value,
	              unwrap<Symbol>(z).scheme.type._tag)
		    << "Got " << type_name(unwrap<Symbol>(z).scheme.type._tag) << std::endl;
    }

    // y should also be a non-function (as I keep forgetting: x
    // _applied_ to y to produces a function)
    {
	    auto y = AstSubscripter(e1)[1][1].value;
	    ASSERT_EQ(tag<Symbol>::value, y._tag);
	    ASSERT_EQ(tag<Type>::value,
	              unwrap<Symbol>(y).scheme.type._tag)
		    << "Got " << type_name(unwrap<Symbol>(y).scheme.type._tag) << std::endl;

    }

    {
	    using namespace pattern_match;
	    auto x = AstSubscripter(e1)[1][0].value;
	    ASSERT_EQ(tag<Symbol>::value, x._tag);
	    ASSERT_TRUE
		    (match(fnt("a", "b", "c"),
		           unwrap<Symbol>(x).scheme.type))
		    << "\n" << printer::print(unwrap<Symbol>(x).scheme.type);
    }

}

TEST_F(Inference, test_recursive_fn)
{
    using namespace make_ast;
    using namespace inference;

    auto rec = mk(wrap<Define>(),
                  "rec", mk(wrap<Lambda>(),
                            mk("x"),
                            mk("rec", "x")))
	    (aalloc());
    auto Wrec = W(store, new_types, gamma, wrap(rec));
    {
	    using namespace pattern_match;
	    ASSERT_TRUE
		    (match(fnt("a", "b"),
		           unwrap<Scheme>(Wrec.type).type))
		    << "\n" << printer::print(Wrec.type);
    }
}

TEST_F(Inference, test_cxx_functor)
{
	using namespace make_ast;
	using namespace inference;

	auto add = atl::cxx_functions::WrapStdFunction<bool (long, long)>::a
		([](long a, long b) { return a + b; },
		 store,
		 "add");

	auto expr = mk(wrap(add), "x", "y")(aalloc());

	auto inferred = W(store, new_types, gamma, wrap(expr));

	ASSERT_TRUE(is<Type>(inferred.type));
	ASSERT_EQ(tag<Bool>::value
	          , unwrap<Type>(inferred.type).value());

	apply_substitution(store, gamma, inferred.subs, ref_wrap(expr));

	{
		using namespace pattern_match;
		Symbol a(""), b("");

		ASSERT_TRUE
			(match(ast(tag<CxxFunctor>::value,
			           capture(a), capture(b)),
			       wrap(expr)))
			<< "\n" << printer::with_type(expr);

		ASSERT_TRUE(match(wrap<Type>(tag<Fixnum>::value), a.scheme.type));
		ASSERT_TRUE(match(wrap<Type>(tag<Fixnum>::value), b.scheme.type));
	}
}

/* See if inference works with ((a) (-> bool (-> a (-> a (-> a))))) */
TEST_F(Inference, test_if_scheme)
{
	using namespace make_ast;
	using namespace inference;

	auto arg_type = ++new_types;

	auto foo = *store.symbol
		("foo",
		 Scheme(std::unordered_set<Type::value_type>({arg_type}),
		        wrap(fn_type::fn(fn_type::tt<Bool>(), arg_type, arg_type, arg_type)(aalloc()))));

	auto expr = mk(wrap(&foo), "x", wrap<Type>(tag<Fixnum>::value), "y")(aalloc());

	auto inferred = W(store, new_types, gamma, wrap(expr));

	apply_substitution(store, gamma, inferred.subs, ref_wrap(expr));

	{
		using namespace pattern_match;

		Symbol a, b;
		ASSERT_TRUE
			(match(ast("foo", capture(a), tt<Fixnum>(), capture(b)),
			       wrap(expr)))
			<< "\n" << printer::with_type(expr);

		ASSERT_TRUE(match(tt<Bool>(), a.scheme.type));
		ASSERT_TRUE(match(tt<Fixnum>(), b.scheme.type));
	}
}

TEST_F(Inference, test_thunk)
{
	using namespace make_ast;
	using namespace inference;

	auto add = atl::cxx_functions::WrapStdFunction<bool (long, long)>::a
		([](long a, long b) { return a + b; },
		 store,
		 "add2");

	Symbol fn("add2");
	symbol_assign(fn, wrap(add));

	auto expr = mk(wrap<Lambda>(),
	               mk(),
	               mk(wrap(&fn), "foo", "foo"))(ast_alloc(store));

	auto inferred = W(store, new_types, gamma, wrap(expr));

	{
		using namespace pattern_match;
		ASSERT_TRUE
			(match(ast(wrap<Type>(tag<FunctionConstructor>::value),
			           wrap<Type>(tag<Bool>::value)),
			       inferred.type));
	}

}

TEST_F(Inference, test_applying_defined_lambda)
{
	using namespace make_ast;
	using namespace inference;

	auto def = mk(wrap<Define>(), "foo", 3)(ast_alloc(store)),

		expr = mk(wrap<Define>(), "main",
	               mk(wrap<Lambda>(),
	                  mk(),
	                  mk("add2", "foo", "foo")))(ast_alloc(store));

	// Test that defining a constant works
	W(store, new_types, gamma, wrap(def));
	auto inferred = W(store, new_types, gamma, wrap(expr));

	{
		using namespace pattern_match;
		ASSERT_TRUE(is<Scheme>(inferred.type));

		auto scheme = unwrap<Scheme>(inferred.type);
		ASSERT_TRUE
			(match(ast(wrap<Type>(tag<FunctionConstructor>::value),
			           "a"),
			       scheme.type))
			<< " " << printer::print(scheme.type) << std::endl;
	}
}
