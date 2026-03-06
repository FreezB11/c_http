/// @file: tinyURL.cc
/// Build: g++ -O2 -std=c++17 -Iinclude tinyURL.cc -L. -lhttp -lpthread -lm -o tinyURL
#include <v1/app.hpp>
#include <v1/middleware.hpp>
#include <v1/json.hpp>

#include <unordered_map>
#include <shared_mutex>
#include <optional>
#include <algorithm>
#include <chrono>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <mutex>

using namespace http;

/* ─────────────────────────────────────────────────────────────────────────
   Base62
   Alphabet: 0-9 A-Z a-z  (62 chars)
   Counter-based: each new URL gets the next base62 number as its code.
   This gives short, unique, deterministic codes with no collision.
   ───────────────────────────────────────────────────────────────────────── */
namespace base62 {

static constexpr char CHARS[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

std::string encode(uint64_t n) {
    if (n == 0) return "0";
    std::string result;
    while (n > 0) {
        result += CHARS[n % 62];
        n /= 62;
    }
    std::reverse(result.begin(), result.end());
    return result;
}

uint64_t decode(const std::string &s) {
    uint64_t result = 0;
    for (char c : s) {
        result *= 62;
        if (c >= '0' && c <= '9') result += c - '0';
        else if (c >= 'A' && c <= 'Z') result += c - 'A' + 10;
        else if (c >= 'a' && c <= 'z') result += c - 'a' + 36;
        else return 0; /* invalid char */
    }
    return result;
}

} // namespace base62

/* ─────────────────────────────────────────────────────────────────────────
   URL Store — thread-safe in-memory store
   ───────────────────────────────────────────────────────────────────────── */
struct URLEntry {
    std::string long_url;
    std::string short_code;
    std::string created_at;
    std::string last_accessed;
    uint64_t    hits = 0;
};

class URLStore {
public:
    /* Returns the short code. If custom_code is given, tries to use it. */
    std::string insert(const std::string &long_url,
                       const std::string &custom_code = "") {
        std::unique_lock lock(mu_);

        /* Deduplicate: same long_url → return existing code */
        auto it = by_long_.find(long_url);
        if (it != by_long_.end())
            return it->second;

        std::string code = custom_code.empty()
            ? base62::encode(++counter_)
            : custom_code;

        /* Custom code collision check */
        if (!custom_code.empty() && by_short_.count(code))
            return "";   /* signal conflict to caller */

        URLEntry e;
        e.long_url     = long_url;
        e.short_code   = code;
        e.created_at   = now_iso();
        e.last_accessed= "";
        e.hits         = 0;

        by_short_[code]    = e;
        by_long_[long_url] = code;
        return code;
    }

    /* Returns nullptr if not found */
    std::optional<URLEntry> get(const std::string &code) {
        std::shared_lock lock(mu_);
        auto it = by_short_.find(code);
        if (it == by_short_.end()) return std::nullopt;
        return it->second;
    }

    /* Record a hit — returns the long_url or "" */
    std::string hit(const std::string &code) {
        std::unique_lock lock(mu_);
        auto it = by_short_.find(code);
        if (it == by_short_.end()) return "";
        it->second.hits++;
        it->second.last_accessed = now_iso();
        return it->second.long_url;
    }

    bool remove(const std::string &code) {
        std::unique_lock lock(mu_);
        auto it = by_short_.find(code);
        if (it == by_short_.end()) return false;
        by_long_.erase(it->second.long_url);
        by_short_.erase(it);
        return true;
    }

    size_t total() {
        std::shared_lock lock(mu_);
        return by_short_.size();
    }

    /* Returns all entries sorted by creation order (by counter value) */
    std::vector<URLEntry> list() {
        std::shared_lock lock(mu_);
        std::vector<URLEntry> result;
        result.reserve(by_short_.size());
        for (auto &[code, entry] : by_short_)
            result.push_back(entry);
        std::sort(result.begin(), result.end(), [](const URLEntry &a, const URLEntry &b){
            return base62::decode(a.short_code) < base62::decode(b.short_code);
        });
        return result;
    }

    static std::string now_iso() {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t));
        return buf;
    }

private:
    std::shared_mutex                          mu_;
    std::unordered_map<std::string, URLEntry>  by_short_;
    std::unordered_map<std::string, std::string> by_long_;
    uint64_t                                   counter_ = 1000; /* start at "GI" */
};

static URLStore store;

/* ─────────────────────────────────────────────────────────────────────────
   Validation
   ───────────────────────────────────────────────────────────────────────── */
static const JsonSchema CREATE_SCHEMA = JsonSchema::compile(R"({
    "type": "object",
    "required": ["long_url"],
    "properties": {
        "long_url":   {"type": "string", "minLength": 7,  "maxLength": 2048},
        "short_code": {"type": "string", "minLength": 1,  "maxLength": 12}
    }
})");

static bool starts_with(const std::string &s, const std::string &prefix) {
    return s.size() >= prefix.size() &&
           s.compare(0, prefix.size(), prefix) == 0;
}

static bool is_valid_url(const std::string &url) {
    return starts_with(url, "http://") || starts_with(url, "https://");
}

static bool is_valid_code(const std::string &code) {
    if (code.empty() || code.size() > 12) return false;
    for (char c : code) {
        bool ok = (c >= '0' && c <= '9') ||
                  (c >= 'A' && c <= 'Z') ||
                  (c >= 'a' && c <= 'z');
        if (!ok) return false;
    }
    return true;
}

/* ─────────────────────────────────────────────────────────────────────────
   Handlers
   ───────────────────────────────────────────────────────────────────────── */

/* POST /
   Body: { "long_url": "https://...", "short_code": "optionalCustom" }
   Returns: { "short_code", "long_url", "short_url", "created_at" }           */
void create_url(Context &ctx) {
    auto body = ctx.body_json_safe();
    if (body.is_null()) {
        ctx.status(400).json_error("invalid or missing JSON body");
        return;
    }

    std::string schema_err;
    if (!CREATE_SCHEMA.validate(body, schema_err)) {
        ctx.status(422).json_error(schema_err);
        return;
    }

    std::string long_url = body["long_url"].str();
    if (!is_valid_url(long_url)) {
        ctx.status(422).json_error("long_url must start with http:// or https://");
        return;
    }

    std::string custom = body["short_code"].str("");
    if (!custom.empty() && !is_valid_code(custom)) {
        ctx.status(422).json_error("short_code must be 1-12 alphanumeric chars");
        return;
    }

    std::string code = store.insert(long_url, custom);
    if (code.empty()) {
        ctx.status(409).json_error("short_code already taken");
        return;
    }

    auto entry = store.get(code);
    std::string host = ctx.header_host();
    if (host.empty()) host = "localhost:8080";

    ctx.status(201).json(Json::object()
        .set("short_code", code)
        .set("short_url",  "http://" + host + "/" + code)
        .set("long_url",   long_url)
        .set("created_at", entry->created_at));
}

/* GET /:code
   Responds with a 302 redirect to the long URL.
   Returns JSON with location so curl can show it cleanly.                    */
void redirect_url(Context &ctx) {
    std::string code = ctx.param("code");
    std::string long_url = store.hit(code);

    if (long_url.empty()) {
        ctx.status(404).json_error("short code not found");
        return;
    }

    /* Real redirect — in a browser this would navigate.
       We return 302 + JSON so it's also curl-friendly.                       */
    ctx.status(302).json(Json::object()
        .set("location", long_url)
        .set("message",  "Redirecting..."));
}

/* GET /:code/stats
   Returns hit count, created_at, last_accessed for a code.                  */
void get_stats(Context &ctx) {
    std::string code = ctx.param("code");
    auto entry = store.get(code);

    if (!entry) {
        ctx.status(404).json_error("short code not found");
        return;
    }

    std::string host = ctx.header_host();
    if (host.empty()) host = "localhost:8080";

    ctx.json(Json::object()
        .set("short_code",    entry->short_code)
        .set("short_url",     "http://" + host + "/" + entry->short_code)
        .set("long_url",      entry->long_url)
        .set("hits",          (int)entry->hits)
        .set("created_at",    entry->created_at)
        .set("last_accessed", entry->last_accessed.empty()
                                  ? "never"
                                  : entry->last_accessed));
}

/* DELETE /:code  */
void delete_url(Context &ctx) {
    std::string code = ctx.param("code");
    if (store.remove(code))
        ctx.end(204);
    else
        ctx.status(404).json_error("short code not found");
}

/* GET /health  */
void health(Context &ctx) {
    ctx.json(Json::object()
        .set("status",     "ok")
        .set("total_urls", (int)store.total())
        .set("server",     "tinyURL v1.0"));
}

/* GET /list?limit=50&offset=0
   Returns paginated list of all shortened URLs.                              */
void list_urls(Context &ctx) {
    int limit  = std::stoi(ctx.query("limit",  "50"));
    int offset = std::stoi(ctx.query("offset", "0"));

    if (limit  < 1)   limit  = 1;
    if (limit  > 200) limit  = 200;
    if (offset < 0)   offset = 0;

    auto all  = store.list();
    int  total = (int)all.size();

    std::string host = ctx.header_host();
    if (host.empty()) host = "localhost:8080";

    auto urls = Json::array();
    for (int i = offset; i < std::min((int)all.size(), offset + limit); i++) {
        const auto &e = all[i];
        urls.push(Json::object()
            .set("short_code",    e.short_code)
            .set("short_url",     "http://" + host + "/" + e.short_code)
            .set("long_url",      e.long_url)
            .set("hits",          (int)e.hits)
            .set("created_at",    e.created_at)
            .set("last_accessed", e.last_accessed.empty() ? "never" : e.last_accessed));
    }

    ctx.json(Json::object()
        .set("total",  total)
        .set("offset", offset)
        .set("limit",  limit)
        .set("urls",   std::move(urls)));
}

/* ─────────────────────────────────────────────────────────────────────────
   Main
   ───────────────────────────────────────────────────────────────────────── */
int main() {
    App app;

    app.use(middleware::logger());
    app.use(middleware::cors());

    app.get("/health",        health);
    app.get("/list",          list_urls);
    app.post("/",             create_url);
    app.get("/:code",         redirect_url);
    app.get("/:code/stats",   get_stats);
    app.del("/:code",         delete_url);

    app.threads(8).listen(8080);
    return 0;
}