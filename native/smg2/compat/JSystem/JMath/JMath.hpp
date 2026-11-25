#pragma once

#include <cstring>
#include <cmath>
#include <revolution/mtx.h>
#include <revolution/types.h>

void JMAMTXApplyScale(const Mtx, Mtx, f32, f32, f32);
void JMAVECLerp(const Vec*, const Vec*, Vec*, f32);
void JMAVECScaleAdd(const Vec*, const Vec*, Vec*, f32);
void JMAQuatLerp(const Quaternion*, const Quaternion*, f32, Quaternion*);
void JMAEulerToQuat(s16, s16, s16, Quaternion*);

inline f32 JMAFastSqrt(const f32 input) {
    return input > 0.0f ? sqrtf(input) : input;
}

inline f32 JMAHermiteInterpolation(f32 p1, f32 p2, f32 p3, f32 p4, f32 p5, f32 p6, f32 p7) {
    const f32 denom = p5 - p2;
    const f32 t = (p1 - p2) / denom;
    const f32 t2 = t * t;
    const f32 t3 = t2 * t;
    const f32 h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
    const f32 h01 = -2.0f * t3 + 3.0f * t2;
    const f32 h10 = t3 - 2.0f * t2 + t;
    const f32 h11 = t3 - t2;
    return h00 * p3 + h01 * p6 + denom * (h10 * p4 + h11 * p7);
}

namespace JMath {
    inline f32 fastReciprocal(f32 value) {
        return 1.0f / value;
    }

    template <typename T>
    inline T fastSqrt(T value) {
        return JMAFastSqrt(value);
    }

    inline void gekko_ps_copy3(void* dest, const void* src) {
        std::memcpy(dest, src, 12);
    }

    inline void gekko_ps_copy6(void* dest, const void* src) {
        std::memcpy(dest, src, 24);
    }

    inline void gekko_ps_copy12(void* pDest, const void* pSrc) {
        std::memcpy(pDest, pSrc, 48);
    }

    inline void gekko_ps_copy16(void* pDest, const void* pSrc) {
        std::memcpy(pDest, pSrc, 64);
    }
}  // namespace JMath

namespace JMathInlineVEC {
    using ::NebulaVecAdd;
    using ::NebulaVecSubtract;
    using ::NebulaVecDotProduct;

    inline void PSVECCopy(const Vec* src, Vec* dest) {
        dest->x = src->x;
        dest->y = src->y;
        dest->z = src->z;
    }

    inline f32 PSVECSquareMag(const Vec* src) {
        return ::NebulaVecSquareMag(src);
    }

    inline void PSVECNegate(const Vec* src, Vec* dst) {
        ::NebulaVecNegate(src, dst);
    }

    inline f32 PSVECSquareDistance(const Vec* a, const Vec* b) {
        return ::NebulaVecSquareDistance(a, b);
    }

    inline void PSVECMultiply(const Vec* vec1, const Vec* vec2, Vec* dst) {
        ::NebulaVecMultiply(vec1, vec2, dst);
    }
}  // namespace JMathInlineVEC

template <typename T>
inline T JMAMax(T param_0, T param_1) {
    return param_0 > param_1 ? param_0 : param_1;
}
