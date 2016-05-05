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
		struct Printer
		{
			virtual std::ostream& print(std::ostream&) const = 0;
		};

		typedef ::Range<typename Ast::const_iterator> Range;

		struct PrintRange : public Printer
		{
			Range _range;
			char _delim_open, _delim_close;

			PrintRange(Range const& range, char delim_open, char delim_close)
				: _range(range),
				  _delim_open(delim_open),
				  _delim_close(delim_close)
			{}

			virtual std::ostream& print(std::ostream&) const;
		};

		struct PrintAny : public Printer
		{
			const Any &a;
			PrintAny(const Any &a_) : a(a_) {}
			virtual std::ostream& print(std::ostream&) const;
		};


		PrintRange range(Range const& rr, char open='(', char close=')')
		{ return PrintRange(rr, open, close); }

		PrintAny any(Any const& aa) { return PrintAny(aa); }

		std::ostream& operator<<(std::ostream& out, const printer::Printer& p) { return p.print(out); }

		std::ostream& PrintRange::print(std::ostream& out) const
		{
			if(_range.empty())
				return out << _delim_open << _delim_close << endl;
			else {
				out << _delim_open << printer::any(_range[0]) << flush;
				for(auto& vv : slice(_range, 1)) out << " " << printer::any(vv);
				return out << _delim_close;
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
					return out << "'" << unwrap<string>(any);
				case tag<Bound>::value:
					return out << "#<Bound " << unwrap<Bound>(any).value->name << ">";
				case tag<Parameter>::value:
					return out << "#<Parameter " << unwrap<Parameter>(any).value << ">";
				case tag<ClosureParameter>::value:
					return out << "#<ClosureParameter" << unwrap<ClosureParameter>(any).value << ">";
				case tag<Type>::value:
					return out << "#{" << unwrap<Type>(any).value << "}";
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
					return out << unwrap<CxxFunctor>(any)._name;
				case tag<Pointer>::value:
					{
						if(any.value)
							return out << "#<Pointer-" << hex << (reinterpret_cast<long>(any.value) & (256 - 1)) << ">";
						else
							return out << "#<Pointer-NULL>";
					}
				case tag<DefProcedure>::value:
					{
						out << "#<DefProcedure" << flush;
						auto closure = unwrap<DefProcedure>(any).closure;
						if(!closure.empty())
							{
								out << " (" << closure.size() << " free vars)";
							}
						return out << ">" << flush;;
					}
				default:
					return out << "#<" << type_name(any._tag) << ">";
				}
		}

		std::ostream& PrintAny::print(std::ostream& out) const
		{
			using namespace std;
			switch(a._tag)
				{
				case tag<Ast>::value:
					return out << range(make_range(unwrap<Ast>(a)));
				case tag<AstData>::value:
					return out << range(make_range(unwrap<AstData>(a)));
				case tag<Slice>::value:
					return out << range(make_range(unwrap<Slice>(a)), '[', ']');
				case tag<DefProcedure>::value:
					{
						out << "#<DefProcedure" << flush;
						auto closure = unwrap<DefProcedure>(a).closure;
						if(!closure.empty())
							{
								out << " (" << closure.size() << " free vars)";
							}
						return out << ">" << flush;;
					}
				default:
					return print_atom(a, out);
				}
		}
	}

	void dbg_any(Any vv)
	{ cout << printer::any(vv) << endl; }

	void dbg_pbv(PassByValue value)
	{ return dbg_any(*value.any); }

	void dbg_ast(Ast const& vv)
	{ cout << printer::range(make_range(vv)) << endl; }

	std::ostream& print_type(PassByValue const& value, std::ostream& out)
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
				{ return out << "#<Type: " << unwrap<Type>(value).value << ">"; }
			default:
				return out << type_name(value._tag);
			}
	}
	void dbg_type(PassByValue const& value)
	{ print_type(value, std::cout) << std::endl; }

	std::ostream& print_with_type(PassByValue value, std::ostream& out)
	{
		auto print_sym = [&](Symbol& sym) -> std::ostream&
			{
				out << "['" << sym.name << ":";
				print_type(pass_value(wrap(&sym.scheme)), out);
				return out << "]";
			};

		switch(value._tag)
			{
			case tag<Slice>::value:
				{
					auto ast = unwrap<Slice>(value);
					out << "(";
					if(!ast.empty())
						{
							print_with_type(ast[0], out);
							for(auto& vv : slice(ast, 1))
								{
									out << ' ';
									print_with_type(vv, out);
								}
						}
					return out << ")";
				}
			case tag<Bound>::value:
				{
					cout << "[Bound";
					print_sym(*unwrap<Bound>(value).value);
					return cout << "]";
				}
			case tag<Symbol>::value:
				{
					return print_sym(unwrap<Symbol>(value));
				}
			default:
				return printer::print_atom(*value.any, out);
			}
	}
	void dbg_with_type(PassByValue const& value)
	{ print_with_type(value, std::cout) << std::endl; }
}


#endif
