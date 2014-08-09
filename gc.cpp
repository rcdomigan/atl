#include "gc.hpp"
#include "./helpers.hpp"

/**
 * @file /home/ryan/programming/atl/gc.cc
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Jan 28, 2014
 *
 * Helpers needs to see the GC interface, and I want to use the helpers in GC::mark.
 * Stupid compiler.  The real advantage of seperate implementation and interface continues to elude me.
 */

namespace atl {
  void GC::mark(Any a) {
  /* loop: */
  /*   if( a._value == nullptr ) */
  /*     return; */

  /*   if( is<Pointer>(a) ) { */
  /*     a = *unwrap<Pointer>(a)._value; */
  /*     goto loop; */
  /*   } */

  /*   switch( type_tag(a) ) { */
  /*   case tag<Undefined>::value: */
  /*     _undefined_heap.mark( unwrap<Undefined*>(a) ); */
  /*     return; */

  /*   case tag<Mark>::value: */
  /*     unwrap<Mark>(a)._mark(*this); */
  /*     return; */

  /*   case tag<Macro>::value: */
  /*   case tag<Procedure>::value: */
  /*     if( _procedure_heap.mark( unwrap<Procedure*>(a) ) ) { */
  /* 	mark(unwrap<Procedure>(a)._body); /\* implicitly marks used parameters *\/ */

  /* 	int num_params = unwrap<Procedure>(a)._num_params; */
  /* 	Any *params = unwrap<Procedure>(a)._stack; */

  /* 	while( params != nullptr ) { */
  /* 	  for(int i = 0; i < num_params; ++i) */
  /* 	    mark(params[i]); */
  /* 	  params = unwrap<Any>(params[num_params]); */
  /* 	} */
  /*     } */
  /*     return; */
  /*   } */
  } /*** GC::mark ***/
}
