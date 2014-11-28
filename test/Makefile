COMMON_FLAGS=  -pipe -I../ -std=c++11 -ggdb -Wall
GTEST_LINK=-lgtest -lgtest_main
CXXFLAGS=$(CFLAGS) $(COMMON_FLAGS)

CXX=g++ $(CXXFLAGS)
#CXX=clang $(COMMON_FLAGS) -lc++ -stdlib=libc++

vm_test: ../*hpp vm_test.cpp
	$(CXX) vm_test.cpp $(GTEST_LINK) -o vm_test

compiler: ../*hpp compiler.cpp
	$(CXX) compiler.cpp $(GTEST_LINK) -o compiler
