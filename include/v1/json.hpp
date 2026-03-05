#pragma once
#include <http/json.h>
#include <string>
#include <vector>
#include <stdexcept>

namespace http {

/* ─────────────────────────────────────────────────────────────────────────
   Json — owning RAII wrapper around json_value_t.

   Build:
       auto j = Json::object()
                    .set("name", "alice")
                    .set("age",  30)
                    .set("ok",   true);

       auto arr = Json::array().push("a").push("b").push(42);

   Parse:
       auto body = Json::parse(ctx.body_text());
       std::string name = body["name"].str("anonymous");
       double age = body["age"].num(0);

   Serialize:
       std::string s = j.dump();   // {"name":"alice","age":30,"ok":true}

   Schema:
       auto schema = JsonSchema::compile(R"({"type":"object","required":["name"]})");
       std::string err;
       if (!schema.validate(body, err)) ctx.status(422).json_error(err);
   ───────────────────────────────────────────────────────────────────────── */
class Json {
    json_value_t *v_;
    bool          owned_;

    explicit Json(json_value_t *v, bool owned) noexcept
        : v_(v), owned_(owned) {}

public:
    /* ── Constructors ──────────────────────────────────────────────────── */
    Json() noexcept : v_(json_make_null()), owned_(true) {}

    explicit Json(std::nullptr_t) noexcept
        : v_(json_make_null()), owned_(true) {}

    explicit Json(bool b) noexcept
        : v_(json_make_bool(b ? 1 : 0)), owned_(true) {}

    /* Disambiguate int from bool */
    explicit Json(int n) noexcept
        : v_(json_make_int(n)), owned_(true) {}

    explicit Json(double n) noexcept
        : v_(json_make_number(n)), owned_(true) {}

    explicit Json(const char *s) noexcept
        : v_(json_make_cstr(s ? s : "")), owned_(true) {}

    explicit Json(const std::string &s) noexcept
        : v_(json_make_string(s.data(), (int)s.size())), owned_(true) {}

    /* Move */
    Json(Json &&o) noexcept : v_(o.v_), owned_(o.owned_) {
        o.v_ = nullptr; o.owned_ = false;
    }
    Json &operator=(Json &&o) noexcept {
        if (this != &o) {
            if (owned_ && v_) json_free(v_);
            v_ = o.v_; owned_ = o.owned_;
            o.v_ = nullptr; o.owned_ = false;
        }
        return *this;
    }

    /* No copy — use clone() explicitly */
    Json(const Json &)            = delete;
    Json &operator=(const Json &) = delete;

    ~Json() { if (owned_ && v_) json_free(v_); }

    /* ── Factories ─────────────────────────────────────────────────────── */
    static Json object() { return Json(json_make_object(), true); }
    static Json array()  { return Json(json_make_array(),  true); }

    static Json parse(const std::string &s) {
        json_value_t *v = json_parse(s.data(), (int)s.size());
        if (!v) throw std::runtime_error("JSON parse error");
        return Json(v, true);
    }
    static Json parse(const char *s, int len) {
        json_value_t *v = json_parse(s, len);
        if (!v) throw std::runtime_error("JSON parse error");
        return Json(v, true);
    }
    /* Try-parse — returns null Json on failure instead of throwing */
    static Json try_parse(const std::string &s) noexcept {
        json_value_t *v = json_parse(s.data(), (int)s.size());
        return v ? Json(v, true) : Json(json_make_null(), true);
    }

    /* Deep clone */
    Json clone() const { return Json(json_clone(v_), true); }

    /* ── Type queries ──────────────────────────────────────────────────── */
    bool is_null()   const { return !v_ || v_->type == JSON_NULL;   }
    bool is_bool()   const { return  v_ && v_->type == JSON_BOOL;   }
    bool is_number() const { return  v_ && v_->type == JSON_NUMBER; }
    bool is_string() const { return  v_ && v_->type == JSON_STRING; }
    bool is_array()  const { return  v_ && v_->type == JSON_ARRAY;  }
    bool is_object() const { return  v_ && v_->type == JSON_OBJECT; }

    /* ── Value access ──────────────────────────────────────────────────── */
    std::string str(const std::string &def = "") const {
        const char *s = json_str_val(v_, nullptr);
        return s ? std::string(s) : def;
    }
    double num(double def = 0.0) const { return json_num_val(v_, def); }
    int    integer(int def = 0)  const { return json_int_val(v_, def); }
    bool   boolean(bool def = false) const {
        return json_bool_val(v_, def ? 1 : 0) != 0;
    }
    int    size() const {
        if (!v_) return 0;
        if (v_->type == JSON_ARRAY)  return json_array_len(v_);
        if (v_->type == JSON_OBJECT) return json_object_len(v_);
        return 0;
    }
    bool has(const std::string &key) const {
        return v_ && json_has_key(v_, key.c_str());
    }

    /* ── Object access (returns cloned child) ──────────────────────────── */
    Json operator[](const std::string &key) const {
        json_value_t *child = json_get(v_, key.c_str());
        return child ? Json(json_clone(child), true) : Json();
    }
    Json operator[](const char *key) const {
        return (*this)[std::string(key)];
    }

    /* ── Array access (returns cloned child) ───────────────────────────── */
    Json operator[](int idx) const {
        json_value_t *child = json_at(v_, idx);
        return child ? Json(json_clone(child), true) : Json();
    }

    /* ── Fluent object builder ─────────────────────────────────────────── */
    /* Lvalue version (named variable): returns Json& */
    Json &set(const std::string &key, Json val) & {
        json_object_set(v_, key.c_str(), val.release());
        return *this;
    }
    /* Rvalue version (temporary / chain): returns Json&& so chains keep propagating */
    Json &&set(const std::string &key, Json val) && {
        json_object_set(v_, key.c_str(), val.release());
        return std::move(*this);
    }

    /* Convenience scalar overloads — delegate to the above */
    Json &set(const std::string &k, const char       *v) & { return set(k, Json(v)); }
    Json &set(const std::string &k, const std::string&v) & { return set(k, Json(v)); }
    Json &set(const std::string &k, int               v) & { return set(k, Json(v)); }
    Json &set(const std::string &k, double            v) & { return set(k, Json(v)); }
    Json &set(const std::string &k, bool              v) & { return set(k, Json(v)); }

    Json &&set(const std::string &k, const char       *v) && { return std::move(*this).set(k, Json(v)); }
    Json &&set(const std::string &k, const std::string&v) && { return std::move(*this).set(k, Json(v)); }
    Json &&set(const std::string &k, int               v) && { return std::move(*this).set(k, Json(v)); }
    Json &&set(const std::string &k, double            v) && { return std::move(*this).set(k, Json(v)); }
    Json &&set(const std::string &k, bool              v) && { return std::move(*this).set(k, Json(v)); }

    /* ── Fluent array builder ──────────────────────────────────────────── */
    Json &push(Json val) & {
        json_array_push(v_, val.release());
        return *this;
    }
    Json &&push(Json val) && {
        json_array_push(v_, val.release());
        return std::move(*this);
    }

    Json &push(const char       *v) & { return push(Json(v)); }
    Json &push(const std::string&v) & { return push(Json(v)); }
    Json &push(int               v) & { return push(Json(v)); }
    Json &push(double            v) & { return push(Json(v)); }
    Json &push(bool              v) & { return push(Json(v)); }

    Json &&push(const char       *v) && { return std::move(*this).push(Json(v)); }
    Json &&push(const std::string&v) && { return std::move(*this).push(Json(v)); }
    Json &&push(int               v) && { return std::move(*this).push(Json(v)); }
    Json &&push(double            v) && { return std::move(*this).push(Json(v)); }
    Json &&push(bool              v) && { return std::move(*this).push(Json(v)); }

    /* ── Serialize ─────────────────────────────────────────────────────── */
    std::string dump() const {
        if (!v_) return "null";
        char buf[65536];
        int n = json_stringify(v_, buf, sizeof(buf));
        if (n < 0) return "null";
        return std::string(buf, n);
    }
    /* Write into caller-supplied buffer — avoids heap alloc on hot path */
    int dump_into(char *buf, int cap) const {
        if (!v_) return -1;
        return json_stringify(v_, buf, cap);
    }

    /* ── Raw pointer access (for passing back to C layer) ──────────────── */
    json_value_t *raw()     const { return v_; }
    json_value_t *release() noexcept {
        owned_ = false;
        json_value_t *p = v_;
        v_ = nullptr;
        return p;
    }
};

/* ─────────────────────────────────────────────────────────────────────────
   JsonSchema — compiled schema for request validation
   ───────────────────────────────────────────────────────────────────────── */
class JsonSchema {
    json_schema_t *s_;
public:
    explicit JsonSchema(json_schema_t *s) noexcept : s_(s) {}
    JsonSchema(JsonSchema &&o) noexcept : s_(o.s_) { o.s_ = nullptr; }
    JsonSchema &operator=(JsonSchema &&o) noexcept {
        if (this != &o) { json_schema_free(s_); s_ = o.s_; o.s_ = nullptr; }
        return *this;
    }
    JsonSchema(const JsonSchema &)            = delete;
    JsonSchema &operator=(const JsonSchema &) = delete;
    ~JsonSchema() { json_schema_free(s_); }

    static JsonSchema compile(const std::string &schema_str) {
        auto j = Json::parse(schema_str);
        json_schema_t *s = json_schema_compile(j.raw());
        if (!s) throw std::runtime_error("Invalid JSON schema");
        return JsonSchema(s);
    }

    /* Returns true if valid. err is populated on failure. */
    bool validate(const Json &value, std::string &err) const {
        char buf[512];
        int ok = json_schema_validate(s_, value.raw(), buf, sizeof(buf));
        if (!ok) err = buf;
        return ok != 0;
    }
};

} // namespace http