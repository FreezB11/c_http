#include <http/http.h>
#include <http/utils.h>
#include <http/worker.h>

#include <stdio.h>
#include <string.h>

// helper
SIV setup_socket(int fd){
    int flags = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &flags, sizeof(flags));
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &flags, sizeof(flags));

    // larger buffer
    int buff_size = 524288; // 512KB
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buff_size, sizeof(buff_size));
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buff_size, sizeof(buff_size));

    // quick ACK
    setsockopt(fd, IPPROTO_TCP, TCP_QUICKACK, &flags, sizeof(flags));

    fcntl(fd, F_SETFL, O_NONBLOCK);
}

http* CreateServer(){
    signal(SIGPIPE, SIG_IGN);
    int threads = THREAD_COUNT;
    
    printf("\n");
    printf("╔════════════════════════════════════════════╗\n");
    printf("║  🚀 HYPER-OPTIMIZED C HTTP SERVER         ║\n");
    printf("╠════════════════════════════════════════════╣\n");
    printf("║  📡 Port: 8080                            ║\n");
    printf("║  🔥 Threads: %-2d                           ║\n", threads);
    printf("╚════════════════════════════════════════════╝\n");
    printf("\n📊 Benchmark:\n");
    printf("   wrk -t12 -c400 -d30s http://localhost:8080/ping\n");
    printf("   wrk -t12 -c400 -d30s http://localhost:8080/echo?key=value\n\n");
    fflush(stdout);

    conn_pool = mmap(NULL, sizeof(conn_ctx_t) * CONN_POOL_SIZE,
                     PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (conn_pool == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }
    pthread_t thread_pool[threads];
    for (int i = 0; i < threads; i++) {
        pthread_create(&thread_pool[i], NULL, worker_thread, (void *)(intptr_t)i);
    }

    // for (int i = 0; i < threads; i++) {
    //     pthread_join(thread_pool[i], NULL);
    // }

    // munmap(conn_pool, sizeof(conn_ctx_t) * CONN_POOL_SIZE);
    http *dummy = (http*)1; // non null ptr
    return dummy;
}