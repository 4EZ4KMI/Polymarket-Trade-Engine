#include "util/pt_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *skip_ws_(const char *p, const char *end)
{
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
    return p;
}

static const char *find_matching_(const char *p, const char *end, char open_c, char close_c)
{
    int depth = 0;
    int in_str = 0;
    while (p < end) {
        if (*p == '"' && (p == end || *(p - 1) != '\\')) {
            in_str = !in_str;
        } else if (!in_str) {
            if (*p == open_c) depth++;
            else if (*p == close_c) {
                depth--;
                if (depth == 0) return p;
            }
        }
        p++;
    }
    return end;
}

int pt_json_get(const char *json, size_t len, const char *key, pt_json_tok_t *val)
{
    if (json == NULL || key == NULL || val == NULL) return 0;
    const char *p = json;
    const char *end = json + len;
    size_t klen = strlen(key);

    while (p < end) {
        p = skip_ws_(p, end);
        if (p >= end) break;
        if (*p == '"') {
            const char *ks = p + 1;
            const char *ke = memchr(ks, '"', end - ks);
            if (!ke) break;
            size_t match_len = (size_t)(ke - ks);
            p = skip_ws_(ke + 1, end);
            if (p < end && *p == ':') {
                p = skip_ws_(p + 1, end);
                int is_match = (match_len == klen && strncmp(ks, key, klen) == 0);
                const char *vs = p;
                const char *ve = p;
                pt_json_type_t t = PT_JSON_NONE;

                if (*p == '"') {
                    t = PT_JSON_STRING;
                    vs = p + 1;
                    ve = memchr(vs, '"', end - vs);
                    if (!ve) break;
                    p = ve + 1;
                } else if (*p == '{') {
                    t = PT_JSON_OBJECT;
                    ve = find_matching_(p, end, '{', '}');
                    if (ve < end) ve++;
                    p = ve;
                } else if (*p == '[') {
                    t = PT_JSON_ARRAY;
                    ve = find_matching_(p, end, '[', ']');
                    if (ve < end) ve++;
                    p = ve;
                } else {
                    t = PT_JSON_NUMBER;
                    while (p < end && *p != ',' && *p != '}' && *p != ']' &&
                           *p != ' ' && *p != '\n' && *p != '\r' && *p != '\t') p++;
                    ve = p;
                }

                if (is_match) {
                    val->type = t;
                    val->p    = vs;
                    val->len  = (size_t)(ve - vs);
                    return 1;
                }
            }
        } else {
            p++;
        }
    }
    return 0;
}

int pt_json_tok_int64(const pt_json_tok_t *tok, int64_t *out)
{
    if (!tok || !tok->p || tok->len == 0 || !out) return 0;
    char buf[64];
    size_t n = tok->len < sizeof(buf) - 1 ? tok->len : sizeof(buf) - 1;
    memcpy(buf, tok->p, n);
    buf[n] = '\0';
    char *endptr = NULL;
    *out = strtoll(buf, &endptr, 10);
    return endptr != buf;
}

int pt_json_tok_double(const pt_json_tok_t *tok, double *out)
{
    if (!tok || !tok->p || tok->len == 0 || !out) return 0;
    char buf[64];
    size_t n = tok->len < sizeof(buf) - 1 ? tok->len : sizeof(buf) - 1;
    memcpy(buf, tok->p, n);
    buf[n] = '\0';
    char *endptr = NULL;
    *out = strtod(buf, &endptr);
    return endptr != buf;
}

size_t pt_json_tok_str(const pt_json_tok_t *tok, char *dst, size_t dst_max)
{
    if (!tok || !tok->p || !dst || dst_max == 0) return 0;
    size_t n = tok->len < dst_max - 1 ? tok->len : dst_max - 1;
    memcpy(dst, tok->p, n);
    dst[n] = '\0';
    return n;
}

const char *pt_json_array_next(const char *arr_start, size_t arr_len,
                               const char *iter, pt_json_tok_t *elem)
{
    if (!arr_start || arr_len == 0 || !elem) return NULL;
    const char *end = arr_start + arr_len;
    const char *p = iter ? iter : arr_start;
    p = skip_ws_(p, end);
    if (p < end && *p == '[') p = skip_ws_(p + 1, end);
    if (p < end && *p == ',') p = skip_ws_(p + 1, end);
    if (p >= end || *p == ']') return NULL;

    const char *vs = p;
    const char *ve = p;
    pt_json_type_t t = PT_JSON_NONE;

    if (*p == '"') {
        t = PT_JSON_STRING;
        vs = p + 1;
        ve = memchr(vs, '"', end - vs);
        if (!ve) return NULL;
        p = ve + 1;
    } else if (*p == '{') {
        t = PT_JSON_OBJECT;
        ve = find_matching_(p, end, '{', '}');
        if (ve < end) ve++;
        p = ve;
    } else if (*p == '[') {
        t = PT_JSON_ARRAY;
        ve = find_matching_(p, end, '[', ']');
        if (ve < end) ve++;
        p = ve;
    } else {
        t = PT_JSON_NUMBER;
        while (p < end && *p != ',' && *p != ']') p++;
        ve = p;
    }

    elem->type = t;
    elem->p    = vs;
    elem->len  = (size_t)(ve - vs);
    return p;
}