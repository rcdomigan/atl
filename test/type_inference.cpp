/* @file /home/ryan/programming/atl/test/inference.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Feb 28, 2015
 *
 * Test the inference engine on a simple language.
 */

#include <gtest/gtest.h>

#include <atl/lexical_environment.hpp>
#include <atl/type_inference.hpp>
#include <atl/ffi.hpp>
#include <atl/helpers/pattern_match.hpp>

using namespace atl;

// Make sure WResult actually marks things in scope
TEST(TestBasic, test_marking_WResult)
{
	using namespace atl;
	using namespace inference;
	GC gc;

	auto alloc_stuff = [&]() -> WResult
		{
			WResult rval(SubstituteMap(gc), gc.make<String>("str_0"));

			// Stuff in subs should get marked too
			rval.subs[Type(1)] = wrap(gc.raw_make<String>("str_1"));

			return rval;
		};

	{
		auto result = alloc_stuff();

		ASSERT_EQ(*result.subs.manage_marking.itr,
		          dynamic_cast<MarkBase*>(&result.subs));

		ASSERT_EQ(2, gc.cells_allocated());
		gc.gc();
		ASSERT_EQ(2, gc.cells_allocated());
	}

	gc.gc();
	ASSERT_EQ(0, gc.cells_allocated());
}


struct TestSubstitution
	: public ::testing::Test
{
	GC gc;

	TestSubstitution()
	{ init_types(); }
};


TEST_F(TestSubstitution, test_move_SubstituteMap)
{
	using namespace inference;
	SubstituteMap subs0(gc);

	ASSERT_EQ(1, gc._mark_bases.size());
	ASSERT_EQ(gc._mark_bases.begin(), subs0.manage_marking.itr);

	SubstituteMap subs1(std::move(subs0));
	ASSERT_EQ(1, gc._mark_bases.size());
	ASSERT_EQ(gc._mark_bases.begin(), subs1.manage_marking.itr);
	ASSERT_EQ(*subs1.manage_marking.itr,
	          dynamic_cast<MarkBase*>(&subs1));

	ASSERT_EQ(nullptr, subs0.manage_marking.gc);
}

TEST_F(TestSubstitution, test_nop_type_substitution)
{
	using namespace make_ast;
	inference::SubstituteMap subst(gc);

    subst[Type(1)] = wrap<Type>(3);

    auto pre = gc(mk(wrap<Type>(4), wrap<Type>(5)));
    auto post = inference::substitute_type(gc,
                                           subst,
                                           pre.any);

    ASSERT_EQ(*pre, unwrap<Ast>(post));
}

TEST_F(TestSubstitution, test_substitution_as_root)
{
	using namespace make_ast;

	ASSERT_EQ(0, gc._mark_bases.size());

	{
		inference::SubstituteMap subst2(gc);
		ASSERT_EQ(1, gc._mark_bases.size());
		{
			inference::SubstituteMap subst(gc);
			ASSERT_EQ(2, gc._mark_bases.size());
		}
		ASSERT_EQ(1, gc._mark_bases.size());
	}
	ASSERT_EQ(0, gc._mark_bases.size());
}

TEST_F(TestSubstitution, test_type_substitution)
{
	using namespace make_ast;

	inference::SubstituteMap subst(gc);
    subst[Type(1)] = wrap<Type>(3);

    auto ast = gc
	    (mk(wrap<Type>(1),
	        mk(wrap<Type>(1)),
	        wrap<Type>(2)));

    auto expr = inference::substitute_type(gc, subst, ast.any);

    auto expect = gc
	    (mk(wrap<Type>(3),
	        mk(wrap<Type>(3)),
	        wrap<Type>(2)));

    ASSERT_EQ(*expect, unwrap<Ast>(expr))
	    << "Got: " << printer::with_type(expr) << std::endl;
}

/* substitution example from Ian Grant page 8 */
TEST_F(TestSubstitution, test_type_substitution_p8)
{
	using namespace make_ast;

	auto type = [](char cc)
		{ return wrap<Type>(cc + LAST_CONCRETE_TYPE); };

	inference::SubstituteMap subs(gc);
    subs[Type('x' + LAST_CONCRETE_TYPE)] = wrap<Type>('a' + LAST_CONCRETE_TYPE);
    subs[Type('y' + LAST_CONCRETE_TYPE)] = wrap(*gc(mk(type('b'), type('a'))));

    auto do_sub = [&](Any& ast) -> Ast
	    {
		    return unwrap<Ast>
		    (inference::substitute_type(gc, subs, ast));
	    };

    auto first = gc(mk(type('x'),
                       mk(type('b'),
                          type('x'))));

    auto second = gc(mk(type('a'),
                        type('y')));

    auto expect = gc(mk(type('a'), mk(type('b'), type('a'))));

    ASSERT_EQ(*expect, do_sub(first.any))
	    << "Got: " << printer::with_type(first.any) << std::endl;

    ASSERT_EQ(*expect, do_sub(second.any))
	    << "Got: " << printer::with_type(second.any) << std::endl;
}

TEST_F(TestSubstitution, example_p9_assuming_mgu)
{
	using namespace make_ast;
    using namespace inference;

	auto type = [&](char cc) { return wrap<Type>(cc); };

	auto zero = wrap<Type>(0);

	auto left = gc(mk(type(1), type(2), type(3))),
		right = gc(mk(mk(type(2), type(2)),
		              mk(type(3), type(3)),
		              mk(type(0), type(0))));

	SubstituteMap subs(gc);
	subs.emplace(Type(1),
	             wrap(*gc(mk(mk(mk(zero, zero), mk(zero, zero)),
	                         mk(mk(zero, zero), mk(zero, zero))))));
	subs.emplace(Type(2),
	             wrap(*gc(mk(mk(zero, zero), mk(zero, zero)))));

	subs.emplace(Type(3),
	             wrap(*gc(mk(zero, zero))));

    auto sub_left = substitute_type(gc, subs, left.any),
	    sub_right = substitute_type(gc, subs, right.any);

    ASSERT_EQ(explicit_unwrap<Ast>(sub_left),
              explicit_unwrap<Ast>(sub_right));
}

TEST_F(TestSubstitution, test_substitution_composition)
{
	using namespace make_ast;
	using namespace inference;


	{
		SubstituteMap left(gc), right(gc);

		left.emplace(Type(2), wrap<Type>(0));
		right.emplace(Type(1), wrap(*gc(mk(wrap<Type>(2), wrap<Type>(3)))));

		compose_subs(gc, left, right);

		ASSERT_EQ(2, left.size());
		ASSERT_EQ(*gc(mk(wrap<Type>(0), wrap<Type>(3))),
		          explicit_unwrap<Ast>(left[Type(1)]));
	}
}


/* // I'm using 'Ian Grant (The Hindley-Milner Type Inference */
/* // Algorithm)' as the basis for the rules, I'm numbering them */
/* // (i)-(iv) after his Unification algorithm */
struct Unification
    : public ::testing::Test
{
	atl::GC gc;

	Unification()
	{ init_types(); }

	Any type(size_t tt) { return wrap<Type>(tt); }
};

TEST_F(Unification, rule_1)
{
	using namespace make_ast;
	using namespace inference;
    // rule (i)
    SubstituteMap subst = most_general_unification
	    (gc, wrap<Type>('x'),
	     wrap<Type>('y'));


    ASSERT_EQ(subst.size(), 1);
    ASSERT_EQ(subst.begin()->first, Type('y'));
    ASSERT_EQ(subst.begin()->second, wrap<Type>('x'));
}

TEST_F(Unification, rule_2a)
{
	using namespace make_ast;

    //rule (iia) ... should this be an error?  Am I an errorist?
	auto ast = gc(mk(type(1),type(2)));
    auto subst = inference::most_general_unification
	    (gc,
	     wrap<Type>(0),
	     wrap(*ast));

    ASSERT_EQ(subst.size(), 1);
    ASSERT_EQ(tag<Ast>::value, subst[Type(0)]._tag);
    ASSERT_EQ(explicit_unwrap<Ast>(subst[Type(0)]),
              *gc(mk(wrap<Type>(1), wrap<Type>(2))));
}

TEST_F(Unification, rule_2aa)
{
	using namespace make_ast;
	using namespace inference;

    //rule (iia)
	auto ast = gc(mk(type(1), type(2)));
	auto subst = most_general_unification
	    (gc,
	     wrap<Type>(0),
	     wrap(*ast));

    ASSERT_EQ(subst.size(), 1);
    ASSERT_EQ(tag<Ast>::value, subst[Type(0)]._tag);
    ASSERT_EQ(explicit_unwrap<Ast>(subst[Type(0)]),
              *gc(mk(type(1), type(2))));
}

TEST_F(Unification, rule_2b)
{
    using namespace make_ast;
	using namespace inference;

    //rule (iib)
    auto ast = gc(mk(type(0), type(1)));
    auto subst = most_general_unification
	    (gc,
	     wrap(*ast),
	     wrap<Type>(2));

    ASSERT_EQ(subst.size(), 1);
    ASSERT_EQ(tag<Ast>::value, subst[Type(2)]._tag);
    ASSERT_EQ(unwrap<Ast>(subst[Type(2)]),
              *gc(mk(type(0), type(1))));
}

TEST_F(Unification, rule_3)
{
	using namespace make_ast;
	using namespace inference;

    //rule (iii)
	SubstituteMap subst = most_general_unification
	    (gc, wrap<Type>(0), wrap<Type>(0));
    ASSERT_EQ(subst.size(), 0);
}

TEST_F(Unification,  rule_4)
{
	using namespace make_ast;
	auto ast1 = gc(mk(type(0), type(1)));
	auto ast2 = gc(mk(type(2), type(3)));

    auto subst = inference::most_general_unification
		(gc,
		 wrap(*ast1),
		 wrap(*ast2));

    ASSERT_EQ(subst.size(), 2);
    ASSERT_EQ(subst[Type(2)], wrap<Type>(0));
    ASSERT_EQ(subst[Type(3)], wrap<Type>(1));
}

TEST_F(Unification, example_p8)
{
    using namespace make_ast;
    using namespace inference;

    enum { A=0, B=1, X=10, Y=11 };

    auto left = gc(mk(type(A), type(Y))),
	    right = gc(mk(type(X), mk(type(B), type(X))));

    auto subs = inference::most_general_unification
	    (gc, wrap(*left), wrap(*right));

    auto left_post = substitute_type(gc, subs, left.any),
	    right_post = substitute_type(gc, subs, right.any);

    ASSERT_EQ(explicit_unwrap<Ast>(left_post),
              explicit_unwrap<Ast>(right_post));
}

TEST_F(Unification, example_p9)
{
    using namespace make_ast;
    using namespace inference;

    auto left = gc(mk(type(1), type(2), type(3))),
	    right = gc(mk(mk(type(2), type(2)),
	                  mk(type(3), type(3)),
	                  mk(type(0), type(0))));

    auto subs = most_general_unification(gc, wrap(*left), wrap(*right));

    auto sub_left = substitute_type(gc, subs, left.any),
	    sub_right = substitute_type(gc, subs, right.any);

    ASSERT_EQ(explicit_unwrap<Ast>(sub_left),
              explicit_unwrap<Ast>(sub_right));
}

struct SpecializeAndGeneralize
    : public ::testing::Test
{
    GC gc;
	Type::value_type new_types;
	inference::Gamma gamma;

	SpecializeAndGeneralize()
		: new_types(LAST_CONCRETE_TYPE),
		  gamma(gc, new_types)
	{ init_types(); }
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
	              wrap(*gc(mk(wrap<Type>(cc),
	                          mk(wrap<Type>(aa),
	                             wrap<Type>(bb)),
	                          wrap<Type>(dd)))));

	auto expected = gc(mk(wrap<Type>(cc),
	                      mk(wrap<Type>(ee),
	                         wrap<Type>(ff)),
	                      wrap<Type>(dd)));
	Scheme::Quantified expected_subs{ee, ff};

	auto instantiated = gc.marked(instantiate(gc, new_types, scheme));
	ASSERT_EQ(tag<Ast>::value, instantiated->_tag);
	ASSERT_NE(*expected, unwrap<Ast>(*instantiated));

	auto inner_ast = subscripter(unwrap<Ast>(*instantiated))[1];

	ASSERT_TRUE(expected_subs.count(unwrap<Type>(inner_ast[0].value).value()))
		<< "got: " << unwrap<Type>(inner_ast[0].value).value();

	ASSERT_TRUE(expected_subs.count(unwrap<Type>(inner_ast[1].value).value()));
	ASSERT_NE(unwrap<Type>(inner_ast[0].value).value(),
	          unwrap<Type>(inner_ast[1].value).value());
}

TEST_F(SpecializeAndGeneralize, test_free_vars_type)
{
	using namespace inference;

	Type::value_type new_types = LAST_CONCRETE_TYPE;
	auto aa = ++new_types;

	auto type = wrap<Type>(aa);

	Scheme::Quantified free;
	free_type_variables(free, type);

	ASSERT_EQ(1, free.size());
	ASSERT_EQ(1, free.count(aa));
}

TEST_F(SpecializeAndGeneralize, test_free_vars_scheme_free)
{
	using namespace make_ast;
	using namespace inference;

	Type::value_type new_types = LAST_CONCRETE_TYPE;
	auto aa = ++new_types,
		bb = ++new_types;

	auto scheme = gc.make<Scheme>();

	scheme->type = wrap(gc.raw_ast(mk(wrap<Type>(aa), wrap<Type>(bb))));

	auto free = free_type_variables(*scheme);

	ASSERT_EQ(2, free.size());
	ASSERT_EQ(1, free.count(aa));
	ASSERT_EQ(1, free.count(bb));
}

TEST_F(SpecializeAndGeneralize, test_free_vars_scheme_bound)
{
	using namespace make_ast;
	using namespace inference;

	Type::value_type new_types = LAST_CONCRETE_TYPE;
	auto aa = ++new_types,
		bb = ++new_types;

	auto scheme = gc.make<Scheme>();

	scheme->type = wrap
		(gc.raw_ast
		 (mk(wrap<Type>(aa), wrap<Type>(bb))));
	scheme->quantified.insert(aa);
	scheme->quantified.insert(bb);

	auto free = free_type_variables(*scheme);

	ASSERT_EQ(0, free.size());
}

TEST_F(SpecializeAndGeneralize, test_gamma_free_type_vars)
{
	using namespace inference;

	Type::value_type new_types = LAST_CONCRETE_TYPE;
	auto aa = ++new_types;

	gamma.symbols["foo"] = Scheme(Scheme::Quantified(),
	                              wrap<Type>(aa));

	auto free = free_type_variables(gamma);
	ASSERT_EQ(1, free.size());
	ASSERT_EQ(1, free.count(aa));
}

TEST_F(SpecializeAndGeneralize, test_generalize_free_type)
{
	using namespace make_ast;
	using namespace inference;

	Type::value_type new_types = LAST_CONCRETE_TYPE;
	auto aa = ++new_types;

	Scheme scheme = generalize(gamma, wrap<Type>(aa));

	ASSERT_EQ(1, scheme.quantified.size());
}

TEST_F(SpecializeAndGeneralize, test_generalize_free_in_something_else)
{
	using namespace make_ast;
	using namespace inference;

	Type::value_type new_types = LAST_CONCRETE_TYPE;
	auto aa = ++new_types;

	gamma.symbols["foo"] = Scheme(Scheme::Quantified(),
	                              wrap<Type>(aa));

	Scheme scheme = generalize(gamma, wrap<Type>(aa));

	ASSERT_EQ(0, scheme.quantified.size());
}

TEST_F(SpecializeAndGeneralize, test_apply_substitution)
{
	using namespace make_ast;
	using namespace inference;

	Type::value_type new_types = LAST_CONCRETE_TYPE;
	auto aa_type = ++new_types,
		bb_type = ++new_types;

	auto aa = gc.make<Symbol>("aa", Scheme(wrap<Type>(aa_type))),
		bb = gc.make<Symbol>("bb", Scheme(wrap<Type>(bb_type)));

	auto expr = gc(mk(aa.any, bb.any));

	SubstituteMap subs{gc};

	subs[aa_type] = wrap(gc.raw_ast(mk(wrap<Type>(new_types + 1),
	                                   wrap<Type>(new_types + 2))));
	new_types += 2;

	subs[bb_type] = wrap<Type>(++new_types);

	apply_substitution(gc, subs, expr.any);

	ASSERT_TRUE(is<Ast>(aa->scheme.type));
	ASSERT_EQ(gc.raw_ast(mk(wrap<Type>(bb_type + 1),
	                        wrap<Type>(bb_type + 2))),
	          unwrap<Ast>(aa->scheme.type));
	ASSERT_EQ(wrap<Type>(bb_type + 3), bb->scheme.type);
}

struct Inference
	: public Unification
{
	Type::value_type new_types;
	inference::Gamma gamma;
	inference::AlgorithmW w;

	Inference()
		: Unification(),
		  new_types(LAST_CONCRETE_TYPE),
		  gamma(gc, new_types),
		  w(gc, new_types, gamma)
	{}
};

TEST_F(Inference, test_lambda)
{
    using namespace inference;
    using namespace make_ast;

    auto foo = gc.make<LambdaMetadata>
	    (gc(mk("a")));

    auto e1 = gc(mk(wrap<Lambda>(foo.pointer()),
                    mk("a"),
                    "a"));

    auto We1 = w.W(e1->subex());

    ASSERT_EQ(tag<Ast>::value, We1.type->_tag);

    auto type = unwrap<Ast>(*We1.type);
    ASSERT_EQ(type[0], function_constructor())
	    << "Got: " << printer::print(type);

    ASSERT_EQ(type[1], type[2]);
}

TEST_F(Inference, test_multi_arg_lambda)
{
    using namespace inference;
    using namespace make_ast;

    auto metadata = gc.make<LambdaMetadata>
	    (gc(mk("a", "b")));
    auto e1 = gc(mk(wrap<Lambda>(metadata.pointer()),
                    mk("a", "b"),
                    "a"));

    auto We1 = w.W(e1);

    {
	    using namespace pattern_match;
	    ASSERT_TRUE
		    (match(fnt_rest("a", "b", "a"),
		           *We1.type))
		    << printer::with_type(*e1);
    }
}

TEST_F(Inference, test_simple_application)
{
	using namespace inference;
	using namespace make_ast;

	auto fn_type = new_types + 1;
	auto expr = gc(mk("fn", "arg"));

	auto inferred = w.W(expr);

	auto scheme = inferred.subs.at(fn_type);

	ASSERT_TRUE(is<Ast>(scheme));
	// Function constructor, argument, result: (-> a b)
	ASSERT_EQ(3, unwrap<Ast>(scheme).size())
		<< printer::print(scheme);
}

/* Same "simple lambda" used in analyse_and_compile */
TEST_F(Inference, test_simple_lambda)
{
	using namespace inference;
	using namespace make_ast;

	auto metadata = gc.make<LambdaMetadata>
		(gc(mk("a", "b")));
	auto expr = gc(mk(wrap<Lambda>(metadata.pointer()),
	                  mk("a", "b"),
	                  mk("add2", "a", "b")));

	auto inferred = w.W(expr);
	apply_substitution(gc, inferred.subs, expr.any);

	{
		using namespace pattern_match;
		ASSERT_TRUE
			(match(fnt_rest("x", "y", "z"),
			       *inferred.type))
			<< "\n" << printer::with_type(*inferred.type);
	}
}

TEST_F(Inference, test_application)
{
    using namespace make_ast;
    using namespace inference;

	auto metadata = gc.make<LambdaMetadata>
		(gc(mk("x", "y")));
	auto e1 = gc(mk(wrap<Lambda>(metadata.pointer()),
	                mk("x", "y"),
	                mk("x", "y")));
    auto We1 = w.W(e1);

    /* Check the body: */
    {
	    using namespace pattern_match;
	    ASSERT_TRUE
		    (match(fnt_rest(fnt("a", "b"), fnt("a", "b")),
		           *We1.type))
		    << printer::with_type(*We1.type);
    }
}

TEST_F(Inference, test_define)
{
    using namespace make_ast;
    using namespace inference;

    auto e1 = gc(mk(wrap<Define>(), "a", "b"));

    auto We1 = w.W(e1);

    apply_substitution(gc, We1.subs, e1.any);

    ASSERT_TRUE(is<Scheme>(*We1.type));

    auto& scheme = unwrap<Scheme>(*We1.type);
    ASSERT_EQ(1, scheme.quantified.size());

    ASSERT_TRUE(is<Type>(scheme.type));
    ASSERT_EQ(1, scheme.quantified.count(unwrap<Type>(scheme.type).value()));
}

TEST_F(Inference, test_apply_defined)
{
    using namespace make_ast;
    using namespace inference;

    // id function
    auto metadata = gc.make<LambdaMetadata>
	    (gc(mk("x")));

    auto e1 = gc(mk(wrap<Define>(), "id",
                    mk(wrap<Lambda>(metadata.pointer()), mk("x"), "x")));

    auto We1 = w.W(e1);

    ASSERT_TRUE(is<Scheme>(*We1.type));

    auto id_scheme = unwrap<Scheme>(*We1.type);
    ASSERT_EQ(1, id_scheme.quantified.size());

    ASSERT_TRUE(is<Ast>(id_scheme.type));
    ASSERT_EQ(*id_scheme.quantified.begin(),
              unwrap<Type>(explicit_unwrap<Ast>(id_scheme.type)[1]).value());
    ASSERT_EQ(*id_scheme.quantified.begin(),
              unwrap<Type>(explicit_unwrap<Ast>(id_scheme.type)[2]).value());

    ASSERT_TRUE(We1.subs.empty());

    apply_substitution(gc, We1.subs, e1.any);

    // Now see if we can apply the newly defined "id"
    auto e2 = gc(mk("id", "y"));
    auto We2 = w.W(e2);

    ASSERT_EQ(2, We2.subs.size());
    ASSERT_TRUE(is<Type>(*We2.type));

    ASSERT_NE(unwrap<Type>(*We2.type).value(),
              *id_scheme.quantified.begin());

    ASSERT_TRUE(is<Symbol>((*e2)[1]));
    ASSERT_TRUE(is<Type>(unwrap<Symbol>((*e2)[1]).scheme.type));

    ASSERT_NE(unwrap<Type>(*We2.type).value(),
              unwrap<Type>(unwrap<Symbol>((*e2)[1]).scheme.type).value());

    apply_substitution(gc, We2.subs, e2.any);

    ASSERT_EQ(unwrap<Type>(*We2.type).value(),
              unwrap<Type>(unwrap<Symbol>((*e2)[1]).scheme.type).value());

}

TEST_F(Inference, test_nested_application)
{
    using namespace make_ast;
    using namespace inference;

    auto metadata = gc.make<LambdaMetadata>
	    (gc(mk("x", "y", "z")));
    auto e1 = gc(mk(wrap<Lambda>(metadata.pointer()),
                    mk("x", "y", "z"),
                    mk("x", mk("y", "z"))));

    auto We1 = w.W(e1);

    {
	    using namespace pattern_match;
	    ASSERT_TRUE
		    (match(fnt_rest(fnt("a", "b"), fnt(fnt("c", "a"), fnt("c", "b"))),
		           *We1.type))
		    << "\n" << printer::print(*We1.type);
    }
}

TEST_F(Inference, test_multi_arg_application)
{
    using namespace make_ast;
    using namespace inference;

    auto metadata = gc.make<LambdaMetadata>
	    (gc(mk("x", "y", "z")));
    auto e1 = gc(mk(wrap<Lambda>(metadata.pointer()),
                    mk("x", "y", "z"),
                    mk("x", "y", "z")));

    auto We1 = w.W(e1);

    {
	    using namespace pattern_match;
	    ASSERT_TRUE
		    (match(fnt_rest(fnt("a", "b", "c"), "a", "b", "c"),
		           *We1.type))
		    << "\n" << printer::print(*We1.type);
    }
    apply_substitution(gc, We1.subs, e1.any);

    // z should be a non-function type
    {
	    auto z = subscript(*e1, 1, 2);
	    ASSERT_EQ(tag<Symbol>::value, z._tag);
	    ASSERT_EQ(tag<Type>::value,
	              unwrap<Symbol>(z).scheme.type._tag)
		    << "Got " << type_name(unwrap<Symbol>(z).scheme.type._tag) << std::endl;
    }

    // y should also be a non-function (as I keep forgetting: x
    // _applied_ to y to produces a function)
    {
	    auto y = AstSubscripter(*e1)[1][1].value;
	    ASSERT_EQ(tag<Symbol>::value, y._tag);
	    ASSERT_EQ(tag<Type>::value,
	              unwrap<Symbol>(y).scheme.type._tag)
		    << "Got " << type_name(unwrap<Symbol>(y).scheme.type._tag) << std::endl;

    }

    {
	    using namespace pattern_match;
	    auto x = AstSubscripter(*e1)[1][0].value;
	    ASSERT_EQ(tag<Symbol>::value, x._tag);
	    ASSERT_TRUE
		    (match(fnt_rest("a", "b", "c"),
		           unwrap<Symbol>(x).scheme.type))
		    << "\n" << printer::print(unwrap<Symbol>(x).scheme.type);
    }

}

TEST_F(Inference, test_recursive_fn)
{
    using namespace make_ast;
    using namespace inference;

    auto metadata = gc.make<LambdaMetadata>
	    (gc(mk("x")));
    auto rec = gc(mk(wrap<Define>(),
                     "rec", mk(wrap<Lambda>(metadata.pointer()),
                               mk("x"),
                               mk("rec", "x"))));
    auto Wrec = w.W(rec);
    {
	    using namespace pattern_match;
	    ASSERT_TRUE
		    (match(fnt_rest("a", "b"),
		           unwrap<Scheme>(*Wrec.type).type))
		    << "\n" << printer::print(*Wrec.type);
    }
}

TEST_F(Inference, test_cxx_functor)
{
	using namespace make_ast;
	using namespace inference;

	auto add = atl::cxx_functions::WrapStdFunction<bool (long, long)>::a
		([](long a, long b) { return a + b; },
		 gc,
		 "add");

	auto expr = gc(mk(add.any, "x", "y"));

	auto inferred = w.W(expr);

	ASSERT_TRUE(is<Type>(*inferred.type));
	ASSERT_EQ(tag<Bool>::value
	          , unwrap<Type>(*inferred.type).value());

	apply_substitution(gc, inferred.subs, expr.any);

	{
		using namespace pattern_match;
		Symbol a(""), b("");

		ASSERT_TRUE
			(match(rest(tag<CxxFunctor>::value,
			           capture(a), capture(b)),
			       expr))
			<< "\n" << printer::with_type(*expr);

		ASSERT_EQ(tt<Fixnum>(), a.scheme.type);
		ASSERT_EQ(tt<Fixnum>(), b.scheme.type);
	}
}

/* See if inference works with ((a) (-> bool (-> a (-> a (-> a))))) */
TEST_F(Inference, test_if_scheme)
{
	using namespace make_ast;
	using namespace inference;

	auto arg_type = ++new_types;

	auto sym_type = gc(fn_type::fn(fn_type::tt<Bool>(), arg_type, arg_type, arg_type));
	auto foo = gc.make<Symbol>
		("foo",
		 Scheme({arg_type}, sym_type.any));

	auto expr = gc(mk(foo.any, "x", wrap<Type>(tag<Fixnum>::value), "y"));

	auto inferred = w.W(expr);

	ASSERT_EQ(inferred.type.any, wrap<Type>(tag<Fixnum>::value));

	apply_substitution(gc, inferred.subs, expr.any);

	{
		using namespace pattern_match;

		Symbol a, b;
		throwing_match(rest("foo", capture(a), tt<Fixnum>(), capture(b)),
		               *expr);

		ASSERT_EQ(tt<Bool>(), a.scheme.type);
		ASSERT_EQ(tt<Fixnum>(), b.scheme.type);
	}
}

TEST_F(Inference, test_thunk)
{
	using namespace make_ast;
	using namespace inference;

	auto equal2 = atl::cxx_functions::WrapStdFunction<bool (long, long)>::a
		([](long a, long b) { return a == b; },
		 gc,
		 "eqal2");

	auto metadata = gc.make<LambdaMetadata>
		(gc(mk()));

	auto expr = gc(mk(wrap<Lambda>(metadata.pointer()),
	               mk(),
	               mk(equal2.any, "foo", "foo")));

	auto inferred = w.W(expr);

	{
		using namespace pattern_match;
		ASSERT_TRUE
			(match(fnt_rest(wrap<Type>(tag<Bool>::value)),
			       *inferred.type))
			<< "got: " << printer::print(*inferred.type) << std::endl;
	}

}

TEST_F(Inference, test_define_thunk)
{
	using namespace make_ast;
	using namespace inference;

	auto equal2 = atl::cxx_functions::WrapStdFunction<bool (long, long)>::a
		([](long a, long b) { return a == b; },
		 gc,
		 "eqal2");

	auto metadata = gc.make<LambdaMetadata>
		(gc(mk()));

	auto expr = gc
		(mk(wrap<Define>(),
		    "bar",
		    mk(wrap<Lambda>(metadata.pointer()),
		       mk(),
		       mk(equal2.any, "foo", "foo"))));
	w.W(expr);

	auto expr2 = gc(mk("bar"));
	auto inferred = w.W(expr2);

	{
		using namespace pattern_match;
		ASSERT_TRUE(is<Type>(*inferred.type));

		auto type = unwrap<Type>(*inferred.type);
		ASSERT_EQ(tag<Bool>::value, type.value());
	}
}

TEST_F(Inference, test_applying_defined_lambda)
{
	using namespace make_ast;
	using namespace inference;

    auto metadata = gc.make<LambdaMetadata>
	    (gc(mk()));
    auto def = gc(mk(wrap<Define>(), "foo", 3)),

	    expr = gc(mk(wrap<Define>(), "main",
	                 mk(wrap<Lambda>(metadata.pointer()),
	                    mk(),
	                    mk("add2", "foo", "foo"))));

	// Test that defining a constant works
	w.W(def);
	auto inferred = w.W(expr);

	{
		using namespace pattern_match;
		ASSERT_TRUE(is<Scheme>(*inferred.type));

		auto scheme = unwrap<Scheme>(*inferred.type);
		ASSERT_TRUE
			(match(fnt_rest("a"), scheme.type))
			<< " " << printer::print(scheme.type) << std::endl;
	}
}

TEST_F(Inference, test_if_constant_results)
{
	using namespace fn_type;
	using namespace make_ast;

	auto if_result = ++new_types;

	auto if_scheme = gc(fn(tt<Bool>(), if_result, if_result, if_result));
	auto equal_scheme = gc(fn(tt<Fixnum>(), tt<Fixnum>(), tt<Bool>()));

	auto _if = gc.make<Symbol>("if", Scheme({if_result}, if_scheme.any)),
		equal = gc.make<Symbol>("equal", Scheme(equal_scheme.any)),
		consequent = gc.make<Symbol>("consequent", Scheme(wrap<Type>(++new_types))),
		alternate = gc.make<Symbol>("alternate", Scheme(tt<Fixnum>()));

	auto expr = gc(mk(_if.any,
	                  mk(equal.any, 1, 2),
	                  consequent.any,
	                  alternate.any));

	auto inferred = w.W(expr);

	apply_substitution(gc, inferred.subs, expr.any);

	ASSERT_EQ(tt<Fixnum>(), inferred.type.any)
			<< " " << printer::print(inferred.type.any) << std::endl;
}

TEST_F(Inference, test_if_function_results)
{
	using namespace fn_type;
	using namespace make_ast;

	auto if_result = ++new_types;

	auto if_scheme = gc(fn(tt<Bool>(), if_result, if_result, if_result));
	auto equal_scheme = gc(fn(tt<Fixnum>(), tt<Fixnum>(), tt<Bool>()));
	auto add_scheme = gc(fn(tt<Fixnum>(), tt<Fixnum>(), tt<Fixnum>()));
	auto sub_scheme = gc(fn(tt<Fixnum>(), tt<Fixnum>(), tt<Fixnum>()));

	auto _if = gc.make<Symbol>("if", Scheme({if_result}, if_scheme.any)),
		equal = gc.make<Symbol>("equal", Scheme(equal_scheme.any)),
		add = gc.make<Symbol>("add", Scheme(add_scheme.any)),
		sub = gc.make<Symbol>("sub", Scheme(sub_scheme.any));

	auto expr = gc(mk(_if.any,
	                  mk(equal.any, 1, 2),
	                  mk(add.any, 7, 3),
	                  mk(sub.any, 7, 3)));

	auto inferred = w.W(expr);

	ASSERT_EQ(tt<Fixnum>(), inferred.type.any)
			<< " " << printer::print(inferred.type.any) << std::endl;
}
