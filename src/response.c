#include <http/http.h>
#include <http/conn.h>
#include <http/utils.h>

#include <stdio.h>
#include <string.h>

// SIV build_resp(conn_ctx_t *ctx, http_resp_t *res){
//     // if static responce, just copy
//     if(res->is_static){
//         memcpy(ctx->write_buf, res->body_ptr, res->body_len);
//         ctx->write_total = res->body_len;
//         ctx->write_pos = 0;
//         return ;
//     }

//     // dynamic response for /echo?key=value
//     char *buf = ctx->write_buf;
//     int pos = 0;

//     // build response for echo
//     pos += sprintf(buf + pos, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n");

//     // we calc the body len
//     int body_s = pos + 100;
//     int body_p = body_s;

//     body_p += sprintf(buf + body_p, "{");
//     for(int i =0; i < ctx->req.param_count; i++){
//         if(i >0) body_p += sprintf(buf + body_p, ",");
//         body_p += sprintf(buf + body_p, "\"%.*s\":\"%.*s\"",
//                             ctx->req.params[i].key_len, ctx->req.params[i].key,
//                             ctx->req.params[i].val_len, ctx->req.params[i].val);
//     }
//     body_p += sprintf(buf + body_p, "}");

//     int body_len = body_p - body_s;

//     // now writing the header
//     pos += sprintf(buf + pos, "Content-Length: %d\r\n", body_len);
//     pos += sprintf(buf + pos, "Connection: keep-alive\r\nCache-Control: no-cache\r\n\r\n");

//     // moving body to the correct position
//     memmove(buf + pos, buf + body_s, body_len);

//     ctx->write_total = pos + body_len;
//     ctx->write_pos = 0;
// }
SIV build_resp(conn_ctx_t *ctx, http_resp_t *res){
    char *buf = ctx->write_buf;
    int pos = 0;

    // Status line
    pos += snprintf(buf + pos, RESP_BUFFER_SIZE - pos,
        "HTTP/1.1 %d %s\r\n",
        res->status,
        res->status == 200 ? "OK" :
        res->status == 404 ? "Not Found" : "Error"
    );

    // Headers
    pos += snprintf(buf + pos, RESP_BUFFER_SIZE - pos,
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: keep-alive\r\n"
        "Cache-Control: no-cache\r\n"
        "\r\n",
        res->body_len
    );

    // Body
    memcpy(buf + pos, res->body_ptr, res->body_len);

    ctx->write_total = pos + res->body_len;
    ctx->write_pos = 0;
}
