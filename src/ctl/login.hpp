#pragma once

namespace tgfuse::ctl {

/// Execute login command - authenticate with Telegram interactively
/// Loads config automatically, prompts for API credentials if missing
int exec_login();

/// Execute logout command - log out from Telegram
int exec_logout();

/// Execute status command - show authentication status
int exec_status();

}  // namespace tgfuse::ctl
