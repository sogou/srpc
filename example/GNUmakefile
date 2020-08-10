workflow_inc = -I${Workflow_DIR}/_include
workflow_lib = -L${Workflow_DIR}/_lib
includes     = -I../_include $(workflow_inc)
libs         = -L../_lib     $(workflow_lib)
jemalloc     = -ljemalloc

all: info example

info:
ifeq ("${Workflow_DIR}info", "info")
	@echo "Please set \"Workflow_DIR\" to make example."
	exit 1
endif

example:
	protoc echo.proto --cpp_out=./ --proto_path=./
	../_bin/srpc_generator protobuf ./echo.proto ./
	../_bin/srpc_generator thrift ./msg.thrift ./
	g++ -o client client.cc echo.pb.cc -g -O2 -std=c++11 -fno-exceptions -lsrpc $(jemalloc) $(includes) $(libs)
	g++ -o server server.cc echo.pb.cc -g -O2 -std=c++11 -fno-exceptions -lsrpc $(jemalloc) $(includes) $(libs) 

clean:
	rm -f client server

