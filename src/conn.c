#include <http/http.h>
#include <http/conn.h>
#include <http/parse.h>
#include <http/route.h>
#include <http/response.h>

#include <stdio.h>
#include <string.h>

conn_ctx_t *alloc_conn(){
    pthread_mutex_lock(&pool_mutex);
    // checks if we have exhausted the pool
    if(conn_pool_next >= CONN_POOL_SIZE){
        // we fall back to dynamic allocation
        pthread_mutex_unlock(&pool_mutex);
        return calloc(1, sizeof(conn_ctx_t));
    }

    conn_ctx_t *ctx = &conn_pool[conn_pool_next++];
    pthread_mutex_unlock(&pool_mutex);

    if(!ctx->read_buf){
        ctx->read_buf = malloc(BUFFER_SIZE);
        ctx->write_buf = malloc(RESP_BUFFER_SIZE);
    }
    memset(&ctx->req, 0, sizeof(http_req_t));
    memset(&ctx->resp, 0, sizeof(http_resp_t));
    ctx->read_total = 0;
    ctx->write_total = 0;
    ctx->write_pos = 0;

    return ctx;
}

SIV handle_readable(conn_ctx_t *ctx){
    while(1){
        int space = BUFFER_SIZE - ctx->read_total;
        if(space <= 0){
            ctx->state = CONN_CLOSE;
            return;
        }
        int n = read(ctx->fd, ctx->read_buf + ctx->read_total, space);
        if(n < 0){
            if(errno == EAGAIN || errno == EWOULDBLOCK) break;
            ctx->state = CONN_CLOSE;
            return;
        }
        if(n==0){
            ctx->state = CONN_CLOSE;
            return;
        }
        ctx->read_total += n;

        int consumed = parse_http_req(ctx, &ctx->req);
        if(consumed >0){
            // printf("[LOG]: we wil now find the route in the base\n");
            route_req(&ctx->req, &ctx->resp);
            // printf("[DBG]: route=%.*s is_static=%d body_len=%zu body_ptr=%p\n",
    //    ctx->req.path_len,
    //    ctx->req.path,
    //    ctx->resp.is_static,
    //    ctx->resp.body_len,
    //    ctx->resp.body_ptr);

            // printf("[LOG]: now the response will be built\n");
            build_resp(ctx, &ctx->resp);

            if(consumed < ctx->read_total){
                memmove(ctx->read_buf, ctx->read_buf + consumed, ctx->read_total - consumed);
            }
            ctx->read_total -= consumed;
            ctx->state = CONN_WRITING;
            break;
        }
    }
}

SIV handle_writable(conn_ctx_t *ctx){
    while (ctx->write_pos < ctx->write_total) {
        int remaining = ctx->write_total - ctx->write_pos;
        int n = send(ctx->fd, ctx->write_buf + ctx->write_pos, remaining, MSG_NOSIGNAL);
        
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            ctx->state = CONN_CLOSE;
            return;
        }
        if (n == 0) {
            ctx->state = CONN_CLOSE;
            return;
        }
        
        ctx->write_pos += n;
    }
    
    if (ctx->write_pos >= ctx->write_total) {
        ctx->write_total = 0;
        ctx->write_pos = 0;
        ctx->state = CONN_READING;
    }
}