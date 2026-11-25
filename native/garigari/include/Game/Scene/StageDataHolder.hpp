#pragma once

#include <revolution/types.h>

class JKRArchive;

class StageDataHolder {
public:
    StageDataHolder();

    bool mountStage(const char* pStageName);
    const char* getStageName() const { return mStageName; }
    bool isMounted() const { return mMounted; }

private:
    char mStageName[64];
    JKRArchive* mScenarioArchive;
    bool mMounted;
};
