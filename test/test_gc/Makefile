PROJECT_ROOT=../../

COMMON_FLAGS:=-pipe -std=c++11 -ggdb -Wall -O0
CXXFLAGS:=$(CFLAGS) $(COMMON_FLAGS)

GCC_INCLUDE=-I$(PROJECT_ROOT)
CLANG_INCLUDE=$(GCC_INCLUDE) -isystem /usr/lib/clang/3.6/include

IWYU=iwyu -std=c++11 $(CLANG_INCLUDE)

CXX=g++ $(CXXFLAGS) $(GCC_INCLUDE)
#CXX=clang $(CXXFLAGS) -lc++ -stdlib=libc++ $(CLANG_INCLUDE)

%: %.cpp $(PROJECT_ROOT)/*.hpp $(PROJECT_ROOT)/gc/*.hpp $(PROJECT_ROOT)/test/*.hpp
	$(CXX) $< -lgtest -lgtest_main -lpthread -o $@

iwyu-%:
	$(IWYU) $(subst iwyu-,,$@)
