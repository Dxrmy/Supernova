#pragma once

#include <revolution/types.h>

class GameDataHolder;
class SaveDataHandler;

class SaveDataFileAccessor {
public:
    explicit SaveDataFileAccessor(SaveDataHandler* pHandler);

    bool saveUserSlot(s32 userSlot, const GameDataHolder* pHolder);
    bool loadUserSlot(s32 userSlot, GameDataHolder* pHolder);
    bool flushToDisk();

private:
    SaveDataHandler* mHandler;
};
