#include "Game/System/GameSystemFunction.hpp"
#include "Game/NameObj/NameObjRegister.hpp"
#include "Game/System/GameSystem.hpp"
#include "Game/System/GameSystemSceneController.hpp"
#include "Game/Util/SingletonHolder.hpp"

namespace GameSystemFunction {
    bool isResetProcessing() {
        if (!SingletonHolder<GameSystem>::exists()) {
            return false;
        }
        GameSystem* pSys = SingletonHolder<GameSystem>::get();
        return pSys->mSceneController != nullptr && pSys->mSceneController->mIsResetProcessing;
    }

    bool isOccurredSystemWarning() {
        return false;
    }

    void setSceneNameObjHolderToNameObjRegister() {
        if (!SingletonHolder<GameSystem>::exists() || !SingletonHolder<NameObjRegister>::exists()) {
            return;
        }
        GameSystem* pSys = SingletonHolder<GameSystem>::get();
        if (pSys->mSceneController != nullptr && pSys->mSceneController->mObjHolder != nullptr) {
            SingletonHolder<NameObjRegister>::get()->setCurrentHolder(pSys->mSceneController->mObjHolder);
        }
    }

    void restartGameSystem() {
        if (SingletonHolder<GameSystem>::exists()) {
            SingletonHolder<GameSystem>::get()->prepareReset();
        }
    }

    bool isDoneLoadSystemArchive() {
        return SingletonHolder<GameSystem>::exists() && SingletonHolder<GameSystem>::get()->isDoneLoadSystemArchive();
    }

    void startToLoadSystemArchive() {
        if (SingletonHolder<GameSystem>::exists()) {
            SingletonHolder<GameSystem>::get()->startToLoadSystemArchive();
        }
    }
}  // namespace GameSystemFunction
