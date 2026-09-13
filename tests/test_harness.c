#include "test_harness.h"

int pt_g_checks = 0;
int pt_g_fails  = 0;

typedef struct pt_case {
    const char     *name;
    void          (*fn)(void);
    struct pt_case *next;
} pt_case_t;

static pt_case_t *s_head = NULL;
static pt_case_t *s_tail = NULL;

void pt_reg_case(const char *name, void (*fn)(void))
{
    pt_case_t *c = (pt_case_t *)calloc(1, sizeof(*c));
    if (c == NULL) return;
    c->name = name;
    c->fn   = fn;
    if (s_tail) s_tail->next = c; else s_head = c;
    s_tail = c;
}

int main(void)
{
    int fails = 0, passed = 0;
    for (pt_case_t *c = s_head; c; c = c->next) {
        int before = pt_g_fails;
        c->fn();
        if (pt_g_fails > before) {
            fails++;
            fprintf(stderr, "  [FAIL] %s\n", c->name);
        } else {
            passed++;
        }
    }
    printf("cases: %d passed, %d failed, %d assertions, %d assertion-failures\n",
           passed, fails, pt_g_checks, pt_g_fails);
    return fails ? 1 : 0;
}