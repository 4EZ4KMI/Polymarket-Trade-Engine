#ifndef PMT_PT_JSON_H
#define PMT_PT_JSON_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Zero-copy micro JSON parser.
 * Works on read-only buffers; tokenizes keys and scalar values.
 */

typedef enum {
    PT_JSON_NONE = 0,
    PT_JSON_OBJECT,
    PT_JSON_ARRAY,
    PT_JSON_STRING,
    PT_JSON_NUMBER,
    PT_JSON_BOOL,
    PT_JSON_NULL
} pt_json_type_t;

typedef struct {
    pt_json_type_t type;
    const char    *p;
    size_t         len;
} pt_json_tok_t;

/* Find key in JSON object text (at top level of obj).
 * Returns 1 if found and fills *val, 0 if not found. */
int pt_json_get(const char *json, size_t len, const char *key, pt_json_tok_t *val);

/* Parse numeric values from a token */
int      pt_json_tok_int64(const pt_json_tok_t *tok, int64_t *out);
int      pt_json_tok_double(const pt_json_tok_t *tok, double *out);
/* copy string (unquoted) to dst */
size_t   pt_json_tok_str(const pt_json_tok_t *tok, char *dst, size_t dst_max);

/* Iterate over array elements.
 * Start with iter=NULL. Returns next token pointer, fills *elem. */
const char *pt_json_array_next(const char *arr_start, size_t arr_len,
                               const char *iter, pt_json_tok_t *elem);

#ifdef __cplusplus
}
#endif
#endif /* PMT_PT_JSON_H */