#include <http/http.h>
#include <http/route.h>
#include <stdio.h>
#include <string.h>

static route_t        routes[128];
static int            route_count = 0;
static cpp_dispatch_fn g_cpp_dispatcher = NULL;

void set_cpp_dispatcher(cpp_dispatch_fn fn) {
    g_cpp_dispatcher = fn;
}

void add_route(const char *method, const char *path, route_handler_t handler) {
    if (route_count >= 128) return;
    routes[route_count++] = (route_t){ method, path, handler };
}

void handle_ping(http_req_t *req, http_resp_t *res) {
    (void)req;
    static __thread char json[] = "{\"status\":\"ok\"}";
    res->status    = 200;
    res->body_ptr  = json;
    res->body_len  = sizeof(json) - 1;
    res->is_static = 0;
}

void handle_echo(http_req_t *req, http_resp_t *res) {
    static __thread char body[RESP_BUFFER_SIZE];
    int pos = 0;
    pos += snprintf(body + pos, RESP_BUFFER_SIZE - pos, "{");
    for (int i = 0; i < req->param_count; i++) {
        if (i > 0) pos += snprintf(body + pos, RESP_BUFFER_SIZE - pos, ",");
        pos += snprintf(body + pos, RESP_BUFFER_SIZE - pos,
                        "\"%.*s\":\"%.*s\"",
                        req->params[i].key_len, req->params[i].key,
                        req->params[i].val_len, req->params[i].val);
    }
    pos += snprintf(body + pos, RESP_BUFFER_SIZE - pos, "}");
    res->status    = 200;
    res->body_ptr  = body;
    res->body_len  = pos;
    res->is_static = 0;
}

void handle_404(http_req_t *req, http_resp_t *res) {
    (void)req;
    static const char json[] = "{\"error\":\"Not Found\"}";
    res->status    = 404;
    res->body_ptr  = json;
    res->body_len  = sizeof(json) - 1;
    res->is_static = 0;
}

void handle_500(http_req_t *req, http_resp_t *res) {
    (void)req;
    static const char json[] = "{\"error\":\"Internal Error\"}";
    res->status    = 500;
    res->body_ptr  = json;
    res->body_len  = sizeof(json) - 1;
    res->is_static = 0;
}

void route_req(http_req_t *req, http_resp_t *res) {
    /* C++ layer gets first crack — it owns routing when set */
    if (g_cpp_dispatcher) {
        g_cpp_dispatcher(req, res);
        return;
    }

    /* Fallback: C route table */
    for (int i = 0; i < route_count; i++) {
        if (req->method_len == (int)strlen(routes[i].method) &&
            strncmp(req->method, routes[i].method, req->method_len) == 0 &&
            req->path_len == (int)strlen(routes[i].path) &&
            strncmp(req->path, routes[i].path, req->path_len) == 0) {
            routes[i].handler(req, res);
            return;
        }
    }
    handle_404(req, res);
}