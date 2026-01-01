#include <http/http.h>
#include <http/route.h>
#include <stdlib.h>
#include <stdio.h>

conn_ctx_t *conn_pool = NULL;
int conn_pool_next = 0;
pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;

// void user_handler(http_req_t *req, http_resp_t *res, void *user_data){
//       const char *id = http_get_param(req, "id");
//       http_json_response(res, 200, "{\"userId\":\"%s\"}", id);
// }

int main(){
    /*
    http server = http_server(<optional_name>, port, ipv4 or ipv6)
    add_route(server, "/route/[]");
    =================================
    http_server_t *srv = http_server_create(4);

    http_server_route(srv, "GET", "/ping", ping_handler, NULL);
    http_server_route(srv, "GET", "/echo", echo_handler, NULL);

    http_server_listen(srv, "0.0.0.0", 8080);
    http_server_run(srv);

    http_server_destroy(srv);

    */
    // signal(SIGPIPE, SIG_IGN);

    http *App = CreateServer();
    // http_route(App, "GET", "/", home_handler, NULL);
    // add_route("GET", "/users/", user_handler, NULL);
    /*
    void user_handler(http_req_t *req, http_resp_t *res, void *user_data){
      const char *id = http_get_param(req, "id");
      http_json_response(res, 200, "{\"userId\":\"%s\"}", id);
    }
    */
    add_route("GET","/echo", handle_echo);
    add_route("GET","/ping", handle_ping);
    if (!App) {
        fprintf(stderr, "Failed to create server\n");
        return 1;
    }
    
    printf("✅ Server running! Press Ctrl+C to stop.\n\n");
    fflush(stdout);
    
    // Keep main thread alive
    for(;;) sleep(1);

    return HTTP_STATS;
}

// gcc -O3 -march=native -flto -pthread -D_GNU_SOURCE claud.c -o claud     
/*
best result
for threadcount 7 and tested with 7 threads
❯ wrk -t7 -c400 -d10s http://localhost:8080/echo\?key\=value
Running 10s test @ http://localhost:8080/echo?key=value
  7 threads and 400 connections
  Thread Stats   Avg      Stdev     Max   +/- Stdev
    Latency   145.58us   66.29us   5.41ms   91.78%
    Req/Sec   212.58k    60.04k  311.75k    71.86%
  14807030 requests in 10.03s, 1.86GB read
Requests/sec: 1476283.74
Transfer/sec:    190.07MB

: 1765966243:0;gcc -O3 -I include -march=native -flto -pthread -D_GNU_SOURCE main.c ./src/*.c -o http
: 1765966245:0;./http

*/