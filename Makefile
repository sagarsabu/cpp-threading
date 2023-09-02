.PHONY: all debug clean

CMAKE=$(shell which cmake)
CORES=$(shell nproc)
BUILD_DIR=$(CURDIR)/build
BIN_DIR=$(CURDIR)/bin

all: $(BUILD_DIR)
	+$(CMAKE) --build $(BUILD_DIR) --config Release --target all  -j$(CORES) --

debug: $(BUILD_DIR)
	+$(CMAKE) --build $(BUILD_DIR) --config Debug --target all  -j$(CORES) --

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

$(BUILD_DIR): CMakeLists.txt
	+$(CMAKE) -B $(BUILD_DIR)

