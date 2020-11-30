all: flipper proto

flipper: flipper.cc
	$(CXX) -Wall -Werror -O3 -pthread flipper.cc -o flipper

clean_flipper: flipper
	rm flipper

proto: market_data.proto
	protoc -I=. --python_out=. market_data.proto

clean_proto: market_data_pb2.py
	rm market_data_pb2.py

clean: clean_flipper clean_proto
