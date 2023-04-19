cmake_minimum_required(VERSION 3.6)

project(srpc-ctl
		VERSION 0.10.0
		LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE RelWithDebInfo)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR})

set(generator_code
	"../src/generator/generator.cc"
	"../src/generator/parser.cc")

set(srpc_ctl_code
	"srpc_ctl.cc"
	"srpc_config.cc"
	"srpc_controller.cc"
	"srpc_basic_controller.cc"
	"srpc_rpc_controller.cc"
	"srpc_proxy_controller.cc")

include_directories("../src/")
add_executable(srpc ${srpc_ctl_code} ${generator_code})
target_link_libraries(srpc ${LIBRARY_NAME})

