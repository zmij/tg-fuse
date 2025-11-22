.PHONY: build-debug build-release clean-debug clean-release clean-all format help

# Default target
help:
	@echo "Available targets:"
	@echo "  build-debug    - Configure and build debug version in build/debug"
	@echo "  build-release  - Configure and build release version in build/release"
	@echo "  clean-debug    - Clean debug build directory"
	@echo "  clean-release  - Clean release build directory"
	@echo "  clean-all      - Clean both debug and release build directories"
	@echo "  format         - Run clang-format on all source files"

# Detect number of CPU cores for parallel builds
NPROCS := 1
OS := $(shell uname -s)
ifeq ($(OS),Linux)
	NPROCS := $(shell nproc)
endif
ifeq ($(OS),Darwin)
	NPROCS := $(shell sysctl -n hw.ncpu)
endif

# Build debug version
build-debug:
	@mkdir -p build/debug
	@cd build/debug && cmake -DCMAKE_BUILD_TYPE=Debug ../..
	@cd build/debug && $(MAKE) -j$(NPROCS)

# Build release version
build-release:
	@mkdir -p build/release
	@cd build/release && cmake -DCMAKE_BUILD_TYPE=Release ../..
	@cd build/release && $(MAKE) -j$(NPROCS)

# Clean debug build
clean-debug:
	@if [ -d build/debug ]; then \
		cd build/debug && $(MAKE) clean; \
	fi

# Clean release build
clean-release:
	@if [ -d build/release ]; then \
		cd build/release && $(MAKE) clean; \
	fi

# Clean all builds
clean-all: clean-debug clean-release
	@echo "All build directories cleaned"

# Format source code
format:
	@echo "Formatting source files..."
	@find src include tests -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" \) -exec clang-format -i {} +
	@echo "Formatting complete"
