#ifndef ATL_PARSER_HPP
#define ATL_PARSER_HPP
/**
 * @file /home/ryan/programming/atl/parser.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jun 29, 2013
 */

#include <iterator>
#include <sstream>

#include <string>
#include <vector>
#include <iostream>
#include <functional>

#include <iterator>

#include "./exception.hpp"
#include "./type.hpp"
#include "./conversion.hpp"
#include "./type_testing.hpp"
#include "./helpers.hpp"
#include "./print.hpp"


namespace atl
{
	// A simple minded parser, this just transforms a string into an
	// Ast.  There are a couple of reserved symbols:
	//
	//   '(' ')'         : Open and close an Ast branch
	//
	//   DELIM '\'' : 'n expands to (quote n) (should probably be a
	//                 macro).  Can still be used as part of a
	//                 variable name (ie x and x' are both valid
	//                 symbols).
	//
	//   '\"' : starts and ends a string literal
	//
	//   DELIM 0..9 DELIM: a number (a number must be all digits ie
	//                     124567, possibly with a decimal point.  If
	//                     there is a non-digit ie 12345a, it is a
	//                     symbol.  hex/octal/binary representations
	//                     are not a thing ATM).
	//  ';' : comments out to the end of line
	class ParseString
	{
	private:
		template<class Itr>
		void skip_ws_and_comments(Itr &itr, Itr const& end)
		{
			for(; itr != end; ++itr)
				{
					if(*itr == ';')
						for(; (itr != end) && (*itr != '\n'); ++itr);
					else if(!std::isspace(*itr))
						return;

					if(*itr == '\n')
						++_line;
				}
		}


		GC &_gc;
		unsigned long _line;

		static const string delim;	/* symbol deliminator */
		static const string _ws; 	/* white space */
		string scratch;

		inline bool digit(char c) { return ((c >= '0') && (c <= '9')); }
		bool string_is_number(const string& str) {
			return (digit(str[0]) || ((str[0] == '-') && (str.length() > 1) && digit(str[1])));
		}

		template<class Itr>
		void parse(AstSubstrate &vec, Itr &&itr, Itr &&end) {
			for(; itr != end; ++itr) {
				switch(*itr)
					{
					case '\n':
						++_line;
					case ' ':		/* whitespace */
					case '\t':
						continue;

					case ';': 		/* comment */
						for(; itr != end && *itr != '\n'; ++itr);
						if(*itr == '\n') ++_line;
						continue;

					case '\'':
						{		/* quote */
							auto quote = push_nested_ast(vec);
							vec.push_back(wrap<Quote>());

							++itr;
							parse(vec, itr, end);
							quote.end_ast();
							return;
						}

					case '(':
						{		/* sub expression */
							++itr;
							if(*itr == ')') {
								push_nested_ast(vec);
								return;
							}

							auto ast = push_nested_ast(vec);
							while(*itr != ')')
								{
									if(itr == end)
										throw UnbalancedParens
											(to_string(_line)
											 .append(": error: unbalanced parens"));
									parse(vec, itr, end);
								}

							++itr;
							ast.end_ast();
							return;
						}

					case ')': return;

					case '"':
						{		/* string */
							std::string *str = reinterpret_cast<std::string*>(_gc.make<String>());

							for(++itr; *itr != '"'; ++itr)
								{
									if(itr == end) throw to_string(_line)
										               .append("string not terminated.");
									if(*itr == '\n') ++_line;

									str->push_back(*itr);
								}

							++itr;
							vec.emplace_back(tag<String>::value, str);
							return;
						}

					default:
						{
							scratch.clear();

							for(; (itr != end) && (delim.find(*itr) == string::npos); ++itr)
								scratch.push_back(*itr);

							if((itr != end) && (*itr == '\n')) ++_line;

							if(string_is_number(scratch))
								{
									vec.push_back(wrap(atoi(scratch.c_str())));
								}
							else
								{
									vec.push_back(_gc.amake<Symbol>(scratch));
								}

							return;
						}
					}
			}
			return;
		}

	public:
		ParseString(GC &gc) : _gc(gc) {
			_line = 1;
			//_gc.mark_callback( [this](GC &gc) { _gc.mark(_mark); });
		}


		/* parse one S-expression from a string into an ast */
		PassByValue string_(const std::string& input)
		{
			auto& vec = _gc.sequence();

			auto itr = input.begin(),
				end = input.end();
			parse(vec, itr, end);

			// Check that there was just one expression in our string
			while(itr != input.end() && std::isspace(*itr)) ++itr;
			if(itr != input.end())
				throw MultiExpressionString(std::string("More than one expression in `")
				                            .append(input)
				                            .append("`"));

			return PassByValue(*vec.begin());
		}

		/* parse one S-expression from a stream into an ast */
		PassByValue stream(istream &stream)
		{
			auto initial_flags = stream.flags();
			noskipws(stream);

			auto& vec = _gc.sequence();

			auto itr = istreambuf_iterator<char>(stream),
				end = istreambuf_iterator<char>();
			parse(vec, itr, end);

			// Swallow any following whitepace or comments so the caller
			// of the parser doesn't have to check.
			skip_ws_and_comments(itr, end);

			stream.flags(initial_flags);
			return PassByValue(*vec.begin());
		}

		void reset_line_number() { _line = 1; }
	};
	const std::string ParseString::_ws = " \n\t";
	const std::string ParseString::delim = "()\" \n\t";
}
#endif
