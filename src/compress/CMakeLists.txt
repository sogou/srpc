cmake_minimum_required(VERSION 3.6)
project(compress)

set(SRC
	rpc_compress.cc
	rpc_compress_snappy.cc
)

if(NOT VCPKG_TOOLCHAIN)
	set(SNAPPY_SRC
		../../third_party/snappy/config.h
		../../third_party/snappy/snappy-internal.h
		../../third_party/snappy/snappy-sinksource.cc
		../../third_party/snappy/snappy-sinksource.h
		../../third_party/snappy/snappy-stubs-internal.cc
		../../third_party/snappy/snappy-stubs-internal.h
		../../third_party/snappy/snappy-stubs-public.h
		../../third_party/snappy/snappy.cc
		../../third_party/snappy/snappy.h
	)

	set(LZ4_SRC
		../../third_party/lz4/lib/lz4.c
		../../third_party/lz4/lib/lz4.h
		../../third_party/lz4/lib/lz4hc.h
		../../third_party/lz4/lib/lz4hc.c
		../../third_party/lz4/lib/lz4frame.h
		../../third_party/lz4/lib/lz4frame.c
		../../third_party/lz4/lib/xxhash.c
		../../third_party/lz4/lib/xxhash.h
	)
	add_library(
		${PROJECT_NAME} OBJECT
		${SRC}
		${SNAPPY_SRC}
		${LZ4_SRC}
	)
else()
	add_library(
		${PROJECT_NAME} OBJECT
		${SRC}
	)
	target_link_libraries(${PROJECT_NAME} lz4 snappy)
endif()



#add_library(${PROJECT_NAME} OBJECT ${SRC})

