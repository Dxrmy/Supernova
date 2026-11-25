#pragma once

#include <revolution/types.h>

class GameDataHolder;
class SaveDataFileAccessor;
class SaveDataHandler;

class SaveDataHandleSequence {
public:
    SaveDataHandleSequence();
    ~SaveDataHandleSequence();

    void update();
    bool isActive() const { return mIsActive; }
    bool isDone() const;

    bool requestPreLoad(GameDataHolder* pHolder);
    bool requestSaveUserSlot(s32 userSlot, const GameDataHolder* pHolder);
    bool restoreUserSlot(s32 userSlot, GameDataHolder* pHolder);

    SaveDataHandler* getHandler() const { return mHandler; }

private:
    SaveDataHandler* mHandler;
    SaveDataFileAccessor* mAccessor;
    bool mIsActive;
};
