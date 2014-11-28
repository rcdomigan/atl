#ifndef ATL_BYTE_COMPILE_HPP
#define ATL_BYTE_COMPILE_HPP
/**
 * @file /home/ryan/programming/atl/compile.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Nov 22, 2014
 *
 * A boot-strapping compiler.
 */

#include "./exception.hpp"
#include "./tiny_vm.hpp"
#include "./type.hpp"
#include "./environment.hpp"

namespace atl {

    namespace compile_helpers {
        struct IncHopRAII {
            lexical::Map *env;

            template<class Fn>
            static void _for_every_parameter(Fn fn, lexical::Map *env) {
                auto mm = env;
                while(mm) {
                    for(auto& aa : mm->_local) {
                        if(is<Parameter>(aa.second))
                            fn(unwrap<Parameter>(aa.second));
                    }
                    mm = mm->_prev; }}

            IncHopRAII(lexical::Map *env_) : env(env_){
                _for_every_parameter([](Parameter& vv) { ++vv.hops; }, env);
            }

            ~IncHopRAII() {
                _for_every_parameter([](Parameter& vv) { --vv.hops; }, env);
            }
        };
    }

    struct Compile {
        lexical::Map *_env;     // pushing a scope mutates where this points
        GC& gc;

        Any& scoped_value(Symbol& sym) { return _env->value(sym); }

        Compile(Environment& env) : gc(env.gc) {
            _env = &env.toplevel;
        }

        enum class Form {done, function, applicable};

        // What needs to be handled specially in the car of an Ast?
        std::pair<Any, Form> form(Ast ast, AssembleVM& code) {
            using namespace std;
            typedef PCode::value_type value_type;

            auto head = ast[0];
        setup_form:
            switch(head._tag) {
            case tag<Symbol>::value:
                head = scoped_value(unwrap<Symbol>(head));
                goto setup_form;

            case tag<Lambda>::value: {
                auto size = flat_iterator::size(ast[1]);

                compile_helpers::IncHopRAII inc_hops(_env);
                lexical::MapRAII local(&_env);

                for(auto ff : zip(flat_iterator::range(ast[1]),
                                  CountingRange()))
                    local.map.define(unwrap<Symbol>(*get<0>(ff)).name,
                                     // The `offset` should go high to low
                                     gc.amake<Parameter>(size - *get<1>(ff), 0));

                // todo: For now I'm just putting the lambda body
                // inline with the rest of the code.  I could save
                // some jumps if the lambda bodies where gathered in
                // one place in the closure.
                code.pointer(nullptr);
                auto skip_to = code.last(); // skip over the lambda body when running the code
                code.jump();

                auto entry_point = code.end();
                _compile(ast[2], code);

                *skip_to = reinterpret_cast<PCode::value_type>(code.end());
                return std::make_pair(wrap(ProcedureLocation(entry_point)),
                                      Form::done);
            }
            case tag<If>::value: {
                auto alt_address = code.end();
                code.constant(0);         // leave a space for the `alternate` address
                _compile(ast[1],  code);  // get the predicate
                code.if_();

                // consiquent
                _compile(ast[2], code);
                auto after_alt = code.end();
                code.constant(0).jump();

                // alternate
                *alt_address = reinterpret_cast<value_type>(code.end());
                code.pointer(code.end());
                _compile(ast[3], code);

                *after_alt = reinterpret_cast<value_type>(code.end());

                return std::make_pair(aimm<Null>(), Form::done);
            }
            case tag<Ast>::value:
                return std::make_pair(_compile(head, code),
                                      Form::function);
            default:
                return std::make_pair(head,
                                      Form::function);
            }
        }

        // :returns: thing that can be applied
        Any _compile(Any input, AssembleVM& code) {
        compile_value:
            switch(input._tag) {
            case tag<Ast>::value: {
                // todo... does the IF form needs to move in here.
                auto ast = unwrap<Ast>(input);
                auto form = this->form(ast, code);

                switch(form.second) {
                case Form::done:
                    return form.first;
                case Form::applicable:
                    return form.first;
                case Form::function:
                    cout << "function args:" << printer::range(slice(ast,1)) << endl;
                    for(auto& vv : slice(ast, 1))
                        _compile(vv, code);
                    _compile(form.first, code);
                }
                return aimm<Null>();
            }
            case tag<Symbol>::value:
                input = scoped_value(unwrap<Symbol>(input));
                goto compile_value;
            case tag<Parameter>::value: {
                auto param = unwrap<Parameter>(input);
                if(param.hops) { // -> non local
                    code.nested_argument(param.offset, param.hops);
                } else {
                    code.argument(param.offset);
                }
                break;
            }
            case tag<Fixnum>::value:
                code.constant(unwrap<Fixnum>(input).value);
                break;
            case tag<Bool>::value:
                code.constant(unwrap<Bool>(input).value);
                break;
            case tag<CxxFn2>::value: {
                code.foreign_2(unwrap<CxxFn2>(input).value);
                break;
            }
            case tag<ProcedureLocation>::value:
                code.procedure(unwrap<ProcedureLocation>(input).value);
                break;

                // case tag<DefineMacro>::value:
                // 	*stack = ast[2];-
                // 	argument_value(stack);
                // 	stack[0]._tag = tag<Macro>::value;
                // 	goto backpatch_definition;
                // case tag<Define>::value:
                // 	*stack = ast[2];
                // 	argument_value(stack);
                // backpatch_definition: {
                // 	    Undefined *u = reinterpret_cast<Undefined*>(ast[1]._value);
                // 	    for( auto i : u->_backtrack ) *i = *stack;
                // 	    return;
                // 	}
                // case tag<Macro>::value: {
                // 	auto &fn = unwrap<Procedure>(*stack);
                // 	*stack = wrap(deep_copy::to<Data>(flat_iterator::range(slice(ast, 1)),
                // 					  _env.gc));
                // 	do_apply(fn, stack + 1);
                // 	return;
                // }
                // case tag<PrimitiveRecursive>::value: {
                // 	auto end = bind_args(stack + 1, slice(ast, 1));
                // 	stack[0] = unwrap<PrimitiveRecursive>(*stack).call(stack + 1, end);
                // 	return;
                // }
                // case tag<Quote>::value:
                // 	*stack = ast[1];
                // 	return;
            default: {
                throw std::string("Illegal syntax or something.");
            }}
            return aimm<Null>();
        }

        PCode any(Any ast) {
            auto output = gc.alloc_pcode();
            auto wrapped = AssembleVM(output);

            _compile(ast, wrapped);
            wrapped.finish();
            wrapped.print();

            return PCode(&output[0]);
        }
    };
}


#endif
