#pragma once

#include "math_types.hpp"
#include <revolution/types.h>

namespace JMath {
    template <typename A1, typename B1>
    struct JmaPair {
        A1 a1;
        B1 b1;
        JmaPair() : a1(), b1() {}
    };

    template <typename T>
    struct TAngleConstant_;

    template <>
    struct TAngleConstant_<f32> {
        static f32 RADIAN_DEG090() { return 1.5707964f; }
        static f32 RADIAN_DEG180() { return 3.1415927f; }
        static f32 RADIAN_DEG360() { return 6.2831855f; }
        static f32 RADIAN_TO_DEGREE_FACTOR() { return 180.0f / RADIAN_DEG180(); }
    };

    template <int Bits, typename T>
    class TSinCosTable {
    public:
        TSinCosTable();

        static const u32 LEN = 1 << Bits;
        JmaPair<T, T> table[LEN];

        T sinShort(s16 v) const { return table[static_cast<u16>(v) >> (16U - Bits)].a1; }
        T cosShort(s16 v) const { return table[static_cast<u16>(v) >> (16 - Bits)].b1; }
        inline T sin_(u16 v) { return table[v & (LEN - 1)].a1; }
        inline T cos_(u16 v) { return table[v & (LEN - 1)].b1; }

        inline T sinRadian(f32 v) {
            return v < 0.0f ? -sin_(v * -(LEN / TAngleConstant_<f32>::RADIAN_DEG360()))
                            : sin_(v * (LEN / TAngleConstant_<f32>::RADIAN_DEG360()));
        }
        inline T sinDegree(f32 v) {
            return v < 0.0f ? -sin_(v * -(LEN / 360.0f)) : sin_(v * (LEN / 360.0f));
        }
        inline T sinLap(f32 v) {
            return v < 0.0f ? -sin_(v * -(f32)LEN) : sin_(v * (f32)LEN);
        }
        inline T cosRadian(f32 v) {
            if (v < 0.0f) v = -v;
            return cos_(v * (LEN / TAngleConstant_<f32>::RADIAN_DEG360()));
        }
        inline T cosDegree(f32 v) {
            if (v < 0.0f) v = -v;
            return cos_(v * (LEN / 360.0f));
        }
        inline T cosLap(f32 v) {
            if (v < 0.0f) v = -v;
            return cos_(v * (f32)LEN);
        }
    };

    template <s32 Len, typename T>
    class TAtanTable {
    public:
        TAtanTable();
        T atan2_(T y, T x) const { return static_cast<T>(atan2f(static_cast<f32>(y), static_cast<f32>(x))); }
        T get_(T y, T x) const { return static_cast<T>(atan2f(static_cast<f32>(y), static_cast<f32>(x))); }
        T mTable[Len];
        T _1000;
    };

    template <s32 Len, typename T>
    class TAsinAcosTable {
    public:
        TAsinAcosTable();
        T get_(T y, T x) const { return static_cast<T>(atan2f(static_cast<f32>(y), static_cast<f32>(x))); }
        T asin_(T x) const {
            if (x >= 1.0f) return TAngleConstant_<T>::RADIAN_DEG090();
            if (x <= -1.0f) return -TAngleConstant_<T>::RADIAN_DEG090();
            if (x < 0.0f) return -mTable[(u32)(-x * 1023.5f)];
            return mTable[(u32)(x * 1023.5f)];
        }
        T acos_(T x) const {
            if (x >= 1.0f) return 0.0f;
            if (x <= -1.0f) return TAngleConstant_<T>::RADIAN_DEG180();
            if (x < 0.0f) return mTable[(u32)(-x * 1023.5f)] + TAngleConstant_<T>::RADIAN_DEG090();
            return TAngleConstant_<T>::RADIAN_DEG090() - mTable[(u32)(x * 1023.5f)];
        }
        T acosDegree(T x) const { return acos_(x) * TAngleConstant_<T>::RADIAN_TO_DEGREE_FACTOR(); }
        T mTable[Len];
        T _1000;
    };

    extern TSinCosTable<14, f32> sSinCosTable;
    extern TAtanTable<1024, f32> sAtanTable;
    extern TAsinAcosTable<1024, f32> sAsinAcosTable;

    inline f32 acosDegree(f32 x) { return sAsinAcosTable.acosDegree(x); }
}  // namespace JMath

inline f32 JMACosShort(s16 v) { return JMath::sSinCosTable.cosShort(v); }
inline f32 JMASinShort(s16 v) { return JMath::sSinCosTable.sinShort(v); }
inline f32 JMASCos(s16 v) { return JMACosShort(v); }
inline f32 JMASSin(s16 v) { return JMASinShort(v); }
inline f32 JMAAcosRadian(f32 v) { return JMath::sAsinAcosTable.acos_(v); }
inline f32 JMAAsinRadian(f32 v) { return JMath::sAsinAcosTable.asin_(v); }
inline f32 JMACosDegree(f32 angle) { return JMath::sSinCosTable.cosDegree(angle); }
inline f32 JMACosRadian(f32 angle) { return JMath::sSinCosTable.cosRadian(angle); }
inline f32 JMACosLap(f32 angle) { return JMath::sSinCosTable.cosLap(angle); }
inline f32 JMASinDegree(f32 angle) { return JMath::sSinCosTable.sinDegree(angle); }
inline f32 JMASinRadian(f32 angle) { return JMath::sSinCosTable.sinRadian(angle); }
inline f32 JMASinLap(f32 angle) { return JMath::sSinCosTable.sinLap(angle); }
inline f32 JMAATan2(f32 x, f32 y) { return JMath::sAtanTable.atan2_(x, y); }
