// Bridge between Petari's native Ui/LayoutManager/FrameList stack and SMG2's SB4E01 game.pak assets.

#include "Game/System/Arenas.hpp"
#include "Game/System/AssetFolder.hpp"
#include "Game/System/GameAssets.hpp"
#include "Game/System/GameSystemFontHolder.hpp"
#include "Game/System/LayoutHolder.hpp"
#include "Game/System/MessageHolder.hpp"
#include "Game/System/NebulaFrameClock.hpp"
#include "Game/Util/FileUtil.hpp"
#include "Game/Util/ScreenUtil.hpp"
#include "Game/Util/StringUtil.hpp"
#include "Game/Util/Table.hpp"
#include "Game/Render/FrameList.hpp"
#include "Game/Render/Material.hpp"
#include "Game/Render/Model.hpp"
#include "Game/Render/ModelData.hpp"
#include "Game/Screen/LayoutActor.hpp"
#include "Game/Screen/LayoutCoreUtil.hpp"
#include "Game/Screen/LayoutManager.hpp"
#include "Game/Screen/LayoutPaneCtrl.hpp"
#include <Game/Ui/ResFont.hpp>
#include <Game/Ui/TextBox.hpp>
#include <nebula_math.h>
#include <nebula_module.h>

#include <cstring>

namespace {
    struct CachedHolder {
        char name[128];
        LayoutHolder* holder;
    };
    CachedHolder sHolders[64]{};
    u32 sHolderCount = 0;

    AssetFolder* sFontFolder = nullptr;
    Ui::ResFont* sMessageFont = nullptr;
    Ui::ResFont* sMenuFont = nullptr;
    Ui::ResFont* sNumberFont = nullptr;
    Ui::ResFont* sPictureFont = nullptr;
    Ui::ResFont* sCinemaFont = nullptr;

    Ui::ResFont* loadFontFromFolder(AssetFolder* folder, const char* fileName) {
        if (folder == nullptr) {
            return nullptr;
        }
        void* res = folder->getResource(fileName);
        if (res == nullptr) {
            return nullptr;
        }
        Ui::ResFont* font = new (Arenas::permanent(), 4) Ui::ResFont();
        font->SetResource(res);
        font->SetAlternateChar('?');
        return font;
    }

    void ensureFontsLoaded() {
        if (sFontFolder != nullptr) {
            return;
        }
        static const char* const kFontPaths[] = {
            "/LocalizeData/UsEnglish/LayoutData/Font",
            "/LayoutData/Font",
        };
        for (const char* path : kFontPaths) {
            if (GameAssets::find(path) >= 0) {
                sFontFolder = AssetFolder::mount(path, Arenas::permanent());
                break;
            }
        }
        if (sFontFolder == nullptr) {
            nebula_log("[SMG2-UI] TODO(Garigari): Font folder not found in game.pak");
            return;
        }
        sMessageFont = loadFontFromFolder(sFontFolder, "MessageFont26.brfnt");
        sMenuFont = loadFontFromFolder(sFontFolder, "MenuFont64.brfnt");
        sNumberFont = loadFontFromFolder(sFontFolder, "NumberFont.brfnt");
        sPictureFont = loadFontFromFolder(sFontFolder, "PictureFont.brfnt");
        sCinemaFont = loadFontFromFolder(sFontFolder, "CinemaFont26.brfnt");
        nebula_log("[SMG2-UI] Loaded fonts from game.pak: msg=%p menu=%p num=%p pic=%p",
                   sMessageFont, sMenuFont, sNumberFont, sPictureFont);
    }

    struct MsbtEntry {
        const char* label;
        const char16_t* text;
    };
    MsbtEntry* sMsbtEntries = nullptr;
    u32 sMsbtCount = 0;
    u32 sMsbtRoom = 0;
    bool sMessageInitAttempted = false;

    u16 readU16(const u8* p, bool be) {
        return be ? static_cast<u16>((static_cast<u16>(p[0]) << 8) | p[1])
                  : static_cast<u16>(static_cast<u16>(p[0]) | (static_cast<u16>(p[1]) << 8));
    }

    u32 readU32(const u8* p, bool be) {
        return be ? ((static_cast<u32>(p[0]) << 24) | (static_cast<u32>(p[1]) << 16) |
                     (static_cast<u32>(p[2]) << 8) | static_cast<u32>(p[3]))
                  : (static_cast<u32>(p[0]) | (static_cast<u32>(p[1]) << 8) |
                     (static_cast<u32>(p[2]) << 16) | (static_cast<u32>(p[3]) << 24));
    }

    void addMsbtEntry(const char* label, u32 labelLen, const char16_t* text) {
        if (sMsbtCount == sMsbtRoom) {
            const u32 newRoom = sMsbtRoom != 0 ? sMsbtRoom * 2 : 512;
            auto* next = new (Arenas::permanent(), 0) MsbtEntry[newRoom];
            if (sMsbtEntries != nullptr && sMsbtCount > 0) {
                std::memcpy(next, sMsbtEntries, sMsbtCount * sizeof(MsbtEntry));
            }
            sMsbtEntries = next;
            sMsbtRoom = newRoom;
        }
        char* copy = new (Arenas::permanent(), 0) char[labelLen + 1];
        std::memcpy(copy, label, labelLen);
        copy[labelLen] = '\0';
        sMsbtEntries[sMsbtCount++] = MsbtEntry{copy, text};
    }

    void parseMsbtFile(const u8* data, u32 size) {
        if (data == nullptr || size < 0x20 || std::memcmp(data, "MsgStdBn", 8) != 0) {
            return;
        }
        const bool be = (data[8] == 0xFE && data[9] == 0xFF);
        const u16 secCount = readU16(data + 0x0E, be);

        const u8* lbl1 = nullptr;
        u32 lbl1Size = 0;
        const u8* txt2 = nullptr;
        u32 txt2Size = 0;

        u32 pos = 0x20;
        for (u16 s = 0; s < secCount && pos + 0x10 <= size; ++s) {
            const u32 secSize = readU32(data + pos + 4, be);
            const u8* secBody = data + pos + 0x10;
            if (pos + 0x10 + secSize > size) {
                break;
            }
            if (std::memcmp(data + pos, "LBL1", 4) == 0) {
                lbl1 = secBody;
                lbl1Size = secSize;
            } else if (std::memcmp(data + pos, "TXT2", 4) == 0) {
                txt2 = secBody;
                txt2Size = secSize;
            }
            pos += 0x10 + ((secSize + 15u) & ~15u);
        }

        if (lbl1 == nullptr || txt2 == nullptr || txt2Size < 4 || lbl1Size < 4) {
            return;
        }

        const u32 txtCount = readU32(txt2, be);
        if (4u + txtCount * 4u > txt2Size) {
            return;
        }
        auto** decodedTexts = new (Arenas::permanent(), 0) const char16_t*[txtCount];
        for (u32 i = 0; i < txtCount; ++i) {
            const u32 off = readU32(txt2 + 4u + i * 4u, be);
            const u32 nextOff = (i + 1u < txtCount) ? readU32(txt2 + 4u + (i + 1u) * 4u, be) : txt2Size;
            if (off >= txt2Size || nextOff <= off || nextOff > txt2Size) {
                decodedTexts[i] = u"";
                continue;
            }
            const u32 maxUnits = (nextOff - off) / 2u;
            auto* buf = new (Arenas::permanent(), 0) char16_t[maxUnits + 1u];
            u32 out = 0;
            for (u32 u = 0; u < maxUnits;) {
                const u16 ch = readU16(txt2 + off + u * 2u, be);
                if (ch == 0) {
                    break;
                }
                if (ch == 0x000Eu && u + 3u < maxUnits) {
                    // Skip MSBT control tag (0x000E, group, type, paramByteLen, params...)
                    const u16 paramBytes = readU16(txt2 + off + (u + 3u) * 2u, be);
                    u += 4u + (paramBytes + 1u) / 2u;
                    continue;
                }
                buf[out++] = static_cast<char16_t>(ch);
                u++;
            }
            buf[out] = 0;
            decodedTexts[i] = buf;
        }

        const u32 groupCount = readU32(lbl1, be);
        if (4u + groupCount * 8u > lbl1Size) {
            return;
        }
        for (u32 g = 0; g < groupCount; ++g) {
            const u32 entryCount = readU32(lbl1 + 4u + g * 8u, be);
            u32 offset = readU32(lbl1 + 8u + g * 8u, be);
            for (u32 e = 0; e < entryCount && offset < lbl1Size; ++e) {
                const u8 len = lbl1[offset++];
                if (offset + len + 4u > lbl1Size) {
                    break;
                }
                const char* labelPtr = reinterpret_cast<const char*>(lbl1 + offset);
                offset += len;
                const u32 strIdx = readU32(lbl1 + offset, be);
                offset += 4u;
                if (strIdx < txtCount) {
                    addMsbtEntry(labelPtr, len, decodedTexts[strIdx]);
                }
            }
        }
    }

    void mountMsbtFolder(const char* folderPath) {
        if (GameAssets::find(folderPath) < 0) {
            return;
        }
        AssetFolder* folder = AssetFolder::mount(folderPath, Arenas::permanent());
        if (folder == nullptr) {
            return;
        }
        GameAssets::FolderEntry fe{};
        for (u32 i = 0; GameAssets::folderEntry(folderPath, i, &fe); ++i) {
            if (fe.mIsFolder || !MR::isEqualSubString(fe.mName, ".msbt")) {
                continue;
            }
            GameAssets::File file{};
            if (!GameAssets::open(fe.mEntry, &file)) {
                continue;
            }
            const void* raw = folder->getResource(fe.mName);
            if (raw != nullptr) {
                parseMsbtFile(static_cast<const u8*>(raw), file.mLength);
            }
        }
    }

    void ensureMessagesLoaded() {
        if (sMessageInitAttempted) {
            return;
        }
        sMessageInitAttempted = true;
        mountMsbtFolder("/LocalizeData/UsEnglish/MessageData/LayoutMessage");
        mountMsbtFolder("/LocalizeData/UsEnglish/MessageData/SystemMessage");
        mountMsbtFolder("/LocalizeData/UsEnglish/MessageData/SceneCommonMessage");
        nebula_log("[SMG2-UI] Loaded MSBT messages: %u entries", sMsbtCount);
    }

    const char16_t* lookupMessageById(const char* pMessageId) {
        if (pMessageId == nullptr || pMessageId[0] == '\0') {
            return nullptr;
        }
        ensureMessagesLoaded();
        for (u32 i = 0; i < sMsbtCount; ++i) {
            if (std::strcmp(sMsbtEntries[i].label, pMessageId) == 0) {
                return sMsbtEntries[i].text;
            }
        }
        return nullptr;
    }

    bool findLayoutArchivePath(MR::FormattedName& rOut, const char* pPrefix) {
        static const char* const kPatterns[] = {
            "/LayoutData/%s.arc",
            "/LocalizeData/UsEnglish/LayoutData/%s.arc",
            "/LayoutData/%s",
            "/LocalizeData/UsEnglish/LayoutData/%s",
        };
        for (const char* pat : kPatterns) {
            MR::FormattedName candidate(pat, pPrefix);
            if (GameAssets::find(candidate.c_str()) >= 0) {
                rOut.format("%s", candidate.c_str());
                return true;
            }
        }
        return false;
    }
}  // namespace

namespace MR {
    bool isScreen16Per9() {
        return true;
    }

    s32 getScreenWidth() {
        if (isScreen16Per9() && getDisplay().wide) {
            return getDisplay().screen_width;
        }
        return isScreen16Per9() ? NebulaFrameClock::cWideScreenWidth : static_cast<s32>(NebulaFrameClock::cLayoutWidth);
    }

    f32 getScreenAspect() {
        if (isScreen16Per9()) {
            return getDisplay().wide ? getDisplay().content_aspect : 16.0f / 9.0f;
        }
        return static_cast<f32>(getScreenWidth()) / getScreenHeight();
    }

    bool makeLayoutArchiveFileNameFromPrefix(MR::FormattedName& rOut, const char* pPrefix, bool /*fallback*/) {
        if (findLayoutArchivePath(rOut, pPrefix)) {
            return true;
        }
        MR::FormattedName wide("%s16x9", pPrefix);
        if (findLayoutArchivePath(rOut, wide.c_str())) {
            return true;
        }
        MR::FormattedName narrow("%s4x3", pPrefix);
        if (findLayoutArchivePath(rOut, narrow.c_str())) {
            return true;
        }
        rOut.format("%s", pPrefix);
        return false;
    }

    LayoutHolder* createAndAddLayoutHolder(const char* pArcName) {
        if (pArcName == nullptr) {
            return nullptr;
        }
        for (u32 i = 0; i < sHolderCount; ++i) {
            if (std::strcmp(sHolders[i].name, pArcName) == 0) {
                return sHolders[i].holder;
            }
        }
        MR::FormattedName path;
        if (!makeLayoutArchiveFileNameFromPrefix(path, pArcName, true)) {
            nebula_log("[SMG2-UI] Layout archive not found in game.pak: %s", pArcName);
            return nullptr;
        }
        AssetFolder* folder = AssetFolder::mount(path.c_str(), Arenas::permanent());
        if (folder == nullptr) {
            return nullptr;
        }
        LayoutHolder* holder = new (Arenas::permanent(), 0) LayoutHolder(*folder);
        if (sHolderCount < 64u) {
            std::strncpy(sHolders[sHolderCount].name, pArcName, sizeof(sHolders[sHolderCount].name) - 1u);
            sHolders[sHolderCount].holder = holder;
            sHolderCount++;
        }
        nebula_log("[SMG2-UI] Mounted LayoutHolder '%s' -> '%s'", pArcName, path.c_str());
        return holder;
    }

    Ui::Font* getFontOnCurrentLanguage() {
        ensureFontsLoaded();
        return sMessageFont;
    }

    Ui::Font* getMenuFont() {
        ensureFontsLoaded();
        return sMenuFont != nullptr ? sMenuFont : sMessageFont;
    }

    Ui::Font* getNumberFont() {
        ensureFontsLoaded();
        return sNumberFont != nullptr ? sNumberFont : sMessageFont;
    }

    Ui::Font* getPictureFont() {
        ensureFontsLoaded();
        return sPictureFont != nullptr ? sPictureFont : sMessageFont;
    }

    Ui::Font* getCinemaFont() {
        ensureFontsLoaded();
        return sCinemaFont != nullptr ? sCinemaFont : sMessageFont;
    }

    void getLayoutMessageID(MR::FormattedName& rOut, const char* pLayoutName, const char* pPaneName) {
        rOut.format("%s_%s", pLayoutName != nullptr ? pLayoutName : "", pPaneName != nullptr ? pPaneName : "");
    }

    const char16_t* getLayoutMessageDirect(const char* pMessageId) {
        if (const char16_t* msg = lookupMessageById(pMessageId)) {
            nebula_log("[SMG2-UI] LayoutMsg HIT: %s", pMessageId);
            return msg;
        }
        // Fallback: if LayoutName_UserData didn't match, try stripping LayoutName_ prefix
        if (pMessageId != nullptr) {
            if (const char* underscore = std::strchr(pMessageId, '_')) {
                if (const char16_t* msg = lookupMessageById(underscore + 1)) {
                    nebula_log("[SMG2-UI] LayoutMsg HIT(suffix): %s", underscore + 1);
                    return msg;
                }
            }
            nebula_log("[SMG2-UI] LayoutMsg MISS: %s", pMessageId);
        }
        return nullptr;
    }

    const char16_t* getGameMessageDirect(const char* pMessageId) {
        return lookupMessageById(pMessageId);
    }

    u32 getRaceBestTime(int) { return 0u; }
    u32 getRaceCurrentTime() { return 0u; }

    bool isDead(const LayoutActor* pActor) {
        return pActor == nullptr || pActor->mFlag.mIsDead;
    }

    bool isDisplayWaitScreen() { return false; }

    const ResTIMG* getScreenResTIMG() { return nullptr; }
    bool isPlayerLuigi() { return false; }
    s32 countMessageChar(const char16_t* pMsg) {
        if (pMsg == nullptr) return 0;
        s32 n = 0;
        while (pMsg[n] != 0) ++n;
        return n;
    }

    static Proj3f sIdentityProj;
    static Pos3f sIdentityView;
    const Proj3f& getCameraProjectionMtx() { return sIdentityProj; }
    const Pos3f& getCameraViewMtx() { return sIdentityView; }
}  // namespace MR

class SoundHandle;
namespace MR {
    SoundHandle* startSystemSE(const char*, s32, s32) { return nullptr; }
}  // namespace MR

const Mtx cIdentityMtx = {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
};

namespace GameSceneFunction {
    void activateDraw3D() {}
    void deactivateDraw3D() {}
}  // namespace GameSceneFunction

namespace NebulaIcons {
    void noteDrawn(const void*) {}
}  // namespace NebulaIcons

namespace Smg2Ui {
    LayoutManager* createLayout(const char* pName, bool usePrefix, u32 rootAnimLayers, u32 textBoxLen, bool keepText) {
        if (pName == nullptr) {
            return nullptr;
        }
        MR::FormattedName path;
        if (usePrefix && !MR::makeLayoutArchiveFileNameFromPrefix(path, pName, true)) {
            nebula_log("[SMG2-UI] Layout '%s' not found in game.pak", pName);
            return nullptr;
        }
        return new LayoutManager(pName, usePrefix, rootAnimLayers, textBoxLen, keepText);
    }
    void movementLayout(LayoutManager* pMgr) {
        if (pMgr != nullptr) pMgr->movement();
    }
    void calcAnimLayout(LayoutManager* pMgr) {
        if (pMgr != nullptr) pMgr->calcAnim();
    }
    void drawLayout(const LayoutManager* pMgr) {
        if (pMgr != nullptr) pMgr->draw();
    }
    void startRootAnim(LayoutManager* pMgr, const char* pAnimName, u32 layer) {
        if (pMgr == nullptr || pAnimName == nullptr) return;
        LayoutPaneCtrl* pCtrl = pMgr->createAndAddRootPaneCtrl(layer + 1u);
        if (pCtrl != nullptr && pMgr->getAnimTransform(pAnimName) != nullptr) {
            pCtrl->start(pAnimName, layer);
        }
    }
    bool startRootAnimFallback(LayoutManager* pMgr, const char* pFirst, const char* pSecond, u32 layer) {
        if (pMgr == nullptr) return false;
        const char* chosen = (pFirst != nullptr && pMgr->getAnimTransform(pFirst) != nullptr)
                                 ? pFirst
                                 : ((pSecond != nullptr && pMgr->getAnimTransform(pSecond) != nullptr) ? pSecond : nullptr);
        if (chosen == nullptr) return false;
        LayoutPaneCtrl* pCtrl = pMgr->createAndAddRootPaneCtrl(layer + 1u);
        if (pCtrl != nullptr) {
            pCtrl->start(chosen, layer);
            return true;
        }
        return false;
    }
    bool isRootAnimStopped(const LayoutManager* pMgr, u32 layer) {
        if (pMgr == nullptr || pMgr->mLayout == nullptr || pMgr->mLayout->mpRootPane == nullptr) return true;
        LayoutPaneCtrl* pCtrl = pMgr->getPaneCtrl(pMgr->mLayout->mpRootPane->mName);
        return pCtrl == nullptr || pCtrl->isAnimStopped(layer);
    }
    static void setTextBoxMessageRecursiveImpl(Ui::Pane* pPane, const char16_t* pMsg) {
        if (pPane == nullptr || pMsg == nullptr) return;
        if (Ui::TextBox* pTextBox = Ui::PaneCast<Ui::TextBox>(pPane)) {
            LayoutCoreUtil::setTextBoxMessage(pTextBox, pMsg);
        }
        for (Ui::Pane* pChild : pPane->mChildList) {
            setTextBoxMessageRecursiveImpl(pChild, pMsg);
        }
    }
    void setTextBoxMessage(LayoutManager* pMgr, const char* pPaneName, const char16_t* pMsg) {
        if (pMgr == nullptr || pMsg == nullptr) return;
        if (Ui::Pane* pPane = pMgr->getPane(pPaneName)) {
            setTextBoxMessageRecursiveImpl(pPane, pMsg);
        }
    }
    void setTextBoxNumber(LayoutManager* pMgr, const char* pPaneName, s32 number) {
        char16_t buf[32]{};
        char ascii[32]{};
        snprintf(ascii, sizeof(ascii), "%d", number);
        for (size_t i = 0; ascii[i] != '\0' && i < 31u; ++i) {
            buf[i] = static_cast<char16_t>(ascii[i]);
        }
        setTextBoxMessage(pMgr, pPaneName, buf);
    }
    void setPaneVisible(LayoutManager* pMgr, const char* pPaneName, bool visible) {
        if (pMgr == nullptr) return;
        if (Ui::Pane* pPane = pMgr->getPane(pPaneName)) {
            pPane->SetVisible(visible);
        }
    }
}  // namespace Smg2Ui

namespace Smg23D {
    namespace {
        Model* sMarioModel = nullptr;
        constexpr u32 kMaxStageModels = 16;
        Model* sStageModels[kMaxStageModels]{};
        u32 sStageModelCount = 0;
        bool sStageModelsMounted = false;

        Model* loadModelFromArchive(const char* archivePath, const char* preferredName) {
            if (GameAssets::find(archivePath) < 0) {
                return nullptr;
            }
            AssetFolder* folder = AssetFolder::mount(archivePath, Arenas::permanent());
            if (folder == nullptr) {
                return nullptr;
            }
            void* res = nullptr;
            const char* foundName = nullptr;
            if (preferredName != nullptr) {
                res = folder->getResource(preferredName);
                if (res != nullptr) foundName = preferredName;
            }
            if (res == nullptr) {
                GameAssets::FolderEntry fe{};
                for (u32 i = 0; GameAssets::folderEntry(archivePath, i, &fe); ++i) {
                    if (!fe.mIsFolder && (MR::isEqualSubString(fe.mName, ".bdl") || MR::isEqualSubString(fe.mName, ".bmd"))) {
                        res = folder->getResource(fe.mName);
                        foundName = fe.mName;
                        if (res != nullptr) break;
                    }
                }
            }
            if (res == nullptr) {
                return nullptr;
            }
            ModelData* pData = ModelData::load(res);
            if (pData == nullptr) {
                return nullptr;
            }
            pData->setResourceName(foundName != nullptr ? foundName : archivePath);
            Model* pModel = new (Arenas::permanent(), 0) Model(pData, ModelFlag_None, 1u);
            nebula_log("[SMG2-3D] Loaded model %s/%s (joints=%u, mats=%u, meshes=%u)",
                       archivePath, foundName != nullptr ? foundName : "?",
                       pData->getJointCount(), pData->getMaterialCount(), pData->getMeshCount());
            return pModel;
        }
    }  // namespace

    void mountStageModels(const char* pStageName) {
        if (sMarioModel == nullptr) {
            sMarioModel = loadModelFromArchive("/ObjectData/Mario", "Mario.bdl");
            if (sMarioModel == nullptr) {
                sMarioModel = loadModelFromArchive("/ObjectData/Mario", "Mario.bmd");
            }
        }
        if (sStageModelsMounted) {
            return;
        }
        sStageModelsMounted = true;

        // Mount planet/sky models matching the stage prefix (e.g., IslandFleet* for IslandFleetGalaxy)
        char prefix[64]{};
        if (pStageName != nullptr) {
            std::strncpy(prefix, pStageName, sizeof(prefix) - 1u);
            if (char* g = std::strstr(prefix, "Galaxy")) {
                *g = '\0';
            }
        }
        if (prefix[0] != '\0') {
            GameAssets::FolderEntry fe{};
            for (u32 i = 0; sStageModelCount < kMaxStageModels && GameAssets::folderEntry("/ObjectData", i, &fe); ++i) {
                if (!fe.mIsFolder) continue;
                if (MR::isEqualSubString(fe.mName, prefix) &&
                    (MR::isEqualSubString(fe.mName, "Planet") || MR::isEqualSubString(fe.mName, "Sky") ||
                     MR::isEqualSubString(fe.mName, "Start") || MR::isEqualSubString(fe.mName, "Island"))) {
                    char path[128]{};
                    snprintf(path, sizeof(path), "/ObjectData/%s", fe.mName);
                    if (Model* m = loadModelFromArchive(path, nullptr)) {
                        sStageModels[sStageModelCount++] = m;
                    }
                }
            }
        }
        nebula_log("[SMG2-3D] Stage '%s' models ready: mario=%p, stageModels=%u",
                   pStageName != nullptr ? pStageName : "", sMarioModel, sStageModelCount);
    }

    void begin3DScene(f32 camX, f32 camY, f32 camZ, f32 targetX, f32 targetY, f32 targetZ, f32 fovY, f32 nearZ, f32 farZ) {
        Point3d camPos = { camX, camY, camZ };
        Vec upVec = { 0.0f, 1.0f, 0.0f };
        Point3d targetPos = { targetX, targetY, targetZ };
        Mtx viewMtx;
        NebulaMtxLookAt(viewMtx, &camPos, &upVec, &targetPos);

        Mtx44 projMtx;
        NebulaMtxPerspective(projMtx, fovY, MR::getScreenAspect(), nearZ, farZ);

        Frame::setPerspective(projMtx);
        Frame::setViewport(0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f);
        Frame::setScissor(0.0f, 0.0f, 1.0f, 1.0f);
        Frame::sceneBegin(viewMtx);

        NebulaMtxCopy(viewMtx, ModelSys::mViewMtx);
        ModelSys::mViewRole = NEBULA_CAMERA_SCENE;
    }

    static void drawSingleModel(Model* pModel, f32 x, f32 y, f32 z, f32 yawRad) {
        if (pModel == nullptr || pModel->getModelData() == nullptr) return;
        Mtx baseMtx;
        const f32 c = __builtin_cosf(yawRad);
        const f32 s = __builtin_sinf(yawRad);
        baseMtx[0][0] = c;    baseMtx[0][1] = 0.0f; baseMtx[0][2] = s;    baseMtx[0][3] = x;
        baseMtx[1][0] = 0.0f; baseMtx[1][1] = 1.0f; baseMtx[1][2] = 0.0f; baseMtx[1][3] = y;
        baseMtx[2][0] = -s;   baseMtx[2][1] = 0.0f; baseMtx[2][2] = c;    baseMtx[2][3] = z;

        pModel->setBaseTRMtx(baseMtx);
        pModel->calc();
        pModel->viewCalc();

        ModelData* pData = pModel->getModelData();
        const u32 matCount = pData->getMaterialCount();
        for (u32 pass = 0; pass < 2u; ++pass) {
            const bool wantXlu = (pass == 1u);
            for (u32 i = 0; i < matCount; ++i) {
                Material* pMat = pModel->getMaterial(i);
                if (pMat == nullptr || pMat->getMesh() == nullptr || pMat->getMesh()->isHidden()) continue;
                if (pMat->isTranslucent() != wantXlu) continue;
                MaterialDraw draw(pMat);
                draw.draw(pModel, pMat->getMesh());
            }
        }
    }

    void drawStageAndMario(f32 marioX, f32 marioY, f32 marioZ, f32 marioYawRad) {
        for (u32 i = 0; i < sStageModelCount; ++i) {
            drawSingleModel(sStageModels[i], 0.0f, 0.0f, 0.0f, 0.0f);
        }
        drawSingleModel(sMarioModel, marioX, marioY, marioZ, marioYawRad);
    }

    void end3DScene() {
        Frame::opaqueEnd(nullptr);
        Frame::sceneEnd();
    }
}  // namespace Smg23D

// Luma hook release callbacks (unused when Luma runtime hooks are disabled)
namespace lumacore { void released(const void*, const void*) {} }
namespace luma {
    namespace content {
        namespace fx { void released(const void*, const void*) {} }
        namespace model { void released(const void*, const void*) {} }
    }
    namespace draw {
        void released(const void*, const void*) {}
        namespace inst { void released(const void*, const void*) {} }
        namespace view { void released(const void*, const void*) {} }
    }
    namespace ui { void released(const void*, const void*) {} }
    namespace anim { void released(const void*, const void*) {} }
    namespace player { void released(const void*, const void*) {} }
    namespace font { Ui::Font* find(const char*) { return nullptr; } }
}  // namespace luma
