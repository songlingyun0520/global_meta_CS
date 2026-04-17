# Makefile — GlobalMetaManagement RPC
#
# 依赖:
#   Ubuntu/Debian: sudo apt install libtirpc-dev rpcbind g++ make

CXX      = g++
CC       = gcc
HIREDIS_CFLAGS := $(shell pkg-config --cflags hiredis)
HIREDIS_LIBS   := $(shell pkg-config --libs hiredis)
CPPFLAGS = -I/usr/include/tirpc -Imodule1/module1/include -Imodule1/common $(HIREDIS_CFLAGS)
CXXFLAGS = -Wall -O2 -std=c++17
CFLAGS   = -Wall -O2
LIBS     = -ltirpc -lpthread $(HIREDIS_LIBS)

# ── GlobalMetaManagement (§5.12) ───────────────────────────────────
GMM_PROG        = global_meta
GMM_GENERATED   = $(GMM_PROG).h $(GMM_PROG)_xdr.c $(GMM_PROG)_svc.c $(GMM_PROG)_clnt.c
GMM_SERVER_OBJS = $(GMM_PROG)_svc.o $(GMM_PROG)_xdr.o \
                  global_meta_server.o \
                  module1/common/metastore/redis_meta_store_backend.o \
                  module1/module1/src/redis_meta_store_global_adapter.o
GMM_CLIENT_OBJS = $(GMM_PROG)_clnt.o $(GMM_PROG)_xdr.o \
                  global_meta_client.o global_meta_client_test.o

.PHONY: all clean

all: gmm_server gmm_client_test

# ── rpcgen 代码生成 ─────────────────────────────────────────────────
$(GMM_GENERATED): $(GMM_PROG).x
	rpcgen -N $(GMM_PROG).x

# ── 编译规则 ────────────────────────────────────────────────────────
%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# ── 头文件依赖 ──────────────────────────────────────────────────────
$(GMM_PROG)_svc.o $(GMM_PROG)_clnt.o $(GMM_PROG)_xdr.o: $(GMM_PROG).h
global_meta_server.o:     $(GMM_PROG).h module1/module1/include/metastore/redis_meta_store_global_adapter.h module1/common/metastore/meta_store_types.h
module1/common/metastore/redis_meta_store_backend.o: module1/common/metastore/redis_meta_store_backend.h module1/common/metastore/meta_store_backend.h module1/common/metastore/meta_store_types.h module1/module1/include/metastore/status.h
module1/module1/src/redis_meta_store_global_adapter.o: module1/module1/include/metastore/redis_meta_store_global_adapter.h module1/common/metastore/redis_meta_store_backend.h
global_meta_client.o:     $(GMM_PROG).h global_meta_client.h gmm_types.h
global_meta_client_test.o: global_meta_client.h gmm_types.h

# ── 链接目标 ────────────────────────────────────────────────────────
gmm_server: $(GMM_GENERATED) $(GMM_SERVER_OBJS)
	$(CXX) -o $@ $(GMM_SERVER_OBJS) $(LIBS)

gmm_client_test: $(GMM_GENERATED) $(GMM_CLIENT_OBJS)
	$(CXX) -o $@ $(GMM_CLIENT_OBJS) $(LIBS)

clean:
	rm -f gmm_server gmm_client_test \
	      $(GMM_GENERATED) $(GMM_SERVER_OBJS) $(GMM_CLIENT_OBJS)
