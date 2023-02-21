cmake_minimum_required(VERSION 3.6)

project(%s LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE RelWithDebInfo)

find_package(OpenSSL REQUIRED)

set(Workflow_DIR "%s")

find_package(Workflow REQUIRED CONFIG HINTS ${Workflow_DIR})

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})

set(LIB workflow pthread OpenSSL::SSL OpenSSL::Crypto)
set(PROJECT_OUTPUT server client)

include_directories(${OPENSSL_INCLUDE_DIR}
                    ${WORKFLOW_INCLUDE_DIR})

link_directories(${OPENSSL_LINK_DIR}
                 ${WORKFLOW_LIB_DIR})

foreach(output ${PROJECT_OUTPUT})
    add_executable(${output} ${output}_main.cc config/config.cc config/Json.cc)
    target_link_libraries(${output} ${LIB})
endforeach()

