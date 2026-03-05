#include <v1/app.hpp>
#include <http/http.h>
#include <http/route.h>
#include <http/conn.h>

namespace http {

/* ─────────────────────────────────────────────────────────────────────────
   Global singleton pointer — the C bridge calls into it.
   ───────────────────────────────────────────────────────────────────────── */
static App *g_app = nullptr;

static void bridge_dispatcher(http_req_t *req, http_resp_t *res) {
    if (g_app) g_app->dispatch(req, res);
}

/* ─────────────────────────────────────────────────────────────────────────
   App::dispatch
   Called on every request from the C layer (worker thread context).
   ───────────────────────────────────────────────────────────────────────── */
void App::dispatch(http_req_t *req, http_resp_t *res) {
    Context ctx(req, res);

    try {
        router_.dispatch(ctx);
    } catch (const std::exception &e) {
        ctx.status(500).json_error(e.what());
    } catch (...) {
        ctx.status(500).json_error("Internal Server Error");
    }

    if (!ctx.is_sent()) {
        ctx.status(404).json_error("Not Found");
    }

    /* Find the conn_ctx_t that owns this req/res pair so we can use its
       resp_body buffer.  The C layer passes us req_t and resp_t directly;
       we recover the container via offsetof arithmetic.  */

    /* Actually simpler: we use a thread-local body buffer.
       Each worker thread handles requests serially, so this is safe. */
    static __thread char tl_body[RESP_BUFFER_SIZE];
    ctx.commit(tl_body, RESP_BUFFER_SIZE);
}

/* ─────────────────────────────────────────────────────────────────────────
   App::listen — registers the bridge and starts the server
   ───────────────────────────────────────────────────────────────────────── */
void App::listen(int port) {
    g_app = this;
    set_cpp_dispatcher(bridge_dispatcher);
    run_server(port, threads_);
    g_app = nullptr;
}

} // namespace http