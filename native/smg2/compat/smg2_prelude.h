#ifndef SMG2_PRELUDE_H
#define SMG2_PRELUDE_H

#include <nebula_prelude.h>

typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u64 vu64;
typedef volatile s8 vs8;
typedef volatile s16 vs16;
typedef volatile s64 vs64;
typedef volatile f32 vf32;
typedef volatile f64 vf64;

#ifndef __REGISTER
#define __REGISTER
#endif
#ifndef AT_ADDRESS
#define AT_ADDRESS(x)
#endif
#ifndef ALWAYS_INLINE
#define ALWAYS_INLINE __attribute__((always_inline))
#endif
#ifndef NO_INLINE
#define NO_INLINE __attribute__((noinline))
#endif
#ifndef ATTRIBUTE_ALIGN
#define ATTRIBUTE_ALIGN(num) __attribute__((aligned(num)))
#endif
#ifndef ATTRIBUTE_PACKED
#define ATTRIBUTE_PACKED __attribute__((packed))
#endif
#ifndef ATTRIBUTE_WEAK
#define ATTRIBUTE_WEAK __attribute__((weak))
#endif

// CodeWarrior PowerPC intrinsic replacements for x86-64 Clang.
#ifdef __cplusplus
inline double __fabs(double x) { return fabs(x); }
inline float __fabsf(float x) { return fabsf(x); }
inline int __abs(int x) { return x < 0 ? -x : x; }
inline double __frsqrte(double x) { return 1.0 / sqrt(x); }
inline int __fpclassifyf(float x) {
    if (__builtin_isnan(x)) return 1;
    if (__builtin_isinf(x)) return 2;
    if (x == 0.0f) return 0;
    return 4;
}
#else
static inline double __fabs(double x) { return fabs(x); }
static inline float __fabsf(float x) { return fabsf(x); }
static inline int __abs(int x) { return x < 0 ? -x : x; }
static inline double __frsqrte(double x) { return 1.0 / sqrt(x); }
#endif

#ifdef __cplusplus
#include <cstddef>
#include <cstring>
#include <new>
#include <utility>
// Garigari's JSystem headers still use C++98 `register` storage specifier in a few places.
#define register
#endif

// Force our native compat Revolution headers ahead of Garigari's quoted includes inside libs/RVL_SDK/include.
#include <revolution/types.h>
#include <revolution/mtx.h>
#include <revolution/os.h>
#include <revolution/gx/GXVert.h>

#endif  // SMG2_PRELUDE_H
