# Makefile — GlobalMetaManagement RPC
#
# 依赖:
#   Ubuntu/Debian: sudo apt install libtirpc-dev rpcbind g++ make

CXX      = g++
CC       = gcc
CPPFLAGS = -I/usr/include/tirpc
CXXFLAGS = -Wall -O2 -std=c++17
CFLAGS   = -Wall -O2
LIBS     = -ltirpc -lpthread

# ── GlobalMetaManagement (§5.12) ───────────────────────────────────
GMM_PROG        = global_meta
GMM_GENERATED   = $(GMM_PROG).h $(GMM_PROG)_xdr.c $(GMM_PROG)_svc.c $(GMM_PROG)_clnt.c
GMM_SERVER_OBJS = $(GMM_PROG)_svc.o $(GMM_PROG)_xdr.o \
                  global_meta_management.o global_meta_server.o
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
global_meta_management.o: $(GMM_PROG).h global_meta_management.h gmm_types.h
global_meta_server.o:     $(GMM_PROG).h global_meta_management.h gmm_types.h
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
