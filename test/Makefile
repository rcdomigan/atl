COMMON_FLAGS=-pipe -std=c++14 -ggdb -Wall
GTEST_LINK=-lgtest -lgtest_main -pthread -O0
CXXFLAGS=$(CFLAGS) $(COMMON_FLAGS)

GCC_INCLUDE=-I../../ -fuse-ld=gold
CLANG_INCLUDE=$(GCC_INCLUDE) -isystem /usr/lib/clang/3.6/include

IWYU=iwyu -std=c++11 $(CLANG_INCLUDE)

CXX=g++ $(CXXFLAGS) $(GCC_INCLUDE)
#CXX=clang $(CXXFLAGS) -lc++ -stdlib=libc++ $(CLANG_INCLUDE)

%: %.cpp ../*.hpp ../helpers/*.hpp ./*.hpp ../gc/*.hpp
	$(CXX) $< $(GTEST_LINK) -o $@

gc: gc.cpp ../*.hpp ../helpers/*.hpp ./*.hpp ../gc/*.hpp ./test_gc/*.cpp
	$(CXX) $< $(GTEST_LINK) -o $@


iwyu-%:
	$(IWYU) $(subst iwyu-,,$@)

test: test.cpp ../*.hpp ./*.cpp ../gc/*.hpp
	$(CXX) test.cpp $(GTEST_LINK) -o test
