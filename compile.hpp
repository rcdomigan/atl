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
	struct _EnvScope
	{
		typedef ToplevelMap Map;
		Map local,
			**current_environment,
			*enclosing;

		_EnvScope(Map **env_)
		{
			current_environment = env_;
			enclosing = *env_;
			*current_environment = &local;
		}

		~_EnvScope()
		{ *current_environment = enclosing; }
	};

    struct Compile
    {
        typedef AssembleCode::const_iterator iterator;
	    typedef pcode::Offset Offset;

	    typedef ToplevelMap Env;
	    Env &toplevel, *current_env;
	    DefProcedure::Closure free_vars;

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

	    Compile(Env& env, GC& gc_)
		    : toplevel(env), current_env(&toplevel), gc(gc_), code(&gc_.alloc_pcode()),
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
	                head = (*current_env)[unwrap<Symbol>(head).name];
                    goto setup_form;

                case tag<CxxMacro>::value:
	                return _Form(pass_value(unwrap<CxxMacro>(head).fn(slice(ast, 1))),
	                             0,
	                             FormTag::macro_expansion,
	                             0);

                case tag<DefProcedure>::value:
                    {
	                    Ast formals = unwrap_ast(ast[1]);
	                    auto size = formals.size();

	                    _EnvScope local_env(&current_env);

                        for(auto ff : zip(formals,
                                          CountingRange()))
                            current_env->define
	                            (unwrap<Symbol>(*get<0>(ff)).name,
	                             // The `offset` should go high to low
	                             wrap<Parameter>(size - *get<1>(ff)));

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

                        return _Form(pass_value(gc.amake<Procedure>(entry_point, size, comp_val.result_tag)),
                                     0,
                                     FormTag::done,
                                     comp_val.result_tag);
                    }
                case tag<Quote>::value:
                    {
                        // TODO: copy?  Not sure how to handle this with GC.
                        code.pointer(&ast[1]);
                        return _Form(pass_value(wrap<Null>()),
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
		                return _Form(pass_value(wrap<Null>()),
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

                        auto found = current_env->find(sym.name);

                        Any def;
                        if(found == current_env->end())
	                        { def = wrap<Undefined>(); }
                        else
	                        { def = found->second; }

                        free_vars.erase(sym.name);
                        current_env->define(sym.name, def);

                        _Compile body_result;

                        while(is<Symbol>(value))
	                        value = (*current_env)[unwrap<Symbol>(value).name];

                        // Lambda is a little bit of a special case.
                        if(is<Ast>(value)) {
	                        head = unwrap<Ast>(value)[0];
                            if(is<Symbol>(head))
	                            head = (*current_env)[unwrap<Symbol>(head).name];

                            if(is<DefProcedure>(head))
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

                                value = pass_value
	                                (gc.amake<Procedure>
	                                 (entry_point, 0, tag<Any>::value));
                            }

                        current_env->define(sym.name, *value.any);

                        code.output->offset_table.set(sym.name,
                                                      entry_point);

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
        _Compile _compile(PassByValue input, AssembleCode& code, Context context)
	    {
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
                            else
                                result_type = _compile
                                    (form.applicable, code, Context(false, false, ast, &observed_types))
                                    .result_tag;

                            return _Compile(form.applicable, tail_size, result_type);
                        }}
                    throw "should be unreachable";
                }
            case tag<Symbol>::value:
	            {
		            auto const& name = unwrap<Symbol>(input).name;
		            auto itr = current_env->find(name);
		            if(itr == current_env->end())
			            { throw UnboundSymbolError(std::string("Can't find definition for symbol ")
			                                       .append(name)); }
		            else
			            { input = itr->second; }
	            }
	            goto compile_value;
            case tag<Parameter>::value:
                {
                    code.argument(unwrap<Parameter>(input).value);
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
            case tag<CxxFunctor>::value:
                {
                    auto& fn = unwrap<CxxFunctor>(input);

                    // TODO: check arity against the CxxFunctions types.
                    code.std_function(&fn.fn, context.expression.size() - 1);
                    return _Compile
	                    (pass_value<Null>(),
	                     0, fn.types.back()._tag);
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
			    {
				    auto tree = unwrap<Ast>(ast);
				    assign_free(*current_env, gc, free_vars, tree);

				    return _compile
					    (tree,
					     code, Context(false, false, tree))
					    .result_tag;
			    }
		    else
			    return _compile
				    (ast, code, Context(false, false, Ast()))
				    .result_tag;
        }

        void dbg();

	    // Check that all symbols have definitions, raise
	    // UnboundSymbolError if they do not
	    void assert_ready()
	    {
		    if(!free_vars.empty())
			    {
				    std::string msg ("Undefined symbols after compilation: ");
				    msg.append(*free_vars.begin());

				    for(auto& nn : make_range(++free_vars.begin(), free_vars.end()))
					    msg.append(nn);

				    throw UnboundSymbolError(msg);
			    }
	    }

	    /** Prepare the `code` to run on the VM.  For a code fragment,
	     * just append the 'finish' instruction.  For a 'program'
	     * (something with a "main" function) append a call to "main"
	     * and add a "__call-main__" symbol in the offset table for the VM
	     * to use.
	     *
	     * @return: The setup code
	     */
	    Code* pass_code_out()
	    {
		    auto main_itr = current_env->find("main");
		    if(main_itr != current_env->end())
			    {
				    auto main = main_itr->second;
				    auto num_tail_params = unwrap<Procedure>(main).tail_params;
				    auto entry = code.pos_end();

				    /* Make sure we're padded out for a tail call. */
				    for(size_t i = 0; i < num_tail_params; ++i)
					    code.constant(0);

				    code.call_procedure(code.output->offset_table["main"]);
				    code.output->offset_table.set("__call-main__",
				                                  entry);
			    }
		    return code.pass_code_out();
	    }

	    /** Prepare to append more instructions to some code.  While
	     * Compile has taken some code, the code is not suitable to
	     * run on the VM.
	     *
	     * @param taking: Code we're going to append to.
	     */
	    void take_code(Code* taking)
	    {
		    if(taking->has_main())
			    {
				    auto& raw_code = taking->code;
				    raw_code.erase(raw_code.begin() + taking->offset_table["__call-main__"],
				                   raw_code.end());
			    }

		    code.take_code(taking);
	    }
    };

    void Compile::dbg()
    { code.output->dbg(); }
}


#endif
