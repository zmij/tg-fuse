#include "fuse/operations.hpp"

#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <cstring>

namespace tgfuse {

namespace {
// Cache the effective uid/gid at startup
const uid_t effective_uid = geteuid();
const gid_t effective_gid = getegid();
}  // namespace

DataProviderOperations::DataProviderOperations(std::shared_ptr<DataProvider> provider)
    : provider_(std::move(provider)) {}

int DataProviderOperations::getattr(const char* path, struct stat* stbuf) {
    std::memset(stbuf, 0, sizeof(struct stat));

    auto entry = provider_->get_entry(path);
    if (!entry) {
        return -ENOENT;
    }

    if (entry->is_directory()) {
        stbuf->st_mode = S_IFDIR | entry->mode;
        stbuf->st_nlink = 2;
    } else if (entry->is_symlink()) {
        stbuf->st_mode = S_IFLNK | entry->mode;
        stbuf->st_nlink = 1;
        stbuf->st_size = static_cast<off_t>(entry->link_target.size());
    } else {
        stbuf->st_mode = S_IFREG | entry->mode;
        stbuf->st_nlink = 1;
        stbuf->st_size = static_cast<off_t>(entry->size);
    }

    stbuf->st_uid = effective_uid;
    stbuf->st_gid = effective_gid;
    stbuf->st_atime = entry->atime;
    stbuf->st_mtime = entry->mtime;
    stbuf->st_ctime = entry->ctime;

    return 0;
}

int DataProviderOperations::readdir(const char* path, DirFiller filler, off_t offset) {
    (void)offset;  // We don't use offset-based iteration

    if (!provider_->is_directory(path)) {
        return -ENOTDIR;
    }

    // Add . and ..
    filler(".", nullptr);
    filler("..", nullptr);

    // Get entries from provider
    auto entries = provider_->list_directory(path);
    for (const auto& entry : entries) {
        struct stat st;
        std::memset(&st, 0, sizeof(st));

        if (entry.is_directory()) {
            st.st_mode = S_IFDIR | entry.mode;
        } else if (entry.is_symlink()) {
            st.st_mode = S_IFLNK | entry.mode;
        } else {
            st.st_mode = S_IFREG | entry.mode;
            st.st_size = static_cast<off_t>(entry.size);
        }
        st.st_uid = effective_uid;
        st.st_gid = effective_gid;

        if (filler(entry.name.c_str(), &st) != 0) {
            break;  // Buffer full
        }
    }

    return 0;
}

int DataProviderOperations::readlink(const char* path, char* buf, size_t size) {
    if (!provider_->is_symlink(path)) {
        return -EINVAL;
    }

    std::string target = provider_->read_link(path);
    if (target.empty()) {
        return -ENOENT;
    }

    // Copy target to buffer (size includes null terminator space)
    size_t len = std::min(target.size(), size - 1);
    std::memcpy(buf, target.c_str(), len);
    buf[len] = '\0';

    return 0;
}

int DataProviderOperations::open(const char* path, struct fuse_file_info* fi) {
    auto entry = provider_->get_entry(path);

    // For files that don't exist yet but we're opening for write+create,
    // let the create() callback handle it
    if (!entry) {
        int access_mode = fi->flags & O_ACCMODE;
        if (access_mode != O_RDONLY && (fi->flags & O_CREAT)) {
            // This will be handled by create() callback
            return 0;
        }
        return -ENOENT;
    }

    if (entry->is_directory()) {
        return -EISDIR;
    }

    int access_mode = fi->flags & O_ACCMODE;

    // Check write access
    if (access_mode != O_RDONLY) {
        if (!provider_->is_writable(path)) {
            return -EACCES;
        }
        // For append-only files, force O_APPEND behaviour
        // (the actual append semantics are handled in write_file)
    }

    return 0;
}

int DataProviderOperations::read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    (void)fi;  // Unused

    auto content = provider_->read_file(path);
    if (!content.readable) {
        return -ENOENT;
    }

    size_t len = content.data.size();
    if (static_cast<size_t>(offset) >= len) {
        return 0;  // EOF
    }

    size_t available = len - static_cast<size_t>(offset);
    size_t to_read = std::min(size, available);

    std::memcpy(buf, content.data.c_str() + offset, to_read);
    return static_cast<int>(to_read);
}

int DataProviderOperations::release(const char* path, struct fuse_file_info* fi) {
    return provider_->release_file(path, fi->fh);
}

int DataProviderOperations::write(
    const char* path,
    const char* buf,
    size_t size,
    off_t offset,
    struct fuse_file_info* fi
) {
    // Check if this is a file upload with a valid file handle
    if (fi && fi->fh != 0) {
        auto result = provider_->write_file(path, buf, size, offset, fi->fh);
        if (!result.success) {
            return -EIO;
        }
        return result.bytes_written;
    }

    // Fallback to old interface (for messages etc.)
    if (!provider_->is_writable(path)) {
        return -EACCES;
    }

    auto result = provider_->write_file(path, buf, size, offset);
    if (!result.success) {
        return -EIO;
    }

    return result.bytes_written;
}

int DataProviderOperations::truncate(const char* path, off_t size) { return provider_->truncate_file(path, size); }

int DataProviderOperations::create(const char* path, mode_t mode, struct fuse_file_info* fi) {
    uint64_t fh = 0;
    int result = provider_->create_file(path, mode, fh);
    if (result == 0) {
        fi->fh = fh;
    }
    return result;
}

int DataProviderOperations::chmod(const char* path, mode_t mode) {
    (void)path;
    (void)mode;
    // Virtual filesystem - chmod is a no-op but succeeds
    return 0;
}

int DataProviderOperations::chown(const char* path, uid_t uid, gid_t gid) {
    (void)path;
    (void)uid;
    (void)gid;
    // Virtual filesystem - chown is a no-op but succeeds
    return 0;
}

int DataProviderOperations::utimens(const char* path, const struct timespec ts[2]) {
    (void)path;
    (void)ts;
    // Virtual filesystem - utimens is a no-op but succeeds
    return 0;
}

#ifdef __APPLE__
int DataProviderOperations::setxattr(
    const char* path,
    const char* name,
    const char* value,
    size_t size,
    int flags,
    uint32_t position
) {
    (void)path;
    (void)name;
    (void)value;
    (void)size;
    (void)flags;
    (void)position;
    // Virtual filesystem silently accepts (but ignores) extended attributes
    // Returning 0 prevents fcopyfile from failing with "Permission denied"
    return 0;
}

int DataProviderOperations::getxattr(const char* path, const char* name, char* value, size_t size, uint32_t position) {
    (void)path;
    (void)name;
    (void)value;
    (void)size;
    (void)position;
    // No extended attributes stored - return "not found"
    return -ENOATTR;
}

int DataProviderOperations::listxattr(const char* path, char* list, size_t size) {
    (void)path;
    (void)list;
    (void)size;
    // Return 0 to indicate empty list of attributes
    return 0;
}

int DataProviderOperations::chflags(const char* path, uint32_t flags) {
    (void)path;
    (void)flags;
    // Virtual filesystem silently accepts (but ignores) file flags
    // This prevents fcopyfile from failing with "Permission denied"
    return 0;
}

int DataProviderOperations::setattr_x(const char* path, struct setattr_x* attr) {
    (void)path;
    (void)attr;
    // Virtual filesystem silently accepts (but ignores) extended attribute changes
    // This is called by macOS for combined attribute operations
    return 0;
}

int DataProviderOperations::fsetattr_x(const char* path, struct setattr_x* attr, struct fuse_file_info* fi) {
    (void)path;
    (void)attr;
    (void)fi;
    // Virtual filesystem silently accepts (but ignores) extended attribute changes
    return 0;
}
#else
int DataProviderOperations::setxattr(const char* path, const char* name, const char* value, size_t size, int flags) {
    (void)path;
    (void)name;
    (void)value;
    (void)size;
    (void)flags;
    // Virtual filesystem silently accepts (but ignores) extended attributes
    return 0;
}

int DataProviderOperations::getxattr(const char* path, const char* name, char* value, size_t size) {
    (void)path;
    (void)name;
    (void)value;
    (void)size;
    // No extended attributes stored - return "not found"
    return -ENODATA;  // Linux uses ENODATA instead of ENOATTR
}

int DataProviderOperations::listxattr(const char* path, char* list, size_t size) {
    (void)path;
    (void)list;
    (void)size;
    // Return 0 to indicate empty list of attributes
    return 0;
}
#endif

}  // namespace tgfuse
