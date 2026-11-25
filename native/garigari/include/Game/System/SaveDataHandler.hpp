#pragma once

#include "Game/System/SaveRequest.hpp"
#include <revolution/types.h>

class GameDataHolder;

class SaveDataHandler {
public:
    static constexpr u32 kSaveMagic = 0x5653424Eu;  // "NBSV"
    static constexpr u32 kSaveVersion = 4u;
    static constexpr u32 kHeaderSize = 16u;
    static constexpr u32 kRowNameSize = 16u;
    static constexpr u32 kRowSize = kRowNameSize + 4u;
    static constexpr u32 kUserFiles = 6u;
    static constexpr u32 kKindCount = 3u;
    static constexpr u32 kSlotCount = kUserFiles * kKindCount;  // 18

    struct FileSlot {
        u8* mData;
        u32 mSize;
        u32 mRoom;
    };

    SaveDataHandler();
    ~SaveDataHandler();

    void update();
    bool isDone() const;
    s32 getLastResultCode() const { return mLastResultCode; }

    bool requestLoadSaveData(const char* pFileName = nullptr);
    bool requestSaveSaveData();
    bool trySave(bool* pOutOk = nullptr);
    bool loadFromExistingOrInit(GameDataHolder* pTargetHolder);
    bool storeGameData(s32 userSlot, const GameDataHolder* pSourceHolder);
    bool restoreGameData(s32 userSlot, GameDataHolder* pTargetHolder) const;
    void initializeAllUserFileMemory();

    static u32 calcCrc32(const u8* pData, u32 size);

private:
    bool tryLoadIndex(int index);
    bool parseNativeNbsv(const u8* pBytes, u32 length, const char* pName);
    u32 serializeNativeNbsv(u8* pOut, u32 capacity) const;
    static int slotIndex(s32 kind, s32 user);

    SaveRequest mRequest;
    FileSlot mFiles[kSlotCount];
    u8* mBuffer;
    u32 mBufferRoom;
    u32 mReadLength;
    s32 mLastResultCode;
    bool mInitialized;
};
