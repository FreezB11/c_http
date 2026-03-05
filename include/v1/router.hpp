#pragma once
#include <v1/context.hpp>
#include <string>
#include <vector>
#include <functional>
#include <sstream>

namespace http {

using HandlerFn    = std::function<void(Context &)>;
using NextFn       = std::function<void()>;
using MiddlewareFn = std::function<void(Context &, NextFn)>;

/* ─────────────────────────────────────────────────────────────────────────
   Internal helpers
   ───────────────────────────────────────────────────────────────────────── */
namespace detail {

inline std::vector<std::string> split_path(const std::string &p) {
    std::vector<std::string> segs;
    std::istringstream ss(p);
    std::string tok;
    while (std::getline(ss, tok, '/')) {
        if (!tok.empty()) segs.push_back(tok);
    }
    return segs;
}

} // namespace detail

/* ─────────────────────────────────────────────────────────────────────────
   Route — one registered route entry
   ───────────────────────────────────────────────────────────────────────── */
struct Route {
    std::string              method;    /* "GET", "POST", …  or "*" for any */
    std::string              pattern;   /* "/user/:id/posts/:n"              */
    std::vector<std::string> segments;  /* pre-split pattern segments        */
    HandlerFn                handler;

    Route(const std::string &m, const std::string &p, HandlerFn h)
        : method(m), pattern(p),
          segments(detail::split_path(p)),
          handler(std::move(h)) {}

    /* Returns true and fills params if this route matches method+path */
    bool match(const std::string &req_method,
               const std::string &req_path,
               std::unordered_map<std::string, std::string> &params) const {
        if (method != "*" && method != req_method) return false;

        auto path_segs = detail::split_path(req_path);
        if (path_segs.size() != segments.size()) return false;

        for (size_t i = 0; i < segments.size(); i++) {
            if (segments[i][0] == ':') {
                params[segments[i].substr(1)] = path_segs[i];
            } else if (segments[i] != path_segs[i]) {
                params.clear();
                return false;
            }
        }
        return true;
    }
};

/* ─────────────────────────────────────────────────────────────────────────
   MiddlewareEntry — global or prefix-scoped middleware
   ───────────────────────────────────────────────────────────────────────── */
struct MiddlewareEntry {
    std::string    prefix;  /* empty = applies to all paths */
    MiddlewareFn   fn;
};

/* ─────────────────────────────────────────────────────────────────────────
   Router — holds routes + middleware, dispatches requests
   ───────────────────────────────────────────────────────────────────────── */
class Router {
public:
    void add_route(const std::string &method,
                   const std::string &path,
                   HandlerFn handler) {
        routes_.emplace_back(method, path, std::move(handler));
    }

    void add_middleware(const std::string &prefix, MiddlewareFn fn) {
        middlewares_.push_back({prefix, std::move(fn)});
    }

    /* Dispatch a request. Fills ctx with a response. */
    void dispatch(Context &ctx) const {
        std::string method = ctx.method();
        std::string path   = ctx.path();

        /* Collect applicable middleware (global or matching prefix) */
        std::vector<const MiddlewareFn *> chain;
        for (const auto &e : middlewares_) {
            if (e.prefix.empty() ||
                path.compare(0, e.prefix.size(), e.prefix) == 0) {
                chain.push_back(&e.fn);
            }
        }

        /* Find matching route */
        const Route *matched = nullptr;
        for (const auto &r : routes_) {
            std::unordered_map<std::string, std::string> params;
            if (r.match(method, path, params)) {
                ctx.path_params = std::move(params);
                matched = &r;
                break;
            }
        }

        /* Build and run the full middleware + handler chain */
        size_t idx = 0;
        std::function<void()> run_chain = [&]() {
            if (idx < chain.size()) {
                const MiddlewareFn &mw = *chain[idx++];
                mw(ctx, run_chain);
            } else if (matched) {
                matched->handler(ctx);
            } else {
                /* 404 */
                ctx.status(404).json_error("Not Found");
            }
        };
        run_chain();
    }

private:
    std::vector<Route>           routes_;
    std::vector<MiddlewareEntry> middlewares_;
};

} // namespace http