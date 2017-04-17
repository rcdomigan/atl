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
		    : public MarkBase,
		      public std::map<Type, Any>
	    {
		    typedef std::map<Type, Any> Map;

		    SubstituteMap(SubstituteMap const&)=delete;

		    SubstituteMap(SubstituteMap&&)=default;

		    SubstituteMap(GC& gc)
			    : MarkBase(gc)
		    {}

		    SubstituteMap& operator=(SubstituteMap&& other)=default;

		    virtual void mark() override
		    { for(auto& item : *this) { manage_marking.gc->mark(item.second); } }

		    void dbg()
		    {
			    using namespace printer;
			    for(auto& item : *this)
				    { std::cout << print(item.first) << std::flush << ": " << print(item.second) << std::endl; }
		    }

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
	    void build_substitute_type(GC& gc, AstBuilder& building, SubstituteMap& subs, Ast::iterator& itr)
	    {
		    switch(itr.tag())
			    {
			    case tag<Type>::value:
				    {
					    auto type = unwrap<Type>(*itr);
					    auto found = subs.find(type);
					    if(found != subs.end())
						    {
							    Marked<Any> new_value = gc.marked(found->second);
							    if(is<Ast>(new_value.any))
								    { ast_hof::copy(unwrap<Ast>(*new_value).subex())(building); }
							    else
								    { building.push_back(*new_value); }
						    }
					    else
						    { building.push_back(*itr); }

					    return;
				    }
			    case tag<Ast>::value:
				    {
					    NestAst nest(building);
					    for(auto vv : itritrs(atl::subex(itr)))
						    {
							    // values goes to `building`
							    build_substitute_type(gc, building, subs, vv);
						    }

					    return;
				    }
			    default:
				    throw WrongTypeError(std::string("Type expressions can only be 'Type' and Asts of Type.  Got a ")
				                         .append(type_name(itr->_tag)));
			    }
	    }

	    void build_substitute_type(GC& gc, AstBuilder& building, SubstituteMap& subs, Ast::iterator const& value)
	    {
		    auto itr = value;
		    build_substitute_type(gc, building, subs, itr);
	    }

	    // Perform substitutions according to `subs` in `values` (which must not be an AstData).
	    Any substitute_type(GC& gc, SubstituteMap& subs, Any& value)
	    {
		    switch(value._tag)
			    {
			    case tag<Type>::value:
				    {
					    auto type = unwrap<Type>(value);
					    auto found = subs.find(type);
					    if((found != subs.end()))
						    {
							    auto new_value = found->second;
							    if(is<Ast>(new_value))
								    { return wrap(*gc(ast_hof::copy(unwrap<Ast>(new_value).subex()))); }
							    return new_value;
						    }
					    return value;
				    }
			    case tag<Ast>::value:
				    {
					    auto builder = gc.ast_builder();
					    build_substitute_type(gc,
					                          builder,
					                          subs,
					                          unwrap<Ast>(value).self_iterator());
					    return wrap(builder.root());
				    }
			    default:
				    throw WrongTypeError(std::string("Type expressions can only be 'Type' and Asts of Type.  Got a ")
				                         .append(type_name(value._tag)));
			    }
	    }

	    /* Gamma is the symbol to scheme map */
	    struct Gamma
		    : public MarkBase,
		      public std::unordered_map<std::string, Scheme>
	    {
		    Gamma(GC& gc) : MarkBase(gc) {}

		    virtual void mark() override
		    {
			    for(auto& item : *this)
				    { manage_marking.gc->mark(item.second.type); }
		    }
	    };

	    /** Look for Symbols and apply subs to them
	     */
	    void apply_substitution(GC& store, SubstituteMap& subs, Any& value);

	    void apply_substitution(GC& store, SubstituteMap& subs, Ast::Subex subex)
	    {
		    for(auto item : itritrs(subex))
			    { apply_substitution(store, subs, item.reference()); }

	    }

	    void apply_substitution(GC& store, SubstituteMap& subs, Any& value)
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
			    case tag<Ast>::value:
				    {
					    apply_substitution(store, subs, unwrap<Ast>(value).subex());
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

	    void compose_subs(GC& store, SubstituteMap& left, SubstituteMap& right)
	    {
		    for(auto& rr : right)
			    { left[rr.first] = substitute_type(store, left, rr.second); }
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
	    SubstituteMap most_general_unification(GC& gc, Any const& left, Any const& right)
        {
	        using namespace make_ast;
	        SubstituteMap rval(gc);

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

	        Marked<Ast> left_ast{gc._cxx_stack, left};
	        Marked<Ast> right_ast{gc._cxx_stack, right};

	        while(true)
	            {
		            auto u1 = most_general_unification
			            (gc, (*left_ast)[0], (*right_ast)[0]);

		            if(left_ast->size() > 1)
			            {
				            if(left_ast->size() != right_ast->size())
					            { arity_mismatch(left_ast.any, right_ast.any); }

				            auto left_store = gc.ast_builder(),
					            right_store = gc.ast_builder();
				            {
					            NestAst nest_left(left_store);
					            NestAst nest_right(right_store);

					            for(auto inner_itr : slice(itritrs(left_ast->subex()), 1))
						            { build_substitute_type(gc, left_store, u1, inner_itr); }

					            for(auto inner_itr : slice(itritrs(right_ast->subex()), 1))
						            { build_substitute_type(gc, right_store, u1, inner_itr); }
				            }
				            *left_ast = left_store.root();
				            *right_ast = right_store.root();

				            compose_subs(gc, u1, rval);
				            rval.swap(u1);
			            }
		            else
			            {
				            compose_subs(gc, u1, rval);
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

		    FormalsScope(Gamma& gamma_, Type::value_type& new_types, Ast::Subex ast)
			    : gamma(gamma_), old(*gamma_.manage_marking.gc)
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

		    WResult(SubstituteMap&& subs_, Marked<Any>&& type_)
			    : subs(std::move(subs_)), type(std::move(type_))
		    {}

		    WResult(SubstituteMap&& subs_, Any const& type_)
			    : WResult(std::move(subs_), subs_.manage_marking.gc->marked(type_))
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

	    struct AlgorithmW
	    {
		    GC& gc;
		    Type::value_type& new_types;
		    Gamma& gamma;

		    AlgorithmW(GC& gc_, Type::value_type& new_types_, Gamma& gamma_)
			    : gc(gc_), new_types(new_types_), gamma(gamma_)
		    {}

		    void new_var(Scheme& scheme)
		    {
			    if(is<Undefined>(scheme.type))
				    { scheme.type = wrap<Type>(++new_types); }
		    }

		    Symbol& get_sym(Symbol& sym)
		    {
			    auto itr = gamma.find(sym.name);
			    if(itr != gamma.end())
				    { sym.scheme = itr->second; }
			    else
				    { new_var(sym.scheme); }

			    return sym;
		    }

		    WResult W(Ast::Subex subex)
		    {
			    auto itr = subex.begin();
			    if(itr.is<Lambda>())
				    {
					    using namespace make_ast;
					    if(subex.size() != 3)
						    { throw ArityError("Lambda accepts only a formals and body lists"); }

					    ++itr;
					    auto formals = atl::subex(itr);

					    ++itr;
					    Scheme::Quantified quantified;
					    WResult body = WResult(SubstituteMap(gc), gc.marked(wrap<Null>()));
					    {
						    FormalsScope scope(gamma, new_types, formals);
						    body = W(itr);
					    }

					    if(formals.empty())
						    { return WResult(std::move(body.subs),
						                     gc(fn_type::fn(*body.type)).wrap()); }
					    else
						    {
							    auto builder = gc.ast_builder();
							    fn_type::FnBuilder rtype_builder(builder);

							    apply_substitution(gc, body.subs, formals);

							    for(auto raw_sym : formals)
								    {
									    auto& sym = get_sym(modify<Symbol>(raw_sym));

									    // pull substitutions of 'quantified' symbols out of body.subs:
									    SubsScope scope(gc, body.subs, sym.scheme.quantified);

									    sym.scheme.type = substitute_type
										    (gc,
										     body.subs,
										     sym.scheme.type);
									    sym.scheme.type = substitute_type(gc, body.subs, sym.scheme.type);

									    quantified.insert(sym.scheme.quantified.begin(),
									                      sym.scheme.quantified.end());
									    rtype_builder.other_arg(sym.scheme.type);
								    }

							    if(is<Scheme>(*body.type))
								    { rtype_builder.last_arg(unwrap<Scheme>(*body.type).type); }
							    else
								    { rtype_builder.last_arg(*body.type); }

							    return WResult(std::move(body.subs), gc.marked(wrap(rtype_builder.root())));
						    }
				    }
			    else if(itr.is<Define>())
				    {
					    ++itr;
					    auto& sym = get_sym(unwrap<Symbol>(itr.reference()));

					    if(gamma.count(sym.name))
						    { throw RedefinitionError("Can't redefine a symbol at the same scope."); }

					    ++itr;
					    auto e1 = W(itr);

					    auto generalize_env = gamma;
					    for(auto& item : generalize_env)
						    {
							    SubsScope knockout_subs(gc, e1.subs, item.second.quantified);
							    item.second.type = substitute_type(gc, e1.subs, item.second.type);
						    }

					    sym.scheme = generalize(generalize_env, *e1.type);

					    gamma[sym.name] = sym.scheme;
					    return WResult(std::move(e1.subs), gc.marked(wrap(&sym.scheme)));
				    }
			    else
				    {
					    using namespace make_ast;

					    auto e1 = W(itr);

					    if(subex.size() == 1) // thunk:
						    {
							    using namespace pattern_match;

							    Any return_type;
							    if(match(fnt(capture(return_type)),
							             *e1.type))
								    { return WResult(std::move(e1.subs), return_type); }

							    else if(is<Type>(*e1.type))
								    {
									    return_type = wrap<Type>(++new_types);
									    e1.subs[unwrap<Type>(*e1.type)] = wrap(*gc(fn_type::fn(return_type)));
									    return WResult(std::move(e1.subs), return_type);
								    }

							    std::cout << "no good: " << printer::print(*e1.type) << std::endl;
							    throw AlgorithmWFailed("Thunk function of several args");
						    }
					    else
						    {
							    for(auto& item : gamma)
								    {
									    SubsScope knockout_subs(gc, e1.subs, item.second.quantified);
									    new_var(item.second);
									    item.second.type = substitute_type(gc, e1.subs, item.second.type);
								    }

							    for(auto arg : slice(itritrs(subex), 1))
								    {
									    WResult e2 = W(arg);

									    auto beta = Type(++new_types);

									    auto left = gc.marked(substitute_type(gc, e2.subs, *e1.type));
									    auto right = gc(fn_type::fn(*e2.type, beta.value()));
									    auto V = most_general_unification(gc, left.any, right.any);

									    auto subbed_beta = gc.marked(substitute_type(gc, V, ref_wrap(beta)));

									    compose_subs(gc, V, e2.subs);
									    compose_subs(gc, V, e1.subs);

									    e1.subs = std::move(V);
									    e1.type = std::move(subbed_beta);
								    }

							    return e1;
						    }
				    }
		    }

		    WResult W(Ast::iterator itr)
		    {
			    switch(itr.tag())
				    {
				    case tag<Symbol>::value:
					    {
						    auto& sym = get_sym(unwrap<Symbol>(itr.reference()));
						    return WResult(SubstituteMap(gc),
						                   gc.marked(instantiate(gc, new_types, sym.scheme)));
					    }

				    case tag<CxxFunctor>::value:
					    {
						    return WResult(SubstituteMap(gc),
						                   gc.marked(wrap(unwrap<CxxFunctor>(*itr).type)));
					    }

				    case tag<Ast>::value:
					    { return W(atl::subex(itr)); }

				    case tag<Type>::value:
					    return WResult(SubstituteMap(gc), gc.marked(*itr));

				    default:
					    return WResult(SubstituteMap(gc), gc.marked(wrap<Type>(itr->_tag)));
				    }
		    }

		    WResult W(Marked<Ast>& ast)
		    { return W(ast->subex()); }
	    };
    }
}

#endif
