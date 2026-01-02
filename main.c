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

static inline const char *
req_get_param(http_req_t *req, const char *key, int *val_len) {
    int klen = strlen(key);

    for (int i = 0; i < req->param_count; i++) {
        if (req->params[i].key_len == klen &&
            memcmp(req->params[i].key, key, klen) == 0) {
            if (val_len) *val_len = req->params[i].val_len;
            return req->params[i].val;
        }
    }
    return NULL;
}

static inline float read_cpu_temp(void) {
    FILE *f = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (!f) return -1.0f;

    int millideg;
    fscanf(f, "%d", &millideg);
    fclose(f);

    return millideg / 1000.0f;
}

#define TEMP_BUF_SIZE 64

static float temp_buf[TEMP_BUF_SIZE];
static int temp_idx = 0;
static int temp_count = 0;
static pthread_mutex_t temp_lock = PTHREAD_MUTEX_INITIALIZER;

static inline void record_temp(float t) {
    pthread_mutex_lock(&temp_lock);
    temp_buf[temp_idx] = t;
    temp_idx = (temp_idx + 1) % TEMP_BUF_SIZE;
    if (temp_count < TEMP_BUF_SIZE) temp_count++;
    pthread_mutex_unlock(&temp_lock);
}

static inline float avg_temp(void) {
    float sum = 0.0f;

    pthread_mutex_lock(&temp_lock);
    for (int i = 0; i < temp_count; i++)
        sum += temp_buf[i];
    pthread_mutex_unlock(&temp_lock);

    return temp_count ? sum / temp_count : 0.0f;
}



void cpu_temp_handler(http_req_t *req, http_resp_t *res) {
    static __thread char buf[512];

    float cur = read_cpu_temp();
    if (cur < 0) {
        res->status = 500;
        res->body_ptr = "temp read error";
        res->body_len = 15;
        return;
    }

    record_temp(cur);

    int avg_len = 0, last_len = 0;
    const char *avg_q  = req_get_param(req, "avg",  &avg_len);
    const char *last_q = req_get_param(req, "last", &last_len);

    int off = 0;
    off += snprintf(buf + off, sizeof(buf) - off, "{");

    int need_comma = 0;

    if (avg_q) {
        off += snprintf(buf + off, sizeof(buf) - off,
                        "\"avg\":%.2f", avg_temp());
        need_comma = 1;
    }

    if (last_q) {
        int n = 0;
        for (int i = 0; i < last_len; i++)
            if (last_q[i] >= '0' && last_q[i] <= '9')
                n = n * 10 + (last_q[i] - '0');

        if (n > TEMP_BUF_SIZE) n = TEMP_BUF_SIZE;

        if (need_comma)
            off += snprintf(buf + off, sizeof(buf) - off, ",");

        off += snprintf(buf + off, sizeof(buf) - off, "\"last\":[");

        pthread_mutex_lock(&temp_lock);
        for (int i = 0; i < n && i < temp_count; i++) {
            int idx = (temp_idx - 1 - i + TEMP_BUF_SIZE) % TEMP_BUF_SIZE;
            off += snprintf(buf + off, sizeof(buf) - off,
                            "%.2f%s",
                            temp_buf[idx],
                            (i + 1 < n && i + 1 < temp_count) ? "," : "");
        }
        pthread_mutex_unlock(&temp_lock);

        off += snprintf(buf + off, sizeof(buf) - off, "]");
        need_comma = 1;
    }

    if (!avg_q && !last_q) {
        off += snprintf(buf + off, sizeof(buf) - off,
                        "\"temp\":%.2f", cur);
    }

    off += snprintf(buf + off, sizeof(buf) - off, "}");

    res->status = 200;
    res->is_static = 1;
    res->body_ptr = buf;
    res->body_len = off;
}

// void hi_router(http_req_t *req, http_resp_t *res){
//     static __thread char res_buf[] = "hello";
//     res->is_static = 1;
//     res->status = 200;
//     res->body_ptr = res_buf;
//     res->body_len = strlen(res_buf);
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
    add_route("GET", "/api/v1/cpu/temp", cpu_temp_handler);
    // add_route("GET","/hi", hi_router);
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