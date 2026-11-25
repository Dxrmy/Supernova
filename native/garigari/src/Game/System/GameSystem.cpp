#include "Game/System/GameSystem.hpp"
#include "Game/LiveActor/Nerve.hpp"
#include "Game/NameObj/NameObjRegister.hpp"
#include "Game/System/GameSequenceDirector.hpp"
#include "Game/System/GameSequenceFunction.hpp"
#include "Game/System/GameSystemDataHolder.hpp"
#include "Game/System/GameSystemDimmingWatcher.hpp"
#include "Game/System/GameSystemFontHolder.hpp"
#include "Game/System/GameSystemFrameControl.hpp"
#include "Game/System/GameSystemFunction.hpp"
#include "Game/System/GameSystemObjHolder.hpp"
#include "Game/System/GameSystemSceneController.hpp"
#include "Game/System/GameSystemStationedArchiveLoader.hpp"
#include "Game/Util/NerveUtil.hpp"
#include "Game/Util/SingletonHolder.hpp"
#include <nebula_module.h>

template <>
GameSystem* SingletonHolder<GameSystem>::sInstance = nullptr;

namespace NrvGameSystem {
    NEW_NERVE(GameSystemInitializeAudio, GameSystem, InitializeAudio);
    NEW_NERVE(GameSystemInitializeLogoScene, GameSystem, InitializeLogoScene);
    NEW_NERVE(GameSystemLoadStationedArchive, GameSystem, LoadStationedArchive);
    NEW_NERVE(GameSystemWaitForReboot, GameSystem, WaitForReboot);
    NEW_NERVE(GameSystemNormal, GameSystem, Normal);
}  // namespace NrvGameSystem

GameSystem::GameSystem()
    : NerveExecutor("GameSystem"),
      _8(nullptr),
      mSequenceDirector(nullptr),
      mDimmingWatcher(nullptr),
      mErrorWatcher(nullptr),
      mFontHolder(nullptr),
      mFrameControl(nullptr),
      mObjHolder(nullptr),
      mSceneController(nullptr),
      mStationedArchiveLoader(nullptr),
      mHomeButtonLayout(nullptr),
      mSystemWipeHolder(nullptr),
      mDataHolder(nullptr),
      mIsExecuteLoadSystemArchive(false) {
}

GameSystem::~GameSystem() = default;

void GameSystem::init() {
    mObjHolder = new GameSystemObjHolder();
    mFontHolder = nullptr;  // TODO(Garigari): wire GameSystemFontHolder once nw4r::ut::ResFont is ported.
    mDataHolder = new GameSystemDataHolder();

    initNerve(&NrvGameSystem::GameSystemInitializeAudio::sInstance);

    mSequenceDirector = new GameSequenceDirector(mDataHolder);
    initGX();
    mSceneController = new GameSystemSceneController();
    mObjHolder->init();
    mErrorWatcher = nullptr;  // TODO(Garigari): wire GameSystemErrorWatcher once Screen/Ui is ported.
    mFrameControl = new GameSystemFrameControl();
    mStationedArchiveLoader = new GameSystemStationedArchiveLoader();
    mHomeButtonLayout = nullptr;  // TODO(Garigari): wire HomeButtonLayout once Layout is ported.
    mDimmingWatcher = new GameSystemDimmingWatcher();

    setNerve(&NrvGameSystem::GameSystemInitializeAudio::sInstance);
}

bool GameSystem::isExecuteLoadSystemArchive() const {
    return mIsExecuteLoadSystemArchive;
}

bool GameSystem::isDoneLoadSystemArchive() const {
    return isNerve(&NrvGameSystem::GameSystemNormal::sInstance);
}

void GameSystem::startToLoadSystemArchive() {
    mIsExecuteLoadSystemArchive = true;
    SingletonHolder<NameObjRegister>::get()->setCurrentHolder(mObjHolder->mObjHolder);
    setNerve(&NrvGameSystem::GameSystemLoadStationedArchive::sInstance);
}

void GameSystem::exeInitializeAudio() {
    if (MR::isFirstStep(this)) {
        nebula_log("[SMG2] GameSystem::exeInitializeAudio: creating audio system");
        mObjHolder->createAudioSystem();
    }

    updateSceneController();

    // TODO(Garigari): wait on AudSystemWrapper::isLoadDoneWaveDataAtSystemInit() once AudioLib is reconstructed (M5).
    setNerve(&NrvGameSystem::GameSystemInitializeLogoScene::sInstance);
}

void GameSystem::exeInitializeLogoScene() {
    if (GameSystemFunction::isResetProcessing()) {
        setNerve(&NrvGameSystem::GameSystemWaitForReboot::sInstance);
        return;
    }

    if (MR::isFirstStep(this)) {
        nebula_log("[SMG2] GameSystem::exeInitializeLogoScene: requesting scene 'Logo'");
        GameSequenceFunction::requestChangeScene("Logo");
    }

    updateSceneController();

    // Advance into stationed archive load once LogoScene has been requested in bring-up.
    if (!mIsExecuteLoadSystemArchive) {
        startToLoadSystemArchive();
    }
}

void GameSystem::exeLoadStationedArchive() {
    if (mStationedArchiveLoader != nullptr) {
        mStationedArchiveLoader->update();
    }

    updateSceneController();

    if (mStationedArchiveLoader == nullptr || mStationedArchiveLoader->isDone()) {
        initAfterStationedResourceLoaded();
        setNerve(&NrvGameSystem::GameSystemNormal::sInstance);
    }
}

void GameSystem::exeWaitForReboot() {
    // TODO(Garigari): reboot handshake once GameSystemResetAndPowerProcess is wired.
}

void GameSystem::exeNormal() {
    updateSceneController();
}

void GameSystem::initGX() {
    nebula_log("[SMG2] TODO(Garigari): GameSystem::initGX() handled by Nebula host renderer");
}

void GameSystem::initAfterStationedResourceLoaded() {
    mObjHolder->initAfterStationedResourceLoaded();
    mSceneController->initAfterStationedResourceLoaded();
    mSequenceDirector->initAfterResourceLoaded();
    mDataHolder->init();
}

void GameSystem::prepareReset() {
    if (mStationedArchiveLoader != nullptr) {
        mStationedArchiveLoader->prepareReset();
    }
}

bool GameSystem::isPreparedReset() const {
    if (isNerve(&NrvGameSystem::GameSystemWaitForReboot::sInstance)) {
        return true;
    }
    return mStationedArchiveLoader != nullptr && mStationedArchiveLoader->isPreparedReset();
}

void GameSystem::frameLoop() {
    draw();
    update();
    calcAnim();
}

void GameSystem::draw() {
    if (mSceneController != nullptr) {
        mSceneController->drawScene();
    }
    if (mSequenceDirector != nullptr) {
        mSequenceDirector->draw();
    }
    if (mObjHolder != nullptr) {
        mObjHolder->drawStarPointer();
        mObjHolder->drawBeforeEndRender();
    }
}

void GameSystem::update() {
    if (mSceneController != nullptr) {
        mSceneController->checkRequestAndChangeScene();
    }
    if (mObjHolder != nullptr) {
        mObjHolder->update();
    }
    if (mDimmingWatcher != nullptr) {
        mDimmingWatcher->update();
    }
    updateNerve();
}

void GameSystem::updateSceneController() {
    if (mSequenceDirector != nullptr) {
        mSequenceDirector->update();
    }
    if (mSceneController != nullptr) {
        mSceneController->updateScene();
    }
    if (mObjHolder != nullptr) {
        mObjHolder->updateAudioSystem();
    }
}

void GameSystem::calcAnim() {
    if (mSceneController != nullptr) {
        mSceneController->calcAnimScene();
    }
}
