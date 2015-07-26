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
    // A simple minded parser, this just transforms a string into an Ast.  There are a couple of reserved symbols:
    //   '(' ')'         : Open and close an Ast branch
    //   DELIM '\\' DELIM : After much debate, I'm making Lambda a reserved word.  In principle, I'd like everything to be a
    //                     usable symbol at local scope.  This may still change (should I be able to do something like
    //                     `(def foo \ )  ((foo (a b) (+ a b)) 1 2)`)?
    //   DELIM 'let' DELIM: Simalar to lambda, if I'm going to getting Hindly Milner with polymorphic let is going to be much easier
    //                      If I can get a tagged type.
    //   DELIM '\''      : 'n expands to (quote n) (should probably be a macro).  Can still be used as part of a variable name (ie x and x' are both
    //                     valid symbols).
    //   '\"'            : starts and ends a string literal
    //   DELIM 0..9 DELIM: a number (a number must be all digits ie 124567, possibly with a decimal point.  If there is a non-digit ie 12345a, it
    //                     is a symbol.  hex/octal/binary representations are not a thing ATM).
    class ParseString
    {
    private:
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
	void parse(GC::DynamicVector &vec, Itr &&itr, Itr &&end) {
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
                            auto quote = vec.push_seq<Ast>();
                            vec[0] = wrap<Quote>();
                            ++vec;

                            ++itr;
                            parse(vec, itr, end);
                            quote->end_at(vec.end());
                            return;
                        }

                    case '(':
                        {		/* sub expression */
                            ++itr;
                            if(*itr == ')') {
                                auto null = vec.push_seq<Ast>();
                                null->end_at(vec.end());
                                return;
                            }

                            auto ast = vec.push_seq<Ast>();
                            while(*itr != ')')
                                {
                                    if(itr == end)
                                        throw UnbalancedParens
                                            (to_string(_line)
                                             .append(": error: unbalanced parens"));
                                    parse(vec, itr, end);
                                }

                            ++itr;
                            ast->end_at(vec.end());
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
                            vec[0] = Any(tag<String>::value, str);
                            ++vec;
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
                                    vec[0] = wrap( atoi(scratch.c_str()) );
                                }
                            else if(scratch == "\\") // Lambda
                                {
                                    vec[0] = wrap<Lambda>();
                                }
                            else if(scratch == "let")
                                {
                                    vec[0] = wrap<Let>();
                                }
                            else
                                {
                                    vec[0] = _gc.amake<Symbol>(scratch);
                                }

                            ++vec;
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
	Any& string_(const std::string& input) {
	    auto vec = _gc.dynamic_seq();
	    **vec = nil<Null>::value();
	    parse(*vec, input.begin(), input.end());

	    return *vec->begin();
	}

	/* parse one S-expression from a stream into an ast */
	Any& stream(istream &stream) {
	    auto initial_flags = stream.flags();
	    noskipws(stream);

	    auto vec = _gc.dynamic_seq();
	    **vec = nil<Null>::value();

	    parse(*vec, istream_iterator<char>(stream), istream_iterator<char>() );

	    stream.flags(initial_flags);
	    return *vec->begin();
	}

	void reset_line_number() { _line = 1; }
    };
    const std::string ParseString::_ws = " \n\t";
    const std::string ParseString::delim = "()\" \n\t";
}
#endif
