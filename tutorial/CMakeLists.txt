cmake_minimum_required(VERSION 3.6)

set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "build type")

project(srpc_tutorial
		LANGUAGES C CXX
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})

if (NOT "$ENV{LIBRARY_PATH}" STREQUAL "")
	string(REPLACE ":" ";" LIBRARY_PATH $ENV{LIBRARY_PATH})
	set(CMAKE_SYSTEM_LIBRARY_PATH ${LIBRARY_PATH};${CMAKE_SYSTEM_LIBRARY_PATH})
endif ()

if (NOT "$ENV{CPLUS_INCLUDE_PATH}" STREQUAL "")
	string(REPLACE ":" ";" INCLUDE_PATH $ENV{CPLUS_INCLUDE_PATH})
	set(CMAKE_SYSTEM_INCLUDE_PATH ${INCLUDE_PATH};${CMAKE_SYSTEM_INCLUDE_PATH})
endif ()

find_package(OpenSSL REQUIRED)

set(protobuf_MODULE_COMPATIBLE ON CACHE BOOL "")
if (WIN32)
	find_package(Protobuf CONFIG REQUIRED)
else ()
	find_package(Protobuf REQUIRED)
endif ()

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/lz4/lib/lz4.h")
	set(LZ4_LIB lz4)
endif ()

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/snappy/cmake")
	set(SNAPPY_LIB snappy)
endif ()

if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/workflow/workflow-config.cmake.in")
	find_package(Workflow REQUIRED CONFIG HINTS ../workflow)
endif ()

find_package(ZLIB REQUIRED)

#if (APPLE)
#	if (WORKFLOW_INCLUDE_DIR)
#		set (WORKFLOW_INCLUDE_DIR ${Workflow_DIR}/_include)
#		set (WORKFLOW_LIB_DIR ${Workflow_DIR}/_lib)
#	else ()
#		find_package(Workflow REQUIRED)
#	endif ()
#else ()
#	find_package(Workflow REQUIRED)
#endif()

find_package(srpc REQUIRED CONFIG HINTS ..)
include_directories(
	${OPENSSL_INCLUDE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
	${Protobuf_INCLUDE_DIR}
	${WORKFLOW_INCLUDE_DIR}
	${SRPC_INCLUDE_DIR}
)

if (WIN32)
	link_directories(${SRPC_LIB_DIR} ${WORKFLOW_LIB_DIR} ${Protobuf_LIB_DIR})
	set(SRPC_GEN_PROGRAM ${SRPC_BIN_DIR}/Debug/srpc_generator.exe)
else ()
	get_filename_component(Protobuf_LIB_DIR ${Protobuf_LIBRARY} DIRECTORY)
	link_directories(${SRPC_LIB_DIR} ${WORKFLOW_LIB_DIR} ${Protobuf_LIB_DIR})
	set(SRPC_GEN_PROGRAM ${SRPC_BIN_DIR}/srpc_generator)
endif ()

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS echo_pb.proto)
protobuf_generate_cpp(HELLOWORLD_SRCS HELLOWORLD_HDRS helloworld.proto)

add_custom_target(
	TURORIAL_GEN ALL
	COMMAND ${SRPC_GEN_PROGRAM} ${PROJECT_SOURCE_DIR}/echo_pb.proto  ${PROJECT_SOURCE_DIR}
	COMMAND ${SRPC_GEN_PROGRAM} ${PROJECT_SOURCE_DIR}/echo_thrift.thrift ${PROJECT_SOURCE_DIR}
	COMMAND ${SRPC_GEN_PROGRAM} protobuf ${PROJECT_SOURCE_DIR}/helloworld.proto  ${PROJECT_SOURCE_DIR}
	COMMENT "sprc generator..."
)

if (WIN32)
	set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   /MP /wd4200")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP /wd4200 /Zc:__cplusplus /std:c++14")
else ()
	set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wall -fPIC -pipe -std=gnu90")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fPIC -pipe -std=c++11 -fno-exceptions")
endif ()

set(TUTORIAL_PB_LIST
	tutorial-01-srpc_pb_server
	tutorial-02-srpc_pb_client
	tutorial-05-brpc_pb_server
	tutorial-06-brpc_pb_client
	tutorial-09-client_task
	tutorial-10-server_async
	tutorial-15-srpc_pb_proxy
)

if (APPLE)
	set(SRPC_LIB srpc workflow pthread OpenSSL::SSL OpenSSL::Crypto protobuf z)
elseif (WIN32)
	set(SRPC_LIB srpc workflow ws2_32 wsock32 OpenSSL::SSL OpenSSL::Crypto protobuf::libprotobuf ZLIB::ZLIB)
else ()
	set(SRPC_LIB srpc ${SNAPPY_LIB} ${LZ4_LIB})
endif ()

foreach(src ${TUTORIAL_PB_LIST})
	string(REPLACE "-" ";" arr ${src})
	list(GET arr -1 bin_name)
	add_executable(${bin_name} ${src}.cc ${PROTO_SRCS} ${PROTO_HDRS})
	target_link_libraries(${bin_name} ${SRPC_LIB})
	add_dependencies(${bin_name} TURORIAL_GEN)
endforeach()

set(TUTORIAL_THRIFT_LIST
	tutorial-03-srpc_thrift_server
	tutorial-04-srpc_thrift_client
	tutorial-07-thrift_thrift_server
	tutorial-08-thrift_thrift_client
)

foreach(src ${TUTORIAL_THRIFT_LIST})
	string(REPLACE "-" ";" arr ${src})
	list(GET arr -1 bin_name)
	add_executable(${bin_name} ${src}.cc)
	target_link_libraries(${bin_name} ${SRPC_LIB})
	add_dependencies(${bin_name} TURORIAL_GEN)
endforeach()

set(TUTORIAL_HELLOWORLD_LIST
	tutorial-11-trpc_pb_server
	tutorial-12-trpc_pb_client
	tutorial-13-trpc_http_server
	tutorial-14-trpc_http_client
)

foreach(src ${TUTORIAL_HELLOWORLD_LIST})
	string(REPLACE "-" ";" arr ${src})
	list(GET arr -1 bin_name)
	add_executable(${bin_name} ${src}.cc ${HELLOWORLD_SRCS} ${HELLOWORLD_HDRS})
	target_link_libraries(${bin_name} ${SRPC_LIB})
	add_dependencies(${bin_name} TURORIAL_GEN)
endforeach()

