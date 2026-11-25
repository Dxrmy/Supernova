#include "Game/System/GameSystemSceneController.hpp"
#include "Game/LiveActor/Spine.hpp"
#include "Game/NameObj/NameObjHolder.hpp"
#include "Game/Scene/Scene.hpp"
#include "Game/Scene/SceneFactory.hpp"
#include "Game/System/ScenarioDataParser.hpp"
#include "Game/Util/SceneUtil.hpp"
#include <cstring>
#include <nebula_module.h>

namespace NrvGameSystemSceneController {
    class NotInitialized : public Nerve {
    public:
        void execute(Spine*) const override {}
    };
    class Normal : public Nerve {
    public:
        void execute(Spine*) const override {}
    };
    class WaitDrawDoneScene : public Nerve {
    public:
        void execute(Spine*) const override {}
    };

    static NotInitialized sNotInitialized;
    static Normal sNormal;
    static WaitDrawDoneScene sWaitDrawDoneScene;
}  // namespace NrvGameSystemSceneController

SceneControlInfo::SceneControlInfo() : mScenarioNo(1), mSelectedScenarioNo(1), mStartIdInfo(nullptr) {
    mScene[0] = '\0';
    mStage[0] = '\0';
    mStartIdInfo = new JMapIdInfo();
}

void SceneControlInfo::setScene(const char* pSceneName) {
    if (pSceneName == nullptr) {
        mScene[0] = '\0';
        return;
    }
    nebula_snprintf(mScene, sizeof(mScene), "%s", pSceneName);
}

void SceneControlInfo::setStage(const char* pStageName) {
    if (pStageName == nullptr) {
        mStage[0] = '\0';
        return;
    }
    nebula_snprintf(mStage, sizeof(mStage), "%s", pStageName);
}

void SceneControlInfo::setStartIdInfo(const JMapIdInfo& rInfo) {
    if (mStartIdInfo != nullptr) {
        *mStartIdInfo = rInfo;
    }
}

GameSystemSceneController::GameSystemSceneController()
    : mCurrSceneControlInfo(),
      mNextSceneControlInfo(),
      mSpine(nullptr),
      mNextNerve(nullptr),
      mIsResetProcessing(false),
      mSceneInitializeHeld(false),
      mScenarioParser(nullptr),
      mObjHolder(nullptr),
      mScene(nullptr),
      mSceneInitializeState(SceneInitializeState_NotInit),
      mIntermissionScene(nullptr),
      mScenarioSelectScene(nullptr),
      mSceneOverlay(nullptr),
      mFirstNormalFrame(false) {
    mObjHolder = new NameObjHolder(0x1300);
    mSpine = new Spine(this, &NrvGameSystemSceneController::sNotInitialized);
}

void GameSystemSceneController::initAfterStationedResourceLoaded() {
    if (mScenarioParser == nullptr) {
        mScenarioParser = new ScenarioDataParser("ScenarioDataParser");
        mScenarioParser->initWithoutIter();
    }
}

void GameSystemSceneController::requestChangeScene() {
    if (isExistRequest() || mIsResetProcessing) {
        return;
    }
    mSceneInitializeState = SceneInitializeState_NotInit;
    mNextNerve = &NrvGameSystemSceneController::sWaitDrawDoneScene;
}

void GameSystemSceneController::checkRequestAndChangeScene() {
    if (mNextNerve != nullptr) {
        mSpine->setNerve(mNextNerve);
        mNextNerve = nullptr;
        if (mSpine->getCurrentNerve() == &NrvGameSystemSceneController::sWaitDrawDoneScene) {
            initializeScene();
            mSpine->setNerve(&NrvGameSystemSceneController::sNormal);
        }
    }
    mSpine->update();
}

void GameSystemSceneController::initializeScene() {
    mSceneInitializeState = SceneInitializeState_Init;
    if (mScene != nullptr) {
        delete mScene;
        mScene = nullptr;
    }
    std::memcpy(&mCurrSceneControlInfo, &mNextSceneControlInfo, sizeof(SceneControlInfo));
    mScene = MR::createScene(mCurrSceneControlInfo.mScene);
    if (mScene != nullptr) {
        mScene->init();
    } else {
        nebula_log("[SMG2] TODO(Garigari): GameSystemSceneController::initializeScene(scene='%s', stage='%s') returned null",
                   mCurrSceneControlInfo.mScene, mCurrSceneControlInfo.mStage);
    }
    mSceneInitializeState = SceneInitializeState_Done;
    mFirstNormalFrame = true;
}

void GameSystemSceneController::destroyScene() {
    mScene = nullptr;
    mSceneInitializeState = SceneInitializeState_NotInit;
}

void GameSystemSceneController::updateScene() {
    if (mScene != nullptr) {
        mScene->update();
    }
    mFirstNormalFrame = false;
}

void GameSystemSceneController::calcAnimScene() {
    if (mScene != nullptr) {
        mScene->calcAnim();
    }
}

void GameSystemSceneController::drawScene() {
    if (mScene != nullptr) {
        mScene->draw();
    }
}

bool GameSystemSceneController::isFirstUpdateSceneNerveNormal() const {
    return mFirstNormalFrame;
}

void GameSystemSceneController::prepareReset() {
    mIsResetProcessing = true;
}

bool GameSystemSceneController::isPreparedReset() const {
    return mIsResetProcessing;
}

bool GameSystemSceneController::isExistRequest() const {
    return mNextNerve != nullptr;
}
