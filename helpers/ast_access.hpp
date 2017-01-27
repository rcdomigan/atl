#ifndef ATL_AST_ACCESS_HPP
#define ATL_AST_ACCESS_HPP

#include <utility.hpp>
#include <type_testing.hpp>
#include <conversion.hpp>

#include <cassert>

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
							if(Ast(const_cast<AstData*>(reinterpret_cast<AstData const*>(aa.value)))
							   != Ast(const_cast<AstData*>(reinterpret_cast<AstData const*>(bb.value))))
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
}

#endif
