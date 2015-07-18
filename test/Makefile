COMMON_FLAGS=  -pipe -I../ -std=c++11 -ggdb -Wall
GTEST_LINK=-lgtest -lgtest_main
CXXFLAGS=$(CFLAGS) $(COMMON_FLAGS)

CXX=g++ $(CXXFLAGS)
#CXX=clang $(CXXFLAGS) -lc++ -stdlib=libc++

%: %.cpp ../*.hpp ./*.hpp
	$(CXX) $< $(GTEST_LINK) -o $@
