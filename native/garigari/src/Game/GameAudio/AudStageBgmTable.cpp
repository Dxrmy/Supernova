#include "Game/GameAudio/AudStageBgmTable.hpp"
#include <cstring>

namespace AudStageBgmTable {
    const char* getBgmName(const char* pStageName) {
        if (pStageName == nullptr) {
            return "STM_TITLE";
        }
        if (std::strcmp(pStageName, "Title") == 0) {
            return "STM_TITLE";
        }
        if (std::strcmp(pStageName, "FileSelect") == 0) {
            return "STM_FILE_SELECT";
        }
        if (std::strcmp(pStageName, "IslandFleetGalaxy") == 0) {
            return "STM_ISLAND_FLEET";
        }
        if (std::strcmp(pStageName, "YosshiHomeGalaxy") == 0) {
            return "STM_YOSSHI_HOME";
        }
        if (std::strcmp(pStageName, "MarioFaceShipGalaxy") == 0) {
            return "STM_STARSHIP_MARIO";
        }
        return "STM_GALAXY_COMMON";
    }
}  // namespace AudStageBgmTable
