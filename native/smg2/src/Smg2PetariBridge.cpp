// Bridge between Petari's native Ui/LayoutManager/FrameList stack and SMG2's SB4E01 game.pak assets.

#include "Game/System/Arenas.hpp"
#include "Game/System/AssetFolder.hpp"
#include "Game/System/GameAssets.hpp"
#include "Game/System/GameSystemFontHolder.hpp"
#include "Game/System/LayoutHolder.hpp"
#include "Game/System/MessageHolder.hpp"
#include "Game/System/NebulaFrameClock.hpp"
#include "Game/Util/FileUtil.hpp"
#include "Game/Util/MtxUtil.hpp"
#include "Game/Util/ScreenUtil.hpp"
#include "Game/Util/StringUtil.hpp"
#include "Game/Util/Table.hpp"
#include "Game/Animation/XanimeCore.hpp"
#include "Game/Map/CollisionCode.hpp"
#include "Game/Map/KCollision.hpp"
#include "Game/Render/Clip.hpp"
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
        if (pMessageId != nullptr && std::strcmp(pMessageId, "PressStart_PressAB") == 0) {
            return u"Press A to Start.";
        }
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

    void vecBlend(const Vec3f& a, const Vec3f& b, Vec3f* out, f32 t) {
        if (out == nullptr) return;
        out->x = a.x + (b.x - a.x) * t;
        out->y = a.y + (b.y - a.y) * t;
        out->z = a.z + (b.z - a.z) * t;
    }
    void extractMtxTrans(MtxPtr mtx, Vec3f* out) {
        if (out == nullptr) return;
        out->set(mtx[0][3], mtx[1][3], mtx[2][3]);
    }
    bool isNearZero(const Vec3f& v, f32 eps) {
        return (v.x * v.x + v.y * v.y + v.z * v.z) <= eps * eps;
    }
    bool isSameDirection(const Vec3f& a, const Vec3f& b, f32 eps) {
        const f32 cx = a.y * b.z - a.z * b.y;
        const f32 cy = a.z * b.x - a.x * b.z;
        const f32 cz = a.x * b.y - a.y * b.x;
        return (cx * cx + cy * cy + cz * cz) <= eps * eps && (a.x * b.x + a.y * b.y + a.z * b.z) > 0.0f;
    }
    void separateScalarAndDirection(f32* scalar, Vec3f* dir, const Vec3f& src) {
        const f32 len = __builtin_sqrtf(src.x * src.x + src.y * src.y + src.z * src.z);
        if (scalar) *scalar = len;
        if (dir) {
            if (len > 1.0e-6f) {
                const f32 inv = 1.0f / len;
                dir->set(src.x * inv, src.y * inv, src.z * inv);
            } else {
                dir->set(0.0f, 0.0f, 0.0f);
            }
        }
    }
    void createBoundingBox(const Vec3f* pts, u32 count, Vec3f* minOut, Vec3f* maxOut) {
        if (pts == nullptr || count == 0 || minOut == nullptr || maxOut == nullptr) return;
        *minOut = pts[0];
        *maxOut = pts[0];
        for (u32 i = 1; i < count; ++i) {
            if (pts[i].x < minOut->x) minOut->x = pts[i].x;
            if (pts[i].y < minOut->y) minOut->y = pts[i].y;
            if (pts[i].z < minOut->z) minOut->z = pts[i].z;
            if (pts[i].x > maxOut->x) maxOut->x = pts[i].x;
            if (pts[i].y > maxOut->y) maxOut->y = pts[i].y;
            if (pts[i].z > maxOut->z) maxOut->z = pts[i].z;
        }
    }
}  // namespace MR
class CollisionDirector;
namespace MR {
    CollisionDirector* getCollisionDirector() { return nullptr; }
    void registerCameraCode(u32) {}
}  // namespace MR

u32 CollisionCode::getCameraID(const TableIter&) { return 0u; }

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
        if (Ui::Pane* pPane = (pPaneName != nullptr ? pMgr->getPane(pPaneName) : (pMgr->mLayout != nullptr ? pMgr->mLayout->mpRootPane : nullptr))) {
            pPane->SetVisible(visible);
        }
    }
    void setPanePos(LayoutManager* pMgr, const char* pPaneName, f32 x, f32 y) {
        if (pMgr == nullptr) return;
        if (Ui::Pane* pPane = (pPaneName != nullptr ? pMgr->getPane(pPaneName) : (pMgr->mLayout != nullptr ? pMgr->mLayout->mpRootPane : nullptr))) {
            pPane->mTranslate.x = x;
            pPane->mTranslate.y = y;
        }
    }
    void setPaneScale(LayoutManager* pMgr, const char* pPaneName, f32 sx, f32 sy) {
        if (pMgr == nullptr) return;
        if (Ui::Pane* pPane = (pPaneName != nullptr ? pMgr->getPane(pPaneName) : (pMgr->mLayout != nullptr ? pMgr->mLayout->mpRootPane : nullptr))) {
            pPane->mScale.x = sx;
            pPane->mScale.y = sy;
        }
    }
    void setPaneAnimFrameAndStop(LayoutManager* pMgr, const char* pPaneName, const char* pAnimName, f32 frame, u32 layer) {
        if (pMgr == nullptr || pAnimName == nullptr) return;
        if (pMgr->getAnimTransform(pAnimName) == nullptr) return;
        LayoutPaneCtrl* pCtrl = (pPaneName != nullptr)
                                    ? pMgr->createAndAddPaneCtrl(pPaneName, layer + 1u)
                                    : pMgr->createAndAddRootPaneCtrl(layer + 1u);
        if (pCtrl != nullptr) {
            pCtrl->start(pAnimName, layer);
            if (FrameCtrl* pFc = pCtrl->getFrameCtrl(layer)) {
                pFc->setFrame(frame);
                pFc->setRate(0.0f);
            }
        }
    }
    static void clearPlaceholderTextBoxesImpl(Ui::Pane* pPane) {
        if (pPane == nullptr) return;
        if (Ui::TextBox* pTb = Ui::PaneCast<Ui::TextBox>(pPane)) {
            const char16_t* s = pTb->mTextBuf;
            if (s != nullptr && s[0] == u'X' && s[1] == u'X') {
                LayoutCoreUtil::setTextBoxMessage(pTb, u"");
                pTb->SetVisible(false);
            }
        }
        for (Ui::Pane* pChild : pPane->mChildList) {
            clearPlaceholderTextBoxesImpl(pChild);
        }
    }
    void clearPlaceholderTextBoxes(LayoutManager* pMgr) {
        if (pMgr == nullptr || pMgr->mLayout == nullptr || pMgr->mLayout->mpRootPane == nullptr) return;
        clearPlaceholderTextBoxesImpl(pMgr->mLayout->mpRootPane);
    }
    void startPaneAnim(LayoutManager* pMgr, const char* pPaneName, const char* pAnimName, u32 layer) {
        if (pMgr == nullptr || pPaneName == nullptr || pAnimName == nullptr) return;
        if (pMgr->getAnimTransform(pAnimName) == nullptr) return;
        LayoutPaneCtrl* pCtrl = pMgr->createAndAddPaneCtrl(pPaneName, layer + 1u);
        if (pCtrl != nullptr) {
            pCtrl->start(pAnimName, layer);
        }
    }
    bool isPointingPane(const LayoutManager* pMgr, const char* pPaneName, f32 screenX, f32 screenY) {
        if (pMgr == nullptr || pPaneName == nullptr) return false;
        Vec2f pt;
        pt.x = screenX;
        pt.y = screenY;
        return pMgr->isPointing(pPaneName, pt);
    }
}  // namespace Smg2Ui

namespace Smg23D {
    namespace {
        Model* sMarioModel = nullptr;
        XanimeCore* sMarioAnimCore = nullptr;
        Clip* sMarioClips[3]{};  // 0=Wait, 1=Run, 2=Jump
        int sMarioAnimState = 0;
        f32 sMarioAnimRate = 1.0f;

        constexpr u32 kMaxStageModels = 16;
        Model* sStageModels[kMaxStageModels]{};
        u32 sStageModelCount = 0;
        Model* sSkyModel = nullptr;
        Model* sMenuShipModel = nullptr;
        Model* sMarioShadowModel = nullptr;
        Model* sCoinModel = nullptr;
        Model* sStarPieceModel = nullptr;
        struct CollectiblePos {
            f32 x, y, z;
            bool isCoin;
            bool collected;
        };
        constexpr u32 kMaxCollectibles = 8;
        CollectiblePos sCollectibles[kMaxCollectibles]{};
        u32 sCollectibleCount = 0;
        KCollisionServer* sKclServers[kMaxStageModels]{};
        u32 sKclCount = 0;
        f32 sCamPosX = 0.0f, sCamPosY = 0.0f, sCamPosZ = 0.0f;
        f32 sCamUpX = 0.0f, sCamUpY = 1.0f, sCamUpZ = 0.0f;
        bool sStageModelsMounted = false;
        bool sMenuBackdropMounted = false;

        Clip* loadClipFromArchive(const char* archivePath, const char* const* names, u32 nameCount, const ModelData* pModelData) {
            if (GameAssets::find(archivePath) < 0) return nullptr;
            AssetFolder* folder = AssetFolder::mount(archivePath, Arenas::permanent());
            if (folder == nullptr) return nullptr;
            for (u32 i = 0; i < nameCount; ++i) {
                void* res = folder->getResource(names[i]);
                if (res != nullptr) {
                    Clip* clip = Clip::load(res);
                    if (clip != nullptr) {
                        if (pModelData != nullptr) clip->bind(pModelData);
                        nebula_log("[SMG2-3D] Loaded Mario clip %s/%s (frames=%d)", archivePath, names[i], clip->getFrameMax());
                        return clip;
                    }
                }
            }
            return nullptr;
        }

        Model* loadModelFromArchive(const char* archivePath, const char* preferredName, bool loadKcl) {
            if (GameAssets::find(archivePath) < 0) {
                return nullptr;
            }
            AssetFolder* folder = AssetFolder::mount(archivePath, Arenas::permanent());
            if (folder == nullptr) {
                return nullptr;
            }
            if (loadKcl && sKclCount < kMaxStageModels) {
                GameAssets::FolderEntry fe{};
                for (u32 i = 0; GameAssets::folderEntry(archivePath, i, &fe); ++i) {
                    if (!fe.mIsFolder && MR::isEqualSubString(fe.mName, ".kcl")) {
                        GameAssets::File kclFile{};
                        void* kclRes = folder->getResource(fe.mName);
                        if (kclRes != nullptr && GameAssets::open(fe.mEntry, &kclFile) && kclFile.mLength >= sizeof(NcolHeader)) {
                            const auto* hdr = static_cast<const NcolHeader*>(kclRes);
                            if (hdr->mMagic[0] == 'N' && hdr->mMagic[1] == 'C' && hdr->mMagic[2] == 'O' && hdr->mMagic[3] == 'L' &&
                                hdr->mPosOffset < kclFile.mLength && hdr->mNormOffset < kclFile.mLength &&
                                hdr->mPrismOffset < kclFile.mLength && hdr->mOctreeOffset < kclFile.mLength) {
                                auto* srv = new (Arenas::permanent(), 0) KCollisionServer();
                                srv->setData(kclRes);
                                if (srv->getTriangleNum() > 0 && srv->mModel.mPos != nullptr &&
                                    srv->mModel.mNorms != nullptr && srv->mModel.mPrisms != nullptr &&
                                    srv->mModel.mOctree != nullptr) {
                                    sKclServers[sKclCount++] = srv;
                                    nebula_log("[SMG2-3D] Mounted KCL %s/%s (tris=%d)", archivePath, fe.mName, srv->getTriangleNum());
                                }
                            }
                        }
                        break;
                    }
                }
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

    bool raycastPlanet(f32 x, f32 y, f32 z, f32 dx, f32 dy, f32 dz, f32 maxDist,
                       f32* outX, f32* outY, f32* outZ, f32* outNx, f32* outNy, f32* outNz, bool topSurfaceOnly = false);

    void mountMenuBackdrop() {
        if (sMenuBackdropMounted) return;
        sMenuBackdropMounted = true;
        if (sSkyModel == nullptr) {
            static const char* const kMenuSkies[] = {
                "/ObjectData/StarrySky",
                "/ObjectData/MilkyWaySky",
                "/ObjectData/IslandFleetSky",
                "/ObjectData/BlueSky",
            };
            for (const char* skyPath : kMenuSkies) {
                if ((sSkyModel = loadModelFromArchive(skyPath, nullptr, false)) != nullptr) {
                    break;
                }
            }
        }
        if (sMenuShipModel == nullptr) {
            sMenuShipModel = loadModelFromArchive("/ObjectData/MarioFaceShipPlanet", nullptr, false);
            if (sMenuShipModel == nullptr) {
                sMenuShipModel = loadModelFromArchive("/ObjectData/MarioFacePlanet", nullptr, false);
            }
        }
        nebula_log("[SMG2-3D] Menu backdrop ready: sky=%p, ship=%p", sSkyModel, sMenuShipModel);
    }

    void mountStageModels(const char* pStageName) {
        mountMenuBackdrop();
        if (sMarioModel == nullptr) {
            sMarioModel = loadModelFromArchive("/ObjectData/Mario", "Mario.bdl", false);
            if (sMarioModel == nullptr) {
                sMarioModel = loadModelFromArchive("/ObjectData/Mario", "Mario.bmd", false);
            }
            if (sMarioModel != nullptr && sMarioModel->getModelData() != nullptr) {
                ModelData* pMarioData = sMarioModel->getModelData();
                static const char* const kWaitNames[] = { "Wait.bck", "Wait.bca", "wait.bck" };
                static const char* const kRunNames[] = { "Run.bck", "Dash.bck", "Walk.bck", "Run.bca" };
                static const char* const kJumpNames[] = { "Jump.bck", "JumpStart.bck", "Fall.bck", "Jump.bca" };
                sMarioClips[0] = loadClipFromArchive("/ObjectData/Mario", kWaitNames, 3u, pMarioData);
                if (sMarioClips[0] == nullptr) sMarioClips[0] = loadClipFromArchive("/ObjectData/MarioAnime", kWaitNames, 3u, pMarioData);
                sMarioClips[1] = loadClipFromArchive("/ObjectData/Mario", kRunNames, 4u, pMarioData);
                if (sMarioClips[1] == nullptr) sMarioClips[1] = loadClipFromArchive("/ObjectData/MarioAnime", kRunNames, 4u, pMarioData);
                sMarioClips[2] = loadClipFromArchive("/ObjectData/Mario", kJumpNames, 4u, pMarioData);
                if (sMarioClips[2] == nullptr) sMarioClips[2] = loadClipFromArchive("/ObjectData/MarioAnime", kJumpNames, 4u, pMarioData);
                if (sMarioClips[0] != nullptr) {
                    sMarioAnimCore = new (Arenas::permanent(), 0) XanimeCore(1u, pMarioData->getJointCount(), pMarioData->getScaleMode());
                    sMarioAnimCore->enableJointTransform(pMarioData);
                    for (u32 j = 0; j < pMarioData->getJointCount(); ++j) {
                        pMarioData->getJointAt(j)->setMtxCalc(sMarioAnimCore);
                    }
                    sMarioAnimCore->setBck(0u, sMarioClips[0]);
                } else {
                    nebula_log("[SMG2-3D] No Mario BCK clips loaded; keeping default bind pose MtxCalc");
                }
            }
        }
        if (sStageModelsMounted) {
            return;
        }
        sStageModelsMounted = true;

        char prefix[64]{};
        if (pStageName != nullptr) {
            std::strncpy(prefix, pStageName, sizeof(prefix) - 1u);
            if (char* g = std::strstr(prefix, "Galaxy")) {
                *g = '\0';
            }
        }
        if (prefix[0] != '\0') {
            GameAssets::FolderEntry fe{};
            for (u32 i = 0; GameAssets::folderEntry("/ObjectData", i, &fe); ++i) {
                if (!fe.mIsFolder) continue;
                if (MR::isEqualSubString(fe.mName, prefix)) {
                    char path[128]{};
                    snprintf(path, sizeof(path), "/ObjectData/%s", fe.mName);
                    if (MR::isEqualSubString(fe.mName, "Sky") && sSkyModel == nullptr) {
                        sSkyModel = loadModelFromArchive(path, nullptr, false);
                    } else if (sStageModelCount < kMaxStageModels &&
                               (MR::isEqualSubString(fe.mName, "Planet") || MR::isEqualSubString(fe.mName, "Start") ||
                                MR::isEqualSubString(fe.mName, "Island") || MR::isEqualSubString(fe.mName, "FaceShip"))) {
                        if (Model* m = loadModelFromArchive(path, nullptr, true)) {
                            sStageModels[sStageModelCount++] = m;
                        }
                    }
                }
            }
        }
        if (sSkyModel == nullptr) {
            static const char* const kFallbackSkies[] = {
                "/ObjectData/StarrySky",
                "/ObjectData/MilkyWaySky",
                "/ObjectData/IslandFleetSky",
                "/ObjectData/BlueSky",
            };
            for (const char* skyPath : kFallbackSkies) {
                if ((sSkyModel = loadModelFromArchive(skyPath, nullptr, false)) != nullptr) {
                    break;
                }
            }
        }
        if (sCoinModel == nullptr) {
            sCoinModel = loadModelFromArchive("/ObjectData/Coin", nullptr, false);
        }
        if (sStarPieceModel == nullptr) {
            sStarPieceModel = loadModelFromArchive("/ObjectData/StarPiece", nullptr, false);
        }
        if (sMarioShadowModel == nullptr) {
            sMarioShadowModel = loadModelFromArchive("/ObjectData/MarioShadow", nullptr, false);
        }
        sCollectibleCount = 0;
        for (u32 c = 0; c < kMaxCollectibles; ++c) {
            const f32 ang = static_cast<f32>(c) * (6.283185f / static_cast<f32>(kMaxCollectibles));
            const f32 rx = 350.0f * __builtin_cosf(ang);
            const f32 rz = 350.0f * __builtin_sinf(ang);
            f32 hx = rx, hy = 1050.0f, hz = rz;
            raycastPlanet(rx, 2000.0f, rz, 0.0f, -1.0f, 0.0f, 4000.0f, &hx, &hy, &hz, nullptr, nullptr, nullptr);
            sCollectibles[sCollectibleCount++] = CollectiblePos{ hx, hy + 60.0f, hz, (c % 2u) == 0u, false };
        }
        nebula_log("[SMG2-3D] Stage '%s' ready: mario=%p, shadow=%p, stageModels=%u, kcl=%u, sky=%p, coin=%p, starPiece=%p",
                   pStageName != nullptr ? pStageName : "", sMarioModel, sMarioShadowModel, sStageModelCount, sKclCount, sSkyModel, sCoinModel, sStarPieceModel);
    }

    bool resolveSphereCollision(f32* ioX, f32* ioY, f32* ioZ, f32 radius, f32* outNormalX, f32* outNormalY, f32* outNormalZ, bool topSurfaceOnly = false) {
        if (sKclCount == 0u || ioX == nullptr || ioY == nullptr || ioZ == nullptr) return false;
        bool hitAny = false;
        f32 bestNx = 0.0f, bestNy = 1.0f, bestNz = 0.0f;
        Vec3f center(*ioX, *ioY, *ioZ);
        for (u32 k = 0; k < sKclCount; ++k) {
            KCollisionServer* srv = sKclServers[k];
            if (srv == nullptr || srv->getTriangleNum() <= 0 || srv->mModel.mPrisms == nullptr ||
                srv->mModel.mNorms == nullptr || srv->mModel.mOctree == nullptr) {
                continue;
            }
            KC_PrismData* prisms[8]{};
            f32 dists[8]{};
            u8 parts[8]{};
            const u32 hits = srv->checkSphere(&center, radius, 0.0f, 8u, prisms, dists, parts);
            for (u32 h = 0; h < hits && h < 8u; ++h) {
                if (prisms[h] == nullptr) continue;
                const s32 triIdx = srv->toIndex(prisms[h]);
                if (triIdx < 0 || triIdx >= srv->getTriangleNum()) continue;
                const f32 pen = radius - dists[h];
                if (pen > 0.0f) {
                    const Vec3f* n = srv->getFaceNormal(prisms[h]);
                    if (n != nullptr && (!topSurfaceOnly || n->y > 0.05f)) {
                        center.x += n->x * pen;
                        center.y += n->y * pen;
                        center.z += n->z * pen;
                        bestNx = n->x;
                        bestNy = n->y;
                        bestNz = n->z;
                        hitAny = true;
                    }
                }
            }
        }
        if (hitAny) {
            *ioX = center.x;
            *ioY = center.y;
            *ioZ = center.z;
            if (outNormalX) *outNormalX = bestNx;
            if (outNormalY) *outNormalY = bestNy;
            if (outNormalZ) *outNormalZ = bestNz;
        }
        return hitAny;
    }

    bool raycastPlanet(f32 x, f32 y, f32 z, f32 dx, f32 dy, f32 dz, f32 maxDist,
                       f32* outX, f32* outY, f32* outZ, f32* outNx, f32* outNy, f32* outNz, bool topSurfaceOnly) {
        if (sKclCount == 0u) return false;
        const Vec3f origin(x, y, z);
        const Vec3f dir(dx * maxDist, dy * maxDist, dz * maxDist);
        f32 bestT = 2.0f;
        Vec3f bestN(0.0f, 1.0f, 0.0f);
        bool found = false;
        for (u32 k = 0; k < sKclCount; ++k) {
            KCollisionServer* srv = sKclServers[k];
            if (srv == nullptr || srv->getTriangleNum() <= 0 || srv->mModel.mPrisms == nullptr ||
                srv->mModel.mNorms == nullptr || srv->mModel.mOctree == nullptr) {
                continue;
            }
            f32 dists[4]{};
            u8 parts[4]{};
            u32 count = 0;
            KC_PrismData* prisms[4]{};
            if (srv->checkArrow(origin, dir, dists, parts, &count, prisms, 4u) != nullptr) {
                for (u32 i = 0; i < count && i < 4u; ++i) {
                    if (prisms[i] == nullptr) continue;
                    const s32 triIdx = srv->toIndex(prisms[i]);
                    if (triIdx < 0 || triIdx >= srv->getTriangleNum()) continue;
                    const Vec3f* n = srv->getFaceNormal(prisms[i]);
                    if (topSurfaceOnly && (n == nullptr || n->y <= 0.05f)) continue;
                    if (dists[i] >= 0.0f && dists[i] < bestT) {
                        bestT = dists[i];
                        if (n != nullptr) {
                            bestN = *n;
                        }
                        found = true;
                    }
                }
            }
        }
        if (found) {
            if (outX) *outX = origin.x + dir.x * bestT;
            if (outY) *outY = origin.y + dir.y * bestT;
            if (outZ) *outZ = origin.z + dir.z * bestT;
            if (outNx) *outNx = bestN.x;
            if (outNy) *outNy = bestN.y;
            if (outNz) *outNz = bestN.z;
        }
        return found;
    }

    void setMarioAnim(int animState, f32 speedScale) {
        if (animState < 0 || animState >= 3) animState = 0;
        sMarioAnimRate = (animState == 1) ? (0.6f + speedScale * 0.9f) : 1.0f;
        if (sMarioAnimState != animState) {
            sMarioAnimState = animState;
            if (sMarioAnimCore != nullptr && sMarioClips[animState] != nullptr) {
                sMarioClips[animState]->setFrame(0.0f);
                sMarioAnimCore->setBck(0u, sMarioClips[animState]);
            }
        }
    }

    void setCameraUp(f32 ux, f32 uy, f32 uz) {
        sCamUpX = ux;
        sCamUpY = uy;
        sCamUpZ = uz;
    }

    void begin3DScene(f32 camX, f32 camY, f32 camZ, f32 targetX, f32 targetY, f32 targetZ, f32 fovY, f32 nearZ, f32 farZ) {
        sCamPosX = camX;
        sCamPosY = camY;
        sCamPosZ = camZ;
        Point3d camPos = { camX, camY, camZ };
        Vec upVec = { sCamUpX, sCamUpY, sCamUpZ };
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

    static void drawModelWithMtx(Model* pModel, const Mtx baseMtx) {
        if (pModel == nullptr || pModel->getModelData() == nullptr || pModel->getModelData()->getJointCount() == 0u) return;
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

    static void drawSingleModel(Model* pModel, f32 x, f32 y, f32 z, f32 yawRad) {
        Mtx baseMtx;
        const f32 c = __builtin_cosf(yawRad);
        const f32 s = __builtin_sinf(yawRad);
        baseMtx[0][0] = c;    baseMtx[0][1] = 0.0f; baseMtx[0][2] = s;    baseMtx[0][3] = x;
        baseMtx[1][0] = 0.0f; baseMtx[1][1] = 1.0f; baseMtx[1][2] = 0.0f; baseMtx[1][3] = y;
        baseMtx[2][0] = -s;   baseMtx[2][1] = 0.0f; baseMtx[2][2] = c;    baseMtx[2][3] = z;
        drawModelWithMtx(pModel, baseMtx);
    }

    void drawMenuBackdrop(u32 step) {
        if (sSkyModel != nullptr && sSkyModel->getModelData() != nullptr) {
            drawSingleModel(sSkyModel, sCamPosX, sCamPosY, sCamPosZ, static_cast<f32>(step) * 0.002f);
        }
        if (sMenuShipModel != nullptr && sMenuShipModel->getModelData() != nullptr) {
            drawSingleModel(sMenuShipModel, 0.0f, -200.0f, 0.0f, static_cast<f32>(step) * 0.004f);
        }
    }

    void drawStageAndMario(f32 marioX, f32 marioY, f32 marioZ, f32 upX, f32 upY, f32 upZ, f32 marioYawRad, u32 step) {
        if (sSkyModel != nullptr && sSkyModel->getModelData() != nullptr) {
            drawSingleModel(sSkyModel, sCamPosX, sCamPosY, sCamPosZ, 0.0f);
        }
        for (u32 i = 0; i < sStageModelCount; ++i) {
            if (sStageModels[i] != nullptr && sStageModels[i]->getModelData() != nullptr) {
                drawSingleModel(sStageModels[i], 0.0f, 0.0f, 0.0f, 0.0f);
            }
        }
        const f32 spin = static_cast<f32>(step) * 0.05f;
        for (u32 c = 0; c < sCollectibleCount; ++c) {
            if (sCollectibles[c].collected) continue;
            Model* m = sCollectibles[c].isCoin ? sCoinModel : sStarPieceModel;
            if (m != nullptr && m->getModelData() != nullptr) {
                drawSingleModel(m, sCollectibles[c].x, sCollectibles[c].y, sCollectibles[c].z, spin + static_cast<f32>(c) * 0.5f);
            }
        }
        // MarioShadow is a Wii stencil shadow-volume mesh; skip opaque draw until stencil shadow passes are implemented.
        if (sMarioModel == nullptr || sMarioModel->getModelData() == nullptr) {
            return;
        }
        if (sMarioAnimCore != nullptr && sMarioAnimState >= 0 && sMarioAnimState < 3 && sMarioClips[sMarioAnimState] != nullptr) {
            Clip* clip = sMarioClips[sMarioAnimState];
            if (clip->getFrameMax() > 0) {
                f32 nextFrame = clip->getFrame() + sMarioAnimRate;
                if (nextFrame >= static_cast<f32>(clip->getFrameMax())) {
                    nextFrame = 0.0f;
                }
                clip->setFrame(nextFrame);
            }
        }
        // Build Mario orientation matrix from surface normal (up) and heading (marioYawRad)
        f32 ux = upX, uy = upY, uz = upZ;
        const f32 uLen = __builtin_sqrtf(ux * ux + uy * uy + uz * uz);
        if (uLen > 1.0e-5f) { ux /= uLen; uy /= uLen; uz /= uLen; } else { ux = 0.0f; uy = 1.0f; uz = 0.0f; }
        f32 fx = __builtin_sinf(marioYawRad), fy = 0.0f, fz = __builtin_cosf(marioYawRad);
        const f32 dot = fx * ux + fy * uy + fz * uz;
        fx -= ux * dot; fy -= uy * dot; fz -= uz * dot;
        const f32 fLen = __builtin_sqrtf(fx * fx + fy * fy + fz * fz);
        if (fLen > 1.0e-5f) { fx /= fLen; fy /= fLen; fz /= fLen; } else { fx = 0.0f; fy = 0.0f; fz = 1.0f; }
        // right = up x forward
        const f32 rx = uy * fz - uz * fy;
        const f32 ry = uz * fx - ux * fz;
        const f32 rz = ux * fy - uy * fx;

        Mtx marioMtx;
        marioMtx[0][0] = rx; marioMtx[0][1] = ux; marioMtx[0][2] = fx; marioMtx[0][3] = marioX;
        marioMtx[1][0] = ry; marioMtx[1][1] = uy; marioMtx[1][2] = fy; marioMtx[1][3] = marioY;
        marioMtx[2][0] = rz; marioMtx[2][1] = uz; marioMtx[2][2] = fz; marioMtx[2][3] = marioZ;
        drawModelWithMtx(sMarioModel, marioMtx);
    }

    void checkCollectiblePickups(f32 marioX, f32 marioY, f32 marioZ, s32* outCoins, s32* outStarPieces) {
        constexpr f32 kPickupRadiusSq = 80.0f * 80.0f;
        for (u32 c = 0; c < sCollectibleCount; ++c) {
            if (sCollectibles[c].collected) continue;
            const f32 dx = sCollectibles[c].x - marioX;
            const f32 dy = sCollectibles[c].y - marioY;
            const f32 dz = sCollectibles[c].z - marioZ;
            if (dx * dx + dy * dy + dz * dz < kPickupRadiusSq) {
                sCollectibles[c].collected = true;
                if (sCollectibles[c].isCoin) {
                    if (outCoins != nullptr) ++(*outCoins);
                } else {
                    if (outStarPieces != nullptr) ++(*outStarPieces);
                }
                nebula_log("[SMG2-3D] Collected %s (coins=%d, starPieces=%d)",
                           sCollectibles[c].isCoin ? "Coin" : "StarPiece",
                           outCoins != nullptr ? *outCoins : 0,
                           outStarPieces != nullptr ? *outStarPieces : 0);
            }
        }
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
