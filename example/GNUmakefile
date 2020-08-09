workflow_inc = -I/search/data2/wujiaxu/sos/SubTask/_include
workflow_lib = -L/search/data2/wujiaxu/sos/SubTask/_lib
includes = -I../_include $(workflow_inc)
libs =     -L../_lib     $(workflow_lib)

all:
	protoc echo.proto --cpp_out=./ --proto_path=./
	../_bin/srpc_generator protobuf ./echo.proto ./
	../_bin/srpc_generator thrift ./msg.thrift ./
	g++ -o client client.cc echo.pb.cc -g -O2 -std=c++11 -fno-exceptions -lsrpc -ljemalloc $(includes) $(libs)
	g++ -o server server.cc echo.pb.cc -g -O2 -std=c++11 -fno-exceptions -lsrpc -ljemalloc $(includes) $(libs) 

clean:
	rm -f client server

