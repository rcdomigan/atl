#ifndef ATL_AST_ACCESS_HPP
#define ATL_AST_ACCESS_HPP

#include <utility.hpp>
#include <is.hpp>
#include <wrap.hpp>
#include <gc/marked.hpp>

#include <print.hpp>

#include <cassert>
#include <memory>

/* Small Ast helpers with minimal external dependencies */
namespace atl
{
	struct AstSubscripter
	{
		Any value;
		AstSubscripter(Any const& in)
			: value(in)
		{ assert(!is<AstData>(in)); }
		AstSubscripter(Ast& value_) : value(wrap(value_)) {}
		AstSubscripter() = delete;

		AstSubscripter operator[](off_t pos)
		{ return AstSubscripter(unwrap<Ast>(value)[pos]); }
	};

	template<class T>
	AstSubscripter subscripter(T&& ast)
	{ return AstSubscripter(ast); }

	AstSubscripter inner_subscript(AstSubscripter ast, size_t pos)
	{ return ast[pos]; }

	template<class ... Args>
	AstSubscripter inner_subscript(AstSubscripter ast, size_t pos, Args ... rest)
	{ return inner_subscript(ast[pos], rest...); }

	template<class T, class ... Args>
	Any subscript(T&& ast, Args ... args)
	{ return inner_subscript(ast, args...).value; }

	Range<Ast::flat_iterator> flat_ast(Ast& ast)
	{ return make_range(ast.flat_begin(), ast.flat_end()); }

	bool operator!=(Ast const& aa_tree, Ast const& bb_tree);

	bool operator==(Ast const& aa_tree, Ast const& bb_tree)
	{
		auto aa = aa_tree.begin(),
			bb = bb_tree.begin();

		while((aa != aa_tree.end()) && (bb != bb_tree.end()))
			{
				if(aa->_tag != bb->_tag)
					{ return false; }

				switch(aa->_tag)
					{
					case tag<AstData>::value:
						{
							if(Ast(const_cast<AstData*>(reinterpret_cast<AstData const*>(aa.pointer())))
							   != Ast(const_cast<AstData*>(reinterpret_cast<AstData const*>(bb.pointer()))))
								{ return false; }
							break;
						}
					case tag<Ast>::value:
						{
							if(reinterpret_cast<Ast const&>(aa)
							   != reinterpret_cast<Ast const&>(bb))
								{ return false; }
							break;
						}
					case tag<Symbol>::value:
						{
							if(reinterpret_cast<Symbol*>(aa->value)->name
							   != reinterpret_cast<Symbol*>(bb->value)->name)
								{ return false; }
							break;
						}
					default:
						if(*bb != *aa)
							{ return false; }
					}

				++bb;
				++aa;
			}

		if((bb != bb_tree.end()) || (aa != aa_tree.end()))
			{ return false; }
		return true;
	}

	bool operator!=(Ast const& aa_tree, Ast const& bb_tree)
	{ return !(aa_tree == bb_tree); }


	// Generic interface for traversing an Ast.  Derived classes can
	// do whatever transforms make sense.
	struct WalkAst
	{
		typedef std::unique_ptr<WalkAst> WalkerPtr;

		virtual ~WalkAst() {}

		virtual tag_t tag()=0;
		virtual Any value()=0;

		virtual bool has_value()=0;
		bool at_end() { return !has_value(); }

		virtual bool at_begin()=0;

		virtual void next()=0;

		virtual bool is_subex()=0;

		virtual void down()=0;

		virtual void up()=0;

		struct SubexRAII
		{
			WalkAst& walker;
			SubexRAII(WalkAst& walker_)
				: walker(walker_)
			{ walker.down(); }

			~SubexRAII()
			{ walker.up(); }
		};

		SubexRAII walk_subex()
		{ return SubexRAII(*this); }

		virtual WalkerPtr clone() const=0;

		// Set the state of this to match the state of other.  Raise
		// WrongType if other's static type is different.
		virtual void reset(WalkAst const& other)=0;

		template<class T>
		WalkerPtr _clone(T const* ptr) const { return WalkerPtr(new T(*ptr)); }

		// Returns an iterator which indicates the walkers current
		// position in the Ast.
		virtual Ast::iterator here()=0;

		// Returns the subex at the position we have walked to.
		// Undefined if we are not on a sub-expression.
		virtual Ast::Subex subex()=0;
	};

	struct WalkAstIterator
		: public WalkAst
	{
		Ast::iterator begin, itr, end;
		std::vector<Ast::Subex> stack;

		WalkAstIterator(Ast::iterator const& begin_, Ast::iterator const& end_)
			: begin(begin_), itr(begin_), end(end_)
		{}

		virtual ~WalkAstIterator() override {}

		virtual bool has_value() override { return itr != end; }

		virtual bool at_begin() override { return itr == begin; }

		virtual Any value() override
		{
			if(!has_value())
				{
					throw RangeError
						("WalkAstIterator can't get value past this->end");
				}
			return *itr;
		}
		virtual tag_t tag() override { return itr.tag(); }

		virtual void next() override { ++itr; }

		virtual bool is_subex() override
		{ return itr.is<AstData>() || itr.is<Ast>(); }

		virtual void down() override
		{
			stack.emplace_back(itr, end);
			std::tie(itr, end) = atl::subex(itr);
		}

		virtual void up() override
		{
			std::tie(itr, end) = stack.back();
			stack.pop_back();
		}

		Ast::Subex _range()
		{
			auto any = value();
			switch(any._tag)
				{
				case atl::tag<Ast>::value:
					return unwrap<Ast>(any).subex();
					break;
				case atl::tag<AstData>::value:
					return Ast(&explicit_unwrap<AstData>(any)).subex();
					break;
				default:
					throw WrongTypeError("WalkAstIterator not on a sub-expression");
				}
		}

		virtual WalkerPtr clone() const override { return _clone(this); }

		virtual void reset(WalkAst const& raw_other) override
		{
			auto const& other = dynamic_cast<WalkAstIterator const&>(raw_other);
			itr = other.itr;
			end = other.end;
		}

		virtual Ast::iterator here() override { return itr; }
		virtual Ast::Subex subex() override { return atl::subex(itr); }
	};

	WalkAstIterator walk(Ast::iterator const& itr, Ast::iterator const& end)
	{ return WalkAstIterator(itr, end); }

	WalkAstIterator walk(Ast& ast)
	{ return WalkAstIterator(ast.begin(), ast.end()); }

	WalkAstIterator walk(Marked<Ast>& ast)
	{ return walk(*ast); }

	WalkAstIterator walk(Ast::Subex& ast)
	{ return walk(std::get<0>(ast), std::get<1>(ast)); }

	WalkAstIterator walk(Ast::Subex&& ast) { return walk(ast); }

	namespace printer
	{
		struct PrintWalker : public Printer
		{
			WalkAst::WalkerPtr walker;

			PrintWalker(WalkAst const& walker_)
				: walker(walker_.clone())
			{}

			void _dispatch(WalkAst& walker, std::ostream& out) const
			{
				if(walker.is_subex())
					{
						auto inner = walker.walk_subex();
						_ast(walker, out);
					}
				else
					{ print_atom(walker.value(), out); }
			}

			void _ast(WalkAst& walker, std::ostream& out) const
			{
				out << "(";
				if(walker.has_value())
					{
						_dispatch(walker, out);
						walker.next();
						for(; walker.has_value(); walker.next())
							{
								out << " ";
								_dispatch(walker, out);
							}
					}
				out << ")";
			}

			virtual std::ostream& print(std::ostream& out) const override
			{
				auto clone = walker->clone();
				_ast(*clone, out);
				return out;
			}
		};

		PrintWalker walker(WalkAst const& walker)
		{ return PrintWalker(walker); }
	}
}

#endif
