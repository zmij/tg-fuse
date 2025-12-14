#pragma once

#include <sys/stat.h>
#include <cstdint>
#include <ctime>
#include <optional>
#include <string>
#include <vector>

namespace tgfuse {

/// Entry type enumeration
enum class EntryType { DIRECTORY, FILE, SYMLINK };

/// File/directory entry information
struct Entry {
    std::string name;
    EntryType type;
    std::size_t size{0};      // For files: content size
    std::time_t mtime{0};     // Modification time
    std::time_t atime{0};     // Access time
    std::time_t ctime{0};     // Change time
    mode_t mode{0};           // File permissions
    std::string link_target;  // For symlinks: target path

    [[nodiscard]] bool is_directory() const { return type == EntryType::DIRECTORY; }
    [[nodiscard]] bool is_file() const { return type == EntryType::FILE; }
    [[nodiscard]] bool is_symlink() const { return type == EntryType::SYMLINK; }

    /// Create a directory entry
    static Entry directory(std::string name, mode_t mode = 0700) {
        Entry e;
        e.name = std::move(name);
        e.type = EntryType::DIRECTORY;
        e.mode = mode;
        e.mtime = e.atime = e.ctime = std::time(nullptr);
        return e;
    }

    /// Create a file entry
    static Entry file(std::string name, std::size_t size, mode_t mode = 0400) {
        Entry e;
        e.name = std::move(name);
        e.type = EntryType::FILE;
        e.size = size;
        e.mode = mode;
        e.mtime = e.atime = e.ctime = std::time(nullptr);
        return e;
    }

    /// Create a symlink entry
    static Entry symlink(std::string name, std::string target, mode_t mode = 0755) {
        Entry e;
        e.name = std::move(name);
        e.type = EntryType::SYMLINK;
        e.link_target = std::move(target);
        e.mode = mode;
        e.mtime = e.atime = e.ctime = std::time(nullptr);
        return e;
    }
};

/// File content result
struct FileContent {
    std::string data;
    bool readable{true};
};

/// Write operation result
struct WriteResult {
    bool success{false};
    int bytes_written{0};
    std::string error_message;
};

/// Abstract data provider interface
///
/// This interface defines the contract for filesystem data sources.
/// Implementations can provide mock data, real Telegram data, or other sources.
class DataProvider {
public:
    virtual ~DataProvider() = default;

    // Directory operations

    /// List entries in a directory
    /// @param path Absolute path to the directory
    /// @return Vector of entries in the directory
    [[nodiscard]] virtual std::vector<Entry> list_directory(std::string_view path) = 0;

    /// Get entry information for a path
    /// @param path Absolute path to the entry
    /// @return Entry information if exists, nullopt otherwise
    [[nodiscard]] virtual std::optional<Entry> get_entry(std::string_view path) = 0;

    /// Check if a path exists
    /// @param path Absolute path to check
    /// @return true if the path exists
    [[nodiscard]] virtual bool exists(std::string_view path) = 0;

    /// Check if a path is a directory
    /// @param path Absolute path to check
    /// @return true if the path is a directory
    [[nodiscard]] virtual bool is_directory(std::string_view path) = 0;

    /// Check if a path is a symlink
    /// @param path Absolute path to check
    /// @return true if the path is a symlink
    [[nodiscard]] virtual bool is_symlink(std::string_view path) = 0;

    // File operations

    /// Read file content
    /// @param path Absolute path to the file
    /// @return File content
    [[nodiscard]] virtual FileContent read_file(std::string_view path) = 0;

    /// Read symlink target
    /// @param path Absolute path to the symlink
    /// @return Target path of the symlink
    [[nodiscard]] virtual std::string read_link(std::string_view path) = 0;

    // Write operations

    /// Write to a file (for append-only files like messages)
    /// @param path Absolute path to the file
    /// @param data Data to write
    /// @param size Size of data
    /// @param offset Write offset (ignored for append-only)
    /// @return WriteResult with status
    virtual WriteResult write_file(std::string_view path, const char* data, std::size_t size, off_t offset) {
        (void)path;
        (void)data;
        (void)size;
        (void)offset;
        return WriteResult{false, 0, "Write not supported"};
    }

    /// Truncate a file
    /// @param path Absolute path to the file
    /// @param size New file size
    /// @return 0 on success, negative errno on error
    virtual int truncate_file(std::string_view path, off_t size) {
        (void)path;
        (void)size;
        return -EACCES;  // Default: not supported
    }

    /// Check if a file is writable
    /// @param path Absolute path to check
    /// @return true if the file is writable
    [[nodiscard]] virtual bool is_writable(std::string_view path) const {
        (void)path;
        return false;
    }

    /// Check if a file is append-only
    /// @param path Absolute path to check
    /// @return true if the file is append-only
    [[nodiscard]] virtual bool is_append_only(std::string_view path) const {
        (void)path;
        return false;
    }

    // File upload operations (for cp to virtual filesystem)

    /// Create a file for writing (called by FUSE create callback)
    /// @param path Absolute path to the file
    /// @param mode File mode
    /// @param fh Output file handle for tracking the upload
    /// @return 0 on success, negative errno on error
    virtual int create_file(std::string_view path, mode_t mode, uint64_t& fh) {
        (void)path;
        (void)mode;
        (void)fh;
        return -EACCES;  // Default: not supported
    }

    /// Write to an open file using file handle
    /// @param path Absolute path to the file
    /// @param data Data to write
    /// @param size Size of data
    /// @param offset Write offset
    /// @param fh File handle from create_file
    /// @return WriteResult with status
    virtual WriteResult
    write_file(std::string_view path, const char* data, std::size_t size, off_t offset, uint64_t fh) {
        // Default implementation delegates to the simpler write_file
        (void)fh;
        return write_file(path, data, size, offset);
    }

    /// Release (close) a file after writing (called by FUSE release callback)
    /// This is where uploaded files are actually sent to Telegram
    /// @param path Absolute path to the file
    /// @param fh File handle from create_file
    /// @return 0 on success, negative errno on error
    virtual int release_file(std::string_view path, uint64_t fh) {
        (void)path;
        (void)fh;
        return 0;  // Default: no-op
    }

    // Metadata

    /// Get filesystem name
    /// @return Name of the filesystem
    [[nodiscard]] virtual std::string get_filesystem_name() const = 0;

    /// Set the mount point for absolute symlink targets
    /// @param mount_point The mount point path
    virtual void set_mount_point(std::string mount_point) { mount_point_ = std::move(mount_point); }

    /// Get the mount point
    [[nodiscard]] const std::string& get_mount_point() const { return mount_point_; }

protected:
    std::string mount_point_;
};

}  // namespace tgfuse
