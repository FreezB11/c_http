#pragma once
#include <http/utils.h>
#include <http/request.h>
#include <http/response.h>

typedef void (*route_handler_t)(http_req_t*, http_resp_t*);

typedef struct {
    const char *method;
    const char *path;
    route_handler_t handler;
} route_t;

void route_req(http_req_t *req, http_resp_t *res);
void add_route(const char *method, const char *path, route_handler_t handler);
void handle_ping(http_req_t *req, http_resp_t *res);
void handle_echo(http_req_t *req, http_resp_t *res);
void handle_404(http_req_t *req, http_resp_t *res);