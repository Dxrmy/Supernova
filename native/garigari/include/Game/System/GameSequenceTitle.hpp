#pragma once

#include <revolution/types.h>

class GameSequenceTitle {
public:
    GameSequenceTitle();

    void init();
    void update();
    bool isDecided() const { return mDecided; }
    s32 getSelectedSlot() const { return mSelectedSlot; }
    void setSelectedSlot(s32 slot) { mSelectedSlot = slot; }
    void decide() { mDecided = true; }

private:
    s32 mSelectedSlot;
    bool mDecided;
};
