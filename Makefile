.PHONY: all release debug release-config debug-config
.PHONY: lint
.PHONY: clean

CXX:=$(shell which g++-14)
export CXX

ifeq ($(CXX),)
$(error Failed to find g++-14)
endif

CMAKE=$(shell which cmake)
CORES?=$(shell nproc)

BUILD_DIR=$(CURDIR)/build
RELEASE_DIR=$(BUILD_DIR)/release
DEBUG_DIR=$(BUILD_DIR)/debug
LINT_DIR=$(BUILD_DIR)/lint

CPPCHECK_PARAMS=\
	--language=c++ --std=c++23 \
	--library=posix \
	--inconclusive --force --inline-suppr \
	--enable=all \
	--suppress=funcArgNamesDifferent \
	--suppress=missingIncludeSystem \
	--suppress=checkersReport \
	--checkers-report=$(LINT_DIR)/details.txt \
	-I $(CURDIR)/src/

.DEFAULT_GOAL=release

all: release debug

release: release-config
	$(info Making release build)
	@+$(CMAKE) --build $(RELEASE_DIR) -t cpp-threading  -j$(CORES)

debug: debug-config
	$(info Making debug build)
	@+$(CMAKE) --build $(DEBUG_DIR) -t cpp-threading  -j$(CORES)

clean:
	rm -rf $(BUILD_DIR)

release-config:
	$(info Generating release cmake build config)
	@+$(CMAKE) -DCMAKE_BUILD_TYPE=Release -S $(CURDIR) -B $(RELEASE_DIR)

debug-config:
	$(info Generating debug cmake build config)
	@+$(CMAKE) -DCMAKE_BUILD_TYPE=Debug -S $(CURDIR) -B $(DEBUG_DIR)

lint:
	@mkdir -p $(LINT_DIR)
	cppcheck $(CPPCHECK_PARAMS) --xml $(CURDIR)/src/ 2> $(LINT_DIR)/cpp-threading.xml
	cppcheck-htmlreport \
		--title=cpp-threading \
		--file=$(LINT_DIR)/cpp-threading.xml \
		--source-dir=$(CURDIR)/src/ \
		--report-dir=$(LINT_DIR)/cpp-threading
