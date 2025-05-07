CXX := g++
CXXFLAGS := -std=c++17 -O3 -w
DEBUGFLAGS := -g -O0

PYBIND11_INCLUDE := $(shell python3 -m pybind11 --includes)
SHARED := -shared -undefined dynamic_lookup

ARCHFLAGS := -arch x86_64

all: main

%: %.cc
	$(CXX) $(CXXFLAGS) -o $* $<

experiments:
	$(CXX) $(CXXFLAGS) $(PYBIND11_INCLUDE) $(SHARED) $(ARCHFLAGS) experiments.cpp -o benchmark_module.cpython-312-darwin.so

run-%: %
	./$<

debug-%: CXXFLAGS := $(CXXFLAGS) $(DEBUGFLAGS)
debug-%: %.cc
	$(CXX) $(CXXFLAGS) -o $* $<

clean:
	rm -f *.o main