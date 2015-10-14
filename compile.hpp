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
#include "./vm.hpp"
#include "./byte_code.hpp"
#include "./type.hpp"
#include "./lexical_environment.hpp"
#include "./utility.hpp"

#include <set>

namespace atl
{

    namespace compile_helpers
    {
	    // If an inner procedures is referencing argument in its
	    // closure I have to follow return addresses to 'hop' up to
	    // the enclosed parameters.
	    //
	    // TODO: It should be possible to compute fixed offsets in may
	    // cases (if the inner function is tail recursive or not
	    // recursive) rather than hopping at runtime
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

    struct Compile
    {
        typedef AssembleCode::const_iterator iterator;
	    typedef pcode::Offset Offset;

	    std::set<std::string> _undefined;
        lexical::Map *_env;     // pushing a scope mutates where this points
        GC& gc;

        AssembleCode code;

	    bool _do_type_check;

	    // Types are currently checked, but it's a primitive system.
	    // A SupressTypeCheck, returned by Compile.supress_type_check,
	    // can disable the checks where they are broken.
	    struct SupressTypeCheck
	    {
		    Compile& compile;
		    SupressTypeCheck(Compile& compile_) : compile(compile_)
		    { compile._do_type_check = false; }
		    ~SupressTypeCheck()
		    { compile._do_type_check = true; }
	    };

        Compile(LexicalEnvironment& env)
	        : _env(&env.toplevel), gc(env.gc), code(&gc.alloc_pcode()),
	          _do_type_check(true)
	    {}

	    Compile(lexical::Map& env, GC& gc_, AssembleCode& output_)
		    : _env(&env), gc(gc_), code(output_),
		      _do_type_check(true)
        {}

	    // Ignore type errors where possible
	    SupressTypeCheck supress_type_check()
	    { return SupressTypeCheck(*this); }

	    // Setup a VM jump instruction which skips over the code
	    // defined during the SkipBlock instance's lifetime.
        struct SkipBlock
        {
	        pcode::Offset _skip_to;
            AssembleCode& code;
            SkipBlock(AssembleCode& code_) : code(code_)
            {
                code.pointer(nullptr);
                _skip_to = code.pos_last();
                code.jump();
            }

            ~SkipBlock()
	        { code[_skip_to] = code.pos_end(); }
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
	    PassByValue value_or_undef(std::string const& name)
        {
            auto def = _env->find(name);

            if(def == _env->end()) {
                auto udef = gc.amake<Undefined>(nullptr);
                _env->define(name, udef);
                _undefined.emplace(name);
                return PassByValue(udef);
            }
            return def->second.value;
        }

	    // done            : The form has been evaluated, _compile can return
	    // function        : The form was a function, _compile should call
	    // declare_type    : The form declares the type of a nested form
	    // macro_expansion : A new expression yielded by a macro
        enum class FormTag
        { done, function, declare_type, macro_expansion };

	    // Describe the thing-to-apply in an expression
        struct _Form
        {
            // result from 'form'
            PassByValue applicable;
            size_t pad_to;
            FormTag form_tag;
            tag_t result_tag;

            _Form(PassByValue applicable_,
                  size_t pad_to_,
                  FormTag form_tag_,
                  tag_t result_tag_)
                : applicable(applicable_),
                  pad_to(pad_to_),
                  form_tag(form_tag_),
                  result_tag(result_tag_)
            {}
        };

        /// \internal
        /// Evaluates the application position of an s-expression and
        /// returns a _Form structure with information for _compile.
        _Form form(Ast ast, AssembleCode& code, Context context)
        {
            using namespace std;

            PassByValue head = ast[0];
            auto result = [&head](PassByValue aa, size_t ss, FormTag ff)
                { return _Form(aa, ss, ff, head._tag); };

            auto compile_tail = [&](PassByValue input)
                {
                    return _compile(input, code,
                                    Context(context.tail, false, ast));
                };

        setup_form:
            switch(head._tag)
                {
                case tag<Symbol>::value:
                    head = value_or_undef(unwrap<Symbol>(head).name);
                    goto setup_form;

                case tag<CxxMacro>::value:
	                return _Form(pass_safe_value(unwrap<CxxMacro>(head).fn(slice(ast, 1))),
	                             0,
	                             FormTag::macro_expansion,
	                             0);

                case tag<Lambda>::value:
                    {
	                    Ast formals = pass_ast(ast[1]);
	                    auto size = formals.size();

                        compile_helpers::IncHopRAII inc_hops(_env);
                        lexical::MapRAII local(&_env);

                        for(auto ff : zip(formals,
                                          CountingRange()))
                            local.map.define(unwrap<Symbol>(*get<0>(ff)).name,
                                             // The `offset` should go high to low
                                             gc.amake<Parameter>(size - *get<1>(ff), 0));

                        // todo: For now I'm just putting the lambda body
                        // inline with the rest of the code.  I could save
                        // some jumps if the lambda bodies where gathered in
                        // one place in the closure.
                        pcode::Offset entry_point;

                        auto compile_body = [&]()
                            {
                                entry_point = code.pos_end();
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

                        return _Form(pass_safe_value(gc.amake<Procedure>(entry_point, size, comp_val.result_tag)),
                                     0,
                                     FormTag::done,
                                     comp_val.result_tag);
                    }
                case tag<Quote>::value:
                    {
                        // TODO: copy?  Not sure how to handle this with GC.
                        code.pointer(&ast[1]);
                        return _Form(pass_safe_value(wrap<Null>()),
                                     0,
                                     FormTag::done,
                                     tag<Pointer>::value);
                    }
                case tag<DeclareType>::value:
	                {
		                // requires a type-expr followed by an expr.
		                // The whole shooting match is given the
		                // Type.value as its type.
		                tag_t rval;
		                rval = Compile::any(ast[1]);
		                return _Form(pass_safe_value(wrap<Null>()),
		                             0,
		                             FormTag::declare_type,
		                             rval);
		           }
                case tag<If>::value:
                    {
	                    auto will_jump = [&]() -> pcode::Offset {
                            code.pointer(nullptr);
                            return code.pos_last();
                        };

                        auto alt_address = will_jump();
                        _compile(ast[1],  code, Context(false, false, ast)); // get the predicate
                        code.if_();

                        // consiquent
                        auto consiquent = compile_tail(ast[2]);

                        auto after_alt = will_jump();
                        code.jump();

                        // alternate
                        code[alt_address] = code.pos_end();
                        auto alternate = compile_tail(ast[3]);

                        code[after_alt] = code.pos_end();

                        auto padding = 0;
                        if(context.tail)
                            padding = max(consiquent.pad_to,
                                          alternate.pad_to);

                        if(_do_type_check && (consiquent.result_tag != alternate.result_tag))
	                        throw WrongTypeError
		                        (std::string("Both branches of an if clause must return the same type: got ")
		                         .append(type_name(consiquent.result_tag))
		                         .append(" and ")
		                         .append(type_name(alternate.result_tag)));

                        return _Form(pass_value<Null>(), padding, FormTag::done,
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
                        auto def_compile = [&](PassByValue input) {
                            return _compile(input, code, Context(true, true, ast));
                        };

                        auto sym = unwrap<Symbol>(ast[1]);
                        PassByValue value = ast[2];

                        pcode::Offset entry_point;

                        PassByValue def;
                        auto found = _env->_local.find(sym.name);
                        if(found == _env->_local.end())
	                        def = pass_safe_value(gc.amake<Undefined>(nullptr));

                        else if(is<Undefined>(found->second.value))
                            def = found->second.value;

                        else
                            throw WrongTypeError("A symbol cannot be defined twice in the same scope");

                        _undefined.erase(sym.name);
                        _env->define(sym.name,
                                     def.as_Any());

                        _Compile body_result;

                        while(is<Symbol>(value))
                            value = value_or_undef(unwrap<Symbol>(value).name);

                        // Lambda is a little bit of a special case.
                        if(is<Ast>(value)) {
	                        head = unwrap<Ast>(value)[0];
                            if(is<Symbol>(head))
                                head = value_or_undef(unwrap<Symbol>(head).name);

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
                                entry_point = code.pos_end();
                                body_result = def_compile(ast[2]);

                                code.return_(0);

                                value = pass_safe_value
	                                (gc.amake<Procedure>
	                                 (entry_point, 0, tag<Any>::value));
                            }

                        for(auto& vv : unwrap<Undefined>(def).backtrack)
                            code[vv] = entry_point;

                        _env->define(sym.name, value.as_Any());

                        if(_env->_prev == nullptr)
                            code.main_entry_point = code.pos_end();

                        return _Form(pass_value<Null>(),
                                     0,
                                     FormTag::done, body_result.result_tag);
                    }
                default:
                    return result(head, 0, FormTag::function);
                }
        }

        struct _Compile
        {
            PassByValue applicable;
            size_t pad_to;
            tag_t result_tag;

            _Compile() = default;
            _Compile(PassByValue aa, size_t pad, tag_t rt)
                : applicable(aa), pad_to(pad), result_tag(rt)
            {}
        };

	    /// \internal Take an input and generate byte-code.
	    ///
	    /// @param input: the thing to compile
	    /// @param code: the byte-code we're generating
	    /// @param context: relavent context, ie are we in a tail call
	    /// @return: Type information and other things a calling _compile needs to know about.
        _Compile _compile(PassByValue input, AssembleCode& code, Context context) {
            using namespace std;

            auto atom_result = [&]()
                {
                    return _Compile(pass_value<Null>(), 0, input._tag);
                };

        compile_value:
            switch(input._tag) {
            case tag<Ast>::value:
                {
                    auto ast = unwrap<Ast>(input);
                    auto form = this->form(ast, code, context);

                    switch(form.form_tag) {
                    case FormTag::done:
                        return _Compile(form.applicable, form.pad_to, form.result_tag);

                    case FormTag::macro_expansion:
	                    {
		                    input = form.applicable;
		                    goto compile_value;
	                    }
                    case FormTag::declare_type:
	                    {
		                    auto rval = _compile(ast[2], code, context);
		                    rval.result_tag = form.result_tag;
		                    return rval;
	                    }
                    case FormTag::function:
                        {
                            auto fn = form.applicable;
                            auto rest = slice(ast, 1);

                            auto is_procedure = is<Procedure>(fn);
                            size_t padding = 0;
                            size_t tail_size = 0;

                            if(is_procedure) {
                                // The function called in tail position may
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
                                    unwrap<Undefined>(fn).backtrack.push_back(code.pos_last());

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
                input = value_or_undef(unwrap<Symbol>(input).name);
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
                    unwrap<Undefined>(input).backtrack.push_back(code.pos_last());

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
                    return _Compile
	                    (pass_value<Null>(),
	                     0,
	                     abstract_type::return_tag(*value<Type>(input)));
                }
            case tag<CxxFunctor>::value:
                {
                    auto& fn = unwrap<CxxFunctor>(input);

                    // TODO: check arity against the CxxFunctions types.
                    code.std_function(&fn.fn, context.expression.size() - 1);
                    return _Compile
	                    (pass_value<Null>(),
	                     0, abstract_type::return_tag(*fn.types));
                }
            case tag<Procedure>::value:
                {
                    auto proc = unwrap<Procedure>(input);
                    code.call_procedure(proc.body);
                    return _Compile(pass_value<Null>(), 0, proc.return_type);
                }

            default:
                {
                    throw std::string("Illegal syntax or something.");
                }}
            return _Compile(pass_value<Null>(), 0, tag<Any>::value);
        }

        // For most external use.  The generated code can be passed to
        // the VM for evaluation.
        tag_t any(PassByValue ast)
	    {
		    if(is<Ast>(ast))
			    return _compile
				    (ast, code, Context(false, false, unwrap<Ast>(ast)))
				    .result_tag;
		    else
			    return _compile
				    (ast, code, Context(false, false, Ast()))
				    .result_tag;
        }

        // Reset the pcode entry point to compile another expression (as for the REPL).
        void repl_reset()
        {
	        code.output->code.erase(code.output->begin() + code.main_entry_point,
	                                code.output->end());
        }

        void reset()
        { code.clear(); }

        void dbg();

	    // Check that all symbols have definitions, raise
	    // UnboundSymbolError if they do not
	    void assert_ready()
	    {
		    if(!_undefined.empty())
			    {
				    std::string msg ("Undefined symbols after compilation: ");
				    msg.append(*_undefined.begin());

				    for(auto& nn : make_range(++_undefined.begin(), _undefined.end()))
					    msg.append(nn);

				    throw UnboundSymbolError(msg);
			    }
	    }

    };

    void Compile::dbg()
    {
        cout << "Main entry point: " << code.main_entry_point << endl;
        dbg_code(*code.output);
    }
}


#endif
