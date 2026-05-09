#ifndef QNUM_H
#define QNUM_H

#include <stdint.h>

/* ============================================================================
 * QNum — Fixed-Point Math, Two Precision Tiers
 *
 * Pure C99, header-only, zero dependencies.
 * Two's-complement integers assumed (universal on all C99 targets; C23
 * mandates it). No undefined behaviour on any defined input within each
 * tier's stated contract.
 *
 * ── Q4.28 tier  (q4_28_t = int32_t) ────────────────────────────────────────
 *   4 integer bits + 28 fractional bits. Range ≈ ±8.
 *   Fast, GBA-friendly, ~1 ULP. Arithmetic wraps by design — the FM
 *   phase accumulator relies on this. For: GBA homebrew, Pokémon Gen 3
 *   mechanics, deterministic game logic, real-time FM synthesis, and
 *   embedded systems with no FPU.
 *
 * ── Q32.32 tier (qnum_t = struct) ───────────────────────────────────────────
 *   32 integer bits + 32 fractional bits. Range ≈ ±2.147e9.
 *   Saturating, ~9–10 decimal digits of precision. For: cases where Q4.28
 *   lacks range or precision.
 *
 * ── Cross-tier ───────────────────────────────────────────────────────────────
 *   qnum_from_q4_28(raw)  — explicit promotion from Q4.28 → Q32.32.
 * ============================================================================ */


/* ============================================================================
 * Q4.28 TIER
 * Angle convention: 2^28 = 1 full turn  (0x10000000 = 1.0 turn)
 * ============================================================================ */

typedef int32_t q4_28_t;

#define Q_FRAC_BITS  28
#define Q_ONE        ((q4_28_t)((int32_t)1 << Q_FRAC_BITS))
#define Q_FRAC_MASK  ((1U << Q_FRAC_BITS) - 1U)

/* --- Conversions ---------------------------------------------------------- */

/* int_to_q: routes through uint32 to avoid signed-left-shift UB.
 * Valid for i in [-8, 7]; behaviour for out-of-range i is
 * implementation-defined (two's-complement truncation). */
static inline q4_28_t int_to_q(int32_t i) {
    return (q4_28_t)((uint32_t)i << Q_FRAC_BITS);
}

/* q_to_int: arithmetic right shift assumed (universal on all C99 targets). */
static inline int32_t q_to_int(q4_28_t q) {
    return (int32_t)(q >> Q_FRAC_BITS);
}

/* float_to_q: clamped to avoid out-of-range cast UB. */
static inline q4_28_t float_to_q(double f) {
    if (!(f > -8.0))  return INT32_MIN;   /* catches NaN */
    if (f >= 8.0)     return INT32_MAX;
    return (q4_28_t)(f * (double)Q_ONE);
}

static inline double q_to_float(q4_28_t q) {
    return (double)q / (double)Q_ONE;
}

/* --- Arithmetic ----------------------------------------------------------- */

/* q_add / q_sub wrap on overflow — intentional; phase accumulation relies
 * on this. On two's-complement platforms (all real C99 targets) the
 * arithmetic works as expected. */
static inline q4_28_t q_add(q4_28_t a, q4_28_t b) { return a + b; }
static inline q4_28_t q_sub(q4_28_t a, q4_28_t b) { return a - b; }

/* q_neg saturates INT32_MIN → INT32_MAX to avoid UB. */
static inline q4_28_t q_neg(q4_28_t a) {
    if (a == INT32_MIN) return INT32_MAX;
    return -a;
}

static inline q4_28_t q_abs(q4_28_t x) {
    if (x == INT32_MIN) return INT32_MAX;
    return x < 0 ? q_neg(x) : x;
}

/* q_mul: fully portable, no UB, truncates toward zero. */
static inline q4_28_t q_mul(q4_28_t a, q4_28_t b) {
    int64_t prod = (int64_t)a * (int64_t)b;
    if (prod >= 0) {
        return (q4_28_t)(prod >> Q_FRAC_BITS);
    } else {
        return (q4_28_t)-((-prod) >> Q_FRAC_BITS);
    }
}

/* q_div: saturates on divide-by-zero. */
static inline q4_28_t q_div(q4_28_t a, q4_28_t b) {
    if (b == 0) return INT32_MAX;
    return (q4_28_t)((((int64_t)a) << Q_FRAC_BITS) / b);
}

/* --- Trigonometry: 256-entry LUT + linear interpolation ------------------ */
static const int32_t q_sin_lut[256] = {
    0, 6587736, 13171504, 19747337, 26311276, 32859365, 39387662, 45892233,
    52369160, 58814541, 65224495, 71595161, 77922700, 84203301, 90433181, 96608588,
    102725802, 108781137, 114770946, 120691622, 126539598, 132311351, 138003405, 143612330,
    149134749, 154567334, 159906814, 165149973, 170293651, 175334750, 180270234, 185097131,
    189812531, 194413596, 198897553, 203261702, 207503414, 211620133, 215609380, 219468752,
    223195925, 226788652, 230244771, 233562198, 236738937, 239773073, 242662778, 245406313,
    248002024, 250448347, 252743810, 254887030, 256876715, 258711668, 260390782, 261913046,
    263277544, 264483453, 265530048, 266416696, 267142866, 267708119, 268112114, 268354608,
    268435456, 268354608, 268112114, 267708119, 267142866, 266416696, 265530048, 264483453,
    263277544, 261913046, 260390782, 258711668, 256876715, 254887030, 252743810, 250448347,
    248002024, 245406313, 242662778, 239773073, 236738937, 233562198, 230244771, 226788652,
    223195925, 219468752, 215609380, 211620133, 207503414, 203261702, 198897553, 194413596,
    189812531, 185097131, 180270234, 175334750, 170293651, 165149973, 159906814, 154567334,
    149134749, 143612330, 138003405, 132311351, 126539598, 120691622, 114770946, 108781137,
    102725802, 96608588, 90433181, 84203301, 77922700, 71595161, 65224495, 58814541,
    52369160, 45892233, 39387662, 32859365, 26311276, 19747337, 13171504, 6587736,
    0, -6587736, -13171504, -19747337, -26311276, -32859365, -39387662, -45892233,
    -52369160, -58814541, -65224495, -71595161, -77922700, -84203301, -90433181, -96608588,
    -102725802, -108781137, -114770946, -120691622, -126539598, -132311351, -138003405, -143612330,
    -149134749, -154567334, -159906814, -165149973, -170293651, -175334750, -180270234, -185097131,
    -189812531, -194413596, -198897553, -203261702, -207503414, -211620133, -215609380, -219468752,
    -223195925, -226788652, -230244771, -233562198, -236738937, -239773073, -242662778, -245406313,
    -248002024, -250448347, -252743810, -254887030, -256876715, -258711668, -260390782, -261913046,
    -263277544, -264483453, -265530048, -266416696, -267142866, -267708119, -268112114, -268354608,
    -268435456, -268354608, -268112114, -267708119, -267142866, -266416696, -265530048, -264483453,
    -263277544, -261913046, -260390782, -258711668, -256876715, -254887030, -252743810, -250448347,
    -248002024, -245406313, -242662778, -239773073, -236738937, -233562198, -230244771, -226788652,
    -223195925, -219468752, -215609380, -211620133, -207503414, -203261702, -198897553, -194413596,
    -189812531, -185097131, -180270234, -175334750, -170293651, -165149973, -159906814, -154567334,
    -149134749, -143612330, -138003405, -132311351, -126539598, -120691622, -114770946, -108781137,
    -102725802, -96608588, -90433181, -84203301, -77922700, -71595161, -65224495, -58814541,
    -52369160, -45892233, -39387662, -32859365, -26311276, -19747337, -13171504, -6587736
};

static inline q4_28_t q_sin(q4_28_t angle) {
    uint32_t u      = (uint32_t)angle;
    uint32_t frac28 = u & Q_FRAC_MASK;
    uint32_t idx0   = frac28 >> 20;
    uint32_t idx1   = (idx0 + 1) & 0xFFu;

    q4_28_t v0   = q_sin_lut[idx0];
    q4_28_t v1   = q_sin_lut[idx1];
    q4_28_t diff = v1 - v0;

    uint32_t frac20 = frac28 & 0xFFFFFu;   /* 20-bit interpolation fraction */
    return v0 + (q4_28_t)(((int64_t)diff * frac20) >> 20);
}

static inline q4_28_t q_cos(q4_28_t angle) {
    return q_sin(angle + (q4_28_t)((int32_t)1 << (Q_FRAC_BITS - 2)));
}

/* --- FM Operator ---------------------------------------------------------- */

typedef struct {
    q4_28_t phase_acc;
    q4_28_t phase_inc;
} q_fm_op_t;

static inline void q_fm_init(q_fm_op_t *op, q4_28_t inc) {
    op->phase_acc = 0;
    op->phase_inc = inc;
}

static inline q4_28_t q_fm_step(q_fm_op_t *op, q4_28_t mod_in) {
    op->phase_acc = q_add(op->phase_acc, q_add(op->phase_inc, mod_in));
    return q_sin(op->phase_acc);
}

/* --- Perfect-fifth harmonic helper --------------------------------------- */
/* fifth_powers[n] = raw Q4.28 of (3/2)^n for n = 0..5.
 * (3/2)^6 = 11.39 exceeds Q4.28 range; table capped at 6 entries. */
static inline q4_28_t q_perfect_fifth_ratio(q4_28_t base, uint32_t harmonic) {
    static const q4_28_t fifth_powers[6] = {
        0x10000000, /* (3/2)^0 = 1.0   */
        0x18000000, /* (3/2)^1 = 1.5   */
        0x24000000, /* (3/2)^2 = 2.25  */
        0x36000000, /* (3/2)^3 = 3.375 */
        0x51000000, /* (3/2)^4 = 5.0625 */
        0x79800000  /* (3/2)^5 = 7.59375 */
    };
    if (harmonic >= 6) return 0x7FFFFFFF;
    return q_mul(base, fifth_powers[harmonic]);
}

/* ============================================================================
 * Q32.32 TIER
 * ============================================================================ */

/* ============================================================================
 * QNum — Q32.32 Fixed-Point Math (Split Integer + Fraction)
 *
 * Pure C99, header-only, zero dependencies.
 * 32 signed integer bits + 32 fractional bits.
 * Range: -2147483648.0 … +2147483647.999999999 (≈ ±2.147e9)
 * ~9–10 decimal digits of precision.
 *
 * Drop-in bigger sibling to orb42-qmath Q4.28.
 * Two's-complement integers assumed (universal on all C99 targets in
 * practice; mandated by C23). No undefined behaviour on any defined input.
 * ============================================================================ */

typedef struct {
    int32_t  integer;   /* Signed integer part */
    uint32_t fraction;  /* Fractional part [0, 2^32) representing [0, 1) */
} qnum_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

#define QNUM_FRAC_MAX   ((uint32_t)0xFFFFFFFFu)  /* largest representable fraction; one ULP below 1.0 */
#define QNUM_ZERO       ((qnum_t){0, 0})
#define QNUM_ONE        ((qnum_t){1, 0})
#define QNUM_MAX        ((qnum_t){INT32_MAX, QNUM_FRAC_MAX})
#define QNUM_MIN        ((qnum_t){INT32_MIN, 0})

/* Backwards-compatibility alias. The fraction field cannot represent 1.0
 * by construction, so this is "max fraction", not "one as fraction".
 * Prefer QNUM_FRAC_MAX in new code. */
#define QNUM_ONE_FRAC   QNUM_FRAC_MAX

/* ============================================================================
 * Conversions
 * ============================================================================ */

static inline qnum_t int_to_qnum(int32_t i) {
    qnum_t q = {i, 0};
    return q;
}

static inline int32_t qnum_to_int(qnum_t q) {
    return q.integer;
}

static inline qnum_t float_to_qnum(double f) {
    if (!(f > -2147483649.0)) return QNUM_MIN;   /* catches NaN too */
    if (f >= 2147483648.0)    return QNUM_MAX;

    qnum_t q;
    q.integer = (int32_t)f;
    double frac_d = f - (double)q.integer;

    if (frac_d < 0.0) {
        q.integer--;
        frac_d += 1.0;
    }

    if (frac_d >= 1.0) {
        if (q.integer < INT32_MAX) {
            q.integer++;
            q.fraction = 0;
        } else {
            q.fraction = QNUM_FRAC_MAX;
        }
        return q;
    }

    q.fraction = (uint32_t)(frac_d * 4294967296.0);
    return q;
}

static inline double qnum_to_double(qnum_t q) {
    return (double)q.integer + (double)q.fraction / 4294967296.0;
}

static inline float qnum_to_float(qnum_t q) {
    return (float)qnum_to_double(q);
}

/* ============================================================================
 * Basic Arithmetic (saturating)
 * ============================================================================ */

static inline qnum_t qnum_add(qnum_t a, qnum_t b) {
    uint64_t frac_sum = (uint64_t)a.fraction + b.fraction;
    uint32_t new_frac = (uint32_t)frac_sum;
    int32_t  carry    = (int32_t)(frac_sum >> 32);

    int64_t int_sum = (int64_t)a.integer + b.integer + carry;

    if (int_sum > INT32_MAX) return QNUM_MAX;
    if (int_sum < INT32_MIN) return QNUM_MIN;

    return (qnum_t){(int32_t)int_sum, new_frac};
}

static inline qnum_t qnum_sub(qnum_t a, qnum_t b) {
    uint64_t frac_diff = (uint64_t)a.fraction - b.fraction;
    uint32_t new_frac  = (uint32_t)frac_diff;
    int32_t  borrow    = (a.fraction < b.fraction) ? -1 : 0;

    int64_t int_diff = (int64_t)a.integer - b.integer + borrow;

    if (int_diff > INT32_MAX) return QNUM_MAX;
    if (int_diff < INT32_MIN) return QNUM_MIN;

    return (qnum_t){(int32_t)int_diff, new_frac};
}

static inline qnum_t qnum_neg(qnum_t a) {
    if (a.integer == INT32_MIN && a.fraction == 0) {
        return QNUM_MAX;
    }

    uint32_t frac_neg = (uint32_t)((uint64_t)0 - a.fraction);
    int32_t  int_neg  = ~a.integer + (frac_neg == 0 ? 1 : 0);

    return (qnum_t){int_neg, frac_neg};
}

static inline qnum_t qnum_abs(qnum_t x) {
    return (x.integer < 0) ? qnum_neg(x) : x;
}

/* ============================================================================
 * Multiplication (precise, UB-free, ARM-friendly 32x32→64 partials)
 * ============================================================================ */

static inline qnum_t qnum_mul(qnum_t a, qnum_t b) {
    int neg = (a.integer < 0) ^ (b.integer < 0);

    uint64_t a_bits = ((uint64_t)(uint32_t)a.integer << 32) | a.fraction;
    uint64_t b_bits = ((uint64_t)(uint32_t)b.integer << 32) | b.fraction;
    uint64_t a_mag  = (a.integer < 0) ? ((uint64_t)0 - a_bits) : a_bits;
    uint64_t b_mag  = (b.integer < 0) ? ((uint64_t)0 - b_bits) : b_bits;

    uint32_t a_lo = (uint32_t)a_mag;
    uint32_t a_hi = (uint32_t)(a_mag >> 32);
    uint32_t b_lo = (uint32_t)b_mag;
    uint32_t b_hi = (uint32_t)(b_mag >> 32);

    uint64_t P_hh = (uint64_t)a_hi * b_hi;
    uint64_t P_hl = (uint64_t)a_hi * b_lo;
    uint64_t P_lh = (uint64_t)a_lo * b_hi;
    uint64_t P_ll = (uint64_t)a_lo * b_lo;

    uint64_t col_frac = (P_ll >> 32) + (P_hl & 0xFFFFFFFFu) + (P_lh & 0xFFFFFFFFu);
    uint32_t res_frac = (uint32_t)col_frac;
    uint64_t carry1   = col_frac >> 32;

    uint64_t col_int  = (P_hh & 0xFFFFFFFFu) + (P_hl >> 32) + (P_lh >> 32) + carry1;
    uint32_t res_int  = (uint32_t)col_int;
    uint64_t carry2   = col_int >> 32;

    uint64_t overflow = (P_hh >> 32) + carry2;

    if (overflow != 0) {
        return neg ? QNUM_MIN : QNUM_MAX;
    }

    if (neg) {
        if (res_int > 0x80000000u || (res_int == 0x80000000u && res_frac > 0)) {
            return QNUM_MIN;
        }
        uint64_t raw = ((uint64_t)res_int << 32) | res_frac;
        uint64_t neg_raw = (uint64_t)0 - raw;
        return (qnum_t){(int32_t)(uint32_t)(neg_raw >> 32), (uint32_t)neg_raw};
    } else {
        if (res_int > (uint32_t)INT32_MAX) {
            return QNUM_MAX;
        }
        return (qnum_t){(int32_t)res_int, res_frac};
    }
}

/* ============================================================================
 * Internal helpers for division
 * ============================================================================ */

/* Count leading zeros of a 64-bit value. Returns 64 for x == 0. */
static inline int qnum_clz64(uint64_t x) {
    int n = 0;
    if (x == 0) return 64;
    if ((x & 0xFFFFFFFF00000000ULL) == 0) { n += 32; x <<= 32; }
    if ((x & 0xFFFF000000000000ULL) == 0) { n += 16; x <<= 16; }
    if ((x & 0xFF00000000000000ULL) == 0) { n += 8;  x <<= 8;  }
    if ((x & 0xF000000000000000ULL) == 0) { n += 4;  x <<= 4;  }
    if ((x & 0xC000000000000000ULL) == 0) { n += 2;  x <<= 2;  }
    if ((x & 0x8000000000000000ULL) == 0) { n += 1; }
    return n;
}

/* Q32.32 multiply, raw uint64 in/out. Extracts bits [32..95] of the
 * 128-bit product. Caller ensures the product fits in [0, 2^64). */
static inline uint64_t qnum_mul_raw(uint64_t a, uint64_t b) {
    uint32_t a_lo = (uint32_t)a;
    uint32_t a_hi = (uint32_t)(a >> 32);
    uint32_t b_lo = (uint32_t)b;
    uint32_t b_hi = (uint32_t)(b >> 32);

    uint64_t P_hh = (uint64_t)a_hi * b_hi;
    uint64_t P_hl = (uint64_t)a_hi * b_lo;
    uint64_t P_lh = (uint64_t)a_lo * b_hi;
    uint64_t P_ll = (uint64_t)a_lo * b_lo;

    uint64_t col_frac = (P_ll >> 32) + (P_hl & 0xFFFFFFFFu) + (P_lh & 0xFFFFFFFFu);
    uint64_t carry1   = col_frac >> 32;
    uint64_t col_int  = (P_hh & 0xFFFFFFFFu) + (P_hl >> 32) + (P_lh >> 32) + carry1;

    return ((col_int & 0xFFFFFFFFu) << 32) | (uint32_t)col_frac;
}

/* Full unsigned 64x64 → 128 multiplication. Outputs hi and lo halves. */
static inline void qnum_umul128(uint64_t a, uint64_t b,
                                uint64_t *out_hi, uint64_t *out_lo) {
    uint32_t a_lo = (uint32_t)a;
    uint32_t a_hi = (uint32_t)(a >> 32);
    uint32_t b_lo = (uint32_t)b;
    uint32_t b_hi = (uint32_t)(b >> 32);

    uint64_t P_hh = (uint64_t)a_hi * b_hi;
    uint64_t P_hl = (uint64_t)a_hi * b_lo;
    uint64_t P_lh = (uint64_t)a_lo * b_hi;
    uint64_t P_ll = (uint64_t)a_lo * b_lo;

    uint64_t mid       = (P_ll >> 32) + (P_hl & 0xFFFFFFFFu) + (P_lh & 0xFFFFFFFFu);
    uint64_t lo        = (P_ll & 0xFFFFFFFFu) | ((mid & 0xFFFFFFFFu) << 32);
    uint64_t high_low  = (P_hh & 0xFFFFFFFFu) + (P_hl >> 32) + (P_lh >> 32) + (mid >> 32);
    uint64_t high_high = (P_hh >> 32) + (high_low >> 32);
    uint64_t hi        = (high_low & 0xFFFFFFFFu) | (high_high << 32);

    *out_hi = hi;
    *out_lo = lo;
}

/* ============================================================================
 * 256-entry initial reciprocal LUT for Newton-Raphson at Q32.32.
 *
 * Index i corresponds to b_norm in [(256+i)/512, (256+i+1)/512]; entry
 * encodes (r - 1) * 2^32 where r = 1/midpoint and midpoint = (513+2i)/1024.
 * Recover initial estimate as r_raw = (1ULL << 32) + LUT[idx].
 *
 * Generated by:
 *   for i in range(256):
 *       num, den = 511 - 2*i, 513 + 2*i
 *       LUT[i] = round(num * 2**32 / den)
 * ============================================================================ */
static const uint32_t qnum_recip_lut[256] = {
    0xFF007FC0u, 0xFD04794Au, 0xFB0C610Du, 0xF9182B68u, 0xF727CCE6u, 0xF53B3A40u, 0xF352685Au, 0xF16D4C44u,
    0xEF8BDB39u, 0xEDAE0A9Bu, 0xEBD3CFF8u, 0xE9FD2104u, 0xE829F39Bu, 0xE65A3DBEu, 0xE48DF597u, 0xE2C51172u,
    0xE0FF87C0u, 0xDF3D4F18u, 0xDD7E5E31u, 0xDBC2ABE8u, 0xDA0A2F38u, 0xD854DF40u, 0xD6A2B33Fu, 0xD4F3A293u,
    0xD347A4BCu, 0xD19EB156u, 0xCFF8C01Du, 0xCE55C8EBu, 0xCCB5C3B6u, 0xCB18A893u, 0xC97E6FB1u, 0xC7E7115Du,
    0xC65285FDu, 0xC4C0C614u, 0xC331CA3Fu, 0xC1A58B32u, 0xC01C01C0u, 0xBE9526D0u, 0xBD10F365u, 0xBB8F6098u,
    0xBA10679Cu, 0xB89401B9u, 0xB71A284Fu, 0xB5A2D4D6u, 0xB42E00DAu, 0xB2BBA5FFu, 0xB14BBDFDu, 0xAFDE42A3u,
    0xAE732DD2u, 0xAD0A7981u, 0xABA41FBDu, 0xAA401AA4u, 0xA8DE6469u, 0xA77EF751u, 0xA621CDB5u, 0xA4C6E201u,
    0xA36E2EB2u, 0xA217AE57u, 0xA0C35B93u, 0x9F713117u, 0x9E2129A8u, 0x9CD3401Au, 0x9B876F52u, 0x9A3DB247u,
    0x98F603FEu, 0x97B05F8Du, 0x966CC019u, 0x952B20D7u, 0x93EB7D0Bu, 0x92ADD006u, 0x9172152Cu, 0x903847EAu,
    0x8F0063C0u, 0x8DCA6439u, 0x8C9644F0u, 0x8B64018Bu, 0x8A3395C0u, 0x8904FD50u, 0x87D8340Bu, 0x86AD35CBu,
    0x8583FE7Au, 0x845C8A0Du, 0x8336D484u, 0x8212D9ECu, 0x80F0965Eu, 0x7FD005FFu, 0x7EB12500u, 0x7D93EF9Bu,
    0x7C786217u, 0x7B5E78C7u, 0x7A463006u, 0x792F843Cu, 0x781A71DCu, 0x7706F561u, 0x75F50B52u, 0x74E4B040u,
    0x73D5E0C6u, 0x72C89987u, 0x71BCD733u, 0x70B29681u, 0x6FA9D432u, 0x6EA28D12u, 0x6D9CBDF2u, 0x6C9863B2u,
    0x6B957B35u, 0x6A94016Bu, 0x6993F34Au, 0x68954DD2u, 0x67980E0Cu, 0x669C3107u, 0x65A1B3DDu, 0x64A893AEu,
    0x63B0CDA2u, 0x62BA5EEBu, 0x61C544C0u, 0x60D17C62u, 0x5FDF0318u, 0x5EEDD631u, 0x5DFDF303u, 0x5D0F56EDu,
    0x5C21FF52u, 0x5B35E99Fu, 0x5A4B1347u, 0x596179C3u, 0x58791A93u, 0x5791F340u, 0x56AC0157u, 0x55C7426Bu,
    0x54E3B419u, 0x54015401u, 0x53201FCBu, 0x52401524u, 0x516131C0u, 0x50837359u, 0x4FA6D7AFu, 0x4ECB5C87u,
    0x4DF0FFADu, 0x4D17BEF1u, 0x4C3F982Cu, 0x4B688939u, 0x4A928FFBu, 0x49BDAA58u, 0x48E9D63Eu, 0x4817119Fu,
    0x47455A72u, 0x4674AEB4u, 0x45A50C67u, 0x44D67191u, 0x4408DC3Eu, 0x433C4A7Fu, 0x4270BA69u, 0x41A62A17u,
    0x40DC97A8u, 0x40140140u, 0x3F4C6507u, 0x3E85C12Bu, 0x3DC013DCu, 0x3CFB5B51u, 0x3C3795C5u, 0x3B74C177u,
    0x3AB2DCA8u, 0x39F1E5A2u, 0x3931DAB0u, 0x3872BA20u, 0x37B48248u, 0x36F73180u, 0x363AC623u, 0x357F3E90u,
    0x34C4992Eu, 0x340AD461u, 0x3351EE98u, 0x3299E640u, 0x31E2B9CDu, 0x312C67B6u, 0x3076EE75u, 0x2FC24C88u,
    0x2F0E8072u, 0x2E5B88B6u, 0x2DA963DEu, 0x2CF81076u, 0x2C478D0Du, 0x2B97D836u, 0x2AE8F087u, 0x2A3AD49Bu,
    0x298D830Du, 0x28E0FA7Eu, 0x28353990u, 0x278A3EEBu, 0x26E00937u, 0x26369721u, 0x258DE759u, 0x24E5F890u,
    0x243EC97Du, 0x239858D8u, 0x22F2A55Du, 0x224DADC9u, 0x21A970DEu, 0x2105ED5Fu, 0x20632214u, 0x1FC10DC5u,
    0x1F1FAF3Fu, 0x1E7F0551u, 0x1DDF0ECCu, 0x1D3FCA84u, 0x1CA13750u, 0x1C03540Au, 0x1B661F8Du, 0x1AC998B7u,
    0x1A2DBE6Au, 0x19928F89u, 0x18F80AFAu, 0x185E2FA4u, 0x17C4FC73u, 0x172C7053u, 0x16948A34u, 0x15FD4907u,
    0x1566ABC0u, 0x14D0B156u, 0x143B58C0u, 0x13A6A0FAu, 0x13128900u, 0x127F0FD1u, 0x11EC346Eu, 0x1159F5DBu,
    0x10C8531Du, 0x10374B3Bu, 0x0FA6DD3Fu, 0x0F170835u, 0x0E87CB29u, 0x0DF9252Du, 0x0D6B1550u, 0x0CDD9AA6u,
    0x0C50B446u, 0x0BC46146u, 0x0B38A0C0u, 0x0AAD71CFu, 0x0A22D38Fu, 0x0998C51Fu, 0x090F45A1u, 0x08865437u,
    0x07FDF004u, 0x0776182Fu, 0x06EECBE0u, 0x06680A40u, 0x05E1D27Au, 0x055C23BCu, 0x04D6FD33u, 0x04525E10u,
    0x03CE4585u, 0x034AB2C5u, 0x02C7A506u, 0x02451B7Eu, 0x01C31565u, 0x014191F6u, 0x00C0906Cu, 0x00401004u,
};

/* ============================================================================
 * Division and Reciprocal (Newton-Raphson, fully portable, no UB)
 *
 * Algorithm:
 *   1. Sign-extract result; take 64-bit unsigned magnitudes of a and b.
 *   2. Special cases: b == 0 → sign-correct saturation; a == 0 → QNUM_ZERO.
 *   3. Normalize b by left-shifting raw bits until the leading bit is at
 *      position 63. The shift count clz_b lets us compute s = clz_b - 32,
 *      so that normalized b lies in [0.5, 1.0) and 1/b_value =
 *      (1/b_norm_value) * 2^s.
 *   4. Look up an 8-bit-accurate initial reciprocal estimate from the LUT.
 *   5. Refine with three Newton-Raphson iterations
 *        r_{n+1} = r_n * (2 - b_norm * r_n)
 *      Each iteration roughly doubles the bits of precision; three
 *      iterations from 8 bits exceed Q32.32's 32-bit fraction comfortably,
 *      with the third iteration absorbing accumulated rounding.
 *   6. For division, compute the full 128-bit product (a_mag * r_raw) and
 *      extract the appropriate 64-bit window based on the de-normalization
 *      shift. This is more accurate at format-precision boundaries than
 *      composing reciprocal + multiply, because the intermediate keeps
 *      full 128-bit precision before the final shift.
 *   7. Saturate at the magnitude bound and re-apply sign.
 *
 * Saturation policy:
 *   a / 0 = QNUM_MAX if a >= 0, QNUM_MIN if a < 0;  0 / 0 = QNUM_ZERO.
 *
 * Precision: the algorithm matches Q32.32's intrinsic precision floor of
 * 2^-32 absolute. For a result value v, this means relative error of order
 * 2^-32 / |v|. For results near 2^-20 the relative floor is ~2.4e-4; for
 * results in the [1, 2^31] range the floor is ~2.3e-10. This is the format
 * limit, not an algorithm limit.
 * ============================================================================ */

static inline qnum_t qnum_div(qnum_t a, qnum_t b) {
    if (b.integer == 0 && b.fraction == 0) {
        if (a.integer == 0 && a.fraction == 0) return QNUM_ZERO;
        return (a.integer < 0) ? QNUM_MIN : QNUM_MAX;
    }
    if (a.integer == 0 && a.fraction == 0) return QNUM_ZERO;

    int neg = (a.integer < 0) ^ (b.integer < 0);

    uint64_t a_bits = ((uint64_t)(uint32_t)a.integer << 32) | a.fraction;
    uint64_t b_bits = ((uint64_t)(uint32_t)b.integer << 32) | b.fraction;
    uint64_t a_mag  = (a.integer < 0) ? ((uint64_t)0 - a_bits) : a_bits;
    uint64_t b_mag  = (b.integer < 0) ? ((uint64_t)0 - b_bits) : b_bits;

    /* Normalize b: shift left so leading bit is at position 63. */
    int clz = qnum_clz64(b_mag);
    uint64_t b_norm_full = b_mag << clz;        /* Q0.64 raw, value in [0.5, 1) */
    uint64_t b_norm_q32  = b_norm_full >> 32;   /* Q32.32 raw, in [2^31, 2^32) */

    /* Initial reciprocal estimate from LUT, indexed by bits [30..23] of
     * b_norm_q32. Bit 31 is always 1 after normalization. */
    uint8_t idx = (uint8_t)((b_norm_q32 >> 23) & 0xFFu);
    uint64_t r_raw = ((uint64_t)1 << 32) + qnum_recip_lut[idx];

    /* Three Newton-Raphson iterations: r = r * (2 - b_norm * r). */
    int iter;
    for (iter = 0; iter < 3; iter++) {
        uint64_t br_raw       = qnum_mul_raw(b_norm_q32, r_raw);
        uint64_t two_minus_br = ((uint64_t)2 << 32) - br_raw;
        r_raw                 = qnum_mul_raw(r_raw, two_minus_br);
    }
    /* r_raw is now Q32.32 raw of 1/b_norm_value, in [2^32, 2^33]. */

    /* Multiply a_mag * r_raw at full 128-bit precision. The Q32.32 raw of
     * a/b is then this product right-shifted by (64 - clz). */
    uint64_t prod_hi, prod_lo;
    qnum_umul128(a_mag, r_raw, &prod_hi, &prod_lo);

    int extract_shift = 64 - clz;   /* in (0, 64] given b != 0 */

    /* Extract the 64-bit window starting at bit 'extract_shift'. Detect
     * any bits set above the window (overflow). */
    uint64_t result_raw;
    int overflow = 0;
    if (extract_shift < 64) {
        if ((prod_hi >> extract_shift) != 0) overflow = 1;
        result_raw = (prod_lo >> extract_shift)
                   | (prod_hi << (64 - extract_shift));
    } else {
        /* extract_shift == 64: result is the entire prod_hi. */
        result_raw = prod_hi;
    }

    if (overflow) {
        return neg ? QNUM_MIN : QNUM_MAX;
    }

    /* Saturate at the Q32.32 magnitude bound (2^63 - 1 positive, 2^63 with
     * zero fraction for negative). */
    if (neg) {
        if (result_raw > 0x8000000000000000ULL) return QNUM_MIN;
        if (result_raw == 0x8000000000000000ULL) {
            /* Exactly INT32_MIN — representable. */
            return QNUM_MIN;
        }
        uint64_t neg_raw = (uint64_t)0 - result_raw;
        return (qnum_t){(int32_t)(uint32_t)(neg_raw >> 32), (uint32_t)neg_raw};
    } else {
        if (result_raw > (((uint64_t)INT32_MAX << 32) | 0xFFFFFFFFu)) {
            return QNUM_MAX;
        }
        return (qnum_t){(int32_t)(uint32_t)(result_raw >> 32), (uint32_t)result_raw};
    }
}

/* Reciprocal as 1/b. Composed via qnum_div(QNUM_ONE, b) so it inherits the
 * full-precision direct-division path rather than the looser
 * recip-then-multiply composition. Useful as a building block for sqrt
 * (Newton-Raphson rsqrt iteration uses the reciprocal pattern). */
static inline qnum_t qnum_recip(qnum_t b) {
    return qnum_div(QNUM_ONE, b);
}

/* ============================================================================
 * Quick helpers
 * ============================================================================ */

static inline qnum_t qnum_from_q4_28(int32_t q428) {
    if (q428 >= 0) {
        uint32_t u = (uint32_t)q428;
        qnum_t q = {(int32_t)(u >> 28), (u & 0x0FFFFFFFu) << 4};
        return q;
    } else {
        uint32_t mag = (uint32_t)((uint32_t)0 - (uint32_t)q428);
        qnum_t  qp  = {(int32_t)(mag >> 28), (mag & 0x0FFFFFFFu) << 4};
        return qnum_neg(qp);
    }
}


/* ============================================================================
 * Q32.32 — Comparison
 *
 * Value ordering: integer parts compared as signed, then fraction parts as
 * unsigned when integers are equal. Correct for the split representation
 * because fraction is always in [0, 1) and adds positively to integer.
 * ============================================================================ */

static inline int qnum_lt(qnum_t a, qnum_t b) {
    if (a.integer != b.integer) return a.integer < b.integer;
    return a.fraction < b.fraction;
}
static inline int qnum_gt(qnum_t a, qnum_t b) { return qnum_lt(b, a); }
static inline int qnum_le(qnum_t a, qnum_t b) { return !qnum_gt(a, b); }
static inline int qnum_ge(qnum_t a, qnum_t b) { return !qnum_lt(a, b); }
static inline int qnum_eq(qnum_t a, qnum_t b) {
    return a.integer == b.integer && a.fraction == b.fraction;
}
static inline int qnum_ne(qnum_t a, qnum_t b) { return !qnum_eq(a, b); }

/* Value minimum and maximum. Note: qnum_min / qnum_max are value functions;
 * QNUM_MIN / QNUM_MAX are the representable-range boundary constants. */
static inline qnum_t qnum_min(qnum_t a, qnum_t b) { return qnum_lt(a, b) ? a : b; }
static inline qnum_t qnum_max(qnum_t a, qnum_t b) { return qnum_gt(a, b) ? a : b; }

/* ============================================================================
 * Q32.32 — Rounding
 * ============================================================================ */

/* floor: truncate toward -infinity. */
static inline qnum_t qnum_floor(qnum_t q) {
    return (qnum_t){q.integer, 0};
}

/* ceil: round toward +infinity. */
static inline qnum_t qnum_ceil(qnum_t q) {
    if (q.fraction == 0)       return q;
    if (q.integer == INT32_MAX) return QNUM_MAX;
    return (qnum_t){q.integer + 1, 0};
}

/* round: round half-up (ties toward +infinity). */
static inline qnum_t qnum_round(qnum_t q) {
    if (q.fraction < 0x80000000u) return (qnum_t){q.integer, 0};
    if (q.integer == INT32_MAX)   return QNUM_MAX;
    return (qnum_t){q.integer + 1, 0};
}

/* ============================================================================
 * Q32.32 → Q4.28 reverse bridge (lossy)
 *
 * Returns the Q4.28 raw int32 nearest to q's value, saturating at ±8.
 * Lossless for all Q4.28-representable values; loses precision outside
 * Q4.28's 28-bit fraction for values within [-8, +8).
 * ============================================================================ */

static inline int32_t qnum_to_q4_28_raw(qnum_t q) {
    if (q.integer < -8) return INT32_MIN;
    if (q.integer >  7) return INT32_MAX;
    /* Route through uint32 to avoid signed-left-shift UB. For i in [-8, 7],
     * the low 4 bits of (uint32_t)i carry all needed sign information. */
    return (int32_t)(((uint32_t)q.integer << 28) | (q.fraction >> 4));
}

/* ============================================================================
 * Q32.32 — Square Root (Newton-Raphson rsqrt)
 *
 * Algorithm:
 *   1. Normalize x into [0.5, 1.0) or [0.25, 0.5) using an even left shift
 *      (even so that sqrt(x) = sqrt(x_norm) * 2^(-shift/2) holds exactly).
 *   2. Look up an initial rsqrt estimate from qnum_rsqrt_lut, which covers
 *      [0.5, 1.0). For x_norm in [0.25, 0.5), adjust via sqrt(2).
 *   3. Three Newton-Raphson rsqrt iterations: y = y * (3 - x*y²) / 2.
 *      From an 8-bit start: 8 → 16 → 32 → 64 bits; format-limited at 32.
 *   4. sqrt(x) = x * rsqrt(x). Full-precision via umul128, extracted at
 *      bit position (48 - shift/2) of the 128-bit product.
 *
 * Returns QNUM_ZERO for x ≤ 0 (domain error; caller may test beforehand).
 * ============================================================================ */

/* 256-entry initial rsqrt LUT for x_norm in [0.5, 1.0).
 * Entry i encodes (rsqrt_mid - 1) * 2^32, where rsqrt_mid = 1/sqrt(bin_mid).
 * Recover initial y_raw = (1ULL << 32) + LUT[idx].
 *
 * Generated by:
 *   for i in range(256):
 *       mid = (513 + 2*i) / 1024.0
 *       LUT[i] = round((1/sqrt(mid) - 1) * 2**32)
 */
static const uint32_t qnum_rsqrt_lut[256] = {
    0x69AF85D1u, 0x68FB8EF8u, 0x6848A3B6u, 0x6796C179u, 0x66E5E5B3u, 0x66360DE3u, 0x6587378Du, 0x64D9603Fu,
    0x642C8591u, 0x6380A520u, 0x62D5BC92u, 0x622BC997u, 0x6182C9E5u, 0x60DABB38u, 0x60339B57u, 0x5F8D680Eu,
    0x5EE81F30u, 0x5E43BE99u, 0x5DA0442Bu, 0x5CFDADCEu, 0x5C5BF972u, 0x5BBB250Du, 0x5B1B2E9Cu, 0x5A7C1423u,
    0x59DDD3ACu, 0x59406B45u, 0x58A3D906u, 0x58081B09u, 0x576D2F72u, 0x56D31469u, 0x5639C81Au, 0x55A148BAu,
    0x55099482u, 0x5472A9AFu, 0x53DC8686u, 0x53472950u, 0x52B2905Cu, 0x521EB9FDu, 0x518BA48Cu, 0x50F94E67u,
    0x5067B5F1u, 0x4FD6D990u, 0x4F46B7B3u, 0x4EB74EC9u, 0x4E289D48u, 0x4D9AA1ACu, 0x4D0D5A71u, 0x4C80C61Du,
    0x4BF4E337u, 0x4B69B04Au, 0x4ADF2BE7u, 0x4A5554A2u, 0x49CC2915u, 0x4943A7DAu, 0x48BBCF94u, 0x48349EE7u,
    0x47AE147Bu, 0x47282EFCu, 0x46A2ED1Bu, 0x461E4D8Cu, 0x459A4F06u, 0x4516F044u, 0x44943004u, 0x44120D0Au,
    0x4390861Bu, 0x430F99FFu, 0x428F4784u, 0x420F8D79u, 0x41906AB2u, 0x4111DE04u, 0x4093E64Au, 0x40168260u,
    0x3F99B125u, 0x3F1D717Du, 0x3EA1C24Eu, 0x3E26A280u, 0x3DAC1100u, 0x3D320CBBu, 0x3CB894A4u, 0x3C3FA7AFu,
    0x3BC744D3u, 0x3B4F6B0Au, 0x3AD81950u, 0x3A614EA6u, 0x39EB0A0Cu, 0x39754A89u, 0x39000F21u, 0x388B56E0u,
    0x381720D2u, 0x37A36C05u, 0x3730378Au, 0x36BD8275u, 0x364B4BDBu, 0x35D992D5u, 0x3568567Eu, 0x34F795F2u,
    0x34875050u, 0x341784B9u, 0x33A83252u, 0x33395840u, 0x32CAF5AAu, 0x325D09BAu, 0x31EF939Eu, 0x31829281u,
    0x31160596u, 0x30A9EC0Eu, 0x303E451Du, 0x2FD30FF9u, 0x2F684BDAu, 0x2EFDF7FBu, 0x2E941396u, 0x2E2A9DEBu,
    0x2DC19637u, 0x2D58FBBEu, 0x2CF0CDC2u, 0x2C890B87u, 0x2C21B455u, 0x2BBAC775u, 0x2B54442Fu, 0x2AEE29D1u,
    0x2A8877A8u, 0x2A232D02u, 0x29BE4932u, 0x2959CB89u, 0x28F5B35Bu, 0x2891FFFEu, 0x282EB0CAu, 0x27CBC516u,
    0x27693C3Eu, 0x2707159Du, 0x26A55090u, 0x2643EC76u, 0x25E2E8AFu, 0x2582449Du, 0x2521FFA3u, 0x24C21925u,
    0x24629089u, 0x24036537u, 0x23A49697u, 0x23462412u, 0x22E80D15u, 0x228A510Bu, 0x222CEF62u, 0x21CFE78Bu,
    0x217338F4u, 0x2116E30Fu, 0x20BAE550u, 0x205F3F2Bu, 0x2003F015u, 0x1FA8F784u, 0x1F4E54F0u, 0x1EF407D2u,
    0x1E9A0FA4u, 0x1E406BE1u, 0x1DE71C05u, 0x1D8E1F8Fu, 0x1D3575FBu, 0x1CDD1ECBu, 0x1C85197Fu, 0x1C2D6598u,
    0x1BD6029Au, 0x1B7EF008u, 0x1B282D67u, 0x1AD1BA3Du, 0x1A7B9612u, 0x1A25C06Cu, 0x19D038D6u, 0x197AFED9u,
    0x19261200u, 0x18D171D7u, 0x187D1DEBu, 0x182915C9u, 0x17D55901u, 0x1781E722u, 0x172EBFBCu, 0x16DBE261u,
    0x16894EA4u, 0x16370418u, 0x15E50250u, 0x159348E2u, 0x1541D764u, 0x14F0AD6Cu, 0x149FCA92u, 0x144F2E6Eu,
    0x13FED89Au, 0x13AEC8AFu, 0x135EFE49u, 0x130F7903u, 0x12C03879u, 0x12713C48u, 0x12228410u, 0x11D40F6Du,
    0x1185DE00u, 0x1137EF6Au, 0x10EA434Au, 0x109CD944u, 0x104FB0F8u, 0x1002CA0Bu, 0x0FB62420u, 0x0F69BEDDu,
    0x0F1D99E5u, 0x0ED1B4E0u, 0x0E860F73u, 0x0E3AA948u, 0x0DEF8204u, 0x0DA49951u, 0x0D59EED9u, 0x0D0F8246u,
    0x0CC55341u, 0x0C7B6178u, 0x0C31AC94u, 0x0BE83444u, 0x0B9EF834u, 0x0B55F811u, 0x0B0D338Cu, 0x0AC4AA52u,
    0x0A7C5C13u, 0x0A34487Fu, 0x09EC6F48u, 0x09A4D01Eu, 0x095D6AB4u, 0x09163EBCu, 0x08CF4BEAu, 0x088891F0u,
    0x08421084u, 0x07FBC75Au, 0x07B5B627u, 0x076FDCA2u, 0x072A3A80u, 0x06E4CF78u, 0x069F9B43u, 0x065A9D98u,
    0x0615D630u, 0x05D144C3u, 0x058CE90Bu, 0x0548C2C2u, 0x0504D1A4u, 0x04C11569u, 0x047D8DCFu, 0x043A3A92u,
    0x03F71B6Du, 0x03B4301Du, 0x03717861u, 0x032EF3F6u, 0x02ECA29Au, 0x02AA840Cu, 0x0268980Bu, 0x0226DE58u,
    0x01E556B2u, 0x01A400D9u, 0x0162DC90u, 0x0121E996u, 0x00E127AFu, 0x00A0969Du, 0x00603622u, 0x00200601u,
};

/* sqrt(2) in Q32.32 raw, used to adjust rsqrt for x_norm in [0.25, 0.5). */
#define QNUM_SQRT2_RAW ((uint64_t)6074001000u)

static inline qnum_t qnum_sqrt(qnum_t a) {
    int iter;
    if (a.integer < 0)                        return QNUM_ZERO;
    if (a.integer == 0 && a.fraction == 0)    return QNUM_ZERO;

    uint64_t x_raw = ((uint64_t)(uint32_t)a.integer << 32) | a.fraction;

    /* Even normalization shift: clz rounded down to nearest even.
     * After shift, x_norm highest bit is at position 63 (shift = clz, even)
     * or 62 (shift = clz-1, i.e. clz was odd). */
    int clz   = qnum_clz64(x_raw);
    int shift = clz & ~1;
    uint64_t x_norm    = x_raw << shift;
    uint64_t x_norm_q32 = x_norm >> 32;    /* Q32.32 raw, in [2^30, 2^32) */

    /* Initial rsqrt estimate. LUT covers [0.5, 1.0) via bit 31 of x_norm_q32.
     * For x_norm in [0.25, 0.5) (bit 31 clear), double x for the LUT then
     * scale y by sqrt(2): rsqrt(x) = sqrt(2) * rsqrt(2x). */
    uint64_t y;
    if (x_norm_q32 < ((uint64_t)1 << 31)) {
        uint8_t idx = (uint8_t)((x_norm_q32 >> 22) & 0xFFu);
        uint64_t y2x = ((uint64_t)1 << 32) + qnum_rsqrt_lut[idx];
        y = qnum_mul_raw(y2x, QNUM_SQRT2_RAW);
    } else {
        uint8_t idx = (uint8_t)((x_norm_q32 >> 23) & 0xFFu);
        y = ((uint64_t)1 << 32) + qnum_rsqrt_lut[idx];
    }

    /* Three Newton-Raphson rsqrt iterations: y = y * (3 - x*y²) / 2. */
    for (iter = 0; iter < 3; iter++) {
        uint64_t y2        = qnum_mul_raw(y, y);
        uint64_t xy2       = qnum_mul_raw(x_norm_q32, y2);
        uint64_t three_min = ((uint64_t)3 << 32) - xy2;
        y                  = qnum_mul_raw(y, three_min) >> 1;
    }

    /* sqrt(x) = x * rsqrt(x). Full-precision via umul128.
     * result_raw = (x_raw * y_raw) >> (48 - shift/2).
     * Derivation: rsqrt(x_norm) = rsqrt(x * 2^(shift-32)), so
     * rsqrt(x) = y * 2^((shift-32)/2) and sqrt(x) = x * rsqrt(x). */
    uint64_t prod_hi, prod_lo;
    int extract = 48 - shift / 2;   /* always in [17, 48] */
    qnum_umul128(x_raw, y, &prod_hi, &prod_lo);
    uint64_t result_raw = (prod_lo >> extract) | (prod_hi << (64 - extract));

    if (result_raw > (((uint64_t)INT32_MAX << 32) | 0xFFFFFFFFu))
        return QNUM_MAX;

    return (qnum_t){(int32_t)(uint32_t)(result_raw >> 32), (uint32_t)result_raw};
}

/* ============================================================================
 * Q32.32 — Trigonometry: 4096-entry cubic-interpolation LUT
 *
 * Angle convention: only the fraction field is used.
 *   qnum.fraction in [0, 2^32) maps to one full turn.
 *   2^32 = 1 turn,  0x40000000 = 0.25 turn (π/2),  0x80000000 = π, ...
 * The integer field is ignored — angles wrap modulo a full turn automatically.
 *
 * Sizing rationale (precision tier):
 *   Q32.32 has 32 fractional bits (ULP = 2^-32). A 256-entry linear LUT
 *   (qmath-grade) would top out at ~2^-16 — six bits below qmath's *own*
 *   28-bit fraction and embarrassing here. The 4096-entry table with
 *   four-point Catmull-Rom cubic interpolation hits ~2^-30 absolute error
 *   across the full turn — within 2 ULP of Q32.32, which is what a precision
 *   tier owes its users.
 *
 * Storage:
 *   4096 × int64 = 32 KiB. Each entry holds the Q32.32 raw value of
 *   sin(2π·i/4096), so |entry| ≤ 2^32. Cardinal points are exact:
 *   LUT[0]=0, LUT[1024]=2^32, LUT[2048]=0, LUT[3072]=-2^32.
 *
 * Lookup:
 *   Top 12 bits of fraction → LUT index (4096 positions, full turn).
 *   Bottom 20 bits → sub-fraction t for cubic interpolation.
 *
 * Catmull-Rom form (doubled to defer the /2):
 *   y(t) = 0.5 · ( a3·t³ + a2·t² + a1·t + a0 )
 *     a0 =  2·p1
 *     a1 = -p0 + p2
 *     a2 =  2·p0 - 5·p1 + 4·p2 - p3
 *     a3 = -p0 + 3·p1 - 3·p2 + p3
 *   Evaluated by Horner's method with t as a 20-bit unsigned integer; each
 *   stage right-shifts by 20 to rescale. All intermediates fit in int64
 *   (max magnitude ≈ 2^56 in the (sum)·t product before the >>20).
 * ============================================================================ */

static const int64_t qnum_sin_lut[4096] = {
              +0LL,     +6588395LL,    +13176774LL,    +19765122LL,    +26353424LL,    +32941664LL,    +39529826LL,    +46117895LL,
       +52705856LL,    +59293692LL,    +65881389LL,    +72468931LL,    +79056303LL,    +85643488LL,    +92230472LL,    +98817239LL,
      +105403774LL,   +111990060LL,   +118576083LL,   +125161827LL,   +131747276LL,   +138332416LL,   +144917230LL,   +151501702LL,
      +158085819LL,   +164669563LL,   +171252920LL,   +177835874LL,   +184418409LL,   +191000511LL,   +197582163LL,   +204163350LL,
      +210744057LL,   +217324267LL,   +223903967LL,   +230483139LL,   +237061769LL,   +243639842LL,   +250217341LL,   +256794251LL,
      +263370557LL,   +269946243LL,   +276521294LL,   +283095695LL,   +289669429LL,   +296242481LL,   +302814837LL,   +309386480LL,
      +315957395LL,   +322527566LL,   +329096979LL,   +335665617LL,   +342233465LL,   +348800508LL,   +355366730LL,   +361932116LL,
      +368496651LL,   +375060318LL,   +381623102LL,   +388184989LL,   +394745962LL,   +401306007LL,   +407865107LL,   +414423247LL,
      +420980412LL,   +427536587LL,   +434091755LL,   +440645902LL,   +447199012LL,   +453751070LL,   +460302060LL,   +466851967LL,
      +473400776LL,   +479948470LL,   +486495035LL,   +493040456LL,   +499584716LL,   +506127800LL,   +512669694LL,   +519210381LL,
      +525749847LL,   +532288075LL,   +538825051LL,   +545360759LL,   +551895183LL,   +558428309LL,   +564960121LL,   +571490604LL,
      +578019742LL,   +584547519LL,   +591073921LL,   +597598933LL,   +604122538LL,   +610644721LL,   +617165468LL,   +623684762LL,
      +630202589LL,   +636718933LL,   +643233779LL,   +649747111LL,   +656258914LL,   +662769172LL,   +669277872LL,   +675784996LL,
      +682290530LL,   +688794459LL,   +695296767LL,   +701797439LL,   +708296459LL,   +714793813LL,   +721289485LL,   +727783459LL,
      +734275721LL,   +740766255LL,   +747255046LL,   +753742079LL,   +760227338LL,   +766710808LL,   +773192474LL,   +779672321LL,
      +786150333LL,   +792626495LL,   +799100792LL,   +805573208LL,   +812043729LL,   +818512339LL,   +824979024LL,   +831443766LL,
      +837906553LL,   +844367368LL,   +850826195LL,   +857283021LL,   +863737830LL,   +870190606LL,   +876641334LL,   +883090000LL,
      +889536587LL,   +895981082LL,   +902423468LL,   +908863731LL,   +915301854LL,   +921737825LL,   +928171626LL,   +934603243LL,
      +941032661LL,   +947459865LL,   +953884839LL,   +960307568LL,   +966728038LL,   +973146233LL,   +979562138LL,   +985975738LL,
      +992387019LL,   +998795963LL,  +1005202558LL,  +1011606787LL,  +1018008636LL,  +1024408090LL,  +1030805132LL,  +1037199750LL,
     +1043591926LL,  +1049981647LL,  +1056368897LL,  +1062753662LL,  +1069135926LL,  +1075515674LL,  +1081892891LL,  +1088267562LL,
     +1094639673LL,  +1101009208LL,  +1107376152LL,  +1113740490LL,  +1120102207LL,  +1126461289LL,  +1132817720LL,  +1139171486LL,
     +1145522571LL,  +1151870960LL,  +1158216639LL,  +1164559593LL,  +1170899806LL,  +1177237264LL,  +1183571952LL,  +1189903854LL,
     +1196232957LL,  +1202559245LL,  +1208882703LL,  +1215203317LL,  +1221521071LL,  +1227835951LL,  +1234147941LL,  +1240457028LL,
     +1246763195LL,  +1253066429LL,  +1259366714LL,  +1265664036LL,  +1271958380LL,  +1278249730LL,  +1284538073LL,  +1290823393LL,
     +1297105676LL,  +1303384906LL,  +1309661069LL,  +1315934151LL,  +1322204136LL,  +1328471010LL,  +1334734758LL,  +1340995365LL,
     +1347252816LL,  +1353507098LL,  +1359758194LL,  +1366006091LL,  +1372250773LL,  +1378492227LL,  +1384730436LL,  +1390965388LL,
     +1397197066LL,  +1403425456LL,  +1409650544LL,  +1415872315LL,  +1422090755LL,  +1428305848LL,  +1434517580LL,  +1440725936LL,
     +1446930903LL,  +1453132464LL,  +1459330606LL,  +1465525315LL,  +1471716574LL,  +1477904371LL,  +1484088690LL,  +1490269517LL,
     +1496446837LL,  +1502620636LL,  +1508790899LL,  +1514957611LL,  +1521120759LL,  +1527280328LL,  +1533436302LL,  +1539588668LL,
     +1545737412LL,  +1551882518LL,  +1558023973LL,  +1564161761LL,  +1570295869LL,  +1576426281LL,  +1582552984LL,  +1588675964LL,
     +1594795204LL,  +1600910693LL,  +1607022414LL,  +1613130353LL,  +1619234497LL,  +1625334831LL,  +1631431340LL,  +1637524010LL,
     +1643612827LL,  +1649697776LL,  +1655778843LL,  +1661856014LL,  +1667929275LL,  +1673998611LL,  +1680064008LL,  +1686125451LL,
     +1692182927LL,  +1698236421LL,  +1704285919LL,  +1710331406LL,  +1716372869LL,  +1722410293LL,  +1728443664LL,  +1734472968LL,
     +1740498191LL,  +1746519318LL,  +1752536335LL,  +1758549228LL,  +1764557983LL,  +1770562587LL,  +1776563023LL,  +1782559280LL,
     +1788551342LL,  +1794539195LL,  +1800522825LL,  +1806502219LL,  +1812477362LL,  +1818448240LL,  +1824414839LL,  +1830377145LL,
     +1836335144LL,  +1842288821LL,  +1848238164LL,  +1854183158LL,  +1860123788LL,  +1866060042LL,  +1871991904LL,  +1877919361LL,
     +1883842400LL,  +1889761006LL,  +1895675165LL,  +1901584863LL,  +1907490086LL,  +1913390821LL,  +1919287054LL,  +1925178771LL,
     +1931065957LL,  +1936948599LL,  +1942826684LL,  +1948700196LL,  +1954569124LL,  +1960433452LL,  +1966293167LL,  +1972148255LL,
     +1977998702LL,  +1983844495LL,  +1989685620LL,  +1995522063LL,  +2001353810LL,  +2007180848LL,  +2013003163LL,  +2018820741LL,
     +2024633568LL,  +2030441631LL,  +2036244917LL,  +2042043411LL,  +2047837100LL,  +2053625970LL,  +2059410008LL,  +2065189200LL,
     +2070963532LL,  +2076732991LL,  +2082497563LL,  +2088257235LL,  +2094011993LL,  +2099761824LL,  +2105506713LL,  +2111246649LL,
     +2116981616LL,  +2122711602LL,  +2128436593LL,  +2134156575LL,  +2139871536LL,  +2145581461LL,  +2151286337LL,  +2156986152LL,
     +2162680890LL,  +2168370540LL,  +2174055087LL,  +2179734519LL,  +2185408821LL,  +2191077981LL,  +2196741986LL,  +2202400821LL,
     +2208054473LL,  +2213702930LL,  +2219346178LL,  +2224984203LL,  +2230616993LL,  +2236244534LL,  +2241866812LL,  +2247483816LL,
     +2253095531LL,  +2258701944LL,  +2264303042LL,  +2269898812LL,  +2275489241LL,  +2281074316LL,  +2286654023LL,  +2292228349LL,
     +2297797281LL,  +2303360806LL,  +2308918911LL,  +2314471584LL,  +2320018810LL,  +2325560576LL,  +2331096871LL,  +2336627680LL,
     +2342152991LL,  +2347672791LL,  +2353187066LL,  +2358695804LL,  +2364198992LL,  +2369696616LL,  +2375188665LL,  +2380675124LL,
     +2386155981LL,  +2391631224LL,  +2397100839LL,  +2402564813LL,  +2408023134LL,  +2413475788LL,  +2418922764LL,  +2424364047LL,
     +2429799626LL,  +2435229487LL,  +2440653617LL,  +2446072005LL,  +2451484637LL,  +2456891500LL,  +2462292582LL,  +2467687870LL,
     +2473077351LL,  +2478461013LL,  +2483838842LL,  +2489210827LL,  +2494576955LL,  +2499937213LL,  +2505291588LL,  +2510640068LL,
     +2515982640LL,  +2521319292LL,  +2526650010LL,  +2531974784LL,  +2537293599LL,  +2542606444LL,  +2547913306LL,  +2553214173LL,
     +2558509031LL,  +2563797869LL,  +2569080674LL,  +2574357434LL,  +2579628136LL,  +2584892768LL,  +2590151318LL,  +2595403773LL,
     +2600650120LL,  +2605890348LL,  +2611124444LL,  +2616352396LL,  +2621574191LL,  +2626789817LL,  +2631999263LL,  +2637202515LL,
     +2642399561LL,  +2647590390LL,  +2652774988LL,  +2657953344LL,  +2663125446LL,  +2668291281LL,  +2673450838LL,  +2678604104LL,
     +2683751066LL,  +2688891714LL,  +2694026034LL,  +2699154015LL,  +2704275644LL,  +2709390911LL,  +2714499801LL,  +2719602305LL,
     +2724698408LL,  +2729788101LL,  +2734871369LL,  +2739948203LL,  +2745018589LL,  +2750082515LL,  +2755139971LL,  +2760190943LL,
     +2765235421LL,  +2770273391LL,  +2775304843LL,  +2780329764LL,  +2785348143LL,  +2790359968LL,  +2795365227LL,  +2800363908LL,
     +2805355999LL,  +2810341489LL,  +2815320366LL,  +2820292619LL,  +2825258235LL,  +2830217203LL,  +2835169511LL,  +2840115147LL,
     +2845054101LL,  +2849986360LL,  +2854911913LL,  +2859830747LL,  +2864742853LL,  +2869648217LL,  +2874546829LL,  +2879438676LL,
     +2884323748LL,  +2889202033LL,  +2894073520LL,  +2898938196LL,  +2903796051LL,  +2908647073LL,  +2913491250LL,  +2918328572LL,
     +2923159027LL,  +2927982603LL,  +2932799290LL,  +2937609075LL,  +2942411948LL,  +2947207897LL,  +2951996911LL,  +2956778979LL,
     +2961554089LL,  +2966322230LL,  +2971083391LL,  +2975837561LL,  +2980584729LL,  +2985324883LL,  +2990058012LL,  +2994784105LL,
     +2999503152LL,  +3004215140LL,  +3008920059LL,  +3013617897LL,  +3018308645LL,  +3022992289LL,  +3027668821LL,  +3032338228LL,
     +3037000500LL,  +3041655625LL,  +3046303593LL,  +3050944393LL,  +3055578014LL,  +3060204445LL,  +3064823674LL,  +3069435692LL,
     +3074040487LL,  +3078638049LL,  +3083228366LL,  +3087811428LL,  +3092387225LL,  +3096955744LL,  +3101516976LL,  +3106070910LL,
     +3110617535LL,  +3115156841LL,  +3119688816LL,  +3124213451LL,  +3128730733LL,  +3133240654LL,  +3137743202LL,  +3142238366LL,
     +3146726136LL,  +3151206502LL,  +3155679453LL,  +3160144978LL,  +3164603066LL,  +3169053709LL,  +3173496894LL,  +3177932612LL,
     +3182360851LL,  +3186781603LL,  +3191194855LL,  +3195600598LL,  +3199998822LL,  +3204389516LL,  +3208772670LL,  +3213148273LL,
     +3217516315LL,  +3221876786LL,  +3226229675LL,  +3230574973LL,  +3234912670LL,  +3239242754LL,  +3243565216LL,  +3247880045LL,
     +3252187232LL,  +3256486766LL,  +3260778637LL,  +3265062836LL,  +3269339351LL,  +3273608174LL,  +3277869293LL,  +3282122699LL,
     +3286368382LL,  +3290606332LL,  +3294836538LL,  +3299058992LL,  +3303273682LL,  +3307480600LL,  +3311679735LL,  +3315871077LL,
     +3320054617LL,  +3324230344LL,  +3328398249LL,  +3332558322LL,  +3336710553LL,  +3340854932LL,  +3344991450LL,  +3349120097LL,
     +3353240863LL,  +3357353739LL,  +3361458715LL,  +3365555780LL,  +3369644927LL,  +3373726144LL,  +3377799422LL,  +3381864752LL,
     +3385922125LL,  +3389971529LL,  +3394012957LL,  +3398046399LL,  +3402071844LL,  +3406089285LL,  +3410098710LL,  +3414100111LL,
     +3418093478LL,  +3422078802LL,  +3426056074LL,  +3430025284LL,  +3433986423LL,  +3437939481LL,  +3441884449LL,  +3445821319LL,
     +3449750080LL,  +3453670723LL,  +3457583240LL,  +3461487620LL,  +3465383855LL,  +3469271936LL,  +3473151854LL,  +3477023598LL,
     +3480887161LL,  +3484742533LL,  +3488589706LL,  +3492428669LL,  +3496259414LL,  +3500081932LL,  +3503896214LL,  +3507702251LL,
     +3511500034LL,  +3515289554LL,  +3519070803LL,  +3522843770LL,  +3526608449LL,  +3530364828LL,  +3534112901LL,  +3537852657LL,
     +3541584088LL,  +3545307186LL,  +3549021941LL,  +3552728345LL,  +3556426389LL,  +3560116064LL,  +3563797363LL,  +3567470275LL,
     +3571134792LL,  +3574790907LL,  +3578438609LL,  +3582077892LL,  +3585708745LL,  +3589331160LL,  +3592945130LL,  +3596550645LL,
     +3600147697LL,  +3603736278LL,  +3607316378LL,  +3610887990LL,  +3614451106LL,  +3618005716LL,  +3621551813LL,  +3625089388LL,
     +3628618433LL,  +3632138939LL,  +3635650898LL,  +3639154303LL,  +3642649144LL,  +3646135414LL,  +3649613104LL,  +3653082206LL,
     +3656542712LL,  +3659994613LL,  +3663437903LL,  +3666872572LL,  +3670298613LL,  +3673716017LL,  +3677124776LL,  +3680524883LL,
     +3683916329LL,  +3687299106LL,  +3690673207LL,  +3694038624LL,  +3697395348LL,  +3700743371LL,  +3704082687LL,  +3707413286LL,
     +3710735162LL,  +3714048305LL,  +3717352710LL,  +3720648367LL,  +3723935269LL,  +3727213408LL,  +3730482776LL,  +3733743367LL,
     +3736995171LL,  +3740238183LL,  +3743472393LL,  +3746697794LL,  +3749914379LL,  +3753122140LL,  +3756321069LL,  +3759511160LL,
     +3762692404LL,  +3765864794LL,  +3769028322LL,  +3772182982LL,  +3775328765LL,  +3778465665LL,  +3781593674LL,  +3784712784LL,
     +3787822988LL,  +3790924279LL,  +3794016650LL,  +3797100093LL,  +3800174601LL,  +3803240167LL,  +3806296784LL,  +3809344444LL,
     +3812383140LL,  +3815412866LL,  +3818433613LL,  +3821445375LL,  +3824448145LL,  +3827441916LL,  +3830426680LL,  +3833402431LL,
     +3836369162LL,  +3839326865LL,  +3842275534LL,  +3845215161LL,  +3848145741LL,  +3851067265LL,  +3853979728LL,  +3856883122LL,
     +3859777440LL,  +3862662676LL,  +3865538822LL,  +3868405873LL,  +3871263820LL,  +3874112659LL,  +3876952381LL,  +3879782980LL,
     +3882604450LL,  +3885416784LL,  +3888219974LL,  +3891014016LL,  +3893798902LL,  +3896574625LL,  +3899341179LL,  +3902098557LL,
     +3904846754LL,  +3907585762LL,  +3910315575LL,  +3913036187LL,  +3915747591LL,  +3918449781LL,  +3921142750LL,  +3923826493LL,
     +3926501002LL,  +3929166272LL,  +3931822297LL,  +3934469069LL,  +3937106583LL,  +3939734833LL,  +3942353812LL,  +3944963515LL,
     +3947563934LL,  +3950155065LL,  +3952736900LL,  +3955309435LL,  +3957872662LL,  +3960426576LL,  +3962971170LL,  +3965506439LL,
     +3968032378LL,  +3970548979LL,  +3973056236LL,  +3975554145LL,  +3978042699LL,  +3980521892LL,  +3982991719LL,  +3985452174LL,
     +3987903250LL,  +3990344942LL,  +3992777245LL,  +3995200152LL,  +3997613658LL,  +4000017757LL,  +4002412444LL,  +4004797713LL,
     +4007173558LL,  +4009539974LL,  +4011896955LL,  +4014244496LL,  +4016582591LL,  +4018911234LL,  +4021230421LL,  +4023540145LL,
     +4025840401LL,  +4028131185LL,  +4030412489LL,  +4032684310LL,  +4034946641LL,  +4037199478LL,  +4039442815LL,  +4041676647LL,
     +4043900968LL,  +4046115773LL,  +4048321058LL,  +4050516816LL,  +4052703044LL,  +4054879734LL,  +4057046884LL,  +4059204486LL,
     +4061352537LL,  +4063491032LL,  +4065619964LL,  +4067739330LL,  +4069849124LL,  +4071949341LL,  +4074039976LL,  +4076121025LL,
     +4078192482LL,  +4080254343LL,  +4082306603LL,  +4084349257LL,  +4086382299LL,  +4088405726LL,  +4090419533LL,  +4092423715LL,
     +4094418266LL,  +4096403184LL,  +4098378461LL,  +4100344095LL,  +4102300081LL,  +4104246413LL,  +4106183088LL,  +4108110101LL,
     +4110027446LL,  +4111935121LL,  +4113833119LL,  +4115721438LL,  +4117600071LL,  +4119469016LL,  +4121328267LL,  +4123177820LL,
     +4125017671LL,  +4126847815LL,  +4128668249LL,  +4130478967LL,  +4132279966LL,  +4134071241LL,  +4135852789LL,  +4137624604LL,
     +4139386683LL,  +4141139022LL,  +4142881616LL,  +4144614462LL,  +4146337555LL,  +4148050891LL,  +4149754467LL,  +4151448277LL,
     +4153132319LL,  +4154806588LL,  +4156471081LL,  +4158125793LL,  +4159770720LL,  +4161405860LL,  +4163031206LL,  +4164646757LL,
     +4166252509LL,  +4167848456LL,  +4169434596LL,  +4171010925LL,  +4172577440LL,  +4174134136LL,  +4175681009LL,  +4177218057LL,
     +4178745276LL,  +4180262661LL,  +4181770210LL,  +4183267919LL,  +4184755784LL,  +4186233802LL,  +4187701970LL,  +4189160283LL,
     +4190608739LL,  +4192047334LL,  +4193476065LL,  +4194894928LL,  +4196303920LL,  +4197703038LL,  +4199092278LL,  +4200471637LL,
     +4201841112LL,  +4203200700LL,  +4204550397LL,  +4205890201LL,  +4207220108LL,  +4208540114LL,  +4209850218LL,  +4211150416LL,
     +4212440704LL,  +4213721080LL,  +4214991540LL,  +4216252083LL,  +4217502704LL,  +4218743401LL,  +4219974170LL,  +4221195010LL,
     +4222405917LL,  +4223606888LL,  +4224797921LL,  +4225979012LL,  +4227150159LL,  +4228311359LL,  +4229462610LL,  +4230603908LL,
     +4231735252LL,  +4232856637LL,  +4233968062LL,  +4235069525LL,  +4236161021LL,  +4237242550LL,  +4238314108LL,  +4239375693LL,
     +4240427302LL,  +4241468933LL,  +4242500584LL,  +4243522251LL,  +4244533933LL,  +4245535628LL,  +4246527332LL,  +4247509043LL,
     +4248480760LL,  +4249442480LL,  +4250394200LL,  +4251335919LL,  +4252267634LL,  +4253189343LL,  +4254101044LL,  +4255002735LL,
     +4255894413LL,  +4256776076LL,  +4257647723LL,  +4258509352LL,  +4259360959LL,  +4260202544LL,  +4261034104LL,  +4261855638LL,
     +4262667143LL,  +4263468618LL,  +4264260060LL,  +4265041468LL,  +4265812840LL,  +4266574174LL,  +4267325469LL,  +4268066722LL,
     +4268797931LL,  +4269519096LL,  +4270230215LL,  +4270931285LL,  +4271622305LL,  +4272303274LL,  +4272974189LL,  +4273635050LL,
     +4274285855LL,  +4274926601LL,  +4275557289LL,  +4276177915LL,  +4276788480LL,  +4277388980LL,  +4277979416LL,  +4278559785LL,
     +4279130086LL,  +4279690318LL,  +4280240479LL,  +4280780569LL,  +4281310585LL,  +4281830528LL,  +4282340394LL,  +4282840184LL,
     +4283329896LL,  +4283809529LL,  +4284279082LL,  +4284738553LL,  +4285187942LL,  +4285627247LL,  +4286056468LL,  +4286475604LL,
     +4286884652LL,  +4287283614LL,  +4287672487LL,  +4288051271LL,  +4288419964LL,  +4288778567LL,  +4289127078LL,  +4289465495LL,
     +4289793820LL,  +4290112050LL,  +4290420185LL,  +4290718224LL,  +4291006167LL,  +4291284012LL,  +4291551760LL,  +4291809410LL,
     +4292056960LL,  +4292294411LL,  +4292521761LL,  +4292739011LL,  +4292946160LL,  +4293143206LL,  +4293330151LL,  +4293506993LL,
     +4293673732LL,  +4293830368LL,  +4293976900LL,  +4294113327LL,  +4294239650LL,  +4294355869LL,  +4294461982LL,  +4294557990LL,
     +4294643893LL,  +4294719690LL,  +4294785381LL,  +4294840966LL,  +4294886444LL,  +4294921817LL,  +4294947083LL,  +4294962243LL,
     +4294967296LL,  +4294962243LL,  +4294947083LL,  +4294921817LL,  +4294886444LL,  +4294840966LL,  +4294785381LL,  +4294719690LL,
     +4294643893LL,  +4294557990LL,  +4294461982LL,  +4294355869LL,  +4294239650LL,  +4294113327LL,  +4293976900LL,  +4293830368LL,
     +4293673732LL,  +4293506993LL,  +4293330151LL,  +4293143206LL,  +4292946160LL,  +4292739011LL,  +4292521761LL,  +4292294411LL,
     +4292056960LL,  +4291809410LL,  +4291551760LL,  +4291284012LL,  +4291006167LL,  +4290718224LL,  +4290420185LL,  +4290112050LL,
     +4289793820LL,  +4289465495LL,  +4289127078LL,  +4288778567LL,  +4288419964LL,  +4288051271LL,  +4287672487LL,  +4287283614LL,
     +4286884652LL,  +4286475604LL,  +4286056468LL,  +4285627247LL,  +4285187942LL,  +4284738553LL,  +4284279082LL,  +4283809529LL,
     +4283329896LL,  +4282840184LL,  +4282340394LL,  +4281830528LL,  +4281310585LL,  +4280780569LL,  +4280240479LL,  +4279690318LL,
     +4279130086LL,  +4278559785LL,  +4277979416LL,  +4277388980LL,  +4276788480LL,  +4276177915LL,  +4275557289LL,  +4274926601LL,
     +4274285855LL,  +4273635050LL,  +4272974189LL,  +4272303274LL,  +4271622305LL,  +4270931285LL,  +4270230215LL,  +4269519096LL,
     +4268797931LL,  +4268066722LL,  +4267325469LL,  +4266574174LL,  +4265812840LL,  +4265041468LL,  +4264260060LL,  +4263468618LL,
     +4262667143LL,  +4261855638LL,  +4261034104LL,  +4260202544LL,  +4259360959LL,  +4258509352LL,  +4257647723LL,  +4256776076LL,
     +4255894413LL,  +4255002735LL,  +4254101044LL,  +4253189343LL,  +4252267634LL,  +4251335919LL,  +4250394200LL,  +4249442480LL,
     +4248480760LL,  +4247509043LL,  +4246527332LL,  +4245535628LL,  +4244533933LL,  +4243522251LL,  +4242500584LL,  +4241468933LL,
     +4240427302LL,  +4239375693LL,  +4238314108LL,  +4237242550LL,  +4236161021LL,  +4235069525LL,  +4233968062LL,  +4232856637LL,
     +4231735252LL,  +4230603908LL,  +4229462610LL,  +4228311359LL,  +4227150159LL,  +4225979012LL,  +4224797921LL,  +4223606888LL,
     +4222405917LL,  +4221195010LL,  +4219974170LL,  +4218743401LL,  +4217502704LL,  +4216252083LL,  +4214991540LL,  +4213721080LL,
     +4212440704LL,  +4211150416LL,  +4209850218LL,  +4208540114LL,  +4207220108LL,  +4205890201LL,  +4204550397LL,  +4203200700LL,
     +4201841112LL,  +4200471637LL,  +4199092278LL,  +4197703038LL,  +4196303920LL,  +4194894928LL,  +4193476065LL,  +4192047334LL,
     +4190608739LL,  +4189160283LL,  +4187701970LL,  +4186233802LL,  +4184755784LL,  +4183267919LL,  +4181770210LL,  +4180262661LL,
     +4178745276LL,  +4177218057LL,  +4175681009LL,  +4174134136LL,  +4172577440LL,  +4171010925LL,  +4169434596LL,  +4167848456LL,
     +4166252509LL,  +4164646757LL,  +4163031206LL,  +4161405860LL,  +4159770720LL,  +4158125793LL,  +4156471081LL,  +4154806588LL,
     +4153132319LL,  +4151448277LL,  +4149754467LL,  +4148050891LL,  +4146337555LL,  +4144614462LL,  +4142881616LL,  +4141139022LL,
     +4139386683LL,  +4137624604LL,  +4135852789LL,  +4134071241LL,  +4132279966LL,  +4130478967LL,  +4128668249LL,  +4126847815LL,
     +4125017671LL,  +4123177820LL,  +4121328267LL,  +4119469016LL,  +4117600071LL,  +4115721438LL,  +4113833119LL,  +4111935121LL,
     +4110027446LL,  +4108110101LL,  +4106183088LL,  +4104246413LL,  +4102300081LL,  +4100344095LL,  +4098378461LL,  +4096403184LL,
     +4094418266LL,  +4092423715LL,  +4090419533LL,  +4088405726LL,  +4086382299LL,  +4084349257LL,  +4082306603LL,  +4080254343LL,
     +4078192482LL,  +4076121025LL,  +4074039976LL,  +4071949341LL,  +4069849124LL,  +4067739330LL,  +4065619964LL,  +4063491032LL,
     +4061352537LL,  +4059204486LL,  +4057046884LL,  +4054879734LL,  +4052703044LL,  +4050516816LL,  +4048321058LL,  +4046115773LL,
     +4043900968LL,  +4041676647LL,  +4039442815LL,  +4037199478LL,  +4034946641LL,  +4032684310LL,  +4030412489LL,  +4028131185LL,
     +4025840401LL,  +4023540145LL,  +4021230421LL,  +4018911234LL,  +4016582591LL,  +4014244496LL,  +4011896955LL,  +4009539974LL,
     +4007173558LL,  +4004797713LL,  +4002412444LL,  +4000017757LL,  +3997613658LL,  +3995200152LL,  +3992777245LL,  +3990344942LL,
     +3987903250LL,  +3985452174LL,  +3982991719LL,  +3980521892LL,  +3978042699LL,  +3975554145LL,  +3973056236LL,  +3970548979LL,
     +3968032378LL,  +3965506439LL,  +3962971170LL,  +3960426576LL,  +3957872662LL,  +3955309435LL,  +3952736900LL,  +3950155065LL,
     +3947563934LL,  +3944963515LL,  +3942353812LL,  +3939734833LL,  +3937106583LL,  +3934469069LL,  +3931822297LL,  +3929166272LL,
     +3926501002LL,  +3923826493LL,  +3921142750LL,  +3918449781LL,  +3915747591LL,  +3913036187LL,  +3910315575LL,  +3907585762LL,
     +3904846754LL,  +3902098557LL,  +3899341179LL,  +3896574625LL,  +3893798902LL,  +3891014016LL,  +3888219974LL,  +3885416784LL,
     +3882604450LL,  +3879782980LL,  +3876952381LL,  +3874112659LL,  +3871263820LL,  +3868405873LL,  +3865538822LL,  +3862662676LL,
     +3859777440LL,  +3856883122LL,  +3853979728LL,  +3851067265LL,  +3848145741LL,  +3845215161LL,  +3842275534LL,  +3839326865LL,
     +3836369162LL,  +3833402431LL,  +3830426680LL,  +3827441916LL,  +3824448145LL,  +3821445375LL,  +3818433613LL,  +3815412866LL,
     +3812383140LL,  +3809344444LL,  +3806296784LL,  +3803240167LL,  +3800174601LL,  +3797100093LL,  +3794016650LL,  +3790924279LL,
     +3787822988LL,  +3784712784LL,  +3781593674LL,  +3778465665LL,  +3775328765LL,  +3772182982LL,  +3769028322LL,  +3765864794LL,
     +3762692404LL,  +3759511160LL,  +3756321069LL,  +3753122140LL,  +3749914379LL,  +3746697794LL,  +3743472393LL,  +3740238183LL,
     +3736995171LL,  +3733743367LL,  +3730482776LL,  +3727213408LL,  +3723935269LL,  +3720648367LL,  +3717352710LL,  +3714048305LL,
     +3710735162LL,  +3707413286LL,  +3704082687LL,  +3700743371LL,  +3697395348LL,  +3694038624LL,  +3690673207LL,  +3687299106LL,
     +3683916329LL,  +3680524883LL,  +3677124776LL,  +3673716017LL,  +3670298613LL,  +3666872572LL,  +3663437903LL,  +3659994613LL,
     +3656542712LL,  +3653082206LL,  +3649613104LL,  +3646135414LL,  +3642649144LL,  +3639154303LL,  +3635650898LL,  +3632138939LL,
     +3628618433LL,  +3625089388LL,  +3621551813LL,  +3618005716LL,  +3614451106LL,  +3610887990LL,  +3607316378LL,  +3603736278LL,
     +3600147697LL,  +3596550645LL,  +3592945130LL,  +3589331160LL,  +3585708745LL,  +3582077892LL,  +3578438609LL,  +3574790907LL,
     +3571134792LL,  +3567470275LL,  +3563797363LL,  +3560116064LL,  +3556426389LL,  +3552728345LL,  +3549021941LL,  +3545307186LL,
     +3541584088LL,  +3537852657LL,  +3534112901LL,  +3530364828LL,  +3526608449LL,  +3522843770LL,  +3519070803LL,  +3515289554LL,
     +3511500034LL,  +3507702251LL,  +3503896214LL,  +3500081932LL,  +3496259414LL,  +3492428669LL,  +3488589706LL,  +3484742533LL,
     +3480887161LL,  +3477023598LL,  +3473151854LL,  +3469271936LL,  +3465383855LL,  +3461487620LL,  +3457583240LL,  +3453670723LL,
     +3449750080LL,  +3445821319LL,  +3441884449LL,  +3437939481LL,  +3433986423LL,  +3430025284LL,  +3426056074LL,  +3422078802LL,
     +3418093478LL,  +3414100111LL,  +3410098710LL,  +3406089285LL,  +3402071844LL,  +3398046399LL,  +3394012957LL,  +3389971529LL,
     +3385922125LL,  +3381864752LL,  +3377799422LL,  +3373726144LL,  +3369644927LL,  +3365555780LL,  +3361458715LL,  +3357353739LL,
     +3353240863LL,  +3349120097LL,  +3344991450LL,  +3340854932LL,  +3336710553LL,  +3332558322LL,  +3328398249LL,  +3324230344LL,
     +3320054617LL,  +3315871077LL,  +3311679735LL,  +3307480600LL,  +3303273682LL,  +3299058992LL,  +3294836538LL,  +3290606332LL,
     +3286368382LL,  +3282122699LL,  +3277869293LL,  +3273608174LL,  +3269339351LL,  +3265062836LL,  +3260778637LL,  +3256486766LL,
     +3252187232LL,  +3247880045LL,  +3243565216LL,  +3239242754LL,  +3234912670LL,  +3230574973LL,  +3226229675LL,  +3221876786LL,
     +3217516315LL,  +3213148273LL,  +3208772670LL,  +3204389516LL,  +3199998822LL,  +3195600598LL,  +3191194855LL,  +3186781603LL,
     +3182360851LL,  +3177932612LL,  +3173496894LL,  +3169053709LL,  +3164603066LL,  +3160144978LL,  +3155679453LL,  +3151206502LL,
     +3146726136LL,  +3142238366LL,  +3137743202LL,  +3133240654LL,  +3128730733LL,  +3124213451LL,  +3119688816LL,  +3115156841LL,
     +3110617535LL,  +3106070910LL,  +3101516976LL,  +3096955744LL,  +3092387225LL,  +3087811428LL,  +3083228366LL,  +3078638049LL,
     +3074040487LL,  +3069435692LL,  +3064823674LL,  +3060204445LL,  +3055578014LL,  +3050944393LL,  +3046303593LL,  +3041655625LL,
     +3037000500LL,  +3032338228LL,  +3027668821LL,  +3022992289LL,  +3018308645LL,  +3013617897LL,  +3008920059LL,  +3004215140LL,
     +2999503152LL,  +2994784105LL,  +2990058012LL,  +2985324883LL,  +2980584729LL,  +2975837561LL,  +2971083391LL,  +2966322230LL,
     +2961554089LL,  +2956778979LL,  +2951996911LL,  +2947207897LL,  +2942411948LL,  +2937609075LL,  +2932799290LL,  +2927982603LL,
     +2923159027LL,  +2918328572LL,  +2913491250LL,  +2908647073LL,  +2903796051LL,  +2898938196LL,  +2894073520LL,  +2889202033LL,
     +2884323748LL,  +2879438676LL,  +2874546829LL,  +2869648217LL,  +2864742853LL,  +2859830747LL,  +2854911913LL,  +2849986360LL,
     +2845054101LL,  +2840115147LL,  +2835169511LL,  +2830217203LL,  +2825258235LL,  +2820292619LL,  +2815320366LL,  +2810341489LL,
     +2805355999LL,  +2800363908LL,  +2795365227LL,  +2790359968LL,  +2785348143LL,  +2780329764LL,  +2775304843LL,  +2770273391LL,
     +2765235421LL,  +2760190943LL,  +2755139971LL,  +2750082515LL,  +2745018589LL,  +2739948203LL,  +2734871369LL,  +2729788101LL,
     +2724698408LL,  +2719602305LL,  +2714499801LL,  +2709390911LL,  +2704275644LL,  +2699154015LL,  +2694026034LL,  +2688891714LL,
     +2683751066LL,  +2678604104LL,  +2673450838LL,  +2668291281LL,  +2663125446LL,  +2657953344LL,  +2652774988LL,  +2647590390LL,
     +2642399561LL,  +2637202515LL,  +2631999263LL,  +2626789817LL,  +2621574191LL,  +2616352396LL,  +2611124444LL,  +2605890348LL,
     +2600650120LL,  +2595403773LL,  +2590151318LL,  +2584892768LL,  +2579628136LL,  +2574357434LL,  +2569080674LL,  +2563797869LL,
     +2558509031LL,  +2553214173LL,  +2547913306LL,  +2542606444LL,  +2537293599LL,  +2531974784LL,  +2526650010LL,  +2521319292LL,
     +2515982640LL,  +2510640068LL,  +2505291588LL,  +2499937213LL,  +2494576955LL,  +2489210827LL,  +2483838842LL,  +2478461013LL,
     +2473077351LL,  +2467687870LL,  +2462292582LL,  +2456891500LL,  +2451484637LL,  +2446072005LL,  +2440653617LL,  +2435229487LL,
     +2429799626LL,  +2424364047LL,  +2418922764LL,  +2413475788LL,  +2408023134LL,  +2402564813LL,  +2397100839LL,  +2391631224LL,
     +2386155981LL,  +2380675124LL,  +2375188665LL,  +2369696616LL,  +2364198992LL,  +2358695804LL,  +2353187066LL,  +2347672791LL,
     +2342152991LL,  +2336627680LL,  +2331096871LL,  +2325560576LL,  +2320018810LL,  +2314471584LL,  +2308918911LL,  +2303360806LL,
     +2297797281LL,  +2292228349LL,  +2286654023LL,  +2281074316LL,  +2275489241LL,  +2269898812LL,  +2264303042LL,  +2258701944LL,
     +2253095531LL,  +2247483816LL,  +2241866812LL,  +2236244534LL,  +2230616993LL,  +2224984203LL,  +2219346178LL,  +2213702930LL,
     +2208054473LL,  +2202400821LL,  +2196741986LL,  +2191077981LL,  +2185408821LL,  +2179734519LL,  +2174055087LL,  +2168370540LL,
     +2162680890LL,  +2156986152LL,  +2151286337LL,  +2145581461LL,  +2139871536LL,  +2134156575LL,  +2128436593LL,  +2122711602LL,
     +2116981616LL,  +2111246649LL,  +2105506713LL,  +2099761824LL,  +2094011993LL,  +2088257235LL,  +2082497563LL,  +2076732991LL,
     +2070963532LL,  +2065189200LL,  +2059410008LL,  +2053625970LL,  +2047837100LL,  +2042043411LL,  +2036244917LL,  +2030441631LL,
     +2024633568LL,  +2018820741LL,  +2013003163LL,  +2007180848LL,  +2001353810LL,  +1995522063LL,  +1989685620LL,  +1983844495LL,
     +1977998702LL,  +1972148255LL,  +1966293167LL,  +1960433452LL,  +1954569124LL,  +1948700196LL,  +1942826684LL,  +1936948599LL,
     +1931065957LL,  +1925178771LL,  +1919287054LL,  +1913390821LL,  +1907490086LL,  +1901584863LL,  +1895675165LL,  +1889761006LL,
     +1883842400LL,  +1877919361LL,  +1871991904LL,  +1866060042LL,  +1860123788LL,  +1854183158LL,  +1848238164LL,  +1842288821LL,
     +1836335144LL,  +1830377145LL,  +1824414839LL,  +1818448240LL,  +1812477362LL,  +1806502219LL,  +1800522825LL,  +1794539195LL,
     +1788551342LL,  +1782559280LL,  +1776563023LL,  +1770562587LL,  +1764557983LL,  +1758549228LL,  +1752536335LL,  +1746519318LL,
     +1740498191LL,  +1734472968LL,  +1728443664LL,  +1722410293LL,  +1716372869LL,  +1710331406LL,  +1704285919LL,  +1698236421LL,
     +1692182927LL,  +1686125451LL,  +1680064008LL,  +1673998611LL,  +1667929275LL,  +1661856014LL,  +1655778843LL,  +1649697776LL,
     +1643612827LL,  +1637524010LL,  +1631431340LL,  +1625334831LL,  +1619234497LL,  +1613130353LL,  +1607022414LL,  +1600910693LL,
     +1594795204LL,  +1588675964LL,  +1582552984LL,  +1576426281LL,  +1570295869LL,  +1564161761LL,  +1558023973LL,  +1551882518LL,
     +1545737412LL,  +1539588668LL,  +1533436302LL,  +1527280328LL,  +1521120759LL,  +1514957611LL,  +1508790899LL,  +1502620636LL,
     +1496446837LL,  +1490269517LL,  +1484088690LL,  +1477904371LL,  +1471716574LL,  +1465525315LL,  +1459330606LL,  +1453132464LL,
     +1446930903LL,  +1440725936LL,  +1434517580LL,  +1428305848LL,  +1422090755LL,  +1415872315LL,  +1409650544LL,  +1403425456LL,
     +1397197066LL,  +1390965388LL,  +1384730436LL,  +1378492227LL,  +1372250773LL,  +1366006091LL,  +1359758194LL,  +1353507098LL,
     +1347252816LL,  +1340995365LL,  +1334734758LL,  +1328471010LL,  +1322204136LL,  +1315934151LL,  +1309661069LL,  +1303384906LL,
     +1297105676LL,  +1290823393LL,  +1284538073LL,  +1278249730LL,  +1271958380LL,  +1265664036LL,  +1259366714LL,  +1253066429LL,
     +1246763195LL,  +1240457028LL,  +1234147941LL,  +1227835951LL,  +1221521071LL,  +1215203317LL,  +1208882703LL,  +1202559245LL,
     +1196232957LL,  +1189903854LL,  +1183571952LL,  +1177237264LL,  +1170899806LL,  +1164559593LL,  +1158216639LL,  +1151870960LL,
     +1145522571LL,  +1139171486LL,  +1132817720LL,  +1126461289LL,  +1120102207LL,  +1113740490LL,  +1107376152LL,  +1101009208LL,
     +1094639673LL,  +1088267562LL,  +1081892891LL,  +1075515674LL,  +1069135926LL,  +1062753662LL,  +1056368897LL,  +1049981647LL,
     +1043591926LL,  +1037199750LL,  +1030805132LL,  +1024408090LL,  +1018008636LL,  +1011606787LL,  +1005202558LL,   +998795963LL,
      +992387019LL,   +985975738LL,   +979562138LL,   +973146233LL,   +966728038LL,   +960307568LL,   +953884839LL,   +947459865LL,
      +941032661LL,   +934603243LL,   +928171626LL,   +921737825LL,   +915301854LL,   +908863731LL,   +902423468LL,   +895981082LL,
      +889536587LL,   +883090000LL,   +876641334LL,   +870190606LL,   +863737830LL,   +857283021LL,   +850826195LL,   +844367368LL,
      +837906553LL,   +831443766LL,   +824979024LL,   +818512339LL,   +812043729LL,   +805573208LL,   +799100792LL,   +792626495LL,
      +786150333LL,   +779672321LL,   +773192474LL,   +766710808LL,   +760227338LL,   +753742079LL,   +747255046LL,   +740766255LL,
      +734275721LL,   +727783459LL,   +721289485LL,   +714793813LL,   +708296459LL,   +701797439LL,   +695296767LL,   +688794459LL,
      +682290530LL,   +675784996LL,   +669277872LL,   +662769172LL,   +656258914LL,   +649747111LL,   +643233779LL,   +636718933LL,
      +630202589LL,   +623684762LL,   +617165468LL,   +610644721LL,   +604122538LL,   +597598933LL,   +591073921LL,   +584547519LL,
      +578019742LL,   +571490604LL,   +564960121LL,   +558428309LL,   +551895183LL,   +545360759LL,   +538825051LL,   +532288075LL,
      +525749847LL,   +519210381LL,   +512669694LL,   +506127800LL,   +499584716LL,   +493040456LL,   +486495035LL,   +479948470LL,
      +473400776LL,   +466851967LL,   +460302060LL,   +453751070LL,   +447199012LL,   +440645902LL,   +434091755LL,   +427536587LL,
      +420980412LL,   +414423247LL,   +407865107LL,   +401306007LL,   +394745962LL,   +388184989LL,   +381623102LL,   +375060318LL,
      +368496651LL,   +361932116LL,   +355366730LL,   +348800508LL,   +342233465LL,   +335665617LL,   +329096979LL,   +322527566LL,
      +315957395LL,   +309386480LL,   +302814837LL,   +296242481LL,   +289669429LL,   +283095695LL,   +276521294LL,   +269946243LL,
      +263370557LL,   +256794251LL,   +250217341LL,   +243639842LL,   +237061769LL,   +230483139LL,   +223903967LL,   +217324267LL,
      +210744057LL,   +204163350LL,   +197582163LL,   +191000511LL,   +184418409LL,   +177835874LL,   +171252920LL,   +164669563LL,
      +158085819LL,   +151501702LL,   +144917230LL,   +138332416LL,   +131747276LL,   +125161827LL,   +118576083LL,   +111990060LL,
      +105403774LL,    +98817239LL,    +92230472LL,    +85643488LL,    +79056303LL,    +72468931LL,    +65881389LL,    +59293692LL,
       +52705856LL,    +46117895LL,    +39529826LL,    +32941664LL,    +26353424LL,    +19765122LL,    +13176774LL,     +6588395LL,
              +0LL,     -6588395LL,    -13176774LL,    -19765122LL,    -26353424LL,    -32941664LL,    -39529826LL,    -46117895LL,
       -52705856LL,    -59293692LL,    -65881389LL,    -72468931LL,    -79056303LL,    -85643488LL,    -92230472LL,    -98817239LL,
      -105403774LL,   -111990060LL,   -118576083LL,   -125161827LL,   -131747276LL,   -138332416LL,   -144917230LL,   -151501702LL,
      -158085819LL,   -164669563LL,   -171252920LL,   -177835874LL,   -184418409LL,   -191000511LL,   -197582163LL,   -204163350LL,
      -210744057LL,   -217324267LL,   -223903967LL,   -230483139LL,   -237061769LL,   -243639842LL,   -250217341LL,   -256794251LL,
      -263370557LL,   -269946243LL,   -276521294LL,   -283095695LL,   -289669429LL,   -296242481LL,   -302814837LL,   -309386480LL,
      -315957395LL,   -322527566LL,   -329096979LL,   -335665617LL,   -342233465LL,   -348800508LL,   -355366730LL,   -361932116LL,
      -368496651LL,   -375060318LL,   -381623102LL,   -388184989LL,   -394745962LL,   -401306007LL,   -407865107LL,   -414423247LL,
      -420980412LL,   -427536587LL,   -434091755LL,   -440645902LL,   -447199012LL,   -453751070LL,   -460302060LL,   -466851967LL,
      -473400776LL,   -479948470LL,   -486495035LL,   -493040456LL,   -499584716LL,   -506127800LL,   -512669694LL,   -519210381LL,
      -525749847LL,   -532288075LL,   -538825051LL,   -545360759LL,   -551895183LL,   -558428309LL,   -564960121LL,   -571490604LL,
      -578019742LL,   -584547519LL,   -591073921LL,   -597598933LL,   -604122538LL,   -610644721LL,   -617165468LL,   -623684762LL,
      -630202589LL,   -636718933LL,   -643233779LL,   -649747111LL,   -656258914LL,   -662769172LL,   -669277872LL,   -675784996LL,
      -682290530LL,   -688794459LL,   -695296767LL,   -701797439LL,   -708296459LL,   -714793813LL,   -721289485LL,   -727783459LL,
      -734275721LL,   -740766255LL,   -747255046LL,   -753742079LL,   -760227338LL,   -766710808LL,   -773192474LL,   -779672321LL,
      -786150333LL,   -792626495LL,   -799100792LL,   -805573208LL,   -812043729LL,   -818512339LL,   -824979024LL,   -831443766LL,
      -837906553LL,   -844367368LL,   -850826195LL,   -857283021LL,   -863737830LL,   -870190606LL,   -876641334LL,   -883090000LL,
      -889536587LL,   -895981082LL,   -902423468LL,   -908863731LL,   -915301854LL,   -921737825LL,   -928171626LL,   -934603243LL,
      -941032661LL,   -947459865LL,   -953884839LL,   -960307568LL,   -966728038LL,   -973146233LL,   -979562138LL,   -985975738LL,
      -992387019LL,   -998795963LL,  -1005202558LL,  -1011606787LL,  -1018008636LL,  -1024408090LL,  -1030805132LL,  -1037199750LL,
     -1043591926LL,  -1049981647LL,  -1056368897LL,  -1062753662LL,  -1069135926LL,  -1075515674LL,  -1081892891LL,  -1088267562LL,
     -1094639673LL,  -1101009208LL,  -1107376152LL,  -1113740490LL,  -1120102207LL,  -1126461289LL,  -1132817720LL,  -1139171486LL,
     -1145522571LL,  -1151870960LL,  -1158216639LL,  -1164559593LL,  -1170899806LL,  -1177237264LL,  -1183571952LL,  -1189903854LL,
     -1196232957LL,  -1202559245LL,  -1208882703LL,  -1215203317LL,  -1221521071LL,  -1227835951LL,  -1234147941LL,  -1240457028LL,
     -1246763195LL,  -1253066429LL,  -1259366714LL,  -1265664036LL,  -1271958380LL,  -1278249730LL,  -1284538073LL,  -1290823393LL,
     -1297105676LL,  -1303384906LL,  -1309661069LL,  -1315934151LL,  -1322204136LL,  -1328471010LL,  -1334734758LL,  -1340995365LL,
     -1347252816LL,  -1353507098LL,  -1359758194LL,  -1366006091LL,  -1372250773LL,  -1378492227LL,  -1384730436LL,  -1390965388LL,
     -1397197066LL,  -1403425456LL,  -1409650544LL,  -1415872315LL,  -1422090755LL,  -1428305848LL,  -1434517580LL,  -1440725936LL,
     -1446930903LL,  -1453132464LL,  -1459330606LL,  -1465525315LL,  -1471716574LL,  -1477904371LL,  -1484088690LL,  -1490269517LL,
     -1496446837LL,  -1502620636LL,  -1508790899LL,  -1514957611LL,  -1521120759LL,  -1527280328LL,  -1533436302LL,  -1539588668LL,
     -1545737412LL,  -1551882518LL,  -1558023973LL,  -1564161761LL,  -1570295869LL,  -1576426281LL,  -1582552984LL,  -1588675964LL,
     -1594795204LL,  -1600910693LL,  -1607022414LL,  -1613130353LL,  -1619234497LL,  -1625334831LL,  -1631431340LL,  -1637524010LL,
     -1643612827LL,  -1649697776LL,  -1655778843LL,  -1661856014LL,  -1667929275LL,  -1673998611LL,  -1680064008LL,  -1686125451LL,
     -1692182927LL,  -1698236421LL,  -1704285919LL,  -1710331406LL,  -1716372869LL,  -1722410293LL,  -1728443664LL,  -1734472968LL,
     -1740498191LL,  -1746519318LL,  -1752536335LL,  -1758549228LL,  -1764557983LL,  -1770562587LL,  -1776563023LL,  -1782559280LL,
     -1788551342LL,  -1794539195LL,  -1800522825LL,  -1806502219LL,  -1812477362LL,  -1818448240LL,  -1824414839LL,  -1830377145LL,
     -1836335144LL,  -1842288821LL,  -1848238164LL,  -1854183158LL,  -1860123788LL,  -1866060042LL,  -1871991904LL,  -1877919361LL,
     -1883842400LL,  -1889761006LL,  -1895675165LL,  -1901584863LL,  -1907490086LL,  -1913390821LL,  -1919287054LL,  -1925178771LL,
     -1931065957LL,  -1936948599LL,  -1942826684LL,  -1948700196LL,  -1954569124LL,  -1960433452LL,  -1966293167LL,  -1972148255LL,
     -1977998702LL,  -1983844495LL,  -1989685620LL,  -1995522063LL,  -2001353810LL,  -2007180848LL,  -2013003163LL,  -2018820741LL,
     -2024633568LL,  -2030441631LL,  -2036244917LL,  -2042043411LL,  -2047837100LL,  -2053625970LL,  -2059410008LL,  -2065189200LL,
     -2070963532LL,  -2076732991LL,  -2082497563LL,  -2088257235LL,  -2094011993LL,  -2099761824LL,  -2105506713LL,  -2111246649LL,
     -2116981616LL,  -2122711602LL,  -2128436593LL,  -2134156575LL,  -2139871536LL,  -2145581461LL,  -2151286337LL,  -2156986152LL,
     -2162680890LL,  -2168370540LL,  -2174055087LL,  -2179734519LL,  -2185408821LL,  -2191077981LL,  -2196741986LL,  -2202400821LL,
     -2208054473LL,  -2213702930LL,  -2219346178LL,  -2224984203LL,  -2230616993LL,  -2236244534LL,  -2241866812LL,  -2247483816LL,
     -2253095531LL,  -2258701944LL,  -2264303042LL,  -2269898812LL,  -2275489241LL,  -2281074316LL,  -2286654023LL,  -2292228349LL,
     -2297797281LL,  -2303360806LL,  -2308918911LL,  -2314471584LL,  -2320018810LL,  -2325560576LL,  -2331096871LL,  -2336627680LL,
     -2342152991LL,  -2347672791LL,  -2353187066LL,  -2358695804LL,  -2364198992LL,  -2369696616LL,  -2375188665LL,  -2380675124LL,
     -2386155981LL,  -2391631224LL,  -2397100839LL,  -2402564813LL,  -2408023134LL,  -2413475788LL,  -2418922764LL,  -2424364047LL,
     -2429799626LL,  -2435229487LL,  -2440653617LL,  -2446072005LL,  -2451484637LL,  -2456891500LL,  -2462292582LL,  -2467687870LL,
     -2473077351LL,  -2478461013LL,  -2483838842LL,  -2489210827LL,  -2494576955LL,  -2499937213LL,  -2505291588LL,  -2510640068LL,
     -2515982640LL,  -2521319292LL,  -2526650010LL,  -2531974784LL,  -2537293599LL,  -2542606444LL,  -2547913306LL,  -2553214173LL,
     -2558509031LL,  -2563797869LL,  -2569080674LL,  -2574357434LL,  -2579628136LL,  -2584892768LL,  -2590151318LL,  -2595403773LL,
     -2600650120LL,  -2605890348LL,  -2611124444LL,  -2616352396LL,  -2621574191LL,  -2626789817LL,  -2631999263LL,  -2637202515LL,
     -2642399561LL,  -2647590390LL,  -2652774988LL,  -2657953344LL,  -2663125446LL,  -2668291281LL,  -2673450838LL,  -2678604104LL,
     -2683751066LL,  -2688891714LL,  -2694026034LL,  -2699154015LL,  -2704275644LL,  -2709390911LL,  -2714499801LL,  -2719602305LL,
     -2724698408LL,  -2729788101LL,  -2734871369LL,  -2739948203LL,  -2745018589LL,  -2750082515LL,  -2755139971LL,  -2760190943LL,
     -2765235421LL,  -2770273391LL,  -2775304843LL,  -2780329764LL,  -2785348143LL,  -2790359968LL,  -2795365227LL,  -2800363908LL,
     -2805355999LL,  -2810341489LL,  -2815320366LL,  -2820292619LL,  -2825258235LL,  -2830217203LL,  -2835169511LL,  -2840115147LL,
     -2845054101LL,  -2849986360LL,  -2854911913LL,  -2859830747LL,  -2864742853LL,  -2869648217LL,  -2874546829LL,  -2879438676LL,
     -2884323748LL,  -2889202033LL,  -2894073520LL,  -2898938196LL,  -2903796051LL,  -2908647073LL,  -2913491250LL,  -2918328572LL,
     -2923159027LL,  -2927982603LL,  -2932799290LL,  -2937609075LL,  -2942411948LL,  -2947207897LL,  -2951996911LL,  -2956778979LL,
     -2961554089LL,  -2966322230LL,  -2971083391LL,  -2975837561LL,  -2980584729LL,  -2985324883LL,  -2990058012LL,  -2994784105LL,
     -2999503152LL,  -3004215140LL,  -3008920059LL,  -3013617897LL,  -3018308645LL,  -3022992289LL,  -3027668821LL,  -3032338228LL,
     -3037000500LL,  -3041655625LL,  -3046303593LL,  -3050944393LL,  -3055578014LL,  -3060204445LL,  -3064823674LL,  -3069435692LL,
     -3074040487LL,  -3078638049LL,  -3083228366LL,  -3087811428LL,  -3092387225LL,  -3096955744LL,  -3101516976LL,  -3106070910LL,
     -3110617535LL,  -3115156841LL,  -3119688816LL,  -3124213451LL,  -3128730733LL,  -3133240654LL,  -3137743202LL,  -3142238366LL,
     -3146726136LL,  -3151206502LL,  -3155679453LL,  -3160144978LL,  -3164603066LL,  -3169053709LL,  -3173496894LL,  -3177932612LL,
     -3182360851LL,  -3186781603LL,  -3191194855LL,  -3195600598LL,  -3199998822LL,  -3204389516LL,  -3208772670LL,  -3213148273LL,
     -3217516315LL,  -3221876786LL,  -3226229675LL,  -3230574973LL,  -3234912670LL,  -3239242754LL,  -3243565216LL,  -3247880045LL,
     -3252187232LL,  -3256486766LL,  -3260778637LL,  -3265062836LL,  -3269339351LL,  -3273608174LL,  -3277869293LL,  -3282122699LL,
     -3286368382LL,  -3290606332LL,  -3294836538LL,  -3299058992LL,  -3303273682LL,  -3307480600LL,  -3311679735LL,  -3315871077LL,
     -3320054617LL,  -3324230344LL,  -3328398249LL,  -3332558322LL,  -3336710553LL,  -3340854932LL,  -3344991450LL,  -3349120097LL,
     -3353240863LL,  -3357353739LL,  -3361458715LL,  -3365555780LL,  -3369644927LL,  -3373726144LL,  -3377799422LL,  -3381864752LL,
     -3385922125LL,  -3389971529LL,  -3394012957LL,  -3398046399LL,  -3402071844LL,  -3406089285LL,  -3410098710LL,  -3414100111LL,
     -3418093478LL,  -3422078802LL,  -3426056074LL,  -3430025284LL,  -3433986423LL,  -3437939481LL,  -3441884449LL,  -3445821319LL,
     -3449750080LL,  -3453670723LL,  -3457583240LL,  -3461487620LL,  -3465383855LL,  -3469271936LL,  -3473151854LL,  -3477023598LL,
     -3480887161LL,  -3484742533LL,  -3488589706LL,  -3492428669LL,  -3496259414LL,  -3500081932LL,  -3503896214LL,  -3507702251LL,
     -3511500034LL,  -3515289554LL,  -3519070803LL,  -3522843770LL,  -3526608449LL,  -3530364828LL,  -3534112901LL,  -3537852657LL,
     -3541584088LL,  -3545307186LL,  -3549021941LL,  -3552728345LL,  -3556426389LL,  -3560116064LL,  -3563797363LL,  -3567470275LL,
     -3571134792LL,  -3574790907LL,  -3578438609LL,  -3582077892LL,  -3585708745LL,  -3589331160LL,  -3592945130LL,  -3596550645LL,
     -3600147697LL,  -3603736278LL,  -3607316378LL,  -3610887990LL,  -3614451106LL,  -3618005716LL,  -3621551813LL,  -3625089388LL,
     -3628618433LL,  -3632138939LL,  -3635650898LL,  -3639154303LL,  -3642649144LL,  -3646135414LL,  -3649613104LL,  -3653082206LL,
     -3656542712LL,  -3659994613LL,  -3663437903LL,  -3666872572LL,  -3670298613LL,  -3673716017LL,  -3677124776LL,  -3680524883LL,
     -3683916329LL,  -3687299106LL,  -3690673207LL,  -3694038624LL,  -3697395348LL,  -3700743371LL,  -3704082687LL,  -3707413286LL,
     -3710735162LL,  -3714048305LL,  -3717352710LL,  -3720648367LL,  -3723935269LL,  -3727213408LL,  -3730482776LL,  -3733743367LL,
     -3736995171LL,  -3740238183LL,  -3743472393LL,  -3746697794LL,  -3749914379LL,  -3753122140LL,  -3756321069LL,  -3759511160LL,
     -3762692404LL,  -3765864794LL,  -3769028322LL,  -3772182982LL,  -3775328765LL,  -3778465665LL,  -3781593674LL,  -3784712784LL,
     -3787822988LL,  -3790924279LL,  -3794016650LL,  -3797100093LL,  -3800174601LL,  -3803240167LL,  -3806296784LL,  -3809344444LL,
     -3812383140LL,  -3815412866LL,  -3818433613LL,  -3821445375LL,  -3824448145LL,  -3827441916LL,  -3830426680LL,  -3833402431LL,
     -3836369162LL,  -3839326865LL,  -3842275534LL,  -3845215161LL,  -3848145741LL,  -3851067265LL,  -3853979728LL,  -3856883122LL,
     -3859777440LL,  -3862662676LL,  -3865538822LL,  -3868405873LL,  -3871263820LL,  -3874112659LL,  -3876952381LL,  -3879782980LL,
     -3882604450LL,  -3885416784LL,  -3888219974LL,  -3891014016LL,  -3893798902LL,  -3896574625LL,  -3899341179LL,  -3902098557LL,
     -3904846754LL,  -3907585762LL,  -3910315575LL,  -3913036187LL,  -3915747591LL,  -3918449781LL,  -3921142750LL,  -3923826493LL,
     -3926501002LL,  -3929166272LL,  -3931822297LL,  -3934469069LL,  -3937106583LL,  -3939734833LL,  -3942353812LL,  -3944963515LL,
     -3947563934LL,  -3950155065LL,  -3952736900LL,  -3955309435LL,  -3957872662LL,  -3960426576LL,  -3962971170LL,  -3965506439LL,
     -3968032378LL,  -3970548979LL,  -3973056236LL,  -3975554145LL,  -3978042699LL,  -3980521892LL,  -3982991719LL,  -3985452174LL,
     -3987903250LL,  -3990344942LL,  -3992777245LL,  -3995200152LL,  -3997613658LL,  -4000017757LL,  -4002412444LL,  -4004797713LL,
     -4007173558LL,  -4009539974LL,  -4011896955LL,  -4014244496LL,  -4016582591LL,  -4018911234LL,  -4021230421LL,  -4023540145LL,
     -4025840401LL,  -4028131185LL,  -4030412489LL,  -4032684310LL,  -4034946641LL,  -4037199478LL,  -4039442815LL,  -4041676647LL,
     -4043900968LL,  -4046115773LL,  -4048321058LL,  -4050516816LL,  -4052703044LL,  -4054879734LL,  -4057046884LL,  -4059204486LL,
     -4061352537LL,  -4063491032LL,  -4065619964LL,  -4067739330LL,  -4069849124LL,  -4071949341LL,  -4074039976LL,  -4076121025LL,
     -4078192482LL,  -4080254343LL,  -4082306603LL,  -4084349257LL,  -4086382299LL,  -4088405726LL,  -4090419533LL,  -4092423715LL,
     -4094418266LL,  -4096403184LL,  -4098378461LL,  -4100344095LL,  -4102300081LL,  -4104246413LL,  -4106183088LL,  -4108110101LL,
     -4110027446LL,  -4111935121LL,  -4113833119LL,  -4115721438LL,  -4117600071LL,  -4119469016LL,  -4121328267LL,  -4123177820LL,
     -4125017671LL,  -4126847815LL,  -4128668249LL,  -4130478967LL,  -4132279966LL,  -4134071241LL,  -4135852789LL,  -4137624604LL,
     -4139386683LL,  -4141139022LL,  -4142881616LL,  -4144614462LL,  -4146337555LL,  -4148050891LL,  -4149754467LL,  -4151448277LL,
     -4153132319LL,  -4154806588LL,  -4156471081LL,  -4158125793LL,  -4159770720LL,  -4161405860LL,  -4163031206LL,  -4164646757LL,
     -4166252509LL,  -4167848456LL,  -4169434596LL,  -4171010925LL,  -4172577440LL,  -4174134136LL,  -4175681009LL,  -4177218057LL,
     -4178745276LL,  -4180262661LL,  -4181770210LL,  -4183267919LL,  -4184755784LL,  -4186233802LL,  -4187701970LL,  -4189160283LL,
     -4190608739LL,  -4192047334LL,  -4193476065LL,  -4194894928LL,  -4196303920LL,  -4197703038LL,  -4199092278LL,  -4200471637LL,
     -4201841112LL,  -4203200700LL,  -4204550397LL,  -4205890201LL,  -4207220108LL,  -4208540114LL,  -4209850218LL,  -4211150416LL,
     -4212440704LL,  -4213721080LL,  -4214991540LL,  -4216252083LL,  -4217502704LL,  -4218743401LL,  -4219974170LL,  -4221195010LL,
     -4222405917LL,  -4223606888LL,  -4224797921LL,  -4225979012LL,  -4227150159LL,  -4228311359LL,  -4229462610LL,  -4230603908LL,
     -4231735252LL,  -4232856637LL,  -4233968062LL,  -4235069525LL,  -4236161021LL,  -4237242550LL,  -4238314108LL,  -4239375693LL,
     -4240427302LL,  -4241468933LL,  -4242500584LL,  -4243522251LL,  -4244533933LL,  -4245535628LL,  -4246527332LL,  -4247509043LL,
     -4248480760LL,  -4249442480LL,  -4250394200LL,  -4251335919LL,  -4252267634LL,  -4253189343LL,  -4254101044LL,  -4255002735LL,
     -4255894413LL,  -4256776076LL,  -4257647723LL,  -4258509352LL,  -4259360959LL,  -4260202544LL,  -4261034104LL,  -4261855638LL,
     -4262667143LL,  -4263468618LL,  -4264260060LL,  -4265041468LL,  -4265812840LL,  -4266574174LL,  -4267325469LL,  -4268066722LL,
     -4268797931LL,  -4269519096LL,  -4270230215LL,  -4270931285LL,  -4271622305LL,  -4272303274LL,  -4272974189LL,  -4273635050LL,
     -4274285855LL,  -4274926601LL,  -4275557289LL,  -4276177915LL,  -4276788480LL,  -4277388980LL,  -4277979416LL,  -4278559785LL,
     -4279130086LL,  -4279690318LL,  -4280240479LL,  -4280780569LL,  -4281310585LL,  -4281830528LL,  -4282340394LL,  -4282840184LL,
     -4283329896LL,  -4283809529LL,  -4284279082LL,  -4284738553LL,  -4285187942LL,  -4285627247LL,  -4286056468LL,  -4286475604LL,
     -4286884652LL,  -4287283614LL,  -4287672487LL,  -4288051271LL,  -4288419964LL,  -4288778567LL,  -4289127078LL,  -4289465495LL,
     -4289793820LL,  -4290112050LL,  -4290420185LL,  -4290718224LL,  -4291006167LL,  -4291284012LL,  -4291551760LL,  -4291809410LL,
     -4292056960LL,  -4292294411LL,  -4292521761LL,  -4292739011LL,  -4292946160LL,  -4293143206LL,  -4293330151LL,  -4293506993LL,
     -4293673732LL,  -4293830368LL,  -4293976900LL,  -4294113327LL,  -4294239650LL,  -4294355869LL,  -4294461982LL,  -4294557990LL,
     -4294643893LL,  -4294719690LL,  -4294785381LL,  -4294840966LL,  -4294886444LL,  -4294921817LL,  -4294947083LL,  -4294962243LL,
     -4294967296LL,  -4294962243LL,  -4294947083LL,  -4294921817LL,  -4294886444LL,  -4294840966LL,  -4294785381LL,  -4294719690LL,
     -4294643893LL,  -4294557990LL,  -4294461982LL,  -4294355869LL,  -4294239650LL,  -4294113327LL,  -4293976900LL,  -4293830368LL,
     -4293673732LL,  -4293506993LL,  -4293330151LL,  -4293143206LL,  -4292946160LL,  -4292739011LL,  -4292521761LL,  -4292294411LL,
     -4292056960LL,  -4291809410LL,  -4291551760LL,  -4291284012LL,  -4291006167LL,  -4290718224LL,  -4290420185LL,  -4290112050LL,
     -4289793820LL,  -4289465495LL,  -4289127078LL,  -4288778567LL,  -4288419964LL,  -4288051271LL,  -4287672487LL,  -4287283614LL,
     -4286884652LL,  -4286475604LL,  -4286056468LL,  -4285627247LL,  -4285187942LL,  -4284738553LL,  -4284279082LL,  -4283809529LL,
     -4283329896LL,  -4282840184LL,  -4282340394LL,  -4281830528LL,  -4281310585LL,  -4280780569LL,  -4280240479LL,  -4279690318LL,
     -4279130086LL,  -4278559785LL,  -4277979416LL,  -4277388980LL,  -4276788480LL,  -4276177915LL,  -4275557289LL,  -4274926601LL,
     -4274285855LL,  -4273635050LL,  -4272974189LL,  -4272303274LL,  -4271622305LL,  -4270931285LL,  -4270230215LL,  -4269519096LL,
     -4268797931LL,  -4268066722LL,  -4267325469LL,  -4266574174LL,  -4265812840LL,  -4265041468LL,  -4264260060LL,  -4263468618LL,
     -4262667143LL,  -4261855638LL,  -4261034104LL,  -4260202544LL,  -4259360959LL,  -4258509352LL,  -4257647723LL,  -4256776076LL,
     -4255894413LL,  -4255002735LL,  -4254101044LL,  -4253189343LL,  -4252267634LL,  -4251335919LL,  -4250394200LL,  -4249442480LL,
     -4248480760LL,  -4247509043LL,  -4246527332LL,  -4245535628LL,  -4244533933LL,  -4243522251LL,  -4242500584LL,  -4241468933LL,
     -4240427302LL,  -4239375693LL,  -4238314108LL,  -4237242550LL,  -4236161021LL,  -4235069525LL,  -4233968062LL,  -4232856637LL,
     -4231735252LL,  -4230603908LL,  -4229462610LL,  -4228311359LL,  -4227150159LL,  -4225979012LL,  -4224797921LL,  -4223606888LL,
     -4222405917LL,  -4221195010LL,  -4219974170LL,  -4218743401LL,  -4217502704LL,  -4216252083LL,  -4214991540LL,  -4213721080LL,
     -4212440704LL,  -4211150416LL,  -4209850218LL,  -4208540114LL,  -4207220108LL,  -4205890201LL,  -4204550397LL,  -4203200700LL,
     -4201841112LL,  -4200471637LL,  -4199092278LL,  -4197703038LL,  -4196303920LL,  -4194894928LL,  -4193476065LL,  -4192047334LL,
     -4190608739LL,  -4189160283LL,  -4187701970LL,  -4186233802LL,  -4184755784LL,  -4183267919LL,  -4181770210LL,  -4180262661LL,
     -4178745276LL,  -4177218057LL,  -4175681009LL,  -4174134136LL,  -4172577440LL,  -4171010925LL,  -4169434596LL,  -4167848456LL,
     -4166252509LL,  -4164646757LL,  -4163031206LL,  -4161405860LL,  -4159770720LL,  -4158125793LL,  -4156471081LL,  -4154806588LL,
     -4153132319LL,  -4151448277LL,  -4149754467LL,  -4148050891LL,  -4146337555LL,  -4144614462LL,  -4142881616LL,  -4141139022LL,
     -4139386683LL,  -4137624604LL,  -4135852789LL,  -4134071241LL,  -4132279966LL,  -4130478967LL,  -4128668249LL,  -4126847815LL,
     -4125017671LL,  -4123177820LL,  -4121328267LL,  -4119469016LL,  -4117600071LL,  -4115721438LL,  -4113833119LL,  -4111935121LL,
     -4110027446LL,  -4108110101LL,  -4106183088LL,  -4104246413LL,  -4102300081LL,  -4100344095LL,  -4098378461LL,  -4096403184LL,
     -4094418266LL,  -4092423715LL,  -4090419533LL,  -4088405726LL,  -4086382299LL,  -4084349257LL,  -4082306603LL,  -4080254343LL,
     -4078192482LL,  -4076121025LL,  -4074039976LL,  -4071949341LL,  -4069849124LL,  -4067739330LL,  -4065619964LL,  -4063491032LL,
     -4061352537LL,  -4059204486LL,  -4057046884LL,  -4054879734LL,  -4052703044LL,  -4050516816LL,  -4048321058LL,  -4046115773LL,
     -4043900968LL,  -4041676647LL,  -4039442815LL,  -4037199478LL,  -4034946641LL,  -4032684310LL,  -4030412489LL,  -4028131185LL,
     -4025840401LL,  -4023540145LL,  -4021230421LL,  -4018911234LL,  -4016582591LL,  -4014244496LL,  -4011896955LL,  -4009539974LL,
     -4007173558LL,  -4004797713LL,  -4002412444LL,  -4000017757LL,  -3997613658LL,  -3995200152LL,  -3992777245LL,  -3990344942LL,
     -3987903250LL,  -3985452174LL,  -3982991719LL,  -3980521892LL,  -3978042699LL,  -3975554145LL,  -3973056236LL,  -3970548979LL,
     -3968032378LL,  -3965506439LL,  -3962971170LL,  -3960426576LL,  -3957872662LL,  -3955309435LL,  -3952736900LL,  -3950155065LL,
     -3947563934LL,  -3944963515LL,  -3942353812LL,  -3939734833LL,  -3937106583LL,  -3934469069LL,  -3931822297LL,  -3929166272LL,
     -3926501002LL,  -3923826493LL,  -3921142750LL,  -3918449781LL,  -3915747591LL,  -3913036187LL,  -3910315575LL,  -3907585762LL,
     -3904846754LL,  -3902098557LL,  -3899341179LL,  -3896574625LL,  -3893798902LL,  -3891014016LL,  -3888219974LL,  -3885416784LL,
     -3882604450LL,  -3879782980LL,  -3876952381LL,  -3874112659LL,  -3871263820LL,  -3868405873LL,  -3865538822LL,  -3862662676LL,
     -3859777440LL,  -3856883122LL,  -3853979728LL,  -3851067265LL,  -3848145741LL,  -3845215161LL,  -3842275534LL,  -3839326865LL,
     -3836369162LL,  -3833402431LL,  -3830426680LL,  -3827441916LL,  -3824448145LL,  -3821445375LL,  -3818433613LL,  -3815412866LL,
     -3812383140LL,  -3809344444LL,  -3806296784LL,  -3803240167LL,  -3800174601LL,  -3797100093LL,  -3794016650LL,  -3790924279LL,
     -3787822988LL,  -3784712784LL,  -3781593674LL,  -3778465665LL,  -3775328765LL,  -3772182982LL,  -3769028322LL,  -3765864794LL,
     -3762692404LL,  -3759511160LL,  -3756321069LL,  -3753122140LL,  -3749914379LL,  -3746697794LL,  -3743472393LL,  -3740238183LL,
     -3736995171LL,  -3733743367LL,  -3730482776LL,  -3727213408LL,  -3723935269LL,  -3720648367LL,  -3717352710LL,  -3714048305LL,
     -3710735162LL,  -3707413286LL,  -3704082687LL,  -3700743371LL,  -3697395348LL,  -3694038624LL,  -3690673207LL,  -3687299106LL,
     -3683916329LL,  -3680524883LL,  -3677124776LL,  -3673716017LL,  -3670298613LL,  -3666872572LL,  -3663437903LL,  -3659994613LL,
     -3656542712LL,  -3653082206LL,  -3649613104LL,  -3646135414LL,  -3642649144LL,  -3639154303LL,  -3635650898LL,  -3632138939LL,
     -3628618433LL,  -3625089388LL,  -3621551813LL,  -3618005716LL,  -3614451106LL,  -3610887990LL,  -3607316378LL,  -3603736278LL,
     -3600147697LL,  -3596550645LL,  -3592945130LL,  -3589331160LL,  -3585708745LL,  -3582077892LL,  -3578438609LL,  -3574790907LL,
     -3571134792LL,  -3567470275LL,  -3563797363LL,  -3560116064LL,  -3556426389LL,  -3552728345LL,  -3549021941LL,  -3545307186LL,
     -3541584088LL,  -3537852657LL,  -3534112901LL,  -3530364828LL,  -3526608449LL,  -3522843770LL,  -3519070803LL,  -3515289554LL,
     -3511500034LL,  -3507702251LL,  -3503896214LL,  -3500081932LL,  -3496259414LL,  -3492428669LL,  -3488589706LL,  -3484742533LL,
     -3480887161LL,  -3477023598LL,  -3473151854LL,  -3469271936LL,  -3465383855LL,  -3461487620LL,  -3457583240LL,  -3453670723LL,
     -3449750080LL,  -3445821319LL,  -3441884449LL,  -3437939481LL,  -3433986423LL,  -3430025284LL,  -3426056074LL,  -3422078802LL,
     -3418093478LL,  -3414100111LL,  -3410098710LL,  -3406089285LL,  -3402071844LL,  -3398046399LL,  -3394012957LL,  -3389971529LL,
     -3385922125LL,  -3381864752LL,  -3377799422LL,  -3373726144LL,  -3369644927LL,  -3365555780LL,  -3361458715LL,  -3357353739LL,
     -3353240863LL,  -3349120097LL,  -3344991450LL,  -3340854932LL,  -3336710553LL,  -3332558322LL,  -3328398249LL,  -3324230344LL,
     -3320054617LL,  -3315871077LL,  -3311679735LL,  -3307480600LL,  -3303273682LL,  -3299058992LL,  -3294836538LL,  -3290606332LL,
     -3286368382LL,  -3282122699LL,  -3277869293LL,  -3273608174LL,  -3269339351LL,  -3265062836LL,  -3260778637LL,  -3256486766LL,
     -3252187232LL,  -3247880045LL,  -3243565216LL,  -3239242754LL,  -3234912670LL,  -3230574973LL,  -3226229675LL,  -3221876786LL,
     -3217516315LL,  -3213148273LL,  -3208772670LL,  -3204389516LL,  -3199998822LL,  -3195600598LL,  -3191194855LL,  -3186781603LL,
     -3182360851LL,  -3177932612LL,  -3173496894LL,  -3169053709LL,  -3164603066LL,  -3160144978LL,  -3155679453LL,  -3151206502LL,
     -3146726136LL,  -3142238366LL,  -3137743202LL,  -3133240654LL,  -3128730733LL,  -3124213451LL,  -3119688816LL,  -3115156841LL,
     -3110617535LL,  -3106070910LL,  -3101516976LL,  -3096955744LL,  -3092387225LL,  -3087811428LL,  -3083228366LL,  -3078638049LL,
     -3074040487LL,  -3069435692LL,  -3064823674LL,  -3060204445LL,  -3055578014LL,  -3050944393LL,  -3046303593LL,  -3041655625LL,
     -3037000500LL,  -3032338228LL,  -3027668821LL,  -3022992289LL,  -3018308645LL,  -3013617897LL,  -3008920059LL,  -3004215140LL,
     -2999503152LL,  -2994784105LL,  -2990058012LL,  -2985324883LL,  -2980584729LL,  -2975837561LL,  -2971083391LL,  -2966322230LL,
     -2961554089LL,  -2956778979LL,  -2951996911LL,  -2947207897LL,  -2942411948LL,  -2937609075LL,  -2932799290LL,  -2927982603LL,
     -2923159027LL,  -2918328572LL,  -2913491250LL,  -2908647073LL,  -2903796051LL,  -2898938196LL,  -2894073520LL,  -2889202033LL,
     -2884323748LL,  -2879438676LL,  -2874546829LL,  -2869648217LL,  -2864742853LL,  -2859830747LL,  -2854911913LL,  -2849986360LL,
     -2845054101LL,  -2840115147LL,  -2835169511LL,  -2830217203LL,  -2825258235LL,  -2820292619LL,  -2815320366LL,  -2810341489LL,
     -2805355999LL,  -2800363908LL,  -2795365227LL,  -2790359968LL,  -2785348143LL,  -2780329764LL,  -2775304843LL,  -2770273391LL,
     -2765235421LL,  -2760190943LL,  -2755139971LL,  -2750082515LL,  -2745018589LL,  -2739948203LL,  -2734871369LL,  -2729788101LL,
     -2724698408LL,  -2719602305LL,  -2714499801LL,  -2709390911LL,  -2704275644LL,  -2699154015LL,  -2694026034LL,  -2688891714LL,
     -2683751066LL,  -2678604104LL,  -2673450838LL,  -2668291281LL,  -2663125446LL,  -2657953344LL,  -2652774988LL,  -2647590390LL,
     -2642399561LL,  -2637202515LL,  -2631999263LL,  -2626789817LL,  -2621574191LL,  -2616352396LL,  -2611124444LL,  -2605890348LL,
     -2600650120LL,  -2595403773LL,  -2590151318LL,  -2584892768LL,  -2579628136LL,  -2574357434LL,  -2569080674LL,  -2563797869LL,
     -2558509031LL,  -2553214173LL,  -2547913306LL,  -2542606444LL,  -2537293599LL,  -2531974784LL,  -2526650010LL,  -2521319292LL,
     -2515982640LL,  -2510640068LL,  -2505291588LL,  -2499937213LL,  -2494576955LL,  -2489210827LL,  -2483838842LL,  -2478461013LL,
     -2473077351LL,  -2467687870LL,  -2462292582LL,  -2456891500LL,  -2451484637LL,  -2446072005LL,  -2440653617LL,  -2435229487LL,
     -2429799626LL,  -2424364047LL,  -2418922764LL,  -2413475788LL,  -2408023134LL,  -2402564813LL,  -2397100839LL,  -2391631224LL,
     -2386155981LL,  -2380675124LL,  -2375188665LL,  -2369696616LL,  -2364198992LL,  -2358695804LL,  -2353187066LL,  -2347672791LL,
     -2342152991LL,  -2336627680LL,  -2331096871LL,  -2325560576LL,  -2320018810LL,  -2314471584LL,  -2308918911LL,  -2303360806LL,
     -2297797281LL,  -2292228349LL,  -2286654023LL,  -2281074316LL,  -2275489241LL,  -2269898812LL,  -2264303042LL,  -2258701944LL,
     -2253095531LL,  -2247483816LL,  -2241866812LL,  -2236244534LL,  -2230616993LL,  -2224984203LL,  -2219346178LL,  -2213702930LL,
     -2208054473LL,  -2202400821LL,  -2196741986LL,  -2191077981LL,  -2185408821LL,  -2179734519LL,  -2174055087LL,  -2168370540LL,
     -2162680890LL,  -2156986152LL,  -2151286337LL,  -2145581461LL,  -2139871536LL,  -2134156575LL,  -2128436593LL,  -2122711602LL,
     -2116981616LL,  -2111246649LL,  -2105506713LL,  -2099761824LL,  -2094011993LL,  -2088257235LL,  -2082497563LL,  -2076732991LL,
     -2070963532LL,  -2065189200LL,  -2059410008LL,  -2053625970LL,  -2047837100LL,  -2042043411LL,  -2036244917LL,  -2030441631LL,
     -2024633568LL,  -2018820741LL,  -2013003163LL,  -2007180848LL,  -2001353810LL,  -1995522063LL,  -1989685620LL,  -1983844495LL,
     -1977998702LL,  -1972148255LL,  -1966293167LL,  -1960433452LL,  -1954569124LL,  -1948700196LL,  -1942826684LL,  -1936948599LL,
     -1931065957LL,  -1925178771LL,  -1919287054LL,  -1913390821LL,  -1907490086LL,  -1901584863LL,  -1895675165LL,  -1889761006LL,
     -1883842400LL,  -1877919361LL,  -1871991904LL,  -1866060042LL,  -1860123788LL,  -1854183158LL,  -1848238164LL,  -1842288821LL,
     -1836335144LL,  -1830377145LL,  -1824414839LL,  -1818448240LL,  -1812477362LL,  -1806502219LL,  -1800522825LL,  -1794539195LL,
     -1788551342LL,  -1782559280LL,  -1776563023LL,  -1770562587LL,  -1764557983LL,  -1758549228LL,  -1752536335LL,  -1746519318LL,
     -1740498191LL,  -1734472968LL,  -1728443664LL,  -1722410293LL,  -1716372869LL,  -1710331406LL,  -1704285919LL,  -1698236421LL,
     -1692182927LL,  -1686125451LL,  -1680064008LL,  -1673998611LL,  -1667929275LL,  -1661856014LL,  -1655778843LL,  -1649697776LL,
     -1643612827LL,  -1637524010LL,  -1631431340LL,  -1625334831LL,  -1619234497LL,  -1613130353LL,  -1607022414LL,  -1600910693LL,
     -1594795204LL,  -1588675964LL,  -1582552984LL,  -1576426281LL,  -1570295869LL,  -1564161761LL,  -1558023973LL,  -1551882518LL,
     -1545737412LL,  -1539588668LL,  -1533436302LL,  -1527280328LL,  -1521120759LL,  -1514957611LL,  -1508790899LL,  -1502620636LL,
     -1496446837LL,  -1490269517LL,  -1484088690LL,  -1477904371LL,  -1471716574LL,  -1465525315LL,  -1459330606LL,  -1453132464LL,
     -1446930903LL,  -1440725936LL,  -1434517580LL,  -1428305848LL,  -1422090755LL,  -1415872315LL,  -1409650544LL,  -1403425456LL,
     -1397197066LL,  -1390965388LL,  -1384730436LL,  -1378492227LL,  -1372250773LL,  -1366006091LL,  -1359758194LL,  -1353507098LL,
     -1347252816LL,  -1340995365LL,  -1334734758LL,  -1328471010LL,  -1322204136LL,  -1315934151LL,  -1309661069LL,  -1303384906LL,
     -1297105676LL,  -1290823393LL,  -1284538073LL,  -1278249730LL,  -1271958380LL,  -1265664036LL,  -1259366714LL,  -1253066429LL,
     -1246763195LL,  -1240457028LL,  -1234147941LL,  -1227835951LL,  -1221521071LL,  -1215203317LL,  -1208882703LL,  -1202559245LL,
     -1196232957LL,  -1189903854LL,  -1183571952LL,  -1177237264LL,  -1170899806LL,  -1164559593LL,  -1158216639LL,  -1151870960LL,
     -1145522571LL,  -1139171486LL,  -1132817720LL,  -1126461289LL,  -1120102207LL,  -1113740490LL,  -1107376152LL,  -1101009208LL,
     -1094639673LL,  -1088267562LL,  -1081892891LL,  -1075515674LL,  -1069135926LL,  -1062753662LL,  -1056368897LL,  -1049981647LL,
     -1043591926LL,  -1037199750LL,  -1030805132LL,  -1024408090LL,  -1018008636LL,  -1011606787LL,  -1005202558LL,   -998795963LL,
      -992387019LL,   -985975738LL,   -979562138LL,   -973146233LL,   -966728038LL,   -960307568LL,   -953884839LL,   -947459865LL,
      -941032661LL,   -934603243LL,   -928171626LL,   -921737825LL,   -915301854LL,   -908863731LL,   -902423468LL,   -895981082LL,
      -889536587LL,   -883090000LL,   -876641334LL,   -870190606LL,   -863737830LL,   -857283021LL,   -850826195LL,   -844367368LL,
      -837906553LL,   -831443766LL,   -824979024LL,   -818512339LL,   -812043729LL,   -805573208LL,   -799100792LL,   -792626495LL,
      -786150333LL,   -779672321LL,   -773192474LL,   -766710808LL,   -760227338LL,   -753742079LL,   -747255046LL,   -740766255LL,
      -734275721LL,   -727783459LL,   -721289485LL,   -714793813LL,   -708296459LL,   -701797439LL,   -695296767LL,   -688794459LL,
      -682290530LL,   -675784996LL,   -669277872LL,   -662769172LL,   -656258914LL,   -649747111LL,   -643233779LL,   -636718933LL,
      -630202589LL,   -623684762LL,   -617165468LL,   -610644721LL,   -604122538LL,   -597598933LL,   -591073921LL,   -584547519LL,
      -578019742LL,   -571490604LL,   -564960121LL,   -558428309LL,   -551895183LL,   -545360759LL,   -538825051LL,   -532288075LL,
      -525749847LL,   -519210381LL,   -512669694LL,   -506127800LL,   -499584716LL,   -493040456LL,   -486495035LL,   -479948470LL,
      -473400776LL,   -466851967LL,   -460302060LL,   -453751070LL,   -447199012LL,   -440645902LL,   -434091755LL,   -427536587LL,
      -420980412LL,   -414423247LL,   -407865107LL,   -401306007LL,   -394745962LL,   -388184989LL,   -381623102LL,   -375060318LL,
      -368496651LL,   -361932116LL,   -355366730LL,   -348800508LL,   -342233465LL,   -335665617LL,   -329096979LL,   -322527566LL,
      -315957395LL,   -309386480LL,   -302814837LL,   -296242481LL,   -289669429LL,   -283095695LL,   -276521294LL,   -269946243LL,
      -263370557LL,   -256794251LL,   -250217341LL,   -243639842LL,   -237061769LL,   -230483139LL,   -223903967LL,   -217324267LL,
      -210744057LL,   -204163350LL,   -197582163LL,   -191000511LL,   -184418409LL,   -177835874LL,   -171252920LL,   -164669563LL,
      -158085819LL,   -151501702LL,   -144917230LL,   -138332416LL,   -131747276LL,   -125161827LL,   -118576083LL,   -111990060LL,
      -105403774LL,    -98817239LL,    -92230472LL,    -85643488LL,    -79056303LL,    -72468931LL,    -65881389LL,    -59293692LL,
       -52705856LL,    -46117895LL,    -39529826LL,    -32941664LL,    -26353424LL,    -19765122LL,    -13176774LL,     -6588395LL
};

/* qnum_sin: 4096-entry LUT + four-point Catmull-Rom cubic interpolation.
 * Only angle.fraction is used; integer field (whole turns) is ignored.
 * Output is exact in cardinal positions; ~2^-30 absolute error elsewhere. */
static inline qnum_t qnum_sin(qnum_t angle) {
    uint32_t frac = angle.fraction;
    uint32_t idx  = frac >> 20;            /* 12 bits → 0..4095 */
    int64_t  sub  = (int64_t)(frac & 0xFFFFFu);  /* 20-bit sub-fraction */

    int64_t p0 = qnum_sin_lut[(idx - 1u) & 0xFFFu];
    int64_t p1 = qnum_sin_lut[idx];
    int64_t p2 = qnum_sin_lut[(idx + 1u) & 0xFFFu];
    int64_t p3 = qnum_sin_lut[(idx + 2u) & 0xFFFu];

    int64_t a0 = 2*p1;
    int64_t a1 = -p0 + p2;
    int64_t a2 = 2*p0 - 5*p1 + 4*p2 - p3;
    int64_t a3 = -p0 + 3*p1 - 3*p2 + p3;

    /* Horner's method. Right-shifts assume arithmetic shift on signed
     * int64 — universal on all real C99 targets (same assumption the
     * Q4.28 trig already makes). */
    int64_t y = a3;
    y = (y * sub) >> 20;
    y += a2;
    y = (y * sub) >> 20;
    y += a1;
    y = (y * sub) >> 20;
    y += a0;
    y >>= 1;                               /* the Catmull-Rom 0.5 */

    qnum_t result;
    result.integer  = (int32_t)(y >> 32);
    result.fraction = (uint32_t)y;
    return result;
}

/* qnum_cos(x) = qnum_sin(x + 0.25 turn). Carry from the fraction add into
 * the integer field is intentionally discarded — qnum_sin only consults
 * the fraction. */
static inline qnum_t qnum_cos(qnum_t angle) {
    qnum_t a;
    a.integer  = 0;
    a.fraction = angle.fraction + 0x40000000u;
    return qnum_sin(a);
}

#endif /* QNUM_H */
