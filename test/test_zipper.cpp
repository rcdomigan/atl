// A little test of the `zip` function, and the Zipper and CountingRange class

#include <utility.hpp>
#include <iostream>
#include <vector>


int main() {
    using namespace std;

    vector<int> vec({5, 4, 1, 4});

    for(auto zz : zip(vec,
		      CountingRange()))
	cout << *get<0>(zz) << " and " << *get<1>(zz) << endl;

    return 0;
}
