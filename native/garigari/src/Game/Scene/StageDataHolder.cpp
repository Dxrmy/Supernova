#include "Game/Scene/StageDataHolder.hpp"
#include "Game/Gravity/BarrelGravity.hpp"
#include "Game/Gravity/ConeGravity.hpp"
#include "Game/Gravity/CubeGravity.hpp"
#include "Game/Gravity/DiskGravity.hpp"
#include "Game/Gravity/DiskTorusGravity.hpp"
#include "Game/Gravity/GravityCreator.hpp"
#include "Game/Gravity/ParallelGravity.hpp"
#include "Game/Gravity/PlanetGravityManager.hpp"
#include "Game/Gravity/PointGravity.hpp"
#include "Game/Gravity/SegmentGravity.hpp"
#include "Game/Gravity/WireGravity.hpp"
#include "Game/Scene/SceneObjHolder.hpp"
#include "Game/System/ScenarioDataParser.hpp"
#include "Game/Util/FileUtil.hpp"
#include "Game/Util/JMapInfo.hpp"
#include "Game/Util/JMapUtil.hpp"
#include <JSystem/JKernel/JKRArchive.hpp>
#include <cstring>
#include <nebula_module.h>

namespace ScenarioDataFunction {
    ScenarioDataParser* getScenarioDataParser();
}  // namespace ScenarioDataFunction

namespace {
    s32 loadZoneGravities(const char* pZoneName) {
        if (pZoneName == nullptr || pZoneName[0] == '\0') {
            return 0;
        }
        char mapPath[128]{};
        nebula_snprintf(mapPath, sizeof(mapPath), "/StageData/%sMap", pZoneName);
        JKRArchive* pMapArc = MR::receiveArchive(mapPath);
        if (pMapArc == nullptr) {
            return 0;
        }

        static const char* const kLayers[] = {
            "Common", "LayerA", "LayerB", "LayerC", "LayerD", "LayerE", "LayerF", "LayerG",
            "LayerH", "LayerI", "LayerJ", "LayerK", "LayerL", "LayerM", "LayerN", "LayerO", "LayerP"
        };
        s32 loaded = 0;
        for (const char* layer : kLayers) {
            char subPath[128]{};
            nebula_snprintf(subPath, sizeof(subPath), "jmp/Placement/%s/ObjInfo", layer);
            void* pData = pMapArc->getResource(subPath);
            if (pData == nullptr) {
                continue;
            }
            JMapInfo info;
            if (!info.attach(pData)) {
                continue;
            }
            const int n = info.getNumEntries();
            for (int i = 0; i < n; ++i) {
                JMapInfoIter iter(&info, i);
                const char* objName = nullptr;
                if (!MR::getObjectName(&objName, iter) || objName == nullptr) {
                    continue;
                }
                PlanetGravity* g = nullptr;
                if (std::strcmp(objName, "GlobalPointGravity") == 0) {
                    g = PointGravityCreator().createFromJMap(iter);
                } else if (std::strcmp(objName, "GlobalDiskGravity") == 0) {
                    g = DiskGravityCreator().createFromJMap(iter);
                } else if (std::strcmp(objName, "GlobalCubeGravity") == 0) {
                    g = CubeGravityCreator().createFromJMap(iter);
                } else if (std::strcmp(objName, "GlobalPlaneGravity") == 0) {
                    g = PlaneGravityCreator().createFromJMap(iter);
                } else if (std::strcmp(objName, "GlobalPlaneGravityInBox") == 0) {
                    g = PlaneInBoxGravityCreator().createFromJMap(iter);
                } else if (std::strcmp(objName, "GlobalPlaneGravityInCylinder") == 0) {
                    g = PlaneInCylinderGravityCreator().createFromJMap(iter);
                } else if (std::strcmp(objName, "GlobalConeGravity") == 0) {
                    g = ConeGravityCreator().createFromJMap(iter);
                } else if (std::strcmp(objName, "GlobalBarrelGravity") == 0) {
                    g = BarrelGravityCreator().createFromJMap(iter);
                } else if (std::strcmp(objName, "GlobalDiskTorusGravity") == 0) {
                    g = DiskTorusGravityCreator().createFromJMap(iter);
                } else if (std::strcmp(objName, "GlobalSegmentGravity") == 0) {
                    g = SegmentGravityCreator().createFromJMap(iter);
                } else if (std::strcmp(objName, "GlobalWireGravity") == 0) {
                    g = WireGravityCreator().createFromJMap(iter);
                }
                if (g != nullptr) {
                    ++loaded;
                    nebula_log("[SMG2-GRAVITY] %s/%s: registered %s (prio=%d, range=%.1f)",
                               pZoneName, layer, objName, g->mPriority, g->mRange);
                }
            }
        }
        return loaded;
    }
}  // namespace

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

s32 StageDataHolder::loadStageGravities(const char* pStageName) {
    if (pStageName == nullptr || pStageName[0] == '\0') {
        return 0;
    }
    MR::createSceneObj(SceneObj_PlanetGravityManager);
    s32 total = 0;
    ScenarioDataParser* pParser = ScenarioDataFunction::getScenarioDataParser();
    ScenarioData* pScenario = (pParser != nullptr) ? pParser->getScenarioData(pStageName) : nullptr;
    if (pScenario != nullptr && pScenario->getZoneNum() > 0) {
        const s32 zoneNum = pScenario->getZoneNum();
        for (s32 z = 0; z < zoneNum; ++z) {
            const char* zoneName = pScenario->getZoneName(z);
            total += loadZoneGravities(zoneName);
        }
    } else {
        total += loadZoneGravities(pStageName);
    }
    nebula_log("[SMG2-GRAVITY] Loaded %d stage gravity object(s) for '%s'", total, pStageName);
    return total;
}
