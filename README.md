# Supernova

A native-first decompiled PC port of **Super Mario Galaxy 2** for Windows (Direct3D 12), built by combining [keepvaibin/nebula](https://github.com/keepvaibin/nebula)'s host runtime, renderer, and asset cooker with a native-first fork of [SMGCommunity/Garigari](https://github.com/SMGCommunity/Garigari).

> **Legal Notice:** This repository contains **no copyrighted game assets, disc images, executables (`main.dol`), or disassembled Wii binaries**. You must provide your own legally dumped copy of *Super Mario Galaxy 2* to cook local game files (`game.pak`).

---

## Supported Game Version

| Title | Game ID | Region | Revision | Notes |
| --- | --- | --- | --- | --- |
| Super Mario Galaxy 2 | `SB4E01` | USA (`En,Fr,Es`) | Rev 0 | Expected `main.dol` SHA-1: `7d79810889824edd01c4ff02624713060a69818c` |

---

## Current Project Status

Supernova uses a **Hybrid Native-First** approach: reconstructing missing SMG2 subsystems in `native/garigari/` against `SB4E01` reference layouts (and porting shared engine architecture from Nebula's SMG1 `petari` layer) directly for x86-64 Windows without PowerPC emulation or 2009 CodeWarrior register matching.

**Working Today (Early Bring-Up / Vertical Slice):**
- **Disc Importer (`nebula-import`)**: Recognizes `SB4E01`, extracts/validates assets, handles SMG2 `LocalizeData/` languages (`UsEnglish`, `UsFrench`, `UsSpanish`), and packs `game.pak` with cooked `NLYT`/`NLAN` UI layouts and textures.
- **Shader & D3D12 Pipeline**: Merged SMG2 shader programs alongside SMG1 (3,778 shader groups / 14,724 compiled DXIL stages) powering Nebula's native Direct3D 12 host (`Nebula.exe`).
- **Core Boot, Data & Save Spine**: `SystemDataTable.arc`, 62 scenario archives (242 Power Stars), `PlanetGravityManager`, `RailGraph`, `ActorStateKeeper`, `GameSystem*`, `GameDataHolder`, and `SaveDataHandler` (`NBSV` save format).
- **Early Scene, UI, Player & Audio Bring-Up**: `LogoScene` -> `GameScene` Title/File Select cooked artwork rendering, `CameraDirector`, `MarioActor`, and `AudSystemWrapper` audio frame loop hooks.

**Work in Progress:**
- *Super Mario Galaxy 2* has over 50,000 functions across 49 galaxies, Yoshi/power-up mechanics, Starship Mario / World Map events, enemies, bosses, and cutscenes. Systems outside the current bring-up remain stubbed with `TODO(Garigari)` diagnostic markers for incremental reconstruction.

---

## Prerequisites (Windows x64)

- **Visual Studio 2022** (or VS 2022 Build Tools) with **Desktop development with C++** and **CMake**
- **LLVM / Clang** (`clang++` / `lld-link` on `PATH`)
- **Ninja** build system
- **Rust & Cargo** (for `nebula-import`)
- **Windows 10/11 SDK** (including `dxc.exe` for DXIL shader compilation)
- **Python 3.11+**

---

## Building from Source

Clone the repository:

```powershell
git clone https://github.com/Dxrmy/Supernova.git
cd Supernova
```

### 1. Build the Asset Importer (`nebula-import`)

```powershell
cargo build --release -p nebula-import
```

### 2. Build the SMG2 Game Module (`smg_game.dll`)

```powershell
.\native\build_smg2.ps1 -Build build\smg2_game -NoBaseline
```

### 3. Build the Native Windows Host (`Nebula.exe`)

From an **x64 Native Tools Command Prompt for VS 2022** (or after calling `vcvars64.bat`):

```powershell
cmake -S . -B build\smg2_win32 -G Ninja -DCMAKE_BUILD_TYPE=Release -DNEBULA_NASM= -DNEBULA_FSR3=OFF -DNEBULA_AV1=OFF
cmake --build build\smg2_win32 --target Nebula
```

*(Optional headless verification host & E2E checks:)*

```powershell
cmake -S native\smg2\host -B build\smg2_host -G Ninja -DCMAKE_BUILD_TYPE=Release -DNEBULA_NASM=
cmake --build build\smg2_host --target nebula_native_host
python tests\smg2_e2e\run_e2e.py --self-check
```

---

## Importing Your Copy & Running

1. **Cook your legal `SB4E01` disc image (`.iso` / `.wbfs`) into `game.pak`:**

```powershell
.\target\release\nebula-import.exe import <path-to-your-SB4E01.iso> --output build\smg2_content
```

2. **Launch Supernova (`Nebula.exe`):**

```powershell
.\build\smg2_win32\Nebula.exe --pak build\smg2_content\game.pak --module .\build\smg2_game\smg_game.dll --saves build\smg2_saves
```

---

## Credits & Licenses

- **Nebula** ([keepvaibin/nebula](https://github.com/keepvaibin/nebula)): Native Windows/D3D12 host runtime, shader pipeline, and `nebula-import` asset cooker.
- **Garigari** ([SMGCommunity/Garigari](https://github.com/SMGCommunity/Garigari)): Super Mario Galaxy 2 decompilation reference & native-first fork under `native/garigari/`.
- See the repository license files (`LICENSE`, `native/garigari/LICENSE`) for full terms.
