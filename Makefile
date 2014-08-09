CC=g++ -pipe -I./ -std=c++11 -ggdb -Wall -O0
#CC=clang -pipe -I./ -stdlib=libc++ -lc++ -std=c++11 -ggdb -Wall

all: main

main: Makefile main.cpp gc.cpp *.hpp
	$(CC) main.cpp -o main

test: test.cpp *.hpp
	$(CC) test.cpp -o test

