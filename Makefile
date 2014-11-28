CXX=g++ -pipe -I./ -std=c++11 -ggdb -Wall
#CXX=clang -pipe -I./ -stdlib=libc++ -lc++ -std=c++11 -ggdb -Wall

all: main

main: Makefile main.cpp gc.cpp *.hpp
	$(CXX) main.cpp -o main

test: test.cpp *.hpp
	$(CXX) test.cpp -o test


test-vm: test_tiny_vm.cpp *.hpp
	$(CXX) test_tiny_vm.cpp -o test-vm

byte-coded: ./test/byte_coded.cpp *.hpp
	$(CXX) ./test/byte_coded.cpp -o byte-coded

test-parser: ./test/test_parser.cpp *.hpp
	$(CXX) ./test/test_parser.cpp -o test-parser

test-zipper: ./test/test_zipper.cpp *.hpp
	$(CXX) ./test/test_zipper.cpp -o test-zipper
