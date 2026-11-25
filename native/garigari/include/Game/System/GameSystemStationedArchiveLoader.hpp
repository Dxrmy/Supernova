#pragma once

#include <revolution/types.h>

class GameSystemStationedArchiveLoader {
public:
    GameSystemStationedArchiveLoader();

    void update();
    bool isDone() const;
    void prepareReset();
    bool isPreparedReset() const;

    /* 0x00 */ s32 mState;
    /* 0x04 */ bool mIsDone;
    /* 0x05 */ bool mIsPreparedReset;
};
