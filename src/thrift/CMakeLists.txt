cmake_minimum_required(VERSION 3.6)
project(thrift)

set(SRC
	rpc_thrift_buffer.cc
	rpc_thrift_idl.cc
)

add_library(${PROJECT_NAME} OBJECT ${SRC})

