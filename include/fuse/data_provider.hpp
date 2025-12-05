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

    /// Create a symlink entry (symlinks conventionally use 0777, target perms apply)
    static Entry symlink(std::string name, std::string target, mode_t mode = 0777) {
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

    // Metadata

    /// Get filesystem name
    /// @return Name of the filesystem
    [[nodiscard]] virtual std::string get_filesystem_name() const = 0;
};

}  // namespace tgfuse
