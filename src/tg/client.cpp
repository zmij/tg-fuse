#include "tg/client.hpp"
#include "tg/exceptions.hpp"

#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>
#include <td/telegram/td_api.hpp>

#include <spdlog/spdlog.h>

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <future>
#include <map>
#include <mutex>
#include <thread>

namespace tg {

namespace td_api = td::td_api;
namespace fs = std::filesystem;

// Helper functions to convert TDLib types to our types
namespace {

ChatType convert_chat_type(const td_api::ChatType& type) {
    switch (type.get_id()) {
        case td_api::chatTypePrivate::ID:
        case td_api::chatTypeSecret::ID:
            return ChatType::PRIVATE;
        case td_api::chatTypeBasicGroup::ID:
            return ChatType::GROUP;
        case td_api::chatTypeSupergroup::ID: {
            auto& sg = static_cast<const td_api::chatTypeSupergroup&>(type);
            return sg.is_channel_ ? ChatType::CHANNEL : ChatType::SUPERGROUP;
        }
        default:
            return ChatType::PRIVATE;
    }
}

std::pair<UserStatus, int64_t> convert_user_status(const td_api::UserStatus* status) {
    if (!status) {
        return {UserStatus::UNKNOWN, 0};
    }

    switch (status->get_id()) {
        case td_api::userStatusOnline::ID:
            return {UserStatus::ONLINE, std::time(nullptr)};
        case td_api::userStatusOffline::ID: {
            auto& offline = static_cast<const td_api::userStatusOffline&>(*status);
            return {UserStatus::OFFLINE, offline.was_online_};
        }
        case td_api::userStatusRecently::ID:
            return {UserStatus::RECENTLY, 0};
        case td_api::userStatusLastWeek::ID:
            return {UserStatus::LAST_WEEK, 0};
        case td_api::userStatusLastMonth::ID:
            return {UserStatus::LAST_MONTH, 0};
        case td_api::userStatusEmpty::ID:
        default:
            return {UserStatus::UNKNOWN, 0};
    }
}

User convert_user(const td_api::user& user) {
    User result;
    result.id = user.id_;
    if (user.usernames_ && !user.usernames_->active_usernames_.empty()) {
        result.username = user.usernames_->active_usernames_[0];
    }
    result.first_name = user.first_name_;
    result.last_name = user.last_name_;
    result.phone_number = user.phone_number_;
    result.is_contact = user.is_contact_;

    auto [status, last_seen] = convert_user_status(user.status_.get());
    result.status = status;
    result.last_seen = last_seen;

    return result;
}

MediaType convert_message_content_type(const td_api::MessageContent& content) {
    switch (content.get_id()) {
        case td_api::messagePhoto::ID:
            return MediaType::PHOTO;
        case td_api::messageVideo::ID:
            return MediaType::VIDEO;
        case td_api::messageDocument::ID:
            return MediaType::DOCUMENT;
        case td_api::messageAudio::ID:
            return MediaType::AUDIO;
        case td_api::messageVoiceNote::ID:
            return MediaType::VOICE;
        case td_api::messageAnimation::ID:
            return MediaType::ANIMATION;
        case td_api::messageSticker::ID:
            return MediaType::STICKER;
        case td_api::messageVideoNote::ID:
            return MediaType::VIDEO_NOTE;
        default:
            return MediaType::DOCUMENT;
    }
}

std::optional<MediaInfo> extract_media_info(const td_api::MessageContent& content) {
    MediaInfo info;

    switch (content.get_id()) {
        case td_api::messagePhoto::ID: {
            auto& photo = static_cast<const td_api::messagePhoto&>(content);
            if (!photo.photo_->sizes_.empty()) {
                auto& largest = photo.photo_->sizes_.back();
                info.type = MediaType::PHOTO;
                info.file_id = std::to_string(largest->photo_->id_);
                info.filename = "photo.jpg";
                info.mime_type = "image/jpeg";
                info.file_size = largest->photo_->size_;
                info.width = largest->width_;
                info.height = largest->height_;
            }
            return info;
        }

        case td_api::messageVideo::ID: {
            auto& video = static_cast<const td_api::messageVideo&>(content);
            info.type = MediaType::VIDEO;
            info.file_id = std::to_string(video.video_->video_->id_);
            info.filename = video.video_->file_name_;
            info.mime_type = video.video_->mime_type_;
            info.file_size = video.video_->video_->size_;
            info.width = video.video_->width_;
            info.height = video.video_->height_;
            info.duration = video.video_->duration_;
            return info;
        }

        case td_api::messageDocument::ID: {
            auto& doc = static_cast<const td_api::messageDocument&>(content);
            info.type = MediaType::DOCUMENT;
            info.file_id = std::to_string(doc.document_->document_->id_);
            info.filename = doc.document_->file_name_;
            info.mime_type = doc.document_->mime_type_;
            info.file_size = doc.document_->document_->size_;
            return info;
        }

        case td_api::messageAudio::ID: {
            auto& audio = static_cast<const td_api::messageAudio&>(content);
            info.type = MediaType::AUDIO;
            info.file_id = std::to_string(audio.audio_->audio_->id_);
            info.filename = audio.audio_->file_name_;
            info.mime_type = audio.audio_->mime_type_;
            info.file_size = audio.audio_->audio_->size_;
            info.duration = audio.audio_->duration_;
            return info;
        }

        case td_api::messageVoiceNote::ID: {
            auto& voice = static_cast<const td_api::messageVoiceNote&>(content);
            info.type = MediaType::VOICE;
            info.file_id = std::to_string(voice.voice_note_->voice_->id_);
            info.filename = "voice.ogg";
            info.mime_type = voice.voice_note_->mime_type_;
            info.file_size = voice.voice_note_->voice_->size_;
            info.duration = voice.voice_note_->duration_;
            return info;
        }

        case td_api::messageAnimation::ID: {
            auto& anim = static_cast<const td_api::messageAnimation&>(content);
            info.type = MediaType::ANIMATION;
            info.file_id = std::to_string(anim.animation_->animation_->id_);
            info.filename = anim.animation_->file_name_;
            info.mime_type = anim.animation_->mime_type_;
            info.file_size = anim.animation_->animation_->size_;
            info.width = anim.animation_->width_;
            info.height = anim.animation_->height_;
            info.duration = anim.animation_->duration_;
            return info;
        }

        default:
            return std::nullopt;
    }
}

std::string extract_message_text(const td_api::MessageContent& content) {
    if (content.get_id() == td_api::messageText::ID) {
        auto& text_msg = static_cast<const td_api::messageText&>(content);
        return text_msg.text_->text_;
    }
    return "";
}

Message convert_message(const td_api::message& msg) {
    Message result;
    result.id = msg.id_;
    result.chat_id = msg.chat_id_;
    result.sender_id = msg.sender_id_->get_id() == td_api::messageSenderUser::ID
                           ? static_cast<const td_api::messageSenderUser&>(*msg.sender_id_).user_id_
                           : 0;
    result.timestamp = msg.date_;
    result.is_outgoing = msg.is_outgoing_;

    if (msg.content_) {
        result.text = extract_message_text(*msg.content_);
        result.media = extract_media_info(*msg.content_);
    }

    return result;
}

Chat convert_chat(const td_api::chat& chat) {
    Chat result;
    result.id = chat.id_;
    result.title = chat.title_;
    result.type = convert_chat_type(*chat.type_);

    if (chat.last_message_) {
        result.last_message_id = chat.last_message_->id_;
        result.last_message_timestamp = chat.last_message_->date_;
    }

    // Extract username if available
    // Note: TDLib doesn't directly provide username in chat object,
    // we'll need to get it from the user/supergroup info
    result.username = "";

    return result;
}

}  // namespace

// Implementation class
class TelegramClient::Impl {
public:
    explicit Impl(const Config& config, CacheManager* cache)
        : config_(config), cache_(cache), client_id_(0), running_(false), auth_state_(AuthState::WAIT_PHONE) {
        spdlog::info("Creating TelegramClient with database: {}", config.database_directory);
    }

    ~Impl() {
        if (running_) {
            stop();
        }
    }

    void start() {
        if (running_) {
            return;
        }

        // Configure TDLib logging before creating the client
        configure_tdlib_logging();

        running_ = true;
        client_id_ = td::ClientManager::get_manager_singleton()->create_client_id();

        // Start the update handler thread
        update_thread_ = std::thread([this]() { process_updates(); });

        spdlog::info("TelegramClient started with client_id: {}", client_id_);

        // Send initial configuration
        send_query(
            td_api::make_object<td_api::setTdlibParameters>(
                config_.use_test_dc,
                config_.database_directory,
                config_.files_directory,
                "",  // database_encryption_key
                config_.use_file_database,
                config_.use_chat_info_database,
                config_.use_message_database,
                true,  // use_secret_chats
                config_.api_id,
                config_.api_hash,
                "en",
                "Desktop",
                "",
                "1.0"
            ),
            [](auto response) { spdlog::debug("TDLib parameters set"); }
        );
    }

    void stop() {
        if (!running_) {
            return;
        }

        spdlog::info("Stopping TelegramClient");

        // Send close request using a proper query_id
        send_query(td_api::make_object<td_api::close>(), [](auto response) {
            spdlog::debug("Close request acknowledged");
        });

        // Wait for the update thread to finish with a timeout
        // The update thread exits when authorizationStateClosed is received
        if (update_thread_.joinable()) {
            // Use a timed wait instead of blocking forever
            auto start = std::chrono::steady_clock::now();
            constexpr auto timeout = std::chrono::seconds(5);

            while (running_ && std::chrono::steady_clock::now() - start < timeout) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            // Force stop if timeout exceeded
            if (running_) {
                spdlog::warn("TelegramClient shutdown timeout, forcing stop");
                running_ = false;
            }

            update_thread_.join();
        }

        spdlog::info("TelegramClient stopped");
    }

    // Send a query to TDLib and register a callback
    template <typename QueryType, typename Callback>
    void send_query(td_api::object_ptr<QueryType> query, Callback callback) {
        auto query_id = next_query_id_++;

        {
            std::lock_guard<std::mutex> lock(callbacks_mutex_);
            callbacks_[query_id] = [callback](td_api::object_ptr<td_api::Object> response) {
                callback(std::move(response));
            };
        }

        td::ClientManager::get_manager_singleton()->send(client_id_, query_id, std::move(query));
    }

    // Send a query and wait for response synchronously
    template <typename QueryType>
    td_api::object_ptr<td_api::Object> send_query_sync(td_api::object_ptr<QueryType> query, int timeout_ms = 5000) {
        std::promise<td_api::object_ptr<td_api::Object>> result_promise;
        auto result_future = result_promise.get_future();

        send_query(std::move(query), [&result_promise](td_api::object_ptr<td_api::Object> response) {
            result_promise.set_value(std::move(response));
        });

        if (result_future.wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::timeout) {
            throw TimeoutException("Query timeout");
        }

        return result_future.get();
    }

    // Process updates from TDLib
    void process_updates() {
        auto* manager = td::ClientManager::get_manager_singleton();

        while (running_) {
            auto response = manager->receive(1.0);  // 1 second timeout

            if (!response.object) {
                continue;
            }

            if (response.request_id == 0) {
                // This is an update, not a response to a query
                process_update(std::move(response.object));
            } else {
                // This is a response to a query
                std::function<void(td_api::object_ptr<td_api::Object>)> callback;

                {
                    std::lock_guard<std::mutex> lock(callbacks_mutex_);
                    auto it = callbacks_.find(response.request_id);
                    if (it != callbacks_.end()) {
                        callback = std::move(it->second);
                        callbacks_.erase(it);
                    }
                }

                if (callback) {
                    callback(std::move(response.object));
                }
            }
        }
    }

    // Process an update from TDLib
    void process_update(td_api::object_ptr<td_api::Object> update) {
        if (!update) {
            return;
        }

        switch (update->get_id()) {
            case td_api::updateAuthorizationState::ID: {
                auto auth_update = td::move_tl_object_as<td_api::updateAuthorizationState>(update);
                process_authorization_state(std::move(auth_update->authorization_state_));
                break;
            }

            case td_api::updateNewChat::ID: {
                auto chat_update = td::move_tl_object_as<td_api::updateNewChat>(update);
                auto chat = convert_chat(*chat_update->chat_);
                cache_->cache_chat(chat);
                spdlog::debug(
                    "updateNewChat: id={} type={} title='{}'", chat.id, static_cast<int>(chat.type), chat.title
                );
                break;
            }

            case td_api::updateNewMessage::ID: {
                auto message_update = td::move_tl_object_as<td_api::updateNewMessage>(update);
                auto message = convert_message(*message_update->message_);
                cache_->cache_message(message);
                spdlog::debug("updateNewMessage: id={} chat={}", message.id, message.chat_id);

                // Notify callback if set
                {
                    std::lock_guard<std::mutex> lock(message_callback_mutex_);
                    if (message_callback_) {
                        message_callback_(message);
                    }
                }
                break;
            }

            case td_api::updateUser::ID: {
                auto user_update = td::move_tl_object_as<td_api::updateUser>(update);
                auto user = convert_user(*user_update->user_);
                cache_->cache_user(user);
                spdlog::debug("updateUser: id={} @{} '{}'", user.id, user.username, user.display_name());
                break;
            }

            case td_api::updateChatLastMessage::ID: {
                // Chat's last message updated - we can update our cache
                auto msg_update = td::move_tl_object_as<td_api::updateChatLastMessage>(update);
                if (msg_update->last_message_) {
                    auto message = convert_message(*msg_update->last_message_);
                    cache_->cache_message(message);
                    spdlog::debug("updateChatLastMessage: chat={} msg={}", msg_update->chat_id_, message.id);
                }
                break;
            }

            default:
                spdlog::trace("Unhandled update: {}", update->get_id());
                break;
        }
    }

    // Process authorization state updates
    void process_authorization_state(td_api::object_ptr<td_api::AuthorizationState> state) {
        if (!state) {
            return;
        }

        switch (state->get_id()) {
            case td_api::authorizationStateWaitTdlibParameters::ID:
                spdlog::info("Authorization: waiting for TDLib parameters");
                auth_state_ = AuthState::WAIT_PHONE;
                break;

            case td_api::authorizationStateWaitPhoneNumber::ID:
                spdlog::info("Authorization: waiting for phone number");
                auth_state_ = AuthState::WAIT_PHONE;
                auth_cv_.notify_all();
                break;

            case td_api::authorizationStateWaitCode::ID:
                spdlog::info("Authorization: waiting for code");
                auth_state_ = AuthState::WAIT_CODE;
                auth_cv_.notify_all();
                break;

            case td_api::authorizationStateWaitPassword::ID:
                spdlog::info("Authorization: waiting for password");
                auth_state_ = AuthState::WAIT_PASSWORD;
                auth_cv_.notify_all();
                break;

            case td_api::authorizationStateReady::ID:
                spdlog::info("Authorization: ready");
                auth_state_ = AuthState::READY;
                auth_cv_.notify_all();
                break;

            case td_api::authorizationStateLoggingOut::ID:
                spdlog::info("Authorization: logging out");
                break;

            case td_api::authorizationStateClosing::ID:
                spdlog::info("Authorization: closing");
                break;

            case td_api::authorizationStateClosed::ID:
                spdlog::info("Authorization: closed");
                running_ = false;
                break;

            default:
                spdlog::debug("Unhandled authorization state: {}", state->get_id());
                break;
        }
    }

    AuthState get_auth_state() const { return auth_state_; }

    void wait_for_auth_state(AuthState desired_state, int timeout_seconds = 30) {
        std::unique_lock<std::mutex> lock(auth_mutex_);
        auto timeout = std::chrono::seconds(timeout_seconds);

        if (!auth_cv_.wait_for(lock, timeout, [this, desired_state]() { return auth_state_ == desired_state; })) {
            throw TimeoutException("Waiting for auth state");
        }
    }

    void send_phone_number(const std::string& phone) {
        send_query(td_api::make_object<td_api::setAuthenticationPhoneNumber>(phone, nullptr), [](auto response) {
            spdlog::debug("Phone number sent");
        });
    }

    void send_code(const std::string& code) {
        send_query(td_api::make_object<td_api::checkAuthenticationCode>(code), [](auto response) {
            spdlog::debug("Code sent");
        });
    }

    void send_password(const std::string& password) {
        send_query(td_api::make_object<td_api::checkAuthenticationPassword>(password), [](auto response) {
            spdlog::debug("Password sent");
        });
    }

    // Get all chats from cache (no API calls - uses data from updateNewChat events)
    std::vector<Chat> get_all_chats_sync() {
        // TDLib populates chats via updateNewChat during startup.
        // We just read from our cache - no API calls needed.
        return cache_->get_all_cached_chats();
    }

    // Search for a public chat by username
    std::optional<Chat> search_public_chat_sync(const std::string& username) {
        auto response = send_query_sync(td_api::make_object<td_api::searchPublicChat>(username));

        if (response->get_id() == td_api::chat::ID) {
            auto chat_obj = td::move_tl_object_as<td_api::chat>(response);
            auto chat = convert_chat(*chat_obj);
            cache_->cache_chat(chat);
            return chat;
        }

        return std::nullopt;
    }

    // Get chat by ID
    std::optional<Chat> get_chat_sync(int64_t chat_id) {
        auto response = send_query_sync(td_api::make_object<td_api::getChat>(chat_id));

        if (response->get_id() == td_api::chat::ID) {
            auto chat_obj = td::move_tl_object_as<td_api::chat>(response);
            auto chat = convert_chat(*chat_obj);
            cache_->cache_chat(chat);
            return chat;
        }

        return std::nullopt;
    }

    // Send text message
    Message send_text_message_sync(int64_t chat_id, const std::string& text) {
        auto input_content = td_api::make_object<td_api::inputMessageText>(
            td_api::make_object<td_api::formattedText>(text, std::vector<td_api::object_ptr<td_api::textEntity>>()),
            nullptr,
            true
        );

        auto response = send_query_sync(
            td_api::make_object<td_api::sendMessage>(
                chat_id, nullptr, nullptr, nullptr, nullptr, std::move(input_content)
            )
        );

        if (response->get_id() == td_api::message::ID) {
            auto msg_obj = td::move_tl_object_as<td_api::message>(response);
            auto message = convert_message(*msg_obj);
            cache_->cache_message(message);
            return message;
        }

        throw OperationException("Failed to send message");
    }

    // Get chat history
    std::vector<Message> get_chat_history_sync(int64_t chat_id, int limit) {
        std::vector<Message> result;

        auto response = send_query_sync(td_api::make_object<td_api::getChatHistory>(chat_id, 0, 0, limit, false));

        if (response->get_id() == td_api::messages::ID) {
            auto messages_obj = td::move_tl_object_as<td_api::messages>(response);

            for (auto& msg_ptr : messages_obj->messages_) {
                if (msg_ptr) {
                    auto message = convert_message(*msg_ptr);
                    cache_->cache_message(message);
                    result.push_back(std::move(message));
                }
            }
        }

        return result;
    }

    // Get chat history with pagination from a specific message
    std::vector<Message> get_chat_history_from_sync(int64_t chat_id, int64_t from_message_id, int limit) {
        std::vector<Message> result;

        auto response =
            send_query_sync(td_api::make_object<td_api::getChatHistory>(chat_id, from_message_id, 0, limit, false));

        if (response->get_id() == td_api::messages::ID) {
            auto messages_obj = td::move_tl_object_as<td_api::messages>(response);

            for (auto& msg_ptr : messages_obj->messages_) {
                if (msg_ptr) {
                    auto message = convert_message(*msg_ptr);
                    cache_->cache_message(message);
                    result.push_back(std::move(message));
                }
            }
        }

        return result;
    }

    // Get messages iteratively until conditions are met
    std::vector<Message>
    get_messages_until_sync(int64_t chat_id, std::size_t min_messages, std::chrono::seconds max_age) {
        std::vector<Message> result;
        int64_t from_message_id = 0;
        constexpr int batch_size = 50;

        auto now = std::chrono::system_clock::now();
        auto cutoff = now - max_age;
        auto cutoff_ts = std::chrono::duration_cast<std::chrono::seconds>(cutoff.time_since_epoch()).count();

        while (true) {
            auto batch = get_chat_history_from_sync(chat_id, from_message_id, batch_size);

            if (batch.empty()) {
                // No more messages
                break;
            }

            for (auto& msg : batch) {
                result.push_back(std::move(msg));
            }

            // Check termination conditions
            // API returns newest first, so last message in batch is oldest
            const auto& oldest = result.back();

            // Have enough messages AND oldest is older than cutoff
            if (result.size() >= min_messages && oldest.timestamp < cutoff_ts) {
                break;
            }

            // Set up for next batch: continue from oldest message
            from_message_id = oldest.id;
        }

        spdlog::debug(
            "get_messages_until_sync: chat {} fetched {} messages (min={}, max_age={}s)",
            chat_id,
            result.size(),
            min_messages,
            max_age.count()
        );

        return result;
    }

    // Send file
    Message send_file_sync(int64_t chat_id, const std::string& path, SendMode mode) {
        // Determine MIME type and whether to send as photo/video or document
        auto mime = detect_mime_type(path);
        auto detected_type = detect_media_type(fs::path(path).filename().string(), mime);

        td_api::object_ptr<td_api::InputMessageContent> input_content;

        bool send_as_media = (mode == SendMode::MEDIA) || (mode == SendMode::AUTO && is_media_type(detected_type));

        if (send_as_media && detected_type == MediaType::PHOTO) {
            auto input_file = td_api::make_object<td_api::inputFileLocal>(path);
            input_content = td_api::make_object<td_api::inputMessagePhoto>(
                std::move(input_file), nullptr, std::vector<int32_t>(), 0, 0, nullptr, false, nullptr, false
            );
        } else if (send_as_media && (detected_type == MediaType::VIDEO || detected_type == MediaType::ANIMATION)) {
            auto input_file = td_api::make_object<td_api::inputFileLocal>(path);
            input_content = td_api::make_object<td_api::inputMessageVideo>(
                std::move(input_file),
                nullptr,
                nullptr,
                0,
                std::vector<int32_t>(),
                0,
                0,
                0,
                false,
                nullptr,
                false,
                nullptr,
                false
            );
        } else {
            // Send as document
            auto input_file = td_api::make_object<td_api::inputFileLocal>(path);
            input_content =
                td_api::make_object<td_api::inputMessageDocument>(std::move(input_file), nullptr, false, nullptr);
        }

        auto response = send_query_sync(
            td_api::make_object<td_api::sendMessage>(
                chat_id, nullptr, nullptr, nullptr, nullptr, std::move(input_content)
            )
        );

        if (response->get_id() == td_api::message::ID) {
            auto msg_obj = td::move_tl_object_as<td_api::message>(response);
            auto message = convert_message(*msg_obj);
            cache_->cache_message(message);
            return message;
        }

        throw FileUploadException(path);
    }

    // Get the current logged-in user
    User get_me_sync() {
        auto response = send_query_sync(td_api::make_object<td_api::getMe>());

        if (response->get_id() != td_api::user::ID) {
            throw TelegramException("Failed to get current user");
        }

        auto user_obj = td::move_tl_object_as<td_api::user>(response);
        return convert_user(*user_obj);
    }

    // Get user by ID
    std::optional<User> get_user_sync(int64_t user_id) {
        auto response = send_query_sync(td_api::make_object<td_api::getUser>(user_id));

        if (response->get_id() != td_api::user::ID) {
            return std::nullopt;
        }

        auto user_obj = td::move_tl_object_as<td_api::user>(response);
        auto user = convert_user(*user_obj);

        // Try to get full user info for bio
        auto full_response = send_query_sync(td_api::make_object<td_api::getUserFullInfo>(user_id));
        if (full_response->get_id() == td_api::userFullInfo::ID) {
            auto full_info = td::move_tl_object_as<td_api::userFullInfo>(full_response);
            if (full_info->bio_) {
                user.bio = full_info->bio_->text_;
            }
        }

        return user;
    }

    // Get all users from private chats (reads from cache - no API calls)
    std::vector<User> get_users_sync() {
        std::vector<User> result;

        // TDLib populates our cache via updateNewChat and updateUser during startup.
        // No need to call getChats - just read from cache.

        // Read private chats from cache
        auto cached_chats = cache_->get_cached_chats_by_type(ChatType::PRIVATE);
        spdlog::debug("Found {} private chats in cache", cached_chats.size());

        for (const auto& chat : cached_chats) {
            // For private chats, user_id equals positive chat_id
            int64_t user_id = chat.id > 0 ? chat.id : -chat.id;

            // Try to get full user info from cache (populated by updateUser)
            auto cached_user = cache_->get_cached_user(user_id);

            User user;
            if (cached_user) {
                user = *cached_user;
            } else {
                // Fallback: use chat info
                user.id = user_id;
                user.first_name = chat.title;
            }

            // Always use chat's last_message info
            user.last_message_id = chat.last_message_id;
            user.last_message_timestamp = chat.last_message_timestamp;

            result.push_back(std::move(user));
        }

        spdlog::info("Retrieved {} users from cache", result.size());
        return result;
    }

    // Get user bio (separate call to avoid rate limiting during bulk load)
    std::string get_user_bio_sync(int64_t user_id) {
        auto response = send_query_sync(td_api::make_object<td_api::getUserFullInfo>(user_id));
        if (response->get_id() == td_api::userFullInfo::ID) {
            auto full_info = td::move_tl_object_as<td_api::userFullInfo>(response);
            if (full_info->bio_) {
                return full_info->bio_->text_;
            }
        }
        return "";
    }

    // Download file
    std::string download_file_sync(int32_t file_id, const std::string& destination_path) {
        // First, get the file info
        auto file_response = send_query_sync(td_api::make_object<td_api::getFile>(file_id));

        if (file_response->get_id() != td_api::file::ID) {
            throw FileNotFoundException(std::to_string(file_id));
        }

        auto file_obj = td::move_tl_object_as<td_api::file>(file_response);

        // Check if already downloaded
        if (file_obj->local_->is_downloading_completed_) {
            std::string source_path = file_obj->local_->path_;

            if (destination_path.empty()) {
                return source_path;
            }

            // Copy to destination
            fs::copy_file(source_path, destination_path, fs::copy_options::overwrite_existing);
            return destination_path;
        }

        // Download the file
        auto download_response = send_query_sync(
            td_api::make_object<td_api::downloadFile>(file_id, 32, 0, 0, true),
            30000  // 30 second timeout for downloads
        );

        if (download_response->get_id() != td_api::file::ID) {
            throw FileDownloadException(std::to_string(file_id));
        }

        auto downloaded_file = td::move_tl_object_as<td_api::file>(download_response);

        if (!downloaded_file->local_->is_downloading_completed_) {
            throw FileDownloadException(std::to_string(file_id));
        }

        std::string source_path = downloaded_file->local_->path_;

        if (destination_path.empty()) {
            return source_path;
        }

        // Copy to destination
        fs::copy_file(source_path, destination_path, fs::copy_options::overwrite_existing);
        return destination_path;
    }

private:
    void configure_tdlib_logging() {
        // Set log verbosity level
        td::ClientManager::execute(td_api::make_object<td_api::setLogVerbosityLevel>(config_.log_verbosity));

        // Configure log stream
        if (!config_.logs_directory.empty()) {
            fs::create_directories(config_.logs_directory);
            auto log_path = (fs::path(config_.logs_directory) / "tdlib.log").string();

            auto result = td::ClientManager::execute(
                td_api::make_object<td_api::setLogStream>(td_api::make_object<td_api::logStreamFile>(
                    log_path,
                    50 * 1024 * 1024,  // 50 MB max file size
                    false              // Don't redirect stderr
                ))
            );

            if (result->get_id() == td_api::ok::ID) {
                spdlog::info("TDLib logs redirected to: {}", log_path);
            } else if (result->get_id() == td_api::error::ID) {
                auto& error = static_cast<td_api::error&>(*result);
                spdlog::warn("Failed to redirect TDLib logs: {}", error.message_);
            }
        }
    }

    std::string detect_mime_type(const std::string& path) {
        auto ext = fs::path(path).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
        if (ext == ".png") return "image/png";
        if (ext == ".gif") return "image/gif";
        if (ext == ".mp4") return "video/mp4";
        if (ext == ".mov") return "video/quicktime";
        if (ext == ".pdf") return "application/pdf";

        return "application/octet-stream";
    }

    Config config_;
    CacheManager* cache_;
    std::int32_t client_id_;
    std::atomic<bool> running_;
    std::atomic<AuthState> auth_state_;

    // Thread for processing updates
    std::thread update_thread_;

    // Callback management
    std::mutex callbacks_mutex_;
    std::map<std::uint64_t, std::function<void(td_api::object_ptr<td_api::Object>)>> callbacks_;
    std::atomic<std::uint64_t> next_query_id_{1};

    // Authorization synchronisation
    mutable std::mutex auth_mutex_;
    std::condition_variable auth_cv_;

    // Message callback
    std::function<void(const Message&)> message_callback_;
    std::mutex message_callback_mutex_;

public:
    void set_message_callback(std::function<void(const Message&)> callback) {
        std::lock_guard<std::mutex> lock(message_callback_mutex_);
        message_callback_ = std::move(callback);
    }
};

// TelegramClient implementation
TelegramClient::TelegramClient(const Config& config)
    : config_(config),
      cache_(std::make_unique<CacheManager>(config.database_directory + "/cache.db")),
      impl_(std::make_unique<Impl>(config, cache_.get())) {}

TelegramClient::~TelegramClient() = default;

Task<void> TelegramClient::start() {
    impl_->start();
    co_return;
}

Task<void> TelegramClient::stop() {
    impl_->stop();
    co_return;
}

Task<AuthState> TelegramClient::get_auth_state() { co_return impl_->get_auth_state(); }

Task<void> TelegramClient::login(const std::string& phone) {
    impl_->send_phone_number(phone);
    co_return;
}

Task<void> TelegramClient::submit_code(const std::string& code) {
    impl_->send_code(code);
    co_return;
}

Task<void> TelegramClient::submit_password(const std::string& password) {
    impl_->send_password(password);
    co_return;
}

Task<void> TelegramClient::logout() {
    // TODO: Implement logout
    co_return;
}

Task<std::vector<User>> TelegramClient::get_users() { co_return impl_->get_users_sync(); }

Task<std::vector<Chat>> TelegramClient::get_groups() {
    auto all_chats = co_await get_all_chats();
    std::vector<Chat> groups;

    for (const auto& chat : all_chats) {
        if (chat.is_group()) {
            groups.push_back(chat);
        }
    }

    co_return groups;
}

Task<std::vector<Chat>> TelegramClient::get_channels() {
    auto all_chats = co_await get_all_chats();
    std::vector<Chat> channels;

    for (const auto& chat : all_chats) {
        if (chat.is_channel()) {
            channels.push_back(chat);
        }
    }

    co_return channels;
}

Task<std::vector<Chat>> TelegramClient::get_all_chats() { co_return impl_->get_all_chats_sync(); }

Task<std::optional<Chat>> TelegramClient::resolve_username(const std::string& username) {
    // Remove @ or # prefix if present
    std::string clean_username = username;
    if (!clean_username.empty() && (clean_username[0] == '@' || clean_username[0] == '#')) {
        clean_username = clean_username.substr(1);
    }

    co_return impl_->search_public_chat_sync(clean_username);
}

Task<std::optional<Chat>> TelegramClient::get_chat(int64_t chat_id) { co_return impl_->get_chat_sync(chat_id); }

Task<std::optional<User>> TelegramClient::get_user(int64_t user_id) { co_return impl_->get_user_sync(user_id); }

Task<User> TelegramClient::get_me() { co_return impl_->get_me_sync(); }

Task<Message> TelegramClient::send_text(int64_t chat_id, const std::string& text) {
    co_return impl_->send_text_message_sync(chat_id, text);
}

Task<std::vector<Message>> TelegramClient::get_messages(int64_t chat_id, int limit) {
    co_return impl_->get_chat_history_sync(chat_id, limit);
}

Task<std::vector<Message>> TelegramClient::get_last_n_messages(int64_t chat_id, int n) {
    co_return co_await get_messages(chat_id, n);
}

Task<std::vector<Message>>
TelegramClient::get_messages_until(int64_t chat_id, std::size_t min_messages, std::chrono::seconds max_age) {
    co_return impl_->get_messages_until_sync(chat_id, min_messages, max_age);
}

Task<Message> TelegramClient::send_file(int64_t chat_id, const std::string& path, SendMode mode) {
    co_return impl_->send_file_sync(chat_id, path, mode);
}

Task<std::vector<FileListItem>> TelegramClient::list_media(int64_t chat_id) {
    // Get messages and filter for media
    auto messages = co_await get_messages(chat_id, 100);
    std::vector<FileListItem> result;

    for (const auto& msg : messages) {
        if (msg.has_media() && is_media_type(msg.media->type)) {
            FileListItem item;
            item.message_id = msg.id;
            item.filename = msg.media->filename;
            item.file_size = msg.media->file_size;
            item.timestamp = msg.timestamp;
            item.type = msg.media->type;
            item.file_id = msg.media->file_id;
            result.push_back(std::move(item));
        }
    }

    co_return result;
}

Task<std::vector<FileListItem>> TelegramClient::list_files(int64_t chat_id) {
    // Get messages and filter for documents
    auto messages = co_await get_messages(chat_id, 100);
    std::vector<FileListItem> result;

    for (const auto& msg : messages) {
        if (msg.has_media() && is_document_type(msg.media->type)) {
            FileListItem item;
            item.message_id = msg.id;
            item.filename = msg.media->filename;
            item.file_size = msg.media->file_size;
            item.timestamp = msg.timestamp;
            item.type = msg.media->type;
            item.file_id = msg.media->file_id;
            result.push_back(std::move(item));
        }
    }

    co_return result;
}

Task<std::string> TelegramClient::download_file(const std::string& file_id, const std::string& destination_path) {
    try {
        int32_t file_id_int = std::stoi(file_id);
        co_return impl_->download_file_sync(file_id_int, destination_path);
    } catch (const std::exception& e) {
        throw FileNotFoundException(file_id);
    }
}

Task<ChatStatus> TelegramClient::get_chat_status(int64_t chat_id) {
    auto chat = co_await get_chat(chat_id);

    if (chat) {
        co_return ChatStatus{chat->last_message_id, chat->last_message_timestamp};
    }

    co_return ChatStatus{0, 0};
}

Task<std::string> TelegramClient::get_user_bio(int64_t user_id) { co_return impl_->get_user_bio_sync(user_id); }

void TelegramClient::set_message_callback(MessageCallback callback) {
    impl_->set_message_callback(std::move(callback));
}

}  // namespace tg
