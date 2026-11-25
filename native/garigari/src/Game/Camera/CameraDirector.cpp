#include "Game/Camera/CameraDirector.hpp"
#include <nebula_game_api.h>
#include <nebula_module.h>

namespace Smg23D {
    bool raycastPlanet(f32 x, f32 y, f32 z, f32 dx, f32 dy, f32 dz, f32 maxDist,
                       f32* outX, f32* outY, f32* outZ, f32* outNx, f32* outNy, f32* outNz, bool topSurfaceOnly = false);
}  // namespace Smg23D

CameraDirector::CameraDirector()
    : mPos(0.0f, 1600.0f, 1100.0f),
      mTarget(0.0f, 1200.0f, 0.0f),
      mUp(0.0f, 1.0f, 0.0f),
      mYaw(0.0f),
      mTargetYaw(0.0f),
      mDistance(1100.0f) {
}

void CameraDirector::init() {
    mYaw = 0.0f;
    mTargetYaw = 0.0f;
    mDistance = 1100.0f;
    mPos.set(0.0f, 1600.0f, 1100.0f);
    mTarget.set(0.0f, 1200.0f, 0.0f);
    mUp.set(0.0f, 1.0f, 0.0f);
    nebula_log("[SMG2-CAMERA] CameraDirector::init()");
}

void CameraDirector::update(f32 stickX, f32) {
    mTargetYaw += stickX * 0.03f;
    mYaw += (mTargetYaw - mYaw) * 0.15f;
    mPos.x = mTarget.x + mDistance * __builtin_sinf(mYaw);
    mPos.z = mTarget.z + mDistance * __builtin_cosf(mYaw);
}

void CameraDirector::update(const TVec3f& marioPos, const TVec3f& marioUp, f32 marioYaw, u32 buttons, u32 pressed) {
    if ((buttons & NEBULA_BUTTON_LEFT) != 0u) {
        mTargetYaw -= 0.04f;
    }
    if ((buttons & NEBULA_BUTTON_RIGHT) != 0u) {
        mTargetYaw += 0.04f;
    }
    if ((pressed & NEBULA_BUTTON_C) != 0u) {
        mTargetYaw = marioYaw + 3.14159265f;
    }
    mYaw += (mTargetYaw - mYaw) * 0.15f;

    // Smooth camera up-vector toward Mario's local up-vector
    mUp.x += (marioUp.x - mUp.x) * 0.10f;
    mUp.y += (marioUp.y - mUp.y) * 0.10f;
    mUp.z += (marioUp.z - mUp.z) * 0.10f;
    const f32 uLen = __builtin_sqrtf(mUp.x * mUp.x + mUp.y * mUp.y + mUp.z * mUp.z);
    if (uLen > 1.0e-5f) {
        mUp.x /= uLen; mUp.y /= uLen; mUp.z /= uLen;
    } else {
        mUp.set(0.0f, 1.0f, 0.0f);
    }

    const TVec3f desiredTarget(
        marioPos.x + mUp.x * 100.0f,
        marioPos.y + mUp.y * 100.0f,
        marioPos.z + mUp.z * 100.0f);
    TVec3f desiredPos(
        marioPos.x + mDistance * __builtin_sinf(mYaw) + mUp.x * 400.0f,
        marioPos.y + mUp.y * 400.0f,
        marioPos.z + mDistance * __builtin_cosf(mYaw) + mUp.z * 400.0f);

    // Raycast from target to desired camera position to prevent hull clipping
    const f32 dx = desiredPos.x - desiredTarget.x;
    const f32 dy = desiredPos.y - desiredTarget.y;
    const f32 dz = desiredPos.z - desiredTarget.z;
    const f32 rayLen = __builtin_sqrtf(dx * dx + dy * dy + dz * dz);
    if (rayLen > 1.0f) {
        const f32 rdx = dx / rayLen;
        const f32 rdy = dy / rayLen;
        const f32 rdz = dz / rayLen;
        f32 hx = 0.0f, hy = 0.0f, hz = 0.0f;
        if (Smg23D::raycastPlanet(desiredTarget.x, desiredTarget.y, desiredTarget.z,
                                  rdx, rdy, rdz, rayLen, &hx, &hy, &hz, nullptr, nullptr, nullptr, false)) {
            const f32 hitDist = __builtin_sqrtf((hx - desiredTarget.x) * (hx - desiredTarget.x) +
                                                (hy - desiredTarget.y) * (hy - desiredTarget.y) +
                                                (hz - desiredTarget.z) * (hz - desiredTarget.z));
            if (hitDist < rayLen) {
                const f32 safeDist = (hitDist > 200.0f) ? (hitDist - 80.0f) : 120.0f;
                desiredPos.set(desiredTarget.x + rdx * safeDist,
                               desiredTarget.y + rdy * safeDist,
                               desiredTarget.z + rdz * safeDist);
            }
        }
    }

    constexpr f32 kLerp = 0.15f;
    mTarget.x += (desiredTarget.x - mTarget.x) * kLerp;
    mTarget.y += (desiredTarget.y - mTarget.y) * kLerp;
    mTarget.z += (desiredTarget.z - mTarget.z) * kLerp;
    mPos.x += (desiredPos.x - mPos.x) * kLerp;
    mPos.y += (desiredPos.y - mPos.y) * kLerp;
    mPos.z += (desiredPos.z - mPos.z) * kLerp;
}

void CameraDirector::setStageTarget(const char* pStageName) {
    nebula_log("[SMG2-CAMERA] CameraDirector::setStageTarget('%s')", pStageName != nullptr ? pStageName : "");
}
