/*
 * global_meta_client.cpp
 *
 * Implementation of GlobalMetaClient (declared in global_meta_client.h).
 */

#include "global_meta_client.h"

#include <iostream>
#include <string>

GlobalMetaClient::GlobalMetaClient(CLIENT *clnt) : clnt_(clnt) {}

// ── helpers ──────────────────────────────────────────────────────────────────

Status GlobalMetaClient::rpc_status(gmm_status_result *res,
                                     const char *rpc_name) const {
    if (res == nullptr) {
        return Status::error(std::string(rpc_name) + " RPC call failed: " +
                             clnt_sperror(clnt_, rpc_name));
    }
    return res->status == 0 ? Status{}
                             : Status::error(std::string(rpc_name) + " returned error");
}

static Result<IpPair> parse_query_result(gmm_query_result *res,
                                          CLIENT *clnt,
                                          const char *rpc_name) {
    if (res == nullptr) {
        return {{}, Status::error(std::string(rpc_name) + " RPC call failed: " +
                                  clnt_sperror(clnt, rpc_name))};
    }
    if (!res->found) {
        return {{}, Status::error("not found")};
    }
    return {IpPair{res->gmm_query_result_u.ips.token_ip,
                   res->gmm_query_result_u.ips.block_ip},
            Status{}};
}

// ── Single operations ─────────────────────────────────────────────────────────

Status GlobalMetaClient::register_(const RegisterArgs &args) const {
    gmm_register_arg arg;
    arg.token_key = const_cast<char *>(args.token_key.c_str());
    arg.block_key = const_cast<char *>(args.block_key.c_str());
    arg.token_ip  = const_cast<char *>(args.token_ip.c_str());
    arg.block_ip  = const_cast<char *>(args.block_ip.c_str());

    return rpc_status(gmm_register_1(arg, clnt_), "gmm_register_1");
}

Result<IpPair> GlobalMetaClient::query(const std::string &token_key) const {
    gmm_query_arg arg;
    arg.key = const_cast<char *>(token_key.c_str());

    return parse_query_result(gmm_query_1(arg, clnt_), clnt_, "gmm_query_1");
}

Result<IpPair> GlobalMetaClient::query_block(const std::string &block_key) const {
    gmm_query_block_arg arg;
    arg.key = const_cast<char *>(block_key.c_str());

    return parse_query_result(gmm_query_block_1(arg, clnt_), clnt_, "gmm_query_block_1");
}

Status GlobalMetaClient::update(const UpdateArgs &args) const {
    gmm_update_arg arg;
    arg.key          = const_cast<char *>(args.token_key.c_str());
    arg.new_token_ip = const_cast<char *>(args.new_token_ip.c_str());
    arg.new_block_ip = const_cast<char *>(args.new_block_ip.c_str());

    return rpc_status(gmm_update_1(arg, clnt_), "gmm_update_1");
}

Status GlobalMetaClient::remove(const std::string &token_key) const {
    gmm_remove_arg arg;
    arg.key = const_cast<char *>(token_key.c_str());

    return rpc_status(gmm_remove_1(arg, clnt_), "gmm_remove_1");
}

// ── Batch operations ──────────────────────────────────────────────────────────

std::vector<Status>
GlobalMetaClient::batchInsertGlobal(const std::vector<RegisterArgs> &entries) const {
    std::vector<Status> results;
    results.reserve(entries.size());
    for (const auto &e : entries) {
        results.push_back(register_(e));
    }
    return results;
}

std::vector<Result<IpPair>>
GlobalMetaClient::batchQueryGlobal(const std::vector<std::string> &token_keys) const {
    std::vector<Result<IpPair>> results;
    results.reserve(token_keys.size());
    for (const auto &k : token_keys) {
        results.push_back(query(k));
    }
    return results;
}

std::vector<Result<IpPair>>
GlobalMetaClient::batchQueryBlockGlobal(const std::vector<std::string> &block_keys) const {
    std::vector<Result<IpPair>> results;
    results.reserve(block_keys.size());
    for (const auto &k : block_keys) {
        results.push_back(query_block(k));
    }
    return results;
}

std::vector<Status>
GlobalMetaClient::batchUpdateGlobal(const std::vector<UpdateArgs> &entries) const {
    std::vector<Status> results;
    results.reserve(entries.size());
    for (const auto &e : entries) {
        results.push_back(update(e));
    }
    return results;
}

std::vector<Status>
GlobalMetaClient::batchDeleteGlobal(const std::vector<std::string> &token_keys) const {
    std::vector<Status> results;
    results.reserve(token_keys.size());
    for (const auto &k : token_keys) {
        results.push_back(remove(k));
    }
    return results;
}
