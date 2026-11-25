#include "Game/Scene/GameScene.hpp"
#include "Game/AudioLib/AudSystemWrapper.hpp"
#include "Game/Camera/CameraDirector.hpp"
#include "Game/Gravity/PlanetGravityManager.hpp"
#include "Game/LiveActor/Nerve.hpp"
#include "Game/Player/MarioActor.hpp"
#include "Game/Scene/SceneObjHolder.hpp"
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
    void setPanePos(LayoutManager* pMgr, const char* pPaneName, f32 x, f32 y);
    void setPaneScale(LayoutManager* pMgr, const char* pPaneName, f32 sx, f32 sy);
    void setPaneAnimFrameAndStop(LayoutManager* pMgr, const char* pPaneName, const char* pAnimName, f32 frame, u32 layer);
    void clearPlaceholderTextBoxes(LayoutManager* pMgr);
    void startPaneAnim(LayoutManager* pMgr, const char* pPaneName, const char* pAnimName, u32 layer);
    bool isPointingPane(const LayoutManager* pMgr, const char* pPaneName, f32 screenX, f32 screenY);
}  // namespace Smg2Ui

namespace Smg23D {
    void mountMenuBackdrop();
    void mountStageModels(const char* pStageName);
    void setMarioAnim(int animState, f32 speedScale);
    void setCameraUp(f32 ux, f32 uy, f32 uz);
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
    u32 sPrevButtons = 0u;
    LayoutManager* sTitleLogoLayout = nullptr;
    LayoutManager* sPressStartLayout = nullptr;
    LayoutManager* sFileSelectLayout = nullptr;
    LayoutManager* sFileInfoLayout = nullptr;
    LayoutManager* sFileNumberLayouts[3]{};
    LayoutManager* sPointerLayout = nullptr;
    LayoutManager* sHitPointMeterLayout = nullptr;
    LayoutManager* sCoinCounterLayout = nullptr;
    LayoutManager* sStarCounterLayout = nullptr;
    LayoutManager* sStarPieceCounterLayout = nullptr;
    bool sHasPointer = false;
    f32 sPrevPtrX = 0.0f;
    f32 sPrevPtrY = 0.0f;
    bool sTitleWaitStarted = false;
    bool sPressStartWaitStarted = false;
    bool sFileSelectWaitStarted = false;
    bool sConfirmOpen = false;
    bool sEraseConfirm = false;
    bool sUseLuigiIcon = false;
    s32 sStageCoins = 0;
    s32 sStageStarPieces = 0;
    s32 sStageLife = 3;
}  // namespace

// NerveExecutor button controller ported from Petari's ButtonPaneController
class FileButtonCtrl : public NerveExecutor {
public:
    FileButtonCtrl(const char* pPaneName, const char* pBoundingPaneName);
    void appear(LayoutManager* pHost);
    void forceToWait();
    void setPointing(bool pointing) { mIsPointing = pointing; }
    bool isPointing() const;
    bool isDecided() const;
    bool isDecidedWait() const;
    const char* getPaneName() const { return mPaneName; }
    const char* getBoundingPaneName() const { return mBoundingPaneName; }
    bool trySelect(bool padTriggerDecide);

    void exeHidden() {}
    void exeAppear();
    void exeWait();
    void exePointing();
    void exeNotPointing();
    void exeDecided();
    void exeDecidedWait() {}

private:
    LayoutManager* mHost;
    const char* mPaneName;
    const char* mBoundingPaneName;
    bool mIsPointing;
    bool mIsSelected;
    bool mIsActive;
};

namespace NrvFileButtonCtrl {
    NEW_NERVE(FileButtonCtrlNrvHidden, FileButtonCtrl, Hidden);
    NEW_NERVE(FileButtonCtrlNrvAppear, FileButtonCtrl, Appear);
    NEW_NERVE(FileButtonCtrlNrvWait, FileButtonCtrl, Wait);
    NEW_NERVE(FileButtonCtrlNrvPointing, FileButtonCtrl, Pointing);
    NEW_NERVE(FileButtonCtrlNrvNotPointing, FileButtonCtrl, NotPointing);
    NEW_NERVE(FileButtonCtrlNrvDecided, FileButtonCtrl, Decided);
    NEW_NERVE(FileButtonCtrlNrvDecidedWait, FileButtonCtrl, DecidedWait);
}  // namespace NrvFileButtonCtrl

using namespace NrvFileButtonCtrl;

FileButtonCtrl::FileButtonCtrl(const char* pPaneName, const char* pBoundingPaneName)
    : NerveExecutor(pPaneName),
      mHost(nullptr),
      mPaneName(pPaneName),
      mBoundingPaneName(pBoundingPaneName),
      mIsPointing(false),
      mIsSelected(false),
      mIsActive(false) {
}

void FileButtonCtrl::appear(LayoutManager* pHost) {
    if (mSpine == nullptr) {
        initNerve(GET_NERVE_ANON(FileButtonCtrlNrvHidden));
    }
    mHost = pHost;
    mIsPointing = false;
    mIsSelected = false;
    mIsActive = true;
    setNerve(GET_NERVE_ANON(FileButtonCtrlNrvAppear));
}

void FileButtonCtrl::forceToWait() {
    mIsSelected = false;
    mIsPointing = false;
    setNerve(GET_NERVE_ANON(FileButtonCtrlNrvWait));
}

bool FileButtonCtrl::isPointing() const { return isNerve(GET_NERVE_ANON(FileButtonCtrlNrvPointing)); }
bool FileButtonCtrl::isDecided() const { return isNerve(GET_NERVE_ANON(FileButtonCtrlNrvDecided)); }
bool FileButtonCtrl::isDecidedWait() const { return isNerve(GET_NERVE_ANON(FileButtonCtrlNrvDecidedWait)); }

bool FileButtonCtrl::trySelect(bool padTriggerDecide) {
    if (!mIsActive) return false;
    if (isPointing() && padTriggerDecide) {
        mIsSelected = true;
        setNerve(GET_NERVE_ANON(FileButtonCtrlNrvDecided));
        return true;
    }
    return false;
}

void FileButtonCtrl::exeAppear() {
    if (getNerveStep() == 0) {
        Smg2Ui::setPaneVisible(mHost, mPaneName, true);
        Smg2Ui::startPaneAnim(mHost, mPaneName, "ButtonAppear", 0u);
        Smg2Ui::setPaneScale(mHost, mPaneName, 1.0f, 1.0f);
    }
    if (getNerveStep() >= 10) {
        setNerve(GET_NERVE_ANON(FileButtonCtrlNrvWait));
    }
}

void FileButtonCtrl::exeWait() {
    if (getNerveStep() == 0) {
        Smg2Ui::startPaneAnim(mHost, mPaneName, "ButtonWait", 0u);
        Smg2Ui::setPaneScale(mHost, mPaneName, 1.0f, 1.0f);
    }
    if (mIsPointing) {
        setNerve(GET_NERVE_ANON(FileButtonCtrlNrvPointing));
    }
}

void FileButtonCtrl::exePointing() {
    if (getNerveStep() == 0) {
        Smg2Ui::startPaneAnim(mHost, mPaneName, "ButtonSelectIn", 0u);
        Smg2Ui::setPaneScale(mHost, mPaneName, 1.12f, 1.12f);
    }
    if (!mIsPointing) {
        setNerve(GET_NERVE_ANON(FileButtonCtrlNrvNotPointing));
    }
}

void FileButtonCtrl::exeNotPointing() {
    if (getNerveStep() == 0) {
        Smg2Ui::startPaneAnim(mHost, mPaneName, "ButtonSelectOut", 0u);
        Smg2Ui::setPaneScale(mHost, mPaneName, 1.0f, 1.0f);
    }
    if (mIsPointing) {
        setNerve(GET_NERVE_ANON(FileButtonCtrlNrvPointing));
    } else if (getNerveStep() >= 8) {
        setNerve(GET_NERVE_ANON(FileButtonCtrlNrvWait));
    }
}

void FileButtonCtrl::exeDecided() {
    if (getNerveStep() == 0) {
        Smg2Ui::startPaneAnim(mHost, mPaneName, "ButtonDecide", 0u);
    }
    if (getNerveStep() >= 6) {
        setNerve(GET_NERVE_ANON(FileButtonCtrlNrvDecidedWait));
    }
}

namespace {
    FileButtonCtrl sStartBtn("StartButton", "BoxStartButton");
    FileButtonCtrl sMiiBtn("MiiButton", "PicMii");
    FileButtonCtrl sDeleteBtn("DeleteButton", "PicDelete");
    FileButtonCtrl* sFileButtons[3] = { &sStartBtn, &sMiiBtn, &sDeleteBtn };
    FileButtonCtrl* sFocusedBtn = &sStartBtn;

    void updateFileNumberCards(int activeSlot) {
        for (int i = 0; i < 3; ++i) {
            if (sFileNumberLayouts[i] == nullptr) continue;
            const bool selected = (i == activeSlot - 1);
            Smg2Ui::setPaneScale(sFileNumberLayouts[i], nullptr, selected ? 1.15f : 1.0f, selected ? 1.15f : 1.0f);
            Smg2Ui::startRootAnim(sFileNumberLayouts[i], selected ? "SelectIn" : "SelectOut", 1u);
        }
    }

    void populateFileSelectPanes(LayoutManager* pLayout, s32 slot) {
        const char* userName = GameDataFunction::getUserName();
        char16_t userW[32]{};
        if (userName != nullptr) {
            for (u32 i = 0; userName[i] != '\0' && i < 31u; ++i) {
                userW[i] = static_cast<char16_t>(static_cast<u8>(userName[i]));
            }
        }
        const s32 stars = GameDataFunction::calcCurrentPowerStarNum();
        const s32 misses = GameDataFunction::getPlayerGameOverNum();
        LayoutManager* targets[2] = { pLayout, sFileInfoLayout };
        for (LayoutManager* tgt : targets) {
            if (tgt == nullptr) continue;
            Smg2Ui::setTextBoxNumber(tgt, "FileNumber", slot);
            Smg2Ui::setTextBoxMessage(tgt, "FileName", userW);
            Smg2Ui::setTextBoxNumber(tgt, "Star", stars);
            Smg2Ui::setTextBoxNumber(tgt, "StarPiece", 0);
            Smg2Ui::setTextBoxNumber(tgt, "MissCounter", misses);
            Smg2Ui::setTextBoxMessage(tgt, "TxtDay", u"");
            Smg2Ui::setTextBoxMessage(tgt, "TxtTime", u"");
            Smg2Ui::setPaneVisible(tgt, "BrosIcon", true);
            Smg2Ui::setPaneVisible(tgt, "TxtMario", !sUseLuigiIcon);
            Smg2Ui::setPaneVisible(tgt, "TxtLuigi", sUseLuigiIcon);
            Smg2Ui::setPaneVisible(tgt, "Complete1", false);
            Smg2Ui::setPaneVisible(tgt, "Complete2", false);
            Smg2Ui::clearPlaceholderTextBoxes(tgt);
        }
    }

    void updateStageHudCounters() {
        Smg2Ui::setTextBoxNumber(sCoinCounterLayout, "Counter", sStageCoins);
        Smg2Ui::setTextBoxNumber(sCoinCounterLayout, "TxtCoin", sStageCoins);
        Smg2Ui::setTextBoxNumber(sStarCounterLayout, "Counter", GameDataFunction::calcCurrentPowerStarNum());
        Smg2Ui::setTextBoxNumber(sStarCounterLayout, "TxtStar", GameDataFunction::calcCurrentPowerStarNum());
        Smg2Ui::setTextBoxNumber(sStarPieceCounterLayout, "Counter", sStageStarPieces);
        Smg2Ui::setTextBoxNumber(sStarPieceCounterLayout, "TxtStarPiece", sStageStarPieces);
        Smg2Ui::setTextBoxNumber(sHitPointMeterLayout, "HitPointNumber", sStageLife);
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
    if (sFileInfoLayout == nullptr) {
        sFileInfoLayout = Smg2Ui::createLayout("FileInfo", true, 3u, 0u, true);
    }
    for (int i = 0; i < 3; ++i) {
        if (sFileNumberLayouts[i] == nullptr) {
            sFileNumberLayouts[i] = Smg2Ui::createLayout("FileNumber", true, 2u, 0u, true);
        }
        Smg2Ui::setTextBoxNumber(sFileNumberLayouts[i], "FileNumber", i + 1);
        Smg2Ui::setPanePos(sFileNumberLayouts[i], nullptr, static_cast<f32>(i * 150 - 240), -135.0f);
        Smg2Ui::startRootAnimFallback(sFileNumberLayouts[i], "Appear", "Wait", 0u);
    }
    static const char* const kUnusedInfoPanes[] = {
        "Complete1", "Complete2", "PicPeach", "PicRosetta", "PicKinopio", "PicYoshi", "PicLuigi", "PicMii"
    };
    for (const char* pName : kUnusedInfoPanes) {
        Smg2Ui::setPaneVisible(sFileInfoLayout, pName, false);
    }
    if (sPointerLayout == nullptr) {
        sPointerLayout = Smg2Ui::createLayout("DPDPointer", true, 1u, 0u, true);
        Smg2Ui::setPaneVisible(sPointerLayout, "HandPointer", false);
        Smg2Ui::setPaneVisible(sPointerLayout, "HandArrow", false);
        Smg2Ui::setPaneVisible(sPointerLayout, "PicNozzle", false);
        Smg2Ui::setPaneVisible(sPointerLayout, "StarPointer", true);
    }
    Smg2Ui::setPaneVisible(sFileSelectLayout, "P2ManualButton", false);
    sFileSelectWaitStarted = false;
    sConfirmOpen = false;
    sEraseConfirm = false;
    sFocusedBtn = &sStartBtn;
    for (FileButtonCtrl* btn : sFileButtons) {
        btn->appear(sFileSelectLayout);
    }
    Smg2Ui::startRootAnimFallback(sFileSelectLayout, "Appear", "Wait", 0u);
    Smg2Ui::startRootAnimFallback(sFileInfoLayout, "Appear", "Wait", 0u);
    populateFileSelectPanes(sFileSelectLayout, mSelectedFileSlot);
    updateFileNumberCards(mSelectedFileSlot);
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
        mStageDataHolder->loadStageGravities(mActiveStage);
    }
    sStageGravity = MR::getSceneObj<PlanetGravityManager>(SceneObj_PlanetGravityManager);
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
    Smg2Ui::setPaneAnimFrameAndStop(sHitPointMeterLayout, nullptr, "ShowHide", 20.0f, 1u);
    Smg2Ui::setPaneAnimFrameAndStop(sHitPointMeterLayout, "HitPointMeter", "Count", static_cast<f32>(sStageLife), 0u);
    Smg2Ui::setTextBoxNumber(sHitPointMeterLayout, "HitPointNumber", sStageLife);
    Smg2Ui::startRootAnimFallback(sCoinCounterLayout, "Appear", "Wait", 0u);
    Smg2Ui::startRootAnimFallback(sStarCounterLayout, "Appear", "Wait", 0u);
    Smg2Ui::startRootAnimFallback(sStarPieceCounterLayout, "Appear", "Wait", 0u);
    Smg2Ui::setPaneVisible(sStarPieceCounterLayout, "TSPieceCounter", false);
    Smg2Ui::setPaneVisible(sStarPieceCounterLayout, "StarPieceCounter", true);
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
    sHasPointer = hasPtr;
    if (sPointerLayout != nullptr) {
        Smg2Ui::setPaneVisible(sPointerLayout, "StarPointer", hasPtr);
        if (hasPtr) {
            Smg2Ui::setPanePos(sPointerLayout, nullptr, ptrX, ptrY);
            Smg2Ui::movementLayout(sPointerLayout);
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
        const bool ptrMoved = hasPtr && ((ptrX - sPrevPtrX) * (ptrX - sPrevPtrX) + (ptrY - sPrevPtrY) * (ptrY - sPrevPtrY) > 4.0f);
        if (ptrMoved) {
            sPrevPtrX = ptrX;
            sPrevPtrY = ptrY;
        }
        if (hasPtr && (ptrMoved || (pressed & NEBULA_BUTTON_A) != 0u) && !sConfirmOpen && !sEraseConfirm) {
            for (FileButtonCtrl* btn : sFileButtons) {
                if (Smg2Ui::isPointingPane(sFileSelectLayout, btn->getBoundingPaneName(), ptrX, ptrY) ||
                    Smg2Ui::isPointingPane(sFileSelectLayout, btn->getPaneName(), ptrX, ptrY)) {
                    sFocusedBtn = btn;
                    break;
                }
            }
        }
        if (!sConfirmOpen && !sEraseConfirm) {
            if ((pressed & NEBULA_BUTTON_UP) != 0u) {
                sFocusedBtn = (sFocusedBtn == &sStartBtn) ? &sMiiBtn : &sStartBtn;
            } else if ((pressed & NEBULA_BUTTON_DOWN) != 0u) {
                sFocusedBtn = (sFocusedBtn != &sStartBtn) ? &sStartBtn : &sDeleteBtn;
            } else if ((pressed & NEBULA_BUTTON_LEFT) != 0u) {
                if (sFocusedBtn == &sDeleteBtn) sFocusedBtn = &sMiiBtn;
                else if (sFocusedBtn == &sMiiBtn) sFocusedBtn = &sDeleteBtn;
                else mSelectedFileSlot = (mSelectedFileSlot > 1) ? (mSelectedFileSlot - 1) : 3;
            } else if ((pressed & NEBULA_BUTTON_RIGHT) != 0u) {
                if (sFocusedBtn == &sMiiBtn) sFocusedBtn = &sDeleteBtn;
                else if (sFocusedBtn == &sDeleteBtn) sFocusedBtn = &sMiiBtn;
                else mSelectedFileSlot = (mSelectedFileSlot < 3) ? (mSelectedFileSlot + 1) : 1;
            }
            populateFileSelectPanes(sFileSelectLayout, mSelectedFileSlot);
            updateFileNumberCards(mSelectedFileSlot);
        }
        for (FileButtonCtrl* btn : sFileButtons) {
            btn->setPointing(btn == sFocusedBtn);
            btn->updateNerve();
            if (btn->isDecidedWait()) {
                btn->forceToWait();
            }
        }
        Smg2Ui::movementLayout(sFileSelectLayout);
        Smg2Ui::movementLayout(sFileInfoLayout);
        for (int i = 0; i < 3; ++i) {
            Smg2Ui::movementLayout(sFileNumberLayouts[i]);
        }
        const bool decideTrigger = (pressed & (NEBULA_BUTTON_A | NEBULA_BUTTON_PLUS)) != 0u;
        if (decideTrigger) {
            if (!sConfirmOpen && !sEraseConfirm) {
                if (sStartBtn.trySelect(true)) {
                    sConfirmOpen = true;
                    Smg2Ui::setTextBoxMessage(sFileSelectLayout, "StartButton", u"Start this file?");
                    nebula_log("[SMG2-UI] FileSelect slot %d: Start this file? (A=Confirm, B=Cancel)", mSelectedFileSlot);
                } else if (sMiiBtn.trySelect(true)) {
                    sUseLuigiIcon = !sUseLuigiIcon;
                    populateFileSelectPanes(sFileSelectLayout, mSelectedFileSlot);
                    nebula_log("[SMG2-UI] FileSelect Icon toggled -> %s", sUseLuigiIcon ? "Luigi" : "Mario");
                } else if (sDeleteBtn.trySelect(true)) {
                    sEraseConfirm = true;
                    Smg2Ui::setTextBoxMessage(sFileSelectLayout, "DeleteButton", u"Erase File?");
                    nebula_log("[SMG2-UI] FileSelect Erase prompt for slot %d (A=Confirm, B=Cancel)", mSelectedFileSlot);
                }
            } else if (sEraseConfirm) {
                sEraseConfirm = false;
                Smg2Ui::setTextBoxMessage(sFileSelectLayout, "DeleteButton", u"Erase");
                Smg2Ui::setTextBoxNumber(sFileSelectLayout, "Star", 0);
                Smg2Ui::setTextBoxNumber(sFileSelectLayout, "StarPiece", 0);
                Smg2Ui::setTextBoxNumber(sFileSelectLayout, "MissCounter", 0);
                nebula_log("[SMG2-UI] FileSelect slot %d erased", mSelectedFileSlot);
            } else if (sConfirmOpen) {
                sStartBtn.trySelect(true);
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
        sMarioActor.update(stickX, stickY, buttons, sStageGravity,
                           sCameraDirector.getPosition(), sCameraDirector.getTarget(), sCameraDirector.getUp());
        sCameraDirector.update(sMarioActor.getPosition(), sMarioActor.getUpVec(), sMarioActor.getFrontVec(), buttons, pressed);
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
        Smg2Ui::calcAnimLayout(sFileInfoLayout);
        for (int i = 0; i < 3; ++i) {
            Smg2Ui::calcAnimLayout(sFileNumberLayouts[i]);
        }
    } else {
        Smg2Ui::calcAnimLayout(sHitPointMeterLayout);
        Smg2Ui::calcAnimLayout(sCoinCounterLayout);
        Smg2Ui::calcAnimLayout(sStarCounterLayout);
        Smg2Ui::calcAnimLayout(sStarPieceCounterLayout);
    }
    if (sHasPointer) {
        Smg2Ui::calcAnimLayout(sPointerLayout);
    }
}

void GameScene::draw() const {
    if (mPhase == Phase_Title) {
        const f32 ang = static_cast<f32>(mPhaseSteps) * 0.003f;
        Smg23D::setCameraUp(0.0f, 1.0f, 0.0f);
        Smg23D::begin3DScene(9500.0f * __builtin_sinf(ang), 1200.0f, 9500.0f * __builtin_cosf(ang), 0.0f, -500.0f, 0.0f, 55.0f, 10.0f, 100000.0f);
        Smg23D::drawMenuBackdrop(static_cast<u32>(mPhaseSteps));
        Smg23D::end3DScene();
        Smg2Ui::drawLayout(sTitleLogoLayout);
        Smg2Ui::drawLayout(sPressStartLayout);
    } else if (mPhase == Phase_FileSelect) {
        const f32 ang = static_cast<f32>(mPhaseSteps) * 0.003f;
        Smg23D::setCameraUp(0.0f, 1.0f, 0.0f);
        Smg23D::begin3DScene(9500.0f * __builtin_sinf(ang), 1200.0f, 9500.0f * __builtin_cosf(ang), 0.0f, -500.0f, 0.0f, 55.0f, 10.0f, 100000.0f);
        Smg23D::drawMenuBackdrop(static_cast<u32>(mPhaseSteps));
        Smg23D::end3DScene();
        Smg2Ui::drawLayout(sFileInfoLayout);
        for (int i = 0; i < 3; ++i) {
            Smg2Ui::drawLayout(sFileNumberLayouts[i]);
        }
        Smg2Ui::drawLayout(sFileSelectLayout);
    } else {
        const TVec3f& pos = sMarioActor.getPosition();
        const TVec3f& up = sMarioActor.getUpVec();
        const TVec3f& camPos = sCameraDirector.getPosition();
        const TVec3f& camTgt = sCameraDirector.getTarget();
        const TVec3f& camUp = sCameraDirector.getUp();
        Smg23D::setCameraUp(camUp.x, camUp.y, camUp.z);
        Smg23D::begin3DScene(camPos.x, camPos.y, camPos.z, camTgt.x, camTgt.y, camTgt.z, 60.0f, 10.0f, 100000.0f);
        Smg23D::drawStageAndMario(pos.x, pos.y, pos.z, up.x, up.y, up.z, sMarioActor.getYaw(), static_cast<u32>(mPhaseSteps));
        Smg23D::end3DScene();
        Smg2Ui::drawLayout(sHitPointMeterLayout);
        Smg2Ui::drawLayout(sCoinCounterLayout);
        Smg2Ui::drawLayout(sStarCounterLayout);
        Smg2Ui::drawLayout(sStarPieceCounterLayout);
    }
    if (sHasPointer) {
        Smg2Ui::drawLayout(sPointerLayout);
    }
}
