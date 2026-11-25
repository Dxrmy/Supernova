#include "Game/System/SaveDataFileAccessor.hpp"
#include "Game/System/SaveDataHandler.hpp"

SaveDataFileAccessor::SaveDataFileAccessor(SaveDataHandler* pHandler)
    : mHandler(pHandler) {
}

bool SaveDataFileAccessor::saveUserSlot(s32 userSlot, const GameDataHolder* pHolder) {
    if (mHandler == nullptr) {
        return false;
    }
    if (!mHandler->storeGameData(userSlot, pHolder)) {
        return false;
    }
    return mHandler->trySave(nullptr);
}

bool SaveDataFileAccessor::loadUserSlot(s32 userSlot, GameDataHolder* pHolder) {
    if (mHandler == nullptr) {
        return false;
    }
    return mHandler->loadFromExistingOrInit(pHolder) && mHandler->restoreGameData(userSlot, pHolder);
}

bool SaveDataFileAccessor::flushToDisk() {
    return mHandler != nullptr && mHandler->trySave(nullptr);
}
