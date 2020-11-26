all: flipper.cc
	$(CXX) -Wall -Werror -O3 -pthread flipper.cc -o flipper
