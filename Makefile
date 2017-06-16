COMMON_FLAGS=-pipe -std=c++14 -ggdb -Wall
GTEST_LINK=-lgtest -lgtest_main -pthread -O0
CXXFLAGS=$(CFLAGS) $(COMMON_FLAGS)

GCC_INCLUDE=-I../ -fuse-ld=gold
CLANG_INCLUDE=$(GCC_INCLUDE) -isystem /usr/lib/clang/3.6/include

IWYU=iwyu -std=c++14 $(CLANG_INCLUDE)

CXX=g++ $(CXXFLAGS) $(GCC_INCLUDE)
#CXX=clang $(CXXFLAGS) -lc++ -stdlib=libc++ $(CLANG_INCLUDE)

atl: atl.cpp *.hpp
	$(CXX) atl.cpp -o atl

# prints byte code name/value table
table-bc: tiny_vm.hpp
	ln -s tiny_vm.hpp tiny_vm.cpp
	$(CXX) -DTINY_VM_PRINT_BYTE_CODES tiny_vm.cpp -o table-bc
	rm tiny_vm.cpp

test: test.cpp *.hpp
	$(CXX) test.cpp -o test

test-vm: test_tiny_vm.cpp *.hpp
	$(CXX) test_tiny_vm.cpp -o test-vm

byte-coded: ./test/byte_coded.cpp *.hpp
	$(CXX) ./test/byte_coded.cpp -o byte-coded

test-zipper: ./test/test_zipper.cpp *.hpp
	$(CXX) ./test/test_zipper.cpp -o test-zipper

TAGS: ./*.cpp ./*.hpp ./test/*.hpp
	find -iname "*cpp" -o -iname "*hpp" | etags -

iwyu-%:
	$(IWYU) $(subst iwyu-,,$@)
