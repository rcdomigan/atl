
/**
 * @file /home/ryan/programming/atl/test/byte_coded.cpp
 * @author Ryan Domigan <ryan_domigan@sutdents@uml.edu>
 * Created on Nov 29, 2014
 */

#include <tiny_vm.hpp>
#include <primitive_callable.hpp>
#include <compile.hpp>
#include <iostream>

using namespace std;
using namespace atl;

long sub2(long a, long b) { return a - b; }
long add2(long a, long b) { return a + b; }
bool equal2(long a, long b) { return a == b; }


void run_code(TinyVM& vm, TinyVM::iterator input) {
#ifdef DEBUGGING
    vm.run_debug(input, 100);
#else
    vm.run(input);
#endif
}

// int main() {
//     using namespace atl::vm_codes;

//     GC gc;
//     ParseString parse(gc);
//     Environment env(gc);
//     EncodeAST encode(env, gc);
//     EvaluateAST eval(env);

//     setup_interpreter(&gc, &env, &encode, &parse, &eval);

//     encode.define("equal2", wrap(WrapFn2<bool (*)(long, long), equal2>::tagged()));
//     encode.define("add2", wrap(WrapFn2<long (*)(long, long), add2>::tagged()));

//     auto pcode = compile(gc, encode.any(parse.string_("(if #t 1 2)")));
//     // auto pcode = compile(gc, encode.any(parse.string_("(add2 1 2)")));

//     cout << "**INPUT" << endl;
//     int nn = 0;
//     bool data = false;
//     while(nn < 100) {
// 	auto vv = pcode.value[nn];
// 	auto vname = name_or_null(vv);
// 	cout << " " << nn << ": ";

// 	if(data || (vname == nullptr))
// 	    cout << "@" << hex << vv;
// 	else
// 	    cout << name(vv);
// 	cout << endl;

// 	if (!data && (vv == values::push))
// 	    data = true;
// 	else
// 	    data = false;

// 	++nn;
// 	if(vv == values::finish) break;
//     }
//     cout << "INPUT**\n\n";

//     TinyVM vm;
//     vm.run(pcode.value, 20);
//     cout << "Result: " << vm.stack[0] << endl;
//     return 0;
// }

int main() {
    GC gc;
    ParseString parse(gc);
    Environment env(gc);
    EncodeAST encode(env);

    setup_interpreter(&gc, &env, &encode, &parse);

    encode.define("equal2", WrapFn2<bool (*)(long, long), equal2>::any());
    encode.define("add2", WrapFn2<long (*)(long, long), add2>::any());
    encode.define("sub2", WrapFn2<long (*)(long, long), sub2>::any());

    auto ast = parse.string_("((\\ (a b) (add2 a b)) 2 3)");
    encode.any(ast);

    auto pcode = compile(gc, ast);

    TinyVM vm;
    run_code(vm, pcode.value);

    return 0;
}
