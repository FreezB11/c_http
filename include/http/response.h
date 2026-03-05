#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include <http/utils.h>

typedef struct {
    int         status;
    const char *body_ptr;
    int         body_len;
    int         is_static;   /* 1 = body_ptr is a persistent string literal */
} http_resp_t;

/* Pre-built static responses — defined in response.c */
extern const char RESP_PING[];
extern const int  RESP_PING_LEN;
extern const char RESP_404[];
extern const int  RESP_404_LEN;
extern const char RESP_500[];
extern const int  RESP_500_LEN;

#ifdef __cplusplus
}
#endif