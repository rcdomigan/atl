/**
 * @file /home/ryan/programming/atl/test/gc.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jul 25, 2015
 */

#include <iostream>
#include <vector>

#include <gc.hpp>

#include <gtest/gtest.h>

TEST(TestGC, test_marked_move_in_order)
{
	// testing the _move method
	using namespace atl;

	Marked<Any> *top = nullptr;
	Marked<Any> aa(top, wrap<Null>()),
		bb(top, wrap<Null>());

Ast make_expr(AstAllocator backer)
{
	// testing the _move method
	using namespace atl;

	Marked<Any> *top = nullptr;
	Marked<Any> aa(top, wrap<Null>()),
		bb(top, wrap<Null>());

	// so should be top->bb->aa->0
	ASSERT_EQ(top, &bb);
	ASSERT_EQ(bb._up, &aa);

	Marked<Any> cc(std::move(aa));
	// now should be top->cc->bb->0
	ASSERT_EQ(top, &cc);
	ASSERT_EQ(cc._up, &bb);
	ASSERT_EQ(bb._up, nullptr);
}

size_t marked_count(atl::Marked<atl::Any>* itr)
{
	size_t count = 0;
	while(itr)
		{
			itr = itr->_up;
			++count;
		}
	return count;
}

TEST(TestGC, test_marked_equal)
{
	// testing the _move method
	using namespace atl;

	Marked<Any> *top = nullptr;

	Marked<Any> aa(top, wrap<Null>()),
		bb(top, wrap<Null>());

	ASSERT_EQ(2, marked_count(top));

	Marked<Any> cc(top, wrap<Null>());

	ASSERT_EQ(3, marked_count(top));

	cc = std::move(aa);

	ASSERT_EQ(2, marked_count(top));
	ASSERT_EQ(&bb, cc._up);
	ASSERT_EQ(&cc, top);

	bb = Marked<Any>(top, wrap<Null>());

	ASSERT_EQ(2, marked_count(top));
	ASSERT_EQ(&cc, bb._up);
	ASSERT_EQ(&bb, top);
}

TEST(TestGC, test_mark_atoms)
{
	Arena store;
	Ast ast = store(make_expr);

	ASSERT_EQ(2, ast.size());

	ASSERT_EQ(tag<Fixnum>::value, ast[0]._tag);
	ASSERT_EQ(1, reinterpret_cast<Fixnum::value_type>(ast[0].value));

	ASSERT_EQ(tag<Fixnum>::value, ast[1]._tag);
	ASSERT_EQ(2, reinterpret_cast<Fixnum::value_type>(ast[1].value));
}

TEST(TestGC, test_mark_LambdaMetadata)
{
	using namespace atl;

	GC gc;
	init_types();

	auto make_formals = [](GC& gc)
		{
			auto formals = gc.ast_builder();
			{
				NestAst nest(formals);
				formals.push_back(Any(tag<Symbol>::value, gc.raw_make<Symbol>("a")));
				formals.push_back(Any(tag<Symbol>::value, gc.raw_make<Symbol>("b")));
			}
			return formals.root();
		};

	{
		auto metadata = gc.make<LambdaMetadata>
			(make_formals(gc), wrap<Null>());

		{
			auto closure = gc.ast_builder();
			{
				NestAst nest(closure);
				closure.push_back(Any(tag<Symbol>::value, gc.raw_make<Symbol>("a")));
				closure.push_back(Any(tag<Symbol>::value, gc.raw_make<Symbol>("b")));
			}
			metadata->has_closure_values = true;
			metadata->closure_values = closure.root();
		}

		gc.gc();
		ASSERT_EQ(6, gc._ast_pool.size());
		ASSERT_EQ(2, metadata->closure_values.size());
	}

	gc.gc();
	ASSERT_EQ(0, gc.cells_allocated());
}

TEST(TestGC, test_moving_builders)
{
	using namespace atl;

	GC gc;
	init_types();

	{
		auto ast = gc.ast_builder();
		{
			NestAst nest(ast);
			ast.push_back(Any(tag<Fixnum>::value, reinterpret_cast<void*>(1)));
			ast.push_back(Any(tag<Fixnum>::value, reinterpret_cast<void*>(2)));
		}
		auto initial_backer = *ast.backer;

		gc.gc();

		ASSERT_NE(initial_backer._begin, ast.backer->_begin);
		ASSERT_NE(initial_backer._itr, ast.backer->_itr);
		ASSERT_NE(initial_backer._end, ast.backer->_end);

		ASSERT_EQ(3, ast.backer->size());

		// This assumes DEFAULT_INIT_SIZE is greater than 3
		auto default_init_size = GC::AstPool::DEFAULT_INITIAL_SIZE;
		ASSERT_EQ(default_init_size, gc.cells_allocated());
	}

	gc.gc();
	ASSERT_EQ(0, gc.cells_allocated());
}

TEST(TestGC, test_mark_assigning)
{
	// Check that an Ast allocated by an Arena an a GC are the same.
	GC gc;
	init_types();

	Marked<Any> str_2 = gc.marked(wrap<Null>()),
		str_3 = gc.marked(wrap<Null>());

	{
		auto str_0 = gc.make<String>("str_0"),
			str_1 = gc.make<String>("str_1");

		str_2 = str_1.wrap();
		str_3 = gc.make<String>("str_3");

		ASSERT_EQ(3, gc.cells_allocated());
		gc.gc();
		ASSERT_EQ(3, gc.cells_allocated());
	}

	auto ast0 = gc(make_expr);

	auto ast1 = arena(make_expr);

	auto compose = [](AstBuilder& ast)
		{
			NestAst nest(ast);
			ast.push_back(wrap<Fixnum>(1));
			ast.push_back(wrap<Fixnum>(2));
		};

	auto ast = gc(compose);

	for(auto zz : zip(ast0, ast1))
		ASSERT_EQ((*get<0>(zz)), (*get<1>(zz)));
}

TEST(TestGC, test_moving_ast)
{
	using namespace atl;
	GC gc;

	auto compose = [](AstBuilder& ast) -> void
		{
			NestAst nest(ast);
			ast.push_back(wrap<Fixnum>(1));
			ast.push_back(wrap<Fixnum>(2));
		};

	auto ast = gc(compose);
	auto start_pntr = ast.any.value;

	gc.gc();

	ASSERT_NE(start_pntr, ast.any.value);
}

TEST(TestGC, test_moving_any_wrapped_ast)
{
	using namespace atl;
	GC gc;

	auto compose = [](AstBuilder& ast) -> void
		{
			NestAst nest(ast);
			ast.push_back(wrap<Fixnum>(1));
			ast.push_back(wrap<Fixnum>(2));
		};

	auto ast = gc.marked(gc.raw_ast(compose));

	// Make sure a second reference to the Ast also will get updated
	// (via the MovedAstData which gets left behind)
	auto ast_b = gc.marked(ast.any);

	auto start_pntr = ast.any.value;

	ASSERT_EQ(start_pntr, ast_b.any.value);

	gc._mark();
	ASSERT_NE(start_pntr, ast.any.value);
	ASSERT_NE(start_pntr, ast_b.any.value);
	ASSERT_EQ(tag<MovedAstData>::value,
	          reinterpret_cast<Any*>(start_pntr)->_tag);

	gc._sweep();
	ASSERT_EQ(tag<AstData>::value,
	          reinterpret_cast<Any*>(ast.any.value)->_tag);
	ASSERT_EQ(tag<AstData>::value,
	          reinterpret_cast<Any*>(ast_b.any.value)->_tag);

	ASSERT_EQ(ast.any.value, ast_b.any.value);
}

// Does the GC update an Ast which is cast to Any
TEST(TestGC, test_moving_wrapped_ast)
{
	using namespace atl;
	GC gc;

	Ast as_ast;

	{
		auto builder = gc.ast_builder();
		NestAst nest(builder);
		builder.push_back(wrap<Fixnum>(1));
		builder.push_back(wrap<Fixnum>(2));

		as_ast = builder.root();
	}

	Marked<Any> ast = gc.marked(as_ast);

	auto start_pntr = ast.any.value;
	gc.gc();

	ASSERT_NE(start_pntr, ast.any.value);
}
