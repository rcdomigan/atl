#ifndef ATL_INFER_HPP
#define ATL_INFER_HPP
/**
 * @file /home/ryan/programmring/atl/infer.hpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jun 03, 2014
 *
 * A stab at type inference
 */
#include "./print.hpp"

namespace atl {
  Any infer(Any aa) {
    using namespace std;
    cout << printer::any(aa) << endl;

    switch(aa._tag) {
    case tag<If>::value: {
	static constexpr array<Any, 4> if_type{{nil<Bool>::value(), nil<Any>::value(), nil<Any>::value(), nil<Any>::value()}};
	static CxxArray vec(if_type.begin(), if_type.end());
	return wrap(&vec);
    }
    case tag<Procedure>::value: {
	auto& types = unwrap<Procedure>(aa)._parameter_types;
	static CxxArray vec(types.begin(), types.end());
	return wrap(&vec);
    }
    case tag<PrimitiveRecursive>::value:
	return wrap(&unwrap<PrimitiveRecursive>(aa)._parameter_types);
    case tag<Parameter>::value:
	return unwrap<Parameter>(aa).value();
    default:
	return aa;
    }
  }
}

#endif
