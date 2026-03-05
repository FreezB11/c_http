;///@file: framework.h
#include <http/http.h>
#include <http/request.h>
#include <http/response.h>
#include <http/route.h>

typedef void* fn;
typedef http_req_t* Request;
typedef http_resp_t* Response;


class HTTP{
private:
    conn_ctx_t *conn_pool;
    int conn_pool_next;
    pthread_mutex_t pool_mutex;
    route_t routes[128];
    size_t route_count;
public:
    HTTP(){
        conn_ctx_t *conn_pool = nullptr;
        int conn_pool_next = 0;
        pthread_mutex_t pool_mutex = PTHREAD_MUTEX_INITIALIZER;
        size_t route_count = 0;
    }
    fn handle(char* method, char* path, void (*handler)(Request req, Response res)){
        routes[route_count++] = (route_t){method, path, handler};
    };
};