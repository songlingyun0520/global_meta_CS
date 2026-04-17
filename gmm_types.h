#pragma once

/*
 * gmm_types.h
 *
 * Shared types for the GlobalMetaManagement module:
 *   Status        — operation result (ok / error + message)
 *   Result<T>     — value + Status pair, mirrors metastore::Result
 *   IpPair        — (token_ip, block_ip) query payload
 *   RegisterArgs  — arguments for a single register call
 *   UpdateArgs    — arguments for a single update call
 */

#include <string>

// ---------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------
class Status {
public:
    Status() : ok_(true) {}          // 默认构造 = 成功

    static Status error(const std::string &msg) {
        Status s;
        s.ok_      = false;
        s.message_ = msg;
        return s;
    }

    bool               ok()      const { return ok_; }
    const std::string &message() const { return message_; }

private:
    bool        ok_      = true;
    std::string message_;
};

// ---------------------------------------------------------------------------
// Result<T>
// ---------------------------------------------------------------------------
template <typename T>
struct Result {
    bool ok() const { return status.ok(); }
    T      value{};
    Status status = Status::error("not set");
};

// ---------------------------------------------------------------------------
// IpPair — unified query payload for both query modes
// ---------------------------------------------------------------------------
struct IpPair {
    std::string token_ip;
    std::string block_ip;
};

// ---------------------------------------------------------------------------
// RegisterArgs — one entry for batchInsertGlobal
// ---------------------------------------------------------------------------
struct RegisterArgs {
    std::string token_key;
    std::string block_key;
    std::string token_ip;
    std::string block_ip;
};

// ---------------------------------------------------------------------------
// UpdateArgs — one entry for batchUpdateGlobal
// ---------------------------------------------------------------------------
struct UpdateArgs {
    std::string token_key;
    std::string new_token_ip;   // "" = keep existing
    std::string new_block_ip;   // "" = keep existing
};
