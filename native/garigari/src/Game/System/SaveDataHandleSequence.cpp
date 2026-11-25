#include "Game/System/SaveDataHandleSequence.hpp"
#include "Game/System/SaveDataFileAccessor.hpp"
#include "Game/System/SaveDataHandler.hpp"

SaveDataHandleSequence::SaveDataHandleSequence()
    : mHandler(new SaveDataHandler()),
      mAccessor(new SaveDataFileAccessor(mHandler)),
      mIsActive(false) {
}

SaveDataHandleSequence::~SaveDataHandleSequence() {
    delete mAccessor;
    delete mHandler;
}

void SaveDataHandleSequence::update() {
    if (mHandler != nullptr) {
        mHandler->update();
        if (mHandler->isDone()) {
            mIsActive = false;
        }
    }
}

bool SaveDataHandleSequence::isDone() const {
    return mHandler == nullptr || mHandler->isDone();
}

bool SaveDataHandleSequence::requestPreLoad(GameDataHolder* pHolder) {
    if (mHandler == nullptr) {
        return false;
    }
    mIsActive = true;
    const bool ok = mHandler->loadFromExistingOrInit(pHolder);
    mIsActive = false;
    return ok;
}

bool SaveDataHandleSequence::requestSaveUserSlot(s32 userSlot, const GameDataHolder* pHolder) {
    if (mAccessor == nullptr) {
        return false;
    }
    mIsActive = true;
    const bool ok = mAccessor->saveUserSlot(userSlot, pHolder);
    mIsActive = false;
    return ok;
}

bool SaveDataHandleSequence::restoreUserSlot(s32 userSlot, GameDataHolder* pHolder) {
    if (mHandler == nullptr) {
        return false;
    }
    return mHandler->restoreGameData(userSlot, pHolder);
}
