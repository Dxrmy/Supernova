#include "Game/System/GameSystemObjHolder.hpp"
#include "Game/AudioLib/AudSystemWrapper.hpp"
#include "Game/NameObj/NameObjHolder.hpp"
#include "Game/NameObj/NameObjRegister.hpp"
#include "Game/System/GameSystem.hpp"
#include "Game/System/Language.hpp"
#include "Game/System/ResourceHolderManager.hpp"
#include "Game/Util/SingletonHolder.hpp"
#include <nebula_module.h>

GameSystemObjHolder::GameSystemObjHolder()
    : mObjHolder(nullptr),
      mParticleResHolder(nullptr),
      mRenderModeObj(nullptr),
      _C(nullptr),
      _10(nullptr),
      _14(nullptr),
      mCaptureScreenDirector(nullptr),
      mScreenPreserver(nullptr),
      mAudioSystem(nullptr),
      mWPadHolder(nullptr),
      mFunctionAsyncExecutor(nullptr),
      mMessageHolder(nullptr),
      mStarPointerDirector(nullptr),
      mRandom(),
      mLanguage(MR::getDecidedLanguageFromIPL()),
      _3C(0),
      mPadRecorder(nullptr) {
    initNameObj();
}

void GameSystemObjHolder::init() {
    initDvd();
    initNAND();
    initAudio();
    initRenderMode();
    initResourceHolderManager();
    initMessageResource();
    initGameController();
    initDisplay();
}

void GameSystemObjHolder::createAudioSystem() {
    if (mAudioSystem == nullptr) {
        mAudioSystem = new AudSystemWrapper();
    }
    mAudioSystem->createAudioSystem();
}

void GameSystemObjHolder::initAfterStationedResourceLoaded() {
    if (SingletonHolder<NameObjRegister>::exists()) {
        SingletonHolder<NameObjRegister>::get()->setCurrentHolder(mObjHolder);
    }
}

void GameSystemObjHolder::update() {
    // TODO(Garigari): update WPadHolder and StarPointerDirector once Input/Screen are wired.
}

void GameSystemObjHolder::updateAudioSystem() {
    if (mAudioSystem != nullptr) {
        mAudioSystem->movement();
    }
}

void GameSystemObjHolder::clearRequestFileInfo(bool) {
    // TODO(Garigari): clear pending FileLoader requests when FileLoaderThread is wired.
}

void GameSystemObjHolder::drawStarPointer() {
    // TODO(Garigari): draw StarPointerDirector once Screen is wired (M4).
}

void GameSystemObjHolder::drawBeforeEndRender() {
    // TODO(Garigari): draw ScreenPreserver once Screen/Render is wired (M3/M4).
}

void GameSystemObjHolder::captureIfAllowForScreenPreserver() {
    // TODO(Garigari): capture screen preserver when Screen/Render is wired (M3/M4).
}

GXRenderModeObj* GameSystemObjHolder::getRenderModeObj() const {
    return mRenderModeObj;
}

void GameSystemObjHolder::initDvd() {
    // VFS is backed by Nebula's game.pak asset store.
}

void GameSystemObjHolder::initNAND() {
    // Save I/O is backed by NebulaHostServices::save_request / save_wait.
}

void GameSystemObjHolder::initAudio() {
    // TODO(Garigari): allocate AudSystemWrapper when AudioLib is ported (M5).
}

void GameSystemObjHolder::initRenderMode() {
    // Render mode is managed by Nebula's host renderer.
}

void GameSystemObjHolder::initNameObj() {
    mObjHolder = new NameObjHolder(256);
    if (!SingletonHolder<NameObjRegister>::exists()) {
        SingletonHolder<NameObjRegister>::set(new NameObjRegister());
    }
    SingletonHolder<NameObjRegister>::get()->setCurrentHolder(mObjHolder);
}

void GameSystemObjHolder::initMessageResource() {
    // TODO(Garigari): mount SystemMessage / MessageHolder once UI text is ported (M4).
}

void GameSystemObjHolder::initResourceHolderManager() {
    if (!SingletonHolder<ResourceHolderManager>::exists()) {
        SingletonHolder<ResourceHolderManager>::set(new ResourceHolderManager());
    }
}

void GameSystemObjHolder::initGameController() {
    // TODO(Garigari): allocate WPadHolder & StarPointerDirector once input/UI are wired.
}

void GameSystemObjHolder::initDisplay() {
    // TODO(Garigari): allocate CaptureScreenDirector & ScreenPreserver once Render/Screen are wired.
}

namespace MR {
    GameSystemObjHolder* getGameSystemObjHolder() {
        return SingletonHolder<GameSystem>::exists() ? SingletonHolder<GameSystem>::get()->mObjHolder : nullptr;
    }
}  // namespace MR
