ROOT_DIR := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
BUILD_DIR := build.cmake

.PHONY: all clean

all: base
	make -C $(BUILD_DIR) -f Makefile

base:
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake $(ROOT_DIR)

clean:
ifeq ($(BUILD_DIR), $(wildcard $(BUILD_DIR)))
	make -C $(BUILD_DIR) clean
	rm -rf $(BUILD_DIR)
endif

