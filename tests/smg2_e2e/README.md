# SMG2 (`SB4E01`) Garigari + Nebula Opaque-Box E2E Suite

Pure Python 3.11+ standard-library opaque-box E2E test harness for verifying the `SB4E01` Super Mario Galaxy 2 Garigari reconstruction and Nebula Windows x86-64 port.

## Directory Layout

- `e2e_lib.py` — Shared test infrastructure:
  - `@e2e_test` registry decorator with strict `DuplicateTestIdError` and metadata validation (`tier` in `1..4`, `features` subset of `1..15`).
  - `TestStatus` (`"PASS"`, `"FAIL"`) and `TestResult` dataclass (strict **no silent SKIP-as-PASS** policy).
  - `E2EContext` with auto-cleaned `temp_dir()`, `isolated_host_dirs()`, `run_cmd()`, `run_smoke_test()`, and `run_host()`.
  - Pure-Python binary parsers: `NBStoreReader` (`game.pak`), `NBProgReader` (`NBPROG04` `programs_dxil.bin`), and `NebulaSaveReader` (`NBSV` v4 `Nebula.sav`).
  - Probe & log helpers: `sha1_file`, `git_probe`, `dtk_probe`, `find_tool`, `toolchain_probe`, `shader_store_probe`, `parse_host_logs`, `extract_host_counters`, `find_todo_markers`, `scan_crash_reports`.
- `run_e2e.py` — Unified CLI runner auto-discovering `tier*.py` modules in `tests/smg2_e2e/`.
- `tier1_core.py`, `tier2_core.py` — Tier 1 & Tier 2 suites for Features `#1–#7` (added in E2E-M2).
- `tier1_gameplay.py`, `tier2_gameplay.py` — Tier 1 & Tier 2 suites for Features `#8–#15` (added in E2E-M3).
- `tier3_pairwise.py`, `tier4_scenarios.py` — Tier 3 pairwise & Tier 4 scenario suites (added in E2E-M4).

## CLI Usage

Run with `python -X utf8`:

```powershell
# Harness self-check (syntax, duplicate-ID guard, parsers, CLI/JSON, baseline probes)
python -X utf8 tests\smg2_e2e\run_e2e.py --self-check --json <temp_report.json>

# List discovered tests (supports --tier, --feature, --test, --json)
python -X utf8 tests\smg2_e2e\run_e2e.py --list

# Verify coverage thresholds (>=75 T1, >=75 T2, >=15 T3, >=8 T4, >=173 total)
python -X utf8 tests\smg2_e2e\run_e2e.py --verify-thresholds

# Target Execution Mode (exits 0 iff >=1 test matches and 100% PASS)
python -X utf8 tests\smg2_e2e\run_e2e.py --tier all
python -X utf8 tests\smg2_e2e\run_e2e.py --tier 1 --feature 5 --test T1-F05
```

Optional path & timeout overrides:
- `--timeout <seconds>` (default `30`)
- `--artifacts-root <path>` (default repo root `.`)
- `--garigari-dir <path>` (default `native\garigari`)
- `--upstream-garigari <path>` (optional path to upstream `Garigari` repo for clean-tree check)
- `--iso <path>` (optional path to a legal `SB4E01` `.iso` / `.wbfs` disc image)

## Isolation & Repository Rules

1. **Read-Only Trees:** Tests must NEVER modify `native/garigari/`, `native/`, `runtime/`, or `%LOCALAPPDATA%\Nebula`.
2. **Host Process Isolation:** Use `ctx.run_host(...)` or `ctx.isolated_host_dirs(...)` inside `ctx.temp_dir()`. `run_host()` automatically injects unique absolute `--saves` and `--crash-dumps` directories (plus `--settings` and `--data` for `Nebula.exe` to satisfy Windows `SW_HIDE` and single-instance mutex rules) and scans for `nebula-*.txt` / `*.dmp`.
3. **Zero Repo Artifacts:** Never leave `.pak`, `.sav`, logs, crash dumps, or `__pycache__` inside `tests/smg2_e2e/` (`run_e2e.py` sets `sys.dont_write_bytecode = True`).
4. **No Silent Skip:** In normal `--tier` mode, missing binaries or unimplemented subsystems must return `FAIL` via `ctx.fail_result(...)` or `AssertionError` / `E2ETestFailure`.

## Authoring a `tier*.py` Test

```python
from e2e_lib import E2EContext, TestResult, e2e_test


@e2e_test(
    test_id="T1-F01-01",
    tier=1,
    features=[1],
    description="Upstream Garigari remains clean at commit b0a4c566",
    requires_built=(),
)
def test_upstream_garigari_clean(ctx: E2EContext) -> TestResult:
    ...
    return ctx.pass_result("Upstream Garigari clean at b0a4c566")
```
