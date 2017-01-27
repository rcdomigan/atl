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
	    struct SubstituteMap
		    : public MarkBase<GC>,
		      public std::map<Type, Any>
	    {
		    typedef std::map<Type, Any> Map;

		    SubstituteMap(SubstituteMap const&) = delete;

		    SubstituteMap(SubstituteMap&& other)
			    : MarkBase(std::move(other)),
			      Map(std::move(other))
		    {}

		    SubstituteMap(GC& gc)
			    : MarkBase(gc)
		    {}

		    SubstituteMap& operator=(SubstituteMap&& other)
		    {
			    *dynamic_cast<MarkBase<GC>*>(this) = std::move(other);
			    *dynamic_cast<Map*>(this) = std::move(other);
			    return *this;
		    }

		    virtual void mark() override
		    { for(auto& item : *this) { _gc->mark(item.second); } }
	    };


	    /* Remove a set of variables from substitutions (ie, don't substitute
	       for types which are quantified at this scope). */
	    struct SubsScope
	    {
		    SubstituteMap &subs,
			    old_subs;

		    SubsScope(GC& gc, SubstituteMap& subs_, Scheme::Quantified& quantified)
			    : subs(subs_), old_subs(gc)
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
	    void build_substitute_type(AstBuilder& store, SubstituteMap& subs, Any& value)
	    {
		    switch(value._tag)
			    {
			    case tag<Type>::value:
				    {
					    auto type = unwrap<Type>(value);
					    auto itr = subs.find(type);
					    if(itr != subs.end())
						    {
							    Any new_value = itr->second;
							    if(is<Ast>(new_value))
								    { ast_hof::copy(unwrap<Ast>(new_value))(store); }
							    else
								    { store.push_back(new_value); }
						    }
					    else
						    { store.push_back(value); }

					    return;
				    }
			    case tag<Ast>::value:
				    {
					    NestAst nest(store);
					    for(auto& vv : unwrap<Ast>(value).modify_data())
						    {
							    // values goes to `store`
							    build_substitute_type(store, subs, vv);
						    }

					    return;
				    }
			    default:
				    throw WrongTypeError(std::string("Type expressions can only be 'Type' and Asts of Type.  Got a ")
				                         .append(type_name(value._tag)));
			    }
	    }

	    Any substitute_type(GC& gc, SubstituteMap& subs, Any value)
	    {
		    switch(value._tag)
			    {
			    case tag<Type>::value:
				    {
					    auto type = unwrap<Type>(value);
					    auto itr = subs.find(type);
					    if((itr != subs.end()))
						    {
							    auto new_value = itr->second;
							    if(is<Ast>(new_value))
								    { return wrap(*gc(ast_hof::copy(unwrap<Ast>(itr->second)))); }
							    return new_value;
						    }
					    return value;
				    }
			    case tag<Ast>::value:
				    {
					    auto builder = gc.ast_builder();
					    build_substitute_type(builder, subs, value);
					    return wrap(builder.root());
				    }
			    default:
				    throw WrongTypeError(std::string("Type expressions can only be 'Type' and Asts of Type.  Got a ")
				                         .append(type_name(value._tag)));
			    }
	    }

	    /* Gamma is the symbol to scheme map */
	    struct Gamma
		    : public MarkBase<GC>,
		      public std::unordered_map<std::string, Scheme>
	    {
		    Gamma(GC& gc) : MarkBase(gc) {}

		    virtual void mark() override
		    {
			    for(auto& item : *this)
				    { _gc->mark(item.second.type); }
		    }
	    };

	    /** Look for Symbols and apply subs to them
	     */
	    void apply_substitution(GC& store, SubstituteMap& subs, Any value)
	    {
	    switch(value._tag)
			    {
				    // Subbing for the Symbol also takes care of
				    // anything Bound to it.
			    case tag<Symbol>::value:
				    {
					    auto& sym = unwrap<Symbol>(value);

					    SubsScope scope(store, subs, sym.scheme.quantified);
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
							    SubsScope scope(store, subs, scheme.quantified);
							    scheme.type = substitute_type(store, subs, scheme.type);
						    }
					    else
						    { lambda.value->return_type = substitute_type(store, subs, lambda.value->return_type); }

					    break;
				    }
			    case tag<Ast>::value:
				    {
					    for(auto& item : unwrap<Ast>(value).modify_data())
						    { apply_substitution(store, subs, item); }
					    break;
				    }
			    }
	    }

	    Any instantiate(GC& store, Type::value_type& new_types, Scheme& scheme)
	    {
		    if(is<Undefined>(scheme.type))
			    { return scheme.type = wrap<Type>(++new_types); }
		    if(scheme.quantified.empty())
			    { return scheme.type; }
		    else
			    {
				    SubstituteMap subs(store);
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

	    void compose_subs(GC& store, SubstituteMap& left, SubstituteMap const& right)
	    {
		    for(auto const& rr : right)
			    {
				    auto right_temp = rr.second;
				    left[rr.first] = substitute_type(store, left, right_temp);
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
	    SubstituteMap most_general_unification(GC& store, Any const& left, Any const& right)
        {
	        using namespace make_ast;
	        SubstituteMap rval(store);

	        if(left == right)
		        { return rval; }

	        /* Try to pick the concrete type */
	        if(is<Type>(right) && is<Type>(left))
		        {
			        if(right.value < left.value)
				        { rval.emplace(unwrap<Type>(left), right); }
			        else
				        { rval.emplace(unwrap<Type>(right), left); }
			        return rval;
		        }

	        if(!is<Ast>(right))
		        {
			        rval.emplace(typify(right), left);
			        return rval;
		        }

	        if(!is<Ast>(left))
		        {
			        rval.emplace(typify(left), right);
			        return rval;
		        }

	        auto left_ast = unwrap<Ast>(left),
		        right_ast = unwrap<Ast>(right);

	        while(true)
	            {
		            auto u1 = most_general_unification
			            (store, left_ast[0], right_ast[0]);

		            if(left_ast.size() > 1)
			            {
				            auto left_store = store.ast_builder(),
					            right_store = store.ast_builder();

				            left_store.nest_ast();
				            right_store.nest_ast();

				            if(left_ast.size() != right_ast.size())
					            { arity_mismatch(wrap(left_ast), wrap(right_ast)); }

				            for(auto& inner_itr : slice(left_ast.modify_data(), 1))
					            { build_substitute_type(left_store, u1, inner_itr); }

				            for(auto& inner_itr : slice(right_ast.modify_data(), 1))
					            { build_substitute_type(right_store, u1, inner_itr); }

				            left_store.end_ast();
				            right_store.end_ast();

				            left_ast = left_store.root();
				            right_ast = right_store.root();

				            compose_subs(store, u1, rval);
				            rval.swap(u1);
			            }
		            else
			            {
				            compose_subs(store, u1, rval);
				            rval.swap(u1);
				            break;
			            }
	            }

            return rval;
        }

	    /* Adds symbols to the environment for the lifetime of the instance */
	    struct FormalsScope
	    {
		    Gamma& gamma;
		    Gamma old;
		    std::vector<std::string> formals;

		    FormalsScope(Gamma& gamma_, Type::value_type& new_types, Ast ast)
			    : gamma(gamma_), old(*gamma_._gc)
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

	    Scheme generalize(Gamma& gamma, Any const& type)
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

	    // WResult is meant to mark its members on a recieving
	    // function, but not on the returning function (similar to the
	    // logic for MarkedBase; see the comments there for an attempt
	    // at an explanation).
	    struct WResult
	    {
		    SubstituteMap subs;
		    Marked<Any> type;

		    WResult(SubstituteMap&& subs_)
			    : subs(std::move(subs_))
		    {}

		    WResult(SubstituteMap&& subs_, Any const& type_)
			    : subs(std::move(subs_)), type(subs_._gc->marked(type_))
		    {}

		    WResult(SubstituteMap&& subs_, Marked<Any>&& type_)
			    : subs(std::move(subs_)), type(std::move(type_))
		    {}

		    WResult(WResult&& other)
			    : subs(std::move(other.subs)),
			      type(std::move(other.type))
		    {}

		    WResult& operator=(WResult&& other)
		    {
			    subs = std::move(other.subs);
			    type = std::move(other.type);
			    return *this;
		    }
	    };

        // Take a stab at the algorithm W
	    WResult W(GC& store, Type::value_type& new_types, Gamma& gamma, Any expr)
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
			    case tag<Symbol>::value:
				    {
					    auto& sym = get_sym(unwrap<Symbol>(expr));
					    return WResult(SubstituteMap(store),
					                   store.marked(instantiate(store, new_types, sym.scheme)));
				    }

			    case tag<CxxFunctor>::value:
				    {
					    return WResult(SubstituteMap(store),
					                   store.marked(wrap(unwrap<CxxFunctor>(expr).type)));
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
							    auto metadata = unwrap<Lambda>(ast[0]).value;

							    WResult body{SubstituteMap(store)};
							    {
								    FormalsScope scope(gamma, new_types, formals);
								    body = W(store, new_types, gamma, ast[2]);
							    }
							    metadata->return_type = *body.type;

							    if(formals.empty())
								    { return WResult(std::move(body.subs), store(fn_type::fn(*body.type)).wrap()); }
							    else
								    {
									    auto builder = store.ast_builder();
									    fn_type::FnBuilder rtype_builder(builder);

									    apply_substitution(store, body.subs, ref_wrap(formals));

									    for(auto raw_sym : formals)
										    {
											    auto& sym = get_sym(modify<Symbol>(raw_sym));

											    // pull substitutions of 'quantified' symbols out of body.subs:
											    SubsScope scope(store, body.subs, sym.scheme.quantified);

											    sym.scheme.type = substitute_type
												    (store,
												     body.subs,
												     sym.scheme.type);
											    sym.scheme.type = substitute_type(store, body.subs, sym.scheme.type);

											    quantified.insert(sym.scheme.quantified.begin(),
											                      sym.scheme.quantified.end());
											    rtype_builder.other_arg(sym.scheme.type);
										    }

									    if(is<Scheme>(*body.type))
										    { rtype_builder.last_arg(unwrap<Scheme>(*body.type).type); }
									    else
										    { rtype_builder.last_arg(*body.type); }

									    return WResult(std::move(body.subs), store.marked(wrap(rtype_builder.root())));
								    }
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
									    SubsScope knockout_subs(store, e1.subs, item.second.quantified);
									    item.second.type = substitute_type(store, e1.subs, item.second.type);
								    }

							    sym.scheme = generalize(generalize_env, *e1.type);

							    gamma[sym.name] = sym.scheme;
							    return WResult(std::move(e1.subs), store.marked(wrap(&sym.scheme)));
						    }
					    else
						    {
							    using namespace make_ast;

							    auto e1 = W(store,  new_types, gamma, ast[0]);

							    if(ast.size() == 1) // thunk:
								    {
									    using namespace pattern_match;

									    Any return_type;
									    if(match(fnt(capture(return_type)), *e1.type))
										    { return WResult(std::move(e1.subs), return_type); }

									    else if(is<Type>(*e1.type))
										    {
											    return_type = wrap<Type>(++new_types);
											    e1.subs[unwrap<Type>(*e1.type)] = wrap(*store(fn_type::fn(return_type)));
											    return WResult(std::move(e1.subs), return_type);
										    }

									    std::cout << "no good: " << printer::print(*e1.type) << std::endl;
									    throw AlgorithmWFailed("Thunk function of several args");
								    }
							    else
								    {
									    for(auto& item : gamma)
										    {
											    SubsScope knockout_subs(store, e1.subs, item.second.quantified);
											    new_var(item.second);
											    item.second.type = substitute_type(store, e1.subs, item.second.type);
										    }

									    for(auto arg : slice(ast, 1))
										    {
											    WResult e2 = W(store, new_types, gamma, arg);

											    auto beta = Type(++new_types);

											    auto left = store.marked(substitute_type(store, e2.subs, *e1.type));
											    auto right = store(fn_type::fn(*e2.type, beta.value()));
											    auto V = most_general_unification(store, left.any, right.any);

											    auto subbed_beta = store.marked(substitute_type(store, V, ref_wrap(beta)));

											    compose_subs(store, V, e2.subs);
											    compose_subs(store, V, e1.subs);

											    e1.subs = std::move(V);
											    e1.type = std::move(subbed_beta);
										    }

									    return e1;
								    }
						    }
				    }

			    case tag<Type>::value:
				    return WResult(SubstituteMap(store), store.marked(expr));

			    default:
				    return WResult(SubstituteMap(store), store.marked(wrap<Type>(expr._tag)));
			    }
	    }
    }
}

#endif
