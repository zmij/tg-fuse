# tg-fuse

A FUSE-based virtual filesystem that lets you send files to Telegram contacts using standard Unix file operations.

```bash
# Send a file like you'd copy to any directory
cp vacation_photos.zip /dev/tg/@friend_username

# Send a quick message
echo "Running late, be there in 10!" > /dev/tg/@friend_username/text

# Send to a group chat
cp presentation.pdf /dev/tg/#work_group
```

## Features

- **Native filesystem integration** - Use standard Unix tools (`cp`, `mv`, `rsync`, etc.) to send files
- **Cross-platform** - Works on Linux (libfuse) and macOS (osxfuse/macFUSE)
- **Multiple chat types** - Send to users (`@username`), groups (`#groupname`), or channels (by ID)
- **Automatic file type detection** - Photos, documents, videos handled appropriately
- **Contact management** - Username resolution and caching
- **Secure authentication** - OAuth-based login flow via companion CLI tool
- **Full Telegram API** - Powered by TDLib for complete feature support

## Installation

### Linux
```bash
# Install system dependencies only
sudo apt install libfuse3-dev cmake build-essential  # Ubuntu/Debian
# or
sudo dnf install fuse3-devel cmake gcc-c++           # Fedora

# Build from source - all dependencies fetched automatically
git clone https://github.com/yourusername/tg-fuse
cd tg-fuse
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

### macOS
```bash
# Install system dependencies only
brew install macfuse cmake
# or download macFUSE from: https://osxfuse.github.io/

# Build from source - all dependencies fetched automatically  
git clone https://github.com/yourusername/tg-fuse
cd tg-fuse
mkdir build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
sudo make install
```

## Quick Start

```bash
# Authenticate with Telegram
tg-fuse login

# Mount the filesystem (Linux)
tg-fuse mount /dev/tg

# Mount the filesystem (macOS - requires different path)
tg-fuse mount /tmp/tg

# Start sending files!
cp document.pdf /dev/tg/@colleague        # Linux
cp document.pdf /tmp/tg/@colleague        # macOS
echo "Check this out" > /dev/tg/@colleague/text
```

## How it works

tg-fuse creates a virtual filesystem where each Telegram contact appears as a directory. File operations are translated into TDLib API calls, letting you use familiar Unix tools to interact with Telegram.

**Filesystem structure:**
```
/dev/tg/  (or /tmp/tg on macOS)
├── @username/          # Direct messages
├── #groupname/         # Group chats  
├── -1001234567890/     # Channels/supergroups
└── .meta/              # Control interface
```

## Platform Notes

- **Linux**: Uses `/dev/tg` as the default mount point (requires root for `/dev` access)
- **macOS**: Uses `/tmp/tg` as default due to macOS filesystem restrictions
- Both platforms support custom mount points via `tg-fuse mount <path>`

## Build Requirements

### System Dependencies (install manually)
- **Linux**: libfuse3-dev, cmake, C++20 compiler
- **macOS**: macFUSE, cmake, C++20 compiler (Xcode command line tools)

### Automatically Fetched Dependencies
- **TDLib** - Official Telegram client library
- **nlohmann/json** - JSON parsing for configuration
- **spdlog** - Logging framework
- **CLI11** - Command line argument parsing

CMake's `FetchContent` automatically downloads and builds all third-party dependencies during the build process. No manual dependency management required!

## Architecture

Built with modern C++20 and conditional compilation for cross-platform FUSE support. Uses TDLib's asynchronous API with proper C++ coroutines for handling Telegram operations.

```cpp
#ifdef __APPLE__
    #include <osxfuse/fuse.h>
#else
    #include <fuse3/fuse.h>
#endif
```

## Contributing

Just clone and build! CMake handles all the dependency fetching automatically. No submodules, no manual dependency installation beyond system FUSE libraries.

```bash
git clone https://github.com/yourusername/tg-fuse
cd tg-fuse
mkdir build && cd build
cmake .. && make
```

## License

MIT License - see LICENSE file for details.

---

*Why click through Telegram's interface when you can just `cp` files like a proper Unix user?*
