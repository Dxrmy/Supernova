#ifndef MTX_H
#define MTX_H

#include "revolution/types.h"
#include <nebula_math.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef f32 (*Mtx23P)[3];

#define MTXDegToRad(a) ((a) * 0.01745329252f)
#define MTXRadToDeg(a) ((a) * 57.29577951f)
#define MTXRowCol(m, r, c) ((m)[(r)][(c)])

#define C_MTXIdentity NebulaMtxIdentity
#define C_MTXCopy NebulaMtxCopy
#define C_MTXOrtho NebulaMtxOrtho
#define C_MTXPerspective NebulaMtxPerspective
#define C_MTXLightPerspective NebulaMtxLightPerspective
#define C_MTXLightOrtho NebulaMtxLightOrtho

#define PSMTXIdentity NebulaMtxIdentity
#define PSMTXCopy NebulaMtxCopy
#define PSMTXConcat NebulaMtxConcat
#define PSMTXMultVecSR NebulaMtxMultVecSR
#define PSMTXMultVecArraySR NebulaMtxMultVecArraySR
#define PSMTXInverse NebulaMtxInverse
#define PSMTXMultVec NebulaMtxMultVec
#define PSMTXTransApply NebulaMtxTransApply
#define PSMTXScaleApply NebulaMtxApplyScale
#define PSMTXScale NebulaMtxScale
#define PSMTXRotRad NebulaMtxRotRad
#define PSMTXRotAxisRad NebulaMtxRotAxisRad
#define PSMTXRotTrig NebulaMtxRotTrig
#define PSMTXTrans NebulaMtxTrans
#define PSMTX44Identity NebulaMtx44Identity
#define PSMTX44Copy NebulaMtx44Copy

#define PSQUATMultiply NebulaQuatMultiply
#define PSMTXQuat NebulaMtxQuat
#define PSQUATDotProduct NebulaQuatDotProduct
#define C_QUATMtx NebulaQuatFromMtx
#define C_QUATSlerp NebulaQuatSlerp

#define C_VECMag NebulaVecMag
#define PSVECAdd NebulaVecAdd
#define PSVECSubtract NebulaVecSubtract
#define PSVECScale NebulaVecScale
#define PSVECNormalize NebulaVecNormalize
#define PSVECMag NebulaVecMag
#define PSVECDotProduct NebulaVecDotProduct
#define PSVECCrossProduct NebulaVecCrossProduct
#define PSVECDistance NebulaVecDistance

#define VECNormalize PSVECNormalize
#define VECMag PSVECMag

#ifdef __cplusplus
}
#endif

#endif  // MTX_H
