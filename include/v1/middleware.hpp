#pragma once
#include <v1/router.hpp>
#include <v1/json.hpp>
#include <string>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

namespace http {
namespace middleware {

/* ─────────────────────────────────────────────────────────────────────────
   logger() — logs method, path, status, and timing to stdout
   Usage: app.use(middleware::logger());
   ───────────────────────────────────────────────────────────────────────── */
inline MiddlewareFn logger() {
    return [](Context &ctx, NextFn next) {
        struct timespec t0;
        clock_gettime(CLOCK_MONOTONIC, &t0);

        next();

        struct timespec t1;
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double ms = (t1.tv_sec - t0.tv_sec) * 1000.0
                  + (t1.tv_nsec - t0.tv_nsec) / 1e6;

        fprintf(stdout, "[http] %s %s → %d  (%.2f ms)\n",
                ctx.method().c_str(),
                ctx.path().c_str(),
                ctx.status_code(),
                ms);
        fflush(stdout);
    };
}

/* ─────────────────────────────────────────────────────────────────────────
   cors() — adds CORS headers to every response
   Usage: app.use(middleware::cors());
          app.use(middleware::cors("https://mysite.com"));
   ───────────────────────────────────────────────────────────────────────── */
inline MiddlewareFn cors(const std::string &origin = "*",
                         const std::string &methods = "GET, POST, PUT, DELETE, OPTIONS",
                         const std::string &headers = "Content-Type, Authorization") {
    return [origin, methods, headers](Context &ctx, NextFn next) {
        /* Handle pre-flight immediately */
        if (ctx.method() == "OPTIONS") {
            /* Build a minimal 204 response with CORS headers.
               The actual header injection happens via the response body for now;
               full header map support is a Phase-2 item. */
            ctx.status(204).end();
            return;
        }
        next();
        /* NOTE: Custom response headers (beyond Content-Type) require
           extending http_resp_t.  The placeholders below show intent;
           implement once header map is added to the response struct. */
        (void)origin; (void)methods; (void)headers;
    };
}

/* ─────────────────────────────────────────────────────────────────────────
   proxy() — synchronous reverse proxy to a target server
   Usage: app.use("/api", middleware::proxy("http://localhost:3000"));

   ⚠ Blocking implementation: the worker thread waits for the upstream
     response.  Suitable for low-to-moderate load.  Replace with async
     upstream connection in Phase 3 for maximum throughput.
   ───────────────────────────────────────────────────────────────────────── */
inline MiddlewareFn proxy(const std::string &target) {
    /* Parse "http://host:port" or "http://host" */
    std::string host;
    int         port = 80;
    std::string base_path;

    auto parse_target = [&]() {
        std::string url = target;
        if (url.substr(0, 7) == "http://")  url = url.substr(7);
        if (url.substr(0, 8) == "https://") url = url.substr(8); /* TLS not supported */

        size_t slash = url.find('/');
        std::string hostport = (slash == std::string::npos) ? url : url.substr(0, slash);
        base_path = (slash == std::string::npos) ? "" : url.substr(slash);

        size_t colon = hostport.find(':');
        if (colon != std::string::npos) {
            host = hostport.substr(0, colon);
            port = std::stoi(hostport.substr(colon + 1));
        } else {
            host = hostport;
        }
    };
    parse_target();

    return [host, port, base_path](Context &ctx, NextFn /*next*/) {
        /* Build upstream request */
        char req_buf[32768];
        int  req_len = 0;

        std::string upstream_path = base_path + ctx.path();
        std::string qs;
        /* Reconstruct query string */
        http_req_t *r = ctx.raw_req();
        for (int i = 0; i < r->param_count; i++) {
            qs += (i == 0 ? "?" : "&");
            qs += std::string(r->params[i].key, r->params[i].key_len);
            qs += "=";
            qs += std::string(r->params[i].val, r->params[i].val_len);
        }
        if (!qs.empty()) upstream_path += qs;

        std::string body_text = ctx.body_text();
        req_len = snprintf(req_buf, sizeof(req_buf),
            "%s %s HTTP/1.1\r\n"
            "Host: %s:%d\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n",
            ctx.method().c_str(),
            upstream_path.c_str(),
            host.c_str(), port,
            (int)body_text.size());

        if (!body_text.empty() &&
            req_len + (int)body_text.size() < (int)sizeof(req_buf)) {
            memcpy(req_buf + req_len, body_text.data(), body_text.size());
            req_len += (int)body_text.size();
        }

        /* Connect to upstream */
        struct hostent *he = gethostbyname(host.c_str());
        if (!he) { ctx.status(502).json_error("Bad Gateway: DNS"); return; }

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons((uint16_t)port);
        memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

        struct timeval tv{ .tv_sec = 5, .tv_usec = 0 };
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            close(sock);
            ctx.status(502).json_error("Bad Gateway: connect");
            return;
        }

        /* Send request */
        send(sock, req_buf, req_len, MSG_NOSIGNAL);

        /* Read response */
        static __thread char resp_buf[65536];
        int total = 0;
        while (total < (int)sizeof(resp_buf) - 1) {
            int n = recv(sock, resp_buf + total,
                         sizeof(resp_buf) - 1 - total, 0);
            if (n <= 0) break;
            total += n;
        }
        close(sock);
        resp_buf[total] = '\0';

        /* Extract status code and body from upstream response */
        int upstream_status = 200;
        const char *body_start = nullptr;

        if (total > 9 && memcmp(resp_buf, "HTTP/", 5) == 0) {
            const char *sp = (const char *)memchr(resp_buf, ' ', 12);
            if (sp) upstream_status = atoi(sp + 1);
        }
        /* Find end of headers (\r\n\r\n) */
        const char *hdr_end = strstr(resp_buf, "\r\n\r\n");
        if (hdr_end) body_start = hdr_end + 4;

        if (body_start) {
            int body_len = (int)(total - (body_start - resp_buf));
            ctx.status(upstream_status)
               .send(std::string(body_start, body_len), "application/json");
        } else {
            ctx.status(upstream_status).end();
        }
    };
}

} // namespace middleware
} // namespace http