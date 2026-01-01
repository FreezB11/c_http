#include <http/worker.h>
#include <http/http.h>

#include <stdio.h>
#include <string.h>

void *worker_thread(void *arg) {
    int cpu = (intptr_t)arg;
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu % sysconf(_SC_NPROCESSORS_ONLN), &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

    int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd < 0) {
        perror("epoll_create1");
        return NULL;
    }

    // Create listening socket with SO_REUSEPORT
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return NULL;
    }
    
    if (listen(listen_fd, 4096) < 0) {
        perror("listen");
        return NULL;
    }
    
    fcntl(listen_fd, F_SETFL, O_NONBLOCK);

    struct epoll_event ev, events[MAX_EVENTS];
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

    printf("⚡ Worker %d ready on CPU %d\n", cpu, cpu % sysconf(_SC_NPROCESSORS_ONLN));

    while (1) {
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        
        for (int i = 0; i < nfds; i++) {
            int fd = events[i].data.fd;
            
            if (fd == listen_fd) {
                // Accept all pending connections
                while (1) {
                    struct sockaddr_in cli;
                    socklen_t len = sizeof(cli);
                    int cfd = accept4(listen_fd, (struct sockaddr *)&cli, &len, SOCK_NONBLOCK);
                    if (cfd < 0) break;
                    
                    setup_socket(cfd);
                    
                    conn_ctx_t *ctx = alloc_conn();
                    ctx->fd = cfd;
                    ctx->state = CONN_READING;
                    
                    ev.events = EPOLLIN | EPOLLOUT | EPOLLONESHOT;
                    ev.data.ptr = ctx;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cfd, &ev);
                }
            } else {
                conn_ctx_t *ctx = (conn_ctx_t *)events[i].data.ptr;
                
                if ((events[i].events & EPOLLIN) && ctx->state == CONN_READING) {
                    handle_readable(ctx);
                }
                if ((events[i].events & EPOLLOUT) && ctx->state == CONN_WRITING) {
                    handle_writable(ctx);
                }
                
                if (ctx->state == CONN_CLOSE) {
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, ctx->fd, NULL);
                    close(ctx->fd);
                    continue;
                }
                
                // Re-arm
                ev.events = ((ctx->state == CONN_READING) ? EPOLLIN : EPOLLOUT) | EPOLLONESHOT;
                ev.data.ptr = ctx;
                epoll_ctl(epoll_fd, EPOLL_CTL_MOD, ctx->fd, &ev);
            }
        }
    }
    
    return NULL;
}