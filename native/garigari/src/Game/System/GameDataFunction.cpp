#include "Game/System/GameDataFunction.hpp"
#include "Game/System/GameDataHolder.hpp"
#include "Game/Util/SingletonHolder.hpp"

namespace {
    GameDataHolder sDefaultDataHolder;
}  // namespace

namespace GameDataFunction {
    GameDataHolder* getCurrentGameDataHolder() {
        return &sDefaultDataHolder;
    }

    bool isOnGalaxyFlagTicoCoin(const char* pStageName) {
        if (pStageName == nullptr) {
            return false;
        }
        return getCurrentGameDataHolder()->isOnGameEventFlag(pStageName);
    }

    void onGameEventFlag(const char* pFlagName) {
        getCurrentGameDataHolder()->onGameEventFlag(pFlagName);
    }

    void offGameEventFlag(const char* pFlagName) {
        getCurrentGameDataHolder()->offGameEventFlag(pFlagName);
    }

    bool isOnGameEventFlag(const char* pFlagName) {
        return getCurrentGameDataHolder()->isOnGameEventFlag(pFlagName);
    }

    u16 getGameEventValue(const char* pValueName) {
        return getCurrentGameDataHolder()->getGameEventValue(pValueName);
    }

    void setGameEventValue(const char* pValueName, u16 value) {
        getCurrentGameDataHolder()->setGameEventValue(pValueName, value);
    }

    s32 getPlayerGameOverNum() {
        return getCurrentGameDataHolder()->getPlayerMissNum();
    }

    s32 getCurrentWorldId() {
        return getCurrentGameDataHolder()->getCurrentWorldId();
    }

    s32 calcCurrentPowerStarNum() {
        return getCurrentGameDataHolder()->calcCurrentPowerStarNum();
    }

    s32 calcNormalPowerStarNum() {
        return getCurrentGameDataHolder()->calcNormalPowerStarNum();
    }

    bool hasGrandStar(int grandStarId) {
        return getCurrentGameDataHolder()->hasGrandStar(grandStarId);
    }

    const char* getUserName() {
        return getCurrentGameDataHolder()->getUserName();
    }

    void setUserName(const char* pName) {
        getCurrentGameDataHolder()->setUserName(pName);
    }

    void resetAllGameData() {
        getCurrentGameDataHolder()->resetAllData();
    }
}  // namespace GameDataFunction
