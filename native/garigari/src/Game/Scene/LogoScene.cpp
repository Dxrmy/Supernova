#include "Game/Scene/LogoScene.hpp"
#include "Game/LiveActor/Nerve.hpp"
#include "Game/Scene/SceneFunction.hpp"
#include "Game/Scene/SceneObjHolder.hpp"
#include "Game/System/GameSequenceFunction.hpp"
#include "Game/System/GameSystemFunction.hpp"
#include "Game/Util/NerveUtil.hpp"
#include <nebula_module.h>

namespace Smg2Render {
    void setVisualScreenState(int state, float alpha, s32 fileSlot, const char* stageName);
}  // namespace Smg2Render

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
}

void LogoScene::calcAnim() {
    SceneFunction::executeCalcAnimList();
    SceneFunction::executeCalcViewAndEntryList2D();
}

void LogoScene::draw() const {
    // 2D Strap/Logo fade state is pushed to Smg2Render in exe* steps.
}

bool LogoScene::isDisplayStrapRemineder() const {
    return isNerve(GET_NERVE(LogoScene, LogoSceneNrvDeactive)) ||
           isNerve(GET_NERVE(LogoScene, LogoSceneNrvStrapFadein)) ||
           isNerve(GET_NERVE(LogoScene, LogoSceneNrvStrapDisplay)) ||
           isNerve(GET_NERVE(LogoScene, LogoSceneNrvStrapFadeout)) ||
           isNerve(GET_NERVE(LogoScene, LogoSceneNrvWaitReadDoneSystemArchive));
}

void LogoScene::initLayout() {
    // TODO(Garigari): instantiate SimpleLayout("Strap") & LogoFader once LayoutActor/SimpleLayout deps are wired.
    Smg2Render::setVisualScreenState(0, 0.0f, 0, nullptr);
}

bool LogoScene::tryFadeinLayout(LayoutActor*) {
    const s32 step = getNerveStep();
    const float alpha = step >= 15 ? 1.0f : static_cast<float>(step) / 15.0f;
    Smg2Render::setVisualScreenState(0, alpha, 0, nullptr);
    return step >= 15;
}

bool LogoScene::tryFadeoutLayout(LayoutActor*) {
    const s32 step = getNerveStep();
    const float alpha = step >= 15 ? 0.0f : 1.0f - static_cast<float>(step) / 15.0f;
    Smg2Render::setVisualScreenState(0, alpha, 0, nullptr);
    return step >= 15;
}

void LogoScene::exeStrapFadein() {
    if (tryFadeinLayout(nullptr)) {
        setNerve(GET_NERVE(LogoScene, LogoSceneNrvStrapDisplay));
    }
}

void LogoScene::exeStrapDisplay() {
    Smg2Render::setVisualScreenState(0, 1.0f, 0, nullptr);
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
