COMMON_FLAGS=  -pipe -std=c++11 -ggdb -Wall
GTEST_LINK=-lgtest -lgtest_main -pthread -O0
CXXFLAGS=$(CFLAGS) $(COMMON_FLAGS)

GCC_INCLUDE=-I../
CLANG_INCLUDE=$(GCC_INCLUDE) -isystem /usr/lib/clang/3.6/include

IWYU=iwyu -std=c++11 $(CLANG_INCLUDE)

CXX=g++ $(CXXFLAGS) $(GCC_INCLUDE)
#CXX=clang $(CXXFLAGS) -lc++ -stdlib=libc++ $(CLANG_INCLUDE)

%: %.cpp ../*.hpp ./*.hpp
	$(CXX) $< $(GTEST_LINK) -o $@

iwyu-%:
	$(IWYU) $(subst iwyu-,,$@)

test: test.cpp ../*.hpp ./*.cpp
	$(CXX) test.cpp $(GTEST_LINK) -o test
