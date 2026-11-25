#include "Game/Util/MtxUtil.hpp"
#include <revolution/mtx/mtx.h>
#include <JSystem/JGeometry/TVec.hpp>
#include <JSystem/JMath.hpp>

Mtx tmpmtx_sc = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
Mtx tmpmtx_rx = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
Mtx tmpmtx_ry = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
Mtx tmpmtx_rz = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};

namespace MR {
    void setMtxTrans(MtxPtr mtx, f32 x, f32 y, f32 z) {
        mtx[0][3] = x;
        mtx[1][3] = y;
        mtx[2][3] = z;
    }

    void addTransMtx(MtxPtr mtx, const TVec3f& rTrans) {
        mtx[0][3] += rTrans.x;
        mtx[1][3] += rTrans.y;
        mtx[2][3] += rTrans.z;
    }

    void addTransMtxLocal(MtxPtr mtx, const TVec3f& rVec) {
        MR::addTransMtxLocalX(mtx, rVec.x);
        MR::addTransMtxLocalY(mtx, rVec.y);
        MR::addTransMtxLocalZ(mtx, rVec.z);
    }

    void addTransMtxLocalX(MtxPtr mtx, f32 x) {
        mtx[0][3] += mtx[0][0] * x;
        mtx[1][3] += mtx[1][0] * x;
        mtx[2][3] += mtx[2][0] * x;
    }

    void addTransMtxLocalY(MtxPtr mtx, f32 y) {
        mtx[0][3] += mtx[0][1] * y;
        mtx[1][3] += mtx[1][1] * y;
        mtx[2][3] += mtx[2][1] * y;
    }

    void addTransMtxLocalZ(MtxPtr mtx, f32 z) {
        mtx[0][3] += mtx[0][2] * z;
        mtx[1][3] += mtx[1][2] * z;
        mtx[2][3] += mtx[2][2] * z;
    }

    void extractMtxXDir(MtxPtr mtx, TVec3f* pDir) {
        pDir->x = mtx[0][0];
        pDir->y = mtx[1][0];
        pDir->z = mtx[2][0];
    }

    void extractMtxYDir(MtxPtr mtx, TVec3f* pDir) {
        pDir->x = mtx[0][1];
        pDir->y = mtx[1][1];
        pDir->z = mtx[2][1];
    }

    void extractMtxZDir(MtxPtr mtx, TVec3f* pDir) {
        pDir->x = mtx[0][2];
        pDir->y = mtx[1][2];
        pDir->z = mtx[2][2];
    }

    void extractMtxXYZDir(MtxPtr mtx, TVec3f* pX, TVec3f* pY, TVec3f* pZ) {
        MR::extractMtxXDir(mtx, pX);
        MR::extractMtxYDir(mtx, pY);
        MR::extractMtxZDir(mtx, pZ);
    }

    void extractMtxTrans(MtxPtr mtx, TVec3f* pTrans) {
        pTrans->x = mtx[0][3];
        pTrans->y = mtx[1][3];
        pTrans->z = mtx[2][3];
    }

    /*
    f32 sqrt(f32 val) {
        if (val < 0.0f) {
            return val;
        }

        #ifdef __MWERKS__
        f64 sq = __frsqrte(val);
        #else
        f32 sq = 0.0f;
        #endif

        return ((-(((sq * val) * sq) - 3.0f) * (sq * val)) * 0.5f);
    }*/

    bool isSameMtx(MtxPtr a, MtxPtr b) {
        f32* a2 = (f32*)a;
        f32* b2 = (f32*)b;
        for (u32 i = 0; i < 12; a2++, b2++, i++) {
            if (*a2 != *b2) {
                return false;
            }
        }
        return true;
    }

    bool isSameMtxRot(MtxPtr a, MtxPtr b) {
        f32* a2 = (f32*)a;
        f32* b2 = (f32*)b;
        for (u32 i = 0; i < 12; a2++, b2++, i++) {
            if ((i % 4) == 3) {
                continue;
            }
            if (*a2 != *b2) {
                return false;
            }
        }
        return true;
    }

    MtxPtr tmpMtxScale(f32 x, f32 y, f32 z) {
        tmpmtx_sc[0][0] = x;
        tmpmtx_sc[1][1] = y;
        tmpmtx_sc[2][2] = z;
        return tmpmtx_sc;
    }

    MtxPtr tmpMtxRotXRad(f32 angle) {
        f32 cos = JMACosRadian(angle);
        f32 sin = JMASinRadian(angle);
        tmpmtx_rx[1][1] = cos;
        tmpmtx_rx[2][1] = sin;
        tmpmtx_rx[1][2] = -sin;
        tmpmtx_rx[2][2] = cos;
        return tmpmtx_rx;
    }

    MtxPtr tmpMtxRotYRad(f32 angle) {
        f32 cos = JMACosRadian(angle);
        f32 sin = JMASinRadian(angle);
        tmpmtx_ry[0][0] = cos;
        tmpmtx_ry[0][2] = sin;
        tmpmtx_ry[2][0] = -sin;
        tmpmtx_ry[2][2] = cos;
        return tmpmtx_ry;
    }

    MtxPtr tmpMtxRotZRad(f32 angle) {
        f32 cos = JMACosRadian(angle);
        f32 sin = JMASinRadian(angle);
        tmpmtx_rz[0][0] = cos;
        tmpmtx_rz[1][0] = sin;
        tmpmtx_rz[0][1] = -sin;
        tmpmtx_rz[1][1] = cos;
        return tmpmtx_rz;
    }

    MtxPtr tmpMtxRotXDeg(f32 angle) {
        f32 cos = JMACosDegree(angle);
        f32 sin = JMASinDegree(angle);
        tmpmtx_rx[1][1] = cos;
        tmpmtx_rx[2][1] = sin;
        tmpmtx_rx[1][2] = -sin;
        tmpmtx_rx[2][2] = cos;
        return tmpmtx_rx;
    }

    MtxPtr tmpMtxRotYDeg(f32 angle) {
        f32 cos = JMACosDegree(angle);
        f32 sin = JMASinDegree(angle);
        tmpmtx_ry[0][0] = cos;
        tmpmtx_ry[0][2] = sin;
        tmpmtx_ry[2][0] = -sin;
        tmpmtx_ry[2][2] = cos;
        return tmpmtx_ry;
    }

    MtxPtr tmpMtxRotZDeg(f32 angle) {
        f32 cos = JMACosDegree(angle);
        f32 sin = JMASinDegree(angle);
        tmpmtx_rz[0][0] = cos;
        tmpmtx_rz[1][0] = sin;
        tmpmtx_rz[0][1] = -sin;
        tmpmtx_rz[1][1] = cos;
        return tmpmtx_rz;
    }

    void orderRotateMtx(s16 idx, const TVec3f& rSrc, MtxPtr mtx) {
        MtxPtr xRot = MR::tmpMtxRotXRad(rSrc.x);
        MtxPtr yRot = MR::tmpMtxRotYRad(rSrc.y);
        MtxPtr zRot = MR::tmpMtxRotZRad(rSrc.z);

        MtxPtr v9 = yRot;
        MtxPtr v10;
        MtxPtr v12;

        switch (idx) {
        case 0:
            v12 = v9;
            v10 = xRot;
            v9 = zRot;
            break;
        case 1:
            v10 = xRot;
            v12 = zRot;
            break;
        case 2:
            v10 = v9;
            v12 = xRot;
            v9 = zRot;
            break;
        case 3:
            v10 = v9;
            v12 = zRot;
            v9 = xRot;
            break;
        case 4:
            v10 = zRot;
            v12 = xRot;
            v9 = zRot;
            break;
        case 5:
            v12 = v9;
            v10 = zRot;
            v9 = xRot;
            break;
        }

        PSMTXConcat(v12, v10, mtx);
        PSMTXConcat(v9, mtx, mtx);
    }

    void inverseConcatMtx(MtxPtr a1, MtxPtr a2, TMtx34f* pMtx) {
        Mtx mtx;
        PSMTXInverse(a1, mtx);
        PSMTXConcat(mtx, a2, *pMtx);
    }

    void preScaleMtx(MtxPtr mtx, const TVec3f& rScale) {
        if (mtx == nullptr) return;
        PSMTXScale(tmpmtx_sc, rScale.x, rScale.y, rScale.z);
        PSMTXConcat(mtx, tmpmtx_sc, mtx);
    }

    void makeMtxRotate(MtxPtr mtx, f32 rxDeg, f32 ryDeg, f32 rzDeg) {
        if (mtx == nullptr) return;
        constexpr f32 kDegToRad = 3.14159265358979323846f / 180.0f;
        PSMTXRotRad(tmpmtx_rx, 'x', rxDeg * kDegToRad);
        PSMTXRotRad(tmpmtx_ry, 'y', ryDeg * kDegToRad);
        PSMTXRotRad(tmpmtx_rz, 'z', rzDeg * kDegToRad);
        PSMTXConcat(tmpmtx_rz, tmpmtx_ry, mtx);
        PSMTXConcat(mtx, tmpmtx_rx, mtx);
    }

    void makeMtxRotate(MtxPtr mtx, const TVec3f& rRotate) {
        makeMtxRotate(mtx, rRotate.x, rRotate.y, rRotate.z);
    }

    void makeMtxTR(MtxPtr mtx, f32 tx, f32 ty, f32 tz, f32 rx, f32 ry, f32 rz) {
        if (mtx == nullptr) return;
        makeMtxRotate(mtx, rx, ry, rz);
        setMtxTrans(mtx, tx, ty, tz);
    }

    void makeMtxTR(MtxPtr mtx, const TVec3f& rTrans, const TVec3f& rRotate) {
        makeMtxTR(mtx, rTrans.x, rTrans.y, rTrans.z, rRotate.x, rRotate.y, rRotate.z);
    }

    void makeMtxTRS(MtxPtr mtx, const TVec3f& rTrans, const TVec3f& rRotate, const TVec3f& rScale) {
        if (mtx == nullptr) return;
        makeMtxTR(mtx, rTrans, rRotate);
        preScaleMtx(mtx, rScale);
    }
};  // namespace MR
