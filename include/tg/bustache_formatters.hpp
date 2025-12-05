#pragma once

#include <bustache/render/string.hpp>

#include "tg/formatters.hpp"
#include "tg/types.hpp"
#include "types.hpp"

namespace bustache {

template <>
struct impl_model<tg::UserStatus> {
    static constexpr bustache::model kind = bustache::model::atom;
};

template <>
struct impl_test<tg::UserStatus> {
    static bool test([[maybe_unused]] tg::UserStatus status) { return true; }
};

template <>
struct impl_print<tg::UserStatus> {
    static void print(tg::UserStatus status, bustache::output_handler os, [[maybe_unused]] char const* spec) {
        auto str = fmt::format("{}", status);
        os(str.c_str(), str.size());
    }
};

template <>
struct impl_model<tg::MediaType> {
    static constexpr bustache::model kind = bustache::model::atom;
};

template <>
struct impl_test<tg::MediaType> {
    static bool test([[maybe_unused]] tg::MediaType type) { return true; }
};

template <>
struct impl_print<tg::MediaType> {
    static void print(tg::MediaType type, bustache::output_handler os, [[maybe_unused]] char const* spec) {
        auto str = fmt::format("{}", type);
        os(str.c_str(), str.size());
    }
};

template <>
struct impl_model<tg::ChatType> {
    static constexpr bustache::model kind = bustache::model::atom;
};

template <>
struct impl_test<tg::ChatType> {
    static bool test([[maybe_unused]] tg::ChatType type) { return true; }
};

template <>
struct impl_print<tg::ChatType> {
    static void print(tg::ChatType type, bustache::output_handler os, [[maybe_unused]] char const* spec) {
        auto str = fmt::format("{}", type);
        os(str.c_str(), str.size());
    }
};

template <>
struct impl_model<tg::User> {
    static constexpr bustache::model kind = bustache::model::object;
};

// template <>
// struct impl_test<tg::User> {
//     static bool test([[maybe_unused]] tg::User user) { return true; }
// };

template <>
struct impl_object<tg::User> {
    static void get(tg::User user, std::string const& key, bustache::value_handler visit) {
        if (key == "display_name") {
            auto str = fmt::format("{:d}", user);
            return visit(&str);
        }
        if (key == "id") {
            return visit(&user.id);
        }
        if (key == "username") {
            return visit(&user.username);
        }
        if (key == "first_name") {
            return visit(&user.first_name);
        }
        if (key == "last_name") {
            return visit(&user.last_name);
        }
        if (key == "bio") {
            return visit(&user.bio);
        }
        if (key == "phone_number") {
            return visit(&user.phone_number);
        }
        if (key == "is_contact") {
            return visit(&user.is_contact);
        }
        if (key == "status") {
            return visit(&user.status);
        }
        // TODO: formatters for last_seen, last_message_timestamp
        if (key == "last_seen") {
            return visit(&user.last_seen);
        }
        if (key == "last_message_id") {
            return visit(&user.last_message_id);
        }
        if (key == "last_message_timestamp") {
            return visit(&user.last_message_timestamp);
        }
    }
};

template <>
struct impl_model<tg::MessageInfo> {
    static constexpr bustache::model kind = bustache::model::object;
};

template <>
struct impl_test<tg::MessageInfo> {
    static bool test(tg::MessageInfo info) { return !info.message.text.empty() || info.message.has_media(); }
};

template <>
struct impl_object<tg::MessageInfo> {
    static void get(tg::MessageInfo info, std::string const& key, bustache::value_handler visit) {
        if (key == "message") {
            // we don't want to expose the message object directly, so we'll use a formatter
            auto str = fmt::format("{:m}", info);
            return visit(&str);
        }
        if (key == "sender") {
            // we don't want to expose the sender object directly, so we'll use a formatter
            auto str = fmt::format("{:s}", info);
            return visit(&str);
        }
        if (key == "time") {
            // return formatted timestamp
            auto str = fmt::format("{:t}", info);
            return visit(&str);
        }
    }
};

}  // namespace bustache