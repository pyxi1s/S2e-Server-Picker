BUILD_DIR ?= build
BUILD_TYPE ?= Release
GENERATOR ?=
JOBS ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 2)
TARGET := deadlock-server-picker

CMAKE_GEN := $(if $(GENERATOR),-G "$(GENERATOR)",)

.PHONY: all build debug configure run rebuild clean help

all: build

configure:
	cmake -S . -B $(BUILD_DIR) $(CMAKE_GEN) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)

build: configure
	cmake --build $(BUILD_DIR) --config $(BUILD_TYPE) -j $(JOBS)

debug:
	$(MAKE) build BUILD_TYPE=Debug BUILD_DIR=build-debug

run: build
	./$(BUILD_DIR)/$(TARGET)

rebuild: clean build

clean:
	rm -rf build build-debug

help:
	@echo "Targets:"
	@echo "  build    Configure and compile (default, Release)"
	@echo "  debug    Debug build in build-debug/"
	@echo "  run      Build, then run (asks for sudo)"
	@echo "  rebuild  Clean, then build"
	@echo "  clean    Remove build directories"
	@echo "Variables: BUILD_TYPE=Release|Debug  BUILD_DIR=build  GENERATOR=Ninja  JOBS=N"
