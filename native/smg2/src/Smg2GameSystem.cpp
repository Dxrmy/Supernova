// SMG2 (Garigari) bring-up GameSystem harness (M2 boot/system spine).
// Drives Garigari's real GameSystem, GameSystemObjHolder, GameSystemSceneController,
// GameSystemStationedArchiveLoader, GameDataHolder, GameSequenceDirector, and SaveDataHandler
// from SB4E01 `game.pak`, plus the 60 Hz host pacing loop.

#include <nebula_game_api.h>
#include <nebula_module.h>

#include "Game/Gravity/CubeGravity.hpp"
#include "Game/Gravity/DiskGravity.hpp"
#include "Game/Gravity/GravityInfo.hpp"
#include "Game/Gravity/PlanetGravityManager.hpp"
#include "Game/Gravity/PointGravity.hpp"
#include "Game/LiveActor/ActorStateKeeper.hpp"
#include "Game/Map/RailGraph.hpp"
#include "Game/Map/RailGraphEdge.hpp"
#include "Game/Scene/SceneObjHolder.hpp"
#include "Game/System/GalaxyStatusAccessor.hpp"
#include "Game/System/GameDataFunction.hpp"
#include "Game/System/GameDataHolder.hpp"
#include "Game/System/GameSequenceDirector.hpp"
#include "Game/System/GameSystem.hpp"
#include "Game/System/GameSystemDataHolder.hpp"
#include "Game/System/GameSystemSceneController.hpp"
#include "Game/System/SaveDataHandleSequence.hpp"
#include "Game/System/SaveDataHandler.hpp"
#include "Game/System/ScenarioDataParser.hpp"
#include "Game/Util/MathUtil.hpp"
#include "Game/Util/SingletonHolder.hpp"

namespace Smg2Render {
    uint32_t registerPipelines();
    void submitFrame(uint32_t frame);
}  // namespace Smg2Render

namespace {
    SceneObjHolder* sSceneObjHolder = nullptr;
    ScenarioDataParser* sScenarioDataParser = nullptr;
}  // namespace

namespace ScenarioDataFunction {
    ScenarioDataParser* getScenarioDataParser() {
        if (sScenarioDataParser != nullptr) {
            return sScenarioDataParser;
        }
        if (SingletonHolder<GameSystem>::exists()) {
            GameSystemSceneController* ctrl = SingletonHolder<GameSystem>::get()->mSceneController;
            if (ctrl != nullptr && ctrl->getScenarioDataParser() != nullptr) {
                return ctrl->getScenarioDataParser();
            }
        }
        return nullptr;
    }
}  // namespace ScenarioDataFunction

namespace MR {
    SceneObjHolder* getSceneObjHolder() {
        return sSceneObjHolder;
    }

    bool isExistSceneObj(int id) {
        return sSceneObjHolder != nullptr && sSceneObjHolder->isExist(id);
    }
}  // namespace MR

namespace {
    const char* readSampleText(const JMapInfo* table, int row, const char* preferredKey) {
        if (table == nullptr || !table->dataExists() || row < 0 || row >= table->getNumEntries()) {
            return nullptr;
        }
        const char* val = nullptr;
        if (preferredKey != nullptr && table->getValue<const char*>(row, preferredKey, &val) && val != nullptr && val[0] != '\0') {
            return val;
        }
        if (table->isNtbl()) {
            const auto* hdr = reinterpret_cast<const NtblHeader*>(table->mData);
            const NtblColumn* cols = hdr->columns();
            for (int colIdx = 0; colIdx < static_cast<int>(hdr->mColumns); ++colIdx) {
                if (cols[colIdx].mKind == 3 && table->getValueFast(row, colIdx, &val) && val != nullptr && val[0] != '\0') {
                    return val;
                }
            }
        }
        return nullptr;
    }
}  // namespace

extern "C" void nebula_game_main(void) {
    const NebulaHostServices* host = nebula_host();
    const char* gameId = (host != nullptr && host->game_id != nullptr) ? host->game_id(host->user) : "SB4E01";
    nebula_log("[SMG2] nebula_game_main starting (Garigari SB4E01 M2 boot spine, disc=%s)", gameId);

    MR::initAcosTable();
    Smg2Render::registerPipelines();
    sSceneObjHolder = new SceneObjHolder();

    // Initialize real GameSystem spine (GameSystemObjHolder, Font/DataHolder, SceneController, StationedArchiveLoader, SequenceDirector).
    SingletonHolder<GameSystem>::set(new GameSystem());
    GameSystem* pGameSystem = SingletonHolder<GameSystem>::get();
    pGameSystem->init();

    // Advance boot nerves cleanly through InitializeAudio -> InitializeLogoScene -> LoadStationedArchive -> Normal.
    for (int step = 0; step < 8 && !pGameSystem->isDoneLoadSystemArchive(); ++step) {
        pGameSystem->frameLoop();
    }

    GameSystemDataHolder* dataHolder = pGameSystem->mDataHolder;
    const JMapInfo* worldOrder = GameSystemDataFunction::getGalaxyWorldOrderList();
    const JMapInfo* shipEvents = GameSystemDataFunction::getMarioFaceShipEventDataTable();
    const JMapInfo* galaxyTable = dataHolder != nullptr ? dataHolder->mGalaxyDataTable : nullptr;
    nebula_log("[SMG2] SystemDataTable.arc loaded: GalaxyDataTable=%d rows, GalaxyWorldOrderList=%d rows, MarioFaceShipEventDataTable=%d rows",
               galaxyTable != nullptr ? galaxyTable->getNumEntries() : -1,
               worldOrder != nullptr ? worldOrder->getNumEntries() : -1,
               shipEvents != nullptr ? shipEvents->getNumEntries() : -1);

    if (worldOrder != nullptr && worldOrder->getNumEntries() > 0) {
        for (int i = 0; i < worldOrder->getNumEntries() && i < 5; ++i) {
            const char* worldName = readSampleText(worldOrder, i, "WorldName");
            if (worldName != nullptr) {
                nebula_log("[SMG2]   World[%d]: %s", i, worldName);
            }
        }
    }
    if (galaxyTable != nullptr && galaxyTable->getNumEntries() > 0) {
        for (int i = 0; i < galaxyTable->getNumEntries() && i < 5; ++i) {
            const char* galaxyName = readSampleText(galaxyTable, i, "GalaxyName");
            if (galaxyName != nullptr) {
                nebula_log("[SMG2]   Galaxy[%d]: %s", i, galaxyName);
            }
        }
    }

    sScenarioDataParser = pGameSystem->mSceneController != nullptr ? pGameSystem->mSceneController->getScenarioDataParser() : nullptr;
    s32 totalPowerStars = 0;
    s32 validWorldGalaxies = 0;
    for (ScenarioDataIter it = MR::makeBeginScenarioDataIter(); !it.isEnd(); it.goNext()) {
        GalaxyStatusAccessor acc = it.makeAccessor();
        validWorldGalaxies++;
        totalPowerStars += acc.getPowerStarNum();
    }
    nebula_log("[SMG2] ScenarioDataParser loaded %d scenario archives (%d valid-world galaxies, %d total PowerStars)",
               sScenarioDataParser != nullptr ? sScenarioDataParser->mScenarioData.size() : 0, validWorldGalaxies, totalPowerStars);
    if (sScenarioDataParser != nullptr) {
        for (const char* probe : {"IslandFleetGalaxy", "YosshiHomeGalaxy"}) {
            if (ScenarioData* sc = sScenarioDataParser->getScenarioData(probe)) {
                nebula_log("[SMG2]   Scenario[%s]: world=%u, stars=%d (normal=%d), zones=%d",
                           sc->mGalaxyName, sc->getWorldNo(), sc->getPowerStarNum(), sc->getNormalPowerStarNum(), sc->getZoneNum());
            }
        }
    }

    nebula_log("[SMG2] GameDataHolder ready: user='%s', stars=%d, world=%d",
               GameDataFunction::getUserName(),
               GameDataFunction::calcCurrentPowerStarNum(),
               GameDataFunction::getCurrentWorldId());

    // Exercise Garigari's real 12-TU PlanetGravity engine.
    PlanetGravityManager gravityMgr("PlanetGravityManager");
    PointGravity pointGrav;
    pointGrav.setOrig(TVec3f(0.0f, 0.0f, 0.0f));
    pointGrav.mRange = 5000.0f;
    CubeGravity cubeGrav;
    DiskGravity diskGrav;
    gravityMgr.registerGravity(&pointGrav);
    gravityMgr.registerGravity(&cubeGrav);
    gravityMgr.registerGravity(&diskGrav);
    TVec3f gravVec(0.0f, 0.0f, 0.0f);
    GravityInfo gravInfo{};
    const bool hasGrav = gravityMgr.calcTotalGravityVector(&gravVec, &gravInfo, TVec3f(0.0f, 1000.0f, 0.0f), GRAVITY_TYPE_NORMAL, 0, nullptr);
    nebula_log("[SMG2] PlanetGravityManager test at (0,1000,0): active=%d, vec=(%.3f, %.3f, %.3f), priority=%d",
               hasGrav ? 1 : 0, gravVec.x, gravVec.y, gravVec.z, gravInfo.mLargestPriority);

    // Exercise real RailGraph & ActorStateKeeper.
    RailGraph railGraph;
    const s32 n0 = railGraph.addNode(TVec3f(0.0f, 0.0f, 0.0f));
    const s32 n1 = railGraph.addNode(TVec3f(300.0f, 400.0f, 0.0f));
    railGraph.connectNodeTwoWay(n0, n1, nullptr);
    ActorStateKeeper stateKeeper(4);
    nebula_log("[SMG2] RailGraph distance(0->1)=%.1f, ActorStateKeeper dead=%d",
               railGraph.getEdge(0)->mDistance, stateKeeper.isStateDead() ? 1 : 0);

    uint32_t frame = 0;
    while (true) {
        if (host != nullptr && host->frame_wait != nullptr) {
            host->frame_wait(host->user);
        }
        if (host != nullptr && host->frame_begun != nullptr) {
            host->frame_begun(host->user, 0, frame);
        }
        pGameSystem->frameLoop();
        Smg2Render::submitFrame(frame);
        if (host != nullptr && host->test_frame_step != nullptr) {
            host->test_frame_step(host->user);
            break;
        }
        frame++;
        if (host == nullptr || host->frame_wait == nullptr) {
            break;
        }
    }
}
