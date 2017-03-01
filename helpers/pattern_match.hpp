#ifndef ATL_HELPER_PATTERN_MATCH_HPP
#define ATL_HELPER_PATTERN_MATCH_HPP

#include <helpers/misc.hpp>
#include <gc/marked.hpp>
#include <print.hpp>

namespace atl
{
	namespace pattern_match
	{
		using make_type::tt;

		struct Match;
		typedef std::function<bool (Match&, Ast::iterator&)> Matcher;

		struct Match
		{

			/* If we're given a raw string, store the type we see at
			 * that position when doing a match, and make sure all
			 * occurances of that string are of the same type (ie for
			 * ast("a", "b", "a"), (1 "foo" 1) would match, but (1 1
			 * "foo") would not)*/
			std::map<std::string, Type> seen_types;
			virtual bool operator()(std::string const& ss, Ast::iterator& tt)
			{
				auto found = seen_types.find(ss);
				if(found != seen_types.end())
					{ return found->second == typify(*tt); }
				else
					{
						seen_types.emplace(ss, typify(*tt));
						return true;
					}
			}

			virtual bool operator()(long expected_tag, Ast::iterator& tt)
			{ return static_cast<Type::value_type>(expected_tag) == tt.tag(); }

			bool operator()(Any const& value, Ast::iterator& tt)
			{ return value == *tt; }

			bool operator()(Matcher const& fn, Ast::iterator& tt)
			{ return fn(*this, tt); }
		};

		/* Like Match, but assumes an unwrapped string is matching a
		 * particular symbol and an int is referring to a Fixnum.
		 */
		struct LiteralMatch
			: public Match
		{
			virtual bool operator()(std::string const& ss, Ast::iterator& tt) override
			{ return tt.is<Symbol>() && unwrap<Symbol>(*tt).name == ss; }

			virtual bool operator()(long fixnum, Ast::iterator& tt) override
			{ return tt.is<Fixnum>() && unwrap<Fixnum>(*tt).value == fixnum; }
		};

		struct _DispatchMatcher
		{
			typedef Ast::iterator iterator;
			iterator itr, end;
			Match& run;

			_DispatchMatcher(Match& run_, Ast::Subex subex)
				: itr(subex.begin())
				, end(subex.end())
				, run(run_)
			{}

			template<class Test>
			bool operator()(Test const& test)
			{
				if(itr == end) { return false; }
				bool value = run(test, itr);
				++itr;
				return value;
			}
		};

		Matcher capture(Ast::Subex& subex)
		{
			auto hold = &subex;
			return [hold](Match&, Ast::iterator& itr)
				{
					if(itr.is<Ast>())
						{
							*hold = itr.subex();
							return true;
						}
					return false;
				};
		}

		Matcher capture(Ast::iterator& capturing)
		{
			auto pointer_to_capturing = &capturing;
			return [pointer_to_capturing](Match&, Ast::iterator& itr)
				{
					if(itr.is<Ast>())
						{
							*pointer_to_capturing = itr;
							return true;
						}
					return false;
				};
		}

		template<class T>
		Matcher capture(T& passed_hold)
		{
			static_assert(!(std::is_same<T, Ast>::value || std::is_same<T, Ast>::value),
			              "Capturing Ast's isn't safe.  Capture an Ast::Subex.");
			auto hold = &passed_hold;
			return [hold](Match&, Ast::iterator& itr)
				{
					if(itr.is<T>())
						{
							*hold = unwrap<T>(*itr);
							return true;
						}
					return false;
				};
		}

		template<class T>
		Matcher capture_ptr(T const*& passed_hold)
		{
			auto hold = &passed_hold;
			return [hold](Match&, Ast::iterator& itr)
				{
					if(itr.is<T>())
						{
							*hold = &unwrap<T>(*itr);
							return true;
						}
					return false;
				};
		}

		template<class ... Args>
		Matcher ast(Args ... args)
		{
			auto tup = std::make_tuple(args...);
			return [tup](Match& match, Ast::iterator& itr) -> bool
				{
					if(itr.is<Ast>())
						{
							auto run = _DispatchMatcher(match, itr.subex());
							return and_tuple(run, tup) && (run.itr == run.end);
						}
					else { return false; }

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

		bool match(Matcher const& pattern, Marked<Ast>& expr)
		{
			Match context;
			auto itr = expr->self_iterator();
			return pattern(context, itr);
		}

		bool match(Matcher const& pattern, Ast::iterator expr)
		{
			Match context;
			return pattern(context, expr);
		}

		// Matcher's only work with Ast's (or their iterators), not
		// AstData or Any so if we're given an Any, check if it is
		// an Ast first.
		bool match(Matcher const& pattern, Any& expr)
		{
			Match context;
			if(is<Ast>(expr))
				{
					auto itr = unwrap<Ast>(expr).self_iterator();
					return pattern(context, itr);
				}
			return false;
		}

		template<class T>
		bool match(T const& pattern, Ast::iterator expr)
		{
			Match matcher;
			return matcher(pattern, expr);
		}

		bool literal_match(Matcher const& pattern, Ast::iterator expr)
		{
			LiteralMatch context;
			return pattern(context, expr);
		}

		template<class T>
		bool literal_match(T const& pattern, Ast::iterator expr)
		{
			LiteralMatch matcher;
			return matcher(pattern, expr);
		}
	}

}

#endif
