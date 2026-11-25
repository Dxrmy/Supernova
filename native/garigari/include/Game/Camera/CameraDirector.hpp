#pragma once

#include <JSystem/JGeometry/TVec.hpp>
#include <revolution/types.h>

class CameraDirector {
public:
    CameraDirector();

    void init();
    void update(f32 stickX, f32 stickY);
    void setStageTarget(const char* pStageName);

    const TVec3f& getPosition() const { return mPos; }
    const TVec3f& getTarget() const { return mTarget; }
    f32 getYaw() const { return mYaw; }

private:
    TVec3f mPos;
    TVec3f mTarget;
    f32 mYaw;
    f32 mDistance;
};
