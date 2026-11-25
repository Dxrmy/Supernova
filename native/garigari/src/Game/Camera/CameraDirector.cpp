#include "Game/Camera/CameraDirector.hpp"
#include <nebula_module.h>

CameraDirector::CameraDirector()
    : mPos(0.0f, 1200.0f, -900.0f),
      mTarget(0.0f, 1000.0f, 0.0f),
      mYaw(0.0f),
      mDistance(900.0f) {
}

void CameraDirector::init() {
    mYaw = 0.0f;
    mPos.set(0.0f, 1200.0f, -900.0f);
    mTarget.set(0.0f, 1000.0f, 0.0f);
    nebula_log("[SMG2-CAMERA] CameraDirector::init()");
}

void CameraDirector::update(f32 stickX, f32) {
    mYaw += stickX * 0.03f;
    mPos.x = mTarget.x + mDistance * stickX * 0.25f;
}

void CameraDirector::setStageTarget(const char* pStageName) {
    nebula_log("[SMG2-CAMERA] CameraDirector::setStageTarget('%s')", pStageName != nullptr ? pStageName : "");
}
