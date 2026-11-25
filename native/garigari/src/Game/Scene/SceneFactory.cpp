#include "Game/Scene/SceneFactory.hpp"
#include "Game/Scene/GameScene.hpp"
#include "Game/Scene/LogoScene.hpp"
#include <cstring>
#include <nebula_module.h>

namespace MR {
    Scene* createScene(const char* pSceneName) {
        if (pSceneName == nullptr) {
            return nullptr;
        }
        if (std::strcmp(pSceneName, "Logo") == 0) {
            return new LogoScene();
        }
        if (std::strcmp(pSceneName, "Game") == 0) {
            return new GameScene();
        }
        nebula_log("[SMG2-SCENE] TODO(Garigari): SceneFactory::createScene('%s') not yet wired", pSceneName);
        return nullptr;
    }
}  // namespace MR
