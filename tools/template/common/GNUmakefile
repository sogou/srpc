ROOT_DIR := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
BUILD_DIR := build.cmake
SERVER_OUTPUT := server
CLIENT_OUTPUT := client

.PHONY: all clean

all: base
	make -C $(BUILD_DIR) -f Makefile

base:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake $(ROOT_DIR)

clean:
	make -C $(BUILD_DIR) clean
	rm -rf $(BUILD_DIR)
