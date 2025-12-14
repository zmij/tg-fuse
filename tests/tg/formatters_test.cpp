#include "tg/formatters.hpp"
#include "tg/types.hpp"

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <gtest/gtest.h>

namespace tg {
namespace {

// Test UserStatus formatter
TEST(FormattersTest, UserStatusOnline) { EXPECT_EQ(fmt::format("{}", UserStatus::ONLINE), "online"); }

TEST(FormattersTest, UserStatusOffline) { EXPECT_EQ(fmt::format("{}", UserStatus::OFFLINE), "offline"); }

TEST(FormattersTest, UserStatusRecently) { EXPECT_EQ(fmt::format("{}", UserStatus::RECENTLY), "recently"); }

TEST(FormattersTest, UserStatusLastWeek) { EXPECT_EQ(fmt::format("{}", UserStatus::LAST_WEEK), "last week"); }

TEST(FormattersTest, UserStatusLastMonth) { EXPECT_EQ(fmt::format("{}", UserStatus::LAST_MONTH), "last month"); }

TEST(FormattersTest, UserStatusUnknown) { EXPECT_EQ(fmt::format("{}", UserStatus::UNKNOWN), "unknown"); }

// Test ChatType formatter
TEST(FormattersTest, ChatTypePrivate) { EXPECT_EQ(fmt::format("{}", ChatType::PRIVATE), "private"); }

TEST(FormattersTest, ChatTypeGroup) { EXPECT_EQ(fmt::format("{}", ChatType::GROUP), "group"); }

TEST(FormattersTest, ChatTypeSupergroup) { EXPECT_EQ(fmt::format("{}", ChatType::SUPERGROUP), "supergroup"); }

TEST(FormattersTest, ChatTypeChannel) { EXPECT_EQ(fmt::format("{}", ChatType::CHANNEL), "channel"); }

// Test MediaType formatter
TEST(FormattersTest, MediaTypePhoto) { EXPECT_EQ(fmt::format("{}", MediaType::PHOTO), "photo"); }

TEST(FormattersTest, MediaTypeVideo) { EXPECT_EQ(fmt::format("{}", MediaType::VIDEO), "video"); }

TEST(FormattersTest, MediaTypeDocument) { EXPECT_EQ(fmt::format("{}", MediaType::DOCUMENT), "document"); }

TEST(FormattersTest, MediaTypeAudio) { EXPECT_EQ(fmt::format("{}", MediaType::AUDIO), "audio"); }

TEST(FormattersTest, MediaTypeVoice) { EXPECT_EQ(fmt::format("{}", MediaType::VOICE), "voice"); }

TEST(FormattersTest, MediaTypeAnimation) { EXPECT_EQ(fmt::format("{}", MediaType::ANIMATION), "animation"); }

TEST(FormattersTest, MediaTypeSticker) { EXPECT_EQ(fmt::format("{}", MediaType::STICKER), "sticker"); }

// Test MediaInfo formatter
TEST(FormattersTest, MediaInfoPhotoNoFilename) {
    MediaInfo media;
    media.type = MediaType::PHOTO;
    media.filename = "";

    EXPECT_EQ(fmt::format("{}", media), "[photo]");
}

TEST(FormattersTest, MediaInfoDocumentWithFilename) {
    MediaInfo media;
    media.type = MediaType::DOCUMENT;
    media.filename = "report.pdf";

    EXPECT_EQ(fmt::format("{}", media), "[document: report.pdf]");
}

TEST(FormattersTest, MediaInfoAudioWithFilename) {
    MediaInfo media;
    media.type = MediaType::AUDIO;
    media.filename = "song.mp3";

    EXPECT_EQ(fmt::format("{}", media), "[audio: song.mp3]");
}

// Test User formatter with different format specs
TEST(FormattersTest, UserDefaultFormat) {
    User user;
    user.id = 123;
    user.first_name = "John";
    user.last_name = "Doe";
    user.username = "johndoe";

    // Default format (display name)
    std::string result = fmt::format("{}", user);
    EXPECT_TRUE(result.find("John") != std::string::npos);
    EXPECT_TRUE(result.find("Doe") != std::string::npos);
}

TEST(FormattersTest, UserDisplayNameFormat) {
    User user;
    user.id = 123;
    user.first_name = "John";
    user.last_name = "Doe";
    user.username = "johndoe";

    std::string result = fmt::format("{:d}", user);
    EXPECT_TRUE(result.find("John") != std::string::npos);
    EXPECT_TRUE(result.find("@johndoe") != std::string::npos);
}

TEST(FormattersTest, UserUsernameFormat) {
    User user;
    user.id = 123;
    user.first_name = "John";
    user.last_name = "Doe";
    user.username = "johndoe";

    std::string result = fmt::format("{:u}", user);
    EXPECT_EQ(result, "@johndoe");
}

TEST(FormattersTest, UserFullNameFormat) {
    User user;
    user.id = 123;
    user.first_name = "John";
    user.last_name = "Doe";
    user.username = "johndoe";

    std::string result = fmt::format("{:f}", user);
    EXPECT_EQ(result, "John Doe");
}

TEST(FormattersTest, UserIdentifierFormat) {
    User user;
    user.id = 123;
    user.first_name = "John";
    user.last_name = "Doe";
    user.username = "johndoe";

    std::string result = fmt::format("{:i}", user);
    EXPECT_EQ(result, "123");
}

TEST(FormattersTest, UserNoUsernameFormat) {
    User user;
    user.id = 456;
    user.first_name = "Jane";
    user.last_name = "";
    user.username = "";

    std::string result = fmt::format("{:u}", user);
    // Fallback to user ID when no username
    EXPECT_TRUE(result.find("456") != std::string::npos);
}

// Test Chat formatter
TEST(FormattersTest, ChatWithUsername) {
    Chat chat;
    chat.id = 100;
    chat.title = "Developer Chat";
    chat.username = "devchat";
    chat.type = ChatType::SUPERGROUP;

    std::string result = fmt::format("{}", chat);
    EXPECT_EQ(result, "Developer Chat (@devchat)");
}

TEST(FormattersTest, ChatWithoutUsername) {
    Chat chat;
    chat.id = 100;
    chat.title = "Private Group";
    chat.username = "";
    chat.type = ChatType::GROUP;

    std::string result = fmt::format("{}", chat);
    EXPECT_EQ(result, "Private Group");
}

// Test MessageInfo formatter
TEST(FormattersTest, MessageInfoOutgoing) {
    User sender;
    sender.id = 1;
    sender.first_name = "Me";

    Chat chat;
    chat.id = 100;
    chat.title = "Test Chat";

    Message msg;
    msg.id = 1;
    msg.chat_id = 100;
    msg.sender_id = 1;
    msg.timestamp = std::time(nullptr);
    msg.text = "Hello world";
    msg.is_outgoing = true;

    MessageInfo info{msg, sender, chat};
    std::string result = fmt::format("{}", info);

    EXPECT_TRUE(result.find("> **You**") != std::string::npos);
    EXPECT_TRUE(result.find("Hello world") != std::string::npos);
}

TEST(FormattersTest, MessageInfoIncoming) {
    User sender;
    sender.id = 2;
    sender.first_name = "Alice";
    sender.last_name = "Smith";
    sender.username = "alice";

    Chat chat;
    chat.id = 100;
    chat.title = "Test Chat";

    Message msg;
    msg.id = 2;
    msg.chat_id = 100;
    msg.sender_id = 2;
    msg.timestamp = std::time(nullptr);
    msg.text = "Hi there";
    msg.is_outgoing = false;

    MessageInfo info{msg, sender, chat};
    std::string result = fmt::format("{}", info);

    EXPECT_TRUE(result.find("Alice") != std::string::npos);
    EXPECT_TRUE(result.find("Hi there") != std::string::npos);
}

TEST(FormattersTest, MessageInfoWithMedia) {
    User sender;
    sender.id = 3;
    sender.first_name = "Bob";

    Chat chat;
    chat.id = 100;
    chat.title = "Test Chat";

    Message msg;
    msg.id = 3;
    msg.chat_id = 100;
    msg.sender_id = 3;
    msg.timestamp = std::time(nullptr);
    msg.text = "Check this out";
    msg.is_outgoing = false;

    MediaInfo media;
    media.type = MediaType::PHOTO;
    msg.media = media;

    MessageInfo info{msg, sender, chat};
    std::string result = fmt::format("{}", info);

    EXPECT_TRUE(result.find("[photo]") != std::string::npos);
    EXPECT_TRUE(result.find("Check this out") != std::string::npos);
}

// Test formatting a vector of MessageInfo using fmt::join
TEST(FormattersTest, MessageInfoVector) {
    User sender1;
    sender1.id = 1;
    sender1.first_name = "Alice";

    User sender2;
    sender2.id = 2;
    sender2.first_name = "Bob";

    Chat chat;
    chat.id = 100;
    chat.title = "Test Chat";

    Message msg1;
    msg1.id = 1;
    msg1.chat_id = 100;
    msg1.sender_id = 1;
    msg1.timestamp = std::time(nullptr) - 3600;  // 1 hour ago
    msg1.text = "First message";
    msg1.is_outgoing = false;

    Message msg2;
    msg2.id = 2;
    msg2.chat_id = 100;
    msg2.sender_id = 2;
    msg2.timestamp = std::time(nullptr);
    msg2.text = "Second message";
    msg2.is_outgoing = true;

    std::vector<MessageInfo> infos = {{msg1, sender1, chat}, {msg2, sender2, chat}};

    std::string result = fmt::format("{}\n", fmt::join(infos, "\n"));

    EXPECT_TRUE(result.find("Alice") != std::string::npos);
    EXPECT_TRUE(result.find("First message") != std::string::npos);
    EXPECT_TRUE(result.find("**You**") != std::string::npos);
    EXPECT_TRUE(result.find("Second message") != std::string::npos);
}

// Test helper functions
TEST(FormattersTest, FormatTime) {
    // Use a known timestamp: 2024-01-15 14:30:00 UTC
    int64_t timestamp = 1705329000;
    std::string result = format_time(timestamp);

    // Should be in HH:MM format
    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(result[2], ':');
}

TEST(FormattersTest, FormatDatetime) {
    // Use a known timestamp
    int64_t timestamp = 1705329000;
    std::string result = format_datetime(timestamp);

    // Should be in YYYY-MM-DD HH:MM format
    EXPECT_EQ(result.size(), 16);
    EXPECT_EQ(result[4], '-');
    EXPECT_EQ(result[7], '-');
    EXPECT_EQ(result[10], ' ');
    EXPECT_EQ(result[13], ':');
}

}  // namespace
}  // namespace tg
