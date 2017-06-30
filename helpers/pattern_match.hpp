#ifndef ATL_HELPER_PATTERN_MATCH_HPP
#define ATL_HELPER_PATTERN_MATCH_HPP

#include <atl/helpers/misc.hpp>
#include <atl/helpers/ast_access.hpp>
#include <atl/gc/marked.hpp>
#include <atl/print.hpp>

#include <sstream>

namespace atl
{
	namespace pattern_match
	{
		using make_type::tt;

		struct Match;
		typedef std::function<bool (Match&, WalkAst&)> Matcher;

		struct Match
		{

			/* If we're given a raw string, store the type we see at
			 * that position when doing a match, and make sure all
			 * occurances of that string are of the same type (ie for
			 * ast("a", "b", "a"), (1 "foo" 1) would match, but (1 1
			 * "foo") would not)*/
			std::map<std::string, Type> seen_types;
			virtual bool operator()(std::string const& ss, WalkAst& tt)
			{
				auto found = seen_types.find(ss);
				if(found != seen_types.end())
					{ return found->second == typify(tt.value()); }
				else
					{
						seen_types.emplace(ss, typify(tt.value()));
						return true;
					}
			}

			virtual bool operator()(long expected_tag, WalkAst& tt)
			{ return static_cast<Type::value_type>(expected_tag) == tt.tag(); }

			virtual bool operator()(Any const& value, WalkAst& tt)
			{ return value == tt.value(); }

			virtual bool operator()(Matcher const& fn, WalkAst& tt)
			{ return fn(*this, tt); }

			virtual bool at_end(WalkAst& tt)
			{ return tt.at_end(); }

			virtual bool is_subex(WalkAst& tt)
			{ return tt.is_subex(); }
		};

		struct MatchOrThrow : public Match
		{
			bool lob(bool input, WalkAst& walker)
			{
				if(input) { return true; }

				std::stringstream output;
				output << printer::walker(walker) << std::flush;

				throw PatternMatchFailed
					(std::string("Match failed got: ").append(output.str()));
			}

			std::map<std::string, Type> seen_types;
			virtual bool operator()(std::string const& ss, WalkAst& tt) override
			{ return lob(Match::operator()(ss, tt), tt); }

			virtual bool operator()(long expected_tag, WalkAst& tt) override
			{ return lob(Match::operator()(expected_tag, tt), tt); }

			virtual bool operator()(Any const& value, WalkAst& tt) override
			{ return lob(Match::operator()(value, tt), tt); }

			virtual bool operator()(Matcher const& fn, WalkAst& tt) override
			{ return lob(Match::operator()(fn, tt), tt); }

			virtual bool at_end(WalkAst& tt) override
			{ return lob(Match::at_end(tt), tt); }

			virtual bool is_subex(WalkAst& tt)
			{ return lob(Match::is_subex(tt), tt); }

		};

		/* Like Match, but assumes an unwrapped string is matching a
		 * particular symbol and an int is referring to a Fixnum.
		 */
		struct LiteralMatch
			: public Match
		{
			virtual bool operator()(std::string const& ss, WalkAst& tt) override
			{ return is<Symbol>(tt.tag()) && unwrap<Symbol>(tt.value()).name == ss; }

			virtual bool operator()(long fixnum, WalkAst& tt) override
			{ return is<Fixnum>(tt.tag()) && unwrap<Fixnum>(tt.value()).value == fixnum; }
		};

		struct _DispatchMatcher
		{
			WalkAst& walker;
			Match& run;

			_DispatchMatcher(Match& run_, WalkAst& walker_)
				: walker(walker_)
				, run(run_)
			{}

			template<class Test>
			bool operator()(Test const& test)
			{
				if(walker.has_value())
					{
						bool value = run(test, walker);
						walker.next();
						return value;
					}
				return false;
			}
		};

		Matcher capture(Ast::Subex& subex)
		{
			auto hold = &subex;
			return [hold](Match&, WalkAst& walker)
				{
					if(walker.is_subex())
						{
							*hold = walker.subex();
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
			return [hold](Match&, WalkAst& walker)
				{
					auto value = walker.value();
					if(is<T>(value))
						{
							*hold = unwrap<T>(value);
							return true;
						}
					return false;
				};
		}

		template<class T>
		Matcher capture_ptr(T const*& passed_hold)
		{
			static_assert(!is_reinterperable<T>::value,
			              "reinterperable types are passed by value in this context");
			auto hold = &passed_hold;
			return [hold](Match&, WalkAst& walker)
				{
					auto value = walker.value();
					if(is<T>(value))
						{
							*hold = &unwrap<T>(value);
							return true;
						}
					return false;
				};
		}

		template<class T>
		Matcher capture_ptr(T *& passed_hold)
		{
			static_assert(!is_reinterperable<T>::value,
			              "reinterperable types are passed by value in this context");
			auto hold = &passed_hold;
			return [hold](Match&, WalkAst& walker)
				{
					auto value = walker.value();
					if(is<T>(value))
						{
							*hold = &unwrap<T>(value);
							return true;
						}
					return false;
				};
		}

		// Assumes the walker is supposed to be _on_ an Ast (ie
		// walker.is_subex() is true).
		template<class ... Args>
		Matcher ast(Args ... args)
		{
			auto tup = std::make_tuple(args...);
			return [tup](Match& match, WalkAst& walker) -> bool
				{
					if(match.is_subex(walker))
						{
							auto subex = walker.walk_subex();
							auto run = _DispatchMatcher(match, walker);
							return and_tuple(run, tup) && match.at_end(walker);
						}
					else { return false; }

				};
		}

		// assumes we're matching based on an Ast we're already
		// walking _in_.
		template<class ... Args>
		Matcher rest(Args ... args)
		{
			auto tup = std::make_tuple(args...);
			return [tup](Match& match, WalkAst& walker) -> bool
				{
					auto run = _DispatchMatcher(match, walker);
					return and_tuple(run, tup) && match.at_end(walker);
				};
		}

		template<class ... Args>
		Matcher rest_begins(Args ... args)
		{
			auto tup = std::make_tuple(args...);
			return [tup](Match& match, WalkAst& walker) -> bool
				{
					auto run = _DispatchMatcher(match, walker);
					return and_tuple(run, tup);
				};
		}

		template<class ... Args>
		Matcher ast_begins(Args ... args)
		{
			auto tup = std::make_tuple(args...);
			return [tup](Match& match, WalkAst& walker) -> bool
				{
					if(match.is_subex(walker))
						{
							auto subex = walker.walk_subex();
							auto run = _DispatchMatcher(match, walker);
							return and_tuple(run, tup);
						}
					else { return false; }
				};
		}

		/* I have a bunch of tests that want to match a function type;
		 * add a quicker way to build those
		 */
		template<class T>
		Matcher fnt(T const& arg0)
		{ return ast(function_constructor(), arg0); }

		template<class T, class U>
		Matcher fnt(T const& arg0, U const& arg1)
		{ return ast(function_constructor(), arg0, arg1); }

		template<class T, class ... Rest>
		Matcher fnt(T const& arg0, Rest ... rest)
		{ return ast(function_constructor(), arg0, fnt(rest...)); }

		template<class T>
		Matcher fnt_rest(T const& arg0)
		{ return rest(function_constructor(), arg0); }

		template<class T, class ... Rest>
		Matcher fnt_rest(T const& arg0, Rest ... args)
		{ return rest(function_constructor(), arg0, fnt(args...)); }
		template<class T, class U>

		Matcher fnt_rest(T const& arg0, U const& arg1)
		{ return rest(function_constructor(), arg0, arg1); }

		/* Helper for declaring asts of types (something that seems to come up a bit in tests). */
		template<class ... Args>
		Matcher types()
		{ return ast(tag<Args>::value...); }


		template<class T, class U>
		bool match(T const& pattern, U&& traversable);

 		namespace detail
		{
			template<class Context,
			         class Walker,
			         bool is_walker=std::is_base_of<WalkAst, Walker>::value>
			struct DoMatch;

			template<class Context, class Traversable>
			struct DoMatch<Context, Traversable, true>
			{
				static bool match(Matcher const& pattern, WalkAst& walker)
				{
					Context context;
					auto copy = walker.clone();

					if(pattern(context, *copy))
						{
							walker.reset(*copy);
							return true;
						}
					return false;
				}
			};

			template<class Context>
			struct DoMatch<Context, Any, false>
			{
				static bool match(Matcher const& pattern, Any& expr)
				{
					if(is<Ast>(expr))
						{
							auto walker = walk(unwrap<Ast>(expr));
							return DoMatch<Context, WalkAstIterator>::match(pattern, walker);
						}
					return false;
				}
			};

			template<class Context, class Traversable>
			struct DoMatch<Context, Traversable, false>
			{
				static bool match(Matcher const& pattern, Traversable& expr)
				{
					auto walker = walk(expr);
					return DoMatch<Context, WalkAstIterator>::match(pattern, walker);
				}
			};
		}

		template<class T>
		bool match(Matcher const& pattern, T& traversable)
		{
			return detail::DoMatch
				<Match, typename std::remove_reference<T>::type>::match
				(pattern, traversable);
		}

		template<class T>
		bool match(Matcher const& pattern, T&& traversable)
		{ return match(pattern, traversable); }

		bool match(Matcher const& pattern, Ast::iterator& itr)
		{
			if(itr.is<Ast>())
				{ return match(pattern, subex(itr)); }
			return false;
		}

		template<class T>
		bool literal_match(Matcher const& pattern, T& traversable)
		{
			return detail::DoMatch
				<LiteralMatch, typename std::remove_reference<T>::type>::match
				(pattern, traversable);
		}

		template<class T>
		bool literal_match(Matcher const& pattern, T&& traversable)
		{ return literal_match(pattern, traversable); }

		template<class T>
		bool throwing_match(Matcher const& pattern, T& traversable)
		{
			return detail::DoMatch
				<MatchOrThrow, typename std::remove_reference<T>::type>::match
				(pattern, traversable);
		}

		template<class T>
		bool throwing_match(Matcher const& pattern, T&& traversable)
		{ return throwing_match(pattern, traversable); }
	}
}

#endif
