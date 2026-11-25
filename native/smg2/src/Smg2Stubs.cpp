// Isolated TODO(Garigari) stubs for symbols referenced by wired Garigari TUs whose
// defining translation units are not yet decompiled in upstream Garigari (SB4E01).

#include <nebula_module.h>
#include "Game/LiveActor/Nerve.hpp"
#include "Game/NameObj/NameObj.hpp"
#include "Game/NameObj/NameObjListExecutor.hpp"
#include "Game/Scene/SceneNameObjMovementController.hpp"
#include "Game/Scene/SceneNameObjListExecutor.hpp"
#include "Game/System/MainLoopFramework.hpp"
#include "Game/System/NerveExecutor.hpp"
#include "Game/Util/JMapLinkInfo.hpp"
#include "Game/Util/MtxUtil.hpp"
#include "Game/Util/NerveUtil.hpp"
#include "Game/Util/SceneUtil.hpp"

#define SMG2_UNIMPLEMENTED(name)                                                   \
    do {                                                                           \
        static bool sLogged = false;                                               \
        if (!sLogged) {                                                            \
            sLogged = true;                                                        \
            nebula_log("[SMG2] TODO(Garigari): unimplemented symbol %s", (name));  \
        }                                                                          \
    } while (0)

// TODO(Garigari): Nerve.cpp / NerveUtil.cpp not yet decompiled in upstream Garigari.
void Nerve::executeOnEnd(Spine*) const {}

namespace MR {
    bool isFirstStep(const NerveExecutor* pExecutor) {
        return pExecutor != nullptr && pExecutor->getNerveStep() == 0;
    }

    void notifyRequestNameObjMovementOnOff() {
        SMG2_UNIMPLEMENTED("MR::notifyRequestNameObjMovementOnOff");
    }
}  // namespace MR

// TODO(Garigari): NameObjListExecutor.cpp / SceneNameObjListExecutor.cpp not yet decompiled.
NameObjListExecutor::NameObjListExecutor()
    : mBufferHolder(nullptr), mMovementList(nullptr), mCalcAnimList(nullptr), mDrawList(nullptr) {
}
NameObjListExecutor::~NameObjListExecutor() {}
void NameObjListExecutor::initMovementList() { SMG2_UNIMPLEMENTED("NameObjListExecutor::initMovementList"); }
void NameObjListExecutor::initCalcAnimList() { SMG2_UNIMPLEMENTED("NameObjListExecutor::initCalcAnimList"); }
void NameObjListExecutor::initCalcViewAndEntryList() { SMG2_UNIMPLEMENTED("NameObjListExecutor::initCalcViewAndEntryList"); }
void NameObjListExecutor::initDrawList() { SMG2_UNIMPLEMENTED("NameObjListExecutor::initDrawList"); }
void NameObjListExecutor::init() { SMG2_UNIMPLEMENTED("NameObjListExecutor::init"); }

void SceneNameObjListExecutor::initMovementList() { SMG2_UNIMPLEMENTED("SceneNameObjListExecutor::initMovementList"); }
void SceneNameObjListExecutor::initCalcAnimList() { SMG2_UNIMPLEMENTED("SceneNameObjListExecutor::initCalcAnimList"); }
void SceneNameObjListExecutor::initCalcViewAndEntryList() { SMG2_UNIMPLEMENTED("SceneNameObjListExecutor::initCalcViewAndEntryList"); }
void SceneNameObjListExecutor::initDrawList() { SMG2_UNIMPLEMENTED("SceneNameObjListExecutor::initDrawList"); }

// TODO(Garigari): MainLoopFramework::setTickRateFromFrame used by GameSystemFrameControl.cpp.
alignas(MainLoopFramework) static unsigned char sMainLoopFrameworkStorage[sizeof(MainLoopFramework)]{};
MainLoopFramework* MainLoopFramework::sManager = reinterpret_cast<MainLoopFramework*>(sMainLoopFrameworkStorage);
void MainLoopFramework::setTickRateFromFrame(u16) {
    SMG2_UNIMPLEMENTED("MainLoopFramework::setTickRateFromFrame");
}

// TODO(Garigari): ObjUtil.cpp not yet decompiled in upstream Garigari.
namespace MR {
    void requestMovementOn(NameObj* pObj) {
        if (pObj != nullptr) {
            NameObjFunction::requestMovementOn(pObj);
        }
    }

    void requestMovementOff(NameObj* pObj) {
        if (pObj != nullptr) {
            NameObjFunction::requestMovementOff(pObj);
        }
    }
}  // namespace MR

// TODO(Garigari): JMapLinkInfo.cpp not yet decompiled in upstream Garigari.
JMapLinkInfo::JMapLinkInfo() : mFollowId(-1), mZoneId(-1), mContext(-1) {}
JMapLinkInfo::JMapLinkInfo(const JMapInfoIter&, bool) : mFollowId(-1), mZoneId(-1), mContext(-1) {
    SMG2_UNIMPLEMENTED("JMapLinkInfo::JMapLinkInfo(const JMapInfoIter&, bool)");
}
bool JMapLinkInfo::isValid() const { return mFollowId >= 0; }
void JMapLinkInfo::invalidate() { mFollowId = -1; mZoneId = -1; mContext = -1; }
void JMapLinkInfo::setLinkedInfo(const JMapInfoIter&) {
    SMG2_UNIMPLEMENTED("JMapLinkInfo::setLinkedInfo");
}
void JMapLinkInfo::setLinkInfo(const JMapInfoIter&) {
    SMG2_UNIMPLEMENTED("JMapLinkInfo::setLinkInfo");
}

// TODO(Garigari): SceneUtil.cpp is not yet decompiled in upstream Garigari.
namespace MR {
    static TPos3f sIdentityZoneMtx = []() {
        TPos3f m;
        m.identity();
        return m;
    }();

    const char* getCurrentStageName() {
        SMG2_UNIMPLEMENTED("MR::getCurrentStageName");
        return "";
    }

    s32 getPlacedZoneId(const JMapInfoIter&) {
        SMG2_UNIMPLEMENTED("MR::getPlacedZoneId");
        return 0;
    }

    bool isPlacementLocalStage() {
        SMG2_UNIMPLEMENTED("MR::isPlacementLocalStage");
        return false;
    }

    TPos3f& getZonePlacementMtx(const JMapInfoIter&) {
        SMG2_UNIMPLEMENTED("MR::getZonePlacementMtx");
        return sIdentityZoneMtx;
    }

    // TODO(Garigari): Remaining MtxUtil.cpp functions not yet decompiled upstream.
    void makeMtxRotate(MtxPtr mtx, f32, f32, f32) {
        SMG2_UNIMPLEMENTED("MR::makeMtxRotate(MtxPtr, f32, f32, f32)");
        if (mtx != nullptr) PSMTXIdentity(mtx);
    }

    void makeMtxRotate(MtxPtr mtx, const TVec3f&) {
        SMG2_UNIMPLEMENTED("MR::makeMtxRotate(MtxPtr, const TVec3f&)");
        if (mtx != nullptr) PSMTXIdentity(mtx);
    }

    void makeMtxTR(MtxPtr mtx, f32, f32, f32, f32, f32, f32) {
        SMG2_UNIMPLEMENTED("MR::makeMtxTR(MtxPtr, f32 x6)");
        if (mtx != nullptr) PSMTXIdentity(mtx);
    }

    void makeMtxTR(MtxPtr mtx, const TVec3f&, const TVec3f&) {
        SMG2_UNIMPLEMENTED("MR::makeMtxTR(MtxPtr, const TVec3f&, const TVec3f&)");
        if (mtx != nullptr) PSMTXIdentity(mtx);
    }

    void makeMtxFrontUp(TPos3f* mtx, const TVec3f&, const TVec3f&) {
        SMG2_UNIMPLEMENTED("MR::makeMtxFrontUp");
        if (mtx != nullptr) mtx->identity();
    }

    void makeMtxSideUp(TPos3f* mtx, const TVec3f&, const TVec3f&) {
        SMG2_UNIMPLEMENTED("MR::makeMtxSideUp");
        if (mtx != nullptr) mtx->identity();
    }

    void makeMtxUpFront(TPos3f* mtx, const TVec3f&, const TVec3f&) {
        SMG2_UNIMPLEMENTED("MR::makeMtxUpFront");
        if (mtx != nullptr) mtx->identity();
    }

    void makeMtxUpNoSupport(TPos3f* mtx, const TVec3f&) {
        SMG2_UNIMPLEMENTED("MR::makeMtxUpNoSupport");
        if (mtx != nullptr) mtx->identity();
    }
}  // namespace MR
