#include "tg/bustache_formatters.hpp"
#include "tg/types.hpp"

#include <gtest/gtest.h>

namespace tg {
namespace {

const User user{
    123,
    "johndoe",
    "John",
    "Doe",
    "1234567890",
    "I love tg-fuse",
    true,
    UserStatus::ONLINE,
    1234567890,
    1234567890
};

const Chat chat{123, ChatType::PRIVATE, "John Doe", "johndoe", 1234567890, 1234567890};
const Message no_media_message{123, 123, 123, 1234567890, "Hello, world!", std::nullopt, false};
const MediaInfo media_info{MediaType::PHOTO, "1234567890", "photo.jpg", "image/jpeg", 123, "photo.jpg", 800, 600};
const Message media_message{123, 123, 123, 1234567890, "Hello, world!", media_info, false};
const Message outgoing_message{123, 123, 123, 1234567890, "Hello, world!", std::nullopt, true};

const MessageInfo no_media_message_info{no_media_message, user, chat};
const MessageInfo media_message_info{media_message, user, chat};
const MessageInfo outgoing_message_info{outgoing_message, user, chat};

TEST(BustacheFormatTest, UserStatus) {
    bustache::format tmpl{"{{status}}"};
    EXPECT_EQ(to_string(tmpl(user)), "online");
}

TEST(BustacheFormatTest, NoMediaMessage) {
    bustache::format tmpl{"{{message}}"};
    EXPECT_EQ(to_string(tmpl(no_media_message_info)), "Hello, world!");
}

TEST(BustacheFormatTest, MediaMessage) {
    bustache::format tmpl{"{{message}}"};
    EXPECT_EQ(to_string(tmpl(media_message_info)), "[photo: photo.jpg] Hello, world!");
}

TEST(BustacheFormatTest, Sender) {
    bustache::format tmpl{"{{sender}}"};
    EXPECT_EQ(to_string(tmpl(no_media_message_info)), "John Doe (@johndoe)");
    EXPECT_EQ(to_string(tmpl(media_message_info)), "John Doe (@johndoe)");
    EXPECT_EQ(to_string(tmpl(outgoing_message_info)), "You");
}

TEST(BustacheFormatTest, Time) {
    bustache::format tmpl{"{{time}}"};
    EXPECT_EQ(to_string(tmpl(no_media_message_info)), "2009-02-13 23:31");
    EXPECT_EQ(to_string(tmpl(media_message_info)), "2009-02-13 23:31");
    EXPECT_EQ(to_string(tmpl(outgoing_message_info)), "2009-02-13 23:31");
}

TEST(BustacheFormatTest, Message) {
    {
        bustache::format tmpl{"> {{sender}}: {{message}}"};
        EXPECT_EQ(to_string(tmpl(no_media_message_info)), "> John Doe (@johndoe): Hello, world!");
        EXPECT_EQ(to_string(tmpl(media_message_info)), "> John Doe (@johndoe): [photo: photo.jpg] Hello, world!");
        EXPECT_EQ(to_string(tmpl(outgoing_message_info)), "> You: Hello, world!");
    }
    {
        bustache::format tmpl{"> **{{sender}}** [{{time}}]: {{message}}"};
        EXPECT_EQ(
            to_string(tmpl(no_media_message_info)), "> **John Doe (@johndoe)** [2009-02-13 23:31]: Hello, world!"
        );
        EXPECT_EQ(
            to_string(tmpl(media_message_info)),
            "> **John Doe (@johndoe)** [2009-02-13 23:31]: [photo: photo.jpg] Hello, world!"
        );
        EXPECT_EQ(to_string(tmpl(outgoing_message_info)), "> **You** [2009-02-13 23:31]: Hello, world!");
    }
}

}  // namespace
}  // namespace tg
