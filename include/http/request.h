#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char *method;
    int         method_len;
    const char *path;
    int         path_len;
    const char *body;
    int         body_len;
    int         content_length;

    /* query params: ?key=val&key2=val2 — max 16 */
    struct {
        const char *key; int key_len;
        const char *val; int val_len;
    } params[16];
    int param_count;

    /* path params: /user/:id — filled by C++ router, max 8 */
    struct {
        char key[32];
        char val[128];
    } path_params[8];
    int path_param_count;

    /* raw headers — just the ones we need */
    const char *content_type;
    int         content_type_len;
    const char *authorization;
    int         authorization_len;
    const char *host;
    int         host_len;
} http_req_t;

#ifdef __cplusplus
}
#endif