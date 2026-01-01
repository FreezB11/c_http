#include <http/http.h>
#include <stdio.h>
#include <string.h>

/*
parsing this 
GET /<path>?key=value
*/
SII parse_req_line(const char *buf, int buflen, http_req_t *req){
    const char* p = buf, *end = buf + buflen;

    // method
    req->method = p;
    while( p < end && *p != ' ') p++;
    if( p == end) return -1;
    req->method_len = p - req->method;
    p++;

    // path
    req->path = p;
    while(p < end && *p != ' ' && *p != '?') p++;
    if(p == end) return -1;
    req->path_len = p - req->path;

    // query params
    req->param_count = 0;
    if(*p == '?'){
        p++;
        int idx = 0;
        while(p < end && idx < 8 && *p != ' '){
            req->params[idx].key = p;
            while(p < end && *p != '=' && *p != '&' && *p != ' ') p++;
            req->params[idx].key_len = p - req->params[idx].key;

            if(p < end && *p == '='){
                p++;
                req->params[idx].val = p;
                while(p < end && *p != '&' && *p != ' ')p++;
                req->params[idx].val_len = p - req->params[idx].val;
            }else{
                req->params[idx].val = "";
                req->params[idx].val_len = 0;
            }
            idx++;
            if(p < end && *p == '&') p++;
        }
        req->param_count = idx;
    }
    while( p < end && *p != '\n')p++;
    if(p < end) p++;
    return p - buf;
}

SII parse_headers(const char *buf, int buflen, http_req_t *req){
    const char *p = buf, *end = buf + buflen;
    req->content_length = 0;

    while(p < end - 1){
        if(p[0] == '\r' && p[1] == '\n') return (p+2)-buf;

        // not recommended for production
        // we brute-force check to speed up
        if(p[0] == 'C' && p[1] == 'o' && buflen - (p-buf) >= 15){
            if(memcmp(p, "Content-Length:", 15) == 0){
                p+=15;
                while(p < end && (*p == ' ' || *p == '\t')) p++;
                int len = 0;
                while(p < end && *p >= '0' && *p<= '9'){
                    len = len * 10 + (*p-'0');
                    p++;
                }
                req->content_length = len;
            }
        }
        while (p < end && *p != '\n')p++;
        if(p < end) p++;
        
    }
    return -1;
}

SII parse_http_req(conn_ctx_t *ctx, http_req_t *req){
    // we find end of req line
    char *EOL = memchr(ctx->read_buf, '\n', ctx->read_total);
    if(!EOL) return -1;

    int line_len = EOL - ctx->read_buf + 1;
    int consumed = parse_req_line(ctx->read_buf, line_len, req);
    if( consumed < 0) return -1;

    int pos = consumed;
    int header_consumed = parse_headers(ctx->read_buf + pos, ctx->read_total - pos, req);
    if(header_consumed < 0) return -1;
    pos += header_consumed;

    //body
    if( req->content_length > 0){
        if(ctx->read_total - pos < req->content_length) return -1;
        req->body = ctx->read_buf + pos;
        req->body_len = req->content_length;
        pos += req->content_length;
    }else{
        req->body = NULL;
        req->body_len = 0;
    }

    return pos;
}