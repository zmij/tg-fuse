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
    if (!entry) {
        return -ENOENT;
    }

    if (entry->is_directory()) {
        return -EISDIR;
    }

    // For now, we only support read-only access
    if ((fi->flags & O_ACCMODE) != O_RDONLY) {
        return -EACCES;
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
    (void)path;
    (void)fi;
    // Nothing to do for mock filesystem
    return 0;
}

}  // namespace tgfuse
