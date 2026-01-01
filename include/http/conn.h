#pragma once
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
    int fd;
    conn_state_t state;
    char *read_buf;
    char *write_buf;
    int read_total;
    int write_total;
    int write_pos;
    http_req_t req;
    http_resp_t resp;
} conn_ctx_t;

conn_ctx_t *alloc_conn();
SIV handle_readable(conn_ctx_t *ctx);
SIV handle_writable(conn_ctx_t *ctx);