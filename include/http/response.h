#pragma once
#include <http/utils.h>

typedef struct http_resp{
    int status;
    const char *body_ptr;
    int body_len;
    int is_static; // flag for pre-compiled responses
} http_resp_t;

// pre-defined responses
SCC RESP_PING[] = 
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 16\r\n"
    "Connection: keep-alive\r\n"
    "Cache-Control: no-cache\r\n"
    "\r\n"
    "{\"status\":\"ok\"}";
SCI RESP_PING_LEN = sizeof(RESP_PING) - 1;

SCC RESP_404[] = 
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 23\r\n"
    "Connection: keep-alive\r\n"
    "\r\n"
    "{\"error\":\"Not Found\"}";
SCI RESP_404_LEN = sizeof(RESP_404) - 1;