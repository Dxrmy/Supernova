#pragma once

#include "Game/Scene/Scene.hpp"

class StageDataHolder;

class GameScene : public Scene {
public:
    enum Phase {
        Phase_Title = 0,
        Phase_FileSelect = 1,
        Phase_InStage = 2,
    };

    GameScene();
    virtual ~GameScene();

    virtual void init();
    virtual void update();
    virtual void calcAnim();
    virtual void draw() const;

    Phase getPhase() const { return mPhase; }
    const char* getActiveStageName() const { return mActiveStage; }
    void enterFileSelect();
    void enterStage(const char* pStageName);

private:
    Phase mPhase;
    s32 mSelectedFileSlot;
    s32 mPhaseSteps;
    StageDataHolder* mStageDataHolder;
    const char* mActiveStage;
};
