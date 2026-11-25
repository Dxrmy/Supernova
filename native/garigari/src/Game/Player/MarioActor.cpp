#include "Game/Player/MarioActor.hpp"
#include "Game/Gravity/GravityInfo.hpp"
#include "Game/Gravity/PlanetGravity.hpp"
#include "Game/Gravity/PlanetGravityManager.hpp"
#include <nebula_game_api.h>
#include <nebula_module.h>

namespace Smg23D {
    bool resolveSphereCollision(f32* ioX, f32* ioY, f32* ioZ, f32 radius, f32* outNormalX, f32* outNormalY, f32* outNormalZ);
    bool raycastPlanet(f32 x, f32 y, f32 z, f32 dx, f32 dy, f32 dz, f32 maxDist,
                       f32* outX, f32* outY, f32* outZ, f32* outNx, f32* outNy, f32* outNz);
}  // namespace Smg23D

namespace {
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
}  // namespace

MarioActor::MarioActor()
    : mPos(0.0f, 1200.0f, 0.0f),
      mVel(0.0f, 0.0f, 0.0f),
      mGravity(0.0f, -1.0f, 0.0f),
      mUp(0.0f, 1.0f, 0.0f),
      mYaw(0.0f),
      mAnimState(0),
      mGrounded(false) {
}

void MarioActor::init(const char* pStageName) {
    mPos.set(0.0f, 1200.0f, 0.0f);
    mVel.set(0.0f, 0.0f, 0.0f);
    mGravity.set(0.0f, -1.0f, 0.0f);
    mUp.set(0.0f, 1.0f, 0.0f);
    mYaw = 0.0f;
    mAnimState = 0;
    mGrounded = false;

    // Snap initial spawn onto KCL planet surface if available
    f32 hx = 0.0f, hy = 0.0f, hz = 0.0f, nx = 0.0f, ny = 1.0f, nz = 0.0f;
    if (Smg23D::raycastPlanet(mPos.x, mPos.y + 500.0f, mPos.z, 0.0f, -1.0f, 0.0f, 4000.0f,
                              &hx, &hy, &hz, &nx, &ny, &nz)) {
        mPos.set(hx, hy, hz);
        mUp.set(nx, ny, nz);
        mGrounded = true;
    }
    nebula_log("[SMG2-PLAYER] MarioActor::init(stage='%s') spawn=(%.1f, %.1f, %.1f) grounded=%d",
               pStageName != nullptr ? pStageName : "", mPos.x, mPos.y, mPos.z, mGrounded ? 1 : 0);
}

void MarioActor::update(f32 stickX, f32 stickY, u32 buttons, PlanetGravityManager* pGravityMgr) {
    if (pGravityMgr != nullptr) {
        GravityInfo info{};
        TVec3f grav(0.0f, -1.0f, 0.0f);
        if (pGravityMgr->calcTotalGravityVector(&grav, &info, mPos, GRAVITY_TYPE_NORMAL, 0, nullptr)) {
            normalizeVec3(grav, TVec3f(0.0f, -1.0f, 0.0f));
            mGravity = grav;
        }
    }

    // Target up vector is opposite gravity (or surface normal when grounded)
    TVec3f targetUp(-mGravity.x, -mGravity.y, -mGravity.z);
    normalizeVec3(targetUp, TVec3f(0.0f, 1.0f, 0.0f));

    // Smoothly align mUp toward targetUp
    mUp.x += (targetUp.x - mUp.x) * 0.25f;
    mUp.y += (targetUp.y - mUp.y) * 0.25f;
    mUp.z += (targetUp.z - mUp.z) * 0.25f;
    normalizeVec3(mUp, targetUp);

    // Build tangent basis orthogonal to mUp
    TVec3f refFwd(__builtin_sinf(mYaw), 0.0f, __builtin_cosf(mYaw));
    // Project refFwd onto plane perpendicular to mUp
    const f32 dotFU = refFwd.x * mUp.x + refFwd.y * mUp.y + refFwd.z * mUp.z;
    TVec3f fwd(refFwd.x - mUp.x * dotFU, refFwd.y - mUp.y * dotFU, refFwd.z - mUp.z * dotFU);
    if (fwd.x * fwd.x + fwd.y * fwd.y + fwd.z * fwd.z < 1.0e-4f) {
        fwd.set(0.0f, 0.0f, 1.0f);
        const f32 d2 = fwd.x * mUp.x + fwd.y * mUp.y + fwd.z * mUp.z;
        fwd.set(fwd.x - mUp.x * d2, fwd.y - mUp.y * d2, fwd.z - mUp.z * d2);
    }
    normalizeVec3(fwd, TVec3f(0.0f, 0.0f, 1.0f));
    // right = fwd x mUp
    TVec3f right(fwd.y * mUp.z - fwd.z * mUp.y,
                 fwd.z * mUp.x - fwd.x * mUp.z,
                 fwd.x * mUp.y - fwd.y * mUp.x);
    normalizeVec3(right, TVec3f(1.0f, 0.0f, 0.0f));

    const f32 stickMag = __builtin_sqrtf(stickX * stickX + stickY * stickY);
    const f32 moveSpeed = 14.0f;
    const f32 vUp = mVel.x * mUp.x + mVel.y * mUp.y + mVel.z * mUp.z;

    if (stickMag > 0.1f) {
        mYaw = __builtin_atan2f(stickX, -stickY);
        const f32 sx = stickX * moveSpeed;
        const f32 sz = -stickY * moveSpeed;
        mVel.x = mUp.x * vUp + (right.x * sx + fwd.x * sz);
        mVel.y = mUp.y * vUp + (right.y * sx + fwd.y * sz);
        mVel.z = mUp.z * vUp + (right.z * sx + fwd.z * sz);
    } else {
        mVel.x = mUp.x * vUp;
        mVel.y = mUp.y * vUp;
        mVel.z = mUp.z * vUp;
    }

    if (mGrounded && (buttons & NEBULA_BUTTON_A) != 0u) {
        const f32 jumpSpeed = 24.0f;
        mVel.x += mUp.x * jumpSpeed;
        mVel.y += mUp.y * jumpSpeed;
        mVel.z += mUp.z * jumpSpeed;
        mGrounded = false;
    } else {
        mVel.x += mGravity.x * 1.4f;
        mVel.y += mGravity.y * 1.4f;
        mVel.z += mGravity.z * 1.4f;
    }

    mPos.x += mVel.x;
    mPos.y += mVel.y;
    mPos.z += mVel.z;

    // Resolve KCL sphere collision (radius 45.0f around Mario's center at mPos + mUp * 45)
    f32 cx = mPos.x + mUp.x * 45.0f;
    f32 cy = mPos.y + mUp.y * 45.0f;
    f32 cz = mPos.z + mUp.z * 45.0f;
    f32 nx = 0.0f, ny = 1.0f, nz = 0.0f;
    if (Smg23D::resolveSphereCollision(&cx, &cy, &cz, 45.0f, &nx, &ny, &nz)) {
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
        // Check short downward ray for gentle curved slopes
        f32 hx = 0.0f, hy = 0.0f, hz = 0.0f;
        if (mGrounded && Smg23D::raycastPlanet(mPos.x + mUp.x * 40.0f, mPos.y + mUp.y * 40.0f, mPos.z + mUp.z * 40.0f,
                                               -mUp.x, -mUp.y, -mUp.z, 90.0f, &hx, &hy, &hz, &nx, &ny, &nz)) {
            mPos.set(hx, hy, hz);
            mUp.set(nx, ny, nz);
            const f32 vn = mVel.x * nx + mVel.y * ny + mVel.z * nz;
            if (vn < 0.0f) {
                mVel.x -= nx * vn;
                mVel.y -= ny * vn;
                mVel.z -= nz * vn;
            }
        } else {
            mGrounded = false;
        }
    }

    if (!mGrounded) {
        mAnimState = 2;  // Jump
    } else if (stickMag > 0.1f) {
        mAnimState = 1;  // Run
    } else {
        mAnimState = 0;  // Wait
    }
}
