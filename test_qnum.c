#include "qnum.h"
#include <stdio.h>
#include <math.h>

static int failures = 0;

#define CHECK(label, got, expected, tol) do { \
    double _g = (double)(got), _e = (double)(expected); \
    double _err = fabs(_g - _e); \
    double _rel = (_e != 0.0) ? _err / fabs(_e) : _err; \
    if (_rel > (tol)) { \
        printf("FAIL %-40s got=%.10g expected=%.10g rel=%.2e\n", \
               label, _g, _e, _rel); \
        failures++; \
    } else { \
        printf("pass %-40s got=%.10g\n", label, _g); \
    } \
} while(0)

#define CHECK_EXACT(label, got_q, exp_int, exp_frac) do { \
    if ((got_q).integer != (exp_int) || (got_q).fraction != (uint32_t)(exp_frac)) { \
        printf("FAIL %-40s got=(%d,0x%08X) expected=(%d,0x%08X)\n", \
               label, (got_q).integer, (got_q).fraction, exp_int, (uint32_t)(exp_frac)); \
        failures++; \
    } else { \
        printf("pass %-40s\n", label); \
    } \
} while(0)

int main(void) {
    printf("=== Q4.28 tier ===\n");
    CHECK("q_mul 1.5*0.5",     q_to_float(q_mul(float_to_q(1.5),float_to_q(0.5))), 0.75, 1e-9);
    CHECK("q_neg(INT32_MIN)",  (double)q_neg(INT32_MIN), (double)INT32_MAX, 0);
    CHECK("q_abs(INT32_MIN)",  (double)q_abs(INT32_MIN), (double)INT32_MAX, 0);
    CHECK("q_sin(0.25turn)",   q_to_float(q_sin(Q_ONE>>2)), 1.0, 1e-7);
    CHECK("q_cos(0)",          q_to_float(q_cos(0)), 1.0, 1e-7);
    CHECK("float_to_q clamp+", (double)float_to_q(9.0), (double)INT32_MAX, 0);
    CHECK("float_to_q clamp-", (double)float_to_q(-9.0), (double)INT32_MIN, 0);

    printf("\n=== Q32.32 arithmetic ===\n");
    CHECK("add 1+0.5",     qnum_to_double(qnum_add(QNUM_ONE, float_to_qnum(0.5))), 1.5, 1e-10);
    CHECK("sub 2-0.75",    qnum_to_double(qnum_sub(float_to_qnum(2),float_to_qnum(0.75))), 1.25, 1e-10);
    CHECK("mul 3*4",       qnum_to_double(qnum_mul(float_to_qnum(3),float_to_qnum(4))), 12.0, 1e-10);
    CHECK("div 355/113",   qnum_to_double(qnum_div(float_to_qnum(355),float_to_qnum(113))), 355.0/113.0, 1e-9);
    CHECK("recip 7",       qnum_to_double(qnum_recip(float_to_qnum(7))), 1.0/7, 1e-9);
    CHECK("mul QNUM_MIN*1",qnum_to_double(qnum_mul(QNUM_MIN,QNUM_ONE)), -2147483648.0, 1e-10);
    CHECK("div 5/0 sat",   qnum_to_double(qnum_div(float_to_qnum(5),QNUM_ZERO)), qnum_to_double(QNUM_MAX), 0);
    CHECK("div -5/0 sat",  qnum_to_double(qnum_div(float_to_qnum(-5),QNUM_ZERO)), qnum_to_double(QNUM_MIN), 0);

    printf("\n=== Q32.32 comparison ===\n");
    CHECK("lt 1<2",   (double)qnum_lt(QNUM_ONE,float_to_qnum(2)), 1.0, 0);
    CHECK("gt 2>1",   (double)qnum_gt(float_to_qnum(2),QNUM_ONE), 1.0, 0);
    CHECK("eq 1==1",  (double)qnum_eq(QNUM_ONE,QNUM_ONE), 1.0, 0);
    CHECK("ne 1!=2",  (double)qnum_ne(QNUM_ONE,float_to_qnum(2)), 1.0, 0);
    CHECK("lt neg",   (double)qnum_lt(QNUM_MIN,QNUM_ZERO), 1.0, 0);
    CHECK("min",      qnum_to_double(qnum_min(float_to_qnum(3),float_to_qnum(7))), 3.0, 0);
    CHECK("max",      qnum_to_double(qnum_max(float_to_qnum(3),float_to_qnum(7))), 7.0, 0);

    printf("\n=== Q32.32 rounding ===\n");
    CHECK_EXACT("floor  1.75", qnum_floor(float_to_qnum(1.75)),   1, 0);
    CHECK_EXACT("floor -1.25", qnum_floor(float_to_qnum(-1.25)), -2, 0);
    CHECK_EXACT("ceil   1.25", qnum_ceil(float_to_qnum(1.25)),    2, 0);
    CHECK_EXACT("ceil  -1.75", qnum_ceil(float_to_qnum(-1.75)),  -1, 0);
    CHECK_EXACT("ceil   2.0",  qnum_ceil(float_to_qnum(2.0)),     2, 0);
    CHECK_EXACT("round  1.4",  qnum_round(float_to_qnum(1.4)),    1, 0);
    CHECK_EXACT("round  1.5",  qnum_round(float_to_qnum(1.5)),    2, 0);
    CHECK_EXACT("round -0.5",  qnum_round(float_to_qnum(-0.5)),   0, 0);

    printf("\n=== Cross-tier bridge ===\n");
    CHECK("from_q4_28 1.5",   qnum_to_double(qnum_from_q4_28(float_to_q(1.5))), 1.5, 1e-8);
    CHECK("from_q4_28 -3.75", qnum_to_double(qnum_from_q4_28(float_to_q(-3.75))), -3.75, 1e-8);
    CHECK("to_q4_28_raw 1.5", q_to_float(qnum_to_q4_28_raw(float_to_qnum(1.5))), 1.5, 1e-8);
    CHECK("to_q4_28_raw -8",  q_to_float(qnum_to_q4_28_raw(float_to_qnum(-8.0))), -8.0, 1e-9);
    CHECK("to_q4_28 sat >7",  (double)qnum_to_q4_28_raw(float_to_qnum(100.0)), (double)INT32_MAX, 0);

    printf("\n=== Q32.32 sqrt ===\n");
    CHECK("sqrt 0",    qnum_to_double(qnum_sqrt(QNUM_ZERO)), 0.0, 0);
    CHECK("sqrt 1",    qnum_to_double(qnum_sqrt(QNUM_ONE)),  1.0, 1e-9);
    CHECK("sqrt 4",    qnum_to_double(qnum_sqrt(float_to_qnum(4))),   2.0,   1e-9);
    CHECK("sqrt 9",    qnum_to_double(qnum_sqrt(float_to_qnum(9))),   3.0,   1e-9);
    CHECK("sqrt 2",    qnum_to_double(qnum_sqrt(float_to_qnum(2))),   sqrt(2),  1e-8);
    CHECK("sqrt 0.5",  qnum_to_double(qnum_sqrt(float_to_qnum(0.5))), sqrt(0.5),1e-8);
    CHECK("sqrt 0.25", qnum_to_double(qnum_sqrt(float_to_qnum(0.25))),0.5,   1e-9);
    CHECK("sqrt 100",  qnum_to_double(qnum_sqrt(float_to_qnum(100))), 10.0,  1e-8);
    CHECK("sqrt 1e6",  qnum_to_double(qnum_sqrt(float_to_qnum(1000000))), 1000.0, 1e-7);
    CHECK("sqrt neg",  qnum_to_double(qnum_sqrt(float_to_qnum(-1))), 0.0, 0);

    printf("\n%s — %d failure(s)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
