#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include <http/utils.h>
#include <http/request.h>
#include <http/conn.h>

int parse_req_line(const char *buf, int buflen, http_req_t *req);
int parse_headers(const char *buf, int buflen, http_req_t *req);
int parse_http_req(conn_ctx_t *ctx, http_req_t *req);

#ifdef __cplusplus
}
#endif