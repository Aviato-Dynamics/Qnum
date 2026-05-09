# QNum

Fixed-point arithmetic in two precision tiers. Pure C99, header-only, zero dependencies, no undefined behaviour on any defined input within each tier's stated contract.

```c
#include "qnum.h"
```

That's the installation.

---

## Two tiers, one header

**Q4.28** (`q4_28_t = int32_t`) — range ±8, 28 fractional bits. Wrapping arithmetic by design: the phase accumulator in the FM operator relies on it. For real-time synthesis, game logic, and embedded targets without an FPU. GBA-compatible throughout.

**Q32.32** (`qnum_t = struct`) — range ±2.1 × 10⁹, 32 fractional bits. Saturating arithmetic throughout. For cases where Q4.28's ceiling or precision aren't enough: accumulated simulation state, high-resolution DSP, anything that drifts at Q4.28.

A cross-tier bridge (`qnum_from_q4_28`, `qnum_to_q4_28_raw`) makes explicit promotion and demotion lossless for all Q4.28-representable values.

---

## Precision at a glance

|                      | Q4.28                          | Q32.32                          |
|----------------------|--------------------------------|---------------------------------|
| Storage              | `int32_t`                      | `struct { int32_t; uint32_t; }` |
| Range                | ±8                             | ±2.147 × 10⁹                    |
| Fractional bits      | 28                             | 32                              |
| Overflow             | Wraps (intentional)            | Saturates                       |
| sin / cos error      | ~2⁻¹⁶ (256-entry, linear)      | ~2⁻³⁰ (4096-entry, cubic)       |
| Target platform      | Any C99, FPU optional          | C99, `int64_t` required         |

---

## API reference

### Q4.28 tier

Angle convention: 2²⁸ = one full turn. `Q_ONE = 0x10000000`.

| Function | Description |
|---|---|
| `int_to_q(i)` | Integer → Q4.28 |
| `q_to_int(q)` | Q4.28 → integer (truncates toward −∞) |
| `float_to_q(f)` | Double → Q4.28, clamped |
| `q_to_float(q)` | Q4.28 → double |
| `q_add(a, b)` | Wrapping addition |
| `q_sub(a, b)` | Wrapping subtraction |
| `q_mul(a, b)` | Full-precision fixed-point multiply, truncates toward zero |
| `q_div(a, b)` | Fixed-point divide, saturates on divide-by-zero |
| `q_neg(a)` | Negate; `INT32_MIN` saturates to `INT32_MAX` |
| `q_abs(a)` | Absolute value; `INT32_MIN` saturates |
| `q_sin(angle)` | 256-entry LUT + linear interpolation |
| `q_cos(angle)` | `q_sin(angle + π/2)` |
| `q_fm_init(op, inc)` | Initialise FM operator |
| `q_fm_step(op, mod_in)` | Advance phase accumulator, return sin output |
| `q_perfect_fifth_ratio(base, n)` | `base × (3/2)^n`, n = 0..5 |

### Q32.32 tier

| Function | Description |
|---|---|
| `int_to_qnum(i)` | Integer → Q32.32 |
| `qnum_to_int(q)` | Q32.32 → integer (truncates) |
| `float_to_qnum(f)` | Double → Q32.32, clamped |
| `qnum_to_double(q)` | Q32.32 → double |
| `qnum_add(a, b)` | Saturating addition |
| `qnum_sub(a, b)` | Saturating subtraction |
| `qnum_mul(a, b)` | Full-precision via 32×32→64 partial products, no `__int128` |
| `qnum_div(a, b)` | Newton-Raphson division; saturates on divide-by-zero |
| `qnum_recip(a)` | 1/a, Newton-Raphson |
| `qnum_neg(a)` | Negate; saturates at `QNUM_MIN` |
| `qnum_abs(a)` | Absolute value |
| `qnum_lt/gt/le/ge/eq/ne(a, b)` | Comparison predicates, return `int` |
| `qnum_min(a, b)` | Minimum |
| `qnum_max(a, b)` | Maximum |
| `qnum_floor(a)` | Round toward −∞ |
| `qnum_ceil(a)` | Round toward +∞ |
| `qnum_round(a)` | Round half-up |
| `qnum_sqrt(a)` | Newton-Raphson rsqrt, 256-entry seed LUT, 3 iterations |
| `qnum_sin(angle)` | 4096-entry LUT + Catmull-Rom cubic interpolation |
| `qnum_cos(angle)` | `qnum_sin(angle + 0.25 turn)` |

### Cross-tier bridge

| Function | Description |
|---|---|
| `qnum_from_q4_28(raw)` | Q4.28 raw int32 → Q32.32, lossless |
| `qnum_to_q4_28_raw(q)` | Q32.32 → Q4.28 raw int32, saturates outside [−8, 7], lossy outside Q4.28 range |

---

## Usage

### FM synthesis (Q4.28)

```c
#include "qnum.h"

/* Phase increments: frequency / sample_rate in turns per sample. */
q_fm_op_t carrier, modulator;
q_fm_init(&modulator, float_to_q(330.0 / 44100.0));
q_fm_init(&carrier,   float_to_q(440.0 / 44100.0));

for (int i = 0; i < n_samples; i++) {
    q4_28_t mod_out = q_fm_step(&modulator, 0);
    sample[i] = q_fm_step(&carrier, q_mul(mod_out, float_to_q(0.5)));
}
```

### High-precision trig (Q32.32)

```c
/* Angle convention: qnum.fraction field encodes sub-turn position.
 * fraction = 0x40000000 = 0.25 turn = π/2. */
qnum_t angle = float_to_qnum(0.125);   /* 0.125 turns = π/4 */
qnum_t s = qnum_sin(angle);            /* 0.70710678..., err < 2^-30 */
qnum_t c = qnum_cos(angle);            /* 0.70710678..., err < 2^-30 */

/* Pythagoras holds to within a few ULP. */
qnum_t sum = qnum_add(qnum_mul(s, s), qnum_mul(c, c));
/* sum ≈ 1.0 */
```

### Cross-tier bridge

```c
q4_28_t q28 = float_to_q(1.5);
qnum_t  q32 = qnum_from_q4_28(q28);   /* exact: 1.5 */

qnum_t  big = float_to_qnum(3.14159);
q4_28_t raw = qnum_to_q4_28_raw(big); /* lossy — truncates fractional precision */
```

---

## Design notes

**No undefined behaviour.** Signed left shifts route through `uint32_t`. Division by zero saturates. `INT32_MIN` negation saturates. The wrapping arithmetic in Q4.28 add/sub is explicit and intentional; on all real C99 targets it does what it says in two's-complement.

**Q32.32 multiply without `__int128`.** `qnum_mul` uses four 32×32→64 partial products and carries. No compiler extensions; works on MSVC, GCC, and Clang without flags.

**Q32.32 trig precision.** A 256-entry linear LUT at Q32.32 would give ~2⁻¹⁶ — throwing away 16 of the 32 fractional bits and making the precision tier indistinguishable from qmath. The 4096-entry cubic LUT targets the format: 32 KiB, Catmull-Rom in int64, measured max absolute error ~2⁻³¹ in random sweeps against libm. Angle convention matches the Q4.28 tier (turns, not radians) so phase accumulation patterns compose across tiers.

**Arithmetic shift.** Both tiers assume arithmetic right shift for negative signed integers. This is implementation-defined by the C standard but is universal on every real target. The same assumption appears in Q4.28 `q_to_int` and the Newton-Raphson rsqrt — adding it to the cubic interpolation introduces no new portability constraint.

---

## Relationship to qmath

QNum grew from [orb42-qmath](https://github.com/Aviato-Dynamics/orb42-qmath). The Q4.28 tier is arithmetically identical to qmath; `qnum_from_q4_28` provides the upgrade path. If you only need Q4.28, qmath is the right library — its README tells that story. QNum's job is the precision tier.

---

## Non-goals

**GBA as the primary platform.** The Q4.28 tier runs on GBA; the Q32.32 tier needs `int64_t` and is not GBA-compatible. If the target is GBA exclusively, use qmath.

**Arbitrary precision.** QNum is bounded fixed-point. For unbounded integers or rationals, use a bignum library.

**Float replacement everywhere.** Floating-point is the right tool for many things. QNum is for deterministic, reproducible fixed-point arithmetic where floats aren't appropriate — not a protest vote against IEEE 754.

---

## Licence

MIT — Aviato Dynamics
