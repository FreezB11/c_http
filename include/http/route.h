#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include <http/utils.h>
#include <http/request.h>
#include <http/response.h>

typedef void (*route_handler_t)(http_req_t *, http_resp_t *);

typedef struct {
    const char     *method;
    const char     *path;
    route_handler_t handler;
} route_t;

/* C-layer route table (fallback when no C++ dispatcher is set) */
void add_route(const char *method, const char *path, route_handler_t handler);
void route_req(http_req_t *req, http_resp_t *res);

/* C++ dispatcher hook — if set, bypasses C route table entirely */
typedef void (*cpp_dispatch_fn)(http_req_t *, http_resp_t *);
void set_cpp_dispatcher(cpp_dispatch_fn fn);

/* Built-in handlers */
void handle_ping(http_req_t *req, http_resp_t *res);
void handle_echo(http_req_t *req, http_resp_t *res);
void handle_404(http_req_t *req, http_resp_t *res);
void handle_500(http_req_t *req, http_resp_t *res);

void build_resp(conn_ctx_t *ctx, http_resp_t *res);

#ifdef __cplusplus
}
#endif