#pragma once

#include "fuse/data_provider.hpp"
#include "fuse/platform.hpp"

#include <memory>

namespace tgfuse {

/// FUSE operations implementation using a DataProvider
///
/// This class implements the FuseOperations interface by delegating
/// to a DataProvider for actual filesystem data.
class DataProviderOperations : public FuseOperations {
public:
    /// Construct operations with a data provider
    /// @param provider Shared pointer to the data provider
    explicit DataProviderOperations(std::shared_ptr<DataProvider> provider);

    ~DataProviderOperations() override = default;

    // FuseOperations interface
    int getattr(const char* path, struct stat* stbuf) override;
    int readdir(const char* path, DirFiller filler, off_t offset) override;
    int readlink(const char* path, char* buf, size_t size) override;
    int open(const char* path, struct fuse_file_info* fi) override;
    int read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) override;
    int release(const char* path, struct fuse_file_info* fi) override;
    int write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) override;
    int truncate(const char* path, off_t size) override;

private:
    std::shared_ptr<DataProvider> provider_;
};

}  // namespace tgfuse
