CXX := g++
CXXFLAGS := -std=c++17 -O3
DEBUGFLAGS := -g -O0

PYBIND11_INCLUDE := $(shell python3 -m pybind11 --includes)
PYTHON_SOABI := $(shell python3-config --extension-suffix)

LDFLAGS := -shared

all: main

%: %.cc
	$(CXX) $(CXXFLAGS) -o $@ $<

experiments:
	$(CXX) $(CXXFLAGS) -fPIC $(PYBIND11_INCLUDE) $(LDFLAGS) experiments.cpp -o benchmark_module$(PYTHON_SOABI)

run-%: %
	./$<

debug-%: CXXFLAGS := $(CXXFLAGS) $(DEBUGFLAGS)
debug-%: %.cc
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f *.o main *.so *.cpython-*.so
