.PHONY: all debug clean

CMAKE=$(shell which cmake)
CORES=$(shell nproc)
BUILD_DIR=$(CURDIR)/build
BIN_DIR=$(CURDIR)/bin

ifdef DEBUG
BUILD_MODE=Debug
else
BUILD_MODE=Release
endif

all: $(BUILD_DIR)
	$(info Making '$(BUILD_MODE)' build)
	@+$(CMAKE) --build $(BUILD_DIR) --config $(BUILD_MODE) --target all  -j$(CORES) --

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

$(BUILD_DIR): CMakeLists.txt
	$(info Generating '$(BUILD_MODE)' cmake build config)
	@+$(CMAKE) \
		-DCMAKE_BUILD_TYPE:STRING=$(BUILD_MODE) \
		-DCMAKE_EXPORT_COMPILE_COMMANDS:BOOL=TRUE \
		-DCMAKE_C_COMPILER:FILEPATH=$(CC) \
		-DCMAKE_CXX_COMPILER:FILEPATH=$(CXX) \
		-S $(CURDIR) \
		-B $(BUILD_DIR) \
		-G "Unix Makefiles"

