#ifndef SMG2_COMPAT_REVOLUTION_TYPES_H
#define SMG2_COMPAT_REVOLUTION_TYPES_H

// Delegate core scalar typedefs (s8..u64, f32, f64, BOOL) and macros to Nebula's prelude/types
// and avoid Garigari's CodeWarrior-only `#define nullptr 0` and `#define override`.
#include <nebula_types.h>

#ifndef SMG2_EXTRA_TYPES_DEFINED
#define SMG2_EXTRA_TYPES_DEFINED
typedef volatile u8 vu8;
typedef volatile u16 vu16;
typedef volatile u64 vu64;
typedef volatile s8 vs8;
typedef volatile s16 vs16;
typedef volatile s64 vs64;
typedef volatile f32 vf32;
typedef volatile f64 vf64;
#endif

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

#endif  // SMG2_COMPAT_REVOLUTION_TYPES_H
