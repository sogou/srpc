ROOT_DIR := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
BUILD_DIR := build.cmake
OUTPUT := srpc-ctl

.PHONY: all clean

all: base
	make -C $(BUILD_DIR) -f Makefile

base:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -D CMAKE_BUILD_TYPE=Debug $(ROOT_DIR)

clean:
	rm -rf $(BUILD_DIR)

