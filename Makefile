.PHONY: all release debug
.PHONY: clean

CMAKE=$(shell which cmake)
CORES?=$(shell nproc)
BUILD_DIR=$(CURDIR)/build
RELEASE_DIR=$(BUILD_DIR)/release
DEBUG_DIR=$(BUILD_DIR)/debug

.DEFAULT_GOAL=release

all: release debug

release: $(RELEASE_DIR)
	$(info Making release build)
	@+$(CMAKE) --build $< --config Release --target all  -j$(CORES) --

debug: $(DEBUG_DIR)
	$(info Making debug build)
	@+$(CMAKE) --build $< --config Debug --target all  -j$(CORES) --

clean:
	rm -rf $(BUILD_DIR)

$(RELEASE_DIR): CMakeLists.txt
	$(info Generating release cmake build config)
	@+$(CMAKE) -DCMAKE_BUILD_TYPE:STRING=Release -S $(CURDIR) -B $@

$(DEBUG_DIR): CMakeLists.txt
	$(info Generating debug cmake build config)
	@+$(CMAKE) -DCMAKE_BUILD_TYPE:STRING=Debug -S $(CURDIR) -B $@
