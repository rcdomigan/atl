#ifndef ATL_GC_MARKED_HPP
#define ATL_GC_MARKED_HPP

namespace atl
{
	struct GC;
	typedef std::function<void (GC&)> MarkCallback;

	// The purpose of the Marked structs is to maintain a singly
	// linked list on the call stack.  The idea is that some function
	// like GC::make returns a Marked instance which is not part of
	// the list.  The function receiving this initializes or assigns
	// its own the Marked using either the move assign or construct,
	// so it's the move assign/construct methods which actually tie
	// the instance to the GC mark.
	template<class T>
	struct Marked;

	// Add a base so I can use std::is_base to check if something is
	// Marked
	struct MarkedBase {};

	template<>
	struct Marked<Any> : public MarkedBase
	{
		Marked** _top;
		Marked *_up;
		Any any;
		bool _owner;

		typedef Any value_type;

		void _pop()
		{
			if(_owner)
				{
					// so; we should mostly be popping from the top,
					// or close to the top, since a given frame should
					// only have a couple of Marked locals
					if(*_top == this)
						{ *_top = _up; }
					else
						{
							auto top = *_top;
							while(top)
								{
									if(top->_up == this)
										{
											top->_up = _up;
											break;
										}
									else
										{ top = top->_up; }
								}
						}
				}

			any = wrap<Null>();
			_owner = false;
			_up = nullptr;
		}

		void _move(Marked&& other)
		{
			_top = other._top;
			any = other.any;
			_owner = other._owner;

			other._pop();

			_up = *_top;
			*_top = this;
		}

		Marked()=delete;

		Marked(Marked*& top, Any const& any_)
			: _top(&top), _up(*_top), any(any_), _owner(true)
		{ *_top = this; }

		Marked(Marked&& other)
		{ _move(std::move(other)); }

		Marked& operator=(Marked&& other)
		{
			_pop();
			_move(std::move(other));
			return *this;
		}

		Marked(Marked const&) = delete;

		~Marked() { _pop(); }

		Any& operator*() { return any; }
		Any* operator->() { return &any; }
	};

	template<class T>
	struct Marked : public Marked<Any>
	{
		typedef Marked<Any> MarkedBase;
		typedef T value_type;

		Marked() : MarkedBase() {}
		Marked(MarkedBase*& top, Any const& any) : MarkedBase(top, any) {}

		Marked(Marked&& other) : MarkedBase(std::move(other)) {}

		T& operator*() { return unwrap<T>(any); }
		T* operator->() { return &unwrap<T>(any); }

		T* pointer() { return &unwrap<T>(any); }

		Marked& operator=(Marked&& other)
		{
			_pop();
			MarkedBase::_move(std::move(other));
			return *this;
		}

		Marked<Any> wrap()
		{ return Marked<Any>(std::move(*this)); }
	};

	// Return the value a Marked<..> is wrapping, or pass a value
	// through if it's not a Marked<..>.
	template<class T, bool is_marked=std::is_base_of<MarkedBase, T>::value>
	struct UnpackMarked;

	template<class T>
	struct UnpackMarked<T, true>
	{
		typedef typename T::value_type value_type;
		static typename T::value_type& a(T& input) { return *input; }
	};

	template<class T>
	struct UnpackMarked<T, false>
	{
		typedef T value_type;
		static T& a(T& input) { return input; }
	};

	template<class T>
	typename UnpackMarked<T>::value_type&
	unpack_marked(T& value)
	{ return UnpackMarked<T>::a(value); }
}

#endif
