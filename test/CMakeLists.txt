cmake_minimum_required(VERSION 3.6)

set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "build type")

project(srpc_test
		LANGUAGES C CXX
)

if (NOT "$ENV{LIBRARY_PATH}" STREQUAL "")
	string(REPLACE ":" ";" LIBRARY_PATH $ENV{LIBRARY_PATH})
	set(CMAKE_SYSTEM_LIBRARY_PATH ${LIBRARY_PATH};${CMAKE_SYSTEM_LIBRARY_PATH})
endif ()

if (NOT "$ENV{CPLUS_INCLUDE_PATH}" STREQUAL "")
	string(REPLACE ":" ";" INCLUDE_PATH $ENV{CPLUS_INCLUDE_PATH})
	set(CMAKE_SYSTEM_INCLUDE_PATH ${INCLUDE_PATH};${CMAKE_SYSTEM_INCLUDE_PATH})
endif ()

find_package(OpenSSL REQUIRED)
find_package(srpc REQUIRED CONFIG HINTS ..)

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

include_directories(
	${OPENSSL_INCLUDE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
	${Protobuf_INCLUDE_DIR}
	${SRPC_INCLUDE_DIR}
	${WORKFLOW_INCLUDE_DIR}
)

if (WIN32)
	link_directories(${SRPC_LIB_DIR} ${WORKFLOW_LIB_DIR} ${Protobuf_LIB_DIR})
	set(SRPC_GEN_PROGRAM ${SRPC_BIN_DIR}/Debug/srpc_generator.exe)
else ()
	get_filename_component(Protobuf_LIB_DIR ${Protobuf_LIBRARY} DIRECTORY)
	link_directories(${SRPC_LIB_DIR} ${WORKFLOW_LIB_DIR} ${Protobuf_LIB_DIR})
	set(SRPC_GEN_PROGRAM ${SRPC_BIN_DIR}/srpc_generator)
endif ()

find_program(CMAKE_MEMORYCHECK_COMMAND valgrind)
set(memcheck_command ${CMAKE_MEMORYCHECK_COMMAND} ${CMAKE_MEMORYCHECK_COMMAND_OPTIONS} --error-exitcode=1 --leak-check=full --show-leak-kinds=all)

add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})

enable_testing()

if (WIN32)
	find_package(GTest CONFIG REQUIRED)
else ()
	find_package(GTest REQUIRED)
endif ()

set(CXX_STD "c++11")
if (GTest_VERSION VERSION_GREATER_EQUAL "1.13.0")
	set(CXX_STD "c++14")
else ()
	get_filename_component(GTEST_PATH ${GTEST_LIBRARY} DIRECTORY ABSOLUTE)
	execute_process(COMMAND grep Version "${GTEST_PATH}/pkgconfig/gtest.pc"
					RESULT_VARIABLE GTEST_VERSION_RESULT
					OUTPUT_VARIABLE GTEST_VERSION_OUTPUT)
	if (${GTEST_VERSION_RESULT} EQUAL "0")
		string(REPLACE " " ";" GTEST_VERSION_STR ${GTEST_VERSION_OUTPUT})
		list(GET GTEST_VERSION_STR 1 GTEST_VERSION)
		if (${GTEST_VERSION} VERSION_GREATER_EQUAL "1.13.0")
			set(CXX_STD "c++14")
		endif ()
	endif ()
endif ()

if (WIN32)
	set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   /MP /wd4200")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP /wd4200 /Zc:__cplusplus /std:c++14")
else ()
	set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wall -fPIC -pipe -std=gnu90")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fPIC -pipe -std=${CXX_STD} -fno-exceptions")
endif ()

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS test_pb.proto)

add_custom_target(
	SRPC_GEN ALL
	COMMAND ${SRPC_GEN_PROGRAM} ${PROJECT_SOURCE_DIR}/test_pb.proto ${PROJECT_SOURCE_DIR}
	COMMAND ${SRPC_GEN_PROGRAM} ${PROJECT_SOURCE_DIR}/test_thrift.thrift ${PROJECT_SOURCE_DIR}
	COMMENT "srpc generator..."
)

set(GTEST_LIB GTest::GTest GTest::Main)

if (APPLE)
	set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ${CMAKE_FIND_LIBRARY_SUFFIXES})
	find_library(Workflow_LIB workflow HINTS ../workflow/_lib)
	find_library(Srpc_LIB srpc HINTS ../_lib)

	set(SRPC_LIB
		${Srpc_LIB}
		${Workflow_LIB}
		pthread
		OpenSSL::SSL
		OpenSSL::Crypto
		protobuf
		z
		)
elseif (WIN32)
	set(SRPC_LIB
		srpc
		workflow
		ws2_32
		wsock32
		OpenSSL::SSL
		OpenSSL::Crypto
		protobuf::libprotobuf
		ZLIB::ZLIB
		Snappy::snappy
		${LZ4_LIBRARY}
		)
	set(GTEST_LIB GTest::gtest GTest::gtest_main)
else ()
	if (SNAPPY_INSTALLED)
		set(SNAPPY_LIB snappy)
	endif ()

	if (LZ4_INSTALLED)
		set(LZ4_LIB lz4)
	endif ()

	set(SRPC_LIB
		srpc
		workflow
		pthread
		OpenSSL::SSL
		OpenSSL::Crypto
		protobuf
		z
		${SNAPPY_LIB}
		${LZ4_LIB}
		)
endif ()

set(TEST_LIST unittest)
set(BASIC_TEST var_unittest)
set(ALL_TEST var_unittest unittest)

foreach(src ${TEST_LIST})
	add_executable(${src} EXCLUDE_FROM_ALL ${src}.cc ${PROTO_SRCS} ${PROTO_HDRS})
	target_link_libraries(${src} ${SRPC_LIB} ${GTEST_LIB})
	add_test(${src} ${src})
	add_dependencies(${src} SRPC_GEN)
	add_dependencies(check ${src})
endforeach()

foreach(src ${BASIC_TEST})
	add_executable(${src} EXCLUDE_FROM_ALL ${src}.cc)
	target_link_libraries(${src} ${SRPC_LIB} ${GTEST_LIB})
	add_test(${src} ${src})
	add_dependencies(check ${src})
endforeach()

if (WIN32)
	set(memcheck nothing)
elseif (NOT ${CMAKE_MEMORYCHECK_COMMAND} STREQUAL "CMAKE_MEMORYCHECK_COMMAND-NOTFOUND")
	foreach(src ${ALL_TEST})
		add_test(${src}-memory-check ${memcheck_command} ./${src})
	endforeach()
endif ()

