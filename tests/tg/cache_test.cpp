#include "tg/cache.hpp"

#include "tg/exceptions.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <thread>

namespace tg {
namespace {

namespace fs = std::filesystem;

// Test fixture for cache tests
class CacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary database
        temp_db_path_ = "/tmp/tg_cache_test_" + std::to_string(std::time(nullptr)) + ".db";
        cache_ = std::make_unique<CacheManager>(temp_db_path_);
    }

    void TearDown() override {
        cache_.reset();  // Close database
        fs::remove(temp_db_path_);  // Clean up
    }

    std::string temp_db_path_;
    std::unique_ptr<CacheManager> cache_;
};

// User caching tests
TEST_F(CacheTest, CacheAndRetrieveUser) {
    User user;
    user.id = 123;
    user.username = "testuser";
    user.first_name = "Test";
    user.last_name = "User";
    user.phone_number = "+1234567890";
    user.is_contact = true;
    user.last_message_id = 456;
    user.last_message_timestamp = 1234567890;

    cache_->cache_user(user);

    auto retrieved = cache_->get_cached_user(123);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->id, user.id);
    EXPECT_EQ(retrieved->username, user.username);
    EXPECT_EQ(retrieved->first_name, user.first_name);
    EXPECT_EQ(retrieved->last_name, user.last_name);
    EXPECT_EQ(retrieved->phone_number, user.phone_number);
    EXPECT_EQ(retrieved->is_contact, user.is_contact);
}

TEST_F(CacheTest, GetUserByUsername) {
    User user;
    user.id = 123;
    user.username = "alice";
    user.first_name = "Alice";

    cache_->cache_user(user);

    auto retrieved = cache_->get_cached_user_by_username("alice");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->id, 123);
    EXPECT_EQ(retrieved->username, "alice");
}

TEST_F(CacheTest, GetNonExistentUser) {
    auto retrieved = cache_->get_cached_user(999);
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(CacheTest, GetAllCachedUsers) {
    User user1{123, "alice", "Alice", "", "", true, 0, 0};
    User user2{456, "bob", "Bob", "", "", false, 0, 0};

    cache_->cache_user(user1);
    cache_->cache_user(user2);

    auto users = cache_->get_all_cached_users();
    EXPECT_EQ(users.size(), 2);
}

TEST_F(CacheTest, UpdateExistingUser) {
    User user{123, "alice", "Alice", "", "", true, 0, 0};
    cache_->cache_user(user);

    // Update user
    user.first_name = "Alice Updated";
    user.last_name = "Smith";
    cache_->cache_user(user);

    auto retrieved = cache_->get_cached_user(123);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->first_name, "Alice Updated");
    EXPECT_EQ(retrieved->last_name, "Smith");
}

// Chat caching tests
TEST_F(CacheTest, CacheAndRetrieveChat) {
    Chat chat;
    chat.id = 123;
    chat.type = ChatType::PRIVATE;
    chat.title = "Test Chat";
    chat.username = "testchat";
    chat.last_message_id = 456;
    chat.last_message_timestamp = 1234567890;

    cache_->cache_chat(chat);

    auto retrieved = cache_->get_cached_chat(123);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->id, chat.id);
    EXPECT_EQ(retrieved->type, chat.type);
    EXPECT_EQ(retrieved->title, chat.title);
    EXPECT_EQ(retrieved->username, chat.username);
}

TEST_F(CacheTest, GetChatByUsername) {
    Chat chat{123, ChatType::GROUP, "My Group", "mygroup", 0, 0};
    cache_->cache_chat(chat);

    auto retrieved = cache_->get_cached_chat_by_username("mygroup");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->id, 123);
}

TEST_F(CacheTest, GetChatsByType) {
    Chat private_chat{123, ChatType::PRIVATE, "Alice", "alice", 0, 0};
    Chat group_chat{456, ChatType::GROUP, "Group", "group", 0, 0};
    Chat channel{789, ChatType::CHANNEL, "Channel", "channel", 0, 0};

    cache_->cache_chat(private_chat);
    cache_->cache_chat(group_chat);
    cache_->cache_chat(channel);

    auto groups = cache_->get_cached_chats_by_type(ChatType::GROUP);
    EXPECT_EQ(groups.size(), 1);
    EXPECT_EQ(groups[0].id, 456);

    auto channels = cache_->get_cached_chats_by_type(ChatType::CHANNEL);
    EXPECT_EQ(channels.size(), 1);
    EXPECT_EQ(channels[0].id, 789);
}

TEST_F(CacheTest, UpdateChatStatus) {
    Chat chat{123, ChatType::PRIVATE, "Alice", "alice", 0, 0};
    cache_->cache_chat(chat);

    cache_->update_chat_status(123, 999, 1234567899);

    auto retrieved = cache_->get_cached_chat(123);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->last_message_id, 999);
    EXPECT_EQ(retrieved->last_message_timestamp, 1234567899);
}

// Message caching tests
TEST_F(CacheTest, CacheAndRetrieveMessage) {
    Message msg;
    msg.id = 1;
    msg.chat_id = 123;
    msg.sender_id = 456;
    msg.timestamp = 1234567890;
    msg.text = "Hello world";
    msg.is_outgoing = true;

    cache_->cache_message(msg);

    auto retrieved = cache_->get_cached_message(123, 1);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->id, msg.id);
    EXPECT_EQ(retrieved->chat_id, msg.chat_id);
    EXPECT_EQ(retrieved->text, msg.text);
    EXPECT_EQ(retrieved->is_outgoing, msg.is_outgoing);
}

TEST_F(CacheTest, CacheMessageWithMedia) {
    Message msg;
    msg.id = 1;
    msg.chat_id = 123;
    msg.sender_id = 456;
    msg.timestamp = 1234567890;
    msg.text = "Photo";
    msg.is_outgoing = false;

    MediaInfo media;
    media.type = MediaType::PHOTO;
    media.file_id = "file123";
    media.filename = "photo.jpg";
    media.mime_type = "image/jpeg";
    media.file_size = 1024;
    media.width = 800;
    media.height = 600;

    msg.media = media;

    cache_->cache_message(msg);

    auto retrieved = cache_->get_cached_message(123, 1);
    ASSERT_TRUE(retrieved.has_value());
    ASSERT_TRUE(retrieved->has_media());
    EXPECT_EQ(retrieved->media->type, MediaType::PHOTO);
    EXPECT_EQ(retrieved->media->filename, "photo.jpg");
    EXPECT_EQ(retrieved->media->file_size, 1024);
    EXPECT_EQ(retrieved->media->width, 800);
    EXPECT_EQ(retrieved->media->height, 600);
}

TEST_F(CacheTest, BulkCacheMessages) {
    std::vector<Message> messages;
    for (int i = 0; i < 10; ++i) {
        Message msg;
        msg.id = i;
        msg.chat_id = 123;
        msg.sender_id = 456;
        msg.timestamp = 1234567890 + i;
        msg.text = "Message " + std::to_string(i);
        msg.is_outgoing = (i % 2 == 0);
        messages.push_back(msg);
    }

    cache_->cache_messages(messages);

    auto retrieved_messages = cache_->get_cached_messages(123, 10);
    EXPECT_EQ(retrieved_messages.size(), 10);
}

TEST_F(CacheTest, GetLastNMessages) {
    std::vector<Message> messages;
    for (int i = 0; i < 20; ++i) {
        Message msg;
        msg.id = i;
        msg.chat_id = 123;
        msg.sender_id = 456;
        msg.timestamp = 1234567890 + i;
        msg.text = "Message " + std::to_string(i);
        msg.is_outgoing = false;
        messages.push_back(msg);
    }

    cache_->cache_messages(messages);

    auto last_5 = cache_->get_last_n_messages(123, 5);
    EXPECT_EQ(last_5.size(), 5);

    // Should be in reverse chronological order (newest first)
    EXPECT_EQ(last_5[0].id, 19);
    EXPECT_EQ(last_5[4].id, 15);
}

// File caching tests
TEST_F(CacheTest, CacheFileItem) {
    FileListItem item;
    item.message_id = 123;
    item.filename = "photo.jpg";
    item.file_size = 1024;
    item.timestamp = 1234567890;
    item.type = MediaType::PHOTO;
    item.file_id = "file123";

    cache_->cache_file_item(456, item);

    auto files = cache_->get_cached_file_list(456);
    EXPECT_EQ(files.size(), 1);
    EXPECT_EQ(files[0].filename, "photo.jpg");
}

TEST_F(CacheTest, FilterFilesByType) {
    FileListItem photo{1, "photo.jpg", 1024, 1234567890, MediaType::PHOTO, "file1"};
    FileListItem video{2, "video.mp4", 2048, 1234567891, MediaType::VIDEO, "file2"};
    FileListItem doc{3, "doc.pdf", 512, 1234567892, MediaType::DOCUMENT, "file3"};

    cache_->cache_file_item(123, photo);
    cache_->cache_file_item(123, video);
    cache_->cache_file_item(123, doc);

    auto all_files = cache_->get_cached_file_list(123);
    EXPECT_EQ(all_files.size(), 3);

    auto photos = cache_->get_cached_file_list(123, MediaType::PHOTO);
    EXPECT_EQ(photos.size(), 1);
    EXPECT_EQ(photos[0].type, MediaType::PHOTO);

    auto documents = cache_->get_cached_file_list(123, MediaType::DOCUMENT);
    EXPECT_EQ(documents.size(), 1);
    EXPECT_EQ(documents[0].type, MediaType::DOCUMENT);
}

// Cache invalidation tests
TEST_F(CacheTest, InvalidateChatMessages) {
    // Add messages for two chats
    Message msg1{1, 123, 456, 1234567890, "Chat 1 msg", std::nullopt, false};
    Message msg2{2, 456, 456, 1234567891, "Chat 2 msg", std::nullopt, false};

    cache_->cache_message(msg1);
    cache_->cache_message(msg2);

    // Invalidate chat 123
    cache_->invalidate_chat_messages(123);

    // Chat 123 messages should be gone
    auto chat1_msgs = cache_->get_cached_messages(123, 10);
    EXPECT_EQ(chat1_msgs.size(), 0);

    // Chat 456 messages should remain
    auto chat2_msgs = cache_->get_cached_messages(456, 10);
    EXPECT_EQ(chat2_msgs.size(), 1);
}

TEST_F(CacheTest, InvalidateChat) {
    Chat chat{123, ChatType::PRIVATE, "Test", "test", 0, 0};
    Message msg{1, 123, 456, 1234567890, "Test message", std::nullopt, false};
    FileListItem file{1, "file.txt", 100, 1234567890, MediaType::DOCUMENT, "file1"};

    cache_->cache_chat(chat);
    cache_->cache_message(msg);
    cache_->cache_file_item(123, file);

    cache_->invalidate_chat(123);

    EXPECT_FALSE(cache_->get_cached_chat(123).has_value());
    EXPECT_EQ(cache_->get_cached_messages(123, 10).size(), 0);
    EXPECT_EQ(cache_->get_cached_file_list(123).size(), 0);
}

TEST_F(CacheTest, ClearAll) {
    User user{123, "alice", "Alice", "", "", true, 0, 0};
    Chat chat{456, ChatType::GROUP, "Group", "group", 0, 0};
    Message msg{1, 456, 123, 1234567890, "Message", std::nullopt, false};

    cache_->cache_user(user);
    cache_->cache_chat(chat);
    cache_->cache_message(msg);

    cache_->clear_all();

    EXPECT_EQ(cache_->get_all_cached_users().size(), 0);
    EXPECT_EQ(cache_->get_all_cached_chats().size(), 0);
    EXPECT_EQ(cache_->get_cached_messages(456, 10).size(), 0);
}

// Concurrent access tests (TDLib pattern)
TEST_F(CacheTest, ConcurrentUserCaching) {
    const int num_threads = 10;
    const int users_per_thread = 100;

    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, users_per_thread]() {
            for (int i = 0; i < users_per_thread; ++i) {
                User user;
                user.id = t * users_per_thread + i;
                user.username = "user" + std::to_string(user.id);
                user.first_name = "User";
                user.is_contact = true;

                cache_->cache_user(user);
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    auto all_users = cache_->get_all_cached_users();
    EXPECT_EQ(all_users.size(), num_threads * users_per_thread);
}

TEST_F(CacheTest, ConcurrentReadWrite) {
    // Add initial user
    User initial_user{1, "test", "Test", "", "", true, 0, 0};
    cache_->cache_user(initial_user);

    std::atomic<int> read_count{0};
    std::atomic<int> write_count{0};

    std::vector<std::thread> threads;

    // Reader threads
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, &read_count]() {
            for (int j = 0; j < 1000; ++j) {
                auto user = cache_->get_cached_user(1);
                if (user) {
                    read_count++;
                }
            }
        });
    }

    // Writer threads
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([this, &write_count]() {
            for (int j = 0; j < 1000; ++j) {
                User user{1, "test", "Test Updated", "", "", true, 0, 0};
                cache_->cache_user(user);
                write_count++;
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(read_count, 5000);
    EXPECT_EQ(write_count, 5000);
}

// Persistence test
TEST_F(CacheTest, Persistence) {
    User user{123, "alice", "Alice", "", "", true, 0, 0};
    cache_->cache_user(user);

    // Close and reopen
    std::string db_path = temp_db_path_;
    cache_.reset();

    cache_ = std::make_unique<CacheManager>(db_path);

    auto retrieved = cache_->get_cached_user(123);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->username, "alice");
}

// Stress test (TDLib pattern)
TEST_F(CacheTest, StressTestMessageCaching) {
    const int num_chats = 10;
    const int messages_per_chat = 1000;

    for (int chat = 0; chat < num_chats; ++chat) {
        std::vector<Message> messages;
        for (int i = 0; i < messages_per_chat; ++i) {
            Message msg;
            msg.id = i;
            msg.chat_id = chat;
            msg.sender_id = 123;
            msg.timestamp = 1234567890 + i;
            msg.text = "Message " + std::to_string(i);
            msg.is_outgoing = (i % 2 == 0);
            messages.push_back(msg);
        }

        cache_->cache_messages(messages);
    }

    // Verify all messages are cached
    for (int chat = 0; chat < num_chats; ++chat) {
        auto messages = cache_->get_cached_messages(chat, messages_per_chat);
        EXPECT_EQ(messages.size(), messages_per_chat);
    }
}

}  // namespace
}  // namespace tg
