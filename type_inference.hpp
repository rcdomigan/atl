#ifndef ATL_INFER_HPP
#define ATL_INFER_HPP
/**
 * @file /home/ryan/programmring/atl/infer.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jun 03, 2014
 *
 * A stab at type inference
 */
#include "./print.hpp"
#include "./helpers.hpp"
#include "./helpers/pattern_match.hpp"

#include <unordered_map>
#include <vector>
#include <functional>

namespace atl
{
    namespace inference
    {
	    using ast_hof::AstAllocator;
	    typedef std::map<Type, Any> SubstituteMap;


	    /* Remove a set of variables from substitutions (ie, don't substitute
	       for types which are quantified at this scope). */
	    struct SubsScope
	    {
		    SubstituteMap &subs,
			    old_subs;

		    SubsScope(SubstituteMap& subs_, Scheme::Quantified& quantified)
			    : subs(subs_)
		    {
			    for(auto& raw_var : quantified)
				    {
					    Type var(raw_var);

					    auto itr = subs.find(var);
					    if(itr != subs.end())
						    {
							    old_subs.emplace(var, itr->second);
							    subs.erase(itr);
						    }
				    }
		    }

		    ~SubsScope()
		    {
			    for(auto& item : old_subs)
				    { subs[item.first] = item.second; }
		    }
	    };

	    bool operator==(SubstituteMap const& left, SubstituteMap const& right)
	    {
		    if(left.size() != right.size())
			    { return false; }

		    for(auto& ll : left)
			    {
				    auto rr = right.find(ll.first);
				    if(rr == right.end())
					    { return false; }

				    if(rr->second._tag != ll.second._tag)
					    { return false; }

				    if(is<Ast>(rr->second)
				       && (explicit_unwrap<Ast>(rr->second) != explicit_unwrap<Ast>(ll.second)))
					    { return false; }
			    }
		    return true;
	    }

	    void dbg_subs(SubstituteMap const& subs)
	    {
		    using namespace printer;
		    for(auto& item : subs)
			    {
				    std::cout << print(item.first) << ": " << print(item.second) << "\n";
			    }
		    std::cout << std::flush;
	    }

	    /** Perform type substitution on the Type part of a Scheme.
	     * @param store:
	     * @param subs:
	     * @param value:
	     * @return:
	     */
	    Any substitute_type(AstAllocator store, SubstituteMap& subs, Any& value)
	    {
		    switch(value._tag)
			    {
			    case tag<Type>::value:
				    {
					    auto type = unwrap<Type>(value);
					    auto itr = subs.find(type);
					    Any new_value;
					    if(itr != subs.end())
						    {
							    new_value = itr->second;
							    if(is<Ast>(new_value))
								    { new_value = wrap(*ast_hof::copy(unwrap<Ast>(new_value),
								                                      store)); }
							    else
								    { store.push_back(new_value); }
						    }
					    else
						    {
							    new_value = value;
							    store.push_back(new_value);
						    }

					    return new_value;
				    }
			    case tag<Ast>::value:
				    {
					    ast_hof::NestAst nest(store);

					    for(auto& vv : unwrap<Ast>(value).modify_data())
						    {
							    // values goes to `store`
							    substitute_type(store, subs, vv);
						    }

					    return wrap(*nest.ast);
				    }
			    default:
				    throw WrongTypeError(std::string("Type expressions can only be 'Type' and Asts of Type.  Got a ")
				                         .append(type_name(value._tag)));
			    }
	    }

	    /* Gamma is the symbol to scheme map */
	    typedef std::unordered_map<std::string, Scheme> Gamma;

	    void dbg_gamma(Gamma& gamma)
	    {
		    for(auto& item : gamma)
			    {
				    std::cout<<item.first<<": ";
				    dbg_type(wrap(&item.second));
			    }
	    }

	    /** Look for Symbols and apply subs to them
	     * @param store:
	     * @param subs:
	     * @param value:
	     */
	    void apply_substitution(AstAllocator store, Gamma& gamma, SubstituteMap& subs, Any& value)
	    {
		    switch(value._tag)
			    {
				    // Subbing for the Symbol also takes care of
				    // anything Bound to it.
			    case tag<Symbol>::value:
				    {
					    auto& sym = unwrap<Symbol>(value);

					    SubsScope scope(subs, sym.scheme.quantified);
					    sym.scheme.type = substitute_type(store, subs, sym.scheme.type);
					    break;
				    }
			    case tag<Lambda>::value:
				    {
					    auto& lambda = unwrap<Lambda>(value);
					    assert(lambda.value);

					    if(is<Scheme>(lambda.value->return_type))
						    {
							    auto& scheme = unwrap<Scheme>(lambda.value->return_type);
							    SubsScope scope(subs, scheme.quantified);
							    scheme.type = substitute_type(store, subs, scheme.type);
						    }
					    else
						    { lambda.value->return_type = substitute_type(store, subs, lambda.value->return_type); }

					    break;
				    }
			    case tag<Ast>::value:
				    {
					    for(auto& item : unwrap<Ast>(value).modify_data())
						    { apply_substitution(store, gamma, subs, item); }
					    break;
				    }
			    case tag<Type>::value:
				    {
					    auto found = subs.find(unwrap<Type>(value));
					    if(found != subs.end())
						    { value = found->second; }
				    }
			    }
	    }

	    Any instantiate(AstAllocator store, Type::value_type& new_types, Scheme& scheme)
	    {
		    if(is<Undefined>(scheme.type))
			    { return scheme.type = wrap<Type>(++new_types); }
		    if(scheme.quantified.empty())
			    { return scheme.type; }
		    else
			    {
				    SubstituteMap subs;
				    for(auto& item : scheme.quantified)
					    {
						    subs[Type(item)] = wrap<Type>(++new_types);
					    }
				    return substitute_type(store, subs, scheme.type);
			    }
	    }

	    void free_type_variables(Scheme::Quantified& rval, Any const& value)
	    {
		    switch(value._tag)
			    {
			    case tag<Type>::value:
				    rval.insert(unwrap<Type>(value).value());
				    return;
			    case tag<Ast>::value:
				    for(auto item : unwrap<Ast>(value))
					    { free_type_variables(rval, item); }
				    return;
			    default:
				    throw WrongTypeError("Don't know how to find free type of this..");
			    }
	    }

	    Scheme::Quantified free_type_variables(Scheme& scheme)
	    {
		    Scheme::Quantified rval;
		    free_type_variables(rval, scheme.type);
		    for(auto& item : scheme.quantified)
			    { rval.erase(item); }
		    return rval;
	    }

	    void compose_subs(AllocatorBase& store, SubstituteMap& left, SubstituteMap const& right)
	    {
		    for(auto const& rr : right)
			    {
				    auto right_temp = rr.second;
				    left[rr.first] = substitute_type(make_ast::ast_alloc(store), left, right_temp);
			    }
	    }

	    void arity_mismatch(Any const& left, Any const& right)
	    {
		    std::stringstream output;

		    output << "Can only unify functions of same arity:\n";
		    output << "   first expression: ";
		    printer::print_type(left, output) << "\n";

		    output << "   second expression: ";
		    printer::print_type(right, output) << "\n";

		    throw ArityError(output.str());
	    }

	    /* Unify the Types lists `left` and `right` */
	    SubstituteMap most_general_unification(AllocatorBase& store, Any const& left, Any const& right)
        {
	        using namespace make_ast;
            SubstituteMap sub{};

	        if(left == right)
		        { return sub; }

	        /* Try to pick the concrete type */
	        if(is<Type>(right) && is<Type>(left))
		        {
			        if(right.value < left.value)
				        { return SubstituteMap({{unwrap<Type>(left), right}}); }
			        else
				        { return SubstituteMap({{unwrap<Type>(right), left}}); }
		        }

	        if(!is<Ast>(right))
		        { return SubstituteMap({{typify(right), left}}); }

	        if(!is<Ast>(left))
		        { return SubstituteMap({{typify(left), right}}); }

	        auto left_ast = unwrap<Ast>(left),
		        right_ast = unwrap<Ast>(right);

	        while(true)
	            {
		            auto u1 = most_general_unification
			            (store, left_ast[0], right_ast[0]);

		            if(left_ast.size() > 1)
			            {
				            auto left_store = ast_alloc(store),
					            right_store = ast_alloc(store);

				            auto new_left = left_store.nest_ast(),
					            new_right = right_store.nest_ast();

				            if(left_ast.size() != right_ast.size())
					            { arity_mismatch(wrap(left_ast), wrap(right_ast)); }

				            for(auto inner_itr : slice(left_ast.modify_data(), 1))
					            { substitute_type(left_store, u1, inner_itr); }

				            for(auto inner_itr : slice(right_ast.modify_data(), 1))
					            { substitute_type(right_store, u1, inner_itr); }

				            new_left.end_ast();
				            new_right.end_ast();

				            left_ast = *new_left;
				            right_ast = *new_right;

				            compose_subs(store, u1, sub);
				            sub.swap(u1);
			            }
		            else
			            {
				            compose_subs(store, u1, sub);
				            sub.swap(u1);
				            break;
			            }
	            }

            return sub;
        }

	    /* Adds symbols to the environment for the lifetime of the instance */
	    struct FormalsScope
	    {
		    Gamma& gamma;
		    Gamma old;
		    std::vector<std::string> formals;

		    FormalsScope(Gamma& gamma_, Type::value_type& new_types, Ast ast)
			    : gamma(gamma_)
		    {
			    for(auto var : ast)
				    {
					    auto& sym = unwrap<Symbol>(var);

					    formals.push_back(sym.name);

					    auto itr = gamma.find(sym.name);
					    if(itr != gamma.end())
						    { old.insert(std::move(*itr)); }

					    sym.scheme.type = wrap<Type>(++new_types);
					    gamma[sym.name] = sym.scheme;
				    }
		    }

		    ~FormalsScope()
		    {
			    for(auto& formal : formals)
				    { gamma.erase(formal); }

			    for(auto& item : old)
				    { gamma[item.first] = std::move(item.second); }
		    }
	    };

	    Scheme::Quantified free_type_variables(Gamma& gamma)
	    {
		    Scheme::Quantified rval;
		    for(auto& item : gamma)
			    {
				    auto inner = free_type_variables(item.second);
				    rval.insert(inner.begin(), inner.end());
			    }
		    return rval;
	    }

	    Scheme generalize(Gamma& gamma, Any& type)
	    {
		    Scheme rval;

		    switch(type._tag)
			    {
			    case tag<Type>::value:
				    {
					    rval.quantified.insert(unwrap<Type>(type).value());
					    rval.type = type;
					    break;
				    }
			    case tag<Scheme>::value:
				    {
					    auto& scheme = unwrap<Scheme>(type);
					    rval.quantified = free_type_variables(scheme);
					    rval.type = scheme.type;
					    break;
				    }
			    case tag<Ast>::value:
				    {
					    auto ast = unwrap<Ast>(type);
					    rval.type = type;

					    if(!ast.empty())
						    {
							    // Skip the constructor
							    for(auto vv : slice(ast, 1))
								    { free_type_variables(rval.quantified, vv); }
						    }
					    break;
				    }
			    default:
				    throw WrongTypeError("Don't know how to generalize this..");
			    }

		    for(auto& type_tag : free_type_variables(gamma))
			    { rval.quantified.erase(type_tag); }

		    return rval;
	    }

	    struct WResult
	    {
		    SubstituteMap subs;
		    Any type;

		    WResult(SubstituteMap subs_, Any type_)
			    : subs(subs_), type(type_)
		    {}

		    WResult() = default;
	    };

        // Take a stab at the algorithm W
	    WResult W(AllocatorBase& store, Type::value_type& new_types, Gamma& gamma, Any expr)
	    {
		    auto new_var = [&](Scheme& scheme)
			    {
				    if(is<Undefined>(scheme.type))
					    { scheme.type = wrap<Type>(++new_types); }
			    };

		    auto get_sym = [&](Symbol& sym) -> Symbol&
			    {
				    auto itr = gamma.find(sym.name);
				    if(itr != gamma.end())
					    { sym.scheme = itr->second; }
				    else
					    { new_var(sym.scheme); }

				    return sym;
			    };

		    switch(expr._tag)
			    {
			    case tag<Bound>::value:
				    {
					    return WResult(SubstituteMap(),
					                   instantiate(store,
					                               new_types,
					                               get_sym(*unwrap<Bound>(expr).sym).scheme));
				    }

			    case tag<Symbol>::value:
				    {
					    auto& sym = get_sym(unwrap<Symbol>(expr));
					    return WResult(SubstituteMap(),
					                   instantiate(store, new_types, sym.scheme));
				    }

			    case tag<CxxFunctor>::value:
				    {
					    return WResult(SubstituteMap(),
					                   wrap(unwrap<CxxFunctor>(expr).type));
				    }

			    case tag<Ast>::value:
				    {
					    auto ast = unwrap<Ast>(expr);

					    if(is<Lambda>(ast[0]))
						    {
							    using namespace make_ast;
							    if(ast.size() != 3)
								    { throw ArityError("Lambda accepts only a formals and body lists"); }

							    auto formals = unwrap<Ast>(ast[1]);

							    Scheme::Quantified quantified;
							    Ast result_type;

							    WResult body;
							    {
								    FormalsScope scope(gamma, new_types, formals);
								    body = W(store, new_types, gamma, ast[2]);
							    }

							    if(formals.empty())
								    { result_type = mk(wrap<Type>(tag<FunctionConstructor>::value), body.type)(ast_alloc(store)); }
							    else
								    {
									    fn_type::FnBuilder rtype_builder(ast_alloc(store));

									    auto tmp = wrap(formals);
									    apply_substitution(store, gamma, body.subs, tmp);

									    for(auto raw_sym : formals)
										    {
											    auto& sym = get_sym(modify<Symbol>(raw_sym));
											    SubsScope scope(body.subs, sym.scheme.quantified);
											    sym.scheme.type = substitute_type
												    (store,
												     body.subs,
												     sym.scheme.type);

											    quantified.insert(sym.scheme.quantified.begin(),
											                      sym.scheme.quantified.end());
											    rtype_builder.other_arg(sym.scheme.type);
										    }

									    if(is<Scheme>(body.type))
										    { rtype_builder.last_arg(unwrap<Scheme>(body.type).type); }
									    else
										    { rtype_builder.last_arg(body.type); }

									    result_type = rtype_builder.root();
								    }

							    if(is<Scheme>(body.type))
								    {
									    auto& scheme = unwrap<Scheme>(body.type);
									    quantified.insert(scheme.quantified.begin(),
									                      scheme.quantified.end());
								    }

							    auto& lambda = unwrap<Lambda>(ast.reference(0));
							    if(nullptr == lambda.value)
								    { lambda.value = store.lambda_metadata(); }

							    lambda.value->return_type = body.type;

							    return WResult(body.subs, wrap(result_type));
						    }
					    else if(is<Define>(ast[0]))
						    {
							    auto& sym = get_sym(modify<Symbol>(ast[1]));

							    if(gamma.count(sym.name))
								    { throw RedefinitionError("Can't redefine a symbol at the same scope."); }

							    auto e1 = W(store, new_types, gamma, ast[2]);

							    auto generalize_env = gamma;
							    for(auto& item : generalize_env)
								    {
									    SubsScope knockout_subs(e1.subs, item.second.quantified);
									    substitute_type(store, e1.subs, item.second.type);
								    }

							    sym.scheme = generalize(generalize_env, e1.type);

							    gamma[sym.name] = sym.scheme;
							    return WResult(e1.subs, wrap(&sym.scheme));
						    }
					    else
						    {
							    using namespace make_ast;

							    auto e1 = W(store,  new_types, gamma, ast[0]);

							    if(ast.size() == 1) // thunk:
								    {
									    using namespace pattern_match;

									    Any return_type;
									    if(match(pattern_match::ast(tt<FunctionConstructor>(),
									                                capture(return_type)),
									             e1.type))
										    { return WResult(e1.subs, return_type); }

									    else if(is<Type>(e1.type))
										    {
											    return_type = wrap<Type>(++new_types);
											    e1.subs[unwrap<Type>(e1.type)] = wrap(fn_type::fn(return_type)(ast_alloc(store)));
											    return WResult(e1.subs, return_type);
										    }

									    throw AlgorithmWFailed("Thunk function of several args");
								    }
							    else
								    {
									    for(auto& item : gamma)
										    {
											    SubsScope knockout_subs(e1.subs, item.second.quantified);
											    new_var(item.second);
											    item.second.type = substitute_type(store, e1.subs, item.second.type);
										    }

									    for(auto arg : slice(ast, 1))
										    {
											    WResult e2 = W(store, new_types, gamma, arg);

											    auto beta = Type(++new_types);

											    auto V = most_general_unification
												    (store,
												     substitute_type(store, e2.subs, e1.type),
												     wrap(fn_type::fn(e2.type, beta.value())(ast_alloc(store))));

											    auto subbed_beta = substitute_type(store, V, ref_wrap(beta));

											    compose_subs(store, V, e2.subs);
											    compose_subs(store, V, e1.subs);

											    e1.subs = V;
											    e1.type = subbed_beta;
										    }

									    return e1;
								    }
						    }
				    }

			    case tag<Type>::value:
				    return WResult(SubstituteMap(), expr);

			    default:
				    return WResult(SubstituteMap(), wrap<Type>(expr._tag));
			    }
	    }
    }
}

#endif
