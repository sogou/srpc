cmake_minimum_required(VERSION 3.6)
project(%s LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE RelWithDebInfo)

set(Protobuf_ERROR_MSG "ERROR: Failed to find protobuf. Some suggestions for installation.")
set(Protobuf_ERROR_MSG_LINUX "${Protobuf_ERROR_MSG}
For Debian / Ubuntu ...:
    sudo apt-get update
    sudo apt-get install -y libprotobuf-dev protobuf-compiler
For RedHat / Fedora / CentOS ...:
    sudo yum makecache
    sudo yum install -y protobuf-devel protobuf-compiler")

set(Protobuf_ERROR_MSG_MACOS "${Protobuf_ERROR_MSG}
For MacOS :
    sudo brew update
    sudo brew install protobuf protobuf-c")

# Find all the dependencies
find_package(OpenSSL REQUIRED)

set(Workflow_DIR "%s")
find_package(Workflow REQUIRED CONFIG HINTS ${Workflow_DIR})

set(Srpc_DIR "%s")
find_package(srpc REQUIRED CONFIG HINTS ${Srpc_DIR})

find_package(Protobuf)
if ("x${Protobuf_DIR}" STREQUAL "xProtobuf_DIR-NOTFOUND")
    if (APPLE)
        message (FATAL_ERROR ${Protobuf_ERROR_MSG_MACOS})
    else ()
        message (FATAL_ERROR ${Protobuf_ERROR_MSG_LINUX})
    endif ()
endif ()

get_filename_component(Protobuf_LIB_DIR ${Protobuf_LIBRARY} DIRECTORY)

if (NOT EXISTS "${Srpc_DIR}/third_party/lz4/lib/lz4.h")
    set(LZ4_LIB lz4)
endif ()

if (NOT EXISTS "${Srpc_DIR}/third_party/snappy/cmake")
    set(SNAPPY_LIB snappy)
endif ()

find_package(ZLIB REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})

# Generate idl code: xx.srpc.h xx.pb.h xx.pb.cc xx.thrift.h
set(IDL_FILE %s)
set(SRPC_GEN_PROGRAM ${SRPC_BIN_DIR}/srpc_generator) %s

add_custom_target(SRPC_GEN ALL
    COMMAND ${SRPC_GEN_PROGRAM} ${PROJECT_SOURCE_DIR}/${IDL_FILE} ${PROJECT_SOURCE_DIR} -s
    COMMENT "sprc generator..."
)

# Prefer to static link first
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ${CMAKE_FIND_LIBRARY_SUFFIXES})
find_library(Workflow_LIB workflow HINTS ${Workflow_DIR}/_lib)
find_library(Srpc_LIB srpc HINTS ${Srpc_DIR}/_lib)

# Set all the libraries here
set(LIB ${Srpc_LIB} ${Workflow_LIB} pthread OpenSSL::SSL OpenSSL::Crypto
    protobuf z ${SNAPPY_LIB} ${LZ4_LIB})

# Add all the common code here
set(COMMON_CODE config/config.cc config/Json.cc ${PROTO_SRCS})

# Add header directories and library directories here
include_directories(${OPENSSL_INCLUDE_DIR} ${Protobuf_INCLUDE_DIR}
                    ${WORKFLOW_INCLUDE_DIR} ${SRPC_INCLUDE_DIR}
                    ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
link_directories(${OPENSSL_LINK_DIR} ${Protobuf_LIB_DIR}
                 ${WORKFLOW_LIB_DIR} ${SRPC_LIB_DIR})

# Build executable outputs
set(PROJECT_OUTPUT server client%s)
foreach(output ${PROJECT_OUTPUT})
    add_executable(${output} ${output}_main.cc ${COMMON_CODE})
    target_link_libraries(${output} ${LIB})
endforeach()

