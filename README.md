# tg-fuse

A FUSE-based virtual filesystem that lets you send files to Telegram contacts using standard Unix file operations.

```bash
# Send a file like you'd copy to any directory
cp vacation_photos.zip /mnt/tg/@friend_username

# Send a quick message
echo "Running late, be there in 10!" > /mnt/tg/@friend_username/text

# Send to a group chat
cp presentation.pdf /mnt/tg/#work_group
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

### Quick Install

```bash
# Install system dependencies
# Ubuntu/Debian
sudo apt install libfuse3-dev cmake build-essential pkg-config

# Fedora
sudo dnf install fuse3-devel cmake gcc-c++ pkg-config

# macOS
brew install macfuse cmake

# Clone and build
git clone https://github.com/yourusername/tg-fuse
cd tg-fuse
make build-release
cd build/release
sudo make install
```

**For detailed build instructions, troubleshooting, and development setup, see [BUILDING.md](BUILDING.md).**

## Quick Start

```bash
# Authenticate with Telegram
tg-fuse login

# Mount the filesystem (Linux)
tg-fuse mount /mnt/tg

# Mount the filesystem (macOS - requires different path)
tg-fuse mount /Volumes/tg

# Start sending files!
cp document.pdf /mnt/tg/@colleague        # Linux
cp document.pdf /Volumes/tg/@colleague    # macOS
echo "Check this out" > /mnt/tg/@colleague/text
```

## How it works

tg-fuse creates a virtual filesystem where each Telegram contact appears as a directory. File operations are translated into TDLib API calls, letting you use familiar Unix tools to interact with Telegram.

**Filesystem structure:**
```
/mnt/tg/  (or /Volumes/tg on macOS)
├── @username/          # Direct messages
├── #groupname/         # Group chats
├── -1001234567890/     # Channels/supergroups
└── .meta/              # Control interface
```

## Platform Notes

- **Linux**: Uses `/mnt/tg` as the default mount point
- **macOS**: Uses `/Volumes/tg` as the default mount point
- Both platforms support custom mount points via `tg-fuse mount <path>`

## Dependencies

**System dependencies** (install manually):
- Linux: libfuse3-dev, cmake, C++20 compiler, pkg-config
- macOS: macFUSE, cmake, C++20 compiler (Xcode command line tools)

**Third-party dependencies** (fetched automatically):
- TDLib, nlohmann/json, spdlog, CLI11, GoogleTest

CMake automatically downloads and builds all third-party dependencies. See [BUILDING.md](BUILDING.md) for details.

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

Contributions are welcome! The build system is simple:

```bash
git clone https://github.com/yourusername/tg-fuse
cd tg-fuse
make build-debug        # Build with debug symbols
make format             # Format code before committing
cd build/debug && ctest # Run tests
```

See [BUILDING.md](BUILDING.md) for development setup and [CLAUDE.md](CLAUDE.md) for project structure guidance.

## License

MIT License - see LICENSE file for details.

---

*Why click through Telegram's interface when you can just `cp` files like a proper Unix user?*
