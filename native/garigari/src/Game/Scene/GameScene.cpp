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

class LayoutManager;
namespace Smg2Ui {
    LayoutManager* createLayout(const char* pName, bool usePrefix, u32 rootAnimLayers, u32 textBoxLen, bool keepText);
    void movementLayout(LayoutManager* pMgr);
    void calcAnimLayout(LayoutManager* pMgr);
    void drawLayout(const LayoutManager* pMgr);
    void startRootAnim(LayoutManager* pMgr, const char* pAnimName, u32 layer);
    bool startRootAnimFallback(LayoutManager* pMgr, const char* pFirst, const char* pSecond, u32 layer);
    bool isRootAnimStopped(const LayoutManager* pMgr, u32 layer);
    void setTextBoxMessage(LayoutManager* pMgr, const char* pPaneName, const char16_t* pMsg);
    void setTextBoxNumber(LayoutManager* pMgr, const char* pPaneName, s32 number);
    void setPaneVisible(LayoutManager* pMgr, const char* pPaneName, bool visible);
}  // namespace Smg2Ui

namespace {
    GameSequenceTitle sTitleSeq;
    CameraDirector sCameraDirector;
    MarioActor sMarioActor;
    PlanetGravityManager* sStageGravity = nullptr;
    PointGravity sStagePointGrav;
    u32 sPrevButtons = 0u;
    LayoutManager* sTitleLogoLayout = nullptr;
    LayoutManager* sPressStartLayout = nullptr;
    LayoutManager* sFileSelectLayout = nullptr;
    bool sTitleWaitStarted = false;
    bool sPressStartWaitStarted = false;
    bool sFileSelectWaitStarted = false;

    void populateFileSelectPanes(LayoutManager* pLayout, s32 slot) {
        if (pLayout == nullptr) return;
        const char* userName = GameDataFunction::getUserName();
        char16_t userW[32]{};
        if (userName != nullptr) {
            for (u32 i = 0; userName[i] != '\0' && i < 31u; ++i) {
                userW[i] = static_cast<char16_t>(static_cast<u8>(userName[i]));
            }
        }
        const s32 stars = GameDataFunction::calcCurrentPowerStarNum();
        const s32 misses = GameDataFunction::getPlayerGameOverNum();
        Smg2Ui::setTextBoxNumber(pLayout, "FileNumber", slot);
        Smg2Ui::setTextBoxMessage(pLayout, "FileName", userW);
        Smg2Ui::setTextBoxNumber(pLayout, "Star", stars);
        Smg2Ui::setTextBoxNumber(pLayout, "StarPiece", 0);
        Smg2Ui::setTextBoxNumber(pLayout, "MissCounter", misses);
        Smg2Ui::setPaneVisible(pLayout, "TxtMario", true);
        Smg2Ui::setPaneVisible(pLayout, "TxtLuigi", false);
    }
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
    if (sTitleLogoLayout == nullptr) {
        sTitleLogoLayout = Smg2Ui::createLayout("TitleLogo", true, 2u, 0u, true);
    }
    if (sPressStartLayout == nullptr) {
        sPressStartLayout = Smg2Ui::createLayout("PressStart", true, 1u, 0u, true);
    }
    sTitleWaitStarted = false;
    sPressStartWaitStarted = false;
    Smg2Ui::startRootAnim(sTitleLogoLayout, "Appear", 0u);
    Smg2Ui::startRootAnim(sPressStartLayout, "Appear", 0u);
    nebula_log("[SMG2-SCENE] GameScene::init() -> Title Screen (real NLYT/NLAN TitleLogo + PressStart)");
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
    if (sFileSelectLayout == nullptr) {
        sFileSelectLayout = Smg2Ui::createLayout("FileSelect", true, 2u, 0u, true);
    }
    sFileSelectWaitStarted = false;
    Smg2Ui::startRootAnimFallback(sFileSelectLayout, "Appear", "Wait", 0u);
    populateFileSelectPanes(sFileSelectLayout, mSelectedFileSlot);
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
        if (Smg2Ui::isRootAnimStopped(sTitleLogoLayout, 0u)) {
            sTitleWaitStarted = true;
            Smg2Ui::startRootAnimFallback(sTitleLogoLayout, "Wait", "Loop", 0u);
        }
        if (Smg2Ui::isRootAnimStopped(sPressStartLayout, 0u)) {
            sPressStartWaitStarted = true;
            Smg2Ui::startRootAnimFallback(sPressStartLayout, "Wait", "Loop", 0u);
        }
        Smg2Ui::movementLayout(sTitleLogoLayout);
        Smg2Ui::movementLayout(sPressStartLayout);
        if ((pressed & (NEBULA_BUTTON_A | NEBULA_BUTTON_PLUS)) != 0u) {
            Smg2Ui::startRootAnim(sTitleLogoLayout, "Decide", 0u);
            Smg2Ui::startRootAnim(sPressStartLayout, "End", 0u);
            enterFileSelect();
        }
    } else if (mPhase == Phase_FileSelect) {
        if (Smg2Ui::isRootAnimStopped(sFileSelectLayout, 0u)) {
            sFileSelectWaitStarted = true;
            Smg2Ui::startRootAnimFallback(sFileSelectLayout, "Wait", "Loop", 0u);
        }
        Smg2Ui::movementLayout(sFileSelectLayout);
        if ((pressed & NEBULA_BUTTON_UP) != 0u || (pressed & NEBULA_BUTTON_LEFT) != 0u) {
            mSelectedFileSlot = mSelectedFileSlot > 1 ? mSelectedFileSlot - 1 : 3;
            populateFileSelectPanes(sFileSelectLayout, mSelectedFileSlot);
        } else if ((pressed & NEBULA_BUTTON_DOWN) != 0u || (pressed & NEBULA_BUTTON_RIGHT) != 0u) {
            mSelectedFileSlot = mSelectedFileSlot < 3 ? mSelectedFileSlot + 1 : 1;
            populateFileSelectPanes(sFileSelectLayout, mSelectedFileSlot);
        } else if ((pressed & (NEBULA_BUTTON_A | NEBULA_BUTTON_PLUS)) != 0u) {
            sTitleSeq.setSelectedSlot(mSelectedFileSlot);
            sTitleSeq.decide();
            enterStage("IslandFleetGalaxy");
        } else if ((pressed & NEBULA_BUTTON_B) != 0u) {
            mPhase = Phase_Title;
            mPhaseSteps = 0;
            mActiveStage = "Title";
            sTitleWaitStarted = false;
            sPressStartWaitStarted = false;
            Smg2Ui::startRootAnim(sTitleLogoLayout, "Appear", 0u);
            Smg2Ui::startRootAnim(sPressStartLayout, "Appear", 0u);
        }
    } else {
        sCameraDirector.update(stickX, stickY);
        sMarioActor.update(stickX, stickY, buttons, sStageGravity);
    }
}

void GameScene::calcAnim() {
    if (mPhase == Phase_Title) {
        Smg2Ui::calcAnimLayout(sTitleLogoLayout);
        Smg2Ui::calcAnimLayout(sPressStartLayout);
    } else if (mPhase == Phase_FileSelect) {
        Smg2Ui::calcAnimLayout(sFileSelectLayout);
    }
}

void GameScene::draw() const {
    if (mPhase == Phase_Title) {
        Smg2Ui::drawLayout(sTitleLogoLayout);
        Smg2Ui::drawLayout(sPressStartLayout);
    } else if (mPhase == Phase_FileSelect) {
        Smg2Ui::drawLayout(sFileSelectLayout);
    }
}
