cmake_minimum_required(VERSION 3.6)
set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "build type")

project(srpc
	VERSION 0.9.6
	LANGUAGES C CXX)

#### CHECK

include(CheckIncludeFile)
include(CheckIncludeFileCXX)

check_include_file("lz4.h" LZ4_INSTALLED)
if (NOT LZ4_INSTALLED)
	if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lz4/lib/lz4.h")
		message( FATAL_ERROR "Lz4 is neither installed not in third_party!")
	endif ()
else ()
	if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/lz4/lib/lz4.h")
		message("Lz4 third_party FOUND. Use for source code dependencies.")
		set(LZ4_INSTALLED 0)
	endif ()
endif ()

check_include_file_cxx("snappy.h" SNAPPY_INSTALLED)
if (NOT SNAPPY_INSTALLED)
	if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/snappy/cmake")
		message( FATAL_ERROR "Snappy is neither installed not in third_party!")
	endif ()
else ()
	if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/snappy/cmake")
		message("Snappy third_party FOUND. Use for source code dependencies.")
		set(SNAPPY_INSTALLED 0)
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

add_subdirectory(src)

#### CONFIG

include(CMakePackageConfigHelpers)
set(CONFIG_INC_DIR ${INC_DIR})
set(CONFIG_LIB_DIR ${LIB_DIR})
if(NOT VCPKG_TOOLCHAIN)
	set(CONFIG_BIN_DIR ${BIN_DIR})	
else()
	set(CONFIG_BIN_DIR ${CMAKE_INSTALL_PREFIX}/tools/srpc)
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

install(
	FILES ${CMAKE_CONFIG_INSTALL_FILE}
	DESTINATION ${CMAKE_CONFIG_INSTALL_DIR}
	COMPONENT devel
	RENAME ${PROJECT_NAME}-config.cmake
)

#### PACK

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Sogou RPC")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})

# set(CPACK_RPM_PRE_INSTALL_SCRIPT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/pre_script.sh)
# set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE ${CMAKE_CURRENT_SOURCE_DIR}/post_script.sh)

execute_process(
	COMMAND git log -n1 --format=%ad --date=short
	RESULT_VARIABLE GIT_LOG_RET
	OUTPUT_VARIABLE GIT_YMD
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

if (NOT GIT_LOG_RET EQUAL 0)
	set(GIT_YMD 0)
endif()

string(REPLACE "-" "" RELEASE_LABEL ${GIT_YMD})
set(CPACK_RPM_PACKAGE_RELEASE ${RELEASE_LABEL})

set(CPACK_RPM_COMPONENT_INSTALL ON)
set(CPACK_COMPONENTS_ALL devel)
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_RELEASE_DIST ON)

set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION /usr/lib64/cmake)

set(PB_PACKAGE_REQUIRED "protobuf-devel")

set(CPACK_RPM_DEVEL_PACKAGE_REQUIRES "workflow-devel >= 0.9.9, ${PB_PACKAGE_REQUIRED}")

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

set(CPACK_RPM_SPEC_MORE_DEFINE "%define __spec_install_post /bin/true") # disable strip
set(CPACK_GENERATOR "RPM")

include(CPack)

