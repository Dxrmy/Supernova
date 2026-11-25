#pragma once

#include <cstring>
#include <revolution.h>

#define JMAP_VALUE_TYPE_LONG 0
#define JMAP_VALUE_TYPE_STRING 1
#define JMAP_VALUE_TYPE_FLOAT 2
#define JMAP_VALUE_TYPE_LONG_2 3
#define JMAP_VALUE_TYPE_SHORT 4
#define JMAP_VALUE_TYPE_BYTE 5
#define JMAP_VALUE_TYPE_STRING_PTR 6
#define JMAP_VALUE_TYPE_NULL 7

class JMapInfoIter;

// Nebula cooked NTBL header/column structs (see crates/nebula-import/src/cook/table.rs).
struct NtblColumn {
    u32 mHash;
    u32 mMask;
    u8 mKind;   // 0=Int, 1=Field, 2=Float, 3=Text
    u8 mWidth;  // 8, 16, or 32 for Int
    u16 mReserved;
};

struct NtblHeader {
    char mMagic[4];  // "NTBL"
    u32 mRows;
    u32 mColumns;
    u32 mRowsAt;
    u32 mTextAt;
    u32 mTextSize;
    u32 mReserved[2];

    const NtblColumn* columns() const {
        return reinterpret_cast<const NtblColumn*>(this + 1);
    }
    const u32* cells() const {
        return reinterpret_cast<const u32*>(reinterpret_cast<const char*>(this) + mRowsAt);
    }
    const char* text() const {
        return reinterpret_cast<const char*>(this) + mTextAt;
    }
};

struct JMapItem {
    u32 mHash;
    u32 mMask;
    u16 mOffsData;
    u8 mShift;
    u8 mType;
};

struct JMapData {
    s32 mNumEntries;
    s32 mNumFields;
    s32 mDataOffset;
    u32 mEntrySize;
    const JMapItem mItems[];
};

template <typename T>
inline bool compareValues(const T a, const T b) {
    return a == b;
}

template <>
inline bool compareValues<const char*>(const char* pA, const char* pB) {
    if (pA == nullptr || pB == nullptr) {
        return pA == pB;
    }
    return strcmp(pA, pB) == 0;
}

class JMapInfo {
public:
    JMapInfo();
    ~JMapInfo();

    inline bool operator==(const JMapInfo& rInfo) const {
        return mData == rInfo.mData;
    }

    inline bool dataExists() const {
        return mData != nullptr;
    }

    inline bool isNtbl() const {
        return mData != nullptr && std::memcmp(mData, "NTBL", 4) == 0;
    }

    int getNumEntries() const;
    int getNumFields() const;

    bool attach(const void* pData);
    void setName(const char* pName);
    const char* getName() const;
    s32 searchItemInfo(const char* pKey) const;
    s32 getValueType(const char* pKey) const;
    bool getValueFast(int entryIndex, int itemIndex, const char** pValueOut) const;
    bool getValueFast(int entryIndex, int itemIndex, u32* pValueOut) const;
    bool getValueFast(int entryIndex, int itemIndex, s32* pValueOut) const;
    bool getValueFast(int entryIndex, int itemIndex, f32* pValueOut) const;
    bool getValueFast(int entryIndex, int itemIndex, bool* pValueOut) const;

    JMapInfoIter findElementBinary(const char* pKey, const char* pValue) const;

    template <typename T>
    const bool getValue(int entryIndex, const char* pKey, T* pValueOut) const;

    template <typename T>
    JMapInfoIter findElement(const char* pKey, T searchValue, int startIndex) const;

    inline JMapInfoIter begin() const;
    inline JMapInfoIter end() const;

    const JMapData* mData;
    const char* mName;
};

template <typename T>
const bool JMapInfo::getValue(int entryIndex, const char* pKey, T* pValueOut) const {
    s32 itemIndex = searchItemInfo(pKey);
    if (itemIndex < 0) {
        return false;
    }
    return getValueFast(entryIndex, itemIndex, pValueOut);
}

class JMapInfoIter {
public:
    JMapInfoIter() : mInfo(nullptr), mIndex(-1) {}
    JMapInfoIter(const JMapInfo* pInfo, s32 index) : mInfo(pInfo), mIndex(index) {}

    bool operator==(const JMapInfoIter& rIter) const {
        return mIndex == rIter.mIndex && mInfo != nullptr && rIter.mInfo != nullptr && *mInfo == *rIter.mInfo;
    }

    bool operator!=(const JMapInfoIter& rIter) const {
        return !(*this == rIter);
    }

    bool isValid() const {
        return mInfo != nullptr && mIndex >= 0 && mIndex < mInfo->getNumEntries();
    }

    template <typename T>
    bool getValue(const char* pKey, T* pValueOut) const {
        return mInfo->getValue(mIndex, pKey, pValueOut);
    }

    const JMapInfo* mInfo;
    s32 mIndex;
};

template <typename T>
JMapInfoIter JMapInfo::findElement(const char* pKey, T searchValue, int startIndex) const {
    int entryIndex = startIndex;
    T value{};
    while (entryIndex < getNumEntries()) {
        if (getValue<T>(entryIndex, pKey, &value) && compareValues<T>(value, searchValue)) {
            return JMapInfoIter(this, entryIndex);
        }
        entryIndex++;
    }
    return end();
}

inline JMapInfoIter JMapInfo::begin() const {
    return JMapInfoIter(this, 0);
}

inline JMapInfoIter JMapInfo::end() const {
    return JMapInfoIter(this, getNumEntries());
}

namespace MR {
    JMapInfoIter findJMapInfoElementNoCase(const JMapInfo* pInfo, const char* pKey, const char* pValue, int startIndex);
};  // namespace MR
