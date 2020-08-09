cmake_minimum_required(VERSION 3.6)
project(compress)

set(SRC
	rpc_compress.cc
	rpc_compress_snappy.cc
)

add_library(${PROJECT_NAME} OBJECT ${SRC})

