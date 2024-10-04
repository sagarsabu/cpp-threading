.PHONY: all release debug
.PHONY: lint
.PHONY: clean

CMAKE=$(shell which cmake)
CORES?=$(shell nproc)
BUILD_DIR=$(CURDIR)/build
RELEASE_DIR=$(BUILD_DIR)/release
DEBUG_DIR=$(BUILD_DIR)/debug
LINT_DIR=$(BUILD_DIR)/lint

CPPCHECK_PARAMS=\
	--language=c++ --std=c++20 \
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
	@+$(CMAKE) -DCMAKE_BUILD_TYPE=Release -S $(CURDIR) -B $@

$(DEBUG_DIR): CMakeLists.txt
	$(info Generating debug cmake build config)
	@+$(CMAKE) -DCMAKE_BUILD_TYPE=Debug -S $(CURDIR) -B $@

lint:
	@mkdir -p $(LINT_DIR)
	cppcheck $(CPPCHECK_PARAMS) --xml $(CURDIR)/src/ 2> $(LINT_DIR)/cpp-threading.xml
	cppcheck-htmlreport \
		--title=cpp-threading \
		--file=$(LINT_DIR)/cpp-threading.xml \
		--source-dir=$(CURDIR)/src/ \
		--report-dir=$(LINT_DIR)/cpp-threading
