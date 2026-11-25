#pragma once

#include <revolution/types.h>

class SaveRequest {
public:
    enum Type {
        Type_None = 0,
        Type_Write = 1,
        Type_Read = 2,
    };

    enum Result {
        Result_Done = 0,
        Result_Pending = 1,
        Result_NoFile = 2,
        Result_Corrupted = 3,
        Result_Failed = 4,
    };

    SaveRequest();

    void init();
    bool isDone() const;
    void wait() const;
    Result getResult() const;
    void start();

    const char* setWriteSeq(const char* pName, const void* pBuf, u32 size);
    const char* setReadSeq(const char* pName, void* pBuf, u32 capacity, u32* pLength);

private:
    Type mType;
    const char* mName;
    void* mBuffer;
    u32 mSize;
    u32* mReadLength;
    volatile s32 mStatus;
};
