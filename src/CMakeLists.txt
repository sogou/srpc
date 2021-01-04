cmake_minimum_required(VERSION 3.6)

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

if (WIN32)
	set (HAVE_SYS_UIO_H_01 false)
else ()
	set (HAVE_SYS_UIO_H_01 true)
endif ()
set(SNAPPY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/snappy)
configure_file(${SNAPPY_DIR}/cmake/config.h.in ${SNAPPY_DIR}/config.h)
configure_file(${SNAPPY_DIR}/snappy-stubs-public.h.in ${SNAPPY_DIR}/snappy-stubs-public.h)
#find_package(Workflow REQUIRED CONFIG HINTS /usr/lib64/cmake/workflow)
find_package(Workflow REQUIRED CONFIG HINTS ../workflow)

include_directories(
	${OPENSSL_INCLUDE_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	${Protobuf_INCLUDE_DIR}
	${WORKFLOW_INCLUDE_DIR}
	${INC_DIR}/srpc
)

if (WIN32)
	set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   /MP /wd4200")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP /wd4200 /Zc:__cplusplus /std:c++14")
else ()
	set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   -Wall -fPIC -pipe -std=gnu90")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fPIC -pipe -std=c++11 -fno-exceptions")
endif ()

add_subdirectory(generator)

set(SRC
	rpc_buffer.cc
	rpc_global.cc
	rpc_span_policies.cc
)

add_subdirectory(thrift)
add_subdirectory(compress)
add_subdirectory(message)

add_dependencies(thrift LINK_HEADERS)
add_dependencies(compress LINK_HEADERS)
add_dependencies(message LINK_HEADERS)

add_library(
	${PROJECT_NAME} STATIC
	${SRC}
	$<TARGET_OBJECTS:thrift>
	$<TARGET_OBJECTS:compress>
	$<TARGET_OBJECTS:message>
)
add_dependencies(${PROJECT_NAME} LINK_HEADERS)
if (WIN32)
	target_compile_definitions(
		${PROJECT_NAME} PRIVATE
		strdup=_strdup
		strcasecmp=_stricmp
		strncasecmp=_strnicmp
	)
endif ()

install(
	TARGETS ${PROJECT_NAME}
	ARCHIVE
	DESTINATION ${CMAKE_INSTALL_LIBDIR}
	COMPONENT devel
)

if (APPLE)
	set(LIBSO ${LIB_DIR}/libsrpc.a)
else ()
	set(LIBSO ${LIB_DIR}/libsrpc.so)
	add_custom_target(
		SCRIPT_SHARED_LIB ALL
		COMMAND ${CMAKE_COMMAND} -E echo 'GROUP ( libsrpc.a AS_NEEDED ( libz.so libworkflow.so libprotobuf.so ) ) ' > ${LIBSO}
	)
	add_dependencies(SCRIPT_SHARED_LIB ${PROJECT_NAME})
endif ()

install(
	FILES ${LIBSO}
	DESTINATION ${CMAKE_INSTALL_LIBDIR}
	COMPONENT devel
)

