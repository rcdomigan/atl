#ifndef ATL_PRINT_HH
#define ATL_PRINT_HH
/**
 * @file /home/ryan/programming/atl/print.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Aug 10, 2013
 */
#include <functional>
#include <iostream>
#include <utility>
#include <cassert>

#include "./utility.hpp"
#include "./wrap.hpp"
#include "./is.hpp"

namespace atl
{
	namespace printer
	{
		long _trim_addr(void *pntr)
		{ return reinterpret_cast<long>(pntr) & (256 - 1); };

		/* Print a Type atom */
		std::ostream& _print_type_atom(Any value, std::ostream& out)
		{
			auto& type = unwrap<Type>(value);
			if(type.value() == tag<FunctionConstructor>::value)
				{ return out << "->"; }
			else
				{
					out << "#<Type: ";

					if(type.value() > BOOST_PP_SEQ_SIZE(ATL_TYPES_SEQ))
						{ out << type.value(); }
					else
						{ out << type_name(type.value()); }

					return out << ">";
				}
		}

		std::ostream& print_atom(Any any, std::ostream& out)
		{
			using namespace std;

			switch(any._tag)
				{
				case tag<Undefined>::value:
					{
						out << "#<Undefined ";
						return out << ":" << hex << _trim_addr(any.value) << ">";
					}

				case tag<Symbol>::value:
					return out << "'" << unwrap<Symbol>(any).name;

				case tag<Parameter>::value:
					return out << "#<Paremeter " << unwrap<Parameter>(any).value << ">";

				case tag<ClosureParameter>::value:
					return out << "#<ClosureParameter " << unwrap<ClosureParameter>(any).value << ">";

				case tag<Type>::value:
					return _print_type_atom(any, out);

				case tag<Fixnum>::value:
					return out << value<Fixnum>(any);

				case tag<Bool>::value:
					{
						if(value<bool>(any))
							return out << "True";
						else
							return out << "False";
					}
				case tag<String>::value:
					return out << '"' << *reinterpret_cast<string*>(any.value) << '"';

				case tag<CxxFunctor>::value:
					return out << unwrap<CxxFunctor>(any).name;

				case tag<Pointer>::value:
					{
						if(any.value)
							return out << "#<Pointer-" << hex << (reinterpret_cast<long>(any.value) & (256 - 1)) << ">";
						else
							return out << "#<Pointer-NULL>";
					}
				case tag<LambdaMetadata>::value:
					{
						out << "#<LambdaMetadata" << flush;
						auto& closure = unwrap<LambdaMetadata>(any).closure;
						if(!closure.empty())
							{
								out << "(" << closure.size() << " free vars)";
							}
						return out << ">" << flush;;
					}
				case tag<Lambda>::value:
					{ return out << "#<Lambda " << any.value << ">" << flush;; }

				default:
					return out << "#<" << type_name(any._tag) << ">";
				}
		}

		std::ostream& print_type(Any const& value, std::ostream& out)
		{
			switch(value._tag)
				{
				case tag<Ast>::value:
					{
						auto ast = unwrap<Ast>(value);
						out << "(";
						if(!ast.empty())
							{
								print_type(ast[0], out);
								for(auto vv : slice(ast, 1))
									{
										out << ' ';
										print_type(vv, out);
									}
							}
						return out << ")";
					}
				case tag<Type>::value:
					return _print_type_atom(value, out);

				case tag<Scheme>::value:
					{
						auto& scheme = unwrap<Scheme>(value);
						out << "#<Scheme: ";
						if(!scheme.quantified.empty())
							{
								out << "[" << *scheme.quantified.begin();
								for(auto& item : make_range(++scheme.quantified.begin(),
								                            scheme.quantified.end()))
									{ out << " " << item; }
								out << "] ";
							}
						return print_type(scheme.type, out) << ">";
					}
				default:
					return out << "#<BAD TYPE: " << type_name(value._tag) << ">";
				}
		}

		struct Printer
		{ virtual std::ostream& print(std::ostream&) const = 0; };

		struct PrintAny
			: public Printer
		{
			Any root;

			PrintAny(Any const& vv)
				: root(vv)
			{ assert(!is<AstData>(vv)); }

			std::ostream& _print(Any const& value, std::ostream& out) const
			{
				switch(value._tag)
					{
					case tag<Ast>::value:
						{
							auto ast = unwrap<Ast>(value);
							out << "(";
							if(!ast.empty())
								{
									_print(ast[0], out);
									for(auto vv : slice(ast, 1))
										{
											out << ' ';
											_print(vv, out);
										}
								}
							return out << ")";
						}
					default:
						return print_atom(value, out);
					}
			}

			virtual std::ostream& print(std::ostream& out) const override
			{ return _print(root, out); }
		};

		PrintAny print(Any const& aa) { return PrintAny(aa); }

		template<class T>
		PrintAny print(T const& aa) { return PrintAny(wrap(aa)); }

		struct PrintWithType
			: public Printer
		{
			Any root;

			PrintWithType(Any const& vv) : root(vv) { assert(!is<AstData>(vv)); }

			std::ostream& _print(Any const& value, std::ostream& out) const
			{
				auto print_sym = [&](Symbol const& sym) -> std::ostream&
					{
						out << sym.name << ":";
						print_type(wrap(&const_cast<Symbol&>(sym).scheme), out);
						return out;
					};

				switch(value._tag)
					{
					case tag<Ast>::value:
						{
							auto ast = unwrap<Ast>(value);
							out << "(";
							if(!ast.empty())
								{
									_print(ast[0], out);
									for(auto vv : slice(ast, 1))
										{
											out << ' ';
											_print(vv, out);
										}
								}
							return out << ")";
						}
					case tag<Parameter>::value:
						{
							return out << "[Parameter "
							           << unwrap<Parameter>(value).value
							           << "]";
						}
					case tag<ClosureParameter>::value:
						{
							return out << "[ClosureParameter "
							           << unwrap<ClosureParameter>(value).value
							           << "]";
						}
					case tag<Symbol>::value:
						{
							return print_sym(unwrap<Symbol>(value));
						}
					case tag<CxxFunctor>::value:
						{
							auto fn = unwrap<CxxFunctor>(value);
							out << "#<c++-fn: " << fn.name;
							print_type(wrap(fn.type), out);
							return out << ">";
						}
					case tag<Lambda>::value:
						{
							auto metadata = unwrap<Lambda>(value).value;
							return out << "#<fn: " << _trim_addr(&metadata) << ">";
						}
					case tag<Scheme>::value:
						{ return print_type(value, out); }
					default:
						return print_atom(value, out);
					}
			}
			virtual std::ostream& print(std::ostream& out) const override
			{ return _print(root, out); }
		};

		template<class T>
		PrintWithType with_type(T const& aa) { return PrintWithType(wrap(aa)); }

		std::ostream& operator<<(std::ostream& out, const printer::Printer& p) { return p.print(out); }
	}

	std::ostream& dbg_any(Any vv)
	{ return std::cout << printer::print(vv) << std::endl; }

	std::ostream& dbg_ast(Ast const& vv)
	{ return std::cout << printer::print(vv) << std::endl; }

	std::ostream& dbg_type(Any const& value)
	{ return printer::print_type(value, std::cout) << std::endl; }

	std::ostream& dbg_with_type(Any const& value)
	{ return std::cout << printer::PrintWithType(value) << std::endl; }
}


#endif
