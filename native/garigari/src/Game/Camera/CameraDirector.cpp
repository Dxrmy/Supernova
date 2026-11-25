#include "Game/Camera/CameraDirector.hpp"
#include <nebula_game_api.h>
#include <nebula_module.h>

CameraDirector::CameraDirector()
    : mPos(0.0f, 1200.0f, -900.0f),
      mTarget(0.0f, 1000.0f, 0.0f),
      mYaw(0.0f),
      mTargetYaw(0.0f),
      mDistance(1100.0f) {
}

void CameraDirector::init() {
    mYaw = 0.0f;
    mTargetYaw = 0.0f;
    mDistance = 1100.0f;
    mPos.set(0.0f, 1450.0f, 1100.0f);
    mTarget.set(0.0f, 1150.0f, 0.0f);
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

    const TVec3f desiredTarget(
        marioPos.x + marioUp.x * 100.0f,
        marioPos.y + marioUp.y * 100.0f,
        marioPos.z + marioUp.z * 100.0f);
    const TVec3f desiredPos(
        marioPos.x + mDistance * __builtin_sinf(mYaw) + marioUp.x * 400.0f,
        marioPos.y + marioUp.y * 400.0f,
        marioPos.z + mDistance * __builtin_cosf(mYaw) + marioUp.z * 400.0f);

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
