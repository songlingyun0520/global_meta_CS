#pragma once

/*
 * global_meta_management.h
 *
 * In-process implementation of GlobalMetaManagement (skills.md §5.12).
 * Thread safety: all public methods are protected by a mutex (§7.1).
 */

extern "C" {
#include "global_meta.h"
}

#include "gmm_types.h"

#include <mutex>
#include <string>
#include <unordered_map>

class GlobalMetaManagement {
public:
    Status         register_(const std::string &token_key, const std::string &block_key,
                              const std::string &token_ip,  const std::string &block_ip);

    Result<IpPair> query(const std::string &token_key) const;
    Result<IpPair> query_block(const std::string &block_key) const;

    Status         remove(const std::string &token_key);
    Status         update(const std::string &token_key,
                          const std::string &new_token_ip,
                          const std::string &new_block_ip);

private:
    struct Entry {
        std::string block_key;
        std::string token_ip;
        std::string block_ip;
    };

    std::unordered_map<std::string, Entry>       global_map_;
    std::unordered_map<std::string, std::string> block_index_;
    mutable std::mutex                           mutex_;
};
