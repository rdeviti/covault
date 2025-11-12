#pragma once
#include <hiredis/hiredis.h>
#include <cstdio>
#include <cstdlib>
#include <experimental/optional>
#include <string>
#include <vector>

struct ByteView {
   private:
    uint8_t const *m_ptr;
    size_t m_size;

   public:
    ByteView(std::string const &str)
        : m_ptr(reinterpret_cast<uint8_t const *>(str.data())),
          m_size(str.length()) {}
    ByteView(char const *ptr)
        : m_ptr(reinterpret_cast<uint8_t const *>(ptr)),
          m_size(std::strlen(ptr)) {}
    ByteView(uint8_t const *ptr, size_t size) : m_ptr(ptr), m_size(size) {}
    uint8_t const *data() const { return m_ptr; }
    size_t size() const { return m_size; }
};

struct RedisValue {
   private:
    redisReply *m_reply;

   public:
    RedisValue(redisReply *reply) : m_reply(reply) {}
    ~RedisValue() {
        if (m_reply != nullptr) {
            freeReplyObject(m_reply);
            m_reply = nullptr;
        }
    }

    RedisValue(RedisValue &val) = delete;
    RedisValue operator=(RedisValue &val) = delete;

    RedisValue(RedisValue &&val) {
        m_reply = val.m_reply;
        val.m_reply = nullptr;
    }
    RedisValue operator=(RedisValue &&val) {
        auto r = RedisValue(val.m_reply);
        val.m_reply = nullptr;
        return r;
    }

    // conversion to bool
    //
    operator bool() { return m_reply != nullptr; }

    uint8_t const *data() const {
        if (m_reply == nullptr) {
            return nullptr;
        }

        return reinterpret_cast<uint8_t *>(m_reply->str);
    }

    size_t size() const {
        if (m_reply == nullptr) {
            return 0;
        }

        return m_reply->len;
    }
};
class Redis {
   public:
    using val_t = std::vector<uint8_t>;
    Redis(std::string const &hostname, uint16_t port,
          std::string const &password) {
        m_ctx = redisConnect(hostname.c_str(), port);
        if (m_ctx == NULL || m_ctx->err) {
            if (m_ctx) {
                std::fprintf(stderr, "[redis]: error connecting to %s:%d: %s\n",
                             hostname.c_str(), port, m_ctx->errstr);
                redisFree(m_ctx);
                std::exit(-1);
            } else {
                std::fprintf(stderr,
                             "[redis]: failed to connect error connecting to "
                             "%s:%d (alloc failure)\n",
                             hostname.c_str(), port);
                std::exit(-1);
            }
        }

        if (redisEnableKeepAlive(m_ctx) == REDIS_ERR) {
            std::fprintf(stderr, "[redis]: failed to enable keepalive\n");
            redisFree(m_ctx);
            std::exit(-1);
        }

        redisReply *reply = static_cast<redisReply *>(
            redisCommand(m_ctx, "AUTH %s", password.c_str()));
        if (reply == nullptr) {
            std::fprintf(stderr, "[redis]: failed to authenticate\n");
            redisFree(m_ctx);
            std::exit(-1);
        }
        freeReplyObject(reply);
    }

    ~Redis() {
        redisFree(m_ctx);
        m_ctx = nullptr;
    }

    RedisValue get(ByteView key) {
        redisReply *reply = static_cast<redisReply *>(
            redisCommand(m_ctx, "GET %b", key.data(), key.size()));

        if (reply == nullptr) {
            handle_error();

            reply = static_cast<redisReply *>(
                redisCommand(m_ctx, "GET %b", key.data(), key.size()));
            if (reply == NULL) {
                std::fprintf(stderr, "[redis]: read failed (successively)\n");
                std::exit(-1);
            }
        }

        return RedisValue(reply);
    };

    bool set(ByteView key, uint8_t const *data, size_t size) {
        redisReply *reply = static_cast<redisReply *>(redisCommand(
            m_ctx, "SET %b %b", key.data(), key.size(), data, size));
        if (reply == nullptr && !try_handle_error()) {
            return false;
        }

        freeReplyObject(reply);
        return true;
    }

   private:
    redisContext *m_ctx = nullptr;

    bool try_handle_error() {
        if (m_ctx->err == 1 && redisReconnect(m_ctx) == REDIS_OK) {
            return true;
        }

        if (m_ctx->err) {
            std::fprintf(stderr, "[redis]: irrecoverable error (%d): %s\n",
                         m_ctx->err, m_ctx->errstr);
            return false;
        }

        return true;
    }
    void handle_error() {
        if (!try_handle_error()) {
            std::exit(-1);
        }
    }
};
