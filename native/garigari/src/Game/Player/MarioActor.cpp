#include "Game/Player/MarioActor.hpp"
#include "Game/Gravity/GravityInfo.hpp"
#include "Game/Gravity/PlanetGravity.hpp"
#include "Game/Gravity/PlanetGravityManager.hpp"
#include <nebula_game_api.h>
#include <nebula_module.h>

namespace Smg23D {
    bool resolveSphereCollision(f32* ioX, f32* ioY, f32* ioZ, f32 radius, f32* outNormalX, f32* outNormalY, f32* outNormalZ, bool topSurfaceOnly = false);
    bool raycastPlanet(f32 x, f32 y, f32 z, f32 dx, f32 dy, f32 dz, f32 maxDist,
                       f32* outX, f32* outY, f32* outZ, f32* outNx, f32* outNy, f32* outNz, bool topSurfaceOnly = false);
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
    : mPos(0.0f, 1600.0f, 0.0f),
      mSafeSpawnPos(0.0f, 1600.0f, 0.0f),
      mVel(0.0f, 0.0f, 0.0f),
      mGravity(0.0f, -1.0f, 0.0f),
      mUp(0.0f, 1.0f, 0.0f),
      mYaw(0.0f),
      mAnimState(0),
      mGrounded(false),
      mUsePlanarGravity(true) {
}

void MarioActor::init(const char* pStageName) {
    mPos.set(0.0f, 1600.0f, 0.0f);
    mVel.set(0.0f, 0.0f, 0.0f);
    mGravity.set(0.0f, -1.0f, 0.0f);
    mUp.set(0.0f, 1.0f, 0.0f);
    mYaw = 0.0f;
    mAnimState = 0;
    mGrounded = false;
    mUsePlanarGravity = true;
    if (pStageName != nullptr && std::strstr(pStageName, "Disc") == nullptr &&
        std::strstr(pStageName, "IslandFleet") == nullptr &&
        std::strstr(pStageName, "MarioFaceShip") == nullptr) {
        mUsePlanarGravity = false;
    }

    // Snap initial spawn onto top deck KCL surface (normal.y > 0.05f when planar/disc)
    f32 hx = 0.0f, hy = 0.0f, hz = 0.0f, nx = 0.0f, ny = 1.0f, nz = 0.0f;
    if (Smg23D::raycastPlanet(mPos.x, mPos.y + 400.0f, mPos.z, 0.0f, -1.0f, 0.0f, 4000.0f,
                              &hx, &hy, &hz, &nx, &ny, &nz, mUsePlanarGravity)) {
        mPos.set(hx, hy, hz);
        mUp.set(nx, ny, nz);
        mGrounded = true;
    }
    mSafeSpawnPos = mPos;
    nebula_log("[SMG2-PLAYER] MarioActor::init(stage='%s', planar=%d) spawn=(%.1f, %.1f, %.1f) grounded=%d",
               pStageName != nullptr ? pStageName : "", mUsePlanarGravity ? 1 : 0, mPos.x, mPos.y, mPos.z, mGrounded ? 1 : 0);
}

void MarioActor::update(f32 stickX, f32 stickY, u32 buttons, PlanetGravityManager* pGravityMgr, f32 camYaw) {
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

    // Target up vector is opposite gravity (or surface normal when grounded)
    TVec3f targetUp(-mGravity.x, -mGravity.y, -mGravity.z);
    normalizeVec3(targetUp, TVec3f(0.0f, 1.0f, 0.0f));

    // Smoothly align mUp toward targetUp
    mUp.x += (targetUp.x - mUp.x) * 0.25f;
    mUp.y += (targetUp.y - mUp.y) * 0.25f;
    mUp.z += (targetUp.z - mUp.z) * 0.25f;
    normalizeVec3(mUp, targetUp);

    const f32 stickMagRaw = __builtin_sqrtf(stickX * stickX + stickY * stickY);
    const f32 stickMag = (stickMagRaw > 1.0f) ? 1.0f : stickMagRaw;
    const f32 moveSpeed = 14.0f;
    const f32 vUp = mVel.x * mUp.x + mVel.y * mUp.y + mVel.z * mUp.z;

    if (stickMag > 0.1f) {
        // Camera-relative world direction (camYaw=0 places camera at +Z looking toward -Z)
        const f32 cosY = __builtin_cosf(camYaw);
        const f32 sinY = __builtin_sinf(camYaw);
        const f32 worldX = stickX * cosY - stickY * sinY;
        const f32 worldZ = -stickX * sinY - stickY * cosY;

        // Project onto surface tangent plane perpendicular to mUp
        const f32 dotU = worldX * mUp.x + worldZ * mUp.z;
        TVec3f moveDir(worldX - mUp.x * dotU, -mUp.y * dotU, worldZ - mUp.z * dotU);
        normalizeVec3(moveDir, TVec3f(worldX, 0.0f, worldZ));

        // Smoothly rotate Mario's facing yaw toward movement direction
        const f32 targetYaw = __builtin_atan2f(moveDir.x, moveDir.z);
        f32 diff = targetYaw - mYaw;
        while (diff > 3.14159265f) diff -= 6.2831853f;
        while (diff < -3.14159265f) diff += 6.2831853f;
        mYaw += diff * 0.25f;

        const f32 speed = stickMag * moveSpeed;
        mVel.x = mUp.x * vUp + moveDir.x * speed;
        mVel.y = mUp.y * vUp + moveDir.y * speed;
        mVel.z = mUp.z * vUp + moveDir.z * speed;
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

    // Clamp fall speed to prevent KCL tunneling
    const f32 vFall = mVel.x * mGravity.x + mVel.y * mGravity.y + mVel.z * mGravity.z;
    if (vFall > 35.0f) {
        const f32 excess = vFall - 35.0f;
        mVel.x -= mGravity.x * excess;
        mVel.y -= mGravity.y * excess;
        mVel.z -= mGravity.z * excess;
    }
    mPos.x += mVel.x;
    mPos.y += mVel.y;
    mPos.z += mVel.z;

    // Resolve KCL sphere collision (radius 45.0f around Mario's center at mPos + mUp * 45)
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
        // Raycast downward at the NEW horizontal position (mPos) and adjust ONLY altitude along mUp
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

    // Void fall kill plane & respawn
    if (mUsePlanarGravity && mPos.y < -1500.0f) {
        mPos = mSafeSpawnPos;
        mVel.set(0.0f, 0.0f, 0.0f);
        mUp.set(0.0f, 1.0f, 0.0f);
        mGrounded = true;
        nebula_log("[SMG2-PLAYER] Void fall respawn -> (%.1f, %.1f, %.1f)", mPos.x, mPos.y, mPos.z);
    }

    if (!mGrounded) {
        mAnimState = 2;  // Jump
    } else if (stickMag > 0.1f) {
        mAnimState = 1;  // Run
    } else {
        mAnimState = 0;  // Wait
    }
}
