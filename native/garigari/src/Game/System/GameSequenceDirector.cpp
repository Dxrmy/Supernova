#include "Game/System/GameSequenceDirector.hpp"
#include "Game/System/GameDataFunction.hpp"
#include "Game/System/GameSystem.hpp"
#include "Game/System/GameSystemDataHolder.hpp"
#include "Game/System/GameSystemSceneController.hpp"
#include "Game/System/SaveDataHandleSequence.hpp"
#include "Game/Util/SingletonHolder.hpp"
#include <nebula_module.h>

GameSequenceDirector::GameSequenceDirector(GameSystemDataHolder* pDataHolder)
    : mDataHolder(pDataHolder),
      mSaveSequence(new SaveDataHandleSequence()),
      mInitializedAfterResource(false) {
}

void GameSequenceDirector::update() {
    if (mSaveSequence != nullptr) {
        mSaveSequence->update();
    }
}

void GameSequenceDirector::draw() const {
    // TODO(Garigari): draw sequence-level wipe/save UI once Screen/Ui is reconstructed (M4).
}

void GameSequenceDirector::initAfterResourceLoaded() {
    if (mInitializedAfterResource) {
        return;
    }
    mInitializedAfterResource = true;
    if (mSaveSequence != nullptr) {
        mSaveSequence->requestPreLoad(GameDataFunction::getCurrentGameDataHolder());
    }
}

void GameSequenceDirector::requestChangeScene(const char* pSceneName) {
    if (!SingletonHolder<GameSystem>::exists()) {
        return;
    }
    GameSystemSceneController* pCtrl = SingletonHolder<GameSystem>::get()->mSceneController;
    if (pCtrl == nullptr) {
        return;
    }
    pCtrl->mNextSceneControlInfo.setScene(pSceneName);
    pCtrl->requestChangeScene();
}

bool GameSequenceDirector::isActiveSaveDataHandleSequence() const {
    return mSaveSequence != nullptr && mSaveSequence->isActive();
}
