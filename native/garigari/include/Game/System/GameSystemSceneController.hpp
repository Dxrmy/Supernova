#pragma once

#include "Game/LiveActor/Nerve.hpp"
#include "Game/Util/JMapIdInfo.hpp"
#include <revolution/types.h>

class NameObjHolder;
class ScenarioDataParser;
class Scene;
class Spine;

class SceneControlInfo {
public:
    SceneControlInfo();

    void setScene(const char* pSceneName);
    void setStage(const char* pStageName);
    void setStartIdInfo(const JMapIdInfo& rInfo);

    /* 0x00 */ char mScene[0x40];
    /* 0x40 */ char mStage[0x40];
    /* 0x80 */ s32 mScenarioNo;
    /* 0x84 */ s32 mSelectedScenarioNo;
    /* 0x88 */ JMapIdInfo* mStartIdInfo;
};

class GameSystemSceneController {
public:
    enum SceneInitializeState {
        SceneInitializeState_NotInit = 0,
        SceneInitializeState_Init = 1,
        SceneInitializeState_Done = 2,
    };

    GameSystemSceneController();

    void initAfterStationedResourceLoaded();
    void requestChangeScene();
    void checkRequestAndChangeScene();
    void initializeScene();
    void destroyScene();
    void updateScene();
    void calcAnimScene();
    void drawScene();
    bool isFirstUpdateSceneNerveNormal() const;
    void prepareReset();
    bool isPreparedReset() const;

    bool isExistRequest() const;
    const char* getCurrentSceneName() const { return mCurrSceneControlInfo.mScene; }
    const char* getCurrentStageName() const { return mCurrSceneControlInfo.mStage; }
    ScenarioDataParser* getScenarioDataParser() const { return mScenarioParser; }

    /* 0x000 */ SceneControlInfo mCurrSceneControlInfo;
    /* 0x08C */ SceneControlInfo mNextSceneControlInfo;
    /* 0x118 */ Spine* mSpine;
    /* 0x11C */ const Nerve* mNextNerve;
    /* 0x120 */ bool mIsResetProcessing;
    /* 0x121 */ bool mSceneInitializeHeld;
    /* 0x124 */ ScenarioDataParser* mScenarioParser;
    /* 0x128 */ NameObjHolder* mObjHolder;
    /* 0x12C */ Scene* mScene;
    /* 0x130 */ s32 mSceneInitializeState;
    /* 0x134 */ void* mIntermissionScene;
    /* 0x138 */ void* mScenarioSelectScene;
    /* 0x13C */ void* mSceneOverlay;
    /* 0x140 */ bool mFirstNormalFrame;
};
