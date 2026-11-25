# SMG2 (Garigari) game module CMakeLists.txt
# Compiles decompiled SMG2 translation units directly from SMGCommunity/Garigari
# with Nebula's native compat shim layer into smg_game.dll.
cmake_minimum_required(VERSION 3.24)
project(NebulaSmg2Game LANGUAGES C CXX)

if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    message(FATAL_ERROR "The SMG2 game module must be built with clang's GNU driver (clang++, not clang-cl).")
endif()

set(NEBULA_GARIGARI_DIR "${CMAKE_CURRENT_LIST_DIR}/../garigari" CACHE PATH "Path to Garigari SMG2 source tree")
if(NOT EXISTS "${NEBULA_GARIGARI_DIR}/configure.py")
    message(FATAL_ERROR "NEBULA_GARIGARI_DIR must point at a Garigari checkout ('${NEBULA_GARIGARI_DIR}')")
endif()

set(NEBULA_GAME_VERSION "SB4E01" CACHE STRING "SMG2 game release")
set(NEBULA_CPU_LEVEL "x86-64-v3" CACHE STRING "CPU level (-march)")

set(garigari "${NEBULA_GARIGARI_DIR}")
set(nebula_native "${CMAKE_CURRENT_LIST_DIR}/..")
set(nebula_root "${nebula_native}/..")

# Real unmodified & reconstructed Garigari C++ translation units wired across Phases 4, 6, and M2 (60 total).
set(garigari_sources
    "${garigari}/src/Game/System/NerveExecutor.cpp"
    "${garigari}/src/Game/System/GameSystem.cpp"
    "${garigari}/src/Game/System/GameSystemDataHolder.cpp"
    "${garigari}/src/Game/System/GameSystemDimmingWatcher.cpp"
    "${garigari}/src/Game/System/GameSystemFrameControl.cpp"
    "${garigari}/src/Game/System/GameSystemFunction.cpp"
    "${garigari}/src/Game/System/GameSystemObjHolder.cpp"
    "${garigari}/src/Game/System/GameSystemSceneController.cpp"
    "${garigari}/src/Game/System/GameSystemStationedArchiveLoader.cpp"
    "${garigari}/src/Game/System/GameDataFunction.cpp"
    "${garigari}/src/Game/System/GameDataHolder.cpp"
    "${garigari}/src/Game/System/GameSequenceDirector.cpp"
    "${garigari}/src/Game/System/GameSequenceFunction.cpp"
    "${garigari}/src/Game/System/GameSequenceTitle.cpp"
    "${garigari}/src/Game/System/Language.cpp"
    "${garigari}/src/Game/System/ResourceHolderManager.cpp"
    "${garigari}/src/Game/System/ResourceInfo.cpp"
    "${garigari}/src/Game/System/SaveDataFileAccessor.cpp"
    "${garigari}/src/Game/System/SaveDataHandleSequence.cpp"
    "${garigari}/src/Game/System/SaveDataHandler.cpp"
    "${garigari}/src/Game/System/SaveRequest.cpp"
    "${garigari}/src/Game/System/ScenarioDataParser.cpp"
    "${garigari}/src/Game/LiveActor/ActorStateBase.cpp"
    "${garigari}/src/Game/LiveActor/ActorStateKeeper.cpp"
    "${garigari}/src/Game/LiveActor/LiveActorFlag.cpp"
    "${garigari}/src/Game/LiveActor/Spine.cpp"
    "${garigari}/src/Game/NameObj/MovementOnOffGroupHolder.cpp"
    "${garigari}/src/Game/NameObj/NameObj.cpp"
    "${garigari}/src/Game/NameObj/NameObjAdaptor.cpp"
    "${garigari}/src/Game/NameObj/NameObjCategoryList.cpp"
    "${garigari}/src/Game/NameObj/NameObjHolder.cpp"
    "${garigari}/src/Game/NameObj/NameObjRegister.cpp"
    "${garigari}/src/Game/NameObj/NameObjGroup.cpp"
    "${garigari}/src/Game/Scene/Scene.cpp"
    "${garigari}/src/Game/Scene/SceneObjHolder.cpp"
    "${garigari}/src/Game/Scene/SceneFactory.cpp"
    "${garigari}/src/Game/Scene/SceneFunction.cpp"
    "${garigari}/src/Game/Scene/LogoScene.cpp"
    "${garigari}/src/Game/Scene/GameScene.cpp"
    "${garigari}/src/Game/Scene/StageDataHolder.cpp"
    "${garigari}/src/Game/Util/HashUtil.cpp"
    "${garigari}/src/Game/Util/JMapIdInfo.cpp"
    "${garigari}/src/Game/Util/JMapUtil.cpp"
    "${garigari}/src/Game/Util/MathUtil.cpp"
    "${garigari}/src/Game/Util/MtxUtil.cpp"
    "${garigari}/src/Game/Util/TriggerChecker.cpp"
    "${garigari}/src/Game/Util/SpringValue.cpp"
    "${garigari}/src/Game/Util/BothDirList.cpp"
    "${garigari}/src/Game/Gravity/BarrelGravity.cpp"
    "${garigari}/src/Game/Gravity/ConeGravity.cpp"
    "${garigari}/src/Game/Gravity/CubeGravity.cpp"
    "${garigari}/src/Game/Gravity/DiskGravity.cpp"
    "${garigari}/src/Game/Gravity/DiskTorusGravity.cpp"
    "${garigari}/src/Game/Gravity/GravityInfo.cpp"
    "${garigari}/src/Game/Gravity/ParallelGravity.cpp"
    "${garigari}/src/Game/Gravity/PlanetGravity.cpp"
    "${garigari}/src/Game/Gravity/PlanetGravityManager.cpp"
    "${garigari}/src/Game/Gravity/PointGravity.cpp"
    "${garigari}/src/Game/Gravity/SegmentGravity.cpp"
    "${garigari}/src/Game/Gravity/WireGravity.cpp"
    "${garigari}/src/Game/Map/FileSelectIconID.cpp"
    "${garigari}/src/Game/Map/RailGraph.cpp"
    "${garigari}/src/Game/Map/RailGraphEdge.cpp"
    "${garigari}/src/Game/Map/RailGraphIter.cpp"
    "${garigari}/src/Game/Map/RailGraphNode.cpp"
    "${garigari}/src/Game/AreaObj/AreaForm.cpp"
    "${garigari}/src/Game/Camera/CameraDirector.cpp"
    "${garigari}/src/Game/Player/MarioActor.cpp"
    "${garigari}/src/Game/AudioLib/AudSystemWrapper.cpp"
    "${garigari}/src/Game/GameAudio/AudStageBgmTable.cpp"
)

set(smg2_bridge_sources
    src/Smg2Module.cpp
    src/Smg2JMapInfo.cpp
    src/Smg2CompatMath.cpp
    src/Smg2CompatRuntime.cpp
    src/Smg2GameSystem.cpp
    src/Smg2Render.cpp
    src/Smg2Stubs.cpp
)

set(game_runtime_sources
    "${nebula_native}/math/src/nebula_math.c"
    "${nebula_native}/math/src/nebula_libm.c"
    "${nebula_native}/math/src/nebula_libc.c"
    "${nebula_native}/petari/src/Game/System/NebulaFormat.c"
    $<$<PLATFORM_ID:Windows>:${nebula_native}/math/src/nebula_runtime_win.c>
)

add_library(smg2_petari_ui STATIC
    "${nebula_native}/petari/src/Game/Ui/Animation.cpp"
    "${nebula_native}/petari/src/Game/Ui/Bounding.cpp"
    "${nebula_native}/petari/src/Game/Ui/CharStrmReader.cpp"
    "${nebula_native}/petari/src/Game/Ui/CharWriter.cpp"
    "${nebula_native}/petari/src/Game/Ui/DrawInfo.cpp"
    "${nebula_native}/petari/src/Game/Ui/Font.cpp"
    "${nebula_native}/petari/src/Game/Ui/Group.cpp"
    "${nebula_native}/petari/src/Game/Ui/Layout.cpp"
    "${nebula_native}/petari/src/Game/Ui/Material.cpp"
    "${nebula_native}/petari/src/Game/Ui/Pane.cpp"
    "${nebula_native}/petari/src/Game/Ui/Picture.cpp"
    "${nebula_native}/petari/src/Game/Ui/ResFont.cpp"
    "${nebula_native}/petari/src/Game/Ui/ResFontBase.cpp"
    "${nebula_native}/petari/src/Game/Ui/ResourceAccessor.cpp"
    "${nebula_native}/petari/src/Game/Ui/TagProcessorBase.cpp"
    "${nebula_native}/petari/src/Game/Ui/TexMap.cpp"
    "${nebula_native}/petari/src/Game/Ui/TextBox.cpp"
    "${nebula_native}/petari/src/Game/Ui/TextWriterBase.cpp"
    "${nebula_native}/petari/src/Game/Ui/UiCommon.cpp"
    "${nebula_native}/petari/src/Game/Ui/UiInit.cpp"
    "${nebula_native}/petari/src/Game/Ui/Window.cpp"
    "${nebula_native}/petari/src/Game/Render/Clip.cpp"
    "${nebula_native}/petari/src/Game/Render/FrameList.cpp"
    "${nebula_native}/petari/src/Game/Render/GameTexture.cpp"
    "${nebula_native}/petari/src/Game/Render/Material.cpp"
    "${nebula_native}/petari/src/Game/Render/Model.cpp"
    "${nebula_native}/petari/src/Game/Render/ModelData.cpp"
    "${nebula_native}/petari/src/Game/Render/RenderTexture.cpp"
    "${nebula_native}/petari/src/Game/Render/Transform.cpp"
    "${nebula_native}/petari/src/Game/Render/TypedDraw.cpp"
    "${nebula_native}/petari/src/Game/System/NebulaArena.cpp"
    "${nebula_native}/petari/src/Game/System/Arenas.cpp"
    "${nebula_native}/petari/src/Game/System/GameAssets.cpp"
    "${nebula_native}/petari/src/Game/System/AssetFolder.cpp"
    "${nebula_native}/petari/src/Game/System/NebulaFrameClock.cpp"
    "${nebula_native}/petari/src/Game/System/GameFrame.cpp"
    "${nebula_native}/petari/src/Game/System/LayoutHolder.cpp"
    "${nebula_native}/petari/src/Game/System/GameSystemFontHolder.cpp"
    "${nebula_native}/petari/src/Game/System/NebulaText.cpp"
    "${nebula_native}/petari/src/Game/Screen/LayoutManager.cpp"
    "${nebula_native}/petari/src/Game/Screen/LayoutAspect.cpp"
    "${nebula_native}/petari/src/Game/Screen/LayoutCoreUtil.cpp"
    "${nebula_native}/petari/src/Game/Screen/LayoutGroupCtrl.cpp"
    "${nebula_native}/petari/src/Game/Screen/LayoutPaneCtrl.cpp"
    "${nebula_native}/petari/src/Game/Animation/LayoutAnmPlayer.cpp"
    "${nebula_native}/petari/src/Game/Screen/ReplaceTagProcessor.cpp"
    "${nebula_native}/petari/src/Game/Screen/CustomTagProcessor.cpp"
    "${nebula_native}/petari/src/Game/Screen/MessageTagSkipTagProcessor.cpp"
    "${nebula_native}/petari/src/Game/Util/DrawUtil.cpp"
    "${nebula_native}/petari/src/Game/Util/DirectDraw.cpp"
    "${nebula_native}/petari/src/Game/Util/StringUtil.cpp"
    "${nebula_native}/petari/src/Game/Util/MemoryUtil.cpp"
    "${nebula_native}/petari/src/Game/Util/TextString.cpp"
    src/Smg2PetariBridge.cpp
)

target_include_directories(smg2_petari_ui BEFORE PRIVATE
    "${nebula_native}/math/include"
    "${nebula_native}/petari/include"
    "${nebula_native}/include"
)

target_compile_definitions(smg2_petari_ui PRIVATE
    VERSION=1
    RMGJ01=0
    RMGE01=1
    RMGP01=2
    RMGK01=3
    NEBULA_GAME_MODULE=1
    _HAS_EXCEPTIONS=0
    _CRT_SECURE_NO_WARNINGS
    _USE_STD_VECTOR_ALGORITHMS=0
    _ACRTIMP=
)

set_target_properties(smg2_petari_ui PROPERTIES
    C_STANDARD 11 C_EXTENSIONS ON
    CXX_STANDARD 17 CXX_EXTENSIONS ON
    C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON
)

target_compile_options(smg2_petari_ui PRIVATE
    -ffp-contract=off
    -fno-strict-aliasing
    -fno-math-errno
    -fno-stack-protector
    -fno-delayed-template-parsing
    -Wno-unknown-pragmas
    -Wno-unused-parameter
    -Wno-ignored-qualifiers
    -Wno-deprecated-declarations
    -Wno-c++11-narrowing
    -Wno-writable-strings
    -Wno-gcc-compat
    -mstack-probe-size=65536
    -Wframe-larger-than=16384
    "SHELL:-include nebula_prelude.h"
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti -fno-c++-static-destructors -fno-sized-deallocation -fno-aligned-allocation -fcheck-new>
)

if(NEBULA_CPU_LEVEL)
    target_compile_options(smg2_petari_ui PRIVATE -march=${NEBULA_CPU_LEVEL})
endif()

add_library(smg_game SHARED ${garigari_sources} ${smg2_bridge_sources} ${game_runtime_sources})
target_link_libraries(smg_game PRIVATE smg2_petari_ui)

# Compat headers MUST precede Garigari/include and Garigari/libs/*/include.
target_include_directories(smg_game BEFORE PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/compat"
    "${nebula_native}/math/include"
    "${nebula_native}/include"
    "${garigari}/include"
    "${garigari}/libs/RVL_SDK/include"
    "${garigari}/libs/JSystem/include"
    "${garigari}/libs/nw4r/include"
)

# Garigari checks VERSION == SB4E01 (e.g. Game/System/Language.cpp).
target_compile_definitions(smg_game PRIVATE
    VERSION=1
    SB4J01=0
    SB4E01=1
    SB4P01=2
    SB4K01=3
    SB4W01=4
    NEBULA_GAME_MODULE=1
    _HAS_EXCEPTIONS=0
    _CRT_SECURE_NO_WARNINGS
    _USE_STD_VECTOR_ALGORITHMS=0
    _ACRTIMP=
)

set_target_properties(smg_game PROPERTIES
    C_STANDARD 11 C_EXTENSIONS ON
    CXX_STANDARD 17 CXX_EXTENSIONS ON
    C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON
)

target_compile_options(smg_game PRIVATE
    -ffp-contract=off
    -fno-strict-aliasing
    -fno-math-errno
    -fno-stack-protector
    -fno-delayed-template-parsing
    -Wno-unknown-pragmas
    -Wno-unused-parameter
    -Wno-ignored-qualifiers
    -Wno-deprecated-declarations
    -Wno-c++11-narrowing
    -Wno-writable-strings
    -Wno-gcc-compat
    -mstack-probe-size=65536
    -Wframe-larger-than=16384
    "SHELL:-include smg2_prelude.h"
    $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti -fno-c++-static-destructors -fno-sized-deallocation -fno-aligned-allocation -fcheck-new>
)

if(NEBULA_CPU_LEVEL)
    target_compile_options(smg_game PRIVATE -march=${NEBULA_CPU_LEVEL})
endif()

set_source_files_properties(
    "${garigari}/src/Game/Util/MathUtil.cpp"
    PROPERTIES COMPILE_DEFINITIONS "vecScaleAdd=vecScaleAdd_ppc;PSvecBlend=PSvecBlend_ppc;vecBlend=vecBlend_ppc;PSVECKillElement=PSVECKillElement_ppc;vecKillElement=vecKillElement_ppc"
)

set_source_files_properties(
    "${nebula_native}/math/src/nebula_libm.c"
    "${nebula_native}/math/src/nebula_libc.c"
    "${nebula_native}/math/src/nebula_runtime_win.c"
    PROPERTIES COMPILE_OPTIONS "-fno-builtin;-fno-lto"
)

if(WIN32)
    target_link_options(smg_game PRIVATE -nostdlib -fuse-ld=lld "LINKER:/NOENTRY" "LINKER:/NODEFAULTLIB" "LINKER:/errorlimit:0")
endif()

# Native smoke test runner: opens SB4E01 game.pak with Nebula's AssetStore, attaches smg_game.dll,
# and runs Garigari's real GameSystemDataHolder::init() against SystemDataTable.arc.
add_executable(smg2_smoke_test
    tests/smg2_smoke_test.cpp
    "${nebula_root}/runtime/src/asset_store.cpp"
)
target_include_directories(smg2_smoke_test PRIVATE
    "${nebula_native}/include"
    "${nebula_root}/runtime/include"
)
target_compile_features(smg2_smoke_test PRIVATE cxx_std_20)
target_compile_definitions(smg2_smoke_test PRIVATE _CRT_SECURE_NO_WARNINGS)
add_dependencies(smg2_smoke_test smg_game)
