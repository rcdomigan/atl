#ifndef ATL_PRIMITIVE_CALLABLE_HH
#define ATL_PRIMITIVE_CALLABLE_HH
/**
 * @file /home/ryan/programming/atl/primitive_callable.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Dec 12, 2013
 *
 * Playing around with integrating type info with the function primitives.
 */
#include <fstream>

#include "./type.hpp"
#include "./parser.hpp"
#include "./encode.hpp"
#include "./evaluator.hpp"
#include "./helpers.hpp"

#include "./infer.hpp"
#include "ffi.hpp"

namespace atl {
    Any nullP(Any a) { return is<Null>(a) ? atl_true() : atl_false();  }

    namespace primitives {
	struct MakeSequence {
	    GC &_gc;

	    MakeSequence(GC &gc) : _gc(gc) {}

	    Any operator()(const Any *begin, const Any *end) {
		auto stack = _gc.dynamic_vector((end - begin) + 2);
		auto seq = stack->push_seq<Data>();

		std::copy(begin, end, stack->back_insert_iterator());
		seq->end_at(stack->end());
		return wrap(*seq);
	    }
	};
    }

    void setup_typeclasses(Environment& env) {
	env.type_class_map[typeclass_tag<Applicable>::value] = new Applicable::Map();
	env.type_class_map[typeclass_tag<Sequence>::value] = new Sequence::Map();
    }

    void export_recursive_defs(GC *gc, Environment *env, EncodeAST *en, ParseString *parser, EvaluateAST *eval) {
	en->define("\\", aimm<Lambda>());
	en->define("quote", aimm<Quote>());
	en->define("if", aimm<If>());
	en->define("#f", atl_false());
	en->define("#t", atl_true());

	en->define("define-value", aimm<Define>());
	en->define("define-macro", aimm<DefineMacro>());

	en->wrap_fn<Any (string)>
	    ("import",
	     [parser,en,eval](string s) {
		std::ifstream in(s);
		Any last = nil<Null>::value();
		Any parsed;
		while(in) {
		    parsed = parser->stream(in);
		    if(is<Null>(parsed))
			return last;
		    last = eval->any
			(en->any
			 (parsed));
		}
		return last;
	    });

	en->wrap_fn<Any (Any)>("infer", infer);

	/***********************************************************/
	/**     _         _ _   _                     _   _       **/
	/**    / \   _ __(_) |_| |__  _ __ ___   __ _| |_(_) ___  **/
	/**   / _ \ | '__| | __| '_ \| '_ ` _ \ / _` | __| |/ __| **/
	/**  / ___ \| |  | | |_| | | | | | | | | (_| | |_| | (__  **/
	/** /_/   \_\_|  |_|\__|_| |_|_| |_| |_|\__,_|\__|_|\___| **/
	/***********************************************************/
	en->wrap_fn<long (long, long)>("bin-add", [=](long a, long b) {
		cout << "args:" << a << " and " << b << endl;
		return a + b;
	    });
	en->wrap_fn<long (long, long)>("bin-sub", [](long a, long b) {
		cout << "result: " << a - b << endl;
		return a - b; } );

	en->wrap_fn<bool (long, long)>("=",
				       [](long a, long b) {
					   return a == b ? true : false;
				       });
#define ATL_NUM_COMPAIR(op)			\
	en->wrap_fn<bool (long, long)>		\
	    ( # op,				\
	     [](long a, long b) {		\
		return a op b ? true : false;	\
	    });

	ATL_NUM_COMPAIR(=)
	ATL_NUM_COMPAIR(<)
	ATL_NUM_COMPAIR(>)
	ATL_NUM_COMPAIR(<=)
	ATL_NUM_COMPAIR(>=)

#undef ATL_NUM_COMPAIR

	en->wrap_fn<Any (Any)>("print", [](Any a) { cout << printer::any(a); return a;} );

	/////////////////////////////////////////////////////////////////////
	//  ___       _                                 _   _              //
	// |_ _|_ __ | |_ _ __ ___  ___ _ __   ___  ___| |_(_) ___  _ __   //
	//  | || '_ \| __| '__/ _ \/ __| '_ \ / _ \/ __| __| |/ _ \| '_ \  //
	//  | || | | | |_| | | (_) \__ \ |_) |  __/ (__| |_| | (_) | | | | //
	// |___|_| |_|\__|_|  \___/|___/ .__/ \___|\___|\__|_|\___/|_| |_| //
	//                             |_|                                 //
	/////////////////////////////////////////////////////////////////////
	// introspection

	en->wrap_fn<Any (Any cc)>("print-procedure", [](Any a) {
		auto& cc = unwrap<Procedure>(a);
		auto formals = cc.parameters();

		cout << "[" << printer::any(wrap(&formals[0]));
		for(auto vv : slice(formals, 1))
		    cout << ", " << printer::any(wrap(&vv));
		cout << "]";

		cout << printer::any(cc._body);
		return a;
	    });

	/***************************/
	/**  _     _     _        **/
	/** | |   (_)___| |_ ___  **/
	/** | |   | / __| __/ __| **/
	/** | |___| \__ \ |_\__ \ **/
	/** |_____|_|___/\__|___/ **/
	/***************************/

	auto list_factory = primitives::MakeSequence(*gc);
	en->define("list" ,
		   gc->amake<PrimitiveRecursive*>
		   (PrimitiveRecursive::Fn(list_factory), "list"));

	// TODO: this doesn't make sense in a strongly typed language.
	// To bad this language isn't strongly typed yet.
	en->wrap_fn<bool(Any)>("list?", [](Any vv) {
		return (vv._tag == tag<Data>::value)
		    || (vv._tag == tag<Ast>::value)
		    || (vv._tag == tag<CxxArray>::value);
	    });

	en->wrap_fn<bool(Any)>("empty?", [](Any vv) {
		switch(vv._tag) {
		case tag<Data>::value:
		    return unwrap<Data>(vv).empty();
		case tag<Ast>::value:
		    return unwrap<Ast>(vv).empty();
		case tag<CxxArray>::value:
		    return unwrap<CxxArray>(vv).empty();
		}
		return true;
	    });

	en->wrap_fn<Any (Data, Data)>("append", [gc](Data aa, Data bb) {
		auto output = gc->dynamic_vector();
		auto data = output->push_seq<Data>();

		std::copy(aa.begin(), aa.end(),
			  output->back_insert_iterator());
		std::copy(bb.begin(), bb.end(),
			  output->back_insert_iterator());

		data->end_at(output->end());
		return wrap(data);
	    });

	en->wrap_fn<Any (Data, long)>
	    ("take",
	     [gc](Data input, long num) {
		auto output = gc->dynamic_vector();
		auto data = output->push_seq<Data>();
		std::copy(input.begin(), input.begin() + num,
			  output->back_insert_iterator());
		data->end_at(output->end());
		return wrap(data);
	    });

	/////////// SLICE //////////
	// Both Ast and Data have the same memory layout, so the same
	// `slice` will work on them
	auto slice_vec_or_ast = Sequence::Slice();
	en->wrap_fn<Any (Any, long)>
	    ("slice", [gc](Any seq, long nn) {
		using namespace ast_iterator;
		return wrap(gc->make<Slice*>(const_begin(seq) + nn,
					     const_end(seq)));
	    });

	/////////// TO AST //////////
	en->wrap_fn<Ast (Any)>
	    ("list->ast",
	     [&](Any seq) -> Ast {
		return deep_copy::to<Ast>(flat_iterator::range(seq), *gc);
	    });

	////////// cons ////////
	// TODO: make this a monad thing and pass the constructred
	// list into a tail call.
	en->wrap_fn<Data (Any, Any)>
	    ("cons",
	     [gc](Any vv, Any seq) -> Data {
		using namespace ast_iterator;
		auto output = gc->dynamic_vector();
		auto vec = output->push_seq<Data>();

		output->push_back(vv);
		std::copy(const_begin(seq), const_end(seq),
			  std::back_insert_iterator<GC::DynamicVector>(*output));

		vec->end_at(output->end());
		return *vec;
	    });

	auto apply_sequence = [](const Any *args, const Any *_) -> Any {
	    return *(ast_iterator::const_begin(args[0])
		     + unwrap<long>(args[1]));
	};
	(Applicable(*env)).set<Ast>(apply_sequence);
	(Applicable(*env)).set<Data>(apply_sequence);
	(Applicable(*env)).set<Slice>(apply_sequence);
	(Applicable(*env)).set<CxxArray>(apply_sequence);


	///////////////////////////////////////////
        //  __  __                       _       //
        // |  \/  | ___  _ __   __ _  __| |___   //
        // | |\/| |/ _ \| '_ \ / _` |/ _` / __|  //
        // | |  | | (_) | | | | (_| | (_| \__ \  //
        // |_|  |_|\___/|_| |_|\__,_|\__,_|___/  //
        ///////////////////////////////////////////
	// Monads

	// en->wrap_fn<Any (Any, Any)>
	//     (">>=",
	//      [env](Any fn, Any monad) {
	//     });

	// // `return` in Haskell
	// en->wrap_fn<Any (Any)>
	//     ("unit",
	//      [env](Any vv) {
	// 	return 
	//     });
    }

    void setup_interpreter(GC *gc, Environment *env, EncodeAST *en, ParseString *parser, EvaluateAST *eval) {
	setup_typeclasses(*env);
	export_recursive_defs(gc, env, en, parser, eval);
    }
}

#endif
