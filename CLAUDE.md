# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

tg-fuse is a FUSE-based virtual filesystem that enables sending files to Telegram contacts using standard Unix file operations. Users can `cp` files to virtual directories representing Telegram contacts (e.g., `/dev/tg/@username`).

## Build System

This project uses CMake with `FetchContent` for dependency management. Only system-level dependencies (FUSE libraries) need manual installation.

A `Makefile` is provided for convenience with common build targets.

### Building the project

Using the Makefile (recommended):
```bash
make build-debug      # Build debug version in build/debug
make build-release    # Build release version in build/release
make clean-all        # Clean all build directories
make format           # Format all source files with clang-format
```

Using CMake directly:
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)          # Linux
make -j$(sysctl -n hw.ncpu)  # macOS
```

### Installing

```bash
cd build/release
sudo make install
```

### Running the executable

```bash
# Authenticate
tg-fuse login

# Mount filesystem
tg-fuse mount /dev/tg    # Linux
tg-fuse mount /tmp/tg    # macOS
```

## Dependencies

### System dependencies (manual installation required)
- **Linux**: libfuse3-dev, cmake, C++20 compiler
- **macOS**: macFUSE, cmake, C++20 compiler (Xcode command line tools)

### Third-party dependencies (automatically fetched via CMake FetchContent)
- TDLib - Official Telegram client library
- nlohmann/json - JSON parsing
- spdlog - Logging framework
- CLI11 - Command line argument parsing

## Platform-Specific Considerations

### Cross-platform FUSE support
The codebase uses conditional compilation for FUSE headers:
```cpp
#ifdef __APPLE__
    #include <osxfuse/fuse.h>
#else
    #include <fuse3/fuse.h>
#endif
```

### Mount points
- Linux: `/dev/tg` (default, requires root for `/dev` access)
- macOS: `/tmp/tg` (default due to macOS filesystem restrictions)

## Architecture

The application translates filesystem operations into TDLib API calls:
- File operations (`cp`, `mv`, `rsync`) → Telegram file uploads
- Directory structure represents chat hierarchy:
  - `@username/` - Direct messages
  - `#groupname/` - Group chats
  - `-1001234567890/` - Channels/supergroups (by ID)
  - `.meta/` - Control interface

Uses C++20 features with TDLib's asynchronous API and C++ coroutines for handling Telegram operations.

## Development Workflow

Since all dependencies are fetched automatically via CMake, the development process is:
1. Clone repository
2. Install only system FUSE libraries
3. Run CMake (fetches all third-party dependencies)
4. Build and develop

### Running tests

```bash
# From debug build
cd build/debug && ctest

# From release build
cd build/release && ctest
```

### Code formatting

**IMPORTANT**: Always run `clang-format` on modified files before committing.

```bash
# Format all source files (recommended)
make format

# Or format manually
clang-format -i src/main.cpp
```

The project uses a `.clang-format` configuration based on Google style with 120 column limit and 4-space indentation.
