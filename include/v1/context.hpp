#pragma once
#include <v1/json.hpp>
#include <http/request.h>
#include <http/response.h>
#include <string>
#include <unordered_map>
#include <cstring>

namespace http {

/* ─────────────────────────────────────────────────────────────────────────
   Context — passed to every handler and middleware.
   Wraps the raw C request + a pending response that is flushed after the
   handler chain completes.
   ───────────────────────────────────────────────────────────────────────── */
class Context {
public:
    /* Populated by the router before the handler is called */
    std::unordered_map<std::string, std::string> path_params;

    /* ── Request ───────────────────────────────────────────────────────── */

    std::string method() const {
        return std::string(req_->method, req_->method_len);
    }
    std::string path() const {
        return std::string(req_->path, req_->path_len);
    }

    /* URL path param: /user/:id  →  ctx.param("id") */
    std::string param(const std::string &key,
                      const std::string &def = "") const {
        auto it = path_params.find(key);
        return it != path_params.end() ? it->second : def;
    }

    /* Query string param: /search?q=hello  →  ctx.query("q") */
    std::string query(const std::string &key,
                      const std::string &def = "") const {
        for (int i = 0; i < req_->param_count; i++) {
            if (req_->params[i].key_len == (int)key.size() &&
                strncmp(req_->params[i].key, key.c_str(), key.size()) == 0) {
                return std::string(req_->params[i].val,
                                   req_->params[i].val_len);
            }
        }
        return def;
    }

    /* All query params as a map */
    std::unordered_map<std::string, std::string> query_all() const {
        std::unordered_map<std::string, std::string> m;
        for (int i = 0; i < req_->param_count; i++) {
            m[std::string(req_->params[i].key, req_->params[i].key_len)] =
                std::string(req_->params[i].val, req_->params[i].val_len);
        }
        return m;
    }

    /* Raw header values parsed during request parsing */
    std::string header_content_type() const {
        return req_->content_type
            ? std::string(req_->content_type, req_->content_type_len) : "";
    }
    std::string header_authorization() const {
        return req_->authorization
            ? std::string(req_->authorization, req_->authorization_len) : "";
    }
    std::string header_host() const {
        return req_->host ? std::string(req_->host, req_->host_len) : "";
    }

    /* Raw request body as a string */
    std::string body_text() const {
        return req_->body ? std::string(req_->body, req_->body_len) : "";
    }

    /* Parse request body as JSON.
       Throws std::runtime_error if body is not valid JSON.
       Use body_json_safe() to avoid throwing. */
    Json body_json() const {
        if (!req_->body || req_->body_len == 0)
            return Json::try_parse("{}");
        return Json::parse(req_->body, req_->body_len);
    }

    /* Try-parse — returns null Json on bad JSON instead of throwing */
    Json body_json_safe() const noexcept {
        if (!req_->body || req_->body_len == 0) return Json{};
        return Json::try_parse(std::string(req_->body, req_->body_len));
    }

    int content_length() const { return req_->content_length; }

    /* ── Response builder ──────────────────────────────────────────────── */

    /* Set HTTP status code (chainable) */
    Context &status(int code) { status_ = code; return *this; }

    /* Send a JSON response */
    void json(const Json &j) {
        content_type_ = "application/json";
        body_         = j.dump();
        sent_         = true;
    }
    void json(const std::string &raw_json) {
        content_type_ = "application/json";
        body_         = raw_json;
        sent_         = true;
    }

    /* Shorthand: send {"error": msg} */
    void json_error(const std::string &msg, int code = 0) {
        if (code) status_ = code;
        content_type_ = "application/json";
        body_  = "{\"error\":\"";
        body_ += msg;
        body_ += "\"}";
        sent_ = true;
    }

    /* Send a plain-text response */
    void text(const std::string &body) {
        content_type_ = "text/plain";
        body_         = body;
        sent_         = true;
    }

    /* Send with explicit content-type */
    void send(const std::string &body,
              const std::string &ct = "text/plain") {
        content_type_ = ct;
        body_         = body;
        sent_         = true;
    }

    /* Respond with no body */
    void end(int code = 200) {
        status_ = code;
        body_.clear();
        sent_ = true;
    }

    /* ── Inspection (for middleware) ───────────────────────────────────── */
    bool     is_sent()     const { return sent_; }
    int      status_code() const { return status_; }
    const std::string &body()         const { return body_; }
    const std::string &content_type() const { return content_type_; }

    /* Raw C pointers — for middleware that needs them (e.g. proxy) */
    http_req_t  *raw_req()  const { return req_; }
    http_resp_t *raw_resp() const { return res_; }

    /* ── Internal — used by App::dispatch ──────────────────────────────── */
    explicit Context(http_req_t *req, http_resp_t *res) noexcept
        : req_(req), res_(res) {}

    /* After handler chain: commit body_ into resp_body buffer */
    void commit(char *body_buf, int buf_cap) {
        int len = (int)body_.size();
        if (len > buf_cap - 1) len = buf_cap - 1;
        memcpy(body_buf, body_.data(), len);
        body_buf[len] = '\0';
        res_->status    = status_;
        res_->body_ptr  = body_buf;
        res_->body_len  = len;
        res_->is_static = 0;
    }

private:
    http_req_t  *req_;
    http_resp_t *res_;
    int          status_       = 200;
    bool         sent_         = false;
    std::string  body_;
    std::string  content_type_ = "application/json";
};

} // namespace http