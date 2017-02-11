#ifndef ATL_GC_VM_CLOSURE_HPP
#define ATL_GC_VM_CLOSURE_HPP

namespace atl
{
	// Type allocated by the VM to describe a function's closure.
	struct ClosurePool
	{
		std::vector<pcode::value_type*> closures;

		~ClosurePool()
		{ for(auto cc : closures) { delete[] cc; } }

		pcode::value_type* closure(pcode::value_type body_location,
		                           size_t formals,
		                           size_t captures)
		{
			auto rval = new pcode::value_type[captures + 2];
			rval[0] = formals;
			rval[1] = body_location;
			closures.push_back(rval);
			return rval;
		}
	};
}

#endif
