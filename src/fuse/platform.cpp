#include "fuse/platform.hpp"

#include <cerrno>
#include <cstring>

namespace tgfuse {

// Static member definition
FuseOperations* PlatformAdapter::current_impl_ = nullptr;

void PlatformAdapter::set_implementation(FuseOperations* impl) { current_impl_ = impl; }

FuseOperations* PlatformAdapter::get_implementation() { return current_impl_; }

// Platform-specific callback wrappers

#if TG_FUSE_VERSION == 2
// macFUSE (FUSE 2.x) callbacks

static int fuse_getattr_wrapper(const char* path, struct stat* stbuf) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->getattr(path, stbuf);
}

static int
fuse_readdir_wrapper(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi) {
    (void)fi;  // Unused
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }

    // Wrap the filler function
    auto dir_filler = [buf, filler](const char* name, const struct stat* stbuf) -> int {
        return filler(buf, name, stbuf, 0);
    };

    return impl->readdir(path, dir_filler, offset);
}

static int fuse_readlink_wrapper(const char* path, char* buf, size_t size) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->readlink(path, buf, size);
}

static int fuse_open_wrapper(const char* path, struct fuse_file_info* fi) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->open(path, fi);
}

static int fuse_read_wrapper(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->read(path, buf, size, offset, fi);
}

static int fuse_release_wrapper(const char* path, struct fuse_file_info* fi) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->release(path, fi);
}

static int fuse_write_wrapper(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->write(path, buf, size, offset, fi);
}

static int fuse_truncate_wrapper(const char* path, off_t size) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->truncate(path, size);
}

static int fuse_create_wrapper(const char* path, mode_t mode, struct fuse_file_info* fi) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->create(path, mode, fi);
}

static int fuse_chmod_wrapper(const char* path, mode_t mode) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->chmod(path, mode);
}

static int fuse_chown_wrapper(const char* path, uid_t uid, gid_t gid) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->chown(path, uid, gid);
}

static int fuse_utimens_wrapper(const char* path, const struct timespec ts[2]) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->utimens(path, ts);
}

static int fuse_setxattr_wrapper(
    const char* path,
    const char* name,
    const char* value,
    size_t size,
    int flags,
    uint32_t position
) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->setxattr(path, name, value, size, flags, position);
}

static int fuse_getxattr_wrapper(const char* path, const char* name, char* value, size_t size, uint32_t position) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->getxattr(path, name, value, size, position);
}

static int fuse_listxattr_wrapper(const char* path, char* list, size_t size) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->listxattr(path, list, size);
}

static int fuse_chflags_wrapper(const char* path, uint32_t flags) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->chflags(path, flags);
}

static int fuse_setattr_x_wrapper(const char* path, struct setattr_x* attr) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->setattr_x(path, attr);
}

static int fuse_fsetattr_x_wrapper(const char* path, struct setattr_x* attr, struct fuse_file_info* fi) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->fsetattr_x(path, attr, fi);
}

#else
// libfuse3 callbacks

static int fuse_getattr_wrapper(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
    (void)fi;  // Unused
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->getattr(path, stbuf);
}

static int fuse_readdir_wrapper(
    const char* path,
    void* buf,
    fuse_fill_dir_t filler,
    off_t offset,
    struct fuse_file_info* fi,
    enum fuse_readdir_flags flags
) {
    (void)fi;     // Unused
    (void)flags;  // Unused
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }

    // Wrap the filler function
    auto dir_filler = [buf, filler](const char* name, const struct stat* stbuf) -> int {
        return filler(buf, name, stbuf, 0, static_cast<enum fuse_fill_dir_flags>(0));
    };

    return impl->readdir(path, dir_filler, offset);
}

static int fuse_readlink_wrapper(const char* path, char* buf, size_t size) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->readlink(path, buf, size);
}

static int fuse_open_wrapper(const char* path, struct fuse_file_info* fi) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->open(path, fi);
}

static int fuse_read_wrapper(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->read(path, buf, size, offset, fi);
}

static int fuse_release_wrapper(const char* path, struct fuse_file_info* fi) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->release(path, fi);
}

static int fuse_write_wrapper(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->write(path, buf, size, offset, fi);
}

static int fuse_truncate_wrapper(const char* path, off_t size, struct fuse_file_info* fi) {
    (void)fi;  // Unused
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->truncate(path, size);
}

static int fuse_create_wrapper(const char* path, mode_t mode, struct fuse_file_info* fi) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->create(path, mode, fi);
}

static int fuse_chmod_wrapper(const char* path, mode_t mode, struct fuse_file_info* fi) {
    (void)fi;  // Unused
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->chmod(path, mode);
}

static int fuse_chown_wrapper(const char* path, uid_t uid, gid_t gid, struct fuse_file_info* fi) {
    (void)fi;  // Unused
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->chown(path, uid, gid);
}

static int fuse_utimens_wrapper(const char* path, const struct timespec ts[2], struct fuse_file_info* fi) {
    (void)fi;  // Unused
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->utimens(path, ts);
}

static int fuse_setxattr_wrapper(const char* path, const char* name, const char* value, size_t size, int flags) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->setxattr(path, name, value, size, flags);
}

static int fuse_getxattr_wrapper(const char* path, const char* name, char* value, size_t size) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->getxattr(path, name, value, size);
}

static int fuse_listxattr_wrapper(const char* path, char* list, size_t size) {
    auto* impl = PlatformAdapter::get_implementation();
    if (!impl) {
        return -ENOENT;
    }
    return impl->listxattr(path, list, size);
}

#endif

struct fuse_operations PlatformAdapter::get_operations(FuseOperations& impl) {
    set_implementation(&impl);

    struct fuse_operations ops;
    std::memset(&ops, 0, sizeof(ops));

    ops.getattr = fuse_getattr_wrapper;
    ops.readdir = fuse_readdir_wrapper;
    ops.readlink = fuse_readlink_wrapper;
    ops.open = fuse_open_wrapper;
    ops.read = fuse_read_wrapper;
    ops.release = fuse_release_wrapper;
    ops.write = fuse_write_wrapper;
    ops.truncate = fuse_truncate_wrapper;
    ops.create = fuse_create_wrapper;
    ops.chmod = fuse_chmod_wrapper;
    ops.chown = fuse_chown_wrapper;
    ops.utimens = fuse_utimens_wrapper;
    ops.setxattr = fuse_setxattr_wrapper;
    ops.getxattr = fuse_getxattr_wrapper;
    ops.listxattr = fuse_listxattr_wrapper;
#if TG_FUSE_VERSION == 2
    ops.chflags = fuse_chflags_wrapper;
    ops.setattr_x = fuse_setattr_x_wrapper;
    ops.fsetattr_x = fuse_fsetattr_x_wrapper;
#endif

    return ops;
}

}  // namespace tgfuse
