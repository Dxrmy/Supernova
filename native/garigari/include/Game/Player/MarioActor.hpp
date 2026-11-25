#pragma once

#include <JSystem/JGeometry/TVec.hpp>
#include <revolution/types.h>

class PlanetGravityManager;

class MarioActor {
public:
    MarioActor();

    void init(const char* pStageName);
    void update(f32 stickX, f32 stickY, u32 buttons, PlanetGravityManager* pGravityMgr);

    const TVec3f& getPosition() const { return mPos; }
    const TVec3f& getVelocity() const { return mVel; }
    const TVec3f& getGravityVec() const { return mGravity; }
    const TVec3f& getUpVec() const { return mUp; }
    f32 getYaw() const { return mYaw; }
    int getAnimState() const { return mAnimState; }
    bool isGrounded() const { return mGrounded; }

private:
    TVec3f mPos;
    TVec3f mSafeSpawnPos;
    TVec3f mVel;
    TVec3f mGravity;
    TVec3f mUp;
    f32 mYaw;
    int mAnimState;
    bool mGrounded;
    bool mUsePlanarGravity;
};
