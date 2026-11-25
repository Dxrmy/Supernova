// Native runtime implementations for RVL_SDK / JSystem / MR archive & OS primitives used by Garigari.
#include <nebula_game_api.h>
#include <nebula_module.h>

#include "JSystem/JKernel/JKRDisposer.hpp"
#include "JSystem/JKernel/JKRFileFinder.hpp"
#include "JSystem/JKernel/JKRFileLoader.hpp"
#include "JSystem/JKernel/JKRHeap.hpp"
#include "JSystem/JKernel/JKRMemArchive.hpp"
#include "JSystem/JKernel/JKRExpHeap.hpp"
#include "JSystem/JKernel/JKRSolidHeap.hpp"
#include "JSystem/JUtility/JUTNameTab.hpp"
#include "Game/System/GalaxyStatusAccessor.hpp"
#include "Game/System/ScenarioDataParser.hpp"
#include "Game/Util/FileUtil.hpp"
#include "Game/Util/StringUtil.hpp"
#include <revolution/dvd.h>
#include <revolution/os.h>
#include <revolution/sc.h>
#include <revolution/vi.h>
#include <cstring>
#include <cstdio>
#include <stdlib.h>

// ---- OS / SC / VI ----

extern "C" {

int __stdio_common_vsprintf(unsigned long long, char* buf, size_t count, const char* fmt, _locale_t, va_list args) {
    return nebula_vsnprintf(buf, count, fmt, args);
}

int tolower(int c) {
    return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
}

vu32 OS_BUS_CLOCK_SPEED = 243000000u;
volatile PPCWGPipe GXWGFifo{};

void OSReport(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    nebula_vlog(fmt, args);
    va_end(args);
}

void OSVReport(const char* fmt, va_list args) {
    nebula_vlog(fmt, args);
}

void OSPanic(const char* file, int line, const char* fmt, ...) {
    char buf[512];
    va_list args;
    va_start(args, fmt);
    nebula_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    nebula_fatal("OSPanic %s:%d: %s", file != nullptr ? file : "?", line, buf);
}

OSTick OSGetTick(void) {
    const NebulaHostServices* host = nebula_host();
    if (host == nullptr || host->clock_ns == nullptr) {
        return 0;
    }
    const u64 ns = host->clock_ns(host->user);
    return static_cast<OSTick>((ns * (OS_TIMER_CLOCK / 1000ull)) / 1000000ull);
}

OSTime OSGetTime(void) {
    const NebulaHostServices* host = nebula_host();
    if (host == nullptr || host->calendar_ns == nullptr) {
        return 0;
    }
    return static_cast<OSTime>((host->calendar_ns(host->user) * (OS_TIMER_CLOCK / 1000ll)) / 1000000ll);
}

void OSRegisterVersion(const char*) {}
u32 OSGetConsoleType(void) { return 0; }
u32 OSGetSoundMode(void) { return 1; }
void OSSetSoundMode(u32) {}
BOOL OSDisableInterrupts(void) { return TRUE; }
BOOL OSRestoreInterrupts(BOOL prev) { return prev; }
void OSInitMutex(OSMutex*) {}
void OSLockMutex(OSMutex*) {}
void OSUnlockMutex(OSMutex*) {}
void DCFlushRange(void*, u32) {}
void DCInvalidateRange(void*, u32) {}
void DCStoreRange(void*, u32) {}
void ICInvalidateRange(void*, u32) {}

u8 SCGetLanguage(void) {
    const NebulaHostServices* host = nebula_host();
    if (host != nullptr && host->system_setting != nullptr) {
        // NEBULA_SETTING_LANGUAGE returns GameLanguage index; default English (1) if unspecified.
        return 1;
    }
    return 1;
}

u32 VIGetTvFormat(void) {
    return VI_NTSC;
}

BOOL VIEnableDimming(BOOL) {
    return TRUE;
}

namespace {
    s32 sStageDataEntryNum = -1;
    char sDvdDirNameBuf[256]{};
}  // namespace

BOOL DVDOpenDir(const char* dirName, DVDDir* dir) {
    if (dir == nullptr || dirName == nullptr) {
        return FALSE;
    }
    const NebulaHostServices* host = nebula_host();
    if (host == nullptr || host->asset_find == nullptr) {
        return FALSE;
    }
    NebulaAssetEntry entry{};
    if (host->asset_find(host->user, dirName, &entry) == 0 || entry.folder == 0) {
        return FALSE;
    }
    if (nebula_stricmp(dirName, "/StageData") == 0 || nebula_stricmp(dirName, "StageData") == 0) {
        sStageDataEntryNum = entry.index;
    }
    dir->entryNum = static_cast<u32>(entry.index);
    dir->location = 0;
    dir->next = entry.children;
    return TRUE;
}

BOOL DVDReadDir(DVDDir* dir, DVDDirEntry* dirent) {
    if (dir == nullptr || dirent == nullptr) {
        return FALSE;
    }
    const NebulaHostServices* host = nebula_host();
    if (host == nullptr || host->asset_child == nullptr) {
        return FALSE;
    }
    const bool isStageData = (static_cast<s32>(dir->entryNum) == sStageDataEntryNum);
    NebulaAssetEntry child{};
    while (dir->location < dir->next) {
        const u32 idx = dir->location++;
        if (host->asset_child(host->user, static_cast<s32>(dir->entryNum), idx, &child) == 0) {
            break;
        }
        if (child.name == nullptr) {
            continue;
        }
        const size_t nameLen = std::strlen(child.name);
        if (isStageData) {
            // Nebula flattens StageData/<Stage>/<Stage>Scenario.arc into /StageData/<Stage>Scenario.
            if (child.folder == 0 || nameLen <= 8 || nebula_stricmp(child.name + (nameLen - 8), "Scenario") != 0) {
                continue;
            }
            const size_t trimmed = (nameLen - 8) < sizeof(sDvdDirNameBuf) - 1 ? (nameLen - 8) : sizeof(sDvdDirNameBuf) - 1;
            std::memcpy(sDvdDirNameBuf, child.name, trimmed);
            sDvdDirNameBuf[trimmed] = '\0';
            dirent->entryNum = static_cast<u32>(child.index);
            dirent->isDir = TRUE;
            dirent->name = sDvdDirNameBuf;
            return TRUE;
        }
        const size_t copyLen = nameLen < sizeof(sDvdDirNameBuf) - 1 ? nameLen : sizeof(sDvdDirNameBuf) - 1;
        std::memcpy(sDvdDirNameBuf, child.name, copyLen);
        sDvdDirNameBuf[copyLen] = '\0';
        dirent->entryNum = static_cast<u32>(child.index);
        dirent->isDir = child.folder != 0 ? TRUE : FALSE;
        dirent->name = sDvdDirNameBuf;
        return TRUE;
    }
    return FALSE;
}

BOOL DVDCloseDir(DVDDir* dir) {
    if (dir != nullptr) {
        dir->location = dir->next;
    }
    return TRUE;
}

}  // extern "C"

// ---- JSystem JSUList / JKRDisposer / JKRHeap ----

JSUPtrLink::JSUPtrLink(void* pData)
    : mData(pData), mPtrList(nullptr), mPrev(nullptr), mNext(nullptr) {
}

JSUPtrLink::~JSUPtrLink() {
    if (mPtrList != nullptr) {
        mPtrList->remove(this);
    }
}

JSUPtrList::JSUPtrList(bool doInitialize) : mHead(nullptr), mTail(nullptr), mNodeCount(0) {
    (void)doInitialize;
}

JSUPtrList::~JSUPtrList() {
    JSUPtrLink* cur = mHead;
    while (cur != nullptr) {
        JSUPtrLink* next = cur->mNext;
        cur->mPtrList = nullptr;
        cur->mPrev = nullptr;
        cur->mNext = nullptr;
        cur = next;
    }
}

void JSUPtrList::initiate() {
    mHead = nullptr;
    mTail = nullptr;
    mNodeCount = 0;
}

void JSUPtrList::setFirst(JSUPtrLink* link) {
    link->mPtrList = this;
    link->mPrev = nullptr;
    link->mNext = nullptr;
    mHead = link;
    mTail = link;
    mNodeCount = 1;
}

bool JSUPtrList::append(JSUPtrLink* link) {
    if (link->mPtrList != nullptr && !link->mPtrList->remove(link)) {
        return false;
    }
    if (mNodeCount == 0) {
        setFirst(link);
        return true;
    }
    link->mPtrList = this;
    link->mPrev = mTail;
    link->mNext = nullptr;
    mTail->mNext = link;
    mTail = link;
    mNodeCount++;
    return true;
}

bool JSUPtrList::prepend(JSUPtrLink* link) {
    if (link->mPtrList != nullptr && !link->mPtrList->remove(link)) {
        return false;
    }
    if (mNodeCount == 0) {
        setFirst(link);
        return true;
    }
    link->mPtrList = this;
    link->mPrev = nullptr;
    link->mNext = mHead;
    mHead->mPrev = link;
    mHead = link;
    mNodeCount++;
    return true;
}

bool JSUPtrList::remove(JSUPtrLink* link) {
    if (link->mPtrList != this) {
        return false;
    }
    if (mNodeCount == 1) {
        mHead = nullptr;
        mTail = nullptr;
    } else if (link == mHead) {
        link->mNext->mPrev = nullptr;
        mHead = link->mNext;
    } else if (link == mTail) {
        link->mPrev->mNext = nullptr;
        mTail = link->mPrev;
    } else {
        link->mPrev->mNext = link->mNext;
        link->mNext->mPrev = link->mPrev;
    }
    link->mPtrList = nullptr;
    link->mPrev = nullptr;
    link->mNext = nullptr;
    mNodeCount--;
    return true;
}

bool JSUPtrList::insert(JSUPtrLink* before, JSUPtrLink* link) {
    if (before == nullptr) {
        return append(link);
    }
    if (before->mPtrList != this) {
        return false;
    }
    if (before == mHead) {
        return prepend(link);
    }
    if (link->mPtrList != nullptr && !link->mPtrList->remove(link)) {
        return false;
    }
    link->mPtrList = this;
    link->mPrev = before->mPrev;
    link->mNext = before;
    before->mPrev->mNext = link;
    before->mPrev = link;
    mNodeCount++;
    return true;
}

JKRDisposer::JKRDisposer() : mHeap(nullptr), mLink(this) {
}

JKRDisposer::~JKRDisposer() {
}

class NativeJKRHeap final : public JKRHeap {
public:
    NativeJKRHeap() : JKRHeap(nullptr, 0, nullptr, false) {}
    u32 getHeapType() override { return 0x4E415456; }
    bool check() override { return true; }
    bool dump() override { return true; }
    void do_destroy() override {}
    void* do_alloc(u32 size, int) override { return malloc(size == 0 ? 1 : size); }
    void do_free(void* p) override { free(p); }
    void do_freeAll() override {}
    void do_freeTail() override {}
    void do_fillFreeArea() override {}
    s32 do_resize(void* p, u32 size) override { (void)p; (void)size; return -1; }
    s32 do_getSize(void*) override { return 0; }
    s32 do_getFreeSize() override { return 0x10000000; }
    void* do_getMaxFreeBlock() override { return nullptr; }
    s32 do_getTotalFreeSize() override { return 0x10000000; }
};

JKRHeap* JKRHeap::sSystemHeap = nullptr;
JKRHeap* JKRHeap::sCurrentHeap = nullptr;
JKRHeap* JKRHeap::sRootHeap = nullptr;
JKRErrorHandler JKRHeap::mErrorHandler = nullptr;

JKRHeap::JKRHeap(void* start, u32 size, JKRHeap* parent, bool err)
    : mChildTree(this), mDisposerList(true) {
    (void)parent;
    (void)err;
    mStart = static_cast<u8*>(start);
    mEnd = mStart != nullptr ? mStart + size : nullptr;
    mSize = size;
    if (sRootHeap == nullptr) {
        sRootHeap = this;
        sSystemHeap = this;
        sCurrentHeap = this;
    }
}

JKRHeap::~JKRHeap() {}
void JKRHeap::callAllDisposer() {}
bool JKRHeap::dump_sort() { return true; }
s32 JKRHeap::do_changeGroupID(u8) { return 0; }
u8 JKRHeap::do_getCurrentGroupId() { return 0; }
void JKRHeap::state_register(TState*, u32) const {}
bool JKRHeap::state_compare(const TState&, const TState&) const { return true; }
void JKRHeap::state_dump(const TState&) const {}
void* JKRHeap::alloc(u32 size, int align) { return do_alloc(size, align); }
void JKRHeap::free(void* p) { do_free(p); }
JKRHeap* JKRHeap::becomeSystemHeap() { JKRHeap* prev = sSystemHeap; sSystemHeap = this; return prev; }
JKRHeap* JKRHeap::becomeCurrentHeap() { JKRHeap* prev = sCurrentHeap; sCurrentHeap = this; return prev; }
void JKRHeap::destroy() {}
void JKRHeap::freeAll() { do_freeAll(); }
void JKRHeap::freeTail() { do_freeTail(); }
s32 JKRHeap::resize(void* p, u32 s) { return do_resize(p, s); }
s32 JKRHeap::getFreeSize() { return do_getFreeSize(); }
void* JKRHeap::getMaxFreeBlock() { return do_getMaxFreeBlock(); }
s32 JKRHeap::getTotalFreeSize() { return do_getTotalFreeSize(); }
JKRHeap* JKRHeap::find(void*) const { return const_cast<JKRHeap*>(this); }
JKRHeap* JKRHeap::findAllHeap(void*) const { return sCurrentHeap; }
void* JKRHeap::alloc(u32 size, int, JKRHeap*) { return malloc(size == 0 ? 1 : size); }
void JKRHeap::free(void* p, JKRHeap*) { ::free(p); }
void JKRHeap::fillMemory(u8* dst, u32 size, u8 val) { if (dst != nullptr && size > 0) std::memset(dst, val, size); }
void JKRHeap::fillMemory(void* dst, u32 size, u8 val) { fillMemory(static_cast<u8*>(dst), size, val); }
s32 JKRHeap::resize(void*, u32, JKRHeap*) { return -1; }
s32 JKRHeap::getSize(void*, JKRHeap*) { return 0; }
JKRHeap* JKRHeap::findFromRoot(void*) { return sRootHeap; }
JKRErrorHandler JKRHeap::setErrorHandler(JKRErrorHandler handler) {
    JKRErrorHandler prev = mErrorHandler;
    mErrorHandler = handler;
    return prev;
}

void JKRArchive::setExpandSize(SDIFileEntry*, u32) {}
u32 JKRArchive::getExpandSize(SDIFileEntry*) const { return 0; }

void* operator new(size_t size, JKRHeap*) { return malloc(size == 0 ? 1 : size); }
void* operator new(size_t size, JKRHeap*, int) { return malloc(size == 0 ? 1 : size); }
void* operator new[](size_t size, JKRHeap*, int) { return malloc(size == 0 ? 1 : size); }

namespace JGeometry {
    void mulInternal(const f32* vec1, const f32* vec2, f32* dst) {
        dst[0] = vec1[0] * vec2[0];
        dst[1] = vec1[1] * vec2[1];
        dst[2] = vec1[2] * vec2[2];
    }
}  // namespace JGeometry

namespace {
    NativeJKRHeap* sNativeHeap = nullptr;
    void ensureNativeHeap() {
        if (sNativeHeap == nullptr) {
            sNativeHeap = new NativeJKRHeap();
        }
    }
}  // namespace

// ---- Native game.pak-backed JKRFileLoader / JKRArchive / JKRMemArchive ----

JSUList<JKRFileLoader> JKRFileLoader::sVolumeList(false);
JKRFileLoader* JKRFileLoader::sCurrentVolume = nullptr;
OSMutex JKRFileLoader::sVolumeListMutex{};

JKRFileLoader::JKRFileLoader()
    : mLoaderLink(this), mLoaderName(nullptr), mLoaderType(0), mIsMounted(false), _34(0) {
}

JKRFileLoader::~JKRFileLoader() {
    free(mLoaderName);
}

void JKRFileLoader::unmount() {
    mIsMounted = false;
}

void* JKRFileLoader::getGlbResource(const char*, JKRFileLoader*) {
    return nullptr;
}

void JKRFileLoader::initializeVolumeList() {}
void JKRFileLoader::prependVolumeList(JSULink<JKRFileLoader>*) {}
void JKRFileLoader::removeVolumeList(JSULink<JKRFileLoader>*) {}

JKRArchive::JKRArchive() : JKRFileLoader() {}
JKRArchive::JKRArchive(s32, EMountMode) : JKRFileLoader() {}
JKRArchive::~JKRArchive() {}
bool JKRArchive::becomeCurrent(const char*) { sCurrentVolume = this; return true; }
void* JKRArchive::getResource(const char*) { return nullptr; }
void* JKRArchive::getResource(u32, const char*) { return nullptr; }
u32 JKRArchive::readResource(void*, u32, const char*) { return 0; }
u32 JKRArchive::readResource(void*, u32, u32, const char*) { return 0; }
void JKRArchive::removeResourceAll() {}
bool JKRArchive::removeResource(void*) { return false; }
bool JKRArchive::detachResource(void*) { return false; }
s32 JKRArchive::getResSize(const void*) const { return -1; }
s32 JKRArchive::countFile(const char*) const { return 0; }
JKRArcFinder* JKRArchive::getFirstFile(const char*) const { return nullptr; }
u32 JKRArchive::getExpandedResSize(const void*) const { return 0; }

JKRMemArchive::JKRMemArchive()
    : JKRArchive(), mHeader(nullptr), mFileDataStart(nullptr), _6C(false) {}

JKRMemArchive::JKRMemArchive(long, EMountDirection)
    : JKRArchive(), mHeader(nullptr), mFileDataStart(nullptr), _6C(false) {}

JKRMemArchive::~JKRMemArchive() {}

void JKRMemArchive::removeResourceAll() {}
bool JKRMemArchive::removeResource(void*) { return false; }
u32 JKRMemArchive::getExpandedResSize(const void*) const { return 0; }
void* JKRMemArchive::fetchResource(SDIFileEntry*, u32*) { return nullptr; }
void* JKRMemArchive::fetchResource(void*, u32, SDIFileEntry*, u32*) { return nullptr; }
void JKRMemArchive::fixedInit(long) {}
bool JKRMemArchive::mountFixed(void*, JKRMemBreakFlag) { return false; }
bool JKRMemArchive::open(long, EMountDirection) { return false; }
bool JKRMemArchive::open(void*, u32, JKRMemBreakFlag) { return false; }
s32 JKRMemArchive::fetchResource_subroutine(unsigned char*, u32, unsigned char*, u32, int) { return 0; }

namespace {

bool isArchiveName(const char* part, size_t len) {
    if (len <= 4) {
        return false;
    }
    return nebula_stricmp(part + (len - 4), ".arc") == 0 || nebula_stricmp(part + (len - 4), ".szs") == 0;
}

bool sameCaseInsensitiveLen(const char* a, size_t aLen, const char* b) {
    if (a == nullptr || b == nullptr) {
        return false;
    }
    for (size_t i = 0; i < aLen; ++i) {
        if (b[i] == '\0' || tolower(static_cast<unsigned char>(a[i])) != tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return b[aLen] == '\0';
}

// Mirrors galaxy::assets::store_path (runtime/src/asset_store.cpp):
// strips ".arc"/".szs" extensions and flattens StageData/<Stage>/<Archive>.arc -> StageData/<Archive>.
void normalizeStorePath(char* dst, size_t dstSize, const char* src) {
    if (dst == nullptr || dstSize == 0) {
        return;
    }
    dst[0] = '\0';
    if (src == nullptr || src[0] == '\0') {
        return;
    }
    size_t outLen = 0;
    size_t pos = 0;
    while (src[pos] == '/') {
        if (outLen + 1 < dstSize) {
            dst[outLen++] = '/';
        }
        ++pos;
    }
    if (src[pos] == '\0') {
        dst[outLen] = '\0';
        return;
    }

    const char* parts[16]{};
    size_t partLens[16]{};
    size_t partCount = 0;
    while (partCount < 16) {
        const char* seg = src + pos;
        const char* slash = std::strchr(seg, '/');
        if (slash == nullptr) {
            parts[partCount] = seg;
            partLens[partCount] = std::strlen(seg);
            ++partCount;
            break;
        }
        parts[partCount] = seg;
        partLens[partCount] = static_cast<size_t>(slash - seg);
        ++partCount;
        pos += static_cast<size_t>(slash - seg) + 1;
    }

    char extBuf0[128]{};
    char extBuf2[128]{};
    bool isStageFolder = false;
    if (partCount > 2 && sameCaseInsensitiveLen(parts[0], partLens[0], "StageData")) {
        const size_t l1 = partLens[1] < sizeof(extBuf0) - 1 ? partLens[1] : sizeof(extBuf0) - 1;
        std::memcpy(extBuf0, parts[1], l1);
        extBuf0[l1] = '\0';
        const size_t l2 = partLens[2] < sizeof(extBuf2) - 1 ? partLens[2] : sizeof(extBuf2) - 1;
        std::memcpy(extBuf2, parts[2], l2);
        extBuf2[l2] = '\0';
        isStageFolder = !isArchiveName(extBuf0, l1) && isArchiveName(extBuf2, l2);
    }

    bool firstWritten = true;
    for (size_t i = 0; i < partCount; ++i) {
        if (i == 1 && isStageFolder) {
            continue;
        }
        if (!firstWritten && outLen + 1 < dstSize) {
            dst[outLen++] = '/';
        }
        firstWritten = false;
        size_t len = partLens[i];
        char tmp[256]{};
        const size_t copyLen = len < sizeof(tmp) - 1 ? len : sizeof(tmp) - 1;
        std::memcpy(tmp, parts[i], copyLen);
        tmp[copyLen] = '\0';
        if (isArchiveName(tmp, copyLen)) {
            len -= 4;
        }
        for (size_t k = 0; k < len && outLen + 1 < dstSize; ++k) {
            dst[outLen++] = parts[i][k];
        }
    }
    dst[outLen] = '\0';
}

bool findChildRecursive(const NebulaHostServices* host, s32 folderIndex, const char* fileName, NebulaAssetEntry* out) {
    if (host == nullptr || host->asset_child == nullptr || folderIndex < 0 || fileName == nullptr) {
        return false;
    }
    NebulaAssetEntry child{};
    for (u32 i = 0; host->asset_child(host->user, folderIndex, i, &child) != 0; ++i) {
        if (child.folder == 0 && child.name != nullptr && nebula_stricmp(child.name, fileName) == 0) {
            if (out != nullptr) {
                *out = child;
            }
            return true;
        }
    }
    for (u32 i = 0; host->asset_child(host->user, folderIndex, i, &child) != 0; ++i) {
        if (child.folder != 0 && findChildRecursive(host, child.index, fileName, out)) {
            return true;
        }
    }
    return false;
}

}  // namespace

// Native archive wrapper backed directly by NebulaHostServices::asset_find / asset_map.
class NativePakArchive final : public JKRMemArchive {
public:
    NativePakArchive(const char* basePath, s32 entryIndex) : mEntryIndex(entryIndex) {
        const size_t len = std::strlen(basePath);
        mLoaderName = static_cast<char*>(malloc(len + 1));
        if (mLoaderName != nullptr) {
            std::memcpy(mLoaderName, basePath, len + 1);
        }
        mIsMounted = true;
    }

    void* getResource(unsigned long, const char* pSubPath) override { return getResource(pSubPath); }
    u32 readResource(void*, unsigned long, const char*) override { return 0; }
    u32 readResource(void*, unsigned long, unsigned long, const char*) override { return 0; }

    void* getResource(const char* pSubPath) override {
        const NebulaHostServices* host = nebula_host();
        if (host == nullptr || host->asset_find == nullptr || host->asset_map == nullptr || mLoaderName == nullptr || pSubPath == nullptr) {
            return nullptr;
        }
        char full[512];
        const char* sub = pSubPath[0] == '/' ? pSubPath + 1 : pSubPath;
        nebula_snprintf(full, sizeof(full), "%s/%s", mLoaderName, sub);
        NebulaAssetEntry entry{};
        bool found = host->asset_find(host->user, full, &entry) != 0 && entry.folder == 0;
        if (!found) {
            const char* slash = std::strrchr(sub, '/');
            const char* baseName = slash != nullptr ? slash + 1 : sub;
            found = findChildRecursive(host, mEntryIndex, baseName, &entry) && entry.folder == 0;
        }
        if (!found) {
            nebula_log("[SMG2-VFS] archive missing resource: %s", full);
            return nullptr;
        }
        const void* mapped = host->asset_map(host->user, entry.index);
        if (mapped == nullptr && entry.length > 0 && host->asset_read != nullptr) {
            void* buf = malloc(entry.length);
            if (buf != nullptr && host->asset_read(host->user, entry.index, 0, entry.length, buf) == static_cast<s32>(entry.length)) {
                return buf;
            }
            free(buf);
            return nullptr;
        }
        return const_cast<void*>(mapped);
    }

private:
    s32 mEntryIndex = -1;
};

// ---- Minimal MR::FileUtil / MR::StringUtil helpers used by GameSystemDataHolder & ScenarioDataParser ----

namespace MR {
    JKRMemArchive* receiveArchive(const char* pFilePath) {
        ensureNativeHeap();
        const NebulaHostServices* host = nebula_host();
        if (host == nullptr || host->asset_find == nullptr || pFilePath == nullptr) {
            return nullptr;
        }
        const char* matchedPath = pFilePath;
        NebulaAssetEntry entry{};
        char normalized[512]{};
        if (host->asset_find(host->user, pFilePath, &entry) == 0) {
            normalizeStorePath(normalized, sizeof(normalized), pFilePath);
            if (normalized[0] == '\0' || host->asset_find(host->user, normalized, &entry) == 0) {
                nebula_log("[SMG2-VFS] receiveArchive not found in game.pak: %s (normalized=%s)", pFilePath, normalized);
                return nullptr;
            }
            matchedPath = normalized;
        }
        nebula_log("[SMG2-VFS] mounted archive: %s (index=%d, children=%u)", matchedPath, entry.index, entry.children);
        return new NativePakArchive(matchedPath, entry.index);
    }

    bool isFileExist(const char* pFilePath, bool) {
        const NebulaHostServices* host = nebula_host();
        if (host == nullptr || host->asset_find == nullptr || pFilePath == nullptr) {
            return false;
        }
        NebulaAssetEntry entry{};
        if (host->asset_find(host->user, pFilePath, &entry) != 0) {
            return true;
        }
        char normalized[512]{};
        normalizeStorePath(normalized, sizeof(normalized), pFilePath);
        return normalized[0] != '\0' && host->asset_find(host->user, normalized, &entry) != 0;
    }

    void makeScenarioArchiveFileName(char* pDst, u32 size, const char* pStageName) {
        if (pDst == nullptr || size == 0) {
            return;
        }
        nebula_snprintf(pDst, size, "/StageData/%s/%sScenario.arc", pStageName != nullptr ? pStageName : "", pStageName != nullptr ? pStageName : "");
    }

    void copyString(char* pDst, const char* pSrc, u32 maxLen);

    char* removeExtensionString(char* pDst, u32 maxLen, const char* pSrc) {
        copyString(pDst, pSrc, maxLen);
        char* dot = std::strrchr(pDst, '.');
        if (dot != nullptr) {
            *dot = '\0';
        }
        return std::strrchr(pDst, '/');
    }

    void extractString(char* pDst, const char* pSrc, u32 num, u32) {
        if (pDst == nullptr || num == 0) {
            return;
        }
        if (pSrc == nullptr) {
            pDst[0] = '\0';
            return;
        }
        u32 i = 0;
        for (; i < num && pSrc[i] != '\0'; ++i) {
            pDst[i] = pSrc[i];
        }
        pDst[i] = '\0';
    }
}  // namespace MR

// ---- GalaxyStatusAccessor (bridges ScenarioData without missing GameDataFunction.cpp) ----

GalaxyStatusAccessor::GalaxyStatusAccessor(const ScenarioData* pScenarioData)
    : mScenarioData(pScenarioData) {
}

const char* GalaxyStatusAccessor::getName() const {
    return mScenarioData != nullptr ? mScenarioData->mGalaxyName : "";
}

s32 GalaxyStatusAccessor::getPowerStarNum() const {
    return mScenarioData != nullptr ? mScenarioData->getPowerStarNum() : 0;
}

bool GalaxyStatusAccessor::isCompletedNoGreen() const {
    // TODO(Garigari): GameDataFunction.cpp is not yet decompiled upstream.
    return false;
}

bool GalaxyStatusAccessor::isCompletedWithGreen() const {
    // TODO(Garigari): GameDataFunction.cpp is not yet decompiled upstream.
    return false;
}

s32 GalaxyStatusAccessor::getWorldNo() const {
    return mScenarioData != nullptr ? static_cast<s32>(mScenarioData->getWorldNo()) : 0;
}
