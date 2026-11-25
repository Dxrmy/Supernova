#pragma once

#include <revolution/types.h>

class JKRHeap;
class ResourceHolder;

class ResourceHolderManager {
public:
    static constexpr s32 kMaxEntries = 256;

    struct Name2Resource {
        const char* mName;
        ResourceHolder* mHolder;
        JKRHeap* mHeap;
    };

    ResourceHolderManager();

    ResourceHolder* createAndAdd(const char* pName, JKRHeap* pHeap);
    ResourceHolder* createAndAddStationed(const char* pName);
    ResourceHolder* find(const char* pName) const;
    void removeIfIsEqualHeap(JKRHeap* pHeap);

private:
    Name2Resource mEntries[kMaxEntries];
    s32 mCount;
};
