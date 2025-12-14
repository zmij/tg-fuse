#pragma once

#include <string>

namespace tgfuse::ctl {

/// Clear file cache for a specific chat (user/group/channel)
int exec_cache_clear_files(const std::string& entity_name);

/// Clear all file caches
int exec_cache_clear_all_files();

/// Clear all caches (messages, files, etc.)
int exec_cache_clear_all();

/// Show cache statistics
int exec_cache_stats();

}  // namespace tgfuse::ctl
