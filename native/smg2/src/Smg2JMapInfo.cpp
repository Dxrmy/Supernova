// Native NTBL-backed implementation of Garigari's JMapInfo (BCSV reader).
// Nebula's importer cooks every .bcsv/.tbl/.pa/.bcam/.banmt on disc to little-endian NTBL.
#include "Game/Util/JMapInfo.hpp"
#include "Game/Util/HashUtil.hpp"
#include "Game/Util/StringUtil.hpp"
#include <cstring>

namespace {
    inline const NtblHeader* asNtbl(const JMapData* pData) {
        return reinterpret_cast<const NtblHeader*>(pData);
    }
}  // namespace

JMapInfo::JMapInfo() : mData(nullptr), mName("Undefined") {
}

JMapInfo::~JMapInfo() {
}

bool JMapInfo::attach(const void* pData) {
    if (pData == nullptr) {
        return false;
    }
    mData = static_cast<const JMapData*>(pData);
    if (!isNtbl()) {
        const u8* pHead = static_cast<const u8*>(pData);
        NEBULA_PANIC("JMapInfo(%s): expected NTBL table, got %02x%02x%02x%02x",
                     mName != nullptr ? mName : "?", pHead[0], pHead[1], pHead[2], pHead[3]);
    }
    return true;
}

void JMapInfo::setName(const char* pName) {
    mName = pName;
}

const char* JMapInfo::getName() const {
    return mName;
}

int JMapInfo::getNumEntries() const {
    if (!dataExists()) {
        return 0;
    }
    return isNtbl() ? static_cast<int>(asNtbl(mData)->mRows) : mData->mNumEntries;
}

int JMapInfo::getNumFields() const {
    if (!dataExists()) {
        return 0;
    }
    return isNtbl() ? static_cast<int>(asNtbl(mData)->mColumns) : mData->mNumFields;
}

s32 JMapInfo::searchItemInfo(const char* pKey) const {
    if (!dataExists() || pKey == nullptr) {
        return -1;
    }
    const s32 nFields = getNumFields();
    const u32 hash = MR::getHashCode(pKey);
    const NtblColumn* cols = asNtbl(mData)->columns();
    for (s32 i = 0; i < nFields; ++i) {
        if (cols[i].mHash == hash) {
            return i;
        }
    }
    return -1;
}

s32 JMapInfo::getValueType(const char* pItem) const {
    const s32 itemId = searchItemInfo(pItem);
    if (itemId < 0) {
        return JMAP_VALUE_TYPE_NULL;
    }
    const NtblColumn& col = asNtbl(mData)->columns()[itemId];
    switch (col.mKind) {
    case 0:  // Int
        return col.mWidth == 8 ? JMAP_VALUE_TYPE_BYTE : (col.mWidth == 16 ? JMAP_VALUE_TYPE_SHORT : JMAP_VALUE_TYPE_LONG);
    case 1:  // Field
        return JMAP_VALUE_TYPE_LONG_2;
    case 2:  // Float
        return JMAP_VALUE_TYPE_FLOAT;
    case 3:  // Text
        return JMAP_VALUE_TYPE_STRING_PTR;
    default:
        return JMAP_VALUE_TYPE_NULL;
    }
}

bool JMapInfo::getValueFast(int entryIndex, int itemIndex, const char** pValueOut) const {
    if (!dataExists() || entryIndex < 0 || entryIndex >= getNumEntries() || itemIndex < 0 || itemIndex >= getNumFields()) {
        return false;
    }
    const NtblHeader* hdr = asNtbl(mData);
    const u32* cell = hdr->cells() + static_cast<u32>(entryIndex) * hdr->mColumns + static_cast<u32>(itemIndex);
    *pValueOut = hdr->columns()[itemIndex].mKind == 3 ? (hdr->text() + *cell) : reinterpret_cast<const char*>(cell);
    return true;
}

bool JMapInfo::getValueFast(int entryIndex, int itemIndex, u32* pValueOut) const {
    if (!dataExists() || entryIndex < 0 || entryIndex >= getNumEntries() || itemIndex < 0 || itemIndex >= getNumFields()) {
        return false;
    }
    const NtblHeader* hdr = asNtbl(mData);
    const NtblColumn& col = hdr->columns()[itemIndex];
    if (col.mKind != 0 && col.mKind != 1) {
        return false;
    }
    *pValueOut = *(hdr->cells() + static_cast<u32>(entryIndex) * hdr->mColumns + static_cast<u32>(itemIndex));
    return true;
}

bool JMapInfo::getValueFast(int entryIndex, int itemIndex, s32* pValueOut) const {
    if (!dataExists() || entryIndex < 0 || entryIndex >= getNumEntries() || itemIndex < 0 || itemIndex >= getNumFields()) {
        return false;
    }
    const NtblHeader* hdr = asNtbl(mData);
    const NtblColumn& col = hdr->columns()[itemIndex];
    if (col.mKind != 0) {
        return false;
    }
    const u32 val = *(hdr->cells() + static_cast<u32>(entryIndex) * hdr->mColumns + static_cast<u32>(itemIndex));
    *pValueOut = col.mWidth == 8 ? static_cast<s8>(val) : (col.mWidth == 16 ? static_cast<s16>(val) : static_cast<s32>(val));
    return true;
}

bool JMapInfo::getValueFast(int entryIndex, int itemIndex, f32* pValueOut) const {
    if (!dataExists() || entryIndex < 0 || entryIndex >= getNumEntries() || itemIndex < 0 || itemIndex >= getNumFields()) {
        return false;
    }
    const NtblHeader* hdr = asNtbl(mData);
    const u32* cell = hdr->cells() + static_cast<u32>(entryIndex) * hdr->mColumns + static_cast<u32>(itemIndex);
    std::memcpy(pValueOut, cell, sizeof(f32));
    return true;
}

bool JMapInfo::getValueFast(int entryIndex, int itemIndex, bool* pValueOut) const {
    if (!dataExists() || entryIndex < 0 || entryIndex >= getNumEntries() || itemIndex < 0 || itemIndex >= getNumFields()) {
        return false;
    }
    const NtblHeader* hdr = asNtbl(mData);
    const u32 val = *(hdr->cells() + static_cast<u32>(entryIndex) * hdr->mColumns + static_cast<u32>(itemIndex));
    *pValueOut = val != 0;
    return true;
}

JMapInfoIter JMapInfo::findElementBinary(const char* pKey, const char* pValue) const {
    if (pKey == nullptr || pValue == nullptr) {
        return end();
    }
    int low = 0;
    int high = getNumEntries();
    const char* nextVal = nullptr;
    while (low < high) {
        int mid = (low + high) / 2;
        if (!getValue<const char*>(mid, pKey, &nextVal)) {
            return end();
        }
        int cmp = std::strcmp(nextVal, pValue);
        if (cmp == 0) {
            return JMapInfoIter(this, mid);
        } else if (cmp < 0) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    return end();
}

namespace MR {
    JMapInfoIter findJMapInfoElementNoCase(const JMapInfo* pInfo, const char* pKey, const char* pValue, int startIndex) {
        if (pInfo == nullptr || pKey == nullptr || pValue == nullptr) {
            return pInfo != nullptr ? pInfo->end() : JMapInfoIter();
        }
        for (int i = startIndex; i < pInfo->getNumEntries(); ++i) {
            const char* cur = nullptr;
            if (pInfo->getValue<const char*>(i, pKey, &cur) && cur != nullptr && nebula_stricmp(cur, pValue) == 0) {
                return JMapInfoIter(pInfo, i);
            }
        }
        return pInfo->end();
    }
}  // namespace MR
