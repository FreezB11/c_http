#include <http/http.h>
#include <http/route.h>

#include <stdio.h>
#include <string.h>

route_t routes[128];
int route_count = 0;

// void handle_ping(http_req_t *req, http_resp_t *res) {
//     printf("[LOG]: server is pinged\n");
//     static __thread char json[RESP_BUFFER_SIZE] = "{\"status\":\"ok\"}";
//     printf("[LOG]: size of the message = %d\n", sizeof(json));
//     printf("[LOG]: size of the message = %d\n", sizeof("{\"status\":\"ok\"}"));
//     res->status = 200;
//     res->body_ptr = json;
//     res->body_len = sizeof(json);
// }

void handle_ping(http_req_t *req, http_resp_t *res) {
    // printf("[LOG]: server is pinged\n");

    static __thread char json[] = "{\"status\":\"ok\"}";
    res->is_static = 1; 
    res->status = 200;
    res->body_ptr = json;
    res->body_len = strlen(json);  // NOT sizeof(json)
}


void handle_echo(http_req_t *req, http_resp_t *res) {
    // printf("[LOG]: the user hit the /echo route\n");
    static __thread char resp_body[RESP_BUFFER_SIZE];
    int pos = 0;
    pos += snprintf(resp_body + pos, RESP_BUFFER_SIZE - pos, "{");
    for (int i = 0; i < req->param_count; i++) {
        if (i > 0) pos += snprintf(resp_body + pos, RESP_BUFFER_SIZE - pos, ",");
        pos += snprintf(resp_body + pos, RESP_BUFFER_SIZE - pos,
                        "\"%.*s\":\"%.*s\"",
                        req->params[i].key_len, req->params[i].key,
                        req->params[i].val_len, req->params[i].val);
    }
    pos += snprintf(resp_body + pos, RESP_BUFFER_SIZE - pos, "}");
    res->status = 200;
    res->body_ptr = resp_body;
    res->body_len = pos;
}

void handle_404(http_req_t *req, http_resp_t *res) {
    // printf("[LOG]: this is a site not found error code = 404\n");
    static const char json[] = "{\"error\":\"Not Found\"}";
    res->status = 404;
    res->body_ptr = (char *)json;
    res->body_len = sizeof(json) - 1;
}

// this is not dynamic only for /ping and /echo
// SIV route_req(http_req_t *req, http_resp_t *res){
//     // Fast path comparison for /ping
//     if (req->method_len == 3 && req->path_len == 5 &&
//         req->method[0] == 'G' && req->method[1] == 'E' && req->method[2] == 'T' &&
//         req->path[0] == '/' && req->path[1] == 'p' && req->path[2] == 'i' && 
//         req->path[3] == 'n' && req->path[4] == 'g') {
//         res->status = 200;
//         res->body_ptr = RESP_PING;
//         res->body_len = RESP_PING_LEN;
//         res->is_static = 1;
//         return;
//     }
    
//     // /echo endpoint
//     if (req->path_len == 5 &&
//         req->path[0] == '/' && req->path[1] == 'e' && req->path[2] == 'c' && 
//         req->path[3] == 'h' && req->path[4] == 'o') {
//         // Dynamic response for echo
//         res->status = 200;
//         res->is_static = 0;
//         return;
//     }
    
//     // 404
//     res->status = 404;
//     res->body_ptr = RESP_404;
//     res->body_len = RESP_404_LEN;
//     res->is_static = 1;
// }

void add_route(const char *method, const char *path, route_handler_t handler){
    if(route_count >= 128) return;
    routes[route_count++] = (route_t){method, path, handler};
}

void route_req(http_req_t *req, http_resp_t *res) {
    for (int i = 0; i < route_count; i++) {
        if (req->method_len == (int)strlen(routes[i].method) &&
            strncmp(req->method, routes[i].method, req->method_len) == 0 &&
            req->path_len == (int)strlen(routes[i].path) &&
            strncmp(req->path, routes[i].path, req->path_len) == 0) {
            routes[i].handler(req, res);
            return;
        }
    }
    // printf("this is is a 404 error\n");
    handle_404(req, res);
}