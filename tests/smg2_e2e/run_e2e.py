"""Opaque-box E2E test runner for SMG2 (SB4E01) Garigari + Nebula."""
from __future__ import annotations

import argparse
import ast
from datetime import datetime, timezone
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import time
import traceback
from typing import Any
import zlib

# Keep tests/smg2_e2e free of __pycache__ directories.
sys.dont_write_bytecode = True

SUITE_DIR = Path(__file__).resolve().parent
if str(SUITE_DIR) not in sys.path:
    sys.path.insert(0, str(SUITE_DIR))

import e2e_lib  # noqa: E402
from e2e_lib import (  # noqa: E402
    DuplicateTestIdError,
    E2EContext,
    E2ETestFailure,
    NBProgReader,
    NBStoreReader,
    NebulaSaveReader,
    SB4E01_GAME_ID,
    TestResult,
    TestSpec,
    TestStatus,
    UPSTREAM_GARIGARI_COMMIT_PREFIX,
    e2e_test,
    extract_host_counters,
    git_probe,
    read_store_version,
    shader_store_probe,
    tail_text,
    toolchain_probe,
)

_REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_ARTIFACTS_ROOT = str(_REPO_ROOT)
DEFAULT_GARIGARI_DIR = str(_REPO_ROOT / "native" / "garigari")
DEFAULT_UPSTREAM_GARIGARI = os.environ.get("UPSTREAM_GARIGARI_DIR", "")
DEFAULT_ISO_PATH = os.environ.get("SMG2_ISO_PATH", "")

TIER1_MIN_PER_FEATURE = 5
TIER2_MIN_PER_FEATURE = 5
TIER3_MIN_TOTAL = 15
TIER4_MIN_TOTAL = 8
TOTAL_MIN = 173


def discover_tier_modules(suite_dir: Path = SUITE_DIR) -> list[Path]:
    """Return sorted `tier*.py` paths inside the suite directory."""
    return sorted(
        p for p in suite_dir.glob("tier*.py") if p.is_file() and p.name != "__init__.py"
    )


def load_tier_modules(suite_dir: Path = SUITE_DIR) -> tuple[list[Path], list[TestSpec]]:
    """Import all discovered `tier*.py` modules and return registered TestSpecs."""
    e2e_lib.clear_registry()
    modules = discover_tier_modules(suite_dir)
    for mod_path in modules:
        mod_name = f"smg2_e2e_{mod_path.stem}"
        if mod_name in sys.modules:
            del sys.modules[mod_name]
        spec = importlib.util.spec_from_file_location(mod_name, mod_path)
        if spec is None or spec.loader is None:
            raise ImportError(f"Could not create import spec for {mod_path}")
        module = importlib.util.module_from_spec(spec)
        sys.modules[mod_name] = module
        spec.loader.exec_module(module)
    return modules, e2e_lib.get_registered_tests()


def filter_specs(
    specs: list[TestSpec],
    *,
    tier: str | None = None,
    features: list[int] | None = None,
    test_pattern: str | None = None,
) -> list[TestSpec]:
    """Filter TestSpecs by tier, feature IDs, and case-insensitive substring/ID match."""
    out = list(specs)
    if tier is not None and tier != "all":
        tier_num = int(tier)
        out = [s for s in out if s.tier == tier_num]
    if features:
        feat_set = set(features)
        out = [s for s in out if any(f in feat_set for f in s.features)]
    if test_pattern:
        pat = test_pattern.lower()
        out = [
            s
            for s in out
            if pat in s.test_id.lower() or pat in s.description.lower()
        ]
    return out


def evaluate_thresholds(specs: list[TestSpec]) -> dict[str, Any]:
    """Evaluate suite coverage thresholds against registered TestSpecs."""
    t1_by_feature = {f: 0 for f in range(1, 16)}
    t2_by_feature = {f: 0 for f in range(1, 16)}
    t1_total = 0
    t2_total = 0
    t3_total = 0
    t4_total = 0

    for spec in specs:
        if spec.tier == 1:
            t1_total += 1
            for f in spec.features:
                if f in t1_by_feature:
                    t1_by_feature[f] += 1
        elif spec.tier == 2:
            t2_total += 1
            for f in spec.features:
                if f in t2_by_feature:
                    t2_by_feature[f] += 1
        elif spec.tier == 3:
            t3_total += 1
        elif spec.tier == 4:
            t4_total += 1

    unmet: list[str] = []
    for f in range(1, 16):
        if t1_by_feature[f] < TIER1_MIN_PER_FEATURE:
            unmet.append(f"Tier 1 feature #{f}: {t1_by_feature[f]} < {TIER1_MIN_PER_FEATURE}")
        if t2_by_feature[f] < TIER2_MIN_PER_FEATURE:
            unmet.append(f"Tier 2 feature #{f}: {t2_by_feature[f]} < {TIER2_MIN_PER_FEATURE}")
    if t1_total < 15 * TIER1_MIN_PER_FEATURE:
        unmet.append(f"Tier 1 total: {t1_total} < {15 * TIER1_MIN_PER_FEATURE}")
    if t2_total < 15 * TIER2_MIN_PER_FEATURE:
        unmet.append(f"Tier 2 total: {t2_total} < {15 * TIER2_MIN_PER_FEATURE}")
    if t3_total < TIER3_MIN_TOTAL:
        unmet.append(f"Tier 3 total: {t3_total} < {TIER3_MIN_TOTAL}")
    if t4_total < TIER4_MIN_TOTAL:
        unmet.append(f"Tier 4 total: {t4_total} < {TIER4_MIN_TOTAL}")
    if len(specs) < TOTAL_MIN:
        unmet.append(f"Total tests: {len(specs)} < {TOTAL_MIN}")

    return {
        "passed": len(unmet) == 0,
        "total_tests": len(specs),
        "tier1_total": t1_total,
        "tier2_total": t2_total,
        "tier3_total": t3_total,
        "tier4_total": t4_total,
        "tier1_by_feature": t1_by_feature,
        "tier2_by_feature": t2_by_feature,
        "requirements": {
            "tier1_per_feature": TIER1_MIN_PER_FEATURE,
            "tier2_per_feature": TIER2_MIN_PER_FEATURE,
            "tier1_total": 15 * TIER1_MIN_PER_FEATURE,
            "tier2_total": 15 * TIER2_MIN_PER_FEATURE,
            "tier3_total": TIER3_MIN_TOTAL,
            "tier4_total": TIER4_MIN_TOTAL,
            "total": TOTAL_MIN,
        },
        "unmet": unmet,
    }


def execute_spec(spec: TestSpec, base_ctx: E2EContext) -> tuple[TestResult, bool]:
    """Run a single TestSpec, enforcing strict PASS/FAIL semantics and catching errors.

    Returns (TestResult, had_unhandled_exception).
    """
    ctx = E2EContext(
        artifacts_root=base_ctx.artifacts_root,
        garigari_dir=base_ctx.garigari_dir,
        upstream_garigari=base_ctx.upstream_garigari,
        iso_path=base_ctx.iso_path,
        timeout_s=base_ctx.timeout_s,
        self_check=base_ctx.self_check,
        active_spec=spec,
    )
    start = time.monotonic()
    try:
        raw = spec.fn(ctx)
        elapsed = time.monotonic() - start
        if isinstance(raw, TestResult):
            result = TestResult(
                test_id=spec.test_id,
                tier=spec.tier,
                features=list(spec.features),
                description=spec.description,
                status=raw.status,
                summary=raw.summary,
                diagnostics=raw.diagnostics,
                duration_s=raw.duration_s if raw.duration_s > 0.0 else elapsed,
                command=raw.command,
                returncode=raw.returncode,
                stdout_tail=raw.stdout_tail,
                stderr_tail=raw.stderr_tail,
                crash_reports=raw.crash_reports,
                todo_markers=raw.todo_markers,
                details=raw.details,
            )
            return result, False
        if isinstance(raw, bool):
            status = TestStatus.PASS.value if raw else TestStatus.FAIL.value
            return (
                TestResult(
                    test_id=spec.test_id,
                    tier=spec.tier,
                    features=list(spec.features),
                    description=spec.description,
                    status=status,
                    summary="Test returned boolean True" if raw else "Test returned boolean False",
                    duration_s=elapsed,
                ),
                False,
            )
        return (
            TestResult(
                test_id=spec.test_id,
                tier=spec.tier,
                features=list(spec.features),
                description=spec.description,
                status=TestStatus.FAIL.value,
                summary=f"Invalid return type from test: {type(raw).__name__}",
                diagnostics="Test functions must return TestResult (via ctx.pass_result/fail_result) or raise AssertionError.",
                duration_s=elapsed,
            ),
            True,
        )
    except (E2ETestFailure, AssertionError) as exc:
        elapsed = time.monotonic() - start
        return (
            TestResult(
                test_id=spec.test_id,
                tier=spec.tier,
                features=list(spec.features),
                description=spec.description,
                status=TestStatus.FAIL.value,
                summary=str(exc) or "Assertion failed",
                diagnostics=str(exc),
                duration_s=elapsed,
            ),
            False,
        )
    except Exception as exc:
        elapsed = time.monotonic() - start
        return (
            TestResult(
                test_id=spec.test_id,
                tier=spec.tier,
                features=list(spec.features),
                description=spec.description,
                status=TestStatus.FAIL.value,
                summary=f"Unhandled exception: {type(exc).__name__}: {exc}",
                diagnostics=tail_text(traceback.format_exc(), 4000),
                duration_s=elapsed,
            ),
            True,
        )


def _check_result(
    check_id: str,
    passed: bool,
    summary: str,
    diagnostics: str = "",
    details: dict[str, Any] | None = None,
) -> dict[str, Any]:
    return {
        "check_id": check_id,
        "status": TestStatus.PASS.value if passed else TestStatus.FAIL.value,
        "summary": summary,
        "diagnostics": diagnostics,
        "details": details or {},
    }


def _build_synthetic_nbsv() -> bytes:
    slots = list(NebulaSaveReader.EXPECTED_SLOTS)
    rows = []
    payloads = []
    for idx, name in enumerate(slots):
        blob = f"slot:{name}:{idx}".encode("ascii")
        rows.append(struct.pack("<16sI", name.encode("ascii"), len(blob)))
        payloads.append(blob)
    body = b"".join(rows) + b"".join(payloads)
    crc = zlib.crc32(body) & 0xFFFFFFFF
    header = struct.pack("<4sIII", b"NBSV", 4, crc, len(slots))
    return header + body


def _build_synthetic_nbprog() -> bytes:
    interface = "0123456789abcdef0123456789abcdef01234567".encode("ascii")
    identity = b"\x01\x02\x03\x04\x05\x06\x07\x08"
    header = b"NBPROG04" + struct.pack("<II", 1, 1) + interface + identity
    vrow = struct.pack("<QII" + "II" * 10, 0x1111, 1, 2, *([0] * 20))
    prow = struct.pack("<QII" + "II" * 10, 0x2222, 0, 0, *([0] * 20))
    return header + vrow + prow


def run_self_checks(
    ctx: E2EContext,
    tier_modules: list[Path],
    specs: list[TestSpec],
) -> tuple[bool, list[dict[str, Any]], dict[str, Any]]:
    """Run built-in harness self-checks and baseline artifact probes."""
    checks: list[dict[str, Any]] = []

    # 1. Syntax / AST validation of all .py files in tests/smg2_e2e/
    py_files = sorted(SUITE_DIR.glob("*.py"))
    ast_errors: list[str] = []
    for py_path in py_files:
        try:
            ast.parse(py_path.read_text(encoding="utf-8"), filename=str(py_path))
        except SyntaxError as exc:
            ast_errors.append(f"{py_path.name}:{exc.lineno}: {exc.msg}")
    checks.append(
        _check_result(
            "SC-AST",
            len(ast_errors) == 0,
            f"Parsed {len(py_files)} Python file(s) with 0 syntax errors"
            if not ast_errors
            else f"Syntax errors in {len(ast_errors)} file(s)",
            "; ".join(ast_errors),
            {"files": [p.name for p in py_files]},
        )
    )

    # 2. Registry validation, DuplicateTestIdError enforcement, and temp_dir auto-cleanup
    isolated_reg: dict[str, TestSpec] = {}
    dup_rejected = False
    meta_rejected = False
    try:
        @e2e_test("SC-SYN-01", 1, [1], "Synthetic test A", registry=isolated_reg)
        def _syn_a(_ctx: E2EContext) -> TestResult:
            return _ctx.pass_result("ok")

        try:
            @e2e_test("SC-SYN-01", 1, [1], "Duplicate ID", registry=isolated_reg)
            def _syn_dup(_ctx: E2EContext) -> TestResult:
                return _ctx.pass_result("ok")
        except DuplicateTestIdError:
            dup_rejected = True

        try:
            @e2e_test("SC-SYN-BAD", 9, [1], "Bad tier", registry=isolated_reg)
            def _syn_bad_tier(_ctx: E2EContext) -> TestResult:
                return _ctx.pass_result("ok")
        except ValueError:
            meta_rejected = True
    except Exception as exc:
        dup_rejected = False
        meta_rejected = False
        reg_diag = str(exc)
    else:
        reg_diag = ""

    created_tmp_path: Path | None = None
    with ctx.temp_dir(prefix="smg2_sc_") as td:
        created_tmp_path = td
        (td / "marker.txt").write_text("ok", encoding="utf-8")
        iso_dirs = ctx.isolated_host_dirs(td)
        iso_ok = all(p.is_absolute() and p.is_dir() for p in iso_dirs.values())
    tmp_cleaned = created_tmp_path is not None and not created_tmp_path.exists()

    checks.append(
        _check_result(
            "SC-REGISTRY-ISOLATION",
            dup_rejected and meta_rejected and iso_ok and tmp_cleaned,
            "Duplicate test_id rejection, metadata validation, host isolation, and temp_dir cleanup verified",
            reg_diag or f"dup_rejected={dup_rejected}, meta_rejected={meta_rejected}, iso_ok={iso_ok}, tmp_cleaned={tmp_cleaned}",
        )
    )

    # 3. Synthetic NBProgReader & NebulaSaveReader parser self-checks
    try:
        prog_reader = NBProgReader(_build_synthetic_nbprog())
        save_bytes = _build_synthetic_nbsv()
        save_reader = NebulaSaveReader(save_bytes)
        bad_save = bytearray(save_bytes)
        bad_save[-1] ^= 0xFF
        bad_save_reader = NebulaSaveReader(bytes(bad_save))
        parsers_ok = (
            prog_reader.header.vertex_count == 1
            and prog_reader.header.pixel_count == 1
            and prog_reader.find(0x1111, pixel=False) is not None
            and prog_reader.find(0x2222, pixel=True) is not None
            and save_reader.crc_valid is True
            and len(save_reader.slots) == 18
            and bad_save_reader.crc_valid is False
        )
        parser_diag = ""
    except Exception as exc:
        parsers_ok = False
        parser_diag = str(exc)
    checks.append(
        _check_result(
            "SC-BINARY-PARSERS",
            parsers_ok,
            "NBProgReader and NebulaSaveReader (including CRC failure detection) verified",
            parser_diag,
        )
    )

    # 4. CLI / JSON output / no-skip-as-pass invariants
    cli_ok = False
    cli_diag = ""
    with ctx.temp_dir(prefix="smg2_cli_sc_") as cli_tmp:
        json_out = cli_tmp / "list_report.json"
        cmd = [
            sys.executable,
            "-B",
            "-X",
            "utf8",
            str(SUITE_DIR / "run_e2e.py"),
            "--list",
            "--json",
            str(json_out),
        ]
        list_res = ctx.run_cmd(cmd, timeout=15.0)
        missing_smoke = ctx.run_smoke_test(
            exe_path=cli_tmp / "missing_smoke_test.exe",
            dll_path=cli_tmp / "missing.dll",
            pak_path=cli_tmp / "missing.pak",
        )
        missing_host = ctx.run_host(
            cli_tmp / "missing_host.exe",
            isolated_base=cli_tmp,
        )
        if list_res.ok and json_out.is_file():
            payload = json.loads(json_out.read_text(encoding="utf-8"))
            schema_ok = all(
                k in payload for k in ("mode", "timestamp_utc", "summary", "thresholds")
            )
            no_skip_fail_ok = (not missing_smoke.ok) and (not missing_host.ok)
            cli_ok = schema_ok and no_skip_fail_ok
            if not cli_ok:
                cli_diag = f"schema_ok={schema_ok}, no_skip_fail_ok={no_skip_fail_ok}"
        else:
            cli_diag = f"list rc={list_res.returncode}, stderr={list_res.stderr}"
    checks.append(
        _check_result(
            "SC-CLI-JSON-NOSKIP",
            cli_ok,
            "CLI --list --json schema and missing-binary FAIL enforcement verified",
            cli_diag,
        )
    )

    # 5. Live baseline probe: Upstream Garigari clean at b0a4c566 (or standalone native/garigari check)
    if not str(ctx.upstream_garigari) or str(ctx.upstream_garigari) == ".":
        g_ok = (ctx.garigari_dir / "configure.py").is_file()
        checks.append(
            _check_result(
                "SC-BASE-UPSTREAM-GARIGARI",
                g_ok,
                f"Standalone monorepo Garigari tree present at {ctx.garigari_dir}",
                "" if g_ok else "native/garigari/configure.py missing",
                {"garigari_dir": str(ctx.garigari_dir), "standalone": True},
            )
        )
    else:
        g_probe = git_probe(ctx.upstream_garigari)
        upstream_ok = (
            g_probe["is_git"]
            and g_probe["clean"]
            and g_probe["head"].startswith(UPSTREAM_GARIGARI_COMMIT_PREFIX)
        )
        checks.append(
            _check_result(
                "SC-BASE-UPSTREAM-GARIGARI",
                upstream_ok,
                f"Upstream Garigari clean at {g_probe['short_head']}"
                if upstream_ok
                else f"Upstream Garigari check failed: {g_probe}",
                "" if upstream_ok else json.dumps(g_probe),
                g_probe,
            )
        )

    # 6. Live baseline probe: game.pak NBSTORE inspection
    pak_ok = False
    pak_diag = ""
    pak_details: dict[str, Any] = {"pak_path": str(ctx.pak_path)}
    try:
        expected_ver = read_store_version(ctx.artifacts_root)
        with NBStoreReader(ctx.pak_path, expected_version=expected_ver) as pak:
            pipes = pak.find("NebulaData/Pipelines.bin")
            sys_tbl = pak.find("ObjectData/SystemDataTable")
            pipes_magic = pak.read_bytes("NebulaData/Pipelines.bin", max_bytes=8) if pipes else b""
            pak_details.update(
                {
                    "game_id": pak.header.game_id,
                    "version": pak.header.version,
                    "node_count": pak.header.node_count,
                    "pipelines_size": pipes.size if pipes else None,
                    "pipelines_magic": pipes_magic.decode("ascii", errors="replace"),
                    "has_system_data_table": sys_tbl is not None,
                }
            )
            pak_ok = (
                pak.header.game_id == SB4E01_GAME_ID
                and pak.header.version == expected_ver
                and pak.header.node_count > 0
                and pipes is not None
                and pipes_magic == b"NBPIPEL2"
                and sys_tbl is not None
            )
            if not pak_ok:
                pak_diag = f"Unexpected game.pak contents: {pak_details}"
    except Exception as exc:
        pak_diag = str(exc)
    checks.append(
        _check_result(
            "SC-BASE-GAME-PAK",
            pak_ok,
            f"game.pak NBSTORE v{pak_details.get('version')} ({pak_details.get('node_count')} nodes, NBPIPEL2) verified"
            if pak_ok
            else f"game.pak verification failed: {pak_diag}",
            pak_diag,
            pak_details,
        )
    )

    # 7. Live baseline probe: smg2_smoke_test.exe
    smoke_res = ctx.run_smoke_test(timeout=ctx.timeout_s)
    smoke_ok = smoke_res.ok and "smg2_smoke_test completed cleanly" in smoke_res.stdout
    checks.append(
        _check_result(
            "SC-BASE-SMOKE-TEST",
            smoke_ok,
            "smg2_smoke_test.exe exited 0 cleanly"
            if smoke_ok
            else f"smg2_smoke_test.exe failed (rc={smoke_res.returncode})",
            "" if smoke_ok else tail_text(smoke_res.combined_output, 1500),
            {"returncode": smoke_res.returncode, "duration_s": round(smoke_res.duration_s, 3)},
        )
    )

    # 8. Live baseline probe: nebula_native_host.exe (--seconds 1, isolated saves & crash-dumps)
    host_res = ctx.run_host(
        ctx.native_host_exe,
        extra_args=["--seconds", "1"],
        timeout=max(15.0, ctx.timeout_s),
    )
    counters = extract_host_counters(host_res.combined_output)
    host_ok = (
        host_res.ok
        and counters["has_time_limit_reached"]
        and len(host_res.crash_reports) == 0
    )
    checks.append(
        _check_result(
            "SC-BASE-NATIVE-HOST",
            host_ok,
            f"nebula_native_host.exe exited 0 cleanly ({counters['frame']} frames, 0 crash reports)"
            if host_ok
            else f"nebula_native_host.exe failed (rc={host_res.returncode}, crashes={len(host_res.crash_reports)})",
            "" if host_ok else tail_text(host_res.combined_output, 1500),
            {
                "returncode": host_res.returncode,
                "duration_s": round(host_res.duration_s, 3),
                "counters": counters,
                "crash_reports": host_res.crash_reports,
                "todo_markers_count": len(host_res.todo_markers),
            },
        )
    )

    # 9. Live baseline probe: Toolchain & Shader Store consistency
    tc = toolchain_probe(ctx.garigari_dir)
    ss = shader_store_probe(ctx.artifacts_root)
    req_tools = ("cargo", "clang++", "cmake", "ninja", "dxc")
    missing_tools = [t for t in req_tools if not tc[t]["found"]]
    tc_ss_ok = (
        len(missing_tools) == 0
        and ss["version_match"]
        and ss["digest_match"]
        and len(ss["missing_group_hlsl"]) == 0
    )
    checks.append(
        _check_result(
            "SC-BASE-TOOLCHAIN-SHADERS",
            tc_ss_ok,
            f"Toolchain ({', '.join(req_tools)}) & shader store (v{ss['store_version']}, {ss['hlsl_file_count']} .hlsl) verified"
            if tc_ss_ok
            else f"Toolchain/shader store issue: missing={missing_tools}, version_match={ss['version_match']}, digest_match={ss['digest_match']}",
            "" if tc_ss_ok else json.dumps({"missing_tools": missing_tools, "shader_store": ss}),
            {"toolchain": tc, "shader_store": ss},
        )
    )

    # 10. Discovered tier*.py tests (if populated in E2E-M2..M4)
    thresholds = evaluate_thresholds(specs)
    suite_results: list[TestResult] = []
    unhandled_exceptions = 0
    if specs:
        sc_ctx = E2EContext(
            artifacts_root=ctx.artifacts_root,
            garigari_dir=ctx.garigari_dir,
            upstream_garigari=ctx.upstream_garigari,
            iso_path=ctx.iso_path,
            timeout_s=ctx.timeout_s,
            self_check=True,
        )
        for spec in specs:
            res, had_exc = execute_spec(spec, sc_ctx)
            suite_results.append(res)
            if had_exc:
                unhandled_exceptions += 1
        checks.append(
            _check_result(
                "SC-DISCOVERED-TESTS",
                unhandled_exceptions == 0,
                f"Executed {len(suite_results)} discovered test(s) with 0 unhandled exceptions "
                f"(PASS={sum(1 for r in suite_results if r.status == 'PASS')}, "
                f"FAIL={sum(1 for r in suite_results if r.status == 'FAIL')})",
                "" if unhandled_exceptions == 0 else f"{unhandled_exceptions} test(s) raised unhandled exceptions",
                {
                    "executed": len(suite_results),
                    "unhandled_exceptions": unhandled_exceptions,
                    "tier_modules": [m.name for m in tier_modules],
                },
            )
        )
        has_all_tiers = (
            (SUITE_DIR / "tier3_pairwise.py").is_file()
            and (SUITE_DIR / "tier4_scenarios.py").is_file()
        )
        if has_all_tiers:
            checks.append(
                _check_result(
                    "SC-THRESHOLDS",
                    thresholds["passed"],
                    "All coverage thresholds met"
                    if thresholds["passed"]
                    else "Coverage thresholds unmet: " + "; ".join(thresholds["unmet"]),
                    "; ".join(thresholds["unmet"]),
                    thresholds,
                )
            )

    all_checks_ok = all(c["status"] == TestStatus.PASS.value for c in checks)
    return all_checks_ok, checks, {
        "thresholds": thresholds,
        "suite_results": [r.to_dict() for r in suite_results],
    }


def write_json_report(path: Path | str, payload: dict[str, Any]) -> None:
    out_path = Path(path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="SMG2 (SB4E01) Garigari + Nebula Opaque-Box E2E Test Runner",
    )
    parser.add_argument(
        "--tier",
        choices=["1", "2", "3", "4", "all"],
        default=None,
        help="Run tests for tier 1, 2, 3, 4, or all.",
    )
    parser.add_argument(
        "--feature",
        type=int,
        action="append",
        choices=list(range(1, 16)),
        help="Filter tests by feature ID (1..15). May be repeated.",
    )
    parser.add_argument(
        "--test",
        dest="test_pattern",
        default=None,
        help="Filter tests by case-insensitive test_id or description substring.",
    )
    parser.add_argument(
        "--list",
        action="store_true",
        help="List discovered tests matching filters and exit 0.",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Validate harness integrity, isolation, parsers, and baseline artifacts.",
    )
    parser.add_argument(
        "--verify-thresholds",
        action="store_true",
        help="Verify coverage thresholds (>=75 T1, >=75 T2, >=15 T3, >=8 T4, >=173 total).",
    )
    parser.add_argument(
        "--json",
        dest="json_path",
        default=None,
        help="Write structured JSON report to <path>.",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=30.0,
        help="Default per-subprocess timeout in seconds (default: 30).",
    )
    parser.add_argument(
        "--artifacts-root",
        default=DEFAULT_ARTIFACTS_ROOT,
        help=f"Nebula workspace root (default: {DEFAULT_ARTIFACTS_ROOT}).",
    )
    parser.add_argument(
        "--garigari-dir",
        default=DEFAULT_GARIGARI_DIR,
        help=f"Garigari fork root (default: {DEFAULT_GARIGARI_DIR}).",
    )
    parser.add_argument(
        "--upstream-garigari",
        default=DEFAULT_UPSTREAM_GARIGARI,
        help=f"Upstream Garigari root (default: {DEFAULT_UPSTREAM_GARIGARI}).",
    )
    parser.add_argument(
        "--iso",
        dest="iso_path",
        default=DEFAULT_ISO_PATH,
        help=f"SMG2 SB4E01 ISO path (default: {DEFAULT_ISO_PATH}).",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)

    if not any([args.list, args.self_check, args.verify_thresholds, args.tier is not None]):
        print(
            "Error: specify one of --tier {1,2,3,4,all}, --list, --self-check, or --verify-thresholds.",
            file=sys.stderr,
        )
        return 2

    try:
        tier_modules, specs = load_tier_modules()
    except DuplicateTestIdError as exc:
        print(f"FAIL: Duplicate test ID detected: {exc}", file=sys.stderr)
        return 1
    except Exception as exc:
        print(f"FAIL: Error importing test modules: {exc}", file=sys.stderr)
        traceback.print_exc(file=sys.stderr)
        return 1

    ctx = E2EContext(
        artifacts_root=Path(args.artifacts_root),
        garigari_dir=Path(args.garigari_dir),
        upstream_garigari=Path(args.upstream_garigari),
        iso_path=Path(args.iso_path),
        timeout_s=args.timeout,
        self_check=args.self_check,
    )
    thresholds = evaluate_thresholds(specs)
    timestamp_utc = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    if args.list:
        matched = filter_specs(
            specs,
            tier=args.tier,
            features=args.feature,
            test_pattern=args.test_pattern,
        )
        print(f"Discovered {len(specs)} total test(s) across {len(tier_modules)} module(s); {len(matched)} matching:")
        for s in matched:
            feats = ",".join(f"#{f}" for f in s.features)
            print(f"  [T{s.tier}] {s.test_id} ({feats}) — {s.description}")
        if args.json_path:
            write_json_report(
                args.json_path,
                {
                    "mode": "list",
                    "timestamp_utc": timestamp_utc,
                    "artifacts_root": str(ctx.artifacts_root),
                    "garigari_dir": str(ctx.garigari_dir),
                    "upstream_garigari": str(ctx.upstream_garigari),
                    "iso_path": str(ctx.iso_path),
                    "summary": {
                        "total_discovered": len(specs),
                        "matched": len(matched),
                        "modules": [m.name for m in tier_modules],
                    },
                    "thresholds": thresholds,
                    "tests": [
                        {
                            "test_id": s.test_id,
                            "tier": s.tier,
                            "features": list(s.features),
                            "description": s.description,
                            "requires_built": list(s.requires_built),
                            "module": s.module,
                        }
                        for s in matched
                    ],
                },
            )
        return 0

    if args.verify_thresholds:
        print(
            f"Threshold check across {len(specs)} test(s) "
            f"(T1={thresholds['tier1_total']}, T2={thresholds['tier2_total']}, "
            f"T3={thresholds['tier3_total']}, T4={thresholds['tier4_total']}):"
        )
        if thresholds["passed"]:
            print("PASS: All coverage thresholds met.")
        else:
            print("FAIL: Coverage thresholds unmet:")
            for item in thresholds["unmet"]:
                print(f"  - {item}")
        if args.json_path:
            write_json_report(
                args.json_path,
                {
                    "mode": "verify-thresholds",
                    "timestamp_utc": timestamp_utc,
                    "artifacts_root": str(ctx.artifacts_root),
                    "garigari_dir": str(ctx.garigari_dir),
                    "upstream_garigari": str(ctx.upstream_garigari),
                    "iso_path": str(ctx.iso_path),
                    "summary": {
                        "passed": thresholds["passed"],
                        "total_discovered": len(specs),
                    },
                    "thresholds": thresholds,
                },
            )
        return 0 if thresholds["passed"] else 1

    if args.self_check:
        ok, checks, extra = run_self_checks(ctx, tier_modules, specs)
        passed_cnt = sum(1 for c in checks if c["status"] == TestStatus.PASS.value)
        failed_cnt = len(checks) - passed_cnt
        print(f"E2E Self-Check: {passed_cnt}/{len(checks)} checks passed")
        for c in checks:
            print(f"  [{c['status']}] {c['check_id']}: {c['summary']}")
            if c["status"] != TestStatus.PASS.value and c["diagnostics"]:
                print(f"         diag: {c['diagnostics']}")
        if args.json_path:
            write_json_report(
                args.json_path,
                {
                    "mode": "self-check",
                    "timestamp_utc": timestamp_utc,
                    "artifacts_root": str(ctx.artifacts_root),
                    "garigari_dir": str(ctx.garigari_dir),
                    "upstream_garigari": str(ctx.upstream_garigari),
                    "iso_path": str(ctx.iso_path),
                    "summary": {
                        "passed": ok,
                        "checks_total": len(checks),
                        "checks_passed": passed_cnt,
                        "checks_failed": failed_cnt,
                        "discovered_tests": len(specs),
                    },
                    "thresholds": extra["thresholds"],
                    "self_check_results": checks,
                    "test_results": extra["suite_results"],
                },
            )
        return 0 if ok else 1

    # Target Execution Mode (--tier 1|2|3|4|all)
    matched = filter_specs(
        specs,
        tier=args.tier,
        features=args.feature,
        test_pattern=args.test_pattern,
    )
    if not matched:
        print(
            f"FAIL: 0 tests matched (tier={args.tier}, feature={args.feature}, test={args.test_pattern!r}). "
            "No-skip-as-pass policy requires >= 1 matching test.",
            file=sys.stderr,
        )
        if args.json_path:
            write_json_report(
                args.json_path,
                {
                    "mode": "tier",
                    "timestamp_utc": timestamp_utc,
                    "artifacts_root": str(ctx.artifacts_root),
                    "garigari_dir": str(ctx.garigari_dir),
                    "upstream_garigari": str(ctx.upstream_garigari),
                    "iso_path": str(ctx.iso_path),
                    "summary": {
                        "tier": args.tier,
                        "total": 0,
                        "passed": 0,
                        "failed": 0,
                        "exit_code": 1,
                        "reason": "0 matching tests",
                    },
                    "thresholds": thresholds,
                    "test_results": [],
                },
            )
        return 1

    results: list[TestResult] = []
    for spec in matched:
        res, _ = execute_spec(spec, ctx)
        results.append(res)
        print(f"[{res.status}] {res.test_id} ({res.duration_s:.2f}s) — {res.summary}")
        if res.status != TestStatus.PASS.value and res.diagnostics:
            print(f"       diagnostics: {res.diagnostics}")

    passed_cnt = sum(1 for r in results if r.status == TestStatus.PASS.value)
    failed_cnt = len(results) - passed_cnt
    print(f"Summary (tier={args.tier}): {passed_cnt}/{len(results)} PASS, {failed_cnt} FAIL")

    exit_code = 0 if failed_cnt == 0 else 1
    if args.json_path:
        write_json_report(
            args.json_path,
            {
                "mode": "tier",
                "timestamp_utc": timestamp_utc,
                "artifacts_root": str(ctx.artifacts_root),
                "garigari_dir": str(ctx.garigari_dir),
                "upstream_garigari": str(ctx.upstream_garigari),
                "iso_path": str(ctx.iso_path),
                "summary": {
                    "tier": args.tier,
                    "total": len(results),
                    "passed": passed_cnt,
                    "failed": failed_cnt,
                    "exit_code": exit_code,
                },
                "thresholds": thresholds,
                "test_results": [r.to_dict() for r in results],
            },
        )
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
