all: flipper proto backtest

backtest.o: backtest.cpp
	$(CXX) -Wall -Werror -O3 -pthread -c backtest.cpp

backtest: backtest.o market_data.pb.o
	$(CXX) -Wall -Werror -O3 -pthread -lprotobuf backtest.o market_data.pb.o -o backtest

clean_backtest:
	rm backtest

flipper: flipper.cc
	$(CXX) -Wall -Werror -O3 -pthread flipper.cc -o flipper

clean_flipper: flipper
	rm flipper

proto: market_data.proto
	protoc -I=. --python_out=. --cpp_out=. market_data.proto

market_data.pb.o: proto
	$(CXX) -Wall -Werror -O3 -c market_data.pb.cc

clean_proto: market_data_pb2.py
	rm market_data_pb2.py

clean: clean_flipper clean_proto clean_backtest
	rm *.o
