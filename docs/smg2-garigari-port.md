# Supernova — Super Mario Galaxy 2 (`SB4E01`) PC Port

## Overview

- **Project Name:** **Supernova** (built on `keepvaibin/nebula` host/renderer/importer + `SMGCommunity/Garigari` `SB4E01` fork)
- **Host & Runtime Reference:** `keepvaibin/nebula` (`C:\Users\kmric\projects\nebula`, base commit `e957eb63`)
- **SMG2 Game Codebase:** `C:\Users\kmric\projects\garigari-fork` (fork of `SMGCommunity/Garigari` `b0a4c566`; upstream `C:\Users\kmric\projects\Garigari` kept completely unmodified)
- **Target Disc Image:** `D:\Roms\Super Mario Galaxy 2 (USA) (En,Fr,Es).iso` (`SB4E01`, Rev 0 USA, `SUPER MARIO GALAXY MORE`, expected `main.dol` SHA-1 `7d79810889824edd01c4ff02624713060a69818c`)

---

## 1. Gap Report

### (a) Cleanly Mappable Between Nebula and Garigari

1. **Host ↔ Game Module C ABI (`native/include/nebula_game_api.h`, v164):**
   - Plain C boundary (`NebulaHostServices`, `nebula_game_attach`, `nebula_game_run`, `nebula_game_mod_api`, `nebula_game_sound_frame`, `nebula_game_mii_valid`, `nebula_game_mii_changed`, `nebula_game_luma_describe`) is game-agnostic and decouples the host C++ runtime from the game module's C++/SDK types.
2. **Wii Disc Decryption & Asset Cooking (`crates/nebula-import`):**
   - `nod` disc/partition parsing, Yaz0/RARC archive unpacking, J3D models (`.bmd`/`.bdl`), animations (`.bck`/`.btk`/`.brk`/`.bpk`/`.btp`/`.bva`/`.bca`), `.bti`/`.tpl` textures, BCSV→`NTBL` tables, KCL collision octrees, `.jpc` particles, `.brlyt`/`.brlan`/`.brfnt` UI layouts/fonts, `.thp` movies, `.ast` streams, and `game.pak` store indexing carry over structurally between SMG1 and SMG2.
3. **Decompiled Garigari Subsystems (`src/Game/`, 217 files across 15 directories):**
   - `Animation` (1), `AreaObj` (4), `Enemy` (10), `Gravity` (15), `LiveActor` (19), `Map` (14), `MapObj` (44), `NPC` (4), `NameObj` (8), `Ride` (15), `Scene` (4), `Screen` (15), `System` (22), `Util` (22), `World` (20).
   - Core system, name-object, scene, table, and math primitives compile natively with Clang x86-64 once wrapped behind Nebula's compat shims.

### (b) SMG1-Specific Code in Nebula Updated for SMG2

| Component | SMG1 Assumption | SMG2 (`SB4E01`) Update |
|---|---|---|
| `crates/nebula-import/src/lib.rs` | Only `RMGE01`, `RMGP01`, `RMGJ01` and `RMG` prefix accepted; root-level language folders | Added `SB4E01` (`7d79810889824edd01c4ff02624713060a69818c`), `SB4` prefix, `Release::game_title()`, and `LocalizeData/` language folder scanning (`UsEnglish`, `UsFrench`, `UsSpanish`) |
| `crates/nebula-import/src/cook/dol.rs` | Extracts `StoryProgressTable.bcsv`, `GalaxyInfoTable.bcsv`, `ErrorMessageArchive.arc` from fixed SMG1 `main.dol` addresses (`0x8053_C620`, `0x8053_C800`, `0x8053_DFE0`) | Skips SMG1 DOL table extraction for `SB4*`; SMG2 keeps `GalaxyDataTable.bcsv`, `GalaxyWorldOrderList.bcsv`, etc. in `/ObjectData/SystemDataTable.arc` |
| `crates/nebula-import/src/cook/sound.rs` | `track_bound()` treated BMS `OpenTrack` cycles as errors | Breaks `OpenTrack` cycles (`return Ok(1)` with `TODO(SMG2)`) so SMG2 `AudioRes/SMR.szs` BSC sequences cook cleanly |
| `crates/nebula-import/src/cook/ghost.rs` | Strict Shift-JIS animation names and SMG1-only `0x2000` animation hashes | Decodes animation names lossily and preserves unknown `0x2000` hashes with `TODO(SMG2)` (`GhostAttackGhostData01.gst`) |
| `crates/nebula-import/src/cook/pipeline_list.rs` & `cook/mod.rs` | Hard error on any material shader program absent from SMG1's precompiled `table.tsv` | Added `Pipelines::checked_bytes_for_game()` to emit `NOTE TODO(SMG2)` and write `Pipelines.bin` for `SB4*` while keeping strict checks for `RMG*` |
| `native/host/host_main.cpp` | Validates `gPak.id` only against `RMGE01`/`RMGP01`/`RMGJ01`; defaults `game_id` to `RMGE01` | Accepts `SB4E01`/`SB4P01`/`SB4J01` and defaults `game_id` to `SB4E01` |
| `runtime/src/save_transfer.cpp` | `kReleaseCodes = {"RMGE", "RMGP", "RMGJ", "RMGK"}` | Added `"SB4E"`, `"SB4P"`, `"SB4J"`, `"SB4K"` (`53423445`, etc.) plus `same_game_family()` enforcement so SMG1 and SMG2 saves never cross-import |
| `runtime/include/galaxy/render/texture_pack.h`, `runtime/src/render/render_config.cpp`, `runtime/src/render/texture_pack_tests.cpp` | `kGameId = "RMGE01"`, static shader cache fallback `"RMGE01"` | Defaults to `"SB4E01"` and resolves `shader_cache_directory()` dynamically from `texture_pack_game_id()` |
| `runtime/src/dolphin_pack.cpp` & `runtime/src/mods.cpp` | Recognizes `RMG` / `RMGE01` pack/Riivolution folders | Added `sb4` / `SB4E01` recognition alongside `rmg` |
| `runtime/src/ui/native_settings.cpp`, `runtime/src/discord_presence.cpp`, `runtime/src/luma_system.cpp` | Default `"RMGE01"` and `"Super Mario Galaxy (Nebula)"` text | Defaults to `"SB4E01"` and `"Super Mario Galaxy 2 (Nebula)"` |
| `native/CMakeLists.txt` & `native/build.ps1` | Hardcoded to `native/petari` and `RMGE01` | Added standalone `native/smg2/CMakeLists.txt` and `native/build_smg2.ps1` targeting `Garigari` (`SB4E01`) |

### (c) Missing / Incomplete in Garigari (`b0a4c566`, ~141 / 2355 Matching Objects)

1. **Missing `src/Game/` Subsystems (14 directories present in Nebula's Petari, absent in Garigari `src/Game/`):**
   - `AudioLib`, `Boss`, `Camera`, `Demo`, `Effect`, `GameAudio`, `Mii`, `Particle`, `Player`, `Render`, `RhythmLib`, `Sound`, `Speaker`, `Ui`.
2. **Missing Core System / Scene / Save Translation Units:**
   - `GameSystem.cpp` (only `GameSystem.hpp` exists)
   - `GameSystemObjHolder.cpp`, `GameSystemSceneController.cpp`, `GameSystemFunction.cpp`, `GameSystemResetAndPowerProcess.cpp`, `GameSystemStationedArchiveLoader.cpp`
   - `GameSequenceDirector.cpp`, `GameSequenceFunction.cpp`, `GameSequenceWorldMap.cpp`
   - `GameDataHolder.cpp`, `GameDataFunction.cpp`, `GameDataWorldMapStorage.cpp`
   - `SaveDataHandler.cpp`, `SaveDataFileAccessor.cpp`, `SaveDataHandleSequence.cpp`
   - `GameScene.cpp` and `LogoScene.cpp` exist under `Garigari/src/Game/Scene/` (unwired until dependent player/camera/audio subsystems are decompiled)
3. **Render Shader Programs:**
   - Importing `SB4E01` yields **5,539** material pipelines, of which **3,084** are SMG2 shader programs not yet shipped in `runtime/src/render/programs/table.tsv` (requires running `tools/dev/build_programs.py` against DXC/SPIRV-Cross/Metal before full 3D rendering).

---

## 2. SMG2 Game Module (`native/smg2/`) Implementation Details

### Wired Garigari Translation Units (46 unmodified `.cpp` files)

Compiled directly from `${NEBULA_GARIGARI_DIR}` in `native/smg2/CMakeLists.txt`:

**System (7):**
1. `src/Game/System/NerveExecutor.cpp`
2. `src/Game/System/GameSystemDataHolder.cpp`
3. `src/Game/System/GameSystemFrameControl.cpp`
4. `src/Game/System/GameSystemDimmingWatcher.cpp`
5. `src/Game/System/Language.cpp`
6. `src/Game/System/ResourceInfo.cpp`
7. `src/Game/System/ScenarioDataParser.cpp`

**LiveActor (4):**
8. `src/Game/LiveActor/ActorStateBase.cpp`
9. `src/Game/LiveActor/ActorStateKeeper.cpp`
10. `src/Game/LiveActor/LiveActorFlag.cpp`
11. `src/Game/LiveActor/Spine.cpp`

**NameObj & Scene (9):**
12. `src/Game/NameObj/MovementOnOffGroupHolder.cpp`
13. `src/Game/NameObj/NameObj.cpp`
14. `src/Game/NameObj/NameObjAdaptor.cpp`
15. `src/Game/NameObj/NameObjCategoryList.cpp`
16. `src/Game/NameObj/NameObjHolder.cpp`
17. `src/Game/NameObj/NameObjRegister.cpp`
18. `src/Game/NameObj/NameObjGroup.cpp`
19. `src/Game/Scene/Scene.cpp`
20. `src/Game/Scene/SceneObjHolder.cpp`

**Gravity Engine (12):**
21. `src/Game/Gravity/BarrelGravity.cpp`
22. `src/Game/Gravity/ConeGravity.cpp`
23. `src/Game/Gravity/CubeGravity.cpp`
24. `src/Game/Gravity/DiskGravity.cpp`
25. `src/Game/Gravity/DiskTorusGravity.cpp`
26. `src/Game/Gravity/GravityInfo.cpp`
27. `src/Game/Gravity/ParallelGravity.cpp`
28. `src/Game/Gravity/PlanetGravity.cpp`
29. `src/Game/Gravity/PlanetGravityManager.cpp`
30. `src/Game/Gravity/PointGravity.cpp`
31. `src/Game/Gravity/SegmentGravity.cpp`
32. `src/Game/Gravity/WireGravity.cpp`

**Util (8):**
33. `src/Game/Util/HashUtil.cpp`
34. `src/Game/Util/JMapIdInfo.cpp`
35. `src/Game/Util/JMapUtil.cpp`
36. `src/Game/Util/MathUtil.cpp`
37. `src/Game/Util/MtxUtil.cpp`
38. `src/Game/Util/TriggerChecker.cpp`
39. `src/Game/Util/SpringValue.cpp`
40. `src/Game/Util/BothDirList.cpp`

**Map & AreaObj (6):**
41. `src/Game/Map/FileSelectIconID.cpp`
42. `src/Game/Map/RailGraph.cpp`
43. `src/Game/Map/RailGraphEdge.cpp`
44. `src/Game/Map/RailGraphIter.cpp`
45. `src/Game/Map/RailGraphNode.cpp`
46. `src/Game/AreaObj/AreaForm.cpp`

### Compatibility Header Shims (`native/smg2/compat/`)

- `smg2_prelude.h`, `size_t.h`, `va_list.h`, `mem.h`, `algorithm` (CodeWarrior intrinsics `__fabs`/`__fabsf`/`__abs`/`__frsqrte`/`__fpclassifyf`, `register` keyword compatibility, `std::mem_func` alias to `std::mem_fn`)
- `revolution/types.h`, `revolution/mtx.h` (maps `PSMTX*`/`PSVEC*`/`C_MTX*`/`C_QUAT*` onto `<nebula_math.h>`), `revolution/os.h`, `revolution/os/OSTime.h`, `revolution/gx/GXVert.h`
- `JSystem/JKernel/JKRHeap.hpp`, `JSystem/JKernel/JKRMemArchive.hpp`, `JSystem/JMath/JMATrigonometric.hpp`, `JSystem/JMath/JMath.hpp` (portable C++ replacements for CW asm `JMAFastSqrt`, `JMAHermiteInterpolation`, `gekko_ps_copy*`, `JMathInlineVEC`), `JSystem/JGeometry/TVec.hpp` (non-`__MWERKS__` vector ctors, `dot`, `squared`, `negateInternal`, `subInternal`)
- `Game/System/GameSystem.hpp`, `Game/Util/JMapInfo.hpp`

### Native Bridge, Headless Host & Isolated `TODO(Garigari)` Stubs (`native/smg2/`)

- `src/Smg2Module.cpp`: Exports all 7 `NEBULA_GAME_API` v164 symbols (`nebula_game_attach`, `nebula_game_run`, `nebula_game_mod_api`, `nebula_game_sound_frame`, `nebula_game_mii_valid`, `nebula_game_mii_changed`, `nebula_game_luma_describe`) and host allocator/logging hooks.
- `src/Smg2JMapInfo.cpp`: Reads Nebula cooked little-endian `NTBL` tables for Garigari's `JMapInfo` / `JMapInfoIter` API.
- `src/Smg2CompatMath.cpp`: Initializes JSystem `sSinCosTable`/`sAtanTable`/`sAsinAcosTable`, JMA vector/quat helpers, `JMASqrt`, `MR::fastSqrtf`, `MR::getRandom`, and x86-64 implementations for the PPC-asm functions in `MathUtil.cpp` (`vecScaleAdd`, `PSvecBlend`, `vecBlend`, `PSVECKillElement`, `vecKillElement`).
- `src/Smg2CompatRuntime.cpp`: Implements `OSReport`/`OSPanic`/`OSGetTick`/`OSGetTime`/`JKRHeap`/`JKRArchive`/`JKRMemArchive`, `DVDOpenDir`/`DVDReadDir`/`DVDCloseDir` (handling Nebula's flattened `/StageData/<Stage>Scenario` store layout), `GalaxyStatusAccessor`, and `NativePakArchive` / `MR::receiveArchive` / `MR::isFileExist` backed by `NebulaHostServices`.
- `src/Smg2GameSystem.cpp`: Bring-up `nebula_game_main()` initializing `NameObjRegister`, `GameSystem`, real `GameSystemDataHolder::init()` (mounting `/ObjectData/SystemDataTable`), `MR::initAcosTable()`, real `ScenarioDataParser` (all 62 SMG2 scenario archives), `PlanetGravityManager`, `RailGraph`, `ActorStateKeeper`, `SceneObjHolder`, `GameSystemDimmingWatcher`, and host frame pacing (`TODO(Garigari)` marker for missing `GameSystem.cpp`).
- `src/Smg2Stubs.cpp`: Isolated `TODO(Garigari)` stubs logging once on first invocation for undecompiled upstream symbols (`MR::notifyRequestNameObjMovementOnOff`, `NameObjListExecutor::init*`, `SceneNameObjListExecutor::init*`, `MainLoopFramework::setTickRateFromFrame`, `MR::requestMovementOn`/`Off`, `JMapLinkInfo`, `SceneUtil` stubs `MR::getCurrentStageName`/`getPlacedZoneId`/`isPlacementLocalStage`/`getZonePlacementMtx`, and undecompiled `MtxUtil` functions `MR::makeMtxRotate`/`makeMtxTR`/`makeMtxFrontUp`/`makeMtxSideUp`/`makeMtxUpFront`/`makeMtxUpNoSupport`). (`ActorStateKeeper` stub was removed once real `ActorStateKeeper.cpp` was wired in Phase 6.)
- `host/CMakeLists.txt`: Standalone MSVC C++20 CMake project building `nebula_native_host.exe` (`native/host/host_main.cpp` + runtime/Luma/libarchive/BoringSSL dependencies).

---

## 3. Verified Commands & Test Results

- **Disc Importer Tests (`cargo test -p nebula-import`):**
  - 315 unit tests passed (7 ignored), plus integration test suites `f_conformance` (13), `patch_c` (4), `shader_ids` (1), `validate_c` (11).
- **Disc Identification (`.\target\release\nebula-import.exe identify "D:\Roms\Super Mario Galaxy 2 (USA) (En,Fr,Es).iso"`):**
  - Exit 0: `gameId: "SB4E01"`, `title: "SUPER MARIO GALAXY MORE"`, `revision: 0`, `mainDolSha1: "7d79810889824edd01c4ff02624713060a69818c"`, `languages: ["UsEnglish","UsFrench","UsSpanish"]`, `supported: true`.
- **Disc Content Cook (`.\target\release\nebula-import.exe import "D:\Roms\Super Mario Galaxy 2 (USA) (En,Fr,Es).iso" --output build\smg2_content`):**
  - Produced `build\smg2_content\game.pak` (**2,015,241,054 bytes**, 5,539 pipelines; 3,084 unshipped SMG2 shader programs logged).
- **SMG2 Module Build & Smoke Test (46 Garigari TUs):**
  - `.\native\build_smg2.ps1 -Garigari C:\Users\kmric\projects\Garigari -Build build\smg2_game -NoBaseline` -> built `smg_game.dll` and `smg2_smoke_test.exe` with 0 errors.
  - `.\build\smg2_game\smg2_smoke_test.exe .\build\smg2_game\smg_game.dll .\build\smg2_content\game.pak` -> exit 0:
    - Attached API v164, mounted `/ObjectData/SystemDataTable` (`index=1438, children=7`) via `GameSystemDataHolder::init()`
    - Loaded real SMG2 tables: `GalaxyDataTable=49 rows`, `GalaxyWorldOrderList=62 rows`, `MarioFaceShipEventDataTable=30 rows` (`IslandFleetGalaxy`, `YosshiHomeGalaxy`, `DigMineGalaxy`, `MokumokuValleyGalaxy`, `RedBlueExGalaxy`)
    - `ScenarioDataParser` loaded **62 scenario archives** (`62` valid-world galaxies, **242 total PowerStars**; `IslandFleetGalaxy`: world=1, stars=6, zones=11; `YosshiHomeGalaxy`: world=1, stars=6, zones=9)
    - `PlanetGravityManager` at `(0,1000,0)`: `active=1, vec=(0.000, -1.000, 0.000), priority=0`
    - `RailGraph` `distance(0->1)=500.0`, `ActorStateKeeper dead=1`
- **Real Nebula Headless Host Execution (`nebula_native_host.exe`):**
  - Built standalone host via VS 2022 BuildTools + Ninja (`native/smg2/host/CMakeLists.txt` -> `build/smg2_host/nebula_native_host.exe`).
  - `.\build\smg2_host\nebula_native_host.exe --pak .\build\smg2_content\game.pak --saves .\build\smg2_saves --module .\build\smg2_game\smg_game.dll --seconds 1` -> exit 0 (`boot: disc opened`, `ScenarioDataParser loaded 62 scenario archives ... 242 total PowerStars`, `host: frame 500`, `host: time limit reached`, `host: the game ended`).
- **Host/Runtime Unit Tests (`tests/unit` via VS 2022 BuildTools + Ninja):**
  - Built and ran `ctest --test-dir build\unit -R "save_transfer|mods_tests|discord_presence|asset_store|settings_general_page|settings_system_page|mod_consent_tests|install_migration_tests|launcher_model_tests|settings_model_tests|settings_advanced_page_tests"` -> **11 / 11 passed (100%)**.
