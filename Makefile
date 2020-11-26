all: flipper.cc
	$(CXX) -O3 -pthread flipper.cc -o flipper
