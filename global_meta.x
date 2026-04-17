/*
 * global_meta.x — GlobalMetaManagement RPC 接口定义 (skills.md §5.12)
 *
 * 用法: rpcgen -N global_meta.x
 * 生成: global_meta.h  global_meta_xdr.c  global_meta_svc.c  global_meta_clnt.c
 *
 * 五条过程:
 *   1. gmm_register    — 注册 TokenKey+BlockKey → (token_ip, block_ip) 映射
 *   2. gmm_query       — 按 TokenKey 查询，返回 (token_ip, block_ip)
 *   3. gmm_remove      — 删除映射（请求结束或失效时）
 *   4. gmm_update      — 更新映射（节点迁移时）
 *   5. gmm_query_block — 按 BlockKey 查询，返回 (token_ip, block_ip)
 */

/* ── 常量 ──────────────────────────────────────────────────────────── */
const GMM_MAX_KEY_SZ  = 256;   /* TokenKey / BlockKey 最大字节数       */
const GMM_MAX_IP_SZ   = 64;    /* IPAddress 最大字节数                 */

/* ── 基础类型 ──────────────────────────────────────────────────────── */
typedef string GmmTokenKey<GMM_MAX_KEY_SZ>;   /* 全局唯一 token key     */
typedef string GmmBlockKey<GMM_MAX_KEY_SZ>;   /* 全局唯一 block key     */
typedef string GmmIPAddress<GMM_MAX_IP_SZ>;   /* 节点 IP 字符串         */

/* ── GmmIPPair：两种查询的统一响应载荷 ─────────────────────────────
 *   token_ip  — 持有该 token sequence 的 Prefill 节点地址
 *   block_ip  — 持有对应 KV-cache block 的 Decode/Cache 节点地址
 * ─────────────────────────────────────────────────────────────── */
struct GmmIPPair {
    GmmIPAddress token_ip;
    GmmIPAddress block_ip;
};

/* ══════════════════════════════════════════════════════════════════
 * gmm_register
 *   请求: TokenKey + BlockKey + token_ip + block_ip
 *   响应: status（0=成功, -1=失败）
 * ══════════════════════════════════════════════════════════════════ */
struct gmm_register_arg {
    GmmTokenKey  token_key;
    GmmBlockKey  block_key;
    GmmIPAddress token_ip;
    GmmIPAddress block_ip;
};

struct gmm_status_result {
    int status;   /* 0 = ok, -1 = error */
};

/* ══════════════════════════════════════════════════════════════════
 * gmm_query  （按 TokenKey 查询）
 *   请求: TokenKey
 *   响应: discriminated union（found=1 携带 GmmIPPair，found=0 未命中）
 * ══════════════════════════════════════════════════════════════════ */
struct gmm_query_arg {
    GmmTokenKey key;
};

union gmm_query_result switch (int found) {
case 1:
    GmmIPPair ips;
default:
    void;
};

/* ══════════════════════════════════════════════════════════════════
 * gmm_remove
 *   请求: TokenKey（主键）
 *   响应: status（0=成功, -1=不存在）
 * ══════════════════════════════════════════════════════════════════ */
struct gmm_remove_arg {
    GmmTokenKey key;
};

/* ══════════════════════════════════════════════════════════════════
 * gmm_update
 *   请求: TokenKey + 新 token_ip + 新 block_ip
 *         空字符串表示该 IP 不变，允许单独迁移任意一端
 *   响应: status（0=成功, -1=key 不存在）
 * ══════════════════════════════════════════════════════════════════ */
struct gmm_update_arg {
    GmmTokenKey  key;
    GmmIPAddress new_token_ip;   /* "" = 不更新 token 侧 IP */
    GmmIPAddress new_block_ip;   /* "" = 不更新 block 侧 IP */
};

/* ══════════════════════════════════════════════════════════════════
 * gmm_query_block  （按 BlockKey 查询）
 *   请求: BlockKey
 *   响应: 与 gmm_query 共用 gmm_query_result（found=1 携带 GmmIPPair）
 * ══════════════════════════════════════════════════════════════════ */
struct gmm_query_block_arg {
    GmmBlockKey key;
};

/* ══════════════════════════════════════════════════════════════════
 * RPC 程序定义
 *   程序号 0x20001235 位于用户自定义区间 0x20000000~0x3FFFFFFF
 *   与 HiIndexDRAM (0x20001234) 相邻，便于管理
 * ══════════════════════════════════════════════════════════════════ */
program GLOBALMETA_PROG {
    version GLOBALMETA_VERS {

        gmm_status_result gmm_register    (gmm_register_arg)    = 1;
        gmm_query_result  gmm_query       (gmm_query_arg)       = 2;
        gmm_status_result gmm_remove      (gmm_remove_arg)      = 3;
        gmm_status_result gmm_update      (gmm_update_arg)      = 4;
        gmm_query_result  gmm_query_block (gmm_query_block_arg) = 5;

    } = 1;
} = 0x20001235;
