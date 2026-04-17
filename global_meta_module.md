# GlobalMetaManagement 模块文档

## 目录

- [模块概述](#模块概述)
- [文件结构](#文件结构)
- [模块调用关系](#模块调用关系)
- [数据模型](#数据模型)
- [RPC 接口定义](#rpc-接口定义)
- [Server 实现](#server-实现)
- [Client 封装](#client-封装)
- [测试用例](#测试用例)
- [构建说明](#构建说明)
- [线程安全](#线程安全)

---

## 模块概述

GlobalMetaManagement（GMM）是一个基于 ONC RPC（TI-RPC）的全局元数据服务，维护一张
**TokenKey / BlockKey → (token\_ip, block\_ip)** 的双向映射表。

典型使用场景：

- **Prefill 阶段**：节点逐步向 GMM 注册自身持有的 token sequence 和 KV-cache block 的位置信息。
- **查询路由**：当 `FullSeqMetaCache` 本地未命中时，调用 GMM 的 `query` / `query_block` 作为回退路径，获取目标节点的 IP 地址。
- **节点迁移**：通过 `update` 独立更新 Prefill 侧或 Block 侧 IP，无需重新注册整条记录。
- **请求结束**：调用 `remove` 清理映射，释放元数据空间。

程序号 `0x20001235`，位于用户自定义区间（`0x20000000`–`0x3FFFFFFF`），与 HiIndexDRAM（`0x20001234`）相邻。

---

## 文件结构

手写文件：

```
gmm_types.h                    共享类型：Status / Result<T> / IpPair / RegisterArgs / UpdateArgs
global_meta.x                  RPC 接口描述（XDR / rpcgen 源文件）
global_meta_management.h       服务端核心类声明
global_meta_management.cpp     服务端核心类实现
global_meta_server.cpp         rpcgen _svc 分发函数实现
global_meta_client.h           客户端封装类声明
global_meta_client.cpp         客户端封装类实现
global_meta_client_test.cpp    端到端测试驱动（含 main）
```

rpcgen 由 `global_meta.x` 生成以下文件（不纳入版本控制）：

```
global_meta.h        C 类型定义 / 宏
global_meta_xdr.c    XDR 序列化 / 反序列化
global_meta_svc.c    服务端 RPC 分发框架
global_meta_clnt.c   客户端 RPC 桩函数
```

---

## 模块调用关系

### 整体架构

```
┌─────────────────────────────┐        ┌──────────────────────────────────┐
│         Client Side         │        │           Server Side             │
│                             │        │                                  │
│  global_meta_client_test    │        │                                  │
│           │                 │        │   GlobalMetaManagement           │
│           │ 创建/调用        │        │   (management.h / .cpp)          │
│           ▼                 │        │            ▲                     │
│  GlobalMetaClient           │        │            │ 调用                │
│  (client.h / client.cpp)    │        │   gmm_xxx_1_svc()                │
│           │                 │        │   (global_meta_server.cpp)       │
│           │ 调用 RPC 桩      │        │            ▲                     │
│           ▼                 │        │            │ 分发                │
│   global_meta_clnt.c        │        │   global_meta_svc.c              │
│   (rpcgen 生成)             │        │   (rpcgen 生成)                  │
│           │                 │        │            ▲                     │
└───────────┼─────────────────┘        └────────────┼─────────────────────┘
            │                                        │
            └──────────── TCP (RPC over TI-RPC) ─────┘
```

### 头文件依赖关系

```
gmm_types.h  ◄──────────────────────────────────────────────┐
     ▲                                                       │
     │ #include                                             │ #include
     │                                                       │
global_meta_management.h          global_meta_client.h ─────┘
     ▲                                    ▲
     │ #include                           │ #include
     │                                    │
global_meta_server.cpp       global_meta_client.cpp
                                          ▲
                                          │ #include
                                          │
                             global_meta_client_test.cpp


global_meta.h (rpcgen 生成)
     ▲
     │ extern "C" { #include }
     ├── global_meta_management.h
     └── global_meta_client.h
```

### 单次操作调用链（以 query 为例）

```
global_meta_client_test.cpp
  client.batchQueryGlobal({"token:seq001"})
    └─► GlobalMetaClient::batchQueryGlobal()          [global_meta_client.cpp]
          └─► GlobalMetaClient::query("token:seq001") [global_meta_client.cpp]
                └─► gmm_query_1(arg, clnt_)            [global_meta_clnt.c, rpcgen]
                      └─► [TCP 网络传输]
                            └─► gmm_query_1_svc()      [global_meta_server.cpp]
                                  └─► GlobalMetaManagement::query() [global_meta_management.cpp]
                                        └─► global_map_.find()
                                              └─► Result<IpPair>
                                  ◄── 序列化为 gmm_query_result
                      ◄── XDR 反序列化
                └─► parse_query_result()               [global_meta_client.cpp]
          └─► Result<IpPair>
    └─► vector<Result<IpPair>>
```

### 类型在两侧的共享

`gmm_types.h` 是客户端和服务端共用的类型层，两侧均通过它交换结果，
不直接暴露 rpcgen 生成的 C 结构体：

```
                      gmm_types.h
                    ┌─────────────┐
                    │  Status     │
                    │  Result<T>  │
                    │  IpPair     │
                    │ RegisterArgs│
                    │  UpdateArgs │
                    └──────┬──────┘
                           │
              ┌────────────┴────────────┐
              │                         │
   GlobalMetaManagement          GlobalMetaClient
   (服务端核心逻辑)                (客户端 RPC 封装)
```

---

## 数据模型

### 常量

| 常量 | 值 | 含义 |
|---|---|---|
| `GMM_MAX_KEY_SZ` | 256 | TokenKey / BlockKey 最大字节数 |
| `GMM_MAX_IP_SZ` | 64 | IP 地址字符串最大字节数 |

### 核心映射

GMM 内部维护两张哈希表，均保证 O(1) 平均复杂度：

```
global_map_   : TokenKey  → Entry { block_key, token_ip, block_ip }
block_index_  : BlockKey  → TokenKey
```

`block_index_` 是反向索引，支持 `query_block` 按 BlockKey 路由，返回的结果与 `query` 相同（同一条 Entry 中的两个 IP）。

注册时若 TokenKey 已存在，**覆盖写入**（幂等），同时更新旧 BlockKey 的反向索引。

### IpPair

两种查询的统一响应载荷：

| 字段 | 类型 | 含义 |
|---|---|---|
| `token_ip` | `std::string` | 持有该 token sequence 的 Prefill 节点 IP |
| `block_ip` | `std::string` | 持有对应 KV-cache block 的 Decode/Cache 节点 IP |

---

## RPC 接口定义

程序定义（`global_meta.x`）：

```c
program GLOBALMETA_PROG {
    version GLOBALMETA_VERS {
        gmm_status_result gmm_register    (gmm_register_arg)    = 1;
        gmm_query_result  gmm_query       (gmm_query_arg)       = 2;
        gmm_status_result gmm_remove      (gmm_remove_arg)      = 3;
        gmm_status_result gmm_update      (gmm_update_arg)      = 4;
        gmm_query_result  gmm_query_block (gmm_query_block_arg) = 5;
    } = 1;
} = 0x20001235;
```

### 过程 1：gmm\_register

注册一条完整的映射记录，Prefill 阶段调用。

**请求**

```c
struct gmm_register_arg {
    GmmTokenKey  token_key;   // 全局唯一，主键
    GmmBlockKey  block_key;   // 与 token_key 绑定的 block 主键
    GmmIPAddress token_ip;    // Prefill 节点 IP
    GmmIPAddress block_ip;    // Decode/Cache 节点 IP
};
```

**响应**

```c
struct gmm_status_result { int status; };  // 0=成功
```

覆盖写入（幂等），旧 BlockKey 的反向索引随之更新。

---

### 过程 2：gmm\_query

按 **TokenKey** 查询，返回对应的 (token\_ip, block\_ip)。

**请求**

```c
struct gmm_query_arg { GmmTokenKey key; };
```

**响应**（discriminated union）

```c
union gmm_query_result switch (int found) {
case 1:  GmmIPPair ips;   // 命中
default: void;            // 未命中
};
```

---

### 过程 3：gmm\_remove

按 **TokenKey** 删除映射，同步清理 BlockKey 反向索引。

**请求**

```c
struct gmm_remove_arg { GmmTokenKey key; };
```

**响应**：`status` 0=成功，-1=key 不存在。

---

### 过程 4：gmm\_update

节点迁移：更新已有记录的 IP 地址，允许单独更新任意一侧。

**请求**

```c
struct gmm_update_arg {
    GmmTokenKey  key;
    GmmIPAddress new_token_ip;  // "" 表示该侧 IP 不变
    GmmIPAddress new_block_ip;  // "" 表示该侧 IP 不变
};
```

**响应**：`status` 0=成功，-1=key 不存在。

---

### 过程 5：gmm\_query\_block

按 **BlockKey** 查询，通过反向索引定位 TokenKey，再查主表，返回与 `gmm_query` 相同的 `gmm_query_result`。

**请求**

```c
struct gmm_query_block_arg { GmmBlockKey key; };
```

---

## Server 实现

### GlobalMetaManagement 类

声明在 `global_meta_management.h`，实现在 `global_meta_management.cpp`。
所有公有方法均持有互斥锁，线程安全。

| 方法 | 返回值 | 说明 |
|---|---|---|
| `register_(token_key, block_key, token_ip, block_ip)` | `Status` | 覆盖旧条目时同步更新反向索引 |
| `query(token_key)` | `Result<IpPair>` | 按 TokenKey 查；未命中返回 `Status::error("not found")` |
| `query_block(block_key)` | `Result<IpPair>` | 按 BlockKey 查；经反向索引转查主表 |
| `remove(token_key)` | `Status` | 同步清理反向索引；key 不存在返回 error |
| `update(token_key, new_token_ip, new_block_ip)` | `Status` | 空串表示该侧不变；key 不存在返回 error |

### _svc 分发函数（`global_meta_server.cpp`）

每个 `gmm_xxx_1_svc` 函数：

1. 调用 `GlobalMetaManagement` 的对应方法，得到 `Status` / `Result<IpPair>`。
2. 将结果翻译为 rpcgen 要求的 C 结构体（`gmm_status_result` / `gmm_query_result`），query 类结果写入 `static std::string` 再令 `char *` 字段指向它（满足指针生命周期要求）。
3. 打印带时间戳的操作日志；未命中时输出 `status.message()`。

时间戳由 `ts()` 生成，返回 `std::string`，格式为 `HH:MM:SS`。

服务启动日志通过 `__attribute__((constructor))` 在 `main()` 执行前打印。

---

## Client 封装

### GlobalMetaClient 类

声明在 `global_meta_client.h`，实现在 `global_meta_client.cpp`。
持有一个 `CLIENT *` 句柄，将 RPC 调用包装为类型安全的 C++ 接口，风格对齐
`metastore::RedisMetaStoreGlobalAdapter`（见 `demo.cpp`）。

#### 单操作接口

```cpp
Status         register_(const RegisterArgs &args) const;
Result<IpPair> query(const std::string &token_key) const;
Result<IpPair> query_block(const std::string &block_key) const;
Status         update(const UpdateArgs &args) const;
Status         remove(const std::string &token_key) const;
```

#### 批量操作接口

```cpp
vector<Status>         batchInsertGlobal(const vector<RegisterArgs> &entries) const;
vector<Result<IpPair>> batchQueryGlobal(const vector<string> &token_keys) const;
vector<Result<IpPair>> batchQueryBlockGlobal(const vector<string> &block_keys) const;
vector<Status>         batchUpdateGlobal(const vector<UpdateArgs> &entries) const;
vector<Status>         batchDeleteGlobal(const vector<string> &token_keys) const;
```

批量操作在客户端循环调用对应的单操作，每条记录独立返回 `Status` / `Result`。

**错误处理**：RPC 桩返回 `nullptr` 时，`rpc_status()` / `parse_query_result()` 将
`clnt_sperror()` 的描述包装为 `Status::error(message)`，不抛异常。

> `const_cast<char *>(str.c_str())` 用于向 rpcgen 生成的 `char *` 字段传参，是与 C RPC 接口交互的必要妥协。

---

## 测试用例

`global_meta_client_test.cpp` 使用与 `demo.cpp` 相同的输出辅助函数
（`PrintStatus` / `PrintBatchStatuses` / `PrintQueryResults`），依次执行五个场景：

| 场景 | 批量操作 | 验证点 |
|---|---|---|
| 1 | `batchInsertGlobal` × 3 条 | 三条均返回 OK |
| 2 | `batchQueryGlobal` by TokenKey | 两条命中；`seq999` 返回 FAILED(not found) |
| 3 | `batchQueryBlockGlobal` by BlockKey | 两条命中；`blk999` 返回 FAILED(not found) |
| 4 | `batchUpdateGlobal`（单侧/单侧/不存在） | 迁移后再查返回新 IP；不存在的 key 返回 FAILED |
| 5 | `batchDeleteGlobal` + 再查 | 已删除返回 FAILED；未删除的 `seq002` 仍可查 |

---

## 构建说明

### 第一步：生成 RPC 桩代码

```bash
rpcgen -N global_meta.x
# 生成: global_meta.h  global_meta_xdr.c  global_meta_svc.c  global_meta_clnt.c
```

### 第二步：编译 Server

```bash
g++ -std=c++17 -Wall -O2 -I/usr/include/tirpc \
    global_meta_svc.c global_meta_xdr.c \
    global_meta_management.cpp global_meta_server.cpp \
    -o gmm_server -ltirpc -lpthread
```

### 第三步：编译 Client 测试

```bash
g++ -std=c++17 -Wall -O2 -I/usr/include/tirpc \
    global_meta_clnt.c global_meta_xdr.c \
    global_meta_client.cpp global_meta_client_test.cpp \
    -o gmm_client_test -ltirpc
```

### 第四步：运行

```bash
# 终端 1：启动 Server
./gmm_server

# 终端 2：运行测试（替换为实际 Server IP）
./gmm_client_test 127.0.0.1
```

---

## 线程安全

`GlobalMetaManagement` 的所有公有方法均通过 `std::lock_guard<std::mutex>` 保护，满足
§7.1 并发写入要求。`query` / `query_block` 将 `mutex_` 声明为 `mutable`，保持 `const` 语义。

`_svc` 分发函数中的 `static` 变量（用于 RPC 返回指针的生命周期）**不是**线程安全的——TI-RPC
默认单线程分发，若启用多线程模式（`svc_run` + `pthread`），需为每个过程的静态变量增加独立锁保护。
