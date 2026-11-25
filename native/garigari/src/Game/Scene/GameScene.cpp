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
    void startPaneAnim(LayoutManager* pMgr, const char* pPaneName, const char* pAnimName, u32 layer);
    bool isPointingPane(const LayoutManager* pMgr, const char* pPaneName, f32 screenX, f32 screenY);
}  // namespace Smg2Ui

namespace Smg23D {
    void mountMenuBackdrop();
    void mountStageModels(const char* pStageName);
    void setMarioAnim(int animState, f32 speedScale);
    void begin3DScene(f32 camX, f32 camY, f32 camZ, f32 targetX, f32 targetY, f32 targetZ, f32 fovY, f32 nearZ, f32 farZ);
    void drawMenuBackdrop(u32 step);
    void drawStageAndMario(f32 marioX, f32 marioY, f32 marioZ, f32 upX, f32 upY, f32 upZ, f32 marioYawRad, u32 step);
    void checkCollectiblePickups(f32 marioX, f32 marioY, f32 marioZ, s32* outCoins, s32* outStarPieces);
    void end3DScene();
}  // namespace Smg23D

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
    LayoutManager* sHitPointMeterLayout = nullptr;
    LayoutManager* sCoinCounterLayout = nullptr;
    LayoutManager* sStarCounterLayout = nullptr;
    LayoutManager* sStarPieceCounterLayout = nullptr;
    bool sTitleWaitStarted = false;
    bool sPressStartWaitStarted = false;
    bool sFileSelectWaitStarted = false;
    int sFileFocus = 0;       // 0..2 = File 1..3, 3 = Icon (MiiButton), 4 = Erase (DeleteButton)
    int sPrevFileFocus = -1;
    bool sConfirmOpen = false;
    bool sEraseConfirm = false;
    bool sUseLuigiIcon = false;
    s32 sStageCoins = 0;
    s32 sStageStarPieces = 0;
    s32 sStageLife = 3;

    const char* focusPaneName(int focus) {
        switch (focus) {
        case 3: return "MiiButton";
        case 4: return "DeleteButton";
        default: return "StartButton";
        }
    }

    void updateFileFocusAnims(LayoutManager* pLayout, int newFocus) {
        if (pLayout == nullptr || newFocus == sPrevFileFocus) return;
        if (sPrevFileFocus >= 0) {
            Smg2Ui::startPaneAnim(pLayout, focusPaneName(sPrevFileFocus), "ButtonSelectOut", 0u);
        }
        Smg2Ui::startPaneAnim(pLayout, focusPaneName(newFocus), "ButtonSelectIn", 0u);
        sPrevFileFocus = newFocus;
    }

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
        Smg2Ui::setPaneVisible(pLayout, "TxtMario", !sUseLuigiIcon);
        Smg2Ui::setPaneVisible(pLayout, "TxtLuigi", sUseLuigiIcon);
    }

    void updateStageHudCounters() {
        Smg2Ui::setTextBoxNumber(sCoinCounterLayout, "Counter", sStageCoins);
        Smg2Ui::setTextBoxNumber(sCoinCounterLayout, "TxtCoin", sStageCoins);
        Smg2Ui::setTextBoxNumber(sStarCounterLayout, "Counter", GameDataFunction::calcCurrentPowerStarNum());
        Smg2Ui::setTextBoxNumber(sStarCounterLayout, "TxtStar", GameDataFunction::calcCurrentPowerStarNum());
        Smg2Ui::setTextBoxNumber(sStarPieceCounterLayout, "Counter", sStageStarPieces);
        Smg2Ui::setTextBoxNumber(sStarPieceCounterLayout, "TxtStarPiece", sStageStarPieces);
        Smg2Ui::setTextBoxNumber(sHitPointMeterLayout, "Counter", sStageLife);
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
    Smg23D::mountMenuBackdrop();
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
    sFileFocus = (mSelectedFileSlot >= 1 && mSelectedFileSlot <= 3) ? (mSelectedFileSlot - 1) : 0;
    sPrevFileFocus = -1;
    sConfirmOpen = false;
    sEraseConfirm = false;
    Smg2Ui::startRootAnimFallback(sFileSelectLayout, "Appear", "Wait", 0u);
    populateFileSelectPanes(sFileSelectLayout, mSelectedFileSlot);
    updateFileFocusAnims(sFileSelectLayout, sFileFocus);
    nebula_log("[SMG2-SCENE] GameScene -> File Select (slot=%d, user='%s', stars=%d)",
               mSelectedFileSlot, GameDataFunction::getUserName(), GameDataFunction::calcCurrentPowerStarNum());
}

void GameScene::enterStage(const char* pStageName) {
    mPhase = Phase_InStage;
    mPhaseSteps = 0;
    const char* defaultStage = GameDataFunction::calcCurrentPowerStarNum() > 0 ? "MarioFaceShipGalaxy" : "IslandFleetGalaxy";
    mActiveStage = pStageName != nullptr ? pStageName : defaultStage;
    if (mStageDataHolder != nullptr) {
        mStageDataHolder->mountStage(mActiveStage);
    }
    if (sStageGravity == nullptr) {
        sStageGravity = new PlanetGravityManager("StageGravity");
        sStagePointGrav.setOrig(TVec3f(0.0f, 0.0f, 0.0f));
        sStagePointGrav.mRange = 5000.0f;
        sStageGravity->registerGravity(&sStagePointGrav);
    }
    Smg23D::mountStageModels(mActiveStage);
    sCameraDirector.init();
    sCameraDirector.setStageTarget(mActiveStage);
    sMarioActor.init(mActiveStage);
    sStageCoins = 0;
    sStageStarPieces = 0;
    sStageLife = 3;
    if (sHitPointMeterLayout == nullptr) sHitPointMeterLayout = Smg2Ui::createLayout("HitPointMeter", true, 1u, 0u, true);
    if (sCoinCounterLayout == nullptr) sCoinCounterLayout = Smg2Ui::createLayout("CoinCounter", true, 1u, 0u, true);
    if (sStarCounterLayout == nullptr) sStarCounterLayout = Smg2Ui::createLayout("StarCounter", true, 1u, 0u, true);
    if (sStarPieceCounterLayout == nullptr) sStarPieceCounterLayout = Smg2Ui::createLayout("StarPieceCounter", true, 1u, 0u, true);
    Smg2Ui::startRootAnimFallback(sHitPointMeterLayout, "Appear", "Wait", 0u);
    Smg2Ui::startRootAnimFallback(sCoinCounterLayout, "Appear", "Wait", 0u);
    Smg2Ui::startRootAnimFallback(sStarCounterLayout, "Appear", "Wait", 0u);
    Smg2Ui::startRootAnimFallback(sStarPieceCounterLayout, "Appear", "Wait", 0u);
    updateStageHudCounters();
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
    f32 ptrX = 0.0f;
    f32 ptrY = 0.0f;
    bool hasPtr = false;
    const NebulaHostServices* host = nebula_host();
    if (host != nullptr && host->input_read != nullptr) {
        NebulaInputSample sample{};
        host->input_read(host->user, 0u, nullptr, &sample);
        buttons = sample.buttons;
        stickX = sample.stick[0];
        stickY = sample.stick[1];
        if (sample.pointer_on != 0u) {
            ptrX = sample.pointer[0] * 416.0f;
            ptrY = sample.pointer[1] * 228.0f;
            hasPtr = true;
        }
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
        if (hasPtr && !sConfirmOpen && !sEraseConfirm) {
            if (Smg2Ui::isPointingPane(sFileSelectLayout, "PicMii", ptrX, ptrY) ||
                Smg2Ui::isPointingPane(sFileSelectLayout, "MiiButton", ptrX, ptrY)) {
                sFileFocus = 3;
            } else if (Smg2Ui::isPointingPane(sFileSelectLayout, "PicDelete", ptrX, ptrY) ||
                       Smg2Ui::isPointingPane(sFileSelectLayout, "DeleteButton", ptrX, ptrY)) {
                sFileFocus = 4;
            } else if (Smg2Ui::isPointingPane(sFileSelectLayout, "BoxStartButton", ptrX, ptrY) ||
                       Smg2Ui::isPointingPane(sFileSelectLayout, "StartButton", ptrX, ptrY)) {
                sFileFocus = mSelectedFileSlot - 1;
            }
        }
        if (!sConfirmOpen && !sEraseConfirm) {
            if ((pressed & NEBULA_BUTTON_UP) != 0u) {
                sFileFocus = (sFileFocus <= 2) ? 3 : (mSelectedFileSlot - 1);
            } else if ((pressed & NEBULA_BUTTON_DOWN) != 0u) {
                sFileFocus = (sFileFocus >= 3) ? (mSelectedFileSlot - 1) : 4;
            } else if ((pressed & NEBULA_BUTTON_LEFT) != 0u) {
                sFileFocus = (sFileFocus == 4) ? 3 : (sFileFocus == 3 ? 4 : (sFileFocus + 2) % 3);
            } else if ((pressed & NEBULA_BUTTON_RIGHT) != 0u) {
                sFileFocus = (sFileFocus == 3) ? 4 : (sFileFocus == 4 ? 3 : (sFileFocus + 1) % 3);
            }
            if (sFileFocus >= 0 && sFileFocus <= 2 && mSelectedFileSlot != sFileFocus + 1) {
                mSelectedFileSlot = sFileFocus + 1;
                populateFileSelectPanes(sFileSelectLayout, mSelectedFileSlot);
            }
            updateFileFocusAnims(sFileSelectLayout, sFileFocus);
        }
        Smg2Ui::movementLayout(sFileSelectLayout);
        if ((pressed & (NEBULA_BUTTON_A | NEBULA_BUTTON_PLUS)) != 0u) {
            Smg2Ui::startPaneAnim(sFileSelectLayout, focusPaneName(sFileFocus), "ButtonReaction", 0u);
            if (!sConfirmOpen && !sEraseConfirm && sFileFocus <= 2) {
                sConfirmOpen = true;
                Smg2Ui::setTextBoxMessage(sFileSelectLayout, "StartButton", u"Start this file?");
                nebula_log("[SMG2-UI] FileSelect slot %d: Start this file? (A=Confirm, B=Cancel)", mSelectedFileSlot);
            } else if (!sConfirmOpen && !sEraseConfirm && sFileFocus == 3) {
                sUseLuigiIcon = !sUseLuigiIcon;
                populateFileSelectPanes(sFileSelectLayout, mSelectedFileSlot);
                nebula_log("[SMG2-UI] FileSelect Icon toggled -> %s", sUseLuigiIcon ? "Luigi" : "Mario");
            } else if (!sConfirmOpen && !sEraseConfirm && sFileFocus == 4) {
                sEraseConfirm = true;
                Smg2Ui::setTextBoxMessage(sFileSelectLayout, "DeleteButton", u"Erase File?");
                nebula_log("[SMG2-UI] FileSelect Erase prompt for slot %d (A=Confirm, B=Cancel)", mSelectedFileSlot);
            } else if (sEraseConfirm) {
                sEraseConfirm = false;
                Smg2Ui::setTextBoxMessage(sFileSelectLayout, "DeleteButton", u"Erase");
                Smg2Ui::setTextBoxNumber(sFileSelectLayout, "Star", 0);
                Smg2Ui::setTextBoxNumber(sFileSelectLayout, "StarPiece", 0);
                Smg2Ui::setTextBoxNumber(sFileSelectLayout, "MissCounter", 0);
                nebula_log("[SMG2-UI] FileSelect slot %d erased", mSelectedFileSlot);
            } else if (sConfirmOpen) {
                Smg2Ui::startPaneAnim(sFileSelectLayout, "StartButton", "ButtonDecide", 0u);
                sTitleSeq.setSelectedSlot(mSelectedFileSlot);
                sTitleSeq.decide();
                enterStage(nullptr);
            }
        } else if ((pressed & NEBULA_BUTTON_B) != 0u) {
            if (sConfirmOpen) {
                sConfirmOpen = false;
                Smg2Ui::setTextBoxMessage(sFileSelectLayout, "StartButton", u"Start");
            } else if (sEraseConfirm) {
                sEraseConfirm = false;
                Smg2Ui::setTextBoxMessage(sFileSelectLayout, "DeleteButton", u"Erase");
            } else {
                mPhase = Phase_Title;
                mPhaseSteps = 0;
                mActiveStage = "Title";
                sTitleWaitStarted = false;
                sPressStartWaitStarted = false;
                Smg23D::mountMenuBackdrop();
                Smg2Ui::startRootAnim(sTitleLogoLayout, "Appear", 0u);
                Smg2Ui::startRootAnim(sPressStartLayout, "Appear", 0u);
            }
        }
    } else {
        sMarioActor.update(stickX, stickY, buttons, sStageGravity);
        sCameraDirector.update(sMarioActor.getPosition(), sMarioActor.getUpVec(), sMarioActor.getYaw(), buttons, pressed);
        const f32 stickMag = __builtin_sqrtf(stickX * stickX + stickY * stickY);
        Smg23D::setMarioAnim(sMarioActor.getAnimState(), stickMag);
        const s32 prevCoins = sStageCoins;
        const s32 prevPieces = sStageStarPieces;
        const TVec3f& pos = sMarioActor.getPosition();
        Smg23D::checkCollectiblePickups(pos.x, pos.y, pos.z, &sStageCoins, &sStageStarPieces);
        if (sStageCoins != prevCoins || sStageStarPieces != prevPieces) {
            updateStageHudCounters();
        }
        Smg2Ui::movementLayout(sHitPointMeterLayout);
        Smg2Ui::movementLayout(sCoinCounterLayout);
        Smg2Ui::movementLayout(sStarCounterLayout);
        Smg2Ui::movementLayout(sStarPieceCounterLayout);
    }
}

void GameScene::calcAnim() {
    if (mPhase == Phase_Title) {
        Smg2Ui::calcAnimLayout(sTitleLogoLayout);
        Smg2Ui::calcAnimLayout(sPressStartLayout);
    } else if (mPhase == Phase_FileSelect) {
        Smg2Ui::calcAnimLayout(sFileSelectLayout);
    } else {
        Smg2Ui::calcAnimLayout(sHitPointMeterLayout);
        Smg2Ui::calcAnimLayout(sCoinCounterLayout);
        Smg2Ui::calcAnimLayout(sStarCounterLayout);
        Smg2Ui::calcAnimLayout(sStarPieceCounterLayout);
    }
}

void GameScene::draw() const {
    if (mPhase == Phase_Title) {
        const f32 ang = static_cast<f32>(mPhaseSteps) * 0.003f;
        Smg23D::begin3DScene(9500.0f * __builtin_sinf(ang), 1200.0f, 9500.0f * __builtin_cosf(ang), 0.0f, -500.0f, 0.0f, 55.0f, 10.0f, 100000.0f);
        Smg23D::drawMenuBackdrop(static_cast<u32>(mPhaseSteps));
        Smg23D::end3DScene();
        Smg2Ui::drawLayout(sTitleLogoLayout);
        Smg2Ui::drawLayout(sPressStartLayout);
    } else if (mPhase == Phase_FileSelect) {
        const f32 ang = static_cast<f32>(mPhaseSteps) * 0.003f;
        Smg23D::begin3DScene(9500.0f * __builtin_sinf(ang), 1200.0f, 9500.0f * __builtin_cosf(ang), 0.0f, -500.0f, 0.0f, 55.0f, 10.0f, 100000.0f);
        Smg23D::drawMenuBackdrop(static_cast<u32>(mPhaseSteps));
        Smg23D::end3DScene();
        Smg2Ui::drawLayout(sFileSelectLayout);
    } else {
        const TVec3f& pos = sMarioActor.getPosition();
        const TVec3f& up = sMarioActor.getUpVec();
        const TVec3f& camPos = sCameraDirector.getPosition();
        const TVec3f& camTgt = sCameraDirector.getTarget();
        Smg23D::begin3DScene(camPos.x, camPos.y, camPos.z, camTgt.x, camTgt.y, camTgt.z, 60.0f, 10.0f, 100000.0f);
        Smg23D::drawStageAndMario(pos.x, pos.y, pos.z, up.x, up.y, up.z, sMarioActor.getYaw(), static_cast<u32>(mPhaseSteps));
        Smg23D::end3DScene();
        Smg2Ui::drawLayout(sHitPointMeterLayout);
        Smg2Ui::drawLayout(sCoinCounterLayout);
        Smg2Ui::drawLayout(sStarCounterLayout);
        Smg2Ui::drawLayout(sStarPieceCounterLayout);
    }
}
