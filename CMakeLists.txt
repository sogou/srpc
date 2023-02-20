cmake_minimum_required(VERSION 3.6)
set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "build type")
set(CMAKE_SKIP_RPATH TRUE)

project(srpc
	VERSION 0.9.8
	LANGUAGES C CXX)
	
###Options

if (WIN32)
	option(SRPC_BUILD_STATIC_RUNTIME "Use static runtime" ON)
endif ()

#### CHECK

include(CheckIncludeFile)
include(CheckIncludeFileCXX)

find_library(LZ4_LIBRARY NAMES lz4)
check_include_file("lz4.h" LZ4_INSTALLED)
if (NOT LZ4_INSTALLED AND ${LZ4_LIBRARY} STREQUAL "LZ4_LIBRARY-NOTFOUND")
	if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lz4/lib/lz4.h")
		message( FATAL_ERROR "Lz4 is neither installed not in third_party!")
	endif ()
else ()
	if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lz4/lib/lz4.h")
		message("Lz4 third_party FOUND. Use for source code dependencies.")
		set(LZ4_INSTALLED 0 CACHE INTERNAL "check_lz4_installed")
	else ()
		find_path(LZ4_INCLUDE_DIR NAMES "lz4.h")
		include_directories(${LZ4_INCLUDE_DIR})
		set(LZ4_INSTALLED 1 CACHE INTERNAL "check_lz4_installed")
	endif ()
endif ()

find_package(Snappy)
check_include_file_cxx("snappy.h" SNAPPY_INSTALLED)
if (NOT SNAPPY_INSTALLED AND NOT ${Snappy_FOUND})
	if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/snappy/cmake")
		message( FATAL_ERROR "Snappy is neither installed not in third_party!")
	endif ()
else ()
	if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/snappy/cmake")
		message("Snappy third_party FOUND. Use for source code dependencies.")
		set(SNAPPY_INSTALLED 0 CACHE INTERNAL "check_snappy_installed")
	else ()
		find_path(Snappy_INCLUDE_DIR NAMES "snappy.h")
		include_directories(${Snappy_INCLUDE_DIR})
		set(SNAPPY_INSTALLED 1 CACHE INTERNAL "check_snappy_installed")
	endif ()
endif ()

check_include_file_cxx("workflow/Workflow.h" WORKFLOW_INSTALLED)
if (NOT WORKFLOW_INSTALLED)
	if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/workflow/workflow-config.cmake.in")
		message( FATAL_ERROR "Workflow is neither installed not in third_party!")
	endif ()
else ()
	if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/workflow/workflow-config.cmake.in")
		message("Workflow third_party FOUND. Use for source code dependencies.")
		set(WORKFLOW_INSTALLED 0)
	endif ()
endif ()

find_program(PROTOC "protoc")
if(${PROTOC} STREQUAL "PROTOC-NOTFOUND")
    message( FATAL_ERROR "Protobuf compiler is missing!")
endif()

#### PREPARE

set(INC_DIR ${PROJECT_SOURCE_DIR}/_include CACHE PATH "srpc inc")
set(LIB_DIR ${PROJECT_SOURCE_DIR}/_lib CACHE PATH "srpc lib")
set(BIN_DIR ${PROJECT_SOURCE_DIR}/_bin CACHE PATH "srpc bin")

include(GNUInstallDirs)

set(CMAKE_CONFIG_INSTALL_FILE ${PROJECT_BINARY_DIR}/config.toinstall.cmake)
set(CMAKE_CONFIG_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${LIB_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${LIB_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BIN_DIR})

add_custom_target(
	LINK_HEADERS ALL
	COMMENT "link headers..."
)

INCLUDE(CMakeLists_Headers.txt)

macro(makeLink src dest target)
	add_custom_command(
		TARGET ${target} PRE_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${src} ${dest}
		DEPENDS ${dest}
	)
endmacro()

add_custom_command(
	TARGET LINK_HEADERS PRE_BUILD
	COMMAND ${CMAKE_COMMAND} -E make_directory ${INC_DIR}/${PROJECT_NAME}
)

foreach(header_file ${INCLUDE_HEADERS})
	string(REPLACE "/" ";" arr ${header_file})
	list(GET arr -1 file_name)
	makeLink(${PROJECT_SOURCE_DIR}/${header_file} ${INC_DIR}/${PROJECT_NAME}/${file_name} LINK_HEADERS)
endforeach()

if (WIN32)
	if (SRPC_BUILD_STATIC_RUNTIME)
		set(CompilerFlags
				CMAKE_CXX_FLAGS
				CMAKE_CXX_FLAGS_DEBUG
				CMAKE_CXX_FLAGS_RELEASE
				CMAKE_CXX_FLAGS_MINSIZEREL
				CMAKE_C_FLAGS
				CMAKE_C_FLAGS_DEBUG
				CMAKE_C_FLAGS_RELEASE
				CMAKE_C_FLAGS_MINSIZEREL
				)
		foreach(CompilerFlag ${CompilerFlags})
			string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
		endforeach ()
	endif ()
endif ()

add_subdirectory(src)

#### CONFIG

include(CMakePackageConfigHelpers)
set(CONFIG_INC_DIR ${INC_DIR})
set(CONFIG_LIB_DIR ${LIB_DIR})

if (VCPKG_TOOLCHAIN AND EXISTS "${CMAKE_INSTALL_PREFIX}/tools/srpc")
	set(CONFIG_BIN_DIR ${CMAKE_INSTALL_PREFIX}/tools/srpc)
	set(WITH_VCPKG_TOOLCHAIN 1 CACHE INTERNAL "build_with_vcpkg_toolchain")
	message("Install with VCPKG toolchain. Dir ${CMAKE_INSTALL_PREFIX}/tools/srpc.")
else()
	set(CONFIG_BIN_DIR ${BIN_DIR})
	set(WITH_VCPKG_TOOLCHAIN 0 CACHE INTERNAL "build_with_vcpkg_toolchain")
endif()

configure_package_config_file(
	${PROJECT_NAME}-config.cmake.in
	${PROJECT_SOURCE_DIR}/${PROJECT_NAME}-config.cmake
	INSTALL_DESTINATION ${CMAKE_CONFIG_INSTALL_DIR}
	PATH_VARS CONFIG_INC_DIR CONFIG_LIB_DIR CONFIG_BIN_DIR
)

set(CONFIG_INC_DIR ${CMAKE_INSTALL_INCLUDEDIR})
set(CONFIG_LIB_DIR ${CMAKE_INSTALL_LIBDIR})
set(CONFIG_BIN_DIR ${CMAKE_INSTALL_BINDIR})
configure_package_config_file(
	${PROJECT_NAME}-config.cmake.in
	${CMAKE_CONFIG_INSTALL_FILE}
	INSTALL_DESTINATION ${CMAKE_CONFIG_INSTALL_DIR}
	PATH_VARS CONFIG_INC_DIR CONFIG_LIB_DIR CONFIG_BIN_DIR
)

write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
	VERSION ${WORKFLOW_VERSION}
	COMPATIBILITY AnyNewerVersion
)

install(
	FILES ${CMAKE_CONFIG_INSTALL_FILE}
	DESTINATION ${CMAKE_CONFIG_INSTALL_DIR}
	COMPONENT devel
	RENAME ${PROJECT_NAME}-config.cmake
)

install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
	DESTINATION ${CMAKE_CONFIG_INSTALL_DIR}
	COMPONENT devel
)

install(
	FILES ${INCLUDE_HEADERS}
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}
	COMPONENT devel
)

install(
	FILES README.md
	DESTINATION "${CMAKE_INSTALL_DOCDIR}-${PROJECT_VERSION}"
	COMPONENT devel
)
