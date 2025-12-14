#pragma once

// Platform-specific FUSE includes
#ifdef __APPLE__
#include <fuse/fuse.h>
#define TG_FUSE_VERSION 2
#else
#include <fuse3/fuse.h>
#define TG_FUSE_VERSION 3
#endif

#include <sys/stat.h>
#include <functional>
#include <string>

namespace tgfuse {

/// FUSE version enumeration
enum class FuseVersion {
    FUSE2,  // macFUSE
    FUSE3   // libfuse3
};

/// Get the FUSE version at compile time
constexpr FuseVersion get_fuse_version() {
#if TG_FUSE_VERSION == 2
    return FuseVersion::FUSE2;
#else
    return FuseVersion::FUSE3;
#endif
}

/// Platform-independent directory filler function type
/// @param name Entry name
/// @param stbuf Optional stat buffer (can be nullptr)
/// @return 0 on success, 1 to stop iteration
using DirFiller = std::function<int(const char* name, const struct stat* stbuf)>;

/// Abstract FUSE operations interface
///
/// This interface provides platform-agnostic FUSE operations.
/// The platform adapter translates between this interface and
/// the platform-specific FUSE callbacks.
class FuseOperations {
public:
    virtual ~FuseOperations() = default;

    /// Get file attributes
    /// @param path Path to the file/directory
    /// @param stbuf Stat buffer to fill
    /// @return 0 on success, negative errno on error
    virtual int getattr(const char* path, struct stat* stbuf) = 0;

    /// Read directory contents
    /// @param path Path to the directory
    /// @param filler Function to add entries
    /// @param offset Directory offset
    /// @return 0 on success, negative errno on error
    virtual int readdir(const char* path, DirFiller filler, off_t offset) = 0;

    /// Read symlink target
    /// @param path Path to the symlink
    /// @param buf Buffer to store the target
    /// @param size Buffer size
    /// @return 0 on success, negative errno on error
    virtual int readlink(const char* path, char* buf, size_t size) = 0;

    /// Open a file
    /// @param path Path to the file
    /// @param fi File info
    /// @return 0 on success, negative errno on error
    virtual int open(const char* path, struct fuse_file_info* fi) = 0;

    /// Read file data
    /// @param path Path to the file
    /// @param buf Buffer to store data
    /// @param size Number of bytes to read
    /// @param offset Read offset
    /// @param fi File info
    /// @return Number of bytes read, or negative errno on error
    virtual int read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) = 0;

    /// Release (close) a file
    /// @param path Path to the file
    /// @param fi File info
    /// @return 0 on success, negative errno on error
    virtual int release(const char* path, struct fuse_file_info* fi) = 0;

    /// Write data to a file
    /// @param path Path to the file
    /// @param buf Data buffer
    /// @param size Number of bytes to write
    /// @param offset Write offset
    /// @param fi File info
    /// @return Number of bytes written, or negative errno on error
    virtual int write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) = 0;

    /// Truncate a file to a specified length
    /// @param path Path to the file
    /// @param size New file size
    /// @return 0 on success, negative errno on error
    virtual int truncate(const char* path, off_t size) = 0;
};

/// Platform-specific adapter that wraps FuseOperations
///
/// This class provides the glue between platform-specific FUSE
/// callback signatures and the abstract FuseOperations interface.
class PlatformAdapter {
public:
    /// Get FUSE operations structure configured for the given implementation
    /// @param impl Reference to the operations implementation
    /// @return Configured fuse_operations structure
    static struct fuse_operations get_operations(FuseOperations& impl);

    /// Set the current operations implementation for callbacks
    /// @param impl Pointer to the operations implementation (must outlive FUSE session)
    static void set_implementation(FuseOperations* impl);

    /// Get the current operations implementation
    /// @return Pointer to the current implementation
    static FuseOperations* get_implementation();

private:
    static FuseOperations* current_impl_;
};

}  // namespace tgfuse
