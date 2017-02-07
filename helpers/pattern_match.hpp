#ifndef ATL_HELPER_PATTERN_MATCH_HPP
#define ATL_HELPER_PATTERN_MATCH_HPP

#include <helpers/misc.hpp>
#include <print.hpp>

namespace atl
{
	namespace pattern_match
	{
		using make_type::tt;

		struct Match;
		typedef std::function<bool (Match&, Any const&)> Matcher;

		struct Match
		{

			/* If we're given a raw string, store the type we see at
			 * that position when doing a match, and make sure all
			 * occurances of that string are of the same type (ie for
			 * ast("a", "b", "a"), (1 "foo" 1) would match, but (1 1
			 * "foo") would not)*/
			std::map<std::string, Type> seen_types;
			virtual bool operator()(std::string const& ss, Any const& tt)
			{
				auto found = seen_types.find(ss);
				if(found != seen_types.end())
					{ return found->second == typify(tt); }
				else
					{
						seen_types.emplace(ss, typify(tt));
						return true;
					}
			}

			virtual bool operator()(long expected_tag, Any const& tt)
			{ return static_cast<Type::value_type>(expected_tag) == tt._tag; }

			bool operator()(Any const& value, Any const& tt)
			{ return value == tt; }

			bool operator()(Matcher const& fn, Any const& tt)
			{ return fn(*this, tt); }
		};

		/* Like Match, but assumes an unwrapped string is matching a
		 * particular symbol and an int is referring to a Fixnum.
		 */
		struct LiteralMatch
			: public Match
		{
			virtual bool operator()(std::string const& ss, Any const& tt) override
			{ return is<Symbol>(tt) && unwrap<Symbol>(tt).name == ss; }

			virtual bool operator()(long fixnum, Any const& tt) override
			{ return is<Fixnum>(tt) && unwrap<Fixnum>(tt).value == fixnum; }
		};

		struct _DispatchMatcher
		{
			typedef Ast::const_iterator iterator;
			iterator itr, end;
			Match& run;

			_DispatchMatcher(Match& run_, iterator itr_, iterator end_)
				: itr(itr_)
				, end(end_)
				, run(run_)
			{}

			template<class Test>
			bool operator()(Test const& test)
			{
				if(itr == end) { return false; }
				bool value = run(test, *itr);
				++itr;
				return value;
			}
		};

		template<class T>
		Matcher capture(T& passed_hold)
		{
			auto hold = &passed_hold;
			return [hold](Match&, Any const& expr)
				{
					if(is<T>(expr))
						{
							*hold = unwrap<T>(expr);
							return true;
						}
					return false;
				};
		}

		template<class T>
		Matcher capture_ptr(T const*& passed_hold)
		{
			auto hold = &passed_hold;
			return [hold](Match&, Any const& expr)
				{
					if(is<T>(expr))
						{
							*hold = &unwrap<T>(expr);
							return true;
						}
					return false;
				};
		}

		template<class ... Args>
		Matcher ast(Args ... args)
		{
			auto tup = std::make_tuple(args...);
			return [tup](Match& match, Any const& expr) -> bool
				{
					Ast::const_iterator begin, end;

					switch(expr._tag)
						{
						case tag<Ast>::value:
							{
								auto const& ast = reinterpret_cast<Ast const&>(expr);
								begin = ast.begin();
								end = ast.end();
								break;
							}
						default:
							return false;
						}

					auto run = _DispatchMatcher(match, begin, end);
					return and_tuple(run, tup) && (run.itr == run.end);
				};
		}

		/* I have a bunch of tests that want to match a function type;
		 * add a quicker way to build those
		 */
		template<class T>
		Matcher fnt(T const& arg0)
		{ return ast(wrap<Type>(tag<FunctionConstructor>::value), arg0); }

		template<class T, class U>
		Matcher fnt(T const& arg0, U const& arg1)
		{ return ast(wrap<Type>(tag<FunctionConstructor>::value), arg0, arg1); }

		template<class T, class ... Rest>
		Matcher fnt(T const& arg0, Rest ... rest)
		{ return ast(wrap<Type>(tag<FunctionConstructor>::value), arg0, fnt(rest...)); }

		/* Helper for declaring asts of types (something that seems to come up a bit in tests). */
		template<class ... Args>
		Matcher types()
		{ return ast(tag<Args>::value...); }

		bool match(Matcher const& pattern, Any const& expr)
		{
			Match context;
			return pattern(context, expr);
		}

		template<class T>
		bool match(T const& pattern, Any const& expr)
		{
			Match matcher;
			return matcher(pattern, expr);
		}

		bool literal_match(Matcher const& pattern, Any const& expr)
		{
			LiteralMatch context;
			return pattern(context, expr);
		}

		template<class T>
		bool literal_match(T const& pattern, Any const& expr)
		{
			LiteralMatch matcher;
			return matcher(pattern, expr);
		}
	}

}

#endif
