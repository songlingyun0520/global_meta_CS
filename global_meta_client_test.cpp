/*
 * global_meta_client_test.cpp
 *
 * End-to-end demo for the GlobalMetaManagement RPC interface (skills.md §5.12).
 * Interface and output style mirrors demo.cpp / metastore::RedisMetaStoreGlobalAdapter.
 *
 * Build (after rpcgen -N global_meta.x):
 *   g++ -std=c++17 -Wall -O2 -I/usr/include/tirpc \
 *       global_meta_clnt.o global_meta_xdr.o \
 *       global_meta_client.o global_meta_client_test.o \
 *       -o gmm_client_test -ltirpc
 *
 * Usage: ./gmm_client_test <server_ip>
 */

#include "global_meta_client.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

void PrintStatus(const std::string &step, const Status &status) {
    std::cout << step << ": " << (status.ok() ? "OK" : "FAILED");
    if (!status.ok()) {
        std::cout << " (" << status.message() << ")";
    }
    std::cout << '\n';
}

void PrintBatchStatuses(const std::string &step,
                        const std::vector<Status> &statuses) {
    std::cout << step << ":\n";
    for (std::size_t i = 0; i < statuses.size(); ++i) {
        const auto &s = statuses[i];
        std::cout << "  [" << i << "] " << (s.ok() ? "OK" : "FAILED");
        if (!s.ok()) {
            std::cout << " (" << s.message() << ")";
        }
        std::cout << '\n';
    }
}

void PrintQueryResults(const std::string &step,
                       const std::vector<std::string> &keys,
                       const std::vector<Result<IpPair>> &results) {
    std::cout << step << ":\n";
    for (std::size_t i = 0; i < results.size(); ++i) {
        const auto &r = results[i];
        std::cout << "  key=" << keys[i] << " -> ";
        if (r.ok()) {
            std::cout << "token_ip=" << r.value.token_ip
                      << " block_ip=" << r.value.block_ip;
        } else {
            std::cout << "FAILED (" << r.status.message() << ")";
        }
        std::cout << '\n';
    }
}

}  // namespace

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <server_ip>\n";
        return 1;
    }

    CLIENT *clnt = clnt_create(argv[1], GLOBALMETA_PROG, GLOBALMETA_VERS, "tcp");
    if (clnt == nullptr) {
        clnt_pcreateerror(argv[1]);
        return 1;
    }

    GlobalMetaClient client(clnt);
    std::cout << "[GMM Client] Connected to " << argv[1]
              << " (prog=0x20001235)\n\n";

    // ── 场景 1: Prefill 阶段注册 ────────────────────────────────────────
    const std::vector<RegisterArgs> entries = {
        {"token:seq001", "block:blk001", "10.0.1.1", "10.1.1.1"},
        {"token:seq002", "block:blk002", "10.0.1.2", "10.1.1.2"},
        {"token:seq003", "block:blk003", "10.0.1.3", "10.1.1.3"},
    };
    PrintBatchStatuses("batchInsertGlobal", client.batchInsertGlobal(entries));

    // ── 场景 2: 按 TokenKey 查询 ─────────────────────────────────────────
    const std::vector<std::string> token_keys = {
        "token:seq001", "token:seq002", "token:seq999"
    };
    PrintQueryResults("batchQueryGlobal", token_keys,
                      client.batchQueryGlobal(token_keys));

    // ── 场景 3: 按 BlockKey 查询 ─────────────────────────────────────────
    const std::vector<std::string> block_keys = {
        "block:blk001", "block:blk003", "block:blk999"
    };
    PrintQueryResults("batchQueryBlockGlobal", block_keys,
                      client.batchQueryBlockGlobal(block_keys));

    // ── 场景 4: 节点迁移 ─────────────────────────────────────────────────
    const std::vector<UpdateArgs> updates = {
        {"token:seq002", "10.0.2.2", ""},
        {"token:seq003", "",         "10.1.2.3"},
        {"token:seq999", "10.0.9.9", "10.1.9.9"},
    };
    PrintBatchStatuses("batchUpdateGlobal", client.batchUpdateGlobal(updates));

    PrintQueryResults("batchQueryGlobal (after update)",
                      {"token:seq002", "token:seq003"},
                      client.batchQueryGlobal({"token:seq002", "token:seq003"}));

    // ── 场景 5: 删除映射 ─────────────────────────────────────────────────
    PrintBatchStatuses("batchDeleteGlobal",
                       client.batchDeleteGlobal({"token:seq001", "token:seq003", "token:seq999"}));

    const std::vector<std::string> all_keys = {
        "token:seq001", "token:seq002", "token:seq003"
    };
    PrintQueryResults("batchQueryGlobal (after delete)", all_keys,
                      client.batchQueryGlobal(all_keys));

    clnt_destroy(clnt);
    std::cout << "\n>> All GlobalMetaManagement RPC calls completed\n";
    return 0;
}
