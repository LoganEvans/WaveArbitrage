all: simulate proto backtest

CC=clang
CXX=clang++
CXX_FLAGS=-Wall -Werror -O3 -std=c++17

backtest.o: backtest.cpp
	$(CXX) $(CXX_FLAGS) -pthread -c backtest.cpp

backtest: backtest.o market_data.pb.o
	$(CXX) $(CXX_FLAGS) -pthread -lprotobuf backtest.o market_data.pb.o -o backtest

clean_backtest:
	rm -f backtest

simulate: simulate.cc
	$(CXX) $(CXX_FLAGS) -pthread simulate.cc -o simulate

clean_simulate:
	rm -f simulate

proto: market_data.proto
	protoc -I=. --python_out=. --cpp_out=. market_data.proto

market_data.pb.o: proto
	$(CXX) $(CXX_FLAGS) -c market_data.pb.cc

clean_proto:
	rm -f market_data_pb2.py
	rm -f market_data.pb.cc
	rm -f market_data.pb.h

clean: clean_simulate clean_proto clean_backtest
	rm -f *.o

