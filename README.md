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

# IMPORTANT: Ensure macFUSE is compatible with your macOS version.
# If you see "Unsupported macOS Version" or "file system is not available" errors,
# download the latest macFUSE from: https://github.com/osxfuse/osxfuse/releases/latest
# After installation:
#   1. Approve the system extension in System Settings → Privacy & Security
#   2. Reboot if required

# Clone and build
git clone https://github.com/yourusername/tg-fuse
cd tg-fuse
make build-release
cd build/release
sudo make install
```

**For detailed build instructions, troubleshooting, and development setup, see [BUILDING.md](BUILDING.md).**

## Quick Start

### 1. Configure API Credentials (one-time setup)

tg-fuse requires Telegram API credentials. Get them from https://my.telegram.org/apps:

1. Log in with your phone number
2. Create a new application (any name/platform)
3. Copy your `api_id` and `api_hash`

```bash
tg-fuse config set --api-id=YOUR_API_ID --api-hash=YOUR_API_HASH
```

This saves credentials to `~/.config/tg-fuse/config.json`.

### 2. Authenticate with Telegram

```bash
tg-fuse login
```

You'll be prompted for your phone number and verification code.

### 3. Mount and use

```bash
# Mount the filesystem (Linux)
tg-fuse mount /mnt/tg

# Mount the filesystem (macOS)
tg-fuse mount /Volumes/tg

# Start sending files!
cp document.pdf /mnt/tg/@colleague        # Linux
cp document.pdf /Volumes/tg/@colleague    # macOS
echo "Check this out" > /mnt/tg/@colleague/text
```

## How it works

tg-fuse creates a virtual filesystem where Telegram contacts, groups, and channels appear as directories. File operations are translated into TDLib API calls, letting you use familiar Unix tools to interact with Telegram.

**Filesystem structure:**
```
/mnt/tg/  (or /Volumes/tg on macOS)
├── users/
│   ├── alice/              # User directory
│   │   ├── .info           # User information (read-only)
│   │   ├── messages        # Chat messages (read/append)
│   │   ├── files/          # Shared documents (read-only)
│   │   │   ├── 20241205-1430-report.pdf
│   │   │   └── 20241206-0900-notes.docx
│   │   └── media/          # Shared photos/videos (read-only)
│   │       ├── 20241205-1445-photo.jpg
│   │       └── 20241206-1200-video.mp4
│   └── bob/
│       ├── .info
│       ├── messages
│       ├── files/
│       └── media/
├── contacts/
│   ├── alice -> ../users/alice   # Symlinks to contact users
│   └── bob -> ../users/bob
├── groups/
│   ├── family/             # Group directory
│   │   ├── .info           # Group information
│   │   ├── messages        # Group messages (read/append)
│   │   ├── files/          # Shared documents
│   │   └── media/          # Shared photos/videos
│   └── work/
│       ├── .info
│       ├── messages
│       ├── files/
│       └── media/
├── channels/
│   ├── news_channel/       # Channel directory
│   │   ├── .info           # Channel information
│   │   ├── messages        # Channel messages (read/append)
│   │   ├── files/          # Shared documents
│   │   └── media/          # Shared photos/videos
│   └── tech_updates/
│       ├── .info
│       ├── messages
│       ├── files/
│       └── media/
├── @alice -> users/alice   # Symlink for quick access (contacts only)
├── @bob -> users/bob
└── ...
```

**Files and directories:**
- `.info` - Read-only file with entity details (username, name, bio, etc.)
- `messages` - Read recent messages or send new ones (append-only)
- `files/` - Documents shared in the chat (downloadable; upload to send as document)
- `media/` - Photos, videos and animations shared in the chat (downloadable; upload to send as compressed media)

File names in `files/` and `media/` are prefixed with timestamps: `YYYYMMDD-HHMM-original_name.ext`

**Symlinks:** `@<username>` at root and entries in `/contacts/` provide quick access to contact users.

## Sending Files

Copy files directly to chat directories to send them via Telegram:

```bash
# Auto-detect file type (recommended)
cp photo.jpg /mnt/tg/users/alice/      # → sent as compressed photo
cp document.pdf /mnt/tg/users/alice/   # → sent as document
cp notes.txt /mnt/tg/users/alice/      # → sent as text message

# Force specific type
cp image.png /mnt/tg/users/alice/files/   # → sent as document (not compressed)
cp video.mp4 /mnt/tg/users/alice/media/   # → sent as compressed media

# Works with groups and channels too
cp report.pdf /mnt/tg/groups/work/
cp announcement.txt /mnt/tg/channels/news/
```

**Auto-detection rules:**
- `.txt`, `.md` files with valid UTF-8 content → sent as text message (split if >4096 chars)
- `.jpg`, `.jpeg`, `.png`, `.gif`, `.webp`, `.mp4`, `.mov`, `.avi`, `.mkv`, `.webm` → sent as compressed media
- All other files → sent as document

**Upload deduplication:** File hashes are cached, so sending the same file to multiple chats reuses the upload.

**File size limits:** 2 GB for regular users, 4 GB for Telegram Premium.

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
