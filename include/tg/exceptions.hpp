#pragma once

#include <exception>
#include <string>

namespace tg {

// Base exception for all Telegram-related errors
class TelegramException : public std::exception {
public:
    explicit TelegramException(const std::string& message) : message_(message) {}

    const char* what() const noexcept override { return message_.c_str(); }

    const std::string& message() const { return message_; }

private:
    std::string message_;
};

// Authentication-related exceptions
class AuthenticationException : public TelegramException {
public:
    explicit AuthenticationException(const std::string& message) : TelegramException(message) {}
};

class InvalidPhoneException : public AuthenticationException {
public:
    explicit InvalidPhoneException(const std::string& phone)
        : AuthenticationException("Invalid phone number: " + phone) {}
};

class InvalidCodeException : public AuthenticationException {
public:
    InvalidCodeException() : AuthenticationException("Invalid authentication code") {}
};

class InvalidPasswordException : public AuthenticationException {
public:
    InvalidPasswordException() : AuthenticationException("Invalid 2FA password") {}
};

// Network-related exceptions
class NetworkException : public TelegramException {
public:
    explicit NetworkException(const std::string& message) : TelegramException(message) {}
};

class ConnectionException : public NetworkException {
public:
    ConnectionException() : NetworkException("Failed to connect to Telegram servers") {}
};

class TimeoutException : public NetworkException {
public:
    explicit TimeoutException(const std::string& operation = "")
        : NetworkException("Operation timed out" + (operation.empty() ? "" : ": " + operation)) {}
};

// Entity-related exceptions
class EntityException : public TelegramException {
public:
    explicit EntityException(const std::string& message) : TelegramException(message) {}
};

class ChatNotFoundException : public EntityException {
public:
    explicit ChatNotFoundException(int64_t chat_id) : EntityException("Chat not found: " + std::to_string(chat_id)) {}

    explicit ChatNotFoundException(const std::string& username) : EntityException("Chat not found: " + username) {}
};

class UserNotFoundException : public EntityException {
public:
    explicit UserNotFoundException(int64_t user_id) : EntityException("User not found: " + std::to_string(user_id)) {}

    explicit UserNotFoundException(const std::string& username) : EntityException("User not found: " + username) {}
};

class MessageNotFoundException : public EntityException {
public:
    explicit MessageNotFoundException(int64_t message_id)
        : EntityException("Message not found: " + std::to_string(message_id)) {}
};

// File-related exceptions
class FileException : public TelegramException {
public:
    explicit FileException(const std::string& message) : TelegramException(message) {}
};

class FileNotFoundException : public FileException {
public:
    explicit FileNotFoundException(const std::string& file_id) : FileException("File not found: " + file_id) {}
};

class FileDownloadException : public FileException {
public:
    explicit FileDownloadException(const std::string& file_id) : FileException("Failed to download file: " + file_id) {}
};

class FileUploadException : public FileException {
public:
    explicit FileUploadException(const std::string& path) : FileException("Failed to upload file: " + path) {}
};

// Operation-related exceptions
class OperationException : public TelegramException {
public:
    explicit OperationException(const std::string& message) : TelegramException(message) {}
};

class PermissionDeniedException : public OperationException {
public:
    explicit PermissionDeniedException(const std::string& operation)
        : OperationException("Permission denied: " + operation) {}
};

class RateLimitException : public OperationException {
public:
    explicit RateLimitException(int retry_after_seconds = 0)
        : OperationException(
              "Rate limit exceeded" +
              (retry_after_seconds > 0 ? ", retry after " + std::to_string(retry_after_seconds) + " seconds" : "")
          ) {}
};

// Cache-related exceptions
class CacheException : public TelegramException {
public:
    explicit CacheException(const std::string& message) : TelegramException(message) {}
};

class DatabaseException : public CacheException {
public:
    explicit DatabaseException(const std::string& message) : CacheException("Database error: " + message) {}
};

// TDLib-specific exceptions
class TdLibException : public TelegramException {
public:
    TdLibException(int code, const std::string& message)
        : TelegramException("TDLib error [" + std::to_string(code) + "]: " + message), code_(code) {}

    int code() const { return code_; }

private:
    int code_;
};

}  // namespace tg
