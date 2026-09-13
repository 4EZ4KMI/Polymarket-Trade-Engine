#ifndef PMT_TEST_HARNESS_H
#define PMT_TEST_HARNESS_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int pt_g_checks;
extern int pt_g_fails;

void pt_reg_case(const char *name, void (*fn)(void));

#define PT_T(name) \
    static void pt_test_##name(void); \
    __attribute__((constructor)) static void _pt_reg_##name(void){ \
        pt_reg_case(#name, pt_test_##name); \
    } \
    static void pt_test_##name(void)

#define PT_ASSERT(cond) \
    do { \
        pt_g_checks++; \
        if (!(cond)) { \
            pt_g_fails++; \
            fprintf(stderr, "%s:%d ASSERT FAILED: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

#define PT_ASSERT_NEAR(a, b, eps) \
    do { \
        pt_g_checks++; \
        double _a = (double)(a), _b = (double)(b); \
        if (!(fabs(_a - _b) < (eps))) { \
            pt_g_fails++; \
            fprintf(stderr, "%s:%d ASSERT_NEAR FAILED: %.6f vs %.6f (eps %g)\n", \
                    __FILE__, __LINE__, _a, _b, (double)(eps)); \
        } \
    } while (0)

#ifdef __cplusplus
}
#endif
#endif /* PMT_TEST_HARNESS_H */