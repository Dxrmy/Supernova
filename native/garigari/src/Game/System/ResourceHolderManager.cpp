#include "Game/System/ResourceHolderManager.hpp"
#include "Game/Util/FileUtil.hpp"
#include "Game/Util/SingletonHolder.hpp"
#include <JSystem/JKernel/JKRHeap.hpp>
#include <cstring>
#include <nebula_module.h>

template <>
ResourceHolderManager* SingletonHolder<ResourceHolderManager>::sInstance = nullptr;

ResourceHolderManager::ResourceHolderManager() : mCount(0) {
    for (s32 i = 0; i < kMaxEntries; ++i) {
        mEntries[i].mName = nullptr;
        mEntries[i].mHolder = nullptr;
        mEntries[i].mHeap = nullptr;
    }
}

ResourceHolder* ResourceHolderManager::createAndAdd(const char* pName, JKRHeap* pHeap) {
    if (pName == nullptr) {
        return nullptr;
    }
    if (ResourceHolder* existing = find(pName)) {
        return existing;
    }
    JKRArchive* pArchive = MR::receiveArchive(pName);
    if (pArchive == nullptr) {
        nebula_log("[SMG2] ResourceHolderManager::createAndAdd('%s'): archive not found", pName);
        return nullptr;
    }
    if (mCount >= kMaxEntries) {
        nebula_log("[SMG2] ResourceHolderManager: capacity reached (%d)", kMaxEntries);
        return nullptr;
    }
    // Store archive handle in slot; full ResourceHolder model/motion table parsing is wired in M3.
    Name2Resource& entry = mEntries[mCount++];
    entry.mName = pName;
    entry.mHolder = reinterpret_cast<ResourceHolder*>(pArchive);
    entry.mHeap = pHeap;
    return entry.mHolder;
}

ResourceHolder* ResourceHolderManager::createAndAddStationed(const char* pName) {
    return createAndAdd(pName, JKRHeap::sSystemHeap);
}

ResourceHolder* ResourceHolderManager::find(const char* pName) const {
    if (pName == nullptr) {
        return nullptr;
    }
    for (s32 i = 0; i < mCount; ++i) {
        if (mEntries[i].mName != nullptr && std::strcmp(mEntries[i].mName, pName) == 0) {
            return mEntries[i].mHolder;
        }
    }
    return nullptr;
}

void ResourceHolderManager::removeIfIsEqualHeap(JKRHeap* pHeap) {
    s32 writeIdx = 0;
    for (s32 i = 0; i < mCount; ++i) {
        if (mEntries[i].mHeap != pHeap) {
            if (writeIdx != i) {
                mEntries[writeIdx] = mEntries[i];
            }
            writeIdx++;
        }
    }
    for (s32 i = writeIdx; i < mCount; ++i) {
        mEntries[i].mName = nullptr;
        mEntries[i].mHolder = nullptr;
        mEntries[i].mHeap = nullptr;
    }
    mCount = writeIdx;
}
