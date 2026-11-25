#include "Game/System/GameDataHolder.hpp"
#include "Game/Util/HashUtil.hpp"
#include <cstring>
#include <nebula_module.h>

namespace {
    const u32 kBinaryMagic = 0x47443242u;  // "GD2B"
    const u32 kBinaryVersion = 1u;
}  // namespace

GameDataHolder::GameDataHolder()
    : mFlagCount(0),
      mValueCount(0),
      mPowerStarNum(0),
      mNormalPowerStarNum(0),
      mTicoCoinNum(0),
      mStockedStarPieces(0),
      mPlayerMissNum(0),
      mCurrentWorldId(1),
      mCumulativePlayTime(0) {
    resetAllData();
}

void GameDataHolder::resetAllData() {
    mFlagCount = 0;
    mValueCount = 0;
    for (s32 i = 0; i < kMaxFlags; ++i) {
        mFlags[i].mHash = 0;
        mFlags[i].mValue = false;
    }
    for (s32 i = 0; i < kMaxValues; ++i) {
        mValues[i].mHash = 0;
        mValues[i].mValue = 0;
    }
    for (s32 i = 0; i < kMaxGrandStars; ++i) {
        mGrandStars[i] = false;
    }
    mPowerStarNum = 0;
    mNormalPowerStarNum = 0;
    mTicoCoinNum = 0;
    mStockedStarPieces = 0;
    mPlayerMissNum = 0;
    mCurrentWorldId = 1;
    mCumulativePlayTime = 0;
    nebula_snprintf(mUserName, sizeof(mUserName), "%s", "Mario");
}

bool GameDataHolder::isOnGameEventFlag(const char* pFlagName) const {
    if (pFlagName == nullptr) {
        return false;
    }
    const u32 hash = MR::getHashCode(pFlagName);
    for (s32 i = 0; i < mFlagCount; ++i) {
        if (mFlags[i].mHash == hash) {
            return mFlags[i].mValue;
        }
    }
    return false;
}

void GameDataHolder::onGameEventFlag(const char* pFlagName) {
    if (pFlagName == nullptr) {
        return;
    }
    const u32 hash = MR::getHashCode(pFlagName);
    for (s32 i = 0; i < mFlagCount; ++i) {
        if (mFlags[i].mHash == hash) {
            mFlags[i].mValue = true;
            return;
        }
    }
    if (mFlagCount < kMaxFlags) {
        mFlags[mFlagCount].mHash = hash;
        mFlags[mFlagCount].mValue = true;
        mFlagCount++;
    }
}

void GameDataHolder::offGameEventFlag(const char* pFlagName) {
    if (pFlagName == nullptr) {
        return;
    }
    const u32 hash = MR::getHashCode(pFlagName);
    for (s32 i = 0; i < mFlagCount; ++i) {
        if (mFlags[i].mHash == hash) {
            mFlags[i].mValue = false;
            return;
        }
    }
}

u16 GameDataHolder::getGameEventValue(const char* pValueName) const {
    if (pValueName == nullptr) {
        return 0;
    }
    const u32 hash = MR::getHashCode(pValueName);
    for (s32 i = 0; i < mValueCount; ++i) {
        if (mValues[i].mHash == hash) {
            return mValues[i].mValue;
        }
    }
    return 0;
}

void GameDataHolder::setGameEventValue(const char* pValueName, u16 value) {
    if (pValueName == nullptr) {
        return;
    }
    const u32 hash = MR::getHashCode(pValueName);
    for (s32 i = 0; i < mValueCount; ++i) {
        if (mValues[i].mHash == hash) {
            mValues[i].mValue = value;
            return;
        }
    }
    if (mValueCount < kMaxValues) {
        mValues[mValueCount].mHash = hash;
        mValues[mValueCount].mValue = value;
        mValueCount++;
    }
}

void GameDataHolder::incPlayerMissNum() {
    mPlayerMissNum++;
}

s32 GameDataHolder::getPlayerMissNum() const {
    return mPlayerMissNum;
}

s32 GameDataHolder::calcCurrentPowerStarNum() const {
    return mPowerStarNum;
}

s32 GameDataHolder::calcNormalPowerStarNum() const {
    return mNormalPowerStarNum;
}

bool GameDataHolder::hasGrandStar(int grandStarId) const {
    if (grandStarId < 0 || grandStarId >= kMaxGrandStars) {
        return false;
    }
    return mGrandStars[grandStarId];
}

s32 GameDataHolder::calcTicoCoinNum() const {
    return mTicoCoinNum;
}

s32 GameDataHolder::getStocketStarPieceNum() const {
    return mStockedStarPieces;
}

void GameDataHolder::addStockedStarPieceNum(int count) {
    const s32 updated = mStockedStarPieces + count;
    mStockedStarPieces = updated < 0 ? 0 : (updated > 9999 ? 9999 : updated);
}

void GameDataHolder::incCumulativePlayTime() {
    mCumulativePlayTime++;
}

u32 GameDataHolder::getCumulativePlayTime() const {
    return mCumulativePlayTime;
}

void GameDataHolder::setUserName(const char* pName) {
    if (pName == nullptr || pName[0] == '\0') {
        nebula_snprintf(mUserName, sizeof(mUserName), "%s", "Mario");
        return;
    }
    nebula_snprintf(mUserName, sizeof(mUserName), "%s", pName);
}

u32 GameDataHolder::makeFileBinary(u8* pOutBuffer, u32 capacity) const {
    const u32 headerBytes = 40u + 32u;
    const u32 flagBytes = static_cast<u32>(mFlagCount) * 8u;
    const u32 valBytes = static_cast<u32>(mValueCount) * 8u;
    const u32 total = headerBytes + flagBytes + valBytes;
    if (pOutBuffer == nullptr || capacity < total) {
        return total;
    }

    u32* words = reinterpret_cast<u32*>(pOutBuffer);
    words[0] = kBinaryMagic;
    words[1] = kBinaryVersion;
    words[2] = static_cast<u32>(mFlagCount);
    words[3] = static_cast<u32>(mValueCount);
    words[4] = static_cast<u32>(mPowerStarNum);
    words[5] = static_cast<u32>(mNormalPowerStarNum);
    words[6] = static_cast<u32>(mTicoCoinNum);
    words[7] = static_cast<u32>(mStockedStarPieces);
    words[8] = static_cast<u32>(mPlayerMissNum);
    words[9] = mCumulativePlayTime;

    std::memcpy(pOutBuffer + 40u, mUserName, 32u);

    u8* cursor = pOutBuffer + headerBytes;
    for (s32 i = 0; i < mFlagCount; ++i) {
        u32* entry = reinterpret_cast<u32*>(cursor);
        entry[0] = mFlags[i].mHash;
        entry[1] = mFlags[i].mValue ? 1u : 0u;
        cursor += 8u;
    }
    for (s32 i = 0; i < mValueCount; ++i) {
        u32* entry = reinterpret_cast<u32*>(cursor);
        entry[0] = mValues[i].mHash;
        entry[1] = static_cast<u32>(mValues[i].mValue);
        cursor += 8u;
    }
    return total;
}

bool GameDataHolder::loadFromFileBinary(const char*, const u8* pBuffer, u32 size) {
    const u32 headerBytes = 40u + 32u;
    if (pBuffer == nullptr || size < headerBytes) {
        return false;
    }
    const u32* words = reinterpret_cast<const u32*>(pBuffer);
    if (words[0] != kBinaryMagic || words[1] != kBinaryVersion) {
        return false;
    }
    const s32 flags = static_cast<s32>(words[2]);
    const s32 values = static_cast<s32>(words[3]);
    if (flags < 0 || flags > kMaxFlags || values < 0 || values > kMaxValues) {
        return false;
    }
    const u32 needed = headerBytes + static_cast<u32>(flags) * 8u + static_cast<u32>(values) * 8u;
    if (size < needed) {
        return false;
    }

    resetAllData();
    mFlagCount = flags;
    mValueCount = values;
    mPowerStarNum = static_cast<s32>(words[4]);
    mNormalPowerStarNum = static_cast<s32>(words[5]);
    mTicoCoinNum = static_cast<s32>(words[6]);
    mStockedStarPieces = static_cast<s32>(words[7]);
    mPlayerMissNum = static_cast<s32>(words[8]);
    mCumulativePlayTime = words[9];
    std::memcpy(mUserName, pBuffer + 40u, 32u);
    mUserName[31] = '\0';

    const u8* cursor = pBuffer + headerBytes;
    for (s32 i = 0; i < mFlagCount; ++i) {
        const u32* entry = reinterpret_cast<const u32*>(cursor);
        mFlags[i].mHash = entry[0];
        mFlags[i].mValue = (entry[1] != 0u);
        cursor += 8u;
    }
    for (s32 i = 0; i < mValueCount; ++i) {
        const u32* entry = reinterpret_cast<const u32*>(cursor);
        mValues[i].mHash = entry[0];
        mValues[i].mValue = static_cast<u16>(entry[1] & 0xFFFFu);
        cursor += 8u;
    }
    return true;
}
