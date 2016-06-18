/* @file /home/ryan/programming/atl/test/inference.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Feb 28, 2015
 *
 * Test the inference engine on a simple language.
 */

#include <gtest/gtest.h>

#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include <lexical_environment.hpp>
#include <type_inference.hpp>

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

    auto make_expr = make(lift<Type>(4), lift<Type>(5));

    auto pre = make_expr(ast_alloc(arena));
    auto post = inference::substitute_type(ast_alloc(arena),
                                           subst,
                                           pass_value(pre));

    ASSERT_EQ(pre, explicit_unwrap<Ast>(post));
}

TEST_F(TestSubstitution, test_type_substitution)
{
	using namespace make_ast;
	Arena arena;

    inference::SubstituteMap subst;
    subst[Type(1)] = wrap<Type>(3);

    auto expr = inference::substitute_type
	    (ast_alloc(arena),
	     subst,
	     pass_value(make
	                (lift<Type>(1),
	                 make(lift<Type>(1)),
	                 lift<Type>(2))
	                (ast_alloc(arena))));

    auto expect = make(lift<Type>(3),
                       make(lift<Type>(3)),
                       lift<Type>(2))(ast_alloc(arena));

    ASSERT_EQ(expect, explicit_unwrap<Ast>(expr));
}

/* substitution example from Ian Grant page 8 */
TEST_F(TestSubstitution, test_type_substitution_p8)
{
	using namespace make_ast;
	Arena arena;

	auto type = [](char cc)
		{ return lift<Type>(cc + LAST_CONCRETE_TYPE); };


    inference::SubstituteMap subs;
    subs[Type('x' + LAST_CONCRETE_TYPE)] = wrap<Type>('a' + LAST_CONCRETE_TYPE);
    subs[Type('y' + LAST_CONCRETE_TYPE)] = wrap(make(type('b'), type('a'))(ast_alloc(arena)));

    auto do_sub = [&](Ast& ast) -> Ast
	    {
		    return Ast(reinterpret_cast<AstData*>
		               (inference::substitute_type(ast_alloc(arena),
		                                           subs,
		                                           PassByValue(ast)).value));
	    };

    auto first = make(type('x'),
                      make(type('b'),
                           type('x')))(ast_alloc(arena));

    auto second = make(type('a'),
                       type('y'))(ast_alloc(arena));

    auto expect = make(type('a'), make(type('b'), type('a')))(ast_alloc(arena));

    ASSERT_EQ(expect, do_sub(first));
    ASSERT_EQ(expect, do_sub(second));
}

TEST_F(TestSubstitution, example_p9_assuming_mgu)
{
	using namespace make_ast;
    using namespace inference;
    Arena arena;

	auto type = [&](char cc) { return lift<Type>(cc); };

	auto zero = lift<Type>(0); //[&] () { return type(0); }
    auto aalloc = [&](){ return ast_alloc(arena); };

    auto left = make(type(1), type(2), type(3))(aalloc()),
	    right = make(make(type(2), type(2)),
	                 make(type(3), type(3)),
	                 make(type(0), type(0)))(aalloc());

    SubstituteMap subs
	    ({
		    make_pair
			    (Type(1),
			     wrap(make(make(make(zero, zero), make(zero, zero)),
			               make(make(zero, zero), make(zero, zero)))(aalloc()))),

			    make_pair
			    (Type(2),
			     wrap(make(make(zero, zero), make(zero, zero))(aalloc()))),

			    make_pair
			    (Type(3),
			     wrap(make(zero, zero)(aalloc())))
			    });

    auto sub_left = substitute_type(aalloc(), subs, pass_value(left)),
	    sub_right = substitute_type(aalloc(), subs, pass_value(right));

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
			right({{Type(1), wrap(make(lift<Type>(2), lift<Type>(3))(ast_alloc(arena)))}});
		compose_subs(arena, left, right);

		ASSERT_EQ(2, left.size());
		ASSERT_EQ(make(lift<Type>(0), lift<Type>(3))(ast_alloc(arena)),
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

	make_ast::AstAllocator aalloc() { return make_ast::ast_alloc(store); }
	make_ast::ast_composer type(size_t tt)
	{ return make_ast::lift<Type>(tt); }
};

TEST_F(Unification, rule_1)
{
	using namespace make_ast;
    // rule (i)
    auto subst = inference::most_general_unification
	    (store, pass_value<Type>('x'),
	     pass_value<Type>('y'));


    ASSERT_EQ(subst.size(), 1);
    ASSERT_EQ(subst.begin()->first, Type('y'));
    ASSERT_EQ(subst.begin()->second, wrap<Type>('x'));
}

TEST_F(Unification, rule_2a)
{
	using namespace make_ast;

    //rule (iia) ... should this be an error?  Am I an errorist?
    auto subst = inference::most_general_unification
	    (store,
	     pass_value<Type>(0),
	     pass_value(make(lift<Type>(1),
	                     lift<Type>(2))
	                (ast_alloc(store))));

    ASSERT_EQ(subst.size(), 1);
    ASSERT_EQ(tag<Ast>::value, subst[Type(0)]._tag);
    ASSERT_EQ(explicit_unwrap<Ast>(subst[Type(0)]),
              make(lift<Type>(1), lift<Type>(2))(ast_alloc(store)));
}

TEST_F(Unification, rule_2aa)
{
	using namespace make_ast;

    //rule (iia)
    auto subst = inference::most_general_unification
	    (store,
	     pass_value<Type>(0),
	     pass_value(make(type(1), type(2))(ast_alloc(store))));

    ASSERT_EQ(subst.size(), 1);
    ASSERT_EQ(tag<Ast>::value, subst[Type(0)]._tag);
    ASSERT_EQ(explicit_unwrap<Ast>(subst[Type(0)]),
              make(type(1), type(2))(ast_alloc(store)));
}

TEST_F(Unification, rule_2b)
{
    using namespace make_ast;
    //rule (iib)
    auto subst = inference::most_general_unification
	    (store,
	     pass_value(make(type(0), type(1))(ast_alloc(store))),
	     pass_value<Type>(2));

    ASSERT_EQ(subst.size(), 1);
    ASSERT_EQ(tag<Ast>::value, subst[Type(2)]._tag);
    ASSERT_EQ(explicit_unwrap<Ast>(subst[Type(2)]),
              make(type(0), type(1))(ast_alloc(store)));
}

TEST_F(Unification, rule_3)
{
	using namespace make_ast;

    //rule (iii)
    auto subst = inference::most_general_unification
	    (store, pass_value<Type>(0), pass_value<Type>(0));
    ASSERT_EQ(subst.size(), 0);
}

TEST_F(Unification,  rule_4)
{
	using namespace make_ast;

    auto subst = inference::most_general_unification
		(store,
		 pass_value(make(type(0), type(1))(aalloc())),
		 pass_value(make(type(2), type(3))(aalloc())));

    ASSERT_EQ(subst.size(), 2);
    ASSERT_EQ(subst[Type(2)], wrap<Type>(0));
    ASSERT_EQ(subst[Type(3)], wrap<Type>(1));
}

TEST_F(Unification, example_p8)
{
    using namespace make_ast;
    using namespace inference;

    enum { A=0, B=1, X=10, Y=11 };

    auto left = make(type(A), type(Y))(aalloc()),
	    right = make(type(X), make(type(B), type(X)))(aalloc());

    auto subs = inference::most_general_unification
	    (store, pass_value(left), pass_value(right));

    auto left_post = substitute_type(aalloc(), subs, pass_value(left)),
	    right_post = substitute_type(aalloc(), subs, pass_value(right));

    ASSERT_EQ(explicit_unwrap<Ast>(left_post),
              explicit_unwrap<Ast>(right_post));
}

TEST_F(Unification, example_p9)
{
    using namespace make_ast;
    using namespace inference;

    auto left = make(type(1), type(2), type(3))(aalloc()),
	    right = make(make(type(2), type(2)),
	                 make(type(3), type(3)),
	                 make(type(0), type(0)))(aalloc());

    auto subs = most_general_unification(store, pass_value(left), pass_value(right));

    auto sub_left = substitute_type(aalloc(), subs, pass_value(left)),
	    sub_right = substitute_type(aalloc(), subs, pass_value(right));

    ASSERT_EQ(explicit_unwrap<Ast>(sub_left),
              explicit_unwrap<Ast>(sub_right));
}


struct SpecializeAndGeneralize
    : public ::testing::Test
{
    Arena store;
	SpecializeAndGeneralize() { init_types(); }
};

TEST_F(SpecializeAndGeneralize, instantiate)
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
	              wrap(make(lift<Type>(cc),
	                        make(lift<Type>(aa),
	                             lift<Type>(bb)),
	                        lift<Type>(dd))(ast_alloc(store))));

	auto expected = make(lift<Type>(cc),
	                     make(lift<Type>(ee),
	                          lift<Type>(ff)),
	                     lift<Type>(dd))(ast_alloc(store));
	Scheme::Quantified expected_subs{ee, ff};

	auto instantiated = instantiate(store, new_types, scheme);
	ASSERT_EQ(tag<Ast>::value, instantiated._tag);
	ASSERT_NE(expected, explicit_unwrap<Ast>(instantiated));

	auto inner_ast = subscripter(explicit_unwrap<Ast>(instantiated))[1];

	ASSERT_TRUE(expected_subs.count(unwrap<Type>(inner_ast[0].value).value)) << "got: " << unwrap<Type>(inner_ast[0].value).value;
	ASSERT_TRUE(expected_subs.count(unwrap<Type>(inner_ast[1].value).value));
	ASSERT_NE(unwrap<Type>(inner_ast[0].value).value,
	          unwrap<Type>(inner_ast[1].value).value);
}

TEST_F(SpecializeAndGeneralize, generalize)
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
	              wrap(make(lift<Type>(cc),
	                        make(lift<Type>(aa),
	                             lift<Type>(bb)),
	                        lift<Type>(dd))(ast_alloc(store))));

	auto expected = make(lift<Type>(cc),
	                     make(lift<Type>(ee),
	                          lift<Type>(ff)),
	                     lift<Type>(dd))(ast_alloc(store));
	Scheme::Quantified expected_subs{ee, ff};

	auto instantiated = instantiate(store, new_types, scheme);
	ASSERT_EQ(tag<Ast>::value, instantiated._tag);
	ASSERT_NE(expected, explicit_unwrap<Ast>(instantiated));

	auto inner_ast = subscripter(explicit_unwrap<Ast>(instantiated))[1];

	ASSERT_TRUE(expected_subs.count(unwrap<Type>(inner_ast[0].value).value)) << "got: " << unwrap<Type>(inner_ast[0].value).value;
	ASSERT_TRUE(expected_subs.count(unwrap<Type>(inner_ast[1].value).value));
	ASSERT_NE(unwrap<Type>(inner_ast[0].value).value,
	          unwrap<Type>(inner_ast[1].value).value);
}

struct Inference
	: public Unification
{
	Type::value_type new_types;
	inference::Gamma gamma;
	SymbolMap symbols;

	Inference()
		: Unification(), new_types(LAST_CONCRETE_TYPE), symbols(nullptr)
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

	auto e1 = W(store, new_types, gamma, pass_value(wrap(&sym_a)));
    auto e2 = W(store, new_types, gamma, pass_value(wrap(&sym_b)));
	auto e3 = W(store, new_types, gamma, pass_value(wrap(&sym_a)));

    ASSERT_NE(e1.type.value, e2.type.value);
    ASSERT_EQ(e1.type.value, e3.type.value);
}

TEST_F(Inference, test_lambda)
{
    using namespace inference;
    using namespace make_ast;

    auto e1 = make(lift<Lambda>(),
                   make(sym("a")),
                   sym("a"))(aalloc());
    auto wrapped = wrap(e1);

    FreeSymbols _free;
    assign_free(symbols, store, _free, wrapped);

    auto We1 = W(store, new_types, gamma, pass_value(e1));

    ASSERT_EQ(tag<Ast>::value, We1.type._tag);

    auto type = explicit_unwrap<Ast>(We1.type);
    ASSERT_EQ(type[0], wrap<Type>(tag<FunctionConstructor>::value));
    ASSERT_EQ(type[1], type[2]);
}

TEST_F(Inference, test_multi_arg_lambda)
{
    using namespace inference;
    using namespace make_ast;

    auto e1 = make(lift<Lambda>(),
                   make(sym("a"), sym("b")),
                   sym("a"))(aalloc());
    auto wrapped = wrap(e1);

    FreeSymbols _free;
    assign_free(symbols, store, _free, wrapped);

    auto We1 = W(store, new_types, gamma, pass_value(e1));

    auto type = explicit_unwrap<Ast>(We1.type);
    ASSERT_EQ(type[0], wrap<Type>(tag<FunctionConstructor>::value));
    ASSERT_EQ(type[1], type[3]);
    ASSERT_NE(type[1], type[2]);
}

TEST_F(Inference, test_application)
{
    using namespace make_ast;
    using namespace inference;

    auto fcc = []() { return lift<Type>(tag<FunctionConstructor>::value); };

    auto e1 = make(lift<Lambda>(),
                   make(sym("x"), sym("y")),
                   make(sym("x"), sym("y")))(aalloc());
    auto We1 = W(store, new_types, gamma, pass_value(e1));

    auto wrapped = wrap(e1);
    apply_substitution(store, gamma, We1.subs, wrapped);

    /* first possible free type is not used in final inference */
    auto new_types = LAST_CONCRETE_TYPE + 1;

    auto a = ++new_types;
    auto b = ++new_types;

    ASSERT_EQ(make(fcc(),
                   make(fcc(), lift<Type>(a), lift<Type>(b)),
                   lift<Type>(a), lift<Type>(b))(aalloc()),
              explicit_unwrap<Ast>(We1.type));
}

TEST_F(Inference, test_define)
{
    using namespace make_ast;
    using namespace inference;

    auto e1 = make(lift<Define>(), sym("a"), sym("b"))(aalloc());
    auto wrapped = wrap(e1);

    auto We1 = W(store, new_types, gamma, pass_value(e1));

    apply_substitution(store, gamma, We1.subs, wrapped);

    ASSERT_TRUE(is<Scheme>(We1.type));

    auto& scheme = unwrap<Scheme>(We1.type);
    ASSERT_EQ(1, scheme.quantified.size());

    ASSERT_TRUE(is<Type>(scheme.type));
    ASSERT_EQ(1, scheme.quantified.count(unwrap<Type>(scheme.type).value));
}

TEST_F(Inference, test_apply_defined)
{
    using namespace make_ast;
    using namespace inference;

    // id function
    auto e1 = make(lift<Define>(), sym("id"),
                   make(lift<Lambda>(),
                        make(sym("x")),
                        sym("x")))(aalloc());
    auto wrapped = wrap(e1);

    auto We1 = W(store, new_types, gamma, pass_value(e1));

    ASSERT_TRUE(is<Scheme>(We1.type));

    auto id_scheme = unwrap<Scheme>(We1.type);
    ASSERT_EQ(1, id_scheme.quantified.size());

    ASSERT_TRUE(is<Ast>(id_scheme.type));
    ASSERT_EQ(*id_scheme.quantified.begin(),
              unwrap<Type>(explicit_unwrap<Ast>(id_scheme.type)[1]).value);
    ASSERT_EQ(*id_scheme.quantified.begin(),
              unwrap<Type>(explicit_unwrap<Ast>(id_scheme.type)[2]).value);

    ASSERT_TRUE(We1.subs.empty());

    apply_substitution(store, gamma, We1.subs, wrapped);

    // Now see if we can apply the newly defined "id"
    auto e2 = make(sym("id"), sym("y"))(aalloc());
    auto We2 = W(store, new_types, gamma, pass_value(e2));

    ASSERT_EQ(2, We2.subs.size());
    ASSERT_TRUE(is<Type>(We2.type));

    ASSERT_NE(unwrap<Type>(We2.type).value,
              *id_scheme.quantified.begin());

    ASSERT_TRUE(is<Symbol>(e2[1]));
    ASSERT_TRUE(is<Type>(unwrap<Symbol>(e2[1]).scheme.type));

    ASSERT_NE(unwrap<Type>(We2.type).value,
              unwrap<Type>(unwrap<Symbol>(e2[1]).scheme.type).value);

    auto wrapped2 = wrap(e2);
    apply_substitution(store, gamma, We2.subs, wrapped2);

    ASSERT_EQ(unwrap<Type>(We2.type).value,
              unwrap<Type>(unwrap<Symbol>(e2[1]).scheme.type).value);

}


typedef std::map<std::string, Type> SeenShapeMap;


bool _shape_check(SeenShapeMap& map, Slice target, Slice got)
{
	if(target.size() != got.size())
		{ return false; }

	for(auto vv : zip(target, got))
		{
			auto aa = pass_value(*get<0>(vv)),
				bb = pass_value(*get<1>(vv));

			switch(aa._tag)
				{
				case tag<Slice>::value:
					if(!is<Slice>(bb)) { return false; }

					if(!_shape_check(map, aa.slice, bb.slice))
						{ return false; }
					break;

				case tag<Symbol>::value:
					{
						if(!is<Type>(bb)) { return false; }

						auto sym = unwrap<Symbol>(aa);
						auto type = unwrap<Type>(bb);

						auto itr = map.find(sym.name);
						if(itr != map.end() && itr->second != type)
							{ return false; }
						else
							{ map.emplace(make_pair(sym.name, type)); }
						break;
					}
				}
		}
	return true;
}


bool shape_check(Ast target, Ast got)
{
	SeenShapeMap map;
	return _shape_check(map, Slice(target), Slice(got));
}


TEST_F(Inference, test_nested_application)
{
    using namespace make_ast;
    using namespace inference;

    auto e1 = mk(lift<Lambda>(),
                 mk("x", "y", "z"),
                 mk("x", mk("y", "z")))
	    (aalloc());

    auto We1 = W(store, new_types, gamma, pass_value(e1));

    ASSERT_TRUE
	    (shape_check
	     (mk("->",
	         mk("->", "a", "b"), // x
	         mk("->", "c", "a"), // y
	         "c", // z
	         "b")
	      (aalloc()),
	      explicit_unwrap<Ast>(We1.type)))
	    << "\n" << printer::any(We1.type);
}

TEST_F(Inference, test_multi_arg_application)
{
    using namespace make_ast;
    using namespace inference;

    auto e1 = make(lift<Lambda>(),
                   make(sym("x"), sym("y"), sym("z")),
                   make(sym("x"), sym("y"), sym("z")))(aalloc());
    auto We1 = W(store, new_types, gamma, pass_value(e1));

    ASSERT_TRUE
	    (shape_check
	     (mk("->",
	         mk("->", "a", mk("->", "b", "c")),
	         "a",
	         "b",
	         "c")
	      (aalloc()),
	      explicit_unwrap<Ast>(We1.type)))
	    << "\n" << printer::any(We1.type);
}

TEST_F(Inference, test_recursive_fn)
{
    using namespace make_ast;
    using namespace inference;

    auto rec = mk(lift<Define>(),
                  "rec", mk(lift<Lambda>(),
                            mk("x"),
                            mk("rec", "x")))
	    (aalloc());
    auto Wrec = W(store, new_types, gamma, pass_value(rec));

    ASSERT_TRUE
	    (shape_check
	     (mk("->", "a", "b")(aalloc()),
	      explicit_unwrap<Ast>(unwrap<Scheme>(Wrec.type).type)))
	    << "\n" << printer::any(Wrec.type);
}
