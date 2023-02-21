cmake_minimum_required(VERSION 3.6)

project(%s LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE RelWithDebInfo)

find_package(OpenSSL REQUIRED)

set(Workflow_DIR "%s")
find_package(Workflow REQUIRED CONFIG HINTS ${Workflow_DIR})

set(Srpc_DIR "%s")
find_package(srpc REQUIRED CONFIG HINTS ${Srpc_DIR})

find_package(Protobuf REQUIRED)
get_filename_component(Protobuf_LIB_DIR ${Protobuf_LIBRARY} DIRECTORY)

if (NOT EXISTS "${Srpc_DIR}/third_party/lz4/lib/lz4.h")
    set(LZ4_LIB lz4)
endif ()

if (NOT EXISTS "${Srpc_DIR}/third_party/snappy/cmake")
    set(SNAPPY_LIB snappy)
endif ()

find_package(ZLIB REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})

set(IDL_FILE %s)
set(SRPC_GEN_PROGRAM ${SRPC_BIN_DIR}/srpc_generator) %s

add_custom_target(SRPC_GEN ALL
    COMMAND ${SRPC_GEN_PROGRAM} ${PROJECT_SOURCE_DIR}/${IDL_FILE} ${PROJECT_SOURCE_DIR} -s
    COMMENT "sprc generator..."
)

include_directories(${OPENSSL_INCLUDE_DIR}
                    ${Protobuf_INCLUDE_DIR}
                    ${WORKFLOW_INCLUDE_DIR}
                    ${SRPC_INCLUDE_DIR}
                    ${PROJECT_SOURCE_DIR}        # for generated.srpc.h
                    ${CMAKE_CURRENT_BINARY_DIR}  # for generated.srpc.h
                    )

link_directories(${OPENSSL_LINK_DIR}
                 ${Protobuf_LIB_DIR}
                 ${WORKFLOW_LIB_DIR}
                 ${SRPC_LIB_DIR}
                 )

set(LIB workflow srpc pthread OpenSSL::SSL OpenSSL::Crypto protobuf
    z ${SNAPPY_LIB} ${LZ4_LIB})

set(PROJECT_OUTPUT server client)
foreach(output ${PROJECT_OUTPUT})
    add_executable(${output} ${output}_main.cc
                   config/config.cc config/Json.cc ${PROTO_SRCS})
    target_link_libraries(${output} ${LIB})
endforeach()

