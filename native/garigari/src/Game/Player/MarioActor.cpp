#include "Game/Player/MarioActor.hpp"
#include "Game/Gravity/GravityInfo.hpp"
#include "Game/Gravity/PlanetGravity.hpp"
#include "Game/Gravity/PlanetGravityManager.hpp"
#include <nebula_game_api.h>
#include <nebula_module.h>

MarioActor::MarioActor()
    : mPos(0.0f, 1000.0f, 0.0f),
      mVel(0.0f, 0.0f, 0.0f),
      mGravity(0.0f, -1.0f, 0.0f),
      mGrounded(true) {
}

void MarioActor::init(const char* pStageName) {
    mPos.set(0.0f, 1000.0f, 0.0f);
    mVel.set(0.0f, 0.0f, 0.0f);
    mGravity.set(0.0f, -1.0f, 0.0f);
    mGrounded = true;
    nebula_log("[SMG2-PLAYER] MarioActor::init(stage='%s') spawn=(0, 1000, 0)", pStageName != nullptr ? pStageName : "");
}

void MarioActor::update(f32 stickX, f32 stickY, u32 buttons, PlanetGravityManager* pGravityMgr) {
    if (pGravityMgr != nullptr) {
        GravityInfo info{};
        TVec3f grav(0.0f, -1.0f, 0.0f);
        if (pGravityMgr->calcTotalGravityVector(&grav, &info, mPos, GRAVITY_TYPE_NORMAL, 0, nullptr)) {
            mGravity = grav;
        }
    }

    mVel.x = stickX * 12.0f;
    mVel.z = stickY * 12.0f;
    if (mGrounded && (buttons & NEBULA_BUTTON_A) != 0u) {
        mVel.y = 22.0f;
        mGrounded = false;
    } else if (!mGrounded) {
        mVel.y += mGravity.y * 1.2f;
        if (mPos.y + mVel.y <= 1000.0f) {
            mPos.y = 1000.0f;
            mVel.y = 0.0f;
            mGrounded = true;
        }
    }

    mPos.x += mVel.x;
    mPos.y += mVel.y;
    mPos.z += mVel.z;
}
