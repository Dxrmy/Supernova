#pragma once

#include <revolution/types.h>

class GalaxyMoveArgument;
class GameSequenceDirector;
class UserFile;

namespace GameSequenceFunction {
    void collectTicoCoinInStage();
    void collectTicoCoinIfSaved();
    bool isCollectTicoCoinInStage();
    bool isCollectTicoCoinSaved();
    bool isPadMode2();
    void requestChangeScene(const char* pSceneName);
    bool isActiveSaveDataHandleSequence();
    GameSequenceDirector* getGameSequenceDirector();
};  // namespace GameSequenceFunction
