#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

#ifdef __APPLE__
#include <fuse/fuse.h>
#else
#include <fuse3/fuse.h>
#endif

int main(int argc, char* argv[]) {
    CLI::App app{"tg-fuse - Telegram FUSE filesystem"};

    std::string mount_point;
    app.add_option("mount_point", mount_point, "Mount point for the filesystem");

    CLI11_PARSE(app, argc, argv);

    spdlog::info("tg-fuse starting...");
    spdlog::info("This is a placeholder implementation");

    // TODO: Implement FUSE operations and Telegram integration

    return 0;
}
