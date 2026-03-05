#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include <http/request.h>
#include <http/response.h>
#include <http/utils.h>


typedef enum {
    CONN_READING = 0,
    CONN_WRITING = 1,
    CONN_CLOSE = 2
} conn_state_t;

/*
this struct represents a single client connection context. 

fd	            The socket file descriptor for this client.
state	        Current connection state: reading, writing, or closed.
read_buf	    Buffer to store incoming HTTP request data.
write_buf	    Buffer to store HTTP response data to be sent.
read_total	    How many bytes have been read so far.
write_total	    How many bytes are in write_buf.
write_pos	    How many bytes have been written so far.
req	            Parsed HTTP request info (method, path, headers, body).
resp	        Prepared HTTP response info (status, body pointer, length).
*/
typedef struct {
    int           fd;
    conn_state_t  state;
    char         *read_buf;        /* BUFFER_SIZE bytes, heap */
    char         *write_buf;       /* RESP_BUFFER_SIZE bytes, heap */
    int           read_total;
    int           write_total;
    int           write_pos;
    int           pool_index;      /* -1 if heap-allocated fallback */
    http_req_t    req;
    http_resp_t   resp;
    /* dynamic response body — C++ layer writes here before build_resp */
    char          resp_body[32768];
} conn_ctx_t;

conn_ctx_t *alloc_conn(void);
void        free_conn(conn_ctx_t *ctx);
void        handle_readable(conn_ctx_t *ctx);
void        handle_writable(conn_ctx_t *ctx);

#ifdef __cplusplus
}
#endif