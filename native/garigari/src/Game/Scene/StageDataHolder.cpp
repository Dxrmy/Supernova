#include "Game/Scene/StageDataHolder.hpp"
#include "Game/Util/FileUtil.hpp"
#include <cstring>
#include <nebula_module.h>

StageDataHolder::StageDataHolder() : mScenarioArchive(nullptr), mMounted(false) {
    mStageName[0] = '\0';
}

bool StageDataHolder::mountStage(const char* pStageName) {
    if (pStageName == nullptr || pStageName[0] == '\0') {
        return false;
    }
    nebula_snprintf(mStageName, sizeof(mStageName), "%s", pStageName);

    char scenarioPath[128]{};
    nebula_snprintf(scenarioPath, sizeof(scenarioPath), "/StageData/%sScenario", pStageName);
    mScenarioArchive = MR::receiveArchive(scenarioPath);
    if (mScenarioArchive != nullptr) {
        mMounted = true;
        nebula_log("[SMG2-STAGE] Mounted %s", scenarioPath);
    } else {
        mMounted = false;
        nebula_log("[SMG2-STAGE] TODO(Garigari): stage archive not found: %s", scenarioPath);
    }
    return mMounted;
}
