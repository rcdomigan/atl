#ifndef RHAS_PASSES_HPP
#define RHAS_PASSES_HPP

/* The simplest way to do the compiler seems like doing a bunch of
   passes to build up the metadata, then run it through the final
   compiler to spit out byte code.  This file is for odds and ends
   passes (the type inference and lexical scoping passes currently
   have their own files).
   */

#include <type.hpp>
#include <helpers.hpp>

namespace atl
{
	size_t atom_tail_padding(Any const& any)
	{
		switch(any._tag)
			{
			case tag<Lambda>::value:
			case tag<CallLambda>::value:
				return reinterpret_cast<LambdaMetadata*>(any.value)->pad_to;
			default:
				return 0;
			}
	}

	namespace tail_padding_details
	{
		// done            : The form has been evaluated
		// function        : The form was a function
		// declare_type    : The form declares the type of a nested form
		enum class FormTag
		{ done, function, declare_type };

		// Describe the thing-to-apply in an expression
		struct _Form
		{
			// result from 'form'
			size_t pad_to;
			FormTag form_tag;

			_Form(size_t pad_to_,
			      FormTag form_tag_)
				: pad_to(pad_to_),
				  form_tag(form_tag_)
			{}
		};

		size_t assign_padding(Any& input);

		/// \internal
		/// Evaluates the application position of an s-expression and
		/// returns a _Form structure with information for _compile.
		_Form examine_form(Ast ast)
		{
			Any& head = ast[0];

			if(is_astish(head))
				{ return _Form(assign_padding(head),
				               FormTag::function); }

			switch(head._tag)
				{
				case tag<Lambda>::value:
					{
						auto& metadata = *unwrap<Lambda>(head).value;
						Slice formals = unwrap_slice(ast[1]);

						auto formals_size = formals.size();
						auto body_size  = assign_padding(ast[2]);

						auto size = std::max(body_size, formals_size);
						metadata.pad_to = size;

						return _Form(size, FormTag::done);
					}
				case tag<If>::value:
					{
						// predicate
						assign_padding(ast[1]);

						// consiquent
						auto consiquent_size = assign_padding(ast[2]);

						// alternate
						auto alternate_size = assign_padding(ast[3]);

						return _Form(std::max(consiquent_size, alternate_size),
						             FormTag::done);
					}
				case tag<CallLambda>::value:
					{
						auto& metadata = *unwrap<CallLambda>(head).value;
						return _Form(metadata.pad_to,
						             FormTag::function);
					}
				case tag<Symbol>::value:
					{
						auto& sym = unwrap<Symbol>(head);
						if(is<CallLambda>(sym.value))
							{
								return _Form(unwrap<CallLambda>(head).value->pad_to,
								             FormTag::function);
							}

						throw WrongTypeError("Symbol found which is not part of a closure or recursive definition");
					}
				case tag<DeclareType>::value:
					{ return _Form(0, FormTag::declare_type); }

				case tag<CxxFunctor>::value:
				case tag<Quote>::value:
				case tag<Define>::value:
					{ return _Form(0, FormTag::function); }

				default:
					throw WrongTypeError
						(std::string("Dunno how to use ")
						 .append(type_name(head))
						 .append(" as a function"));
				}
		}

		size_t assign_padding(Any& input)
		{
			if(is_astish(input))
				{
					auto ast = unwrap_astish(input);
					auto form = examine_form(ast);

					switch(form.form_tag) {
					case FormTag::done:
						return form.pad_to;

					case FormTag::declare_type:
						return 0;

					case FormTag::function:
						{
							for(auto& vv : slice(ast, 1)) { assign_padding(vv); }
							return form.pad_to;
						}
					}
					assert(0);
				}
			else
				{ return atom_tail_padding(input); }
		}
	}
	using tail_padding_details::assign_padding;
}

#endif
