#pragma once
#include <http/router.hpp>
#include <http/middleware.hpp>
#include <http/json.hpp>
#include <http/http.h>
#include <http/route.h>

namespace http {

/* ─────────────────────────────────────────────────────────────────────────
   App — the entry point for the library.

   Example:
       http::App app;
       app.use(http::middleware::logger());

       app.get("/ping", [](http::Context& ctx) {
           ctx.json(http::Json::object().set("status", "ok"));
       });

       app.get("/user/:id", [](http::Context& ctx) {
           ctx.json(http::Json::object().set("id", ctx.param("id")));
       });

       app.post("/data", [](http::Context& ctx) {
           auto body = ctx.body_json();
           ctx.status(201).json(body);
       });

       app.use("/upstream", http::middleware::proxy("http://localhost:3001"));

       app.listen(8080);   // blocks until SIGINT
   ───────────────────────────────────────────────────────────────────────── */
class App {
public:
    App()  = default;
    ~App() = default;

    App(const App &) = delete;
    App &operator=(const App &) = delete;

    /* ── Routing ───────────────────────────────────────────────────────── */
    App &get(const std::string &path, HandlerFn h) {
        router_.add_route("GET",    path, std::move(h)); return *this;
    }
    App &post(const std::string &path, HandlerFn h) {
        router_.add_route("POST",   path, std::move(h)); return *this;
    }
    App &put(const std::string &path, HandlerFn h) {
        router_.add_route("PUT",    path, std::move(h)); return *this;
    }
    App &del(const std::string &path, HandlerFn h) {
        router_.add_route("DELETE", path, std::move(h)); return *this;
    }
    App &patch(const std::string &path, HandlerFn h) {
        router_.add_route("PATCH",  path, std::move(h)); return *this;
    }
    App &any(const std::string &path, HandlerFn h) {
        router_.add_route("*",      path, std::move(h)); return *this;
    }

    /* ── Middleware ────────────────────────────────────────────────────── */

    /* Global middleware — runs for every request */
    App &use(MiddlewareFn fn) {
        router_.add_middleware("", std::move(fn)); return *this;
    }
    /* Prefix-scoped middleware — only runs when path starts with prefix */
    App &use(const std::string &prefix, MiddlewareFn fn) {
        router_.add_middleware(prefix, std::move(fn)); return *this;
    }

    /* ── Configure ─────────────────────────────────────────────────────── */
    App &threads(int n) { threads_ = n; return *this; }

    /* ── Start ─────────────────────────────────────────────────────────── */
    /* Blocks until SIGINT / SIGTERM */
    void listen(int port = 8080);

    /* Internal dispatch — called by the C bridge */
    void dispatch(http_req_t *req, http_resp_t *res);

private:
    Router router_;
    int    threads_ = THREAD_COUNT;
};

} // namespace http