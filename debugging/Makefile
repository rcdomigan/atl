COMMON_FLAGS=  -pipe -I../ -std=c++11 -ggdb -Wall
CXXFLAGS=$(CFLAGS) $(COMMON_FLAGS)

CXX=g++ $(CXXFLAGS)
#CXX=clang $(CXXFLAGS) -lc++ -stdlib=libc++

%: %.cpp ../*.hpp ./*.hpp
	$(CXX) $< -o $@
