cmake_minimum_required(VERSION 3.6)
project(srpc_generator)

set(SRC
	compiler.cc
	generator.cc
	parser.cc
)

add_executable(${PROJECT_NAME} ${SRC})
if (WIN32)
	target_compile_definitions(
		${PROJECT_NAME} PRIVATE
		getcwd=_getcwd
		strdup=_strdup
		strcasecmp=_stricmp
		strncasecmp=_strnicmp
	)
endif ()

install(
	TARGETS ${PROJECT_NAME}
	RUNTIME
	DESTINATION ${CMAKE_INSTALL_BINDIR}
	COMPONENT devel
)

