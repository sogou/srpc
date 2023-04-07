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
	find_package(Protobuf REQUIRED CONFIG)
else ()
	find_package(Protobuf 3.5.0 REQUIRED)
endif ()

if (WIN32)
	set (HAVE_SYS_UIO_H_01 false)
else ()
	set (HAVE_SYS_UIO_H_01 true)
endif ()

if (WITH_VCPKG_TOOLCHAIN)
	find_package(Workflow REQUIRED CONFIG)
else ()
	# Try to find the package if workflow is nor installed, otherwise proceed with find_package()
	if (NOT WORKFLOW_INSTALLED)
		find_package(Workflow REQUIRED CONFIG HINTS ../workflow)
	endif ()

	if (NOT SNAPPY_INSTALLED)
		set(SNAPPY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/snappy)
		configure_file(${SNAPPY_DIR}/cmake/config.h.in ${SNAPPY_DIR}/config.h)
		configure_file(${SNAPPY_DIR}/snappy-stubs-public.h.in ${SNAPPY_DIR}/snappy-stubs-public.h)
	else ()
		find_package(Snappy 1.1.6 REQUIRED CONFIG)
		set(SNAPPY_LIB snappy)
	endif ()

	if (LZ4_INSTALLED)
		set(LZ4_LIB lz4)
	endif ()
endif()

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
	rpc_basic.cc
	rpc_global.cc
)

add_subdirectory(module)
add_subdirectory(var)
add_subdirectory(thrift)
add_subdirectory(compress)
add_subdirectory(message)
add_subdirectory(http)

add_dependencies(module LINK_HEADERS)
add_dependencies(var LINK_HEADERS)
add_dependencies(thrift LINK_HEADERS)
add_dependencies(compress LINK_HEADERS)
add_dependencies(message LINK_HEADERS)
add_dependencies(http LINK_HEADERS)

if (WIN32)
	add_library(
		${PROJECT_NAME} STATIC
		${SRC}
		$<TARGET_OBJECTS:module>
		$<TARGET_OBJECTS:var>
		$<TARGET_OBJECTS:thrift>
		$<TARGET_OBJECTS:compress>
		$<TARGET_OBJECTS:message>
		$<TARGET_OBJECTS:http>
	)

	add_dependencies(${PROJECT_NAME} LINK_HEADERS)

	target_compile_definitions(
		${PROJECT_NAME} PRIVATE
		strdup=_strdup
		strcasecmp=_stricmp
		strncasecmp=_strnicmp
	)

	install(
		TARGETS ${PROJECT_NAME}
		ARCHIVE
		DESTINATION ${CMAKE_INSTALL_LIBDIR}
		COMPONENT devel
	)

else ()
	set(STATIC_LIB_NAME ${PROJECT_NAME}-static)
	set(SHARED_LIB_NAME ${PROJECT_NAME}-shared)
	link_directories(${OPENSSL_LINK_DIR} ${WORKFLOW_LIB_DIR} ${Protobuf_LIB_DIR})

	add_library(
		${STATIC_LIB_NAME} STATIC
		${SRC}
		$<TARGET_OBJECTS:module>
		$<TARGET_OBJECTS:var>
		$<TARGET_OBJECTS:thrift>
		$<TARGET_OBJECTS:compress>
		$<TARGET_OBJECTS:message>
		$<TARGET_OBJECTS:http>
	)

	add_library(
		${SHARED_LIB_NAME} SHARED
		${SRC}
		$<TARGET_OBJECTS:module>
		$<TARGET_OBJECTS:var>
		$<TARGET_OBJECTS:thrift>
		$<TARGET_OBJECTS:compress>
		$<TARGET_OBJECTS:message>
		$<TARGET_OBJECTS:http>
	)

	target_link_libraries(${SHARED_LIB_NAME} 
						  OpenSSL::SSL
						  OpenSSL::Crypto
						  pthread
						  protobuf
						  workflow
						  z ${SNAPPY_LIB} ${LZ4_LIB})
		
	set_target_properties(${STATIC_LIB_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
	set_target_properties(${SHARED_LIB_NAME} PROPERTIES OUTPUT_NAME ${PROJECT_NAME} VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR} )

	add_dependencies(${STATIC_LIB_NAME} LINK_HEADERS)
	add_dependencies(${SHARED_LIB_NAME} LINK_HEADERS)

	install(
		TARGETS ${STATIC_LIB_NAME}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		COMPONENT devel
	)

	install(
		TARGETS ${SHARED_LIB_NAME}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		COMPONENT devel
	)

endif ()

