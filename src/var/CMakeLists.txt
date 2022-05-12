cmake_minimum_required(VERSION 3.6)
project(var)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(SRC
	rpc_var.cc
)

add_library(${PROJECT_NAME} OBJECT ${SRC})

