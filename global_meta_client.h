#pragma once

/*
 * global_meta_client.h
 *
 * C++ client wrapper for the GlobalMetaManagement RPC interface.
 * Follows the same Status / Result<T> / batch* pattern as
 * metastore::RedisMetaStoreGlobalAdapter (see demo.cpp).
 */

extern "C" {
#include "global_meta.h"
}

#include "gmm_types.h"

#include <string>
#include <vector>

class GlobalMetaClient {
public:
    explicit GlobalMetaClient(CLIENT *clnt);

    // ── Single operations ───────────────────────────────────────────
    Status         register_(const RegisterArgs &args) const;
    Result<IpPair> query(const std::string &token_key) const;
    Result<IpPair> query_block(const std::string &block_key) const;
    Status         update(const UpdateArgs &args) const;
    Status         remove(const std::string &token_key) const;

    // ── Batch operations ────────────────────────────────────────────
    std::vector<Status>         batchInsertGlobal(const std::vector<RegisterArgs> &entries) const;
    std::vector<Result<IpPair>> batchQueryGlobal(const std::vector<std::string> &token_keys) const;
    std::vector<Result<IpPair>> batchQueryBlockGlobal(const std::vector<std::string> &block_keys) const;
    std::vector<Status>         batchUpdateGlobal(const std::vector<UpdateArgs> &entries) const;
    std::vector<Status>         batchDeleteGlobal(const std::vector<std::string> &token_keys) const;

private:
    Status rpc_status(gmm_status_result *res, const char *rpc_name) const;

    CLIENT *clnt_;
};
