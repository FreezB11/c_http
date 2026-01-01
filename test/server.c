#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <asm-generic/socket.h>

#define MAX_EVENTS 1024
#define BUFFER_SIZE 8192
#define MAX_ROUTES 64
#define MAX_PARAMS 8
#define MAX_PARAM_LEN 256

// HTTP Request structure
typedef struct {
    char method[16];
    char path[512];
    char version[16];
    char body[4096];
    char params[MAX_PARAMS][2][MAX_PARAM_LEN]; // [index][0=key, 1=value]
    int param_count;
} http_request_t;

// HTTP Response structure
typedef struct {
    int status;
    char body[8192];
} http_response_t;

// Route handler function pointer
typedef void (*route_handler_t)(http_request_t*, http_response_t*);

// Route structure
typedef struct {
    char method[16];
    char pattern[256];
    char param_names[MAX_PARAMS][MAX_PARAM_LEN];
    int param_count;
    route_handler_t handler;
} route_t;

// Router structure
typedef struct {
    route_t routes[MAX_ROUTES];
    int route_count;
} router_t;

router_t router;

// Fast string utilities
static inline int str_starts_with(const char* str, const char* prefix) {
    while (*prefix) {
        if (*str++ != *prefix++) return 0;
    }
    return 1;
}

static inline void str_copy(char* dest, const char* src, size_t max_len) {
    size_t i = 0;
    while (src[i] && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

// Parse HTTP request (optimized)
void parse_request(const char* raw, http_request_t* req) {
    memset(req, 0, sizeof(http_request_t));
    
    const char* line_end = strstr(raw, "\r\n");
    if (!line_end) return;
    
    // Parse request line
    sscanf(raw, "%15s %511s %15s", req->method, req->path, req->version);
    
    // Extract path parameters
    char* query = strchr(req->path, '?');
    if (query) {
        *query = '\0';
        query++;
        
        // Parse query params
        char* param = strtok(query, "&");
        while (param && req->param_count < MAX_PARAMS) {
            char* eq = strchr(param, '=');
            if (eq) {
                *eq = '\0';
                str_copy(req->params[req->param_count][0], param, MAX_PARAM_LEN);
                str_copy(req->params[req->param_count][1], eq + 1, MAX_PARAM_LEN);
                req->param_count++;
            }
            param = strtok(NULL, "&");
        }
    }
    
    // Find body
    const char* body_start = strstr(raw, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        str_copy(req->body, body_start, sizeof(req->body));
    }
}

// Build HTTP response (optimized)
int build_response(http_response_t* res, char* buffer, size_t buf_size) {
    const char* status_text = "OK";
    if (res->status == 404) status_text = "Not Found";
    else if (res->status == 201) status_text = "Created";
    
    return snprintf(buffer, buf_size,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: keep-alive\r\n"
        "\r\n%s",
        res->status, status_text, strlen(res->body), res->body);
}

// Fast JSON builder (stack-based)
void json_start(char* buf) {
    strcpy(buf, "{");
}

void json_add_string(char* buf, const char* key, const char* value) {
    if (strlen(buf) > 1) strcat(buf, ",");
    strcat(buf, "\"");
    strcat(buf, key);
    strcat(buf, "\":\"");
    strcat(buf, value);
    strcat(buf, "\"");
}

void json_add_int(char* buf, const char* key, int value) {
    if (strlen(buf) > 1) strcat(buf, ",");
    char temp[256];
    sprintf(temp, "\"%s\":%d", key, value);
    strcat(buf, temp);
}

void json_end(char* buf) {
    strcat(buf, "}");
}

// Pattern matching for dynamic routes
int match_pattern(const route_t* route, http_request_t* req) {
    if (strcmp(route->method, req->method) != 0) return 0;
    
    char pattern_copy[256], path_copy[512];
    str_copy(pattern_copy, route->pattern, sizeof(pattern_copy));
    str_copy(path_copy, req->path, sizeof(path_copy));
    
    char* pattern_token = strtok(pattern_copy, "/");
    char* path_token = strtok(path_copy, "/");
    int param_idx = 0;
    
    while (pattern_token && path_token) {
        if (pattern_token[0] == '{' && pattern_token[strlen(pattern_token)-1] == '}') {
            // Dynamic segment - capture parameter
            str_copy(req->params[req->param_count][0], route->param_names[param_idx], MAX_PARAM_LEN);
            str_copy(req->params[req->param_count][1], path_token, MAX_PARAM_LEN);
            req->param_count++;
            param_idx++;
        } else if (strcmp(pattern_token, path_token) != 0) {
            return 0;
        }
        
        pattern_token = strtok(NULL, "/");
        path_token = strtok(NULL, "/");
    }
    
    return (pattern_token == NULL && path_token == NULL);
}

// Add route to router
void add_route(const char* method, const char* pattern, route_handler_t handler) {
    if (router.route_count >= MAX_ROUTES) return;
    
    route_t* route = &router.routes[router.route_count++];
    str_copy(route->method, method, sizeof(route->method));
    str_copy(route->pattern, pattern, sizeof(route->pattern));
    route->handler = handler;
    route->param_count = 0;
    
    // Extract parameter names
    char pattern_copy[256];
    str_copy(pattern_copy, pattern, sizeof(pattern_copy));
    char* token = strtok(pattern_copy, "/");
    
    while (token) {
        if (token[0] == '{' && token[strlen(token)-1] == '}') {
            size_t len = strlen(token) - 2;
            strncpy(route->param_names[route->param_count], token + 1, len);
            route->param_names[route->param_count][len] = '\0';
            route->param_count++;
        }
        token = strtok(NULL, "/");
    }
}

// Route handler
void handle_request(http_request_t* req, http_response_t* res) {
    res->status = 404;
    strcpy(res->body, "{\"error\":\"Not Found\"}");
    
    for (int i = 0; i < router.route_count; i++) {
        // Reset param count for each match attempt
        int original_param_count = req->param_count;
        if (match_pattern(&router.routes[i], req)) {
            router.routes[i].handler(req, res);
            return;
        }
        req->param_count = original_param_count;
    }
}

// Helper to get param value
const char* get_param(http_request_t* req, const char* key) {
    for (int i = 0; i < req->param_count; i++) {
        if (strcmp(req->params[i][0], key) == 0) {
            return req->params[i][1];
        }
    }
    return "";
}

// ============ Route Handlers ============

void handler_root(http_request_t* req, http_response_t* res) {
    res->status = 200;
    strcpy(res->body, "{\"message\":\"Welcome to high-performance HTTP server!\"}");
}

void handler_users_get(http_request_t* req, http_response_t* res) {
    res->status = 200;
    const char* id = get_param(req, "id");
    
    json_start(res->body);
    json_add_string(res->body, "userId", id);
    json_add_string(res->body, "name", "John Doe");
    json_add_int(res->body, "age", 30);
    json_end(res->body);
}

void handler_products_get(http_request_t* req, http_response_t* res) {
    res->status = 200;
    const char* category = get_param(req, "category");
    const char* id = get_param(req, "id");
    
    json_start(res->body);
    json_add_string(res->body, "category", category);
    json_add_string(res->body, "productId", id);
    json_add_int(res->body, "price", 299);
    json_end(res->body);
}

void handler_data_post(http_request_t* req, http_response_t* res) {
    res->status = 201;
    
    json_start(res->body);
    json_add_string(res->body, "status", "created");
    json_add_string(res->body, "received", req->body);
    json_end(res->body);
}

// Set socket to non-blocking
int set_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    // Initialize router
    memset(&router, 0, sizeof(router));
    
    // Register routes
    add_route("GET", "/", handler_root);
    add_route("GET", "/users/{id}", handler_users_get);
    add_route("GET", "/products/{category}/{id}", handler_products_get);
    add_route("POST", "/data", handler_data_post);
    
    // Create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        return 1;
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);
    
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return 1;
    }
    
    if (listen(server_fd, SOMAXCONN) < 0) {
        perror("listen failed");
        return 1;
    }
    
    // Set up epoll for high-performance I/O
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1 failed");
        return 1;
    }
    
    struct epoll_event ev, events[MAX_EVENTS];
    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev) < 0) {
        perror("epoll_ctl failed");
        return 1;
    }
    
    printf("🚀 Server running on http://0.0.0.0:8080\n");
    printf("📊 Ready for stress testing!\n");
    printf("Test with: wrk -t12 -c400 -d30s http://localhost:8080/\n\n");
    
    char buffer[BUFFER_SIZE];
    char response_buf[BUFFER_SIZE];
    
    // Main event loop
    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == server_fd) {
                // Accept new connection
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
                
                if (client_fd < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        perror("accept failed");
                    }
                    continue;
                }
                
                set_nonblocking(client_fd);
                
                ev.events = EPOLLIN | EPOLLET;
                ev.data.fd = client_fd;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
            } else {
                // Handle client request
                int client_fd = events[i].data.fd;
                ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
                
                if (bytes_read <= 0) {
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    close(client_fd);
                    continue;
                }
                
                buffer[bytes_read] = '\0';
                
                // Parse and handle request
                http_request_t req;
                http_response_t res;
                
                parse_request(buffer, &req);
                handle_request(&req, &res);
                
                int response_len = build_response(&res, response_buf, BUFFER_SIZE);
                write(client_fd, response_buf, response_len);
                
                // Close connection (can be changed to keep-alive)
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                close(client_fd);
            }
        }
    }
    
    close(server_fd);
    close(epoll_fd);
    return 0;
}