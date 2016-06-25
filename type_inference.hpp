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
		    for(auto& item : subs)
			    {
				    std::cout << item.first.value << " ";
				    if(is<Type>(item.second))
					    { std::cout << reinterpret_cast<size_t>(item.second.value) << std::endl; }
				    else
					    { dbg_type(pass_value(Any(item.second))); }
			    }
	    }

	    /** Perform type substitution on the Type part of a Scheme.
	     * @param store:
	     * @param subs:
	     * @param value:
	     * @return:
	     */
	    Any substitute_type(AstAllocator store, SubstituteMap& subs, PassByValue value)
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
								    { new_value = wrap(*ast_hof::copy(explicit_unwrap<Ast>(new_value),
								                                      store)); }
							    else
								    { store.push_back(new_value); }
						    }
					    else
						    {
							    new_value = value.any;
							    store.push_back(new_value);
						    }

					    return new_value;
				    }
			    case tag<Slice>::value:
				    {
					    ast_hof::NestAst nest(store);

					    for(auto& vv : unwrap<Slice>(value))
						    {
							    // values goes to `store`
							    substitute_type(store, subs, pass_value(vv));
						    }

					    return wrap(*nest.ast);
				    }
			    default:
				    throw WrongTypeError("Type expressions can only be 'Type' and Asts of Type.");
			    }
	    }

	    typedef std::unordered_map<std::string, Scheme> Gamma;

	    void dbg_gamma(Gamma& gamma)
	    {
		    for(auto& item : gamma)
			    {
				    std::cout<<item.first<<": ";
				    dbg_type(pass_value(&item.second));
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
					    sym.scheme.type = substitute_type(store, subs, pass_value(sym.scheme.type));
					    break;
				    }
			    case tag<Ast>::value:
			    case tag<AstData>::value:
			    case tag<Slice>::value:
				    {
					    for(auto& item : unwrap_slice(value))
						    { apply_substitution(store, gamma, subs, item); }
					    break;
				    }
			    }
	    }


	    Any instantiate(AstAllocator store, Type::value_type& new_types, Scheme& scheme)
	    {
		    if(is<Undefined>(scheme.type))
			    { return scheme.type = wrap<Type>(++new_types); }
		    if(scheme.quantified.empty())
			    { return wrap(scheme.type); }
		    else
			    {
				    SubstituteMap subs;
				    for(auto& item : scheme.quantified)
					    {
						    subs[Type(item)] = wrap<Type>(++new_types);
					    }
				    return substitute_type(store, subs, pass_value(scheme.type));
			    }
	    }

	    void free_type_variables(Scheme::Quantified& rval, PassByValue value)
	    {
		    switch(value._tag)
			    {
			    case tag<Type>::value:
				    rval.insert(unwrap<Type>(value).value);
				    return;
			    case tag<Slice>::value:
				    for(auto& item : unwrap<Slice>(value))
					    { free_type_variables(rval, pass_value(item)); }
				    return;
			    default:
				    throw WrongTypeError("Don't know how to find free type of this..");
			    }
	    }

	    Scheme::Quantified free_type_variables(Scheme& scheme)
	    {
		    Scheme::Quantified rval;
		    free_type_variables(rval, pass_value(scheme.type));
		    for(auto& item : scheme.quantified)
			    { rval.erase(item); }
		    return rval;
	    }

	    void compose_subs(AllocatorBase& store, SubstituteMap& left, SubstituteMap const& right)
	    {
		    for(auto const& rr : right)
			    { left[rr.first] = substitute_type(make_ast::ast_alloc(store), left, pass_value(Any(rr.second))); }
	    }

	    SubstituteMap most_general_unification(AllocatorBase& store, PassByValue left, PassByValue right)
        {
	        using namespace make_ast;
            SubstituteMap sub{};

	        if(left == right)
		        { return sub; }

	        if(!is<Slice>(right))
		        { return SubstituteMap({{typify(right), de_slice(store, left)}}); }

	        if(!is<Slice>(left))
		        { return SubstituteMap({{typify(left), de_slice(store, right)}}); }

	        auto left_ast = unwrap<Slice>(left),
		        right_ast = unwrap<Slice>(right);
	        if(left_ast.size() != right_ast.size())
		        { throw CantUnify("Can only unify functions of same arity"); }

	        while(true)
	            {
		            auto u1 = most_general_unification
			            (store, pass_value(left_ast[0]), pass_value(right_ast[0]));

		            if(left_ast.size() > 1)
			            {
				            auto left_store = ast_alloc(store),
					            right_store = ast_alloc(store);

				            auto new_left = left_store.nest_ast(),
					            new_right = right_store.nest_ast();

				            for(auto inner_itr : zip(slice(left_ast, 1),
				                                     slice(right_ast, 1)))
					            {
						            substitute_type(left_store, u1, pass_value(*get<0>(inner_itr)));
						            substitute_type(right_store, u1, pass_value(*get<1>(inner_itr)));
					            }

				            new_left.end_ast();
				            new_right.end_ast();

				            left_ast = Slice(*new_left);
				            right_ast = Slice(*new_right);

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

		    FormalsScope(Gamma& gamma_, Type::value_type& new_types, Slice ast)
			    : gamma(gamma_)
		    {
			    for(auto& var : ast)
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
					    rval.quantified.insert(unwrap<Type>(type).value);
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
			    case tag<AstData>::value:
			    case tag<Slice>::value:
				    {
					    // Skip the constructor
					    Slice slice(pass_value(type).slice.slice(1));
					    free_type_variables(rval.quantified, pass_value(slice));
					    rval.type = type;
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
	    WResult W(AllocatorBase& store, Type::value_type& new_types, Gamma& gamma, PassByValue expr)
	    {
		    auto new_var = [&](Scheme& scheme)
			    {
				    if(is<Undefined>(scheme.type))
					    { scheme.type = wrap<Type>(++new_types); }
			    };

		    auto get_sym = [&](Any& any) -> Symbol&
			    {
				    auto& sym = unwrap<Symbol>(any);

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
				    return WResult(SubstituteMap(),
				                   instantiate(store, new_types, unwrap<Bound>(expr).value->scheme));

			    case tag<Symbol>::value:
				    {
					    auto& sym = get_sym(expr.any);
					    return WResult(SubstituteMap(),
					                   instantiate(store, new_types, sym.scheme));
				    }
			    case tag<Slice>::value:
				    {
					    auto ast = unwrap<Slice>(expr);

					    if(is<Lambda>(ast[0]))
						    {
							    using namespace make_ast;
							    auto& formals = ast[1];
							    Scheme::Quantified quantified;

							    WResult t2;
							    {
								    FormalsScope scope(gamma, new_types, unwrap_slice(formals));
								    t2 = W(store, new_types, gamma, pass_value(ast[2]));
							    }

							    auto result_type = ast_alloc(store);
							    NestAst nest(result_type);

							    result_type.push_back(wrap<Type>(tag<FunctionConstructor>::value));

							    apply_substitution(store, gamma, t2.subs, formals);

							    auto append_scheme = [&](Scheme& scheme)
								    {
									    auto type = pass_value(scheme.type);
									    if(is<Slice>(type))
										    { ast_hof::copy(unwrap<Slice>(type), result_type); }
									    else
										    { result_type.push_back(type); }

									    quantified.insert(scheme.quantified.begin(),
									                      scheme.quantified.end());
								    };


							    for(auto& raw_sym : unwrap_slice(formals))
								    {
									    auto& sym = get_sym(raw_sym);
									    SubsScope scope(t2.subs, sym.scheme.quantified);
									    sym.scheme.type = substitute_type
										    (store,
										     t2.subs,
										     pass_value(sym.scheme.type));

									    append_scheme(sym.scheme);
								    }

							    if(is<Scheme>(t2.type))
								    { append_scheme(unwrap<Scheme>(t2.type)); }
							    else
								    { copy_into(pass_value(t2.type), result_type); }

							    return WResult(t2.subs, wrap(*nest.ast));
						    }
					    else if(is<Define>(ast[0]))
						    {
							    auto& sym = get_sym(ast[1]);

							    if(gamma.count(sym.name))
								    { throw RedefinitionError("Can't redefine a symbol at the same scope."); }

							    auto e1 = W(store, new_types, gamma, pass_value(ast[2]));

							    auto generalize_env = gamma;
							    for(auto& item : generalize_env)
								    {
									    SubsScope knockout_subs(e1.subs, item.second.quantified);
									    substitute_type(store, e1.subs, pass_value(item.second.type));
								    }

							    sym.scheme = generalize(generalize_env, e1.type);

							    gamma[sym.name] = sym.scheme;
							    return WResult(e1.subs, wrap(&sym.scheme));
						    }
					    else
						    {
							    using namespace make_ast;
							    if(ast.size() < 2)
								    { throw AlgorithmWFailed("Applying function to zero arguments"); }

							    auto e1 = W(store,  new_types, gamma, pass_value(ast[0]));

							    for(auto& item : gamma)
								    {
									    SubsScope knockout_subs(e1.subs, item.second.quantified);
									    new_var(item.second);
									    item.second.type = substitute_type(store, e1.subs, pass_value(item.second.type));
								    }

							    for(auto& arg : slice(ast, 1))
								    {
									    WResult e2 = W(store, new_types, gamma, pass_value(arg));

									    auto beta = Type(++new_types);
									    auto V = most_general_unification
										    (store,
										     pass_value(substitute_type(store, e2.subs, pass_value(e1.type))),
										     pass_value
										     (make(lift<Type>(tag<FunctionConstructor>::value),
										           lift(e2.type),
										           lift<Type>(beta.value))(ast_alloc(store))));

									    auto subbed_beta = substitute_type(store, V, pass_value(beta));

									    compose_subs(store, V, e2.subs);
									    compose_subs(store, V, e1.subs);

									    e1.subs = V;
									    e1.type = subbed_beta;
								    }

							    return e1;
						    }
				    }

			    default:
				    return WResult(SubstituteMap(), wrap<Type>(expr._tag));
			    }
	    }
    }
}

#endif
