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

#include "./utility.hpp"
#include "./helpers.hpp"

namespace atl
{
	namespace printer
	{
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
			auto trim_addr = [](void *pntr)
				{ return reinterpret_cast<long>(pntr) & (256 - 1); };

			switch(any._tag)
				{
				case tag<Undefined>::value:
					{
						out << "#<Undefined ";
						return out << ":" << hex << trim_addr(any.value) << ">";
					}

				case tag<Symbol>::value:
					return out << "'" << unwrap<Symbol>(any).name;

				case tag<Bound>::value:
					return out << "#<Bound " << unwrap<Bound>(any).sym->name << ">";

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
						auto closure = unwrap<LambdaMetadata>(any).closure;
						if(!closure.empty())
							{
								out << "(" << closure.size() << " free vars)";
							}
						return out << ">" << flush;;
					}
				default:
					return out << "#<" << type_name(any._tag) << ">";
				}
		}

		std::ostream& print_type(PassByValue value, std::ostream& out)
		{
			switch(value._tag)
				{
				case tag<Slice>::value:
					{
						auto ast = unwrap<Slice>(value);
						out << "(";
						if(!ast.empty())
							{
								print_type(pass_value(ast[0]), out);
								for(auto& vv : slice(ast, 1))
									{
										out << ' ';
										print_type(pass_value(vv), out);
									}
							}
						return out << ")";
					}
				case tag<Type>::value:
					return _print_type_atom(value.any, out);

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
						return print_type(pass_value(scheme.type), out) << ">";
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
			PassByValue root;

			PrintAny(PassByValue vv) : root(vv) {}

			std::ostream& _print(PassByValue value, std::ostream& out) const
			{
				switch(value._tag)
					{
					case tag<Slice>::value:
						{
							auto ast = value.slice;
							out << "(";
							if(!ast.empty())
								{
									_print(pass_value(ast[0]), out);
									for(auto& vv : slice(ast, 1))
										{
											out << ' ';
											_print(pass_value(vv), out);
										}
								}
							return out << ")";
						}
					default:
						return print_atom(value.any, out);
					}
			}

			virtual std::ostream& print(std::ostream& out) const override
			{ return _print(root, out); }
		};

		PrintAny print(PassByValue aa) { return PrintAny(aa); }

		template<class T>
		PrintAny print(T const& aa) { return PrintAny(pass_value(aa)); }

		struct PrintWithType
			: public Printer
		{
			PassByValue root;

			PrintWithType(PassByValue vv) : root(vv) {}

			std::ostream& _print(PassByValue value, std::ostream& out) const
			{
				auto print_sym = [&](Symbol& sym) -> std::ostream&
					{
						out << sym.name << ":";
						print_type(pass_value(wrap(&sym.scheme)), out);
						return out;
					};

				switch(value._tag)
					{
					case tag<Slice>::value:
						{
							auto ast = value.slice;
							out << "(";
							if(!ast.empty())
								{
									_print(pass_value(ast[0]), out);
									for(auto& vv : slice(ast, 1))
										{
											out << ' ';
											_print(pass_value(vv), out);
										}
								}
							return out << ")";
						}
					case tag<Bound>::value:
						{
							out << "[Bound";
							print_sym(*unwrap<Bound>(value).sym);
							return out << "]";
						}
					case tag<Symbol>::value:
						{
							return print_sym(unwrap<Symbol>(value));
						}
					case tag<CxxFunctor>::value:
						{
							auto fn = unwrap<CxxFunctor>(value);
							out << "#<c++-fn: " << fn.name;
							print_type(pass_value(fn.type),
							           out);
							return out << ">";
						}
					case tag<Lambda>::value:
						{
							auto metadata = unwrap<Lambda>(value).value;

							out << "#<fn: ";
							if(metadata != nullptr)
								{ print_type(pass_value(unwrap<Lambda>(value).value->return_type), out); }
							else
								{ out << "null"; }

							return out << ">";
						}
					case tag<Scheme>::value:
						{ return print_type(value, out); }
					default:
						return print_atom(value.any, out);
					}
			}
			virtual std::ostream& print(std::ostream& out) const override
			{ return _print(root, out); }
		};

		template<class T>
		PrintWithType with_type(T const& aa) { return PrintWithType(pass_value(aa)); }

		std::ostream& operator<<(std::ostream& out, const printer::Printer& p) { return p.print(out); }
	}

	std::ostream& dbg_any(Any vv)
	{ return cout << printer::print(vv) << endl; }

	std::ostream& dbg_pbv(PassByValue value)
	{ return dbg_any(value.any); }

	std::ostream& dbg_ast(Ast const& vv)
	{ return cout << printer::print(vv) << endl; }

	std::ostream& dbg_slice(Slice const& vv)
	{ return cout << printer::print(vv) << endl; }

	std::ostream& dbg_type(PassByValue const& value)
	{ return printer::print_type(value, std::cout) << std::endl; }

	std::ostream& dbg_with_type(PassByValue const& value)
	{ return std::cout << printer::PrintWithType(value) << std::endl; }
}


#endif
