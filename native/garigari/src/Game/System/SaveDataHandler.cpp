#include "Game/System/SaveDataHandler.hpp"
#include "Game/System/GameDataHolder.hpp"
#include <cstring>
#include <nebula_module.h>

namespace {
    const char cPrimarySaveName[] = "Nebula.sav";
    const char* const cReadCandidates[] = {
        "Nebula.sav",
        "Nebula.sav.1",
        "Nebula.sav.2",
        "GameData.bin",
        "GameData.bin.1",
        "GameData.bin.2",
    };
    const char* const cKindNames[] = {"mario", "luigi", "config"};
    const u32 cDefaultSlotRoom = 0x2000u;
    const u32 cBufferCapacity = 0x40000u;  // 256 KiB container buffer

    u32 readU32LE(const u8* pSrc) {
        u32 value = 0;
        std::memcpy(&value, pSrc, sizeof(value));
        return value;
    }

    void writeU32LE(u8* pDst, u32 value) {
        std::memcpy(pDst, &value, sizeof(value));
    }

    bool parseRowName(const char* pName, s32* pKind, s32* pUser) {
        for (s32 kind = 0; kind < static_cast<s32>(SaveDataHandler::kKindCount); ++kind) {
            const u32 prefixLen = static_cast<u32>(std::strlen(cKindNames[kind]));
            if (std::strncmp(pName, cKindNames[kind], prefixLen) != 0) {
                continue;
            }
            if (pName[prefixLen] == '\0' || pName[prefixLen + 1] != '\0') {
                continue;
            }
            const s32 user = static_cast<s32>(pName[prefixLen] - '0');
            if (user < 1 || user > static_cast<s32>(SaveDataHandler::kUserFiles)) {
                return false;
            }
            *pKind = kind;
            *pUser = user;
            return true;
        }
        return false;
    }

    void makeRowName(char (&rOut)[SaveDataHandler::kRowNameSize], s32 kind, s32 user) {
        std::memset(rOut, 0, sizeof(rOut));
        nebula_snprintf(rOut, sizeof(rOut), "%s%1d", cKindNames[kind], user);
    }
}  // namespace

u32 SaveDataHandler::calcCrc32(const u8* pData, u32 size) {
    u32 crc = 0xFFFFFFFFu;
    for (u32 i = 0; i < size; ++i) {
        crc ^= pData[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1u) != 0u ? (0xEDB88320u ^ (crc >> 1u)) : (crc >> 1u);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

int SaveDataHandler::slotIndex(s32 kind, s32 user) {
    return kind * static_cast<s32>(kUserFiles) + (user - 1);
}

SaveDataHandler::SaveDataHandler()
    : mRequest(),
      mBuffer(new u8[cBufferCapacity]),
      mBufferRoom(cBufferCapacity),
      mReadLength(0),
      mLastResultCode(0),
      mInitialized(false) {
    for (u32 i = 0; i < kSlotCount; ++i) {
        mFiles[i].mData = nullptr;
        mFiles[i].mSize = 0;
        mFiles[i].mRoom = 0;
    }
    initializeAllUserFileMemory();
}

SaveDataHandler::~SaveDataHandler() {
    mRequest.wait();
    for (u32 i = 0; i < kSlotCount; ++i) {
        delete[] mFiles[i].mData;
        mFiles[i].mData = nullptr;
    }
    delete[] mBuffer;
    mBuffer = nullptr;
}

void SaveDataHandler::initializeAllUserFileMemory() {
    GameDataHolder defaultData;
    for (u32 i = 0; i < kSlotCount; ++i) {
        if (mFiles[i].mData == nullptr) {
            mFiles[i].mRoom = cDefaultSlotRoom;
            mFiles[i].mData = new u8[cDefaultSlotRoom];
        }
        std::memset(mFiles[i].mData, 0, mFiles[i].mRoom);
        if (i < kUserFiles * 2u) {
            mFiles[i].mSize = defaultData.makeFileBinary(mFiles[i].mData, mFiles[i].mRoom);
        } else {
            // Config slots start empty/zero-initialized (64 bytes).
            mFiles[i].mSize = 64u;
        }
    }
    mInitialized = true;
}

void SaveDataHandler::update() {
    // SaveRequest is asynchronous on host; completion is checked via isDone() / wait().
}

bool SaveDataHandler::isDone() const {
    return mRequest.isDone();
}

bool SaveDataHandler::parseNativeNbsv(const u8* pBytes, u32 length, const char* pName) {
    if (pBytes == nullptr || length < kHeaderSize) {
        return false;
    }
    const u32 magic = readU32LE(pBytes);
    const u32 version = readU32LE(pBytes + 4u);
    const u32 storedCrc = readU32LE(pBytes + 8u);
    const u32 count = readU32LE(pBytes + 12u);
    if (magic != kSaveMagic || version != kSaveVersion) {
        nebula_log("[SMG2-SAVE] %s invalid NBSV header (magic=0x%08X version=%u)", pName, magic, version);
        return false;
    }
    if (count > (length - kHeaderSize) / kRowSize) {
        nebula_log("[SMG2-SAVE] %s invalid row count %u", pName, count);
        return false;
    }
    const u32 actualCrc = calcCrc32(pBytes + kHeaderSize, length - kHeaderSize);
    if (storedCrc != actualCrc) {
        nebula_log("[SMG2-SAVE] %s CRC mismatch (stored=0x%08X actual=0x%08X)", pName, storedCrc, actualCrc);
        return false;
    }

    const u32 tableBytes = kHeaderSize + count * kRowSize;
    u32 offset = tableBytes;
    for (u32 i = 0; i < count; ++i) {
        const u32 payloadSize = readU32LE(pBytes + kHeaderSize + i * kRowSize + kRowNameSize);
        if (payloadSize > length - offset) {
            nebula_log("[SMG2-SAVE] %s truncated payload at row %u", pName, i);
            return false;
        }
        offset += payloadSize;
    }

    offset = tableBytes;
    for (u32 i = 0; i < count; ++i) {
        char rowName[kRowNameSize + 1u]{};
        std::memcpy(rowName, pBytes + kHeaderSize + i * kRowSize, kRowNameSize);
        const u32 payloadSize = readU32LE(pBytes + kHeaderSize + i * kRowSize + kRowNameSize);
        s32 kind = 0;
        s32 user = 0;
        if (parseRowName(rowName, &kind, &user)) {
            const int idx = slotIndex(kind, user);
            if (payloadSize > mFiles[idx].mRoom) {
                delete[] mFiles[idx].mData;
                mFiles[idx].mRoom = payloadSize;
                mFiles[idx].mData = new u8[payloadSize];
            }
            if (payloadSize > 0u) {
                std::memcpy(mFiles[idx].mData, pBytes + offset, payloadSize);
            }
            mFiles[idx].mSize = payloadSize;
        }
        offset += payloadSize;
    }
    return true;
}

u32 SaveDataHandler::serializeNativeNbsv(u8* pOut, u32 capacity) const {
    const u32 tableBytes = kHeaderSize + kSlotCount * kRowSize;
    u32 payloadBytes = 0;
    for (u32 i = 0; i < kSlotCount; ++i) {
        payloadBytes += mFiles[i].mSize;
    }
    const u32 total = tableBytes + payloadBytes;
    if (pOut == nullptr || capacity < total) {
        return 0;
    }

    u32 offset = tableBytes;
    for (s32 kind = 0; kind < static_cast<s32>(kKindCount); ++kind) {
        for (s32 user = 1; user <= static_cast<s32>(kUserFiles); ++user) {
            const int idx = slotIndex(kind, user);
            const u32 rowOff = kHeaderSize + static_cast<u32>(idx) * kRowSize;
            char rowName[kRowNameSize]{};
            makeRowName(rowName, kind, user);
            std::memcpy(pOut + rowOff, rowName, kRowNameSize);
            writeU32LE(pOut + rowOff + kRowNameSize, mFiles[idx].mSize);
            if (mFiles[idx].mSize > 0u && mFiles[idx].mData != nullptr) {
                std::memcpy(pOut + offset, mFiles[idx].mData, mFiles[idx].mSize);
            }
            offset += mFiles[idx].mSize;
        }
    }

    writeU32LE(pOut + 0u, kSaveMagic);
    writeU32LE(pOut + 4u, kSaveVersion);
    writeU32LE(pOut + 8u, calcCrc32(pOut + kHeaderSize, total - kHeaderSize));
    writeU32LE(pOut + 12u, kSlotCount);
    return total;
}

bool SaveDataHandler::tryLoadIndex(int index) {
    if (index < 0 || index >= static_cast<int>(sizeof(cReadCandidates) / sizeof(cReadCandidates[0]))) {
        return false;
    }
    mRequest.wait();
    mReadLength = 0;
    mRequest.setReadSeq(cReadCandidates[index], mBuffer, mBufferRoom, &mReadLength);
    mRequest.start();
    mRequest.wait();
    if (mRequest.getResult() != SaveRequest::Result_Done) {
        return false;
    }
    return parseNativeNbsv(mBuffer, mReadLength, cReadCandidates[index]);
}

bool SaveDataHandler::requestLoadSaveData(const char*) {
    for (int i = 0; i < static_cast<int>(sizeof(cReadCandidates) / sizeof(cReadCandidates[0])); ++i) {
        if (tryLoadIndex(i)) {
            mLastResultCode = 0;
            nebula_log("[SMG2-SAVE] Loaded save container from %s (%u bytes)", cReadCandidates[i], mReadLength);
            return true;
        }
    }
    mLastResultCode = 1;
    return false;
}

bool SaveDataHandler::requestSaveSaveData() {
    mRequest.wait();
    const u32 total = serializeNativeNbsv(mBuffer, mBufferRoom);
    if (total == 0u) {
        mLastResultCode = -1;
        return false;
    }
    mRequest.setWriteSeq(cPrimarySaveName, mBuffer, total);
    mRequest.start();
    mRequest.wait();
    const bool ok = (mRequest.getResult() == SaveRequest::Result_Done);
    mLastResultCode = ok ? 0 : -1;
    if (ok) {
        nebula_log("[SMG2-SAVE] Wrote %s (NBSV v%u, %u bytes, %u slots)", cPrimarySaveName, kSaveVersion, total, kSlotCount);
    } else {
        nebula_log("[SMG2-SAVE] Failed to write %s (result=%d, total=%u)", cPrimarySaveName, static_cast<int>(mRequest.getResult()), total);
    }
    return ok;
}

bool SaveDataHandler::trySave(bool* pOutOk) {
    const bool ok = requestSaveSaveData();
    if (pOutOk != nullptr) {
        *pOutOk = ok;
    }
    return ok;
}

bool SaveDataHandler::loadFromExistingOrInit(GameDataHolder* pTargetHolder) {
    if (requestLoadSaveData()) {
        return restoreGameData(1, pTargetHolder);
    }
    initializeAllUserFileMemory();
    if (pTargetHolder != nullptr) {
        pTargetHolder->resetAllData();
        storeGameData(1, pTargetHolder);
    }
    return trySave(nullptr);
}

bool SaveDataHandler::storeGameData(s32 userSlot, const GameDataHolder* pSourceHolder) {
    if (pSourceHolder == nullptr || userSlot < 1 || userSlot > static_cast<s32>(kUserFiles)) {
        return false;
    }
    const int idx = slotIndex(0, userSlot);
    const u32 needed = pSourceHolder->makeFileBinary(nullptr, 0u);
    if (needed > mFiles[idx].mRoom) {
        delete[] mFiles[idx].mData;
        mFiles[idx].mRoom = needed;
        mFiles[idx].mData = new u8[needed];
    }
    mFiles[idx].mSize = pSourceHolder->makeFileBinary(mFiles[idx].mData, mFiles[idx].mRoom);
    return true;
}

bool SaveDataHandler::restoreGameData(s32 userSlot, GameDataHolder* pTargetHolder) const {
    if (pTargetHolder == nullptr || userSlot < 1 || userSlot > static_cast<s32>(kUserFiles)) {
        return false;
    }
    const int idx = slotIndex(0, userSlot);
    if (mFiles[idx].mData == nullptr || mFiles[idx].mSize == 0u) {
        return false;
    }
    return pTargetHolder->loadFromFileBinary("mario1", mFiles[idx].mData, mFiles[idx].mSize);
}
