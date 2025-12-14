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

    /// Create and open a file
    /// @param path Path to the file to create
    /// @param mode File mode bits
    /// @param fi File info (fh field can be set)
    /// @return 0 on success, negative errno on error
    virtual int create(const char* path, mode_t mode, struct fuse_file_info* fi) = 0;

    /// Change file permissions
    /// @param path Path to the file
    /// @param mode New mode bits
    /// @return 0 on success, negative errno on error
    virtual int chmod(const char* path, mode_t mode) = 0;

    /// Change file ownership (stub - always succeeds for virtual fs)
    /// @param path Path to the file
    /// @param uid New owner uid
    /// @param gid New owner gid
    /// @return 0 on success, negative errno on error
    virtual int chown(const char* path, uid_t uid, gid_t gid) = 0;

    /// Change file timestamps (stub - always succeeds for virtual fs)
    /// @param path Path to the file
    /// @param ts New timestamps (atime, mtime)
    /// @return 0 on success, negative errno on error
    virtual int utimens(const char* path, const struct timespec ts[2]) = 0;

#ifdef __APPLE__
    /// Set extended attribute (macOS - stub returns ENOTSUP)
    /// @param path Path to the file
    /// @param name Attribute name
    /// @param value Attribute value
    /// @param size Value size
    /// @param flags Flags
    /// @param position Position (macOS specific)
    /// @return 0 on success, negative errno on error
    virtual int
    setxattr(const char* path, const char* name, const char* value, size_t size, int flags, uint32_t position) = 0;

    /// Get extended attribute (macOS - stub returns ENOTSUP)
    virtual int getxattr(const char* path, const char* name, char* value, size_t size, uint32_t position) = 0;

    /// List extended attributes (macOS - stub returns ENOTSUP)
    virtual int listxattr(const char* path, char* list, size_t size) = 0;

    /// Change file flags (macOS - used by cp for file flags like locked, hidden)
    /// @param path Path to the file
    /// @param flags BSD file flags (UF_HIDDEN, UF_IMMUTABLE, etc.)
    /// @return 0 on success, negative errno on error
    virtual int chflags(const char* path, uint32_t flags) = 0;

    /// Set extended attributes (macOS - combines chmod, chown, utimens, chflags, truncate)
    /// @param path Path to the file
    /// @param attr Structure containing attributes to set
    /// @return 0 on success, negative errno on error
    virtual int setattr_x(const char* path, struct setattr_x* attr) = 0;

    /// Set extended attributes with file handle (macOS)
    virtual int fsetattr_x(const char* path, struct setattr_x* attr, struct fuse_file_info* fi) = 0;
#else
    /// Set extended attribute (Linux)
    virtual int setxattr(const char* path, const char* name, const char* value, size_t size, int flags) = 0;

    /// Get extended attribute (Linux)
    virtual int getxattr(const char* path, const char* name, char* value, size_t size) = 0;

    /// List extended attributes (Linux)
    virtual int listxattr(const char* path, char* list, size_t size) = 0;
#endif
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
