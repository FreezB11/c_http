#pragma once

#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>

/* ─────────────────────────────────────────────
   Types
   ───────────────────────────────────────────── */

typedef enum {
    JSON_NULL   = 0,
    JSON_BOOL   = 1,
    JSON_NUMBER = 2,
    JSON_STRING = 3,
    JSON_ARRAY  = 4,
    JSON_OBJECT = 5
} json_type_t;

typedef struct json_kv    json_kv_t;
typedef struct json_value json_value_t;

struct json_kv {
    char         *key;
    json_value_t *val;
};

struct json_value {
    json_type_t type;
    union {
        int    bool_val;
        double num_val;
        struct { char *ptr; int len; }           str;
        struct { json_value_t **items; int count; int cap; } arr;
        struct { json_kv_t    *items; int count; int cap; } obj;
    };
};

/* ─────────────────────────────────────────────
   Parse
   ───────────────────────────────────────────── */

/* Parse from buffer. Returns NULL on error. Caller owns result. */
json_value_t *json_parse(const char *s, int len);
json_value_t *json_parse_cstr(const char *s);

/* ─────────────────────────────────────────────
   Build
   ───────────────────────────────────────────── */

json_value_t *json_make_null(void);
json_value_t *json_make_bool(int b);
json_value_t *json_make_number(double n);
json_value_t *json_make_int(int n);
json_value_t *json_make_string(const char *s, int len);
json_value_t *json_make_cstr(const char *s);
json_value_t *json_make_object(void);
json_value_t *json_make_array(void);

/* Returns 0 on success, -1 on OOM. Takes ownership of val. */
int json_object_set(json_value_t *obj, const char *key, json_value_t *val);
int json_array_push(json_value_t *arr, json_value_t *val);

/* Deep clone */
json_value_t *json_clone(const json_value_t *v);

/* ─────────────────────────────────────────────
   Access
   ───────────────────────────────────────────── */

/* Returns NULL if not found / wrong type */
json_value_t *json_get(const json_value_t *obj, const char *key);
json_value_t *json_at(const json_value_t *arr, int idx);

double      json_num_val(const json_value_t *v, double def);
const char *json_str_val(const json_value_t *v, const char *def);
int         json_bool_val(const json_value_t *v, int def);
int         json_int_val(const json_value_t *v, int def);
int         json_is_null(const json_value_t *v);
int         json_array_len(const json_value_t *arr);
int         json_object_len(const json_value_t *obj);
int         json_has_key(const json_value_t *obj, const char *key);

/* ─────────────────────────────────────────────
   Stringify
   ───────────────────────────────────────────── */

/* Writes JSON into buf[cap]. Returns bytes written (excl. NUL) or -1. */
int json_stringify(const json_value_t *v, char *buf, int cap);

/* ─────────────────────────────────────────────
   Free
   ───────────────────────────────────────────── */

void json_free(json_value_t *v);

/* ─────────────────────────────────────────────
   Schema Validation
   ─────────────────────────────────────────────
   Supported subset of JSON Schema draft-07:
     type, required, properties, items,
     minimum, maximum, minLength, maxLength,
     minItems, maxItems, enum (string values)
   ───────────────────────────────────────────── */

typedef struct json_schema json_schema_t;

/* Compile a schema from a json_value_t. Returns NULL on invalid schema. */
json_schema_t *json_schema_compile(const json_value_t *schema_json);

/* Returns 1 if valid. On failure returns 0 and writes message to err[errlen]. */
int json_schema_validate(const json_schema_t *schema,
                         const json_value_t  *value,
                         char *err, int errlen);

void json_schema_free(json_schema_t *schema);

#ifdef __cplusplus
}
#endif