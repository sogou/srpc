cmake_minimum_required(VERSION 3.6)
project(http)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(SRC
	http_module.cc
	http_task.cc
	http_client.cc
	http_server.cc
)

add_library(${PROJECT_NAME} OBJECT ${SRC})
if (WIN32)
	target_compile_definitions(
		${PROJECT_NAME} PRIVATE
		strdup=_strdup
		strcasecmp=_stricmp
		strncasecmp=_strnicmp
	)
endif ()

