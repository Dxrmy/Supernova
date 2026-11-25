#include "Game/Scene/GameScene.hpp"
#include "Game/AudioLib/AudSystemWrapper.hpp"
#include "Game/Camera/CameraDirector.hpp"
#include "Game/Gravity/PlanetGravityManager.hpp"
#include "Game/Gravity/PointGravity.hpp"
#include "Game/Player/MarioActor.hpp"
#include "Game/Scene/StageDataHolder.hpp"
#include "Game/System/GameDataFunction.hpp"
#include "Game/System/GameDataHolder.hpp"
#include "Game/System/GameSequenceTitle.hpp"
#include "Game/System/GameSystemObjHolder.hpp"
#include "Game/Util/SystemUtil.hpp"
#include <nebula_game_api.h>
#include <nebula_module.h>

namespace MR {
    GameSystemObjHolder* getGameSystemObjHolder();
}  // namespace MR

extern "C" const NebulaHostServices* nebula_host(void);

namespace Smg2Render {
    void setVisualScreenState(int state, float alpha, s32 fileSlot, const char* stageName);
    void setCameraAndPlayerPreview(float camYaw, float playerX, float playerZ);
}  // namespace Smg2Render

namespace {
    GameSequenceTitle sTitleSeq;
    CameraDirector sCameraDirector;
    MarioActor sMarioActor;
    PlanetGravityManager* sStageGravity = nullptr;
    PointGravity sStagePointGrav;
    u32 sPrevButtons = 0u;
}  // namespace

GameScene::GameScene()
    : Scene("GameScene"),
      mPhase(Phase_Title),
      mSelectedFileSlot(1),
      mPhaseSteps(0),
      mStageDataHolder(new StageDataHolder()),
      mActiveStage("Title") {
}

GameScene::~GameScene() {
    delete mStageDataHolder;
    mStageDataHolder = nullptr;
    delete sStageGravity;
    sStageGravity = nullptr;
}

void GameScene::init() {
    mPhase = Phase_Title;
    mSelectedFileSlot = 1;
    mPhaseSteps = 0;
    mActiveStage = "Title";
    sTitleSeq.init();
    sPrevButtons = 0u;
    if (GameSystemObjHolder* holder = MR::getGameSystemObjHolder(); holder != nullptr && holder->mAudioSystem != nullptr) {
        holder->mAudioSystem->startStageBgm("Title");
    }
    Smg2Render::setVisualScreenState(1, 1.0f, mSelectedFileSlot, mActiveStage);
    nebula_log("[SMG2-SCENE] GameScene::init() -> Title Screen (press A / Enter / Space to enter File Select)");
}

void GameScene::enterFileSelect() {
    mPhase = Phase_FileSelect;
    mPhaseSteps = 0;
    mActiveStage = "FileSelect";
    if (mStageDataHolder != nullptr) {
        mStageDataHolder->mountStage("FileSelect");
    }
    if (GameSystemObjHolder* holder = MR::getGameSystemObjHolder(); holder != nullptr && holder->mAudioSystem != nullptr) {
        holder->mAudioSystem->startStageBgm("FileSelect");
    }
    Smg2Render::setVisualScreenState(2, 1.0f, mSelectedFileSlot, mActiveStage);
    nebula_log("[SMG2-SCENE] GameScene -> File Select (slot=%d, user='%s', stars=%d)",
               mSelectedFileSlot, GameDataFunction::getUserName(), GameDataFunction::calcCurrentPowerStarNum());
}

void GameScene::enterStage(const char* pStageName) {
    mPhase = Phase_InStage;
    mPhaseSteps = 0;
    mActiveStage = pStageName != nullptr ? pStageName : "IslandFleetGalaxy";
    if (mStageDataHolder != nullptr) {
        mStageDataHolder->mountStage(mActiveStage);
    }
    if (sStageGravity == nullptr) {
        sStageGravity = new PlanetGravityManager("StageGravity");
        sStagePointGrav.setOrig(TVec3f(0.0f, 0.0f, 0.0f));
        sStagePointGrav.mRange = 5000.0f;
        sStageGravity->registerGravity(&sStagePointGrav);
    }
    sCameraDirector.init();
    sCameraDirector.setStageTarget(mActiveStage);
    sMarioActor.init(mActiveStage);
    if (GameSystemObjHolder* holder = MR::getGameSystemObjHolder(); holder != nullptr && holder->mAudioSystem != nullptr) {
        holder->mAudioSystem->startStageBgm(mActiveStage);
    }
    Smg2Render::setVisualScreenState(3, 1.0f, mSelectedFileSlot, mActiveStage);
    nebula_log("[SMG2-SCENE] GameScene -> InStage '%s'", mActiveStage);
}

void GameScene::update() {
    mPhaseSteps++;

    uint32_t buttons = 0u;
    f32 stickX = 0.0f;
    f32 stickY = 0.0f;
    const NebulaHostServices* host = nebula_host();
    if (host != nullptr && host->input_read != nullptr) {
        NebulaInputSample sample{};
        host->input_read(host->user, 0u, nullptr, &sample);
        buttons = sample.buttons;
        stickX = sample.stick[0];
        stickY = sample.stick[1];
    }
    const uint32_t pressed = buttons & ~sPrevButtons;
    sPrevButtons = buttons;

    if (mPhase == Phase_Title) {
        sTitleSeq.update();
        if ((pressed & (NEBULA_BUTTON_A | NEBULA_BUTTON_PLUS)) != 0u) {
            enterFileSelect();
        }
    } else if (mPhase == Phase_FileSelect) {
        if ((pressed & NEBULA_BUTTON_UP) != 0u || (pressed & NEBULA_BUTTON_LEFT) != 0u) {
            mSelectedFileSlot = mSelectedFileSlot > 1 ? mSelectedFileSlot - 1 : 3;
            Smg2Render::setVisualScreenState(2, 1.0f, mSelectedFileSlot, mActiveStage);
        } else if ((pressed & NEBULA_BUTTON_DOWN) != 0u || (pressed & NEBULA_BUTTON_RIGHT) != 0u) {
            mSelectedFileSlot = mSelectedFileSlot < 3 ? mSelectedFileSlot + 1 : 1;
            Smg2Render::setVisualScreenState(2, 1.0f, mSelectedFileSlot, mActiveStage);
        } else if ((pressed & (NEBULA_BUTTON_A | NEBULA_BUTTON_PLUS)) != 0u) {
            sTitleSeq.setSelectedSlot(mSelectedFileSlot);
            sTitleSeq.decide();
            enterStage("IslandFleetGalaxy");
        } else if ((pressed & NEBULA_BUTTON_B) != 0u) {
            mPhase = Phase_Title;
            mPhaseSteps = 0;
            mActiveStage = "Title";
            Smg2Render::setVisualScreenState(1, 1.0f, mSelectedFileSlot, mActiveStage);
        }
    } else {
        sCameraDirector.update(stickX, stickY);
        sMarioActor.update(stickX, stickY, buttons, sStageGravity);
        Smg2Render::setCameraAndPlayerPreview(sCameraDirector.getYaw(), sMarioActor.getPosition().x, sMarioActor.getPosition().z);
    }
}

void GameScene::calcAnim() {
}

void GameScene::draw() const {
    const int state = (mPhase == Phase_Title) ? 1 : (mPhase == Phase_FileSelect ? 2 : 3);
    Smg2Render::setVisualScreenState(state, 1.0f, mSelectedFileSlot, mActiveStage);
}
