#include "Game/System/GameSequenceFunction.hpp"
#include "Game/System/GameSequenceDirector.hpp"
#include "Game/System/GameSystem.hpp"
#include "Game/Util/SingletonHolder.hpp"

namespace {
    bool sTicoCoinInStage = false;
    bool sTicoCoinSaved = false;
}  // namespace

namespace GameSequenceFunction {
    void collectTicoCoinInStage() {
        sTicoCoinInStage = true;
    }

    void collectTicoCoinIfSaved() {
        if (sTicoCoinInStage) {
            sTicoCoinSaved = true;
        }
    }

    bool isCollectTicoCoinInStage() {
        return sTicoCoinInStage;
    }

    bool isCollectTicoCoinSaved() {
        return sTicoCoinSaved;
    }

    bool isPadMode2() {
        return false;
    }

    GameSequenceDirector* getGameSequenceDirector() {
        return SingletonHolder<GameSystem>::exists() ? SingletonHolder<GameSystem>::get()->mSequenceDirector : nullptr;
    }

    void requestChangeScene(const char* pSceneName) {
        if (GameSequenceDirector* pDir = getGameSequenceDirector()) {
            pDir->requestChangeScene(pSceneName);
        }
    }

    bool isActiveSaveDataHandleSequence() {
        GameSequenceDirector* pDir = getGameSequenceDirector();
        return pDir != nullptr && pDir->isActiveSaveDataHandleSequence();
    }
}  // namespace GameSequenceFunction
