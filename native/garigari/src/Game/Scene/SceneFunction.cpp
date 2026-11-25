#include "Game/Scene/SceneFunction.hpp"
#include "Game/NameObj/NameObjHolder.hpp"
#include "Game/NameObj/NameObjRegister.hpp"
#include "Game/Util/SingletonHolder.hpp"

void SceneFunction::createHioBasicNode(Scene*) {
}

void SceneFunction::initForNameObj() {
}

void SceneFunction::allocateDrawBufferActorList() {
    // TODO(Garigari): allocate J3D DrawBuffer actor lists once DrawBufferHolder is wired.
}

void SceneFunction::executeMovementList() {
}

void SceneFunction::executeCalcAnimList() {
    // TODO(Garigari): execute NameObj calcAnim list once full 3D actor anims are wired (M5).
}

void SceneFunction::executeCalcViewAndEntryList2D() {
    // 2D layout draw items are submitted via Smg2Render.
}

void SceneFunction::executeCalcViewAndEntryList() {
    // TODO(Garigari): execute 3D J3D view/entry list once ModelData/J3D bridge is wired.
}

void CategoryList::execute(MR::DrawType) {
}

void CategoryList::drawOpa(MR::DrawBufferType) {
}

void CategoryList::drawXlu(MR::DrawBufferType) {
}
