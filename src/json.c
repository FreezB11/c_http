#include <http/json.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ─────────────────────────────────────────────────────────────────────────
   Helpers
   ───────────────────────────────────────────────────────────────────────── */

static json_value_t *alloc_val(json_type_t type) {
    json_value_t *v = (json_value_t *)calloc(1, sizeof(*v));
    if (v) v->type = type;
    return v;
}

/* ─────────────────────────────────────────────────────────────────────────
   Builder
   ───────────────────────────────────────────────────────────────────────── */

json_value_t *json_make_null(void)        { return alloc_val(JSON_NULL); }
json_value_t *json_make_bool(int b)       { json_value_t *v = alloc_val(JSON_BOOL);   if(v) v->bool_val = b?1:0; return v; }
json_value_t *json_make_number(double n)  { json_value_t *v = alloc_val(JSON_NUMBER); if(v) v->num_val = n;       return v; }
json_value_t *json_make_int(int n)        { return json_make_number((double)n); }

json_value_t *json_make_string(const char *s, int len) {
    json_value_t *v = alloc_val(JSON_STRING);
    if (!v) return NULL;
    v->str.ptr = (char *)malloc(len + 1);
    if (!v->str.ptr) { free(v); return NULL; }
    memcpy(v->str.ptr, s, len);
    v->str.ptr[len] = '\0';
    v->str.len = len;
    return v;
}
json_value_t *json_make_cstr(const char *s) {
    return s ? json_make_string(s, (int)strlen(s)) : json_make_null();
}

json_value_t *json_make_object(void) { return alloc_val(JSON_OBJECT); }
json_value_t *json_make_array(void)  { return alloc_val(JSON_ARRAY);  }

int json_object_set(json_value_t *obj, const char *key, json_value_t *val) {
    if (!obj || obj->type != JSON_OBJECT || !key || !val) return -1;
    /* overwrite if key already exists */
    for (int i = 0; i < obj->obj.count; i++) {
        if (strcmp(obj->obj.items[i].key, key) == 0) {
            json_free(obj->obj.items[i].val);
            obj->obj.items[i].val = val;
            return 0;
        }
    }
    if (obj->obj.count >= obj->obj.cap) {
        int ncap = obj->obj.cap ? obj->obj.cap * 2 : 8;
        json_kv_t *items = (json_kv_t *)realloc(obj->obj.items,
                                                  sizeof(json_kv_t) * ncap);
        if (!items) return -1;
        obj->obj.items = items;
        obj->obj.cap   = ncap;
    }
    json_kv_t *kv = &obj->obj.items[obj->obj.count++];
    kv->key = (char *)malloc(strlen(key) + 1);
    if (!kv->key) { obj->obj.count--; return -1; }
    strcpy(kv->key, key);
    kv->val = val;
    return 0;
}

int json_array_push(json_value_t *arr, json_value_t *val) {
    if (!arr || arr->type != JSON_ARRAY || !val) return -1;
    if (arr->arr.count >= arr->arr.cap) {
        int ncap = arr->arr.cap ? arr->arr.cap * 2 : 8;
        json_value_t **items = (json_value_t **)realloc(arr->arr.items,
                                                          sizeof(json_value_t *) * ncap);
        if (!items) return -1;
        arr->arr.items = items;
        arr->arr.cap   = ncap;
    }
    arr->arr.items[arr->arr.count++] = val;
    return 0;
}

/* ─────────────────────────────────────────────────────────────────────────
   Clone
   ───────────────────────────────────────────────────────────────────────── */

json_value_t *json_clone(const json_value_t *v) {
    if (!v) return json_make_null();
    switch (v->type) {
        case JSON_NULL:   return json_make_null();
        case JSON_BOOL:   return json_make_bool(v->bool_val);
        case JSON_NUMBER: return json_make_number(v->num_val);
        case JSON_STRING: return json_make_string(v->str.ptr, v->str.len);
        case JSON_ARRAY: {
            json_value_t *a = json_make_array();
            for (int i = 0; i < v->arr.count; i++)
                json_array_push(a, json_clone(v->arr.items[i]));
            return a;
        }
        case JSON_OBJECT: {
            json_value_t *o = json_make_object();
            for (int i = 0; i < v->obj.count; i++)
                json_object_set(o, v->obj.items[i].key,
                                json_clone(v->obj.items[i].val));
            return o;
        }
    }
    return json_make_null();
}

/* ─────────────────────────────────────────────────────────────────────────
   Access
   ───────────────────────────────────────────────────────────────────────── */

json_value_t *json_get(const json_value_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT || !key) return NULL;
    for (int i = 0; i < obj->obj.count; i++) {
        if (strcmp(obj->obj.items[i].key, key) == 0)
            return obj->obj.items[i].val;
    }
    return NULL;
}

json_value_t *json_at(const json_value_t *arr, int idx) {
    if (!arr || arr->type != JSON_ARRAY) return NULL;
    if (idx < 0 || idx >= arr->arr.count) return NULL;
    return arr->arr.items[idx];
}

double json_num_val(const json_value_t *v, double def) {
    if (!v) return def;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_STRING) return atof(v->str.ptr);
    return def;
}
const char *json_str_val(const json_value_t *v, const char *def) {
    if (!v || v->type != JSON_STRING) return def;
    return v->str.ptr;
}
int json_bool_val(const json_value_t *v, int def) {
    if (!v) return def;
    if (v->type == JSON_BOOL)   return v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    return def;
}
int json_int_val(const json_value_t *v, int def) {
    return (int)json_num_val(v, (double)def);
}
int json_is_null(const json_value_t *v)    { return !v || v->type == JSON_NULL;  }
int json_array_len(const json_value_t *v)  { return (v && v->type == JSON_ARRAY)  ? v->arr.count : 0; }
int json_object_len(const json_value_t *v) { return (v && v->type == JSON_OBJECT) ? v->obj.count : 0; }
int json_has_key(const json_value_t *v, const char *key) { return json_get(v, key) != NULL; }

/* ─────────────────────────────────────────────────────────────────────────
   Parser
   ───────────────────────────────────────────────────────────────────────── */

typedef struct { const char *s; int pos; int len; } jp_t;

static void skip_ws(jp_t *p) {
    while (p->pos < p->len) {
        char c = p->s[p->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') p->pos++;
        else break;
    }
}

static json_value_t *parse_value(jp_t *p);

static json_value_t *parse_string_tok(jp_t *p) {
    /* p->pos points at '"' */
    p->pos++;
    int start  = p->pos;
    int escapes = 0;
    while (p->pos < p->len) {
        char c = p->s[p->pos];
        if (c == '\\') { p->pos += 2; escapes++; continue; }
        if (c == '"')  break;
        p->pos++;
    }
    if (p->pos >= p->len) return NULL;
    int raw = p->pos - start;
    p->pos++; /* skip closing '"' */

    char *buf = (char *)malloc(raw + 1);
    if (!buf) return NULL;
    int out = 0;
    const char *src = p->s + start;
    for (int i = 0; i < raw; ) {
        if (src[i] == '\\' && i + 1 < raw) {
            i++;
            switch (src[i]) {
                case '"':  buf[out++] = '"';  break;
                case '\\': buf[out++] = '\\'; break;
                case '/':  buf[out++] = '/';  break;
                case 'n':  buf[out++] = '\n'; break;
                case 'r':  buf[out++] = '\r'; break;
                case 't':  buf[out++] = '\t'; break;
                case 'b':  buf[out++] = '\b'; break;
                case 'f':  buf[out++] = '\f'; break;
                /* \uXXXX → simplified: emit '?' for non-ASCII */
                case 'u':
                    buf[out++] = '?';
                    i += 4; /* skip 4 hex digits */
                    break;
                default: buf[out++] = src[i]; break;
            }
            i++;
        } else {
            buf[out++] = src[i++];
        }
    }
    buf[out] = '\0';
    json_value_t *v = alloc_val(JSON_STRING);
    if (!v) { free(buf); return NULL; }
    v->str.ptr = buf;
    v->str.len = out;
    return v;
}

static json_value_t *parse_number_tok(jp_t *p) {
    int start = p->pos;
    if (p->pos < p->len && p->s[p->pos] == '-') p->pos++;
    while (p->pos < p->len && p->s[p->pos] >= '0' && p->s[p->pos] <= '9') p->pos++;
    if (p->pos < p->len && p->s[p->pos] == '.') {
        p->pos++;
        while (p->pos < p->len && p->s[p->pos] >= '0' && p->s[p->pos] <= '9') p->pos++;
    }
    if (p->pos < p->len && (p->s[p->pos]=='e' || p->s[p->pos]=='E')) {
        p->pos++;
        if (p->pos < p->len && (p->s[p->pos]=='+' || p->s[p->pos]=='-')) p->pos++;
        while (p->pos < p->len && p->s[p->pos] >= '0' && p->s[p->pos] <= '9') p->pos++;
    }
    int nlen = p->pos - start;
    if (nlen < 1 || nlen >= 64) return NULL;
    char tmp[64];
    memcpy(tmp, p->s + start, nlen);
    tmp[nlen] = '\0';
    return json_make_number(atof(tmp));
}

static json_value_t *parse_object_tok(jp_t *p) {
    p->pos++; /* skip '{' */
    json_value_t *obj = json_make_object();
    if (!obj) return NULL;
    skip_ws(p);
    if (p->pos < p->len && p->s[p->pos] == '}') { p->pos++; return obj; }

    while (p->pos < p->len) {
        skip_ws(p);
        if (p->pos >= p->len || p->s[p->pos] != '"') { json_free(obj); return NULL; }
        json_value_t *kv = parse_string_tok(p);
        if (!kv) { json_free(obj); return NULL; }
        skip_ws(p);
        if (p->pos >= p->len || p->s[p->pos] != ':') {
            json_free(kv); json_free(obj); return NULL;
        }
        p->pos++;
        skip_ws(p);
        json_value_t *val = parse_value(p);
        if (!val) { json_free(kv); json_free(obj); return NULL; }
        json_object_set(obj, kv->str.ptr, val);
        json_free(kv);
        skip_ws(p);
        if (p->pos < p->len && p->s[p->pos] == ',') { p->pos++; continue; }
        if (p->pos < p->len && p->s[p->pos] == '}') { p->pos++; break; }
        json_free(obj); return NULL;
    }
    return obj;
}

static json_value_t *parse_array_tok(jp_t *p) {
    p->pos++; /* skip '[' */
    json_value_t *arr = json_make_array();
    if (!arr) return NULL;
    skip_ws(p);
    if (p->pos < p->len && p->s[p->pos] == ']') { p->pos++; return arr; }

    while (p->pos < p->len) {
        skip_ws(p);
        json_value_t *val = parse_value(p);
        if (!val) { json_free(arr); return NULL; }
        json_array_push(arr, val);
        skip_ws(p);
        if (p->pos < p->len && p->s[p->pos] == ',') { p->pos++; continue; }
        if (p->pos < p->len && p->s[p->pos] == ']') { p->pos++; break; }
        json_free(arr); return NULL;
    }
    return arr;
}

static json_value_t *parse_value(jp_t *p) {
    skip_ws(p);
    if (p->pos >= p->len) return NULL;
    char c = p->s[p->pos];
    if (c == '"') return parse_string_tok(p);
    if (c == '{') return parse_object_tok(p);
    if (c == '[') return parse_array_tok(p);
    if (c == 't') {
        if (p->len - p->pos >= 4 && memcmp(p->s + p->pos, "true", 4) == 0) {
            p->pos += 4;
            return json_make_bool(1);
        }
        return NULL;
    }
    if (c == 'f') {
        if (p->len - p->pos >= 5 && memcmp(p->s + p->pos, "false", 5) == 0) {
            p->pos += 5;
            return json_make_bool(0);
        }
        return NULL;
    }
    if (c == 'n') {
        if (p->len - p->pos >= 4 && memcmp(p->s + p->pos, "null", 4) == 0) {
            p->pos += 4;
            return json_make_null();
        }
        return NULL;
    }
    if (c == '-' || (c >= '0' && c <= '9')) return parse_number_tok(p);
    return NULL;
}

json_value_t *json_parse(const char *s, int len) {
    if (!s || len <= 0) return NULL;
    jp_t p = { s, 0, len };
    return parse_value(&p);
}
json_value_t *json_parse_cstr(const char *s) {
    return s ? json_parse(s, (int)strlen(s)) : NULL;
}

/* ─────────────────────────────────────────────────────────────────────────
   Stringify
   ───────────────────────────────────────────────────────────────────────── */

static int str_str(const char *s, int slen, char *buf, int pos, int cap) {
    if (pos + 1 >= cap) return -1;
    buf[pos++] = '"';
    for (int i = 0; i < slen; i++) {
        unsigned char ch = (unsigned char)s[i];
        if (ch == '"' || ch == '\\') {
            if (pos + 2 >= cap) return -1;
            buf[pos++] = '\\'; buf[pos++] = (char)ch;
        } else if (ch == '\n') { if (pos+2>=cap) return -1; buf[pos++]='\\'; buf[pos++]='n'; }
        else if (ch == '\r')   { if (pos+2>=cap) return -1; buf[pos++]='\\'; buf[pos++]='r'; }
        else if (ch == '\t')   { if (pos+2>=cap) return -1; buf[pos++]='\\'; buf[pos++]='t'; }
        else { if (pos+1>=cap) return -1; buf[pos++]=(char)ch; }
    }
    if (pos + 1 >= cap) return -1;
    buf[pos++] = '"';
    return pos;
}

static int str_val(const json_value_t *v, char *buf, int pos, int cap) {
    if (pos < 0 || !v) {
        if (pos + 4 < cap) { memcpy(buf+pos,"null",4); return pos+4; }
        return -1;
    }
    switch (v->type) {
        case JSON_NULL:
            if (pos + 4 >= cap) return -1;
            memcpy(buf + pos, "null", 4); return pos + 4;
        case JSON_BOOL:
            if (v->bool_val) {
                if (pos + 4 >= cap) return -1;
                memcpy(buf + pos, "true", 4); return pos + 4;
            } else {
                if (pos + 5 >= cap) return -1;
                memcpy(buf + pos, "false", 5); return pos + 5;
            }
        case JSON_NUMBER: {
            char tmp[64];
            int n;
            double d = v->num_val;
            if (d == (long long)d && d >= -1e15 && d <= 1e15)
                n = snprintf(tmp, 64, "%lld", (long long)d);
            else
                n = snprintf(tmp, 64, "%.17g", d);
            if (pos + n >= cap) return -1;
            memcpy(buf + pos, tmp, n);
            return pos + n;
        }
        case JSON_STRING:
            return str_str(v->str.ptr, v->str.len, buf, pos, cap);
        case JSON_ARRAY: {
            if (pos + 1 >= cap) return -1;
            buf[pos++] = '[';
            for (int i = 0; i < v->arr.count; i++) {
                if (i > 0) { if (pos+1>=cap) return -1; buf[pos++]=','; }
                pos = str_val(v->arr.items[i], buf, pos, cap);
                if (pos < 0) return -1;
            }
            if (pos + 1 >= cap) return -1;
            buf[pos++] = ']';
            return pos;
        }
        case JSON_OBJECT: {
            if (pos + 1 >= cap) return -1;
            buf[pos++] = '{';
            for (int i = 0; i < v->obj.count; i++) {
                if (i > 0) { if (pos+1>=cap) return -1; buf[pos++]=','; }
                pos = str_str(v->obj.items[i].key,
                              (int)strlen(v->obj.items[i].key),
                              buf, pos, cap);
                if (pos < 0) return -1;
                if (pos + 1 >= cap) return -1;
                buf[pos++] = ':';
                pos = str_val(v->obj.items[i].val, buf, pos, cap);
                if (pos < 0) return -1;
            }
            if (pos + 1 >= cap) return -1;
            buf[pos++] = '}';
            return pos;
        }
    }
    return -1;
}

int json_stringify(const json_value_t *v, char *buf, int cap) {
    if (!buf || cap <= 1) return -1;
    int end = str_val(v, buf, 0, cap - 1);
    if (end < 0) return -1;
    buf[end] = '\0';
    return end;
}

/* ─────────────────────────────────────────────────────────────────────────
   Free
   ───────────────────────────────────────────────────────────────────────── */

void json_free(json_value_t *v) {
    if (!v) return;
    switch (v->type) {
        case JSON_STRING:
            free(v->str.ptr);
            break;
        case JSON_ARRAY:
            for (int i = 0; i < v->arr.count; i++)
                json_free(v->arr.items[i]);
            free(v->arr.items);
            break;
        case JSON_OBJECT:
            for (int i = 0; i < v->obj.count; i++) {
                free(v->obj.items[i].key);
                json_free(v->obj.items[i].val);
            }
            free(v->obj.items);
            break;
        default: break;
    }
    free(v);
}

/* ─────────────────────────────────────────────────────────────────────────
   Schema Validation
   ───────────────────────────────────────────────────────────────────────── */

#define SCHEMA_TYPE_NULL    (1<<0)
#define SCHEMA_TYPE_BOOL    (1<<1)
#define SCHEMA_TYPE_NUMBER  (1<<2)
#define SCHEMA_TYPE_STRING  (1<<3)
#define SCHEMA_TYPE_ARRAY   (1<<4)
#define SCHEMA_TYPE_OBJECT  (1<<5)
#define SCHEMA_TYPE_INTEGER (1<<6)
#define SCHEMA_TYPE_ANY     0x7F

struct json_schema {
    int    type_mask;
    /* string */
    int    min_length;  /* -1 = no constraint */
    int    max_length;
    /* number */
    double minimum;     int has_min;
    double maximum;     int has_max;
    /* array */
    int    min_items;
    int    max_items;
    struct json_schema *items_schema; /* for array items */
    /* object */
    char **required;
    int    required_count;
    struct { char *key; struct json_schema *schema; } *properties;
    int    properties_count;
    /* enum (string values only) */
    char **enum_vals;
    int    enum_count;
};

static int parse_type_mask(const char *s) {
    if (!s) return SCHEMA_TYPE_ANY;
    if (strcmp(s,"string")  == 0) return SCHEMA_TYPE_STRING;
    if (strcmp(s,"number")  == 0) return SCHEMA_TYPE_NUMBER;
    if (strcmp(s,"integer") == 0) return SCHEMA_TYPE_INTEGER|SCHEMA_TYPE_NUMBER;
    if (strcmp(s,"boolean") == 0) return SCHEMA_TYPE_BOOL;
    if (strcmp(s,"object")  == 0) return SCHEMA_TYPE_OBJECT;
    if (strcmp(s,"array")   == 0) return SCHEMA_TYPE_ARRAY;
    if (strcmp(s,"null")    == 0) return SCHEMA_TYPE_NULL;
    return SCHEMA_TYPE_ANY;
}

json_schema_t *json_schema_compile(const json_value_t *sj) {
    if (!sj || sj->type != JSON_OBJECT) return NULL;
    json_schema_t *s = (json_schema_t *)calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->type_mask  = SCHEMA_TYPE_ANY;
    s->min_length = -1;
    s->max_length = -1;
    s->min_items  = -1;
    s->max_items  = -1;

    const json_value_t *t = json_get(sj, "type");
    if (t && t->type == JSON_STRING)
        s->type_mask = parse_type_mask(t->str.ptr);

    const json_value_t *mn = json_get(sj, "minimum");
    if (mn && mn->type == JSON_NUMBER) { s->minimum = mn->num_val; s->has_min = 1; }

    const json_value_t *mx = json_get(sj, "maximum");
    if (mx && mx->type == JSON_NUMBER) { s->maximum = mx->num_val; s->has_max = 1; }

    const json_value_t *minl = json_get(sj, "minLength");
    if (minl) s->min_length = json_int_val(minl, -1);

    const json_value_t *maxl = json_get(sj, "maxLength");
    if (maxl) s->max_length = json_int_val(maxl, -1);

    const json_value_t *mini = json_get(sj, "minItems");
    if (mini) s->min_items = json_int_val(mini, -1);

    const json_value_t *maxi = json_get(sj, "maxItems");
    if (maxi) s->max_items = json_int_val(maxi, -1);

    /* items schema for arrays */
    const json_value_t *items = json_get(sj, "items");
    if (items && items->type == JSON_OBJECT)
        s->items_schema = json_schema_compile(items);

    /* required */
    const json_value_t *req = json_get(sj, "required");
    if (req && req->type == JSON_ARRAY && req->arr.count > 0) {
        s->required_count = req->arr.count;
        s->required = (char **)malloc(sizeof(char *) * req->arr.count);
        for (int i = 0; i < req->arr.count; i++) {
            const char *rk = json_str_val(req->arr.items[i], "");
            s->required[i] = (char *)malloc(strlen(rk) + 1);
            strcpy(s->required[i], rk);
        }
    }

    /* properties */
    const json_value_t *props = json_get(sj, "properties");
    if (props && props->type == JSON_OBJECT && props->obj.count > 0) {
        s->properties_count = props->obj.count;
        s->properties = calloc(props->obj.count, sizeof(*s->properties));
        for (int i = 0; i < props->obj.count; i++) {
            s->properties[i].key = (char *)malloc(strlen(props->obj.items[i].key) + 1);
            strcpy(s->properties[i].key, props->obj.items[i].key);
            s->properties[i].schema = json_schema_compile(props->obj.items[i].val);
        }
    }

    /* enum */
    const json_value_t *en = json_get(sj, "enum");
    if (en && en->type == JSON_ARRAY && en->arr.count > 0) {
        s->enum_count = en->arr.count;
        s->enum_vals = (char **)malloc(sizeof(char *) * en->arr.count);
        for (int i = 0; i < en->arr.count; i++) {
            const char *ev = json_str_val(en->arr.items[i], "");
            s->enum_vals[i] = (char *)malloc(strlen(ev) + 1);
            strcpy(s->enum_vals[i], ev);
        }
    }

    return s;
}

int json_schema_validate(const json_schema_t *s, const json_value_t *v,
                         char *err, int errlen) {
    if (!s || !v) return 1;

    /* type check */
    int vtype = 0;
    switch (v->type) {
        case JSON_NULL:   vtype = SCHEMA_TYPE_NULL;   break;
        case JSON_BOOL:   vtype = SCHEMA_TYPE_BOOL;   break;
        case JSON_NUMBER: vtype = SCHEMA_TYPE_NUMBER; break;
        case JSON_STRING: vtype = SCHEMA_TYPE_STRING; break;
        case JSON_ARRAY:  vtype = SCHEMA_TYPE_ARRAY;  break;
        case JSON_OBJECT: vtype = SCHEMA_TYPE_OBJECT; break;
    }
    if (s->type_mask != SCHEMA_TYPE_ANY && !(s->type_mask & vtype)) {
        snprintf(err, errlen, "type mismatch: expected type mask %d, got %d",
                 s->type_mask, vtype);
        return 0;
    }

    /* string constraints */
    if (v->type == JSON_STRING) {
        if (s->min_length >= 0 && v->str.len < s->min_length) {
            snprintf(err, errlen, "string too short (min %d)", s->min_length);
            return 0;
        }
        if (s->max_length >= 0 && v->str.len > s->max_length) {
            snprintf(err, errlen, "string too long (max %d)", s->max_length);
            return 0;
        }
        /* enum check */
        if (s->enum_count > 0) {
            int found = 0;
            for (int i = 0; i < s->enum_count; i++) {
                if (strcmp(v->str.ptr, s->enum_vals[i]) == 0) { found = 1; break; }
            }
            if (!found) {
                snprintf(err, errlen, "value not in enum");
                return 0;
            }
        }
    }

    /* number constraints */
    if (v->type == JSON_NUMBER) {
        if (s->has_min && v->num_val < s->minimum) {
            snprintf(err, errlen, "value %.17g below minimum %.17g",
                     v->num_val, s->minimum);
            return 0;
        }
        if (s->has_max && v->num_val > s->maximum) {
            snprintf(err, errlen, "value %.17g above maximum %.17g",
                     v->num_val, s->maximum);
            return 0;
        }
        if ((s->type_mask & SCHEMA_TYPE_INTEGER) && v->num_val != (long long)v->num_val) {
            snprintf(err, errlen, "expected integer, got float");
            return 0;
        }
    }

    /* array constraints */
    if (v->type == JSON_ARRAY) {
        if (s->min_items >= 0 && v->arr.count < s->min_items) {
            snprintf(err, errlen, "array too short (min %d)", s->min_items);
            return 0;
        }
        if (s->max_items >= 0 && v->arr.count > s->max_items) {
            snprintf(err, errlen, "array too long (max %d)", s->max_items);
            return 0;
        }
        if (s->items_schema) {
            for (int i = 0; i < v->arr.count; i++) {
                if (!json_schema_validate(s->items_schema, v->arr.items[i], err, errlen)) {
                    char tmp[512];
                    snprintf(tmp, sizeof(tmp), "item[%d]: %s", i, err);
                    snprintf(err, errlen, "%s", tmp);
                    return 0;
                }
            }
        }
    }

    /* object: required + properties */
    if (v->type == JSON_OBJECT) {
        for (int i = 0; i < s->required_count; i++) {
            if (!json_has_key(v, s->required[i])) {
                snprintf(err, errlen, "missing required field: %s", s->required[i]);
                return 0;
            }
        }
        for (int i = 0; i < s->properties_count; i++) {
            const json_value_t *child = json_get(v, s->properties[i].key);
            if (child && s->properties[i].schema) {
                if (!json_schema_validate(s->properties[i].schema, child, err, errlen)) {
                    char tmp[512];
                    snprintf(tmp, sizeof(tmp), "%s: %s", s->properties[i].key, err);
                    snprintf(err, errlen, "%s", tmp);
                    return 0;
                }
            }
        }
    }

    return 1;
}

void json_schema_free(json_schema_t *s) {
    if (!s) return;
    json_schema_free(s->items_schema);
    for (int i = 0; i < s->required_count; i++) free(s->required[i]);
    free(s->required);
    for (int i = 0; i < s->properties_count; i++) {
        free(s->properties[i].key);
        json_schema_free(s->properties[i].schema);
    }
    free(s->properties);
    for (int i = 0; i < s->enum_count; i++) free(s->enum_vals[i]);
    free(s->enum_vals);
    free(s);
}