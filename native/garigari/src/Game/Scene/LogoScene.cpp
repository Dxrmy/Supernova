#include "Game/Scene/LogoScene.hpp"
#include "Game/LiveActor/Nerve.hpp"
#include "Game/Scene/SceneFunction.hpp"
#include "Game/Scene/SceneObjHolder.hpp"
#include "Game/System/GameSequenceFunction.hpp"
#include "Game/System/GameSystemFunction.hpp"
#include "Game/Util/NerveUtil.hpp"
#include <nebula_module.h>

class LayoutManager;
namespace Smg2Ui {
    LayoutManager* createLayout(const char* pName, bool usePrefix, u32 rootAnimLayers, u32 textBoxLen, bool keepText);
    void movementLayout(LayoutManager* pMgr);
    void calcAnimLayout(LayoutManager* pMgr);
    void drawLayout(const LayoutManager* pMgr);
    void startRootAnim(LayoutManager* pMgr, const char* pAnimName, u32 layer);
    bool isRootAnimStopped(const LayoutManager* pMgr, u32 layer);
}  // namespace Smg2Ui

namespace {
    LayoutManager* sStrapLayout = nullptr;
}  // namespace

namespace NrvLogoScene {
    NEW_NERVE(LogoSceneNrvStrapFadein, LogoScene, StrapFadein);
    NEW_NERVE(LogoSceneNrvStrapDisplay, LogoScene, StrapDisplay);
    NEW_NERVE(LogoSceneNrvStrapFadeout, LogoScene, StrapFadeout);
    NEW_NERVE(LogoSceneNrvMountGameData, LogoScene, MountGameData);
    NEW_NERVE(LogoSceneNrvWaitReadDoneSystemArchive, LogoScene, WaitReadDoneSystemArchive);
    NEW_NERVE(LogoSceneNrvDeactive, LogoScene, Deactive);
};  // namespace NrvLogoScene

LogoScene::LogoScene() : Scene("LogoScene"), mStrapLayout(nullptr), mLogoFader(nullptr), _19(false) {
}

LogoScene::~LogoScene() = default;

void LogoScene::init() {
    initNerve(GET_NERVE(LogoScene, LogoSceneNrvStrapFadein));

    SceneFunction::createHioBasicNode(this);
    SceneFunction::initForNameObj();
    initLayout();
    nebula_log("[SMG2-SCENE] LogoScene::init()");
}

void LogoScene::update() {
    updateNerve();
    SceneFunction::executeMovementList();
    Smg2Ui::movementLayout(sStrapLayout);
}

void LogoScene::calcAnim() {
    SceneFunction::executeCalcAnimList();
    SceneFunction::executeCalcViewAndEntryList2D();
    Smg2Ui::calcAnimLayout(sStrapLayout);
}

void LogoScene::draw() const {
    Smg2Ui::drawLayout(sStrapLayout);
}

bool LogoScene::isDisplayStrapRemineder() const {
    return isNerve(GET_NERVE(LogoScene, LogoSceneNrvDeactive)) ||
           isNerve(GET_NERVE(LogoScene, LogoSceneNrvStrapFadein)) ||
           isNerve(GET_NERVE(LogoScene, LogoSceneNrvStrapDisplay)) ||
           isNerve(GET_NERVE(LogoScene, LogoSceneNrvStrapFadeout)) ||
           isNerve(GET_NERVE(LogoScene, LogoSceneNrvWaitReadDoneSystemArchive));
}

void LogoScene::initLayout() {
    if (sStrapLayout == nullptr) {
        sStrapLayout = Smg2Ui::createLayout("Strap", true, 1u, 0u, true);
    }
    if (sStrapLayout != nullptr) {
        Smg2Ui::startRootAnim(sStrapLayout, "Appear", 0u);
    }
}

bool LogoScene::tryFadeinLayout(LayoutActor*) {
    if (sStrapLayout != nullptr) {
        return Smg2Ui::isRootAnimStopped(sStrapLayout, 0u) || getNerveStep() >= 30;
    }
    return getNerveStep() >= 15;
}

bool LogoScene::tryFadeoutLayout(LayoutActor*) {
    if (sStrapLayout != nullptr && getNerveStep() == 0) {
        Smg2Ui::startRootAnim(sStrapLayout, "End", 0u);
    }
    if (sStrapLayout != nullptr) {
        return Smg2Ui::isRootAnimStopped(sStrapLayout, 0u) || getNerveStep() >= 30;
    }
    return getNerveStep() >= 15;
}

void LogoScene::exeStrapFadein() {
    if (tryFadeinLayout(nullptr)) {
        setNerve(GET_NERVE(LogoScene, LogoSceneNrvStrapDisplay));
    }
}

void LogoScene::exeStrapDisplay() {
    if (getNerveStep() >= 30) {
        setNerve(GET_NERVE(LogoScene, LogoSceneNrvStrapFadeout));
    }
}

void LogoScene::exeStrapFadeout() {
    if (tryFadeoutLayout(nullptr)) {
        setNerve(GET_NERVE(LogoScene, LogoSceneNrvMountGameData));
    }
}

void LogoScene::exeMountGameData() {
    if (!GameSequenceFunction::isActiveSaveDataHandleSequence()) {
        setNerve(GET_NERVE(LogoScene, LogoSceneNrvWaitReadDoneSystemArchive));
    }
}

void LogoScene::exeWaitReadDoneSystemArchive() {
    if (GameSystemFunction::isDoneLoadSystemArchive()) {
        nebula_log("[SMG2-SCENE] LogoScene complete -> requesting GameScene (Title)");
        GameSequenceFunction::requestChangeScene("Game");
        setNerve(GET_NERVE(LogoScene, LogoSceneNrvDeactive));
    }
}

void LogoScene::exeDeactive() {
}
