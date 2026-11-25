// Bridge between Petari's native Ui/LayoutManager/FrameList stack and SMG2's SB4E01 game.pak assets.

#include "Game/System/Arenas.hpp"
#include "Game/System/AssetFolder.hpp"
#include "Game/System/GameAssets.hpp"
#include "Game/System/GameSystemFontHolder.hpp"
#include "Game/System/LayoutHolder.hpp"
#include "Game/System/NebulaFrameClock.hpp"
#include "Game/Util/FileUtil.hpp"
#include "Game/Util/ScreenUtil.hpp"
#include "Game/Util/StringUtil.hpp"
#include "Game/Screen/LayoutActor.hpp"
#include "Game/Screen/LayoutManager.hpp"
#include "Game/Screen/LayoutPaneCtrl.hpp"
#include <Game/Ui/ResFont.hpp>
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

    const char16_t* getLayoutMessageDirect(const char* /*pMessageId*/) {
        return nullptr;
    }

    const char16_t* getGameMessageDirect(const char* /*pMessageId*/) {
        return nullptr;
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
    bool isRootAnimStopped(const LayoutManager* pMgr, u32 layer) {
        if (pMgr == nullptr || pMgr->mLayout == nullptr || pMgr->mLayout->mpRootPane == nullptr) return true;
        LayoutPaneCtrl* pCtrl = pMgr->getPaneCtrl(pMgr->mLayout->mpRootPane->mName);
        return pCtrl == nullptr || pCtrl->isAnimStopped(layer);
    }
}  // namespace Smg2Ui

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
