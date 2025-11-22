#include "tg/types.hpp"

#include <gtest/gtest.h>

namespace tg {
namespace {

// Test User structure
TEST(TypesTest, UserDisplayName) {
    User user;
    user.id = 123;
    user.username = "johndoe";
    user.first_name = "John";
    user.last_name = "Doe";

    EXPECT_EQ(user.display_name(), "John Doe");
}

TEST(TypesTest, UserDisplayNameFirstOnly) {
    User user;
    user.id = 123;
    user.username = "jane";
    user.first_name = "Jane";
    user.last_name = "";

    EXPECT_EQ(user.display_name(), "Jane");
}

TEST(TypesTest, UserDisplayNameFromUsername) {
    User user;
    user.id = 123;
    user.username = "testuser";
    user.first_name = "";
    user.last_name = "";

    EXPECT_EQ(user.display_name(), "@testuser");
}

TEST(TypesTest, UserDisplayNameFallbackToId) {
    User user;
    user.id = 123;
    user.username = "";
    user.first_name = "";
    user.last_name = "";

    EXPECT_EQ(user.display_name(), "User 123");
}

TEST(TypesTest, UserGetIdentifier) {
    User user;
    user.id = 123;
    user.username = "alice";
    user.first_name = "Alice";

    EXPECT_EQ(user.get_identifier(), "@alice");
}

TEST(TypesTest, UserGetIdentifierNoUsername) {
    User user;
    user.id = 123;
    user.username = "";
    user.first_name = "Bob";
    user.last_name = "Smith";

    EXPECT_EQ(user.get_identifier(), "Bob Smith");
}

// Test Chat structure
TEST(TypesTest, ChatDirectoryNamePrivate) {
    Chat chat;
    chat.id = 123;
    chat.type = ChatType::PRIVATE;
    chat.username = "alice";
    chat.title = "Alice";

    EXPECT_EQ(chat.get_directory_name(), "@alice");
}

TEST(TypesTest, ChatDirectoryNameGroup) {
    Chat chat;
    chat.id = -100123456789;
    chat.type = ChatType::GROUP;
    chat.username = "mygroup";
    chat.title = "My Group";

    EXPECT_EQ(chat.get_directory_name(), "#mygroup");
}

TEST(TypesTest, ChatDirectoryNameChannel) {
    Chat chat;
    chat.id = -100123456789;
    chat.type = ChatType::CHANNEL;
    chat.username = "mychannel";
    chat.title = "My Channel";

    EXPECT_EQ(chat.get_directory_name(), "#mychannel");
}

TEST(TypesTest, ChatDirectoryNameNoUsername) {
    Chat chat;
    chat.id = -100123456789;
    chat.type = ChatType::SUPERGROUP;
    chat.username = "";
    chat.title = "Private Group";

    EXPECT_EQ(chat.get_directory_name(), "-100123456789");
}

TEST(TypesTest, ChatIsPrivate) {
    Chat chat;
    chat.type = ChatType::PRIVATE;

    EXPECT_TRUE(chat.is_private());
    EXPECT_FALSE(chat.is_group());
    EXPECT_FALSE(chat.is_channel());
}

TEST(TypesTest, ChatIsGroup) {
    Chat chat;
    chat.type = ChatType::GROUP;

    EXPECT_FALSE(chat.is_private());
    EXPECT_TRUE(chat.is_group());
    EXPECT_FALSE(chat.is_channel());
}

TEST(TypesTest, ChatIsSupergroup) {
    Chat chat;
    chat.type = ChatType::SUPERGROUP;

    EXPECT_FALSE(chat.is_private());
    EXPECT_TRUE(chat.is_group());  // Supergroups count as groups
    EXPECT_FALSE(chat.is_channel());
}

TEST(TypesTest, ChatIsChannel) {
    Chat chat;
    chat.type = ChatType::CHANNEL;

    EXPECT_FALSE(chat.is_private());
    EXPECT_FALSE(chat.is_group());
    EXPECT_TRUE(chat.is_channel());
}

// Test MediaInfo structure
TEST(TypesTest, MediaInfoGetExtensionFromFilename) {
    MediaInfo info;
    info.filename = "photo.jpg";
    info.type = MediaType::PHOTO;

    EXPECT_EQ(info.get_extension(), ".jpg");
}

TEST(TypesTest, MediaInfoGetExtensionFromType) {
    MediaInfo info;
    info.filename = "";
    info.type = MediaType::VIDEO;

    EXPECT_EQ(info.get_extension(), ".mp4");
}

TEST(TypesTest, MediaInfoGetExtensionPhoto) {
    MediaInfo info;
    info.type = MediaType::PHOTO;
    EXPECT_EQ(info.get_extension(), ".jpg");
}

TEST(TypesTest, MediaInfoGetExtensionAudio) {
    MediaInfo info;
    info.type = MediaType::AUDIO;
    EXPECT_EQ(info.get_extension(), ".mp3");
}

// Test Message structure
TEST(TypesTest, MessageHasMedia) {
    Message msg;
    msg.id = 1;
    msg.chat_id = 123;
    msg.text = "Test";

    EXPECT_FALSE(msg.has_media());

    msg.media = MediaInfo{};
    msg.media->type = MediaType::PHOTO;

    EXPECT_TRUE(msg.has_media());
}

TEST(TypesTest, MessageFormatForDisplay) {
    Message msg;
    msg.id = 1;
    msg.chat_id = 123;
    msg.timestamp = 1234567890;
    msg.text = "Hello world";

    std::string formatted = msg.format_for_display();

    EXPECT_NE(formatted.find("[1234567890]"), std::string::npos);
    EXPECT_NE(formatted.find("Hello world"), std::string::npos);
}

TEST(TypesTest, MessageFormatForDisplayWithMedia) {
    Message msg;
    msg.id = 1;
    msg.chat_id = 123;
    msg.timestamp = 1234567890;
    msg.text = "Check this out";
    msg.media = MediaInfo{};
    msg.media->type = MediaType::PHOTO;
    msg.media->filename = "photo.jpg";

    std::string formatted = msg.format_for_display();

    EXPECT_NE(formatted.find("Check this out"), std::string::npos);
    EXPECT_NE(formatted.find("[photo:"), std::string::npos);
    EXPECT_NE(formatted.find("photo.jpg"), std::string::npos);
}

// Test FileListItem structure
TEST(TypesTest, FileListItemGetSizeString) {
    FileListItem item;

    item.file_size = 512;
    EXPECT_EQ(item.get_size_string(), "512.00 B");

    item.file_size = 2048;
    EXPECT_EQ(item.get_size_string(), "2.00 KB");

    item.file_size = 1024 * 1024;
    EXPECT_EQ(item.get_size_string(), "1.00 MB");

    item.file_size = 1024 * 1024 * 1024;
    EXPECT_EQ(item.get_size_string(), "1.00 GB");
}

// Test utility functions
TEST(TypesTest, ChatTypeToString) {
    EXPECT_EQ(chat_type_to_string(ChatType::PRIVATE), "private");
    EXPECT_EQ(chat_type_to_string(ChatType::GROUP), "group");
    EXPECT_EQ(chat_type_to_string(ChatType::SUPERGROUP), "supergroup");
    EXPECT_EQ(chat_type_to_string(ChatType::CHANNEL), "channel");
}

TEST(TypesTest, MediaTypeToString) {
    EXPECT_EQ(media_type_to_string(MediaType::PHOTO), "photo");
    EXPECT_EQ(media_type_to_string(MediaType::VIDEO), "video");
    EXPECT_EQ(media_type_to_string(MediaType::DOCUMENT), "document");
    EXPECT_EQ(media_type_to_string(MediaType::AUDIO), "audio");
    EXPECT_EQ(media_type_to_string(MediaType::VOICE), "voice");
}

TEST(TypesTest, DetectMediaTypeFromJpeg) {
    auto type = detect_media_type("photo.jpg", "image/jpeg");
    EXPECT_EQ(type, MediaType::PHOTO);
}

TEST(TypesTest, DetectMediaTypeFromPng) {
    auto type = detect_media_type("image.png", "image/png");
    EXPECT_EQ(type, MediaType::PHOTO);
}

TEST(TypesTest, DetectMediaTypeFromGif) {
    auto type = detect_media_type("animation.gif", "image/gif");
    EXPECT_EQ(type, MediaType::ANIMATION);
}

TEST(TypesTest, DetectMediaTypeFromMp4) {
    auto type = detect_media_type("video.mp4", "video/mp4");
    EXPECT_EQ(type, MediaType::VIDEO);
}

TEST(TypesTest, DetectMediaTypeFromMp3) {
    auto type = detect_media_type("song.mp3", "audio/mpeg");
    EXPECT_EQ(type, MediaType::AUDIO);
}

TEST(TypesTest, DetectMediaTypeFromExtensionOnly) {
    auto type = detect_media_type("photo.JPG", "");
    EXPECT_EQ(type, MediaType::PHOTO);
}

TEST(TypesTest, DetectMediaTypeDefault) {
    auto type = detect_media_type("file.xyz", "application/octet-stream");
    EXPECT_EQ(type, MediaType::DOCUMENT);
}

TEST(TypesTest, IsMediaType) {
    EXPECT_TRUE(is_media_type(MediaType::PHOTO));
    EXPECT_TRUE(is_media_type(MediaType::VIDEO));
    EXPECT_TRUE(is_media_type(MediaType::ANIMATION));

    EXPECT_FALSE(is_media_type(MediaType::DOCUMENT));
    EXPECT_FALSE(is_media_type(MediaType::AUDIO));
    EXPECT_FALSE(is_media_type(MediaType::VOICE));
}

TEST(TypesTest, IsDocumentType) {
    EXPECT_TRUE(is_document_type(MediaType::DOCUMENT));
    EXPECT_TRUE(is_document_type(MediaType::AUDIO));
    EXPECT_TRUE(is_document_type(MediaType::VOICE));
    EXPECT_TRUE(is_document_type(MediaType::STICKER));

    EXPECT_FALSE(is_document_type(MediaType::PHOTO));
    EXPECT_FALSE(is_document_type(MediaType::VIDEO));
    EXPECT_FALSE(is_document_type(MediaType::ANIMATION));
}

// Stress test with random data (TDLib pattern)
TEST(TypesTest, StressTestMediaDetection) {
    std::vector<std::pair<std::string, MediaType>> test_cases = {
        {"file.jpg", MediaType::PHOTO},
        {"file.jpeg", MediaType::PHOTO},
        {"file.png", MediaType::PHOTO},
        {"file.gif", MediaType::ANIMATION},
        {"file.mp4", MediaType::VIDEO},
        {"file.mov", MediaType::VIDEO},
        {"file.mp3", MediaType::AUDIO},
        {"file.ogg", MediaType::AUDIO},
        {"file.pdf", MediaType::DOCUMENT},
        {"file.doc", MediaType::DOCUMENT},
    };

    // Run 1000 iterations to stress test
    for (int i = 0; i < 1000; ++i) {
        for (const auto& [filename, expected_type] : test_cases) {
            auto detected = detect_media_type(filename, "");
            EXPECT_EQ(detected, expected_type) << "Failed for: " << filename << " iteration " << i;
        }
    }
}

}  // namespace
}  // namespace tg
