#pragma once

#include <revolution/os.h>

class GameDataHolder;
class GameDataSomeScenarioAccessor;
class JMapInfoIter;

namespace GameDataFunction {
    bool isOnGalaxyFlagTicoCoin(const char* pStageName);
    void onGameEventFlag(const char*);
    void offGameEventFlag(const char*);
    bool isOnGameEventFlag(const char*);
    u16 getGameEventValue(const char*);
    void setGameEventValue(const char*, u16);
    s32 getPlayerGameOverNum();
    s32 getCurrentWorldId();
    s32 calcCurrentPowerStarNum();
    s32 calcNormalPowerStarNum();
    bool hasGrandStar(int);
    const char* getUserName();
    void setUserName(const char*);
    void resetAllGameData();
    GameDataHolder* getCurrentGameDataHolder();
}  // namespace GameDataFunction
