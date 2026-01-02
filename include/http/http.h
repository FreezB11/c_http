#pragma once

// internal
#include <http/request.h>
#include <http/response.h>
#include <http/conn.h>
#include <http/utils.h>
// =========================
// #define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>
// #include <bits/socket.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <signal.h>
#include <netinet/tcp.h>
#include <sched.h>
#include <sys/mman.h>
// =========================
#define MAX_EVENTS 8192
#define BUFFER_SIZE 16384
#define RESP_BUFFER_SIZE 32768
#define THREAD_COUNT 4  // Increase for more cores
#define CONN_POOL_SIZE 16384

/*
conn_pool → a pre-allocated array of connection contexts (size CONN_POOL_SIZE).
conn_pool_next → the index of the next free slot in the pool.
pool_mutex → ensures thread safety when multiple threads allocate 
            connections simultaneously.
*/
// memory pool for connections
extern conn_ctx_t *conn_pool;
extern int conn_pool_next;
extern pthread_mutex_t pool_mutex;

typedef struct{
    // to be done soon
    int dummy; // placeholder
}http;

SIV setup_socket(int fd);
http* CreateServer();
SIV build_resp(conn_ctx_t *ctx, http_resp_t *res);

#define HTTP_STATS 0