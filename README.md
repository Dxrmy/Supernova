# Supernova — Super Mario Galaxy 2 (`SB4E01`) Native PC Port

**Supernova** is a native Windows x86-64 PC port of *Super Mario Galaxy 2* (`SB4E01`) built on:
- **Nebula** (`keepvaibin/nebula`) — Native Direct3D 12 host, asset store (`game.pak` / `NBSTORE`), GX TEV → DXIL shader pipeline (`dxc.exe`), audio mixer, input engine, and disc importer (`nebula-import`).
- **Garigari** (`SMGCommunity/Garigari` fork under `native/garigari/`) — *Super Mario Galaxy 2* (`SB4E01`) decompilation and native C++ reconstruction.

> **Legal Requirement:** Supernova does **not** include any copyrighted Nintendo game assets, ISO/WBFS images, `main.dol`, or extracted archives. You must provide your own legally dumped *Super Mario Galaxy 2* (USA Rev 0, `SB4E01`) ISO to import assets locally.

---

## Prerequisites (Windows x64)
- **Visual Studio 2022 Build Tools** (or VS 2022 Community) with MSVC C++ x64 tools & CMake/Ninja
- **Windows 10/11 SDK** (providing `fxc.exe` and `dxc.exe`, e.g. `10.0.26100.0`)
- **LLVM / Clang** (`C:\Program Files\LLVM\bin\clang++.exe` + `lld-link`)
- **Rust toolchain** (`cargo` / `rustc`, for `crates/nebula-import`)
- **Python 3.11+**

---

## Quick Start (Fresh Clone)

### 1. Build the Disc Importer & Import Your Legal SMG2 ISO
```powershell
cargo build --release -p nebula-import
.\target\release\nebula-import.exe import "<PATH_TO_YOUR_SMG2_SB4E01.iso>" --output build\smg2_content
```
This cooks your ISO into `build\smg2_content\game.pak` (`NBSTORE` v42, 5,539 material pipelines).

### 2. Build the SMG2 Game Module (`smg_game.dll` + `smg2_smoke_test.exe`)
```powershell
.\native\build_smg2.ps1 -Build build\smg2_game -NoBaseline
```

### 3. Build the Windowed Direct3D 12 Host (`Nebula.exe`)
Run inside a Visual Studio 2022 x64 Developer Command Prompt (or after calling `vcvars64.bat`):
```powershell
cmake -S . -B build\smg2_win32 -G Ninja -DCMAKE_BUILD_TYPE=Release -DNEBULA_NASM= -DNEBULA_FSR3=OFF -DNEBULA_AV1=OFF
cmake --build build\smg2_win32 --target Nebula
```

### 4. Launch Supernova
```powershell
.\build\smg2_win32\Nebula.exe --pak .\build\smg2_content\game.pak --saves .\build\smg2_saves --module .\build\smg2_game\smg_game.dll
```

### Optional: Headless Smoke & Verification
```powershell
.\build\smg2_game\smg2_smoke_test.exe .\build\smg2_game\smg_game.dll .\build\smg2_content\game.pak
python .\tests\smg2_e2e\run_e2e.py --self-check
```

---

## Current Status & Known `TODO(Garigari)` Gaps
- **Boot & System Spine (`GameSystem*`, `GameData*`, `SaveDataHandler*`):** Working (`SystemDataTable.arc`, `ScenarioDataParser` loading all 62 SMG2 galaxies / 242 PowerStars, `NBSV` v4 save persistence).
- **Shaders & D3D12 Renderer (`Nebula.exe`):** All 5,539 SMG2 material pipelines and 3,778 compiled DXIL shader groups (`store_version` 42) integrated.
- **Scenes, Screen UI, Camera, Player, Audio:** `LogoScene` → `GameScene` (Title → File Select → `IslandFleetGalaxy` stage mount) renders cooked `TitleLogo` & `FileSelect` artwork in D3D12 with `CameraDirector`, `MarioActor` (`PlanetGravityManager`), and `AudSystemWrapper` (180 Hz sound clock). Full 3D J3D actor rendering, `nw4r::lyt` animation playback, and full Galaxy gameplay objects remain active `TODO(Garigari)` reconstruction targets.

---

## Credits & Licenses
- **Nebula:** See [LICENSE](LICENSE) (`keepvaibin/nebula`).
- **Garigari:** See [native/garigari/LICENSE](native/garigari/LICENSE) (`SMGCommunity/Garigari`).
