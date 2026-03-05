#include <http/http.h>
#include <http/worker.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

/* ── Global pool — declared extern in http.h ─────────────────────────── */
conn_ctx_t      *conn_pool  = NULL;
pthread_mutex_t  pool_mutex = PTHREAD_MUTEX_INITIALIZER;
int              server_port = 8080;

static volatile int g_running = 1;
static void sig_handler(int sig) { (void)sig; g_running = 0; }

void setup_socket(int fd) {
    int flags = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,   &flags, sizeof(flags));
    setsockopt(fd, SOL_SOCKET,  SO_KEEPALIVE,  &flags, sizeof(flags));
    setsockopt(fd, IPPROTO_TCP, TCP_QUICKACK,  &flags, sizeof(flags));

    int buf = 524288; /* 512 KB */
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buf, sizeof(buf));
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buf, sizeof(buf));

    fcntl(fd, F_SETFL, O_NONBLOCK);
}

void run_server(int port, int thread_count) {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);

    server_port = port;

    printf("\n");
    printf("╔═══════════════════════════════════════════╗\n");
    printf("║       C/C++ HTTP Library  v1.0            ║\n");
    printf("╠═══════════════════════════════════════════╣\n");
    printf("║  Port    : %-5d                          ║\n", port);
    printf("║  Threads : %-2d                             ║\n", thread_count);
    printf("╚═══════════════════════════════════════════╝\n\n");
    fflush(stdout);

    conn_pool = (conn_ctx_t *)mmap(NULL,
                    sizeof(conn_ctx_t) * CONN_POOL_SIZE,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (conn_pool == MAP_FAILED) {
        perror("mmap conn_pool");
        return;
    }

    pthread_t threads[thread_count];
    for (int i = 0; i < thread_count; i++) {
        if (pthread_create(&threads[i], NULL, worker_thread,
                           (void *)(intptr_t)i) != 0) {
            perror("pthread_create");
            return;
        }
    }

    printf("Listening on http://0.0.0.0:%d\n\n", port);
    fflush(stdout);

    /* Block until SIGINT/SIGTERM */
    for (int i = 0; i < thread_count; i++)
        pthread_join(threads[i], NULL);

    munmap(conn_pool, sizeof(conn_ctx_t) * CONN_POOL_SIZE);
}