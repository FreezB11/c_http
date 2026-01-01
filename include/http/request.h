#pragma once

typedef struct {
    const char *method;
    int method_len;
    const char *path;
    int path_len;
    const char *body;
    int body_len;
    // we shall content-type
    int content_length;
    struct {
        const char *key;
        int key_len;
        const char *val;
        int val_len;
    } params[8];
    int param_count;
} http_req_t;
