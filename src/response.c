#include <http/http.h>
#include <http/conn.h>
#include <http/response.h>
#include <http/route.h>
#include <stdio.h>
#include <string.h>

/* ── Pre-built static responses ─────────────────────────────────────────
   Defined HERE (not in the header) to avoid ODR violations.            */

const char RESP_PING[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 15\r\n"
    "Connection: keep-alive\r\n"
    "Cache-Control: no-cache\r\n"
    "\r\n"
    "{\"status\":\"ok\"}";
const int RESP_PING_LEN = sizeof(RESP_PING) - 1;

const char RESP_404[] =
    "HTTP/1.1 404 Not Found\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 22\r\n"
    "Connection: keep-alive\r\n"
    "\r\n"
    "{\"error\":\"Not Found\"}";
const int RESP_404_LEN = sizeof(RESP_404) - 1;

const char RESP_500[] =
    "HTTP/1.1 500 Internal Server Error\r\n"
    "Content-Type: application/json\r\n"
    "Content-Length: 27\r\n"
    "Connection: keep-alive\r\n"
    "\r\n"
    "{\"error\":\"Internal Error\"}";
const int RESP_500_LEN = sizeof(RESP_500) - 1;

/* ── build_resp ──────────────────────────────────────────────────────── */

void build_resp(conn_ctx_t *ctx, http_resp_t *res) {
    char *buf = ctx->write_buf;
    int   pos = 0;

    const char *status_text =
        res->status == 200 ? "OK" :
        res->status == 201 ? "Created" :
        res->status == 204 ? "No Content" :
        res->status == 301 ? "Moved Permanently" :
        res->status == 302 ? "Found" :
        res->status == 400 ? "Bad Request" :
        res->status == 401 ? "Unauthorized" :
        res->status == 403 ? "Forbidden" :
        res->status == 404 ? "Not Found" :
        res->status == 405 ? "Method Not Allowed" :
        res->status == 422 ? "Unprocessable Entity" :
        res->status == 429 ? "Too Many Requests" :
        res->status == 500 ? "Internal Server Error" :
        res->status == 502 ? "Bad Gateway" :
        res->status == 503 ? "Service Unavailable" : "OK";

    pos += snprintf(buf + pos, RESP_BUFFER_SIZE - pos,
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %d\r\n"
        "Connection: keep-alive\r\n"
        "\r\n",
        res->status, status_text, res->body_len);

    if (res->body_ptr && res->body_len > 0) {
        int copy = res->body_len;
        if (pos + copy > RESP_BUFFER_SIZE) copy = RESP_BUFFER_SIZE - pos;
        memcpy(buf + pos, res->body_ptr, copy);
        pos += copy;
    }

    ctx->write_total = pos;
    ctx->write_pos   = 0;
}