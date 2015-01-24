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
        typedef PCode::iterator iterator;

        lexical::Map *_env;     // pushing a scope mutates where this points
        GC& gc;
        PCode::value_type* output;
        AssembleVM wrapped;

        Any value_or_undef(Symbol &sym) {
            auto def = _env->find(sym.name);

            if(def == _env->end()) {
                auto udef = gc.amake<Undefined>();
                _env->define(sym.name, udef);
                return udef;
            }
            return def->second;
        }

        Compile(Environment& env) : gc(env.gc) {
            output = gc.alloc_pcode();
            wrapped = AssembleVM(output);
            _env = &env.toplevel;
        }

        enum class Form {done, // The form has been evaluated, _compile can return
                function};     // The form was a function, _compile should call

        enum {applicable=0, pad_to=1, fform=2};

        struct SkipBlock {
            iterator _skip_to;
            AssembleVM& code;
            SkipBlock(AssembleVM& code_) : code(code_) {
                code.pointer(nullptr);
                _skip_to = code.last(); // skip over the lambda body when running the code
                code.jump();
            }

            ~SkipBlock() {
                *_skip_to = reinterpret_cast<PCode::value_type>(code.end());
            }
        };


        // What needs to be handled specially in the car of an Ast?
        std::tuple<Any, size_t, Form>
        form(Ast ast, AssembleVM& code, bool tail) {
            using namespace std;
            typedef PCode::value_type value_type;

            auto head = ast[0];
        setup_form:
            switch(head._tag) {
            case tag<Symbol>::value:
                head = value_or_undef(unwrap<Symbol>(head));
                goto setup_form;

            case tag<Lambda>::value: {
                auto formals = flat_iterator::range(ast[1]);
                auto size = flat_iterator::size(formals);

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
                iterator entry_point;
                {
                    SkipBlock my_def(code);

                    entry_point = code.end();
                    auto result = _compile(ast[2], code, true);

                    // Might need to allocate for more params to make the tail call happy
                    size = max(size, get<pad_to>(result));
                    code.return_(size);
                }

                return std::make_tuple(gc.amake<Procedure>(entry_point, size),
                                       0,
                                       Form::done);
            }
            case tag<If>::value: {
                auto will_jump = [&]() {
                    code.pointer(nullptr);
                    return code.last();
                };

                auto alt_address = will_jump();
                _compile(ast[1],  code, false); // get the predicate
                code.if_();

                // consiquent
                auto consiquent = _compile(ast[2], code, tail);

                auto after_alt = will_jump();
                code.jump();

                // alternate
                *alt_address = reinterpret_cast<value_type>(code.end());
                auto alternate = _compile(ast[3], code, tail);

                *after_alt = reinterpret_cast<value_type>(code.end());

                auto padding = 0;
                if(tail)
                    padding = max(get<pad_to>(consiquent),
                                  get<pad_to>(alternate));

                return std::make_tuple(aimm<Null>(), padding, Form::done);
            }
            case tag<Ast>::value: {
                auto result = _compile(head, code, false);
                return std::make_tuple(get<applicable>(result),
                                       0,
                                       Form::function);
            }
            case tag<Define>::value: {
                // define a symbol
                auto sym = unwrap<Symbol>(ast[1]);
                Any value = ast[2];

                iterator entry_point;

                Any def;
                auto found = _env->_local.find(sym.name);
                if(found == _env->_local.end())
                    def = gc.amake<Undefined>();

                else if(is<Undefined>(found->second))
                    def = found->second;

                else
                    throw WrongTypeError("A symbol cannot be defined twice in the same scope");

                _env->define(sym.name, def);

                while(is<Symbol>(value))
                    value = value_or_undef(unwrap<Symbol>(value));

                // Lambda is a little bit of a special case.
                if(is<Ast>(value)) {
                    head = unwrap<Ast>(value)[0];
                    if(is<Symbol>(head))
                        head = value_or_undef(unwrap<Symbol>(head));

                    if(is<Lambda>(head))
                        value = get<applicable>(_compile(value, code, true));
                }

                if(is<Procedure>(value)) {
                    entry_point = unwrap<Procedure>(value).body;
                } else {
                    // treat non-procedure expression as thunks, I'll
                    // write out definitions and use the procedure
                    // call mechanism to actually get a value.
                    {
                        SkipBlock inlined(code);

                        entry_point = code.end();
                        _compile(ast[2], code, true);

                        code.return_(0);
                    }
                    value = gc.amake<Procedure>(entry_point, 0);
                }

                for(auto& vv : unwrap<Undefined>(def).backtrack)
                    *vv = reinterpret_cast<value_type>(entry_point);

                _env->define(sym.name, value);
                return std::make_tuple(aimm<Null>(), 0, Form::done);
            }
            default:
                return std::make_tuple(head, 0, Form::function);
            }
        }


        // :returns: thing that can be applied and the size of its tail call
        std::tuple<Any, size_t>
        _compile(Any input, AssembleVM& code, bool tail) {
            using namespace std;
        compile_value:
            switch(input._tag) {
            case tag<Ast>::value: {
                // todo... does the IF form needs to move in here.
                auto ast = unwrap<Ast>(input);
                auto form = this->form(ast, code, tail);

                switch(get<fform>(form)) {
                case Form::done:
                    return make_tuple(get<applicable>(form), get<pad_to>(form));

                case Form::function: {
                    auto fn = get<applicable>(form);
                    auto rest = slice(ast, 1);

                    auto is_procedure = is<Procedure>(fn);
                    size_t padding = 0;
                    size_t tail_size = 0;

                    if(is_procedure) {
                        // The function called in tail position my
                        // tail call a function taking more parameters.
                        tail_size = max(unwrap<Procedure>(fn).tail_params,
                                        rest.size());
                        padding = tail_size - rest.size();
                    }

                    // pad out to leave space for a tail call
                    for(size_t i=0; i < padding; ++i) code.constant(-1);

                    for(auto& vv : rest)
                        _compile(vv, code, false);

                    if(is_procedure) {
                        if(tail)
                            code.tail_call(tail_size,
                                           unwrap<Procedure>(fn).body);
                        else
                            code.call_procedure(unwrap<Procedure>(fn).body);
                    }
                    else if(is<Undefined>(fn)) {
                        // TODO: I'm just assuming this is the
                        // definition of the current function.
                        // co-recursive functions with different max
                        // tail size than their aritys will totes
                        // break.
                        code.constant(rest.size())
                            .pointer(nullptr);
                        unwrap<Undefined>(fn).backtrack.push_back(code.last());

                        code.tail_call();
                    }
                    else
                        _compile(get<applicable>(form), code, tail);

                    return make_tuple(get<applicable>(form), tail_size);
                }}
                throw "should be unreachable";
            }
            case tag<Symbol>::value:
                input = value_or_undef(unwrap<Symbol>(input));
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
            case tag<Undefined>::value: {
                code.pointer(nullptr);
                unwrap<Undefined>(input).backtrack.push_back(code.last());

                // Assume a thunk will get patched in later.
                code.call_procedure();
                break;
            }
            case tag<Fixnum>::value:
                code.constant(value<Fixnum>(input));
                break;
            case tag<Bool>::value:
                code.constant(value<Bool>(input));
                break;
            case tag<Pointer>::value:
                code.pointer(value<Pointer>(input));
                break;
            case tag<CxxFn2>::value: {
                code.foreign_2(value<CxxFn2>(input));
                break;
            }
            case tag<Procedure>::value: {
                code.call_procedure(unwrap<Procedure>(input).body);
                break;
            }

                // case tag<DefineMacro>::value:
                // 	*stack = ast[2];-
                // 	argument_value(stack);
                // 	stack[0]._tag = tag<Macro>::value;
                // 	goto backpatch_definition;
                // backpatch_definition: {
                // 	    Undefined *u = reinterpret_cast<Undefined*>(ast[1]._value);
                // 	    for( auto i : u->_backtrack ) *i = *stack;
                // 	    return;
                // 	}
                // case tag<Macro>::value: {
                // 	auto &fn = unwrap<Procedurey>(*stack);
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
            return make_tuple(aimm<Null>(), (size_t)0);
        }

        PCode any(Any ast) {
            _compile(ast, wrapped, false);
            wrapped.finish();

            return PCode(&output[0]);
        }
    };
}


#endif
