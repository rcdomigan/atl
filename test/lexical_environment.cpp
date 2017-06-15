/**
 * @file /home/ryan/programming/atl/test/lexical_environment.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Oct 25, 2015
 *
 * Test the lexical environment, in particular finding free variables.
 */


#include <atl/lexical_environment.hpp>
#include <atl/gc.hpp>

#include <iostream>
#include <gtest/gtest.h>
#include <atl/print.hpp>
#include <atl/helpers.hpp>

using namespace std;
using namespace atl;


struct TestToplevelMap
	: public ::testing::Test
{
	GC gc;
	Slots slots;
	SymbolMap env;
	AssignForms assign_forms;

	Marked<Ast> do_assign(Marked<Ast>&& thing)
	{ return assign_forms(thing); }

	TestToplevelMap()
		: slots(gc),
		  env(gc, slots),
		  assign_forms(gc, env)
	{
		init_types();
		setup_basic_definitions(gc, env);
	}
};


TEST_F(TestToplevelMap, test_parameters)
{
	using namespace make_ast;

	auto expr = do_assign
		(gc
		 (mk
		  (wrap<Lambda>(),
		   mk("a", "b"),
		   mk("a", "b", "c"))));

    {
	    using namespace pattern_match;

	    Symbol const *a, *b;
	    Symbol c;
	    Lambda fn(nullptr);
	    Parameter param_a(-1), param_b(-1);

	    ASSERT_TRUE
		    (match(rest(capture(fn),
		                ast(capture_ptr(a), capture_ptr(b)),
		                ast(capture(param_a), capture(param_b), capture(c))),
		           expr))
		    << printer::print(*expr);

	    ASSERT_EQ(0, param_a.value);
	    ASSERT_EQ(a, &unwrap<Symbol>(fn.value->formals[param_a.value]));

	    ASSERT_EQ(1, param_b.value);
	    ASSERT_EQ(b, &unwrap<Symbol>(fn.value->formals[param_b.value]));
    }
}

TEST_F(TestToplevelMap, test_nested_parameters)
{
	using namespace make_ast;

	auto expr = do_assign
		(gc
		 (mk
		  ("__\\__", mk("a"),
		   mk("__\\__", mk("b"),
		      mk("__\\__", mk("c"), mk("a", "b", "c"))))));

    {
	    using namespace pattern_match;

	    Lambda outer, middle, inner;
	    ClosureParameter a, b;
	    Parameter c;

	    ASSERT_TRUE(match(rest(capture(outer), tag<Ast>::value,
	                           ast(capture(middle), tag<Ast>::value,
	                               ast(capture(inner), tag<Ast>::value,
	                                   ast(capture(a), capture(b), capture(c))))),
	                      expr))
		    << printer::print(*expr) << std::endl;

	    auto print_parameters = [](LambdaMetadata* closure) {
			    return CallbackPrinter([closure](std::ostream& out) -> std::ostream&
		    {
			    for(auto& pair : closure->closure_index_map)
				    {
					    out << pair.first
					        << " "
					        << printer::print(closure->closure[pair.second])
					        << std::endl;
				    }
			    return out;
		    });
	    };

	    ASSERT_EQ(0, outer.value->closure_index_map.size())
		    << print_parameters(outer.value) << std::endl;

	    ASSERT_EQ(1, middle.value->closure_index_map.size())
		    << print_parameters(middle.value) << std::endl;
	    ASSERT_TRUE(middle.value->has_closure_parmeter("a"));

	    ASSERT_EQ(2, inner.value->closure_index_map.size())
		    << print_parameters(middle.value) << std::endl;
	    ASSERT_TRUE(inner.value->has_closure_parmeter("a"));
	    ASSERT_TRUE(inner.value->has_closure_parmeter("b"));
    }
}

TEST_F(TestToplevelMap, test_define_form)
{
	using namespace make_ast;

	auto expr = assign_forms
		(gc
		 (mk("define",
		     "recur",
		     mk("__\\__", mk(), mk("recur")))));

	{
		using namespace pattern_match;
		ASSERT_TRUE(is<GlobalSlot>(env["recur"]));

		auto slot = unwrap<GlobalSlot>(env["recur"]).value;
		auto definition = slots[slot];

		ASSERT_TRUE
			(match(rest(tag<Define>::value, tag<Symbol>::value,
			            ast(tag<Lambda>::value, tag<Ast>::value, tag<Ast>::value)),
			       definition))
			<< "got: " << printer::print(definition) << std::endl;
	}
}

TEST_F(TestToplevelMap, test_getting_definitions)
{
	using namespace make_ast;

	auto expr = do_assign
		(gc(mk("define",
		       "recur",
		       mk("__\\__",
		          mk(),
		          mk("recur")))));

	{
		using namespace pattern_match;
		Symbol *body_recur;

		ASSERT_TRUE(is<GlobalSlot>(env["recur"]));

		auto slot = unwrap<GlobalSlot>(env["recur"]).value;
		auto definition = slots[slot];

		ASSERT_TRUE(match(rest(tag<Define>::value,
		                       tag<Symbol>::value,
		                       ast(tag<Lambda>::value, ast(),
		                           ast(capture_ptr(body_recur)))),
		                   definition))
			<< "got: " << printer::print(definition) << std::endl;

		ASSERT_TRUE(match(rest(tag<Define>::value,
		                       tag<Symbol>::value,
		                       ast(tag<Lambda>::value, ast(),
		                           ast(capture_ptr(body_recur)))),
		                  expr));
	}
}

TEST_F(TestToplevelMap, test_lambda_with_if)
{
	using namespace make_ast;
    auto expr = do_assign
	    (gc
	     (mk
	      (mk(wrap<Lambda>(),
	          mk("a", "b"),
	          mk(wrap<If>(),
	             mk("equal2", "a", "b"),
	             mk("add2", "a", "b"),
	             mk("sub2", "a", "b"))),
	       7, 3)));

    {
	    using namespace pattern_match;
	    ASSERT_TRUE
		    (match(rest(ast(tag<Lambda>::value,
		                    types<Symbol, Symbol>(),
		                    ast(tag<If>::value,
		                        types<Symbol, Parameter, Parameter>(),
		                        types<Symbol, Parameter, Parameter>(),
		                        types<Symbol, Parameter, Parameter>())),
		                wrap<Fixnum>(7), wrap<Fixnum>(3)),
		           expr))
		    << printer::print(*expr) << std::endl;
    }
}

TEST_F(TestToplevelMap, test_closure_params)
{
	using namespace make_ast;

	auto expr = do_assign
		(gc
		 (mk
		  (wrap<Lambda>(),
		   mk("a"),
		   mk(wrap<Lambda>(), mk("b"), mk("a", "b")))));

    {
	    using namespace pattern_match;

	    ClosureParameter param_a(-1);
	    Parameter param_b(-1);

	    ASSERT_TRUE
		    (match(rest(tag<Lambda>::value,
		                tag<Ast>::value,
		                ast(tag<Lambda>::value,
		                    tag<Ast>::value,
		                    ast(capture(param_a), capture(param_b)))),
		           expr))
		    << printer::print(*expr);

	    ASSERT_EQ(0, param_a.value);
	    ASSERT_EQ(0, param_b.value);
    }
}

/* A defined sym can't generally have it's value swapped in for the
   symbol (at least not yet).
 */
TEST_F(TestToplevelMap, test_define_leaves_sym)
{
	using namespace make_ast;

	do_assign(gc
	          (mk
	           (wrap<Define>(),
	            "id",
	            mk(wrap<Lambda>(), mk("a"), mk("a")))));

	auto expr = do_assign(gc(mk(wrap<Lambda>(), mk("b"), mk("id", "b"))));

    {
	    using namespace pattern_match;

	    Symbol *id;
	    Parameter param_b(-1);

	    ASSERT_TRUE
		    (match(rest(tag<Lambda>::value,
		                tag<Ast>::value,
		                ast(capture_ptr(id), capture(param_b))),
		           expr))
		    << printer::print(*expr);

	    ASSERT_EQ(0, param_b.value);
    }
}



TEST_F(TestToplevelMap, test_value_walker)
{
	using namespace make_ast;

	auto expr = gc(mk
	               ("define",
	                "id",
	                mk("__\\__", mk("a"), mk("a"))));

	{
		using namespace pattern_match;

		ASSERT_TRUE
			(match(rest(tag<Define>::value,
			            tag<Symbol>::value,
			            ast(tag<Lambda>::value, tag<Ast>::value, tag<Ast>::value)),
			       walk_values(env, atl::subex(*expr))))
			<< printer::print(*expr);
	}
}
