#include <http/http.h>
#include <http/parse.h>
#include <stdio.h>
#include <string.h>

int parse_req_line(const char *buf, int buflen, http_req_t *req) {
    const char *p = buf, *end = buf + buflen;

    /* Method */
    req->method = p;
    while (p < end && *p != ' ') p++;
    if (p == end) return -1;
    req->method_len = (int)(p - req->method);
    p++;

    /* Path */
    req->path = p;
    while (p < end && *p != ' ' && *p != '?') p++;
    if (p == end) return -1;
    req->path_len = (int)(p - req->path);

    /* Query params */
    req->param_count = 0;
    if (*p == '?') {
        p++;
        int idx = 0;
        while (p < end && idx < 16 && *p != ' ') {
            req->params[idx].key = p;
            while (p < end && *p != '=' && *p != '&' && *p != ' ') p++;
            req->params[idx].key_len = (int)(p - req->params[idx].key);

            if (p < end && *p == '=') {
                p++;
                req->params[idx].val = p;
                while (p < end && *p != '&' && *p != ' ') p++;
                req->params[idx].val_len = (int)(p - req->params[idx].val);
            } else {
                req->params[idx].val = "";
                req->params[idx].val_len = 0;
            }
            idx++;
            if (p < end && *p == '&') p++;
        }
        req->param_count = idx;
    }

    /* Skip rest of request line */
    while (p < end && *p != '\n') p++;
    if (p < end) p++;
    return (int)(p - buf);
}

int parse_headers(const char *buf, int buflen, http_req_t *req) {
    const char *p = buf, *end = buf + buflen;
    req->content_length   = 0;
    req->content_type     = NULL;
    req->content_type_len = 0;
    req->authorization    = NULL;
    req->host             = NULL;

    while (p < end - 1) {
        if (p[0] == '\r' && p[1] == '\n') return (int)((p + 2) - buf);

        int rem = (int)(end - p);

        if (rem >= 15 && memcmp(p, "Content-Length:", 15) == 0) {
            p += 15;
            while (p < end && (*p == ' ' || *p == '\t')) p++;
            int len = 0;
            while (p < end && *p >= '0' && *p <= '9') { len = len*10 + (*p-'0'); p++; }
            req->content_length = len;
        } else if (rem >= 13 && memcmp(p, "Content-Type:", 13) == 0) {
            p += 13;
            while (p < end && (*p == ' ' || *p == '\t')) p++;
            req->content_type = p;
            while (p < end && *p != '\r' && *p != '\n') p++;
            req->content_type_len = (int)(p - req->content_type);
        } else if (rem >= 14 && memcmp(p, "Authorization:", 14) == 0) {
            p += 14;
            while (p < end && (*p == ' ' || *p == '\t')) p++;
            req->authorization = p;
            while (p < end && *p != '\r' && *p != '\n') p++;
            req->authorization_len = (int)(p - req->authorization);
        } else if (rem >= 5 && memcmp(p, "Host:", 5) == 0) {
            p += 5;
            while (p < end && (*p == ' ' || *p == '\t')) p++;
            req->host = p;
            while (p < end && *p != '\r' && *p != '\n') p++;
            req->host_len = (int)(p - req->host);
        }

        while (p < end && *p != '\n') p++;
        if (p < end) p++;
    }
    return -1;
}

int parse_http_req(conn_ctx_t *ctx, http_req_t *req) {
    char *eol = (char *)memchr(ctx->read_buf, '\n', ctx->read_total);
    if (!eol) return -1;

    int line_len = (int)(eol - ctx->read_buf) + 1;
    int consumed = parse_req_line(ctx->read_buf, line_len, req);
    if (consumed < 0) return -1;

    int pos = consumed;
    int hdr = parse_headers(ctx->read_buf + pos, ctx->read_total - pos, req);
    if (hdr < 0) return -1;
    pos += hdr;

    if (req->content_length > 0) {
        if (ctx->read_total - pos < req->content_length) return -1;
        req->body     = ctx->read_buf + pos;
        req->body_len = req->content_length;
        pos += req->content_length;
    } else {
        req->body     = NULL;
        req->body_len = 0;
    }

    return pos;
}