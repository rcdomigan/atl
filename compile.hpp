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

namespace atl
{

    namespace compile_helpers
    {
        struct IncHopRAII
        {
            lexical::Map *env;

            template<class Fn>
            static void _for_every_parameter(Fn fn, lexical::Map *env)
            {
                auto mm = env;
                while(mm) {
                    for(auto& aa : mm->_local) {
                        if(is<Parameter>(aa.second.value))
                            fn(unwrap<Parameter>(aa.second.value));
                    }
                    mm = mm->_prev; }
            }

            IncHopRAII(lexical::Map *env_) : env(env_)
            { _for_every_parameter([](Parameter& vv) { ++vv.hops; }, env); }

            ~IncHopRAII()
            { _for_every_parameter([](Parameter& vv) { --vv.hops; }, env); }
        };
    }

    struct Compile;

    // Compiler information + a VM.
    struct Eval
    {
        Compile* compile;
        TinyVM vm;

        Eval(Compile* outer)
            : compile(outer)
        {}

        Any any(Any input);
    };


    struct Compile
    {
        typedef PCode::iterator iterator;

        lexical::Map *_env;     // pushing a scope mutates where this points
        GC& gc;
        PCode::value_type* output;

        AssembleVM wrapped;

        Compile(Environment& env)
            : _env(&env.toplevel), gc(env.gc),  output(gc.alloc_pcode()), wrapped(AssembleVM(output))
        {}

        struct SkipBlock
        {
            iterator _skip_to;
            AssembleVM& code;
            SkipBlock(AssembleVM& code_) : code(code_)
            {
                code.pointer(nullptr);
                _skip_to = code.last(); // skip over the lambda body when running the code
                code.jump();
            }

            ~SkipBlock()
            { *_skip_to = reinterpret_cast<PCode::value_type>(code.end()); }
        };


        struct Context
        {
            bool tail;
            bool definition;
            Ast expression;
            std::vector<tag_t> *observed_types;

            Context& set_tail(bool in)
            {
                tail = in;
                return *this;
            }
            Context& set_define(bool in)
            {
                definition = in;
                return *this;
            }

            Context(bool t, bool d, Ast ast)
                : tail(t), definition(d), expression(ast), observed_types(nullptr)
            {}

            Context(bool t, bool d, Ast ast, std::vector<tag_t>* ot)
                : tail(t), definition(d), expression(ast), observed_types(ot)
            {}

        };

        // Get a value if available, otherwise return undefined.
        Any value_or_undef(Symbol &sym)
        {
            auto def = _env->find(sym.name);

            if(def == _env->end()) {
                auto udef = gc.amake<Undefined>(nullptr);
                _env->define(sym.name, udef);
                return udef;
            }
            return def->second.value;
        }

        // done     : The form has been evaluated, _compile can return
        // function : The form was a function, _compile should call
        enum class FormTag
        { done, function };

        struct _Form
        {
            // result from 'form'
            Any applicable;
            size_t pad_to;
            FormTag form_tag;
            tag_t result_tag;

            _Form(Any applicable_,
                  size_t pad_to_,
                  FormTag form_tag_,
                  tag_t result_tag_)

                : applicable(applicable_),
                  pad_to(pad_to_),
                  form_tag(form_tag_),
                  result_tag(result_tag_)
            {}
        };


        struct SavedExcursion
        {
            Compile *compile;
            PCode::iterator main_entry, end;

            void enter_end()
            { compile->wrapped.main_entry_point = end; }

            SavedExcursion(Compile* comp)
                : compile(comp), main_entry(comp->wrapped.main_entry_point), end(comp->wrapped._end)
            {}

            ~SavedExcursion()
            {
                compile->wrapped._end = end;
                compile->wrapped.main_entry_point = main_entry;
            }
        };

        SavedExcursion save_excursion()
        {
            return SavedExcursion(this);
        }

        // What needs to be handled specially in the car of an Ast?
        _Form form(Ast ast, AssembleVM& code, Context context)
        {
            using namespace std;
            typedef PCode::value_type value_type;
            typedef PCode::iterator iterator;

            auto head = ast[0];
            auto result = [&head](Any aa, size_t ss, FormTag ff)
                { return _Form(aa, ss, ff, head._tag); };

            auto compile_tail = [&](Any input)
                {
                    return _compile(input, code,
                                    Context(context.tail, false, ast));
                };

        setup_form:
            switch(head._tag)
                {
                case tag<Symbol>::value:
                    head = value_or_undef(unwrap<Symbol>(head));
                    goto setup_form;

                case tag<Lambda>::value:
                    {
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

                        auto compile_body = [&]()
                            {
                                entry_point = code.end();
                                auto comp_val = _compile(ast[2], code, context);

                                // Might need to allocate for more params to make the tail call happy
                                size = max(size, comp_val.pad_to);
                                code.return_(size);
                                return comp_val;
                            };

                        _Compile comp_val;
                        if(context.definition)
                            {
                                comp_val = compile_body();
                            }
                        else
                            {
                                SkipBlock my_def(code);
                                comp_val = compile_body();
                            }

                        return _Form(gc.amake<Procedure>(entry_point, size, comp_val.result_tag),
                                     0,
                                     FormTag::done,
                                     comp_val.result_tag);
                    }
                case tag<Quote>::value:
                    {
                        // TODO: copy?  Not sure how to handle this with GC.
                        code.pointer(ast[1].value);
                        return _Form(aimm<Null>(),
                                     0,
                                     FormTag::done,
                                     ast[1]._tag);
                    }
                case tag<PrimitiveMacro>::value:
                    {
                        auto& fn = unwrap<PrimitiveMacro>(head).fn;
                        Eval eval(this);
                        auto type = fn(eval, slice(ast, 1));

                        return _Form(aimm<Null>(),
                                     0,
                                     FormTag::done,
                                     type);
                    }
                case tag<If>::value:
                    {
                        auto will_jump = [&]() -> iterator {
                            code.pointer(nullptr);
                            return code.last();
                        };

                        auto alt_address = will_jump();
                        _compile(ast[1],  code, Context(false, false, ast)); // get the predicate
                        code.if_();

                        // consiquent
                        auto consiquent = compile_tail(ast[2]);

                        auto after_alt = will_jump();
                        code.jump();

                        // alternate
                        *alt_address = reinterpret_cast<value_type>(code.end());
                        auto alternate = compile_tail(ast[3]);

                        *after_alt = reinterpret_cast<value_type>(code.end());

                        auto padding = 0;
                        if(context.tail)
                            padding = max(consiquent.pad_to,
                                          alternate.pad_to);

                        if(consiquent.result_tag != alternate.result_tag)
	                        throw WrongTypeError
		                        ("Both branches of an if clause must return the same type");

                        return _Form(aimm<Null>(), padding, FormTag::done,
                                     consiquent.result_tag);
                    }
                case tag<Ast>::value:
                    {
                        auto compiled = _compile(head, code, Context(false, false, ast));

                        return _Form(compiled.applicable,
                                     0,
                                     FormTag::function,
                                     compiled.result_tag);
                    }
                case tag<Define>::value:
                    {
                        auto def_compile = [&](Any input) {
                            return _compile(input, code, Context(true, true, ast));
                        };

                        auto sym = unwrap<Symbol>(ast[1]);
                        Any value = ast[2];

                        iterator entry_point;

                        Any def;
                        auto found = _env->_local.find(sym.name);
                        if(found == _env->_local.end())
                            def = gc.amake<Undefined>(nullptr);

                        else if(is<Undefined>(found->second.value))
                            def = found->second.value;

                        else
                            throw WrongTypeError("A symbol cannot be defined twice in the same scope");

                        _env->define(sym.name, def);

                        _Compile body_result;

                        while(is<Symbol>(value))
                            value = value_or_undef(unwrap<Symbol>(value));

                        // Lambda is a little bit of a special case.
                        if(is<Ast>(value)) {
                            head = unwrap<Ast>(value)[0];
                            if(is<Symbol>(head))
                                head = value_or_undef(unwrap<Symbol>(head));

                            if(is<Lambda>(head))
                                body_result = def_compile(value);

                            value = body_result.applicable;
                        }

                        if(is<Procedure>(value))
                            {
                                entry_point = unwrap<Procedure>(value).body;
                            }
                        else
                            {
                                // treat non-procedure expression as
                                // thunks,the procedure call mechanism
                                // to actually get a value.
                                entry_point = code.end();
                                body_result = def_compile(ast[2]);

                                code.return_(0);

                                value = gc.amake<Procedure>(entry_point, 0, tag<Any>::value);
                            }

                        for(auto& vv : unwrap<Undefined>(def).backtrack)
                            *vv = reinterpret_cast<value_type>(entry_point);

                        _env->define(sym.name, value);

                        if(_env->_prev == nullptr)
                            wrapped.main_entry_point = code.end();

                        return _Form(aimm<Null>(), 0, FormTag::done, body_result.result_tag);
                    }
                default:
                    return result(head, 0, FormTag::function);
                }
        }

        struct _Compile
        {
            Any applicable;
            size_t pad_to;
            tag_t result_tag;

            _Compile() = default;
            _Compile(Any aa, size_t pad, tag_t rt)
                : applicable(aa), pad_to(pad), result_tag(rt)
            {}
        };

        // :returns: thing that can be applied and the size of its tail call
        _Compile _compile(Any input, AssembleVM& code, Context context)
        {
            using namespace std;

            auto atom_result = [&]()
                {
                    return _Compile(aimm<Null>(), 0, input._tag);
                };

        compile_value:
            switch(input._tag) {
            case tag<Ast>::value:
                {
                    // todo... does the IF form needs to move in here.
                    auto ast = unwrap<Ast>(input);
                    auto form = this->form(ast, code, context);

                    switch(form.form_tag) {
                    case FormTag::done:
                        return _Compile(form.applicable, form.pad_to, form.result_tag);

                    case FormTag::function:
                        {
                            auto fn = form.applicable;
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

                            auto observed_types = std::vector<tag_t>();
                            observed_types.reserve(rest.size());

                            for(auto& vv : rest)
                                observed_types.push_back(_compile(vv, code, Context(false, false, ast)
                                                                  ).result_tag);

                            tag_t result_type = tag<Any>();

                            if(is_procedure)
                                {
                                    auto proc = unwrap<Procedure>(fn);
                                    result_type = proc.return_type;

                                    if(context.tail)
                                        code.tail_call(tail_size, proc.body);
                                    else
                                        code.call_procedure(proc.body);
                                }
                            else if(is<Undefined>(fn))
                                {
                                    // TODO: I'm just assuming this is the
                                    // definition of the current function.
                                    // co-recursive functions with different max
                                    // tail size than their aritys will totes
                                    // break.
                                    code.constant(rest.size())
                                        .pointer(nullptr);
                                    unwrap<Undefined>(fn).backtrack.push_back(code.last());

                                    code.tail_call();

                                    result_type = tag<Undefined>();
                                }
                            else
                                result_type = _compile
                                    (form.applicable, code, Context(false, false, ast, &observed_types))
                                    .result_tag;

                            return _Compile(form.applicable, tail_size, result_type);
                        }}
                    throw "should be unreachable";
                }
            case tag<Symbol>::value:
                input = value_or_undef(unwrap<Symbol>(input));
                goto compile_value;
            case tag<Parameter>::value:
                {
                    auto param = unwrap<Parameter>(input);
                    if(param.hops) { // -> non local
                        code.nested_argument(param.offset, param.hops);
                    } else {
                        code.argument(param.offset);
                    }
                    break;
                }
            case tag<Undefined>::value:
                {
                    code.pointer(nullptr);
                    unwrap<Undefined>(input).backtrack.push_back(code.last());

                    // Assume a thunk will get patched in later.
                    code.call_procedure();
                    break;
                }
            case tag<Fixnum>::value:
                code.constant(value<Fixnum>(input));
                return atom_result();

            case tag<Bool>::value:
                code.constant(value<Bool>(input));
                return atom_result();

            case tag<Pointer>::value:
                code.pointer(value<Pointer>(input));
                return atom_result();

            case tag<String>::value:
                code.pointer(&value<String>(input));
                return atom_result();

            case tag<Type>::value:
                {
                    code.pointer(value<Type>(input));
                    return _Compile(aimm<Null>(), 0, abstract_type::return_tag(*value<Type>(input)));
                }
            case tag<CxxFunctor>::value:
                {
                    auto& fn = unwrap<CxxFunctor>(input);

                    // TODO: check arity against the CxxFunctions types.
                    code.std_function(&fn.fn, context.expression.size() - 1);
                    return _Compile(aimm<Null>(), 0, abstract_type::return_tag(*fn.types));
                }
            case tag<Method>::value:
                {
                    auto& dispatcher = unwrap<Method>(input);
                    input = dispatcher.value(make_range(&*context.observed_types->begin(),
                                                        &*context.observed_types->end()));
                    goto compile_value;
                }
            case tag<Procedure>::value:
                {
                    auto proc = unwrap<Procedure>(input);
                    code.call_procedure(proc.body);
                    return _Compile(aimm<Null>(), 0, proc.return_type);
                }

            default:
                {
                    throw std::string("Illegal syntax or something.");
                }}
            return _Compile(aimm<Null>(), 0, tag<Any>::value);
        }

        // For recursive entry (as with primitive macros).  The code
        // generated isn't cleanly terminated.
        tag_t in_place_any(Any ast)
        {
            if(wrapped.back() == vm_codes::Tag<vm_codes::finish>::value)
                --wrapped._end;

            return _compile(ast, wrapped, Context(false, false, unwrap<Ast>(ast))).result_tag;
        }

        // Lets macros reach in and explicitly add values to the
        // PCode.  todo: This is not an ideal abstraction.
        PCode::iterator push_value(PCode::value_type value)
        {
            auto rval = wrapped._end;
            wrapped.constant(value);
            return rval;
        }

        // For most external use.  The generated code can be passed to
        // the VM for evaluation.
        tag_t any(Any ast)
        {
            auto tag = in_place_any(ast);
            wrapped.finish();
            return tag;
        }

        // Reset the pcode entry point to compile another expression (as for the REPL).
        void repl_reset()
        {
            wrapped._end = wrapped.main_entry_point;
        }

        void reset()
        {
            wrapped._end = wrapped.main_entry_point = wrapped._begin;
        }

        AssembleVM&& finish()
        {
            return std::move(wrapped);
        }

        void print();
    };

    void Compile::print()
    {
        cout << "Main entry point: " << wrapped.main_entry_point << endl;
        wrapped.dbg();
    }

    Any Eval::any(Any input)
    {
        auto saved_excursion = compile->save_excursion();
        auto tag = compile->any(input);
        vm.run(compile->wrapped.main_entry_point);
        return Any(tag, reinterpret_cast<void*>(vm.stack[0]));
    }
}


#endif
