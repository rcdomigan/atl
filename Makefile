CXX=g++ -pipe -I./ -std=c++11 -ggdb -Wall
#CXX=clang -pipe -I./ -stdlib=libc++ -lc++ -std=c++11 -ggdb -Wall

all: main

# prints byte code name/value table
table-bc: tiny_vm.hpp
	ln -s tiny_vm.hpp tiny_vm.cpp
	$(CXX) -DTINY_VM_PRINT_BYTE_CODES tiny_vm.cpp -o table-bc
	rm tiny_vm.cpp

main: Makefile main.cpp gc.cpp *.hpp
	$(CXX) main.cpp -o main

test: test.cpp *.hpp
	$(CXX) test.cpp -o test


test-vm: test_tiny_vm.cpp *.hpp
	$(CXX) test_tiny_vm.cpp -o test-vm

byte-coded: ./test/byte_coded.cpp *.hpp
	$(CXX) ./test/byte_coded.cpp -o byte-coded

test-zipper: ./test/test_zipper.cpp *.hpp
	$(CXX) ./test/test_zipper.cpp -o test-zipper
