#include <v1/app.hpp>
#include <v1/middleware.hpp>
#include <v1/json.hpp>

/* ─── Handlers ────────────────────────────────────────────────────────── */

void ping_handler(http::Context &ctx) {
    ctx.json(http::Json::object()
        .set("status", "ok")
        .set("version", "1.0"));
}

/* ─── Schema defined once at startup ──────────────────────────────────── */
static const http::JsonSchema user_schema = http::JsonSchema::compile(R"({
    "type": "object",
    "required": ["name", "age"],
    "properties": {
        "name": { "type": "string", "minLength": 1, "maxLength": 64 },
        "age":  { "type": "number", "minimum": 0,  "maximum": 150  },
        "role": { "type": "string", "enum": ["admin", "user", "guest"] }
    }
})");

/* ─── Main ────────────────────────────────────────────────────────────── */

int main() {
    http::App app;

    /* ── Global middleware ─── */
    app.use(http::middleware::logger());
    app.use(http::middleware::cors());

    /* ── Routes ─── */

    app.get("/ping", ping_handler);

    /* Echo query params: GET /echo?name=alice&msg=hello */
    app.get("/echo", [](http::Context &ctx) {
        auto all = ctx.query_all();
        auto resp = http::Json::object();
        for (auto &[k, v] : all)
            resp.set(k, v);
        ctx.json(resp);
    });

    /* Path parameter: GET /user/42 */
    app.get("/user/:id", [](http::Context &ctx) {
        std::string id = ctx.param("id");
        ctx.json(http::Json::object()
            .set("id",   id)
            .set("path", ctx.path()));
    });

    /* Nested path params: GET /user/42/posts/7 */
    app.get("/user/:userId/posts/:postId", [](http::Context &ctx) {
        ctx.json(http::Json::object()
            .set("userId", ctx.param("userId"))
            .set("postId", ctx.param("postId")));
    });

    /* POST with body parsing + schema validation */
    app.post("/user", [](http::Context &ctx) {
        auto body = ctx.body_json_safe();
        if (body.is_null()) {
            ctx.status(400).json_error("invalid JSON body");
            return;
        }

        std::string err;
        if (!user_schema.validate(body, err)) {
            ctx.status(422).json_error(err);
            return;
        }

        ctx.status(201).json(http::Json::object()
            .set("created", body["name"].str())
            .set("age",     body["age"].integer()));
    });

    /* DELETE returning 204 No Content */
    app.del("/user/:id", [](http::Context &ctx) {
        ctx.end(204);
    });

    /* JSON array response */
    app.get("/items", [](http::Context &ctx) {
        auto arr = http::Json::array()
            .push(http::Json::object().set("id", 1).set("name", "Widget A"))
            .push(http::Json::object().set("id", 2).set("name", "Widget B"));
        ctx.json(arr);
    });

    // Proxy: forward /upstream/ to another server
    // Uncomment when you have a target server running:
    // app.use("/upstream", http::middleware::proxy("http://localhost:3001"));

    // Start
    app.threads(4).listen(8080);

    return 0;
}