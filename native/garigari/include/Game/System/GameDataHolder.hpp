#pragma once

#include <revolution/types.h>

class GameDataHolder {
public:
    static constexpr s32 kMaxFlags = 512;
    static constexpr s32 kMaxValues = 128;
    static constexpr s32 kMaxGrandStars = 8;

    GameDataHolder();

    bool isOnGameEventFlag(const char* pFlagName) const;
    void onGameEventFlag(const char* pFlagName);
    void offGameEventFlag(const char* pFlagName);
    u16 getGameEventValue(const char* pValueName) const;
    void setGameEventValue(const char* pValueName, u16 value);

    void incPlayerMissNum();
    s32 getPlayerMissNum() const;
    s32 calcCurrentPowerStarNum() const;
    s32 calcNormalPowerStarNum() const;
    bool hasGrandStar(int grandStarId) const;
    s32 calcTicoCoinNum() const;
    s32 getStocketStarPieceNum() const;
    void addStockedStarPieceNum(int count);
    void incCumulativePlayTime();
    u32 getCumulativePlayTime() const;

    void resetAllData();
    u32 makeFileBinary(u8* pOutBuffer, u32 capacity) const;
    bool loadFromFileBinary(const char* pFileName, const u8* pBuffer, u32 size);

    const char* getUserName() const { return mUserName; }
    void setUserName(const char* pName);
    s32 getCurrentWorldId() const { return mCurrentWorldId; }
    void setCurrentWorldId(s32 worldId) { mCurrentWorldId = worldId; }

private:
    struct FlagEntry {
        u32 mHash;
        bool mValue;
    };

    struct ValueEntry {
        u32 mHash;
        u16 mValue;
    };

    FlagEntry mFlags[kMaxFlags];
    s32 mFlagCount;
    ValueEntry mValues[kMaxValues];
    s32 mValueCount;
    bool mGrandStars[kMaxGrandStars];
    s32 mPowerStarNum;
    s32 mNormalPowerStarNum;
    s32 mTicoCoinNum;
    s32 mStockedStarPieces;
    s32 mPlayerMissNum;
    s32 mCurrentWorldId;
    u32 mCumulativePlayTime;
    char mUserName[32];
};
