// Native x86-64 implementations of JSystem JMath lookup tables, JMath helpers,
// and portable C++ replacements for the PowerPC assembly blocks in MathUtil.cpp.
#include "JSystem/JMath.hpp"
#include "JSystem/JMath/JMATrigonometric.hpp"
#include "Game/Util/MathUtil.hpp"
#include <cmath>

namespace JMath {

template <>
TSinCosTable<14, f32>::TSinCosTable() {
    constexpr f64 kTwoPi = 6.28318530717958647692;
    for (u32 i = 0; i < LEN; ++i) {
        const f64 angle = (kTwoPi * static_cast<f64>(i)) / static_cast<f64>(LEN);
        table[i].a1 = static_cast<f32>(sin(angle));
        table[i].b1 = static_cast<f32>(cos(angle));
    }
}

template <>
TAtanTable<1024, f32>::TAtanTable() : _1000(0.0f) {
    for (s32 i = 0; i < 1024; ++i) {
        mTable[i] = static_cast<f32>(atan(static_cast<f64>(i) / 1024.0));
    }
}

template <>
TAsinAcosTable<1024, f32>::TAsinAcosTable() : _1000(0.0f) {
    for (s32 i = 0; i < 1024; ++i) {
        mTable[i] = static_cast<f32>(asin(static_cast<f64>(i) / 1024.0));
    }
}

TSinCosTable<14, f32> sSinCosTable{};
TAtanTable<1024, f32> sAtanTable{};
TAsinAcosTable<1024, f32> sAsinAcosTable{};

}  // namespace JMath

void JMAMTXApplyScale(const Mtx src, Mtx dst, f32 x, f32 y, f32 z) {
    NebulaMtxApplyScale(src, dst, x, y, z);
}

void JMAVECLerp(const Vec* a, const Vec* b, Vec* dst, f32 t) {
    NebulaVecLerp(a, b, dst, t);
}

void JMAVECScaleAdd(const Vec* a, const Vec* b, Vec* dst, f32 scale) {
    NebulaVecScaleAdd(a, b, dst, scale);
}

void JMAQuatLerp(const Quaternion* p, const Quaternion* q, f32 t, Quaternion* r) {
    NebulaQuatSlerp(p, q, r, t);
}

void JMAEulerToQuat(s16 rx, s16 ry, s16 rz, Quaternion* dst) {
    const f32 toRad = 3.14159265358979323846f / 32768.0f;
    TQuat4f q;
    q.setEuler(static_cast<f32>(rx) * toRad, static_cast<f32>(ry) * toRad, static_cast<f32>(rz) * toRad);
    *dst = q;
}

f32 JMASqrt(f32 x) {
    return x > 0.0f ? sqrtf(x) : x;
}

f32 PSVECKillElement(const Vec* pSrc, const Vec* pKill, const Vec* pDst) {
    if (pSrc == nullptr || pKill == nullptr || pDst == nullptr) {
        return 0.0f;
    }
    const f32 dot = NebulaVecDotProduct(pSrc, pKill);
    Vec* dst = const_cast<Vec*>(pDst);
    dst->x = pSrc->x - dot * pKill->x;
    dst->y = pSrc->y - dot * pKill->y;
    dst->z = pSrc->z - dot * pKill->z;
    return dot;
}

namespace MR {
namespace {
    u32 sRngState = 0x12345678u;
}

f32 fastSqrtf(f32 x) {
    return x > 0.0f ? sqrtf(x) : 0.0f;
}

f32 getRandom() {
    sRngState = sRngState * 1664525u + 1013904223u;
    return static_cast<f32>((sRngState >> 8) & 0xFFFFFFu) / 16777216.0f;
}

void vecScaleAdd(const TVec3f* pA1, const TVec3f* pA2, f32 a3) {
    if (pA1 == nullptr || pA2 == nullptr) {
        return;
    }
    TVec3f* dst = const_cast<TVec3f*>(pA1);
    dst->x += pA2->x * a3;
    dst->y += pA2->y * a3;
    dst->z += pA2->z * a3;
}

void PSvecBlend(const TVec3f* pA1, const TVec3f* pA2, TVec3f* pA3, f32 a4, f32 a5) {
    if (pA1 == nullptr || pA2 == nullptr || pA3 == nullptr) {
        return;
    }
    pA3->x = pA1->x * a4 + pA2->x * a5;
    pA3->y = pA1->y * a4 + pA2->y * a5;
    pA3->z = pA1->z * a4 + pA2->z * a5;
}

void vecBlend(TVec3f* pDst, const TVec3f& rFrom, const TVec3f& rTo, f32 rate) {
    PSvecBlend(&rFrom, &rTo, pDst, 1.0f - rate, rate);
}

f32 vecKillElement(const TVec3f& rSrc, const TVec3f& rKillDir, TVec3f* pDst) {
    if (isNearZero(rKillDir)) {
        *pDst = rSrc;
        return 0.0f;
    }
    return PSVECKillElement(&rSrc, &rKillDir, pDst);
}
}  // namespace MR
