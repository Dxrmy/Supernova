#pragma once

#include "JSystem/JKernel/JKRArchive.hpp"

enum JKRMemBreakFlag { JKR_MEM_BREAK_FLAG_0 = 0, JKR_MEM_BREAK_FLAG_1 = 1 };

class JKRMemArchive : public JKRArchive {
public:
    JKRMemArchive();
    JKRMemArchive(long, EMountDirection);
    virtual ~JKRMemArchive();

    virtual void removeResourceAll() override;
    virtual bool removeResource(void*) override;
    virtual u32 getExpandedResSize(const void*) const override;
    virtual void* fetchResource(SDIFileEntry*, u32*) override;
    virtual void* fetchResource(void*, u32, SDIFileEntry*, u32*) override;

    void fixedInit(long);
    bool mountFixed(void*, JKRMemBreakFlag);
    bool open(long, EMountDirection);
    bool open(void*, u32, JKRMemBreakFlag);
    static s32 fetchResource_subroutine(unsigned char*, u32, unsigned char*, u32, int);

    RarcHeader* mHeader;
    u8* mFileDataStart;
    bool _6C;
    u8 _6D[3];
};
