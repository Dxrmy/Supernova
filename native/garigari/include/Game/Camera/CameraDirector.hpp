#pragma once

#include <JSystem/JGeometry/TVec.hpp>
#include <revolution/types.h>

class CameraDirector {
public:
    CameraDirector();

    void init();
    void update(f32 stickX, f32 stickY);
    void update(const TVec3f& marioPos, const TVec3f& marioUp, f32 marioYaw, u32 buttons, u32 pressed);
    void setStageTarget(const char* pStageName);

    const TVec3f& getPosition() const { return mPos; }
    const TVec3f& getTarget() const { return mTarget; }
    const TVec3f& getUp() const { return mUp; }
    f32 getYaw() const { return mYaw; }

private:
    TVec3f mPos;
    TVec3f mTarget;
    TVec3f mUp;
    f32 mYaw;
    f32 mTargetYaw;
    f32 mDistance;
};
