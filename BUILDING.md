# Building tg-fuse

This document provides detailed instructions for building tg-fuse from source.

## Prerequisites

### Linux (Ubuntu/Debian)

```bash
sudo apt install libfuse3-dev cmake build-essential pkg-config
```

### Linux (Fedora)

```bash
sudo dnf install fuse3-devel cmake gcc-c++ pkg-config
```

### macOS

```bash
# Install Homebrew if you haven't already
# See https://brew.sh

brew install macfuse cmake

# Or download macFUSE from: https://osxfuse.github.io/
```

**Note**: macFUSE requires a kernel extension. You may need to grant permissions in System Preferences → Security & Privacy after installation.

## Quick Start

The easiest way to build the project is using the provided Makefile:

```bash
# Clone the repository
git clone https://github.com/zmij/tg-fuse
cd tg-fuse

# Build debug version
make build-debug

# Or build release version
make build-release

# Install (from release build)
cd build/release
sudo make install
```

## Build Targets

The Makefile provides several convenient targets:

| Target | Description |
|--------|-------------|
| `make build-debug` | Build debug version in `build/debug` |
| `make build-release` | Build release version in `build/release` |
| `make clean-debug` | Clean debug build directory |
| `make clean-release` | Clean release build directory |
| `make clean-all` | Clean both debug and release directories |
| `make format` | Format all source files with clang-format |
| `make help` | Show available targets |

## Manual Build (Using CMake Directly)

If you prefer to use CMake directly:

```bash
# Create build directory
mkdir build && cd build

# Configure (Debug)
cmake -DCMAKE_BUILD_TYPE=Debug ..

# Or configure (Release)
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build (Linux)
make -j$(nproc)

# Build (macOS)
make -j$(sysctl -n hw.ncpu)

# Install
sudo make install
```

## Running Tests

```bash
# If using Makefile builds
cd build/debug
ctest

# Or from release
cd build/release
ctest

# Verbose output
ctest --verbose
```

## Dependencies

### System Dependencies (Manual Installation)

These must be installed on your system before building:

- **FUSE library**: libfuse3-dev (Linux) or macFUSE (macOS)
- **CMake**: Version 3.20 or later
- **C++20 compiler**: GCC 10+, Clang 12+, or Xcode Command Line Tools
- **pkg-config**: For finding FUSE libraries

### Third-Party Dependencies (Automatic)

These are automatically downloaded and built by CMake using `FetchContent`:

- **TDLib** (v1.8.29) - Official Telegram client library
- **nlohmann/json** (v3.11.3) - JSON parsing
- **spdlog** (v1.13.0) - Logging framework
- **CLI11** (v2.4.1) - Command line argument parsing
- **GoogleTest** (v1.14.0) - Unit testing framework

The first build will take longer as CMake downloads and compiles these dependencies. Subsequent builds will be faster as the dependencies are cached.

## Build Configuration Options

### Build Types

- **Debug**: Includes debug symbols, no optimisation (`-g`)
- **Release**: Optimised build (`-O3`), no debug symbols

### Platform-Specific Notes

#### Linux

- Default mount point: `/mnt/tg`
- Uses FUSE3 API (version 35)

#### macOS

- Default mount point: `/Volumes/tg`
- Uses macFUSE/osxfuse API (version 29)
- Requires macFUSE kernel extension to be loaded

## Troubleshooting

### CMake can't find FUSE

**Linux**: Make sure `libfuse3-dev` and `pkg-config` are installed:
```bash
pkg-config --modversion fuse3
```

**macOS**: Ensure macFUSE is installed:
```bash
brew list macfuse
# or check /usr/local/include/osxfuse
```

### TDLib build fails

TDLib requires significant compilation time and resources. Ensure you have:
- At least 2GB of free disk space
- Sufficient RAM (4GB+ recommended)
- A stable internet connection for downloading dependencies

### Compiler errors about C++20

Ensure your compiler supports C++20:
```bash
# GCC
g++ --version  # Should be 10 or later

# Clang
clang++ --version  # Should be 12 or later
```

### macFUSE kernel extension not loaded

After installing macFUSE on macOS, you may need to:
1. Restart your computer
2. Go to System Preferences → Security & Privacy
3. Allow the system extension from the developer
4. Restart again if prompted

## Development Builds

For active development:

```bash
# Build debug version
make build-debug

# Make your changes, then format code
make format

# Rebuild
make build-debug

# Run tests
cd build/debug && ctest
```

## Clean Builds

If you encounter build issues, try a clean build:

```bash
# Clean everything
make clean-all

# Or remove build directories entirely
rm -rf build/

# Then rebuild
make build-debug
```

## Cross-Platform Development

The codebase uses conditional compilation for platform-specific code:

```cpp
#ifdef __APPLE__
    // macOS-specific code
    #include <osxfuse/fuse.h>
#else
    // Linux-specific code
    #include <fuse3/fuse.h>
#endif
```

Both platforms are fully supported and tested.

