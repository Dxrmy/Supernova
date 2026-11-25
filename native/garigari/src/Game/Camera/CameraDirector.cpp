#include "Game/Camera/CameraDirector.hpp"
#include <nebula_game_api.h>
#include <nebula_module.h>

namespace Smg23D {
    bool raycastPlanet(f32 x, f32 y, f32 z, f32 dx, f32 dy, f32 dz, f32 maxDist,
                       f32* outX, f32* outY, f32* outZ, f32* outNx, f32* outNy, f32* outNz, bool topSurfaceOnly = false);
}  // namespace Smg23D

namespace {
    // Authentic CameraFollow constants ported from native/petari/src/Game/Camera/CameraFollow.cpp
    constexpr f32 kPi = 3.14159265f;
    constexpr f32 kDistMin = 300.0f;
    constexpr f32 kDistMax = 1200.0f;
    constexpr f32 kAngleXMin = 10.0f * kPi / 180.0f;
    constexpr f32 kAngleXRange = 20.0f * kPi / 180.0f;
    constexpr f32 kRoundInterval = kPi / 4.0f;
    constexpr f32 kRoundAddition = 0.08f;
    constexpr f32 kRoundResetAddition = 0.2f;

    f32 convergeRadian(f32 cur, f32 target, f32 step) {
        f32 diff = target - cur;
        while (diff > kPi) diff -= 2.0f * kPi;
        while (diff < -kPi) diff += 2.0f * kPi;
        if (diff > step) return cur + step;
        if (diff < -step) return cur - step;
        return target;
    }

    void crossToPolar(const TVec3f& watchPos, const TVec3f& pos, f32* outLen, f32* outAngleX, f32* outAngleY) {
        const f32 dx = pos.x - watchPos.x;
        const f32 dy = pos.y - watchPos.y;
        const f32 dz = pos.z - watchPos.z;
        const f32 len = __builtin_sqrtf(dx * dx + dy * dy + dz * dz);
        *outLen = len;
        if (len > 1.0e-4f) {
            const f32 horiz = __builtin_sqrtf(dx * dx + dz * dz);
            *outAngleX = __builtin_atan2f(dy, horiz);
            *outAngleY = __builtin_atan2f(dx, dz);
        } else {
            *outAngleX = kAngleXMin + kAngleXRange * 0.5f;
            *outAngleY = 0.0f;
        }
    }

    void polarToCross(const TVec3f& watchPos, TVec3f* outPos, f32 len, f32 angleX, f32 angleY) {
        const f32 cosX = __builtin_cosf(angleX);
        outPos->set(
            watchPos.x + len * cosX * __builtin_sinf(angleY),
            watchPos.y + len * __builtin_sinf(angleX),
            watchPos.z + len * cosX * __builtin_cosf(angleY));
    }
}  // namespace

CameraDirector::CameraDirector()
    : mPos(0.0f, 1600.0f, 1100.0f),
      mTarget(0.0f, 1200.0f, 0.0f),
      mUp(0.0f, 1.0f, 0.0f),
      mDistMin(kDistMin),
      mDistMax(kDistMax),
      mAngleXMin(kAngleXMin),
      mAngleXRange(kAngleXRange),
      mAngleY(0.0f),
      mAngleYTarget(0.0f),
      mAngleYSpeed(0.0f),
      mIsRounding(false) {
}

void CameraDirector::init() {
    mAngleY = 0.0f;
    mAngleYTarget = 0.0f;
    mAngleYSpeed = 0.0f;
    mIsRounding = false;
    mPos.set(0.0f, 1600.0f, 1100.0f);
    mTarget.set(0.0f, 1200.0f, 0.0f);
    mUp.set(0.0f, 1.0f, 0.0f);
    nebula_log("[SMG2-CAMERA] CameraDirector::init() (Petari CameraFollow)");
}

void CameraDirector::constrainDistance(TVec3f* pPos, const TVec3f& rWatchPos) {
    TVec3f diff(pPos->x - rWatchPos.x, pPos->y - rWatchPos.y, pPos->z - rWatchPos.z);
    const f32 len = __builtin_sqrtf(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
    if (len < mDistMin) {
        if (len > 1.0e-4f) {
            const f32 s = mDistMin / len;
            diff.set(diff.x * s, diff.y * s, diff.z * s);
        } else {
            diff.set(0.0f, 0.0f, mDistMin);
        }
    } else if (len >= mDistMax) {
        const f32 targetLen = mDistMax + (len - mDistMax) * 0.5f;
        const f32 s = targetLen / len;
        diff.set(diff.x * s, diff.y * s, diff.z * s);
    }
    pPos->set(diff.x + rWatchPos.x, diff.y + rWatchPos.y, diff.z + rWatchPos.z);
}

void CameraDirector::constrainAngleX(f32* pAngleX) {
    if (*pAngleX > mAngleXMin + mAngleXRange) {
        *pAngleX = *pAngleX + (mAngleXMin + mAngleXRange - *pAngleX) * 0.5f;
    }
    if (*pAngleX < mAngleXMin) {
        *pAngleX = *pAngleX + (mAngleXMin - *pAngleX) * 0.5f;
    }
}

void CameraDirector::startRound(f32 targetAngleY, f32 speed) {
    mAngleYTarget = targetAngleY;
    mAngleYSpeed = speed;
    mIsRounding = true;
}

void CameraDirector::update(f32 stickX, f32) {
    mAngleY += stickX * 0.03f;
}

void CameraDirector::update(const TVec3f& marioPos, const TVec3f& marioUp, const TVec3f& marioFront, u32 buttons, u32 pressed) {
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

    const TVec3f watchPos(
        marioPos.x + mUp.x * 120.0f,
        marioPos.y + mUp.y * 120.0f,
        marioPos.z + mUp.z * 120.0f);
    mTarget.x += (watchPos.x - mTarget.x) * 0.25f;
    mTarget.y += (watchPos.y - mTarget.y) * 0.25f;
    mTarget.z += (watchPos.z - mTarget.z) * 0.25f;

    TVec3f pos = mPos;
    constrainDistance(&pos, mTarget);

    f32 length = 0.0f, angleX = 0.0f, angleY = 0.0f;
    crossToPolar(mTarget, pos, &length, &angleX, &angleY);

    if ((pressed & NEBULA_BUTTON_C) != 0u) {
        const f32 frontAngleY = __builtin_atan2f(-marioFront.x, -marioFront.z);
        startRound(frontAngleY, kRoundResetAddition);
    } else if ((pressed & NEBULA_BUTTON_LEFT) != 0u) {
        startRound(angleY - kRoundInterval, kRoundAddition);
    } else if ((pressed & NEBULA_BUTTON_RIGHT) != 0u) {
        startRound(angleY + kRoundInterval, kRoundAddition);
    } else if ((buttons & NEBULA_BUTTON_LEFT) != 0u) {
        angleY -= 0.04f;
        mIsRounding = false;
    } else if ((buttons & NEBULA_BUTTON_RIGHT) != 0u) {
        angleY += 0.04f;
        mIsRounding = false;
    }

    if (mIsRounding) {
        angleY = convergeRadian(angleY, mAngleYTarget, mAngleYSpeed);
        f32 rem = mAngleYTarget - angleY;
        while (rem > kPi) rem -= 2.0f * kPi;
        while (rem < -kPi) rem += 2.0f * kPi;
        if (rem < 0.001f && rem > -0.001f) {
            mIsRounding = false;
        }
    }

    constrainAngleX(&angleX);
    mAngleY = angleY;
    polarToCross(mTarget, &pos, length, angleX, angleY);

    // Terrain collision avoidance (prevent clipping inside hollow ship/planet geometry)
    const f32 dx = pos.x - mTarget.x;
    const f32 dy = pos.y - mTarget.y;
    const f32 dz = pos.z - mTarget.z;
    const f32 rayLen = __builtin_sqrtf(dx * dx + dy * dy + dz * dz);
    if (rayLen > 1.0f) {
        const f32 rdx = dx / rayLen;
        const f32 rdy = dy / rayLen;
        const f32 rdz = dz / rayLen;
        f32 hx = 0.0f, hy = 0.0f, hz = 0.0f;
        if (Smg23D::raycastPlanet(mTarget.x, mTarget.y, mTarget.z,
                                  rdx, rdy, rdz, rayLen, &hx, &hy, &hz, nullptr, nullptr, nullptr, false)) {
            const f32 hitDist = __builtin_sqrtf((hx - mTarget.x) * (hx - mTarget.x) +
                                                (hy - mTarget.y) * (hy - mTarget.y) +
                                                (hz - mTarget.z) * (hz - mTarget.z));
            if (hitDist < rayLen) {
                const f32 safeDist = (hitDist > 200.0f) ? (hitDist - 80.0f) : 120.0f;
                pos.set(mTarget.x + rdx * safeDist,
                        mTarget.y + rdy * safeDist,
                        mTarget.z + rdz * safeDist);
            }
        }
    }

    mPos = pos;
}

void CameraDirector::setStageTarget(const char* pStageName) {
    nebula_log("[SMG2-CAMERA] CameraDirector::setStageTarget('%s')", pStageName != nullptr ? pStageName : "");
}
