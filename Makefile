CXX := g++
CXXFLAGS := -std=c++17 -O3 -w
INCLUDES := -I

DEBUGFLAGS := -g -O0

all: main

%: %.cc
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $<

run-%: %
	./$<

debug-%: CXXFLAGS := $(CXXFLAGS) $(DEBUGFLAGS)
debug-%: %.cc
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $* $<

clean:
	rm -f *.o main