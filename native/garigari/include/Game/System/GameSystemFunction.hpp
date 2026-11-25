#pragma once

#include <revolution/types.h>

class NameObjHolder;

namespace GameSystemFunction {
    bool isResetProcessing();
    bool isOccurredSystemWarning();
    void setSceneNameObjHolderToNameObjRegister();
    void restartGameSystem();
    bool isDoneLoadSystemArchive();
    void startToLoadSystemArchive();
}  // namespace GameSystemFunction
