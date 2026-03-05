#include <http/http.h>
#include <http/conn.h>
#include <http/parse.h>
#include <http/route.h>
#include <http/response.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ─────────────────────────────────────────────────────────────────────────
   Connection pool with a free-list for O(1) alloc and free.
   ───────────────────────────────────────────────────────────────────────── */

static int  free_list[CONN_POOL_SIZE];
static int  free_top = 0;          /* number of entries in the free list   */
static int  initialized = 0;

static void init_free_list(void) {
    for (int i = 0; i < CONN_POOL_SIZE; i++)
        free_list[i] = i;
    free_top = CONN_POOL_SIZE;
    initialized = 1;
}

conn_ctx_t *alloc_conn(void) {
    pthread_mutex_lock(&pool_mutex);
    if (!initialized) init_free_list();

    conn_ctx_t *ctx;
    if (free_top > 0) {
        int idx = free_list[--free_top];
        ctx = &conn_pool[idx];
        ctx->pool_index = idx;
        pthread_mutex_unlock(&pool_mutex);
    } else {
        pthread_mutex_unlock(&pool_mutex);
        /* Pool exhausted — fall back to heap */
        ctx = (conn_ctx_t *)calloc(1, sizeof(conn_ctx_t));
        if (!ctx) return NULL;
        ctx->pool_index = -1;
    }

    /* Lazily allocate I/O buffers */
    if (!ctx->read_buf) {
        ctx->read_buf  = (char *)malloc(BUFFER_SIZE);
        ctx->write_buf = (char *)malloc(RESP_BUFFER_SIZE);
    }

    memset(&ctx->req,  0, sizeof(http_req_t));
    memset(&ctx->resp, 0, sizeof(http_resp_t));
    ctx->read_total  = 0;
    ctx->write_total = 0;
    ctx->write_pos   = 0;
    ctx->state       = CONN_READING;

    return ctx;
}

void free_conn(conn_ctx_t *ctx) {
    if (!ctx) return;
    if (ctx->pool_index >= 0) {
        pthread_mutex_lock(&pool_mutex);
        free_list[free_top++] = ctx->pool_index;
        pthread_mutex_unlock(&pool_mutex);
    } else {
        /* Heap-allocated fallback — free buffers + struct */
        free(ctx->read_buf);
        free(ctx->write_buf);
        free(ctx);
    }
}

/* ─────────────────────────────────────────────────────────────────────────
   I/O handlers
   ───────────────────────────────────────────────────────────────────────── */

void handle_readable(conn_ctx_t *ctx) {
    while (1) {
        int space = BUFFER_SIZE - ctx->read_total;
        if (space <= 0) { ctx->state = CONN_CLOSE; return; }

        int n = (int)read(ctx->fd, ctx->read_buf + ctx->read_total, space);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            ctx->state = CONN_CLOSE;
            return;
        }
        if (n == 0) { ctx->state = CONN_CLOSE; return; }
        ctx->read_total += n;

        int consumed = parse_http_req(ctx, &ctx->req);
        if (consumed > 0) {
            route_req(&ctx->req, &ctx->resp);
            build_resp(ctx, &ctx->resp);

            if (consumed < ctx->read_total)
                memmove(ctx->read_buf, ctx->read_buf + consumed,
                        ctx->read_total - consumed);
            ctx->read_total -= consumed;
            ctx->state = CONN_WRITING;
            break;
        }
    }
}

void handle_writable(conn_ctx_t *ctx) {
    while (ctx->write_pos < ctx->write_total) {
        int remaining = ctx->write_total - ctx->write_pos;
        int n = (int)send(ctx->fd, ctx->write_buf + ctx->write_pos,
                          remaining, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            ctx->state = CONN_CLOSE;
            return;
        }
        if (n == 0) { ctx->state = CONN_CLOSE; return; }
        ctx->write_pos += n;
    }

    if (ctx->write_pos >= ctx->write_total) {
        ctx->write_total = 0;
        ctx->write_pos   = 0;
        ctx->state       = CONN_READING;
    }
}