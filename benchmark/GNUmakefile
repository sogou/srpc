ROOT_DIR := $(shell dirname $(realpath $(firstword $(MAKEFILE_LIST))))
ALL_TARGETS := all clean
MAKE_FILE := Makefile

DEFAULT_BUILD_DIR := build
BUILD_DIR := $(shell if [ -f $(MAKE_FILE) ]; then echo "."; else echo $(DEFAULT_BUILD_DIR); fi)
CMAKE3 := $(shell if which cmake3>/dev/null ; then echo cmake3; else echo cmake; fi;)

.PHONY: $(ALL_TARGETS)

all:
	mkdir -p $(BUILD_DIR)
ifeq ($(DEBUG),y)
	cd $(BUILD_DIR) && $(CMAKE3) -D CMAKE_BUILD_TYPE=Debug $(ROOT_DIR)
else ifneq ("${Workflow_DIR}workflow", "workflow")
	cd $(BUILD_DIR) && $(CMAKE3) -DWorkflow_DIR:STRING=${Workflow_DIR} $(ROOT_DIR)
else
	cd $(BUILD_DIR) && $(CMAKE3) $(ROOT_DIR)
endif
	make -C $(BUILD_DIR) -f Makefile

clean:
ifeq ($(MAKE_FILE), $(wildcard $(MAKE_FILE)))
	-make -f Makefile clean
else ifeq (build, $(wildcard build))
	-make -C build clean
endif
	rm -rf build

#g++ -o thrift_server thrift_server.cc gen-cpp/*.cpp -O2 -g -lthrift -lthriftnb -levent -lpthread -std=c++11
