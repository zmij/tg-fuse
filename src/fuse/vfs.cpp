#include "fuse/vfs.hpp"

#include <spdlog/spdlog.h>

#include <csignal>
#include <cstring>
#include <vector>

namespace tgfuse {

VirtualFilesystem::VirtualFilesystem(std::shared_ptr<DataProvider> provider)
    : provider_(std::move(provider)), operations_(std::make_unique<DataProviderOperations>(provider_)) {}

VirtualFilesystem::~VirtualFilesystem() {
    // Clean up platform adapter
    PlatformAdapter::set_implementation(nullptr);
}

int VirtualFilesystem::mount(const VfsConfig& config) {
    config_ = config;

    spdlog::info("Mounting {} at {}", provider_->get_filesystem_name(), config.mount_point);

    // Build FUSE arguments
    std::vector<std::string> args_storage;
    std::vector<char*> argv;

    // Program name (required by FUSE)
    args_storage.push_back("tg-fused");

    // Mount point
    args_storage.push_back(config.mount_point);

    // Foreground mode
    if (config.foreground) {
        args_storage.push_back("-f");
    }

    // Debug mode
    if (config.debug) {
        args_storage.push_back("-d");
    }

    // Single-threaded mode (simpler for now)
    args_storage.push_back("-s");

    // Allow other users (if requested)
    if (config.allow_other) {
        args_storage.push_back("-o");
        args_storage.push_back("allow_other");
    }

    // Convert to char* array
    for (auto& arg : args_storage) {
        argv.push_back(arg.data());
    }
    argv.push_back(nullptr);

    int argc = static_cast<int>(argv.size() - 1);

    // Get FUSE operations
    auto ops = PlatformAdapter::get_operations(*operations_);

    mounted_ = true;

    // Run FUSE main loop
    spdlog::debug("Starting FUSE main loop with {} arguments", argc);
    for (int i = 0; i < argc; ++i) {
        spdlog::debug("  argv[{}] = {}", i, argv[static_cast<size_t>(i)]);
    }

    int result = fuse_main(argc, argv.data(), &ops, nullptr);

    mounted_ = false;
    spdlog::info("FUSE main loop exited with code {}", result);

    return result;
}

}  // namespace tgfuse
