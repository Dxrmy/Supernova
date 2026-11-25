"""Shared library for the SMG2 (SB4E01) + Nebula opaque-box E2E test suite."""
from __future__ import annotations

import bisect
from contextlib import contextmanager
from dataclasses import asdict, dataclass, field
from enum import Enum
import functools
import hashlib
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time
from typing import Any, Callable, Iterable, Iterator
import uuid
import zlib

# Expected SB4E01 constants from ORIGINAL_REQUEST.md / Garigari config/SB4E01
SB4E01_GAME_ID = "SB4E01"
SB4E01_DOL_SHA1 = "7d79810889824edd01c4ff02624713060a69818c"
UPSTREAM_GARIGARI_COMMIT_PREFIX = "b0a4c566"
NEBULA_GAME_API_VERSION = 164
EXPECTED_SMG1_SHADER_FILES = 2401
EXPECTED_SMG2_PIPELINES = 5539

VALID_TIERS = (1, 2, 3, 4)
VALID_FEATURES = frozenset(range(1, 16))
TAIL_CHAR_LIMIT = 4000


class DuplicateTestIdError(ValueError):
    """Raised when two E2E tests register the same test_id."""


class E2ETestFailure(AssertionError):
    """Raised by a test function to signal a clean opaque-box test failure."""


class TestStatus(str, Enum):
    """Strict two-state test outcome. No silent SKIP-as-PASS is permitted."""

    PASS = "PASS"
    FAIL = "FAIL"


@dataclass(frozen=True)
class TestSpec:
    """Metadata and callable for a registered @e2e_test."""

    test_id: str
    tier: int
    features: tuple[int, ...]
    description: str
    requires_built: tuple[str, ...]
    fn: Callable[["E2EContext"], "TestResult"]
    module: str = ""


@dataclass
class TestResult:
    """Structured outcome produced by every E2E test."""

    test_id: str
    tier: int
    features: list[int]
    description: str
    status: str
    summary: str
    diagnostics: str = ""
    duration_s: float = 0.0
    command: str = ""
    returncode: int | None = None
    stdout_tail: str = ""
    stderr_tail: str = ""
    crash_reports: list[dict[str, Any]] = field(default_factory=list)
    todo_markers: list[str] = field(default_factory=list)
    details: dict[str, Any] = field(default_factory=dict)

    def __post_init__(self) -> None:
        if isinstance(self.status, TestStatus):
            self.status = self.status.value
        if self.status not in (TestStatus.PASS.value, TestStatus.FAIL.value):
            raise ValueError(f"Invalid TestResult status {self.status!r}; must be 'PASS' or 'FAIL'")
        self.features = [int(f) for f in self.features]

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)


@dataclass
class CmdResult:
    """Result from executing an external binary or tool command."""

    command: list[str]
    command_str: str
    returncode: int | None
    stdout: str
    stderr: str
    timed_out: bool
    duration_s: float
    crash_reports: list[dict[str, Any]] = field(default_factory=list)
    todo_markers: list[str] = field(default_factory=list)
    isolated_dirs: dict[str, str] = field(default_factory=dict)

    @property
    def ok(self) -> bool:
        return not self.timed_out and self.returncode == 0

    @property
    def combined_output(self) -> str:
        if self.stdout and self.stderr:
            return f"{self.stdout}\n{self.stderr}"
        return self.stdout or self.stderr


# Module-level test registry keyed by unique test_id.
_REGISTRY: dict[str, TestSpec] = {}


def _validate_test_metadata(
    test_id: str,
    tier: int,
    features: Iterable[int],
    description: str,
    requires_built: Iterable[str],
) -> tuple[str, int, tuple[int, ...], str, tuple[str, ...]]:
    if not isinstance(test_id, str) or not test_id.strip():
        raise ValueError(f"test_id must be a non-empty string, got {test_id!r}")
    if isinstance(tier, bool) or tier not in VALID_TIERS:
        raise ValueError(f"tier must be one of {VALID_TIERS}, got {tier!r} (test_id={test_id!r})")
    if isinstance(features, (str, bytes)):
        raise ValueError(f"features must be an iterable of ints 1..15, got {features!r}")
    feat_list = sorted({int(f) for f in features})
    if not feat_list or not set(feat_list).issubset(VALID_FEATURES):
        raise ValueError(
            f"features must be a non-empty subset of 1..15, got {feat_list!r} (test_id={test_id!r})"
        )
    if not isinstance(description, str) or not description.strip():
        raise ValueError(f"description must be a non-empty string (test_id={test_id!r})")
    req_tuple = tuple(str(r) for r in (requires_built or ()))
    return test_id.strip(), int(tier), tuple(feat_list), description.strip(), req_tuple


def register_test(
    spec: TestSpec,
    registry: dict[str, TestSpec] | None = None,
) -> TestSpec:
    """Register a TestSpec into the target registry, rejecting duplicate test_ids."""
    target = _REGISTRY if registry is None else registry
    if spec.test_id in target:
        existing = target[spec.test_id]
        raise DuplicateTestIdError(
            f"Duplicate test_id {spec.test_id!r} (already registered from {existing.module or '<unknown>'})"
        )
    target[spec.test_id] = spec
    return spec


def get_registered_tests(registry: dict[str, TestSpec] | None = None) -> list[TestSpec]:
    """Return registered TestSpec objects in insertion order."""
    target = _REGISTRY if registry is None else registry
    return list(target.values())


def clear_registry(registry: dict[str, TestSpec] | None = None) -> None:
    """Clear a test registry (primarily for isolated self-checks)."""
    target = _REGISTRY if registry is None else registry
    target.clear()


def e2e_test(
    test_id: str,
    tier: int,
    features: Iterable[int],
    description: str,
    requires_built: Iterable[str] = (),
    registry: dict[str, TestSpec] | None = None,
) -> Callable[[Callable[["E2EContext"], TestResult]], Callable[["E2EContext"], TestResult]]:
    """Decorator registering an opaque-box E2E test function."""
    norm_id, norm_tier, norm_features, norm_desc, norm_reqs = _validate_test_metadata(
        test_id, tier, features, description, requires_built
    )

    def decorator(fn: Callable[["E2EContext"], TestResult]) -> Callable[["E2EContext"], TestResult]:
        spec = TestSpec(
            test_id=norm_id,
            tier=norm_tier,
            features=norm_features,
            description=norm_desc,
            requires_built=norm_reqs,
            fn=fn,
            module=getattr(fn, "__module__", ""),
        )
        register_test(spec, registry=registry)

        @functools.wraps(fn)
        def wrapper(ctx: E2EContext) -> TestResult:
            return fn(ctx)

        setattr(wrapper, "__e2e_spec__", spec)
        return wrapper

    return decorator


def tail_text(text: str, limit: int = TAIL_CHAR_LIMIT) -> str:
    """Return the last `limit` characters of text."""
    if not text:
        return ""
    if len(text) <= limit:
        return text
    return text[-limit:]


def _format_cmd(cmd: Iterable[str | os.PathLike[str]]) -> str:
    return subprocess.list2cmdline([str(c) for c in cmd])


def _is_under_path(candidate: Path, parent: Path) -> bool:
    try:
        candidate.resolve().relative_to(parent.resolve())
        return True
    except ValueError:
        return False


def _localappdata_nebula_dir() -> Path | None:
    local_app = os.environ.get("LOCALAPPDATA")
    if not local_app:
        return None
    return Path(local_app) / "Nebula"


def _is_windowed_nebula_exe(exe_path: Path) -> bool:
    name = exe_path.name.lower()
    return name in ("nebula.exe", "nebulad11.exe", "nebulavk.exe")


def _has_flag(args: list[str], flag: str) -> bool:
    return any(a == flag for a in args)


@dataclass
class E2EContext:
    """Execution context passed to every E2E test."""

    artifacts_root: Path = field(default_factory=lambda: Path(__file__).resolve().parents[2])
    garigari_dir: Path = field(default_factory=lambda: Path(__file__).resolve().parents[2] / "native" / "garigari")
    upstream_garigari: Path = field(default_factory=lambda: Path(os.environ.get("UPSTREAM_GARIGARI_DIR", "")) if os.environ.get("UPSTREAM_GARIGARI_DIR") else Path())
    iso_path: Path = field(default_factory=lambda: Path(os.environ.get("SMG2_ISO_PATH", "")) if os.environ.get("SMG2_ISO_PATH") else Path())
    timeout_s: float = 30.0
    self_check: bool = False
    active_spec: TestSpec | None = field(default=None, repr=False)

    def __post_init__(self) -> None:
        self.artifacts_root = Path(self.artifacts_root)
        self.garigari_dir = Path(self.garigari_dir)
        self.upstream_garigari = Path(self.upstream_garigari)
        self.iso_path = Path(self.iso_path)
        self.timeout_s = float(self.timeout_s)

    @property
    def pak_path(self) -> Path:
        return self.artifacts_root / "build" / "smg2_content" / "game.pak"

    @property
    def smg_dll_path(self) -> Path:
        return self.artifacts_root / "build" / "smg2_game" / "smg_game.dll"

    @property
    def smoke_test_exe(self) -> Path:
        return self.artifacts_root / "build" / "smg2_game" / "smg2_smoke_test.exe"

    @property
    def native_host_exe(self) -> Path:
        return self.artifacts_root / "build" / "smg2_host" / "nebula_native_host.exe"

    @property
    def nebula_exe(self) -> Path:
        return self.artifacts_root / "build" / "Nebula.exe"

    @property
    def programs_dxil_bin(self) -> Path:
        candidates = [
            self.artifacts_root / "build" / "generated" / "programs_dxil.bin",
            self.artifacts_root / "build" / "Release" / "generated" / "programs_dxil.bin",
        ]
        for candidate in candidates:
            if candidate.is_file():
                return candidate
        return candidates[0]

    @contextmanager
    def temp_dir(self, prefix: str = "smg2_e2e_") -> Iterator[Path]:
        """Create and auto-clean a dedicated temporary directory outside tracked repos."""
        path = Path(tempfile.mkdtemp(prefix=prefix)).resolve()
        try:
            yield path
        finally:
            for attempt in range(4):
                if not path.exists():
                    break
                try:
                    shutil.rmtree(path)
                    break
                except OSError:
                    if attempt == 3:
                        shutil.rmtree(path, ignore_errors=True)
                    else:
                        time.sleep(0.05)

    def isolated_host_dirs(self, base_dir: Path | str) -> dict[str, Path]:
        """Create unique absolute saves/settings/data/crash_dumps folders outside %LOCALAPPDATA%\\Nebula."""
        base = Path(base_dir).resolve()
        run_root = (base / f"host_{uuid.uuid4().hex[:10]}").resolve()
        local_nebula = _localappdata_nebula_dir()
        if local_nebula is not None and _is_under_path(run_root, local_nebula):
            raise ValueError(f"Refusing to place isolated host dirs under {local_nebula}: {run_root}")
        dirs = {
            "root": run_root,
            "saves": (run_root / "saves").resolve(),
            "settings": (run_root / "settings").resolve(),
            "data": (run_root / "data").resolve(),
            "crash_dumps": (run_root / "crash_dumps").resolve(),
        }
        for d in dirs.values():
            d.mkdir(parents=True, exist_ok=True)
        return dirs

    def run_cmd(
        self,
        cmd: Iterable[str | os.PathLike[str]],
        cwd: Path | str | None = None,
        env: dict[str, str] | None = None,
        timeout: float | None = None,
    ) -> CmdResult:
        """Execute a subprocess capturing UTF-8 output and cleanly terminating on timeout."""
        cmd_list = [str(c) for c in cmd]
        cmd_str = _format_cmd(cmd_list)
        effective_timeout = self.timeout_s if timeout is None else float(timeout)
        proc_env = None
        if env is not None:
            proc_env = os.environ.copy()
            proc_env.update(env)

        start = time.monotonic()
        try:
            proc = subprocess.Popen(
                cmd_list,
                cwd=str(cwd) if cwd is not None else None,
                env=proc_env,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                encoding="utf-8",
                errors="replace",
            )
        except OSError as exc:
            return CmdResult(
                command=cmd_list,
                command_str=cmd_str,
                returncode=-1,
                stdout="",
                stderr=f"Failed to launch process: {exc}",
                timed_out=False,
                duration_s=time.monotonic() - start,
            )

        timed_out = False
        try:
            stdout, stderr = proc.communicate(timeout=effective_timeout)
            returncode: int | None = proc.returncode
        except subprocess.TimeoutExpired as exc:
            timed_out = True
            proc.terminate()
            try:
                stdout_rest, stderr_rest = proc.communicate(timeout=2.0)
            except subprocess.TimeoutExpired:
                proc.kill()
                stdout_rest, stderr_rest = proc.communicate(timeout=5.0)
            out_head = exc.stdout.decode("utf-8", errors="replace") if isinstance(exc.stdout, bytes) else (exc.stdout or "")
            err_head = exc.stderr.decode("utf-8", errors="replace") if isinstance(exc.stderr, bytes) else (exc.stderr or "")
            stdout = out_head + (stdout_rest or "")
            stderr = err_head + (stderr_rest or "")
            if stderr and not stderr.endswith("\n"):
                stderr += "\n"
            stderr += f"[e2e_lib] Process timed out after {effective_timeout:.1f}s and was terminated."
            returncode = None

        duration_s = time.monotonic() - start
        todos = find_todo_markers(f"{stdout}\n{stderr}")
        return CmdResult(
            command=cmd_list,
            command_str=cmd_str,
            returncode=returncode,
            stdout=stdout,
            stderr=stderr,
            timed_out=timed_out,
            duration_s=duration_s,
            todo_markers=todos,
        )

    def run_smoke_test(
        self,
        dll_path: Path | str | None = None,
        pak_path: Path | str | None = None,
        exe_path: Path | str | None = None,
        timeout: float | None = None,
    ) -> CmdResult:
        """Run smg2_smoke_test.exe <smg_game.dll> <game.pak>."""
        exe = Path(exe_path) if exe_path is not None else self.smoke_test_exe
        dll = Path(dll_path) if dll_path is not None else self.smg_dll_path
        pak = Path(pak_path) if pak_path is not None else self.pak_path
        missing = [str(p) for p in (exe, dll, pak) if not p.is_file()]
        if missing:
            return CmdResult(
                command=[str(exe), str(dll), str(pak)],
                command_str=_format_cmd([exe, dll, pak]),
                returncode=-1,
                stdout="",
                stderr="Missing required file(s) for smg2_smoke_test: " + ", ".join(missing),
                timed_out=False,
                duration_s=0.0,
            )
        return self.run_cmd([str(exe), str(dll), str(pak)], cwd=exe.parent, timeout=timeout)

    def run_host(
        self,
        exe_path: Path | str,
        *,
        pak_path: Path | str | None = None,
        saves_dir: Path | str | None = None,
        module_path: Path | str | None = None,
        extra_args: Iterable[str | os.PathLike[str]] = (),
        isolated_base: Path | str | None = None,
        timeout: float | None = None,
        pass_pak: bool = True,
        pass_saves: bool = True,
        pass_module: bool = True,
        pass_crash_dumps: bool = True,
        pass_renderer_isolation: bool = True,
        env: dict[str, str] | None = None,
        cwd: Path | str | None = None,
    ) -> CmdResult:
        """Run nebula_native_host.exe or Nebula.exe with isolated dirs and crash scanning."""
        exe = Path(exe_path)
        extra = [str(a) for a in extra_args]

        if not exe.is_file():
            return CmdResult(
                command=[str(exe), *extra],
                command_str=_format_cmd([exe, *extra]),
                returncode=-1,
                stdout="",
                stderr=f"Missing host executable: {exe}",
                timed_out=False,
                duration_s=0.0,
            )

        def _execute_in(base: Path) -> CmdResult:
            dirs = self.isolated_host_dirs(base)
            args: list[str] = [str(exe)]

            if pass_pak and not _has_flag(extra, "--pak"):
                pak = Path(pak_path) if pak_path is not None else self.pak_path
                args.extend(["--pak", str(pak)])

            if pass_saves and not _has_flag(extra, "--saves"):
                saves = Path(saves_dir).resolve() if saves_dir is not None else dirs["saves"]
                saves.mkdir(parents=True, exist_ok=True)
                args.extend(["--saves", str(saves)])
            else:
                saves = Path(saves_dir).resolve() if saves_dir is not None else dirs["saves"]

            if pass_module and not _has_flag(extra, "--module"):
                mod = Path(module_path) if module_path is not None else self.smg_dll_path
                args.extend(["--module", str(mod)])

            if pass_crash_dumps and not _has_flag(extra, "--crash-dumps"):
                args.extend(["--crash-dumps", str(dirs["crash_dumps"])])
                crash_dir = dirs["crash_dumps"]
            else:
                crash_dir = dirs["crash_dumps"]
                if "--crash-dumps" in extra:
                    idx = extra.index("--crash-dumps")
                    if idx + 1 < len(extra):
                        crash_dir = Path(extra[idx + 1])

            if pass_renderer_isolation and _is_windowed_nebula_exe(exe):
                if not _has_flag(extra, "--settings"):
                    args.extend(["--settings", str(dirs["settings"])])
                if not _has_flag(extra, "--data"):
                    args.extend(["--data", str(dirs["data"])])

            args.extend(extra)
            run_cwd = Path(cwd) if cwd is not None else exe.parent
            res = self.run_cmd(args, cwd=run_cwd, env=env, timeout=timeout)
            res.crash_reports = scan_crash_reports(crash_dir)
            res.isolated_dirs = {
                "saves": str(saves),
                "settings": str(dirs["settings"]),
                "data": str(dirs["data"]),
                "crash_dumps": str(crash_dir),
            }
            return res

        if isolated_base is not None:
            return _execute_in(Path(isolated_base))
        with self.temp_dir(prefix="smg2_host_") as tmp:
            return _execute_in(tmp)

    def _resolve_meta(
        self,
        test_id: str | None,
        tier: int | None,
        features: Iterable[int] | None,
        description: str | None,
    ) -> tuple[str, int, list[int], str]:
        spec = self.active_spec
        resolved_id = test_id if test_id is not None else (spec.test_id if spec else "")
        resolved_tier = tier if tier is not None else (spec.tier if spec else 1)
        resolved_feats = list(features) if features is not None else (list(spec.features) if spec else [])
        resolved_desc = description if description is not None else (spec.description if spec else "")
        return resolved_id, resolved_tier, resolved_feats, resolved_desc

    def pass_result(
        self,
        summary: str = "OK",
        *,
        test_id: str | None = None,
        tier: int | None = None,
        features: Iterable[int] | None = None,
        description: str | None = None,
        diagnostics: str = "",
        duration_s: float = 0.0,
        cmd_result: CmdResult | None = None,
        details: dict[str, Any] | None = None,
    ) -> TestResult:
        rid, rtier, rfeats, rdesc = self._resolve_meta(test_id, tier, features, description)
        return TestResult(
            test_id=rid,
            tier=rtier,
            features=rfeats,
            description=rdesc,
            status=TestStatus.PASS.value,
            summary=summary,
            diagnostics=diagnostics,
            duration_s=cmd_result.duration_s if (cmd_result and duration_s == 0.0) else duration_s,
            command=cmd_result.command_str if cmd_result else "",
            returncode=cmd_result.returncode if cmd_result else None,
            stdout_tail=tail_text(cmd_result.stdout) if cmd_result else "",
            stderr_tail=tail_text(cmd_result.stderr) if cmd_result else "",
            crash_reports=list(cmd_result.crash_reports) if cmd_result else [],
            todo_markers=list(cmd_result.todo_markers) if cmd_result else [],
            details=dict(details or {}),
        )

    def fail_result(
        self,
        summary: str,
        diagnostics: str = "",
        *,
        test_id: str | None = None,
        tier: int | None = None,
        features: Iterable[int] | None = None,
        description: str | None = None,
        duration_s: float = 0.0,
        cmd_result: CmdResult | None = None,
        details: dict[str, Any] | None = None,
    ) -> TestResult:
        rid, rtier, rfeats, rdesc = self._resolve_meta(test_id, tier, features, description)
        return TestResult(
            test_id=rid,
            tier=rtier,
            features=rfeats,
            description=rdesc,
            status=TestStatus.FAIL.value,
            summary=summary,
            diagnostics=diagnostics,
            duration_s=cmd_result.duration_s if (cmd_result and duration_s == 0.0) else duration_s,
            command=cmd_result.command_str if cmd_result else "",
            returncode=cmd_result.returncode if cmd_result else None,
            stdout_tail=tail_text(cmd_result.stdout) if cmd_result else "",
            stderr_tail=tail_text(cmd_result.stderr) if cmd_result else "",
            crash_reports=list(cmd_result.crash_reports) if cmd_result else [],
            todo_markers=list(cmd_result.todo_markers) if cmd_result else [],
            details=dict(details or {}),
        )


# ---------------------------------------------------------------------------
# Binary & Format Readers
# ---------------------------------------------------------------------------


def read_store_version(nebula_root: Path | str) -> int:
    """Read expected NBSTORE version integer from runtime/include/galaxy/store_version.inc."""
    inc_path = Path(nebula_root) / "runtime" / "include" / "galaxy" / "store_version.inc"
    text = inc_path.read_text(encoding="utf-8").strip()
    return int(text)


def fnv1a_64_store_path(path: str) -> tuple[str, int]:
    """Normalize a store path identically to asset_store.cpp and compute FNV-1a 64."""
    parts: list[str] = []
    for raw_part in path.replace("\\", "/").split("/"):
        if not raw_part or raw_part == ".":
            continue
        if raw_part == "..":
            if parts:
                parts.pop()
            continue
        parts.append(raw_part)
    normalized = "/".join(parts).lower()
    h = 0xCBF29CE484222325
    for byte in normalized.encode("utf-8"):
        h = ((h ^ byte) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return normalized, h


@dataclass(frozen=True)
class NBStoreHeader:
    magic: bytes
    version: int
    node_count: int
    nodes_offset: int
    hashes_offset: int
    names_offset: int
    names_size: int
    game_id: str
    disc_number: int
    revision: int
    file_size: int


@dataclass(frozen=True)
class NBStoreNode:
    index: int
    name: str
    position: int
    size: int
    name_offset: int
    parent: int
    flags: int
    first_child: int
    children: int

    @property
    def is_folder(self) -> bool:
        return bool(self.flags & 1)

    @property
    def is_archive_folder(self) -> bool:
        return bool((self.flags >> 1) & 1)

    @property
    def kind(self) -> int:
        return (self.flags >> 8) & 0xFF


class NBStoreReader:
    """Pure-Python read-only parser for Nebula NBSTORE game.pak archives."""

    MAGIC = b"NBSTORE\x00"
    HEADER_STRUCT = struct.Struct("<8sIIQQQQ6sBB8x")
    NODE_STRUCT = struct.Struct("<QQIIIIII")
    HASH_STRUCT = struct.Struct("<QII")

    def __init__(self, path: Path | str, expected_version: int | None = None) -> None:
        self.path = Path(path)
        self._fp = self.path.open("rb")
        try:
            self._file_size = self.path.stat().st_size
            if self._file_size < self.HEADER_STRUCT.size:
                raise ValueError(f"{self.path} is too small ({self._file_size} bytes) for NBSTORE header")
            raw_header = self._fp.read(self.HEADER_STRUCT.size)
            (
                magic,
                version,
                node_count,
                nodes_offset,
                hashes_offset,
                names_offset,
                names_size,
                game_id_raw,
                disc_number,
                revision,
            ) = self.HEADER_STRUCT.unpack(raw_header)
            if magic != self.MAGIC:
                raise ValueError(f"Invalid NBSTORE magic in {self.path}: {magic!r}")
            if expected_version is not None and version != expected_version:
                raise ValueError(
                    f"NBSTORE version mismatch in {self.path}: got {version}, expected {expected_version}"
                )
            nodes_bytes_len = node_count * self.NODE_STRUCT.size
            hashes_bytes_len = node_count * self.HASH_STRUCT.size
            if nodes_offset + nodes_bytes_len > self._file_size:
                raise ValueError("NBSTORE nodes table extends past EOF")
            if hashes_offset + hashes_bytes_len > self._file_size:
                raise ValueError("NBSTORE hashes table extends past EOF")
            if names_offset + names_size > self._file_size:
                raise ValueError("NBSTORE names table extends past EOF")

            self.header = NBStoreHeader(
                magic=magic,
                version=version,
                node_count=node_count,
                nodes_offset=nodes_offset,
                hashes_offset=hashes_offset,
                names_offset=names_offset,
                names_size=names_size,
                game_id=game_id_raw.split(b"\x00", 1)[0].decode("ascii", errors="replace"),
                disc_number=disc_number,
                revision=revision,
                file_size=self._file_size,
            )

            self._fp.seek(nodes_offset)
            self._nodes_buf = self._fp.read(nodes_bytes_len)
            self._fp.seek(hashes_offset)
            self._hashes_buf = self._fp.read(hashes_bytes_len)
            self._fp.seek(names_offset)
            self._names_buf = self._fp.read(names_size)
            self._hash_keys = [
                self.HASH_STRUCT.unpack_from(self._hashes_buf, i * self.HASH_STRUCT.size)[0]
                for i in range(node_count)
            ]
        except Exception:
            self._fp.close()
            raise

    def __enter__(self) -> "NBStoreReader":
        return self

    def __exit__(self, exc_type: Any, exc: Any, tb: Any) -> None:
        self.close()

    def close(self) -> None:
        if not self._fp.closed:
            self._fp.close()

    def validate_version(
        self,
        nebula_root: Path | str | None = None,
        expected_version: int | None = None,
    ) -> int:
        """Validate header.version against explicit expected_version or store_version.inc."""
        if expected_version is None:
            if nebula_root is None:
                raise ValueError("Either nebula_root or expected_version must be supplied")
            expected_version = read_store_version(nebula_root)
        if self.header.version != expected_version:
            raise ValueError(
                f"NBSTORE version {self.header.version} != expected {expected_version}"
            )
        return self.header.version

    def _read_name(self, name_offset: int) -> str:
        if name_offset >= len(self._names_buf):
            raise ValueError(f"name_offset {name_offset} out of bounds ({len(self._names_buf)})")
        end = self._names_buf.find(b"\x00", name_offset)
        if end == -1:
            end = len(self._names_buf)
        return self._names_buf[name_offset:end].decode("utf-8", errors="replace")

    def node(self, index: int) -> NBStoreNode:
        if index < 0 or index >= self.header.node_count:
            raise IndexError(f"Node index {index} out of range (0..{self.header.node_count})")
        pos, size, name_off, parent, flags, first_child, children, _ = self.NODE_STRUCT.unpack_from(
            self._nodes_buf, index * self.NODE_STRUCT.size
        )
        return NBStoreNode(
            index=index,
            name=self._read_name(name_off),
            position=pos,
            size=size,
            name_offset=name_off,
            parent=parent,
            flags=flags,
            first_child=first_child,
            children=children,
        )

    def node_path(self, index: int) -> str:
        parts: list[str] = []
        cur = index
        seen = 0
        while cur != 0 and seen <= self.header.node_count:
            n = self.node(cur)
            parts.append(n.name)
            cur = n.parent
            seen += 1
        parts.reverse()
        return "/".join(parts)

    def find(self, path: str) -> NBStoreNode | None:
        normalized, target_hash = fnv1a_64_store_path(path)
        if not normalized:
            return self.node(0) if self.header.node_count > 0 else None
        idx = bisect.bisect_left(self._hash_keys, target_hash)
        while idx < len(self._hash_keys) and self._hash_keys[idx] == target_hash:
            _, node_idx, _ = self.HASH_STRUCT.unpack_from(
                self._hashes_buf, idx * self.HASH_STRUCT.size
            )
            if self.node_path(node_idx).lower() == normalized:
                return self.node(node_idx)
            idx += 1
        return None

    def exists(self, path: str) -> bool:
        return self.find(path) is not None

    def list_children(self, path: str = "") -> list[NBStoreNode]:
        parent = self.find(path) if path else self.node(0)
        if parent is None or not parent.is_folder:
            return []
        return [self.node(parent.first_child + i) for i in range(parent.children)]

    def read_bytes(self, path: str, max_bytes: int | None = None) -> bytes:
        n = self.find(path)
        if n is None:
            raise FileNotFoundError(f"Entry not found in {self.path}: {path}")
        if n.is_folder:
            raise IsADirectoryError(f"Entry is a folder in {self.path}: {path}")
        to_read = n.size if max_bytes is None else min(n.size, max_bytes)
        if n.position + to_read > self._file_size:
            raise ValueError(f"Entry {path} extends past EOF ({n.position + to_read} > {self._file_size})")
        self._fp.seek(n.position)
        return self._fp.read(to_read)


@dataclass(frozen=True)
class NBProgHeader:
    magic: bytes
    vertex_count: int
    pixel_count: int
    interface: str
    identity: bytes


@dataclass(frozen=True)
class NBProgRow:
    shader_id: int
    flags: int
    coords: int
    variants: tuple[tuple[int, int], ...]


class NBProgReader:
    """Pure-Python parser for NBPROG04 compiled shader blobs (programs_dxil.bin)."""

    MAGIC = b"NBPROG04"
    STORED = 10
    ROW_STRUCT = struct.Struct("<QII" + "II" * STORED)  # 96 bytes

    def __init__(self, path_or_bytes: Path | str | bytes) -> None:
        if isinstance(path_or_bytes, (bytes, bytearray)):
            data = bytes(path_or_bytes)
        else:
            data = Path(path_or_bytes).read_bytes()
        if len(data) < 64:
            raise ValueError(f"NBPROG04 data too small ({len(data)} bytes)")
        magic = data[:8]
        if magic != self.MAGIC:
            raise ValueError(f"Invalid NBPROG04 magic: {magic!r}")
        vertex_count, pixel_count = struct.unpack_from("<II", data, 8)
        interface = data[16:56].decode("ascii", errors="replace")
        if len(interface) != 40 or not re.fullmatch(r"[0-9a-fA-F]{40}", interface):
            raise ValueError(f"Invalid 40-char interface SHA-1 in NBPROG04 header: {interface!r}")
        identity = data[56:64]
        if identity == b"\x00" * 8:
            raise ValueError("NBPROG04 identity must be non-zero")

        total_rows = vertex_count + pixel_count
        rows_end = 64 + total_rows * self.ROW_STRUCT.size
        if rows_end > len(data):
            raise ValueError(
                f"NBPROG04 row table exceeds file size ({rows_end} > {len(data)})"
            )

        self.header = NBProgHeader(
            magic=magic,
            vertex_count=vertex_count,
            pixel_count=pixel_count,
            interface=interface.lower(),
            identity=identity,
        )
        self.vertex_rows: list[NBProgRow] = []
        self.pixel_rows: list[NBProgRow] = []
        for i in range(total_rows):
            unpacked = self.ROW_STRUCT.unpack_from(data, 64 + i * self.ROW_STRUCT.size)
            shader_id, flags, coords = unpacked[0], unpacked[1], unpacked[2]
            variants = tuple((unpacked[3 + j * 2], unpacked[4 + j * 2]) for j in range(self.STORED))
            row = NBProgRow(shader_id=shader_id, flags=flags, coords=coords, variants=variants)
            if i < vertex_count:
                self.vertex_rows.append(row)
            else:
                self.pixel_rows.append(row)

    def find(self, shader_id: int, pixel: bool = False) -> NBProgRow | None:
        rows = self.pixel_rows if pixel else self.vertex_rows
        lo, hi = 0, len(rows)
        while lo < hi:
            mid = (lo + hi) // 2
            if rows[mid].shader_id < shader_id:
                lo = mid + 1
            else:
                hi = mid
        if lo < len(rows) and rows[lo].shader_id == shader_id:
            return rows[lo]
        return None


@dataclass(frozen=True)
class NebulaSaveHeader:
    magic: bytes
    version: int
    crc32: int
    file_count: int


class NebulaSaveReader:
    """Pure-Python parser for NBSV v4 Nebula.sav files."""

    MAGIC = b"NBSV"
    EXPECTED_VERSION = 4
    EXPECTED_FILE_COUNT = 18
    EXPECTED_SLOTS = tuple(
        [f"mario{i}" for i in range(1, 7)]
        + [f"luigi{i}" for i in range(1, 7)]
        + [f"config{i}" for i in range(1, 7)]
    )
    HEADER_STRUCT = struct.Struct("<4sIII")
    ROW_STRUCT = struct.Struct("<16sI")

    def __init__(self, path_or_bytes: Path | str | bytes) -> None:
        if isinstance(path_or_bytes, (bytes, bytearray)):
            data = bytes(path_or_bytes)
        else:
            data = Path(path_or_bytes).read_bytes()
        if len(data) < self.HEADER_STRUCT.size:
            raise ValueError(f"NBSV data too small ({len(data)} bytes)")
        magic, version, crc32, file_count = self.HEADER_STRUCT.unpack_from(data, 0)
        if magic != self.MAGIC:
            raise ValueError(f"Invalid NBSV magic: {magic!r}")
        if version != self.EXPECTED_VERSION:
            raise ValueError(f"Unexpected NBSV version {version}, expected {self.EXPECTED_VERSION}")
        if file_count != self.EXPECTED_FILE_COUNT:
            raise ValueError(
                f"Unexpected NBSV file_count {file_count}, expected {self.EXPECTED_FILE_COUNT}"
            )
        computed_crc = zlib.crc32(data[self.HEADER_STRUCT.size :]) & 0xFFFFFFFF
        self.crc_valid = computed_crc == crc32
        self.computed_crc32 = computed_crc
        self.header = NebulaSaveHeader(
            magic=magic,
            version=version,
            crc32=crc32,
            file_count=file_count,
        )

        dir_start = self.HEADER_STRUCT.size
        dir_end = dir_start + file_count * self.ROW_STRUCT.size
        if dir_end > len(data):
            raise ValueError(f"NBSV directory extends past EOF ({dir_end} > {len(data)})")

        rows: list[tuple[str, int]] = []
        for i in range(file_count):
            raw_name, payload_size = self.ROW_STRUCT.unpack_from(
                data, dir_start + i * self.ROW_STRUCT.size
            )
            name = raw_name.split(b"\x00", 1)[0].decode("ascii", errors="replace")
            rows.append((name, payload_size))

        self.row_names: list[str] = []
        self.slots: dict[str, bytes] = {}
        offset = dir_end
        for name, payload_size in rows:
            if offset + payload_size > len(data):
                raise ValueError(
                    f"NBSV payload for {name!r} extends past EOF ({offset + payload_size} > {len(data)})"
                )
            self.row_names.append(name)
            self.slots[name] = data[offset : offset + payload_size]
            offset += payload_size


# ---------------------------------------------------------------------------
# Probe & Pattern Utilities
# ---------------------------------------------------------------------------


def sha1_file(path: Path | str) -> str:
    """Compute lowercase SHA-1 hex digest of a file using chunked reads."""
    p = Path(path)
    h = hashlib.sha1()
    with p.open("rb") as f:
        while True:
            chunk = f.read(1024 * 1024)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def git_probe(repo_path: Path | str) -> dict[str, Any]:
    """Inspect a git repository's HEAD commit SHA and clean porcelain status."""
    repo = Path(repo_path)
    if not repo.exists():
        return {"exists": False, "is_git": False, "head": "", "short_head": "", "porcelain": "", "clean": False, "error": "path missing"}
    head_res = subprocess.run(
        ["git", "-C", str(repo), "rev-parse", "HEAD"],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=15,
    )
    if head_res.returncode != 0:
        return {
            "exists": True,
            "is_git": False,
            "head": "",
            "short_head": "",
            "porcelain": "",
            "clean": False,
            "error": head_res.stderr.strip(),
        }
    head = head_res.stdout.strip()
    status_res = subprocess.run(
        ["git", "-C", str(repo), "status", "--porcelain"],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=15,
    )
    porcelain = status_res.stdout.strip()
    return {
        "exists": True,
        "is_git": True,
        "head": head,
        "short_head": head[:8],
        "porcelain": porcelain,
        "clean": status_res.returncode == 0 and porcelain == "",
        "error": status_res.stderr.strip() if status_res.returncode != 0 else "",
    }


def _vs_install_roots() -> list[Path]:
    roots: list[Path] = []
    vswhere = Path(r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe")
    if vswhere.is_file():
        try:
            res = subprocess.run(
                [
                    str(vswhere),
                    "-latest",
                    "-products",
                    "*",
                    "-requires",
                    "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                    "-property",
                    "installationPath",
                ],
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=10,
            )
            for line in res.stdout.splitlines():
                p = Path(line.strip())
                if line.strip() and p.is_dir():
                    roots.append(p)
        except (OSError, subprocess.SubprocessError):
            pass
    for fallback in (
        Path(r"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"),
        Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community"),
        Path(r"C:\Program Files\Microsoft Visual Studio\2022\Professional"),
        Path(r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise"),
    ):
        if fallback.is_dir() and fallback not in roots:
            roots.append(fallback)
    return roots


def _windows_kit_bin_dirs() -> list[Path]:
    base = Path(r"C:\Program Files (x86)\Windows Kits\10\bin")
    dirs: list[Path] = []
    preferred = base / "10.0.26100.0" / "x64"
    if preferred.is_dir():
        dirs.append(preferred)
    if base.is_dir():
        for entry in sorted(base.iterdir(), reverse=True):
            x64 = entry / "x64"
            if x64.is_dir() and x64 not in dirs:
                dirs.append(x64)
    return dirs


def find_tool(name: str, garigari_dir: Path | str | None = None) -> Path | None:
    """Locate a required build/disassembly tool across PATH and known Windows install roots."""
    norm = name.lower()
    if norm.endswith(".exe"):
        norm = norm[:-4]

    which_hit = shutil.which(norm) or shutil.which(f"{norm}.exe")
    if which_hit:
        return Path(which_hit)

    candidates: list[Path] = []
    if norm == "cargo":
        candidates.append(Path.home() / ".cargo" / "bin" / "cargo.exe")
    elif norm in ("clang++", "clang"):
        exe_name = "clang++.exe" if norm == "clang++" else "clang.exe"
        candidates.append(Path(r"C:\Program Files\LLVM\bin") / exe_name)
    elif norm in ("dxc", "fxc"):
        exe_name = f"{norm}.exe"
        for kit_dir in _windows_kit_bin_dirs():
            candidates.append(kit_dir / exe_name)
    elif norm == "vswhere":
        candidates.append(Path(r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"))
    elif norm == "cmake":
        for vs_root in _vs_install_roots():
            candidates.append(
                vs_root / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" / "CMake" / "bin" / "cmake.exe"
            )
    elif norm == "ninja":
        candidates.append(Path(sys.executable).resolve().parent / "Scripts" / "ninja.exe")
        for vs_root in _vs_install_roots():
            candidates.append(
                vs_root / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" / "Ninja" / "ninja.exe"
            )
    elif norm == "dtk":
        if garigari_dir is not None:
            gdir = Path(garigari_dir)
            candidates.extend([gdir / "build" / "tools" / "dtk.exe", gdir / "build" / "tools" / "dtk"])
        candidates.append(Path.home() / ".cargo" / "bin" / "dtk.exe")

    for candidate in candidates:
        if candidate.is_file():
            return candidate
    return None


def dtk_probe(garigari_dir: Path | str) -> dict[str, Any]:
    """Probe dtk binary availability in PATH or <garigari_dir>/build/tools/dtk.exe."""
    gdir = Path(garigari_dir)
    which_hit = shutil.which("dtk")
    if which_hit:
        path = Path(which_hit)
        source = "PATH"
    else:
        local_exe = gdir / "build" / "tools" / "dtk.exe"
        local_bin = gdir / "build" / "tools" / "dtk"
        if local_exe.is_file():
            path = local_exe
            source = "garigari_build_tools"
        elif local_bin.is_file():
            path = local_bin
            source = "garigari_build_tools"
        else:
            return {"found": False, "path": None, "source": None, "version_output": ""}

    version_output = ""
    try:
        res = subprocess.run(
            [str(path), "--version"],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=10,
        )
        version_output = (res.stdout or res.stderr).strip()
    except (OSError, subprocess.SubprocessError) as exc:
        version_output = f"probe error: {exc}"
    return {
        "found": True,
        "path": str(path),
        "source": source,
        "version_output": version_output,
    }


def toolchain_probe(garigari_dir: Path | str | None = None) -> dict[str, dict[str, Any]]:
    """Probe availability of cargo, clang++, cmake, ninja, dxc, fxc, dtk, and vswhere."""
    tools = ("cargo", "clang++", "cmake", "ninja", "dxc", "fxc", "dtk", "vswhere")
    out: dict[str, dict[str, Any]] = {}
    for tool in tools:
        if tool == "dtk" and garigari_dir is not None:
            info = dtk_probe(garigari_dir)
            out[tool] = {"found": info["found"], "path": info["path"], "source": info["source"]}
        else:
            p = find_tool(tool, garigari_dir=garigari_dir)
            out[tool] = {"found": p is not None, "path": str(p) if p else None}
    return out


def shader_store_probe(nebula_root: Path | str) -> dict[str, Any]:
    """Verify store_version.inc, store_programs.txt, table.tsv digest, and .hlsl file counts."""
    root = Path(nebula_root)
    version_path = root / "runtime" / "include" / "galaxy" / "store_version.inc"
    recorded_path = root / "runtime" / "include" / "galaxy" / "store_programs.txt"
    programs_dir = root / "runtime" / "src" / "render" / "programs"
    table_path = programs_dir / "table.tsv"

    store_version = int(version_path.read_text(encoding="utf-8").strip())
    recorded_parts = recorded_path.read_text(encoding="utf-8").split()
    recorded_version = int(recorded_parts[0]) if len(recorded_parts) >= 1 else None
    recorded_digest = recorded_parts[1] if len(recorded_parts) >= 2 else ""

    table_lines = [line for line in table_path.read_text(encoding="utf-8").splitlines() if line]
    names = sorted("".join(line.split("\t")[:2]) for line in table_lines)
    computed_digest = hashlib.sha256("\n".join(names).encode("utf-8")).hexdigest()

    vertex_rows = 0
    pixel_rows = 0
    groups: set[str] = set()
    for line in table_lines:
        cols = line.split("\t")
        if len(cols) >= 3:
            if cols[0] == "v":
                vertex_rows += 1
            elif cols[0] == "p":
                pixel_rows += 1
            groups.add(cols[2])

    hlsl_files = {p.stem for p in programs_dir.glob("*.hlsl")}
    missing_group_hlsl = sorted(groups - hlsl_files)

    return {
        "store_version": store_version,
        "recorded_version": recorded_version,
        "recorded_digest": recorded_digest,
        "computed_digest": computed_digest,
        "version_match": recorded_version == store_version,
        "digest_match": recorded_digest == computed_digest,
        "table_rows": len(table_lines),
        "vertex_rows": vertex_rows,
        "pixel_rows": pixel_rows,
        "group_count": len(groups),
        "hlsl_file_count": len(hlsl_files),
        "missing_group_hlsl": missing_group_hlsl,
    }


_HOST_LOG_RE = re.compile(r"^\[\s*(\d+\.\d+)\]\s+([a-zA-Z0-9_-]+):\s*(.*)$")


def parse_host_logs(text: str) -> list[dict[str, Any]]:
    """Parse `[   X.XXX] tag: message` host log lines (and raw unprefixed SMG2 lines)."""
    entries: list[dict[str, Any]] = []
    for raw_line in text.splitlines():
        line = raw_line.rstrip("\r")
        if not line:
            continue
        m = _HOST_LOG_RE.match(line)
        if m:
            entries.append(
                {
                    "timestamp_s": float(m.group(1)),
                    "tag": m.group(2),
                    "message": m.group(3),
                    "raw": line,
                }
            )
        else:
            entries.append(
                {
                    "timestamp_s": None,
                    "tag": "raw",
                    "message": line,
                    "raw": line,
                }
            )
    return entries


_COUNTERS_RE = re.compile(
    r"host:\s+frame\s+(\d+),\s+file reads\s+(\d+),\s+save ops\s+(\d+),\s+"
    r"sound blocks\s+(\d+),\s+sound subframes\s+(\d+)"
    r"(?:,\s+renderer frames\s+(\d+))?"
)
_SCENES_RE = re.compile(r"host:\s+3D scenes\s+(\d+)")
_BOOT_STEP_RE = re.compile(r"boot:\s+(.+?)\s+at\s+\d+\s+ms")


def extract_host_counters(text: str) -> dict[str, Any]:
    """Extract frame/I/O/save/sound/3D-scene counters and boot steps from host logs."""
    last_frame = 0
    last_reads = 0
    last_saves = 0
    last_sound_blocks = 0
    last_sound_subframes = 0
    last_renderer_frames = 0
    scenes_3d = 0
    boot_steps: list[str] = []

    for m in _BOOT_STEP_RE.finditer(text):
        boot_steps.append(m.group(1))
    for m in _COUNTERS_RE.finditer(text):
        last_frame = int(m.group(1))
        last_reads = int(m.group(2))
        last_saves = int(m.group(3))
        last_sound_blocks = int(m.group(4))
        last_sound_subframes = int(m.group(5))
        if m.group(6) is not None:
            last_renderer_frames = int(m.group(6))
    for m in _SCENES_RE.finditer(text):
        scenes_3d = int(m.group(1))

    return {
        "frame": last_frame,
        "file_reads": last_reads,
        "save_ops": last_saves,
        "sound_blocks": last_sound_blocks,
        "sound_subframes": last_sound_subframes,
        "renderer_frames": last_renderer_frames,
        "3d_scenes": scenes_3d,
        "boot_steps": boot_steps,
        "has_time_limit_reached": "host: time limit reached" in text,
        "has_game_ended": "host: the game ended" in text,
    }


def find_todo_markers(text: str) -> list[str]:
    """Return all lines containing `TODO(Garigari)` diagnostic markers."""
    return [line.strip() for line in text.splitlines() if "TODO(Garigari)" in line]


_NEB_CODE_RE = re.compile(r"\b(NEB-[A-Z]\d{2})\b")


def scan_crash_reports(crash_dir: Path | str) -> list[dict[str, Any]]:
    """Scan a crash-dumps directory for `nebula-*.txt` and `*.dmp` artifacts."""
    cdir = Path(crash_dir)
    if not cdir.is_dir():
        return []
    reports: list[dict[str, Any]] = []
    for path in sorted(cdir.iterdir()):
        if not path.is_file():
            continue
        lower = path.name.lower()
        if lower.endswith(".txt") and lower.startswith("nebula-"):
            content = path.read_text(encoding="utf-8", errors="replace")
            codes = sorted(set(_NEB_CODE_RE.findall(content)))
            first_line = content.splitlines()[0].strip() if content.splitlines() else ""
            reports.append(
                {
                    "type": "txt",
                    "filename": path.name,
                    "path": str(path),
                    "size": path.stat().st_size,
                    "codes": codes,
                    "headline": first_line,
                }
            )
        elif lower.endswith(".dmp"):
            reports.append(
                {
                    "type": "dmp",
                    "filename": path.name,
                    "path": str(path),
                    "size": path.stat().st_size,
                    "codes": [],
                    "headline": "",
                }
            )
    return reports
