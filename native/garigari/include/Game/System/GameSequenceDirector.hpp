#pragma once

#include <revolution/types.h>

class GameSystemDataHolder;
class SaveDataHandleSequence;

class GameSequenceDirector {
public:
    explicit GameSequenceDirector(GameSystemDataHolder* pDataHolder);

    void update();
    void draw() const;
    void initAfterResourceLoaded();
    void requestChangeScene(const char* pSceneName);
    bool isActiveSaveDataHandleSequence() const;
    SaveDataHandleSequence* getSaveDataHandleSequence() const { return mSaveSequence; }

private:
    GameSystemDataHolder* mDataHolder;
    SaveDataHandleSequence* mSaveSequence;
    bool mInitializedAfterResource;
};
