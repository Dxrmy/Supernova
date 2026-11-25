#include "Game/Player/MarioActor.hpp"
#include "Game/Gravity/GravityInfo.hpp"
#include "Game/Gravity/PlanetGravity.hpp"
#include "Game/Gravity/PlanetGravityManager.hpp"
#include "Game/LiveActor/Nerve.hpp"
#include <nebula_game_api.h>
#include <nebula_module.h>
#include <cstring>

namespace Smg23D {
    bool resolveSphereCollision(f32* ioX, f32* ioY, f32* ioZ, f32 radius, f32* outNormalX, f32* outNormalY, f32* outNormalZ, bool topSurfaceOnly = false);
    bool raycastPlanet(f32 x, f32 y, f32 z, f32 dx, f32 dy, f32 dz, f32 maxDist,
                       f32* outX, f32* outY, f32* outZ, f32* outNx, f32* outNy, f32* outNz, bool topSurfaceOnly = false);
}  // namespace Smg23D

namespace NrvMarioActor {
    NEW_NERVE(MarioActorNrvWait, MarioActor, Wait);
    NEW_NERVE(MarioActorNrvWalk, MarioActor, Walk);
    NEW_NERVE(MarioActorNrvJump, MarioActor, Jump);
    NEW_NERVE(MarioActorNrvFall, MarioActor, Fall);
}  // namespace NrvMarioActor

namespace {
    // Authentic SMG1/SMG2 MarioConstTable defaults (native/petari/src/Game/Player/MarioConst.cpp)
    constexpr f32 kJumpHeight = 22.0f;
    constexpr f32 kGravityJumping = 1.8f;
    constexpr f32 kMaxDropSpeed = 35.0f;
    constexpr f32 kWalkSpeedMax = 14.0f;
    constexpr f32 kStickMargin = 0.1f;

    void normalizeVec3(TVec3f& v, const TVec3f& fallback) {
        const f32 len2 = v.x * v.x + v.y * v.y + v.z * v.z;
        if (len2 > 1.0e-6f) {
            const f32 inv = 1.0f / __builtin_sqrtf(len2);
            v.x *= inv;
            v.y *= inv;
            v.z *= inv;
        } else {
            v = fallback;
        }
    }

    TVec3f crossVec3(const TVec3f& a, const TVec3f& b) {
        return TVec3f(a.y * b.z - a.z * b.y,
                      a.z * b.x - a.x * b.z,
                      a.x * b.y - a.y * b.x);
    }
}  // namespace

using namespace NrvMarioActor;

MarioActor::MarioActor()
    : NerveExecutor("MarioActor"),
      mPos(0.0f, 1600.0f, 0.0f),
      mSafeSpawnPos(0.0f, 1600.0f, 0.0f),
      mVel(0.0f, 0.0f, 0.0f),
      mGravity(0.0f, -1.0f, 0.0f),
      mUp(0.0f, 1.0f, 0.0f),
      mMoveUpVec(0.0f, 1.0f, 0.0f),
      mFrontVec(0.0f, 0.0f, 1.0f),
      mYaw(0.0f),
      mAnimState(0),
      mGrounded(false),
      mUsePlanarGravity(true),
      mStickX(0.0f),
      mStickY(0.0f),
      mButtons(0u),
      mPrevButtons(0u) {
}

void MarioActor::init(const char* pStageName) {
    mPos.set(0.0f, 1600.0f, 0.0f);
    mVel.set(0.0f, 0.0f, 0.0f);
    mGravity.set(0.0f, -1.0f, 0.0f);
    mUp.set(0.0f, 1.0f, 0.0f);
    mMoveUpVec.set(0.0f, 1.0f, 0.0f);
    mFrontVec.set(0.0f, 0.0f, 1.0f);
    mYaw = 0.0f;
    mAnimState = 0;
    mGrounded = false;
    mPrevButtons = 0u;
    mUsePlanarGravity = true;
    if (pStageName != nullptr && std::strstr(pStageName, "Disc") == nullptr &&
        std::strstr(pStageName, "IslandFleet") == nullptr &&
        std::strstr(pStageName, "MarioFaceShip") == nullptr) {
        mUsePlanarGravity = false;
    }

    f32 hx = 0.0f, hy = 0.0f, hz = 0.0f, nx = 0.0f, ny = 1.0f, nz = 0.0f;
    if (Smg23D::raycastPlanet(mPos.x, mPos.y + 400.0f, mPos.z, 0.0f, -1.0f, 0.0f, 4000.0f,
                              &hx, &hy, &hz, &nx, &ny, &nz, mUsePlanarGravity)) {
        mPos.set(hx, hy, hz);
        mUp.set(nx, ny, nz);
        mMoveUpVec = mUp;
        mGrounded = true;
    }
    mSafeSpawnPos = mPos;
    if (mSpine == nullptr) {
        initNerve(GET_NERVE_ANON(MarioActorNrvWait));
    } else {
        setNerve(GET_NERVE_ANON(MarioActorNrvWait));
    }
    nebula_log("[SMG2-PLAYER] MarioActor::init(stage='%s', planar=%d) spawn=(%.1f, %.1f, %.1f) grounded=%d",
               pStageName != nullptr ? pStageName : "", mUsePlanarGravity ? 1 : 0, mPos.x, mPos.y, mPos.z, mGrounded ? 1 : 0);
}

// Ported from Mario::calcMoveDir in native/petari/src/Game/Player/MarioMove.cpp
void MarioActor::calcMoveDir(f32 stickX, f32 stickY, const TVec3f& camPos, const TVec3f& camTarget, const TVec3f& camUp, TVec3f* pMoveDir) {
    TVec3f screenZDir(camPos.x - camTarget.x, camPos.y - camTarget.y, camPos.z - camTarget.z);
    normalizeVec3(screenZDir, TVec3f(0.0f, 0.0f, 1.0f));
    TVec3f screenXDir = crossVec3(camUp, screenZDir);
    normalizeVec3(screenXDir, TVec3f(1.0f, 0.0f, 0.0f));
    TVec3f screenYDir = crossVec3(screenZDir, screenXDir);
    normalizeVec3(screenYDir, TVec3f(0.0f, 1.0f, 0.0f));

    TVec3f marioUpDir(-mGravity.x, -mGravity.y, -mGravity.z);
    normalizeVec3(marioUpDir, TVec3f(0.0f, 1.0f, 0.0f));
    const f32 dotUp = mMoveUpVec.x * marioUpDir.x + mMoveUpVec.y * marioUpDir.y + mMoveUpVec.z * marioUpDir.z;
    if (dotUp <= 0.0f) {
        mMoveUpVec = marioUpDir;
    } else {
        mMoveUpVec.x += (marioUpDir.x - mMoveUpVec.x) * 0.1f;
        mMoveUpVec.y += (marioUpDir.y - mMoveUpVec.y) * 0.1f;
        mMoveUpVec.z += (marioUpDir.z - mMoveUpVec.z) * 0.1f;
        normalizeVec3(mMoveUpVec, marioUpDir);
    }

    const f32 moveDirScreenY = screenYDir.x * mMoveUpVec.x + screenYDir.y * mMoveUpVec.y + screenYDir.z * mMoveUpVec.z;
    const f32 moveDirScreenZ = screenZDir.x * mMoveUpVec.x + screenZDir.y * mMoveUpVec.y + screenZDir.z * mMoveUpVec.z;
    const f32 absY = (moveDirScreenY < 0.0f) ? -moveDirScreenY : moveDirScreenY;
    const f32 absZ = (moveDirScreenZ < 0.0f) ? -moveDirScreenZ : moveDirScreenZ;

    TVec3f groundXDir, groundYDirOrtho;
    if (absZ > absY) {
        groundYDirOrtho = (moveDirScreenZ < 0.0f) ? TVec3f(-screenYDir.x, -screenYDir.y, -screenYDir.z) : screenYDir;
        groundXDir = crossVec3(groundYDirOrtho, mMoveUpVec);
    } else {
        groundYDirOrtho = (moveDirScreenY < 0.0f) ? screenZDir : TVec3f(-screenZDir.x, -screenZDir.y, -screenZDir.z);
        const TVec3f zAxis = (moveDirScreenY < 0.0f) ? screenZDir : TVec3f(-screenZDir.x, -screenZDir.y, -screenZDir.z);
        groundXDir = crossVec3(zAxis, mMoveUpVec);
    }

    TVec3f groundYDir = crossVec3(screenXDir, mMoveUpVec);
    normalizeVec3(groundXDir, screenXDir);
    normalizeVec3(groundYDir, groundYDirOrtho);

    pMoveDir->set(
        groundXDir.x * stickX - groundYDir.x * stickY,
        groundXDir.y * stickX - groundYDir.y * stickY,
        groundXDir.z * stickX - groundYDir.z * stickY);
}

void MarioActor::resolveTerrainCollision() {
    f32 cx = mPos.x + mUp.x * 45.0f;
    f32 cy = mPos.y + mUp.y * 45.0f;
    f32 cz = mPos.z + mUp.z * 45.0f;
    f32 nx = 0.0f, ny = 1.0f, nz = 0.0f;
    if (Smg23D::resolveSphereCollision(&cx, &cy, &cz, 45.0f, &nx, &ny, &nz, mUsePlanarGravity)) {
        mPos.set(cx - nx * 45.0f, cy - ny * 45.0f, cz - nz * 45.0f);
        mUp.set(nx, ny, nz);
        const f32 vn = mVel.x * nx + mVel.y * ny + mVel.z * nz;
        if (vn < 0.0f) {
            mVel.x -= nx * vn;
            mVel.y -= ny * vn;
            mVel.z -= nz * vn;
            mGrounded = true;
        }
    } else {
        f32 hx = 0.0f, hy = 0.0f, hz = 0.0f;
        const f32 vnGravity = mVel.x * mGravity.x + mVel.y * mGravity.y + mVel.z * mGravity.z;
        if ((mGrounded || vnGravity >= 0.0f) &&
            Smg23D::raycastPlanet(mPos.x + mUp.x * 55.0f, mPos.y + mUp.y * 55.0f, mPos.z + mUp.z * 55.0f,
                                  -mUp.x, -mUp.y, -mUp.z, 160.0f, &hx, &hy, &hz, &nx, &ny, &nz, mUsePlanarGravity)) {
            const f32 altDelta = (hx - mPos.x) * mUp.x + (hy - mPos.y) * mUp.y + (hz - mPos.z) * mUp.z;
            mPos.x += mUp.x * altDelta;
            mPos.y += mUp.y * altDelta;
            mPos.z += mUp.z * altDelta;
            mUp.set(nx, ny, nz);
            const f32 vn = mVel.x * nx + mVel.y * ny + mVel.z * nz;
            if (vn < 0.0f) {
                mVel.x -= nx * vn;
                mVel.y -= ny * vn;
                mVel.z -= nz * vn;
            }
            mGrounded = true;
        } else {
            mGrounded = false;
        }
    }

    if (mUsePlanarGravity && mPos.y < -1500.0f) {
        mPos = mSafeSpawnPos;
        mVel.set(0.0f, 0.0f, 0.0f);
        mUp.set(0.0f, 1.0f, 0.0f);
        mGrounded = true;
        setNerve(GET_NERVE_ANON(MarioActorNrvWait));
        nebula_log("[SMG2-PLAYER] Void fall respawn -> (%.1f, %.1f, %.1f)", mPos.x, mPos.y, mPos.z);
    }
}

void MarioActor::exeWait() {
    mAnimState = 0;
    const f32 stickMag = __builtin_sqrtf(mStickX * mStickX + mStickY * mStickY);
    if (!mGrounded) {
        setNerve(GET_NERVE_ANON(MarioActorNrvFall));
    } else if ((mButtons & ~mPrevButtons & NEBULA_BUTTON_A) != 0u) {
        setNerve(GET_NERVE_ANON(MarioActorNrvJump));
    } else if (stickMag > kStickMargin) {
        setNerve(GET_NERVE_ANON(MarioActorNrvWalk));
    }
}

void MarioActor::exeWalk() {
    mAnimState = 1;
    const f32 stickMag = __builtin_sqrtf(mStickX * mStickX + mStickY * mStickY);
    if (!mGrounded) {
        setNerve(GET_NERVE_ANON(MarioActorNrvFall));
    } else if ((mButtons & ~mPrevButtons & NEBULA_BUTTON_A) != 0u) {
        setNerve(GET_NERVE_ANON(MarioActorNrvJump));
    } else if (stickMag <= kStickMargin) {
        setNerve(GET_NERVE_ANON(MarioActorNrvWait));
    }
}

void MarioActor::exeJump() {
    mAnimState = 2;
    if (getNerveStep() == 0) {
        mVel.x += mUp.x * kJumpHeight;
        mVel.y += mUp.y * kJumpHeight;
        mVel.z += mUp.z * kJumpHeight;
        mGrounded = false;
    } else if (mGrounded) {
        setNerve(GET_NERVE_ANON(MarioActorNrvWait));
    }
}

void MarioActor::exeFall() {
    mAnimState = 2;
    if (mGrounded) {
        setNerve(GET_NERVE_ANON(MarioActorNrvWait));
    }
}

void MarioActor::update(f32 stickX, f32 stickY, u32 buttons, PlanetGravityManager* pGravityMgr,
                        const TVec3f& camPos, const TVec3f& camTarget, const TVec3f& camUp) {
    mStickX = stickX;
    mStickY = stickY;
    mButtons = buttons;

    if (mUsePlanarGravity) {
        mGravity.set(0.0f, -1.0f, 0.0f);
    } else if (pGravityMgr != nullptr) {
        GravityInfo info{};
        TVec3f grav(0.0f, -1.0f, 0.0f);
        if (pGravityMgr->calcTotalGravityVector(&grav, &info, mPos, GRAVITY_TYPE_NORMAL, 0, nullptr)) {
            normalizeVec3(grav, TVec3f(0.0f, -1.0f, 0.0f));
            mGravity = grav;
        }
    }

    TVec3f targetUp(-mGravity.x, -mGravity.y, -mGravity.z);
    normalizeVec3(targetUp, TVec3f(0.0f, 1.0f, 0.0f));
    mUp.x += (targetUp.x - mUp.x) * 0.25f;
    mUp.y += (targetUp.y - mUp.y) * 0.25f;
    mUp.z += (targetUp.z - mUp.z) * 0.25f;
    normalizeVec3(mUp, targetUp);

    TVec3f moveDir(0.0f, 0.0f, 0.0f);
    calcMoveDir(stickX, stickY, camPos, camTarget, camUp, &moveDir);
    const f32 moveLen = __builtin_sqrtf(moveDir.x * moveDir.x + moveDir.y * moveDir.y + moveDir.z * moveDir.z);
    const f32 clampedMag = (moveLen > 1.0f) ? 1.0f : moveLen;
    const f32 vUp = mVel.x * mUp.x + mVel.y * mUp.y + mVel.z * mUp.z;

    if (clampedMag > kStickMargin) {
        TVec3f moveNorm(moveDir.x / moveLen, moveDir.y / moveLen, moveDir.z / moveLen);
        mFrontVec = moveNorm;
        const f32 targetYaw = __builtin_atan2f(moveNorm.x, moveNorm.z);
        f32 diff = targetYaw - mYaw;
        while (diff > 3.14159265f) diff -= 6.2831853f;
        while (diff < -3.14159265f) diff += 6.2831853f;
        mYaw += diff * 0.25f;

        const f32 speed = clampedMag * kWalkSpeedMax;
        mVel.x = mUp.x * vUp + moveNorm.x * speed;
        mVel.y = mUp.y * vUp + moveNorm.y * speed;
        mVel.z = mUp.z * vUp + moveNorm.z * speed;
    } else {
        mVel.x = mUp.x * vUp;
        mVel.y = mUp.y * vUp;
        mVel.z = mUp.z * vUp;
    }

    updateNerve();
    mPrevButtons = buttons;

    mVel.x += mGravity.x * kGravityJumping;
    mVel.y += mGravity.y * kGravityJumping;
    mVel.z += mGravity.z * kGravityJumping;

    const f32 vFall = mVel.x * mGravity.x + mVel.y * mGravity.y + mVel.z * mGravity.z;
    if (vFall > kMaxDropSpeed) {
        const f32 excess = vFall - kMaxDropSpeed;
        mVel.x -= mGravity.x * excess;
        mVel.y -= mGravity.y * excess;
        mVel.z -= mGravity.z * excess;
    }

    mPos.x += mVel.x;
    mPos.y += mVel.y;
    mPos.z += mVel.z;

    resolveTerrainCollision();
}
