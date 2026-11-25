#pragma once

#include "Game/System/NerveExecutor.hpp"
#include <JSystem/JGeometry/TVec.hpp>
#include <revolution/types.h>

class PlanetGravityManager;

class MarioActor : public NerveExecutor {
public:
    MarioActor();

    void init(const char* pStageName);
    void update(f32 stickX, f32 stickY, u32 buttons, PlanetGravityManager* pGravityMgr,
                const TVec3f& camPos, const TVec3f& camTarget, const TVec3f& camUp);

    void exeWait();
    void exeWalk();
    void exeJump();
    void exeFall();

    const TVec3f& getPosition() const { return mPos; }
    const TVec3f& getVelocity() const { return mVel; }
    const TVec3f& getGravityVec() const { return mGravity; }
    const TVec3f& getUpVec() const { return mUp; }
    const TVec3f& getFrontVec() const { return mFrontVec; }
    f32 getYaw() const { return mYaw; }
    int getAnimState() const { return mAnimState; }
    bool isGrounded() const { return mGrounded; }

private:
    void calcMoveDir(f32 stickX, f32 stickY, const TVec3f& camPos, const TVec3f& camTarget, const TVec3f& camUp, TVec3f* pMoveDir);
    void resolveTerrainCollision();

    TVec3f mPos;
    TVec3f mSafeSpawnPos;
    TVec3f mVel;
    TVec3f mGravity;
    TVec3f mUp;
    TVec3f mMoveUpVec;
    TVec3f mFrontVec;
    f32 mYaw;
    int mAnimState;
    bool mGrounded;
    bool mUsePlanarGravity;
    f32 mStickX;
    f32 mStickY;
    u32 mButtons;
    u32 mPrevButtons;
};
