#include "Game/System/GameSystemStationedArchiveLoader.hpp"
#include "Game/System/ResourceHolderManager.hpp"
#include "Game/Util/SingletonHolder.hpp"
#include <nebula_module.h>

GameSystemStationedArchiveLoader::GameSystemStationedArchiveLoader()
    : mState(0), mIsDone(false), mIsPreparedReset(false) {
}

void GameSystemStationedArchiveLoader::update() {
    if (mIsDone || mIsPreparedReset) {
        return;
    }

    if (SingletonHolder<ResourceHolderManager>::exists()) {
        // Mount SystemDataTable into stationed resource registry if available in game.pak.
        SingletonHolder<ResourceHolderManager>::get()->createAndAddStationed("/ObjectData/SystemDataTable.arc");
    }

    nebula_log("[SMG2] GameSystemStationedArchiveLoader: stationed boot tables ready (TODO(Garigari): full StationedFileInfo async queue)");
    mState = 1;
    mIsDone = true;
}

bool GameSystemStationedArchiveLoader::isDone() const {
    return mIsDone;
}

void GameSystemStationedArchiveLoader::prepareReset() {
    mIsPreparedReset = true;
}

bool GameSystemStationedArchiveLoader::isPreparedReset() const {
    return mIsPreparedReset;
}
