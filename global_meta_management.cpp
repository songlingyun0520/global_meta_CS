/*
 * global_meta_management.cpp
 *
 * Implementation of GlobalMetaManagement (declared in global_meta_management.h).
 */

#include "global_meta_management.h"

Status GlobalMetaManagement::register_(const std::string &token_key,
                                        const std::string &block_key,
                                        const std::string &token_ip,
                                        const std::string &block_ip) {
    const std::lock_guard<std::mutex> guard(mutex_);
    const auto old = global_map_.find(token_key);
    if (old != global_map_.end()) {
        block_index_.erase(old->second.block_key);
    }
    global_map_[token_key] = {block_key, token_ip, block_ip};
    block_index_[block_key] = token_key;
    return Status{};
}

Result<IpPair> GlobalMetaManagement::query(const std::string &token_key) const {
    const std::lock_guard<std::mutex> guard(mutex_);
    const auto it = global_map_.find(token_key);
    if (it == global_map_.end()) {
        return {{}, Status::error("not found")};
    }
    return {IpPair{it->second.token_ip, it->second.block_ip}, Status{}};
}

Result<IpPair> GlobalMetaManagement::query_block(const std::string &block_key) const {
    const std::lock_guard<std::mutex> guard(mutex_);
    const auto idx = block_index_.find(block_key);
    if (idx == block_index_.end()) {
        return {{}, Status::error("not found")};
    }
    const auto it = global_map_.find(idx->second);
    if (it == global_map_.end()) {
        return {{}, Status::error("not found")};
    }
    return {IpPair{it->second.token_ip, it->second.block_ip}, Status{}};
}

Status GlobalMetaManagement::remove(const std::string &token_key) {
    const std::lock_guard<std::mutex> guard(mutex_);
    const auto it = global_map_.find(token_key);
    if (it == global_map_.end()) {
        return Status::error("not found");
    }
    block_index_.erase(it->second.block_key);
    global_map_.erase(it);
    return Status{};
}

Status GlobalMetaManagement::update(const std::string &token_key,
                                     const std::string &new_token_ip,
                                     const std::string &new_block_ip) {
    const std::lock_guard<std::mutex> guard(mutex_);
    const auto it = global_map_.find(token_key);
    if (it == global_map_.end()) {
        return Status::error("not found");
    }
    if (!new_token_ip.empty()) it->second.token_ip = new_token_ip;
    if (!new_block_ip.empty()) it->second.block_ip = new_block_ip;
    return Status{};
}
