#pragma once
#include <http/utils.h>
#include <http/request.h>
#include <http/conn.h>

SII parse_req_line(const char *buf, int buflen, http_req_t *req);
SII parse_headers(const char *buf, int buflen, http_req_t *req);
SII parse_http_req(conn_ctx_t *ctx, http_req_t *req);