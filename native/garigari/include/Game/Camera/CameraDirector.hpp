#pragma once

#include <JSystem/JGeometry/TVec.hpp>
#include <revolution/types.h>

class CameraDirector {
public:
    CameraDirector();

    void init();
    void update(f32 stickX, f32 stickY);
    void update(const TVec3f& marioPos, const TVec3f& marioUp, const TVec3f& marioFront, u32 buttons, u32 pressed);
    void setStageTarget(const char* pStageName);

    const TVec3f& getPosition() const { return mPos; }
    const TVec3f& getTarget() const { return mTarget; }
    const TVec3f& getUp() const { return mUp; }
    f32 getYaw() const { return mAngleY; }

private:
    void constrainDistance(TVec3f* pPos, const TVec3f& rWatchPos);
    void constrainAngleX(f32* pAngleX);
    void startRound(f32 targetAngleY, f32 speed);

    TVec3f mPos;
    TVec3f mTarget;
    TVec3f mUp;
    f32 mDistMin;
    f32 mDistMax;
    f32 mAngleXMin;
    f32 mAngleXRange;
    f32 mAngleY;
    f32 mAngleYTarget;
    f32 mAngleYSpeed;
    bool mIsRounding;
};
