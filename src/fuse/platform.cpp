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

    return ops;
}

}  // namespace tgfuse
