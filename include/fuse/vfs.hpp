#pragma once

#include "fuse/data_provider.hpp"
#include "fuse/operations.hpp"
#include "fuse/platform.hpp"

#include <memory>
#include <string>

namespace tgfuse {

/// VFS configuration
struct VfsConfig {
    std::string mount_point;
    bool foreground{true};    // Run in foreground (useful for debugging)
    bool debug{false};        // Enable FUSE debug output
    bool allow_other{false};  // Allow other users to access mount
};

/// Virtual filesystem manager
///
/// This class manages the lifecycle of a FUSE filesystem,
/// coordinating between the data provider and FUSE operations.
class VirtualFilesystem {
public:
    /// Construct a VFS with a data provider
    /// @param provider Shared pointer to the data provider
    explicit VirtualFilesystem(std::shared_ptr<DataProvider> provider);

    ~VirtualFilesystem();

    // Non-copyable, non-movable
    VirtualFilesystem(const VirtualFilesystem&) = delete;
    VirtualFilesystem& operator=(const VirtualFilesystem&) = delete;
    VirtualFilesystem(VirtualFilesystem&&) = delete;
    VirtualFilesystem& operator=(VirtualFilesystem&&) = delete;

    /// Mount the filesystem and start the FUSE main loop
    /// @param config VFS configuration
    /// @return Exit code from FUSE main loop
    int mount(const VfsConfig& config);

    /// Check if the filesystem is mounted
    /// @return true if mounted
    [[nodiscard]] bool is_mounted() const { return mounted_; }

    /// Get the data provider
    /// @return Reference to the data provider
    [[nodiscard]] DataProvider& provider() { return *provider_; }
    [[nodiscard]] const DataProvider& provider() const { return *provider_; }

private:
    std::shared_ptr<DataProvider> provider_;
    std::unique_ptr<DataProviderOperations> operations_;
    VfsConfig config_;
    bool mounted_{false};
};

}  // namespace tgfuse
