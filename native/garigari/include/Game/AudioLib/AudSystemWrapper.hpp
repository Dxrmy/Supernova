#pragma once

#include <revolution/types.h>

class AudSystemWrapper {
public:
    AudSystemWrapper();

    void createAudioSystem();
    bool isCreated() const { return mCreated; }
    void movement();
    void soundFrameTick();
    void startStageBgm(const char* pStageName);

private:
    bool mCreated;
    bool mVoicesRegistered;
    const char* mCurrentBgm;
};
