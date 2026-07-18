#!/usr/bin/env python3
"""运行 FD2 canonical Ghidra 11.3.2 structural baseline。

默认只生成结构 inventory，不覆盖语义名称表和大型 C 反编译。每次运行使用新的
临时 Ghidra project；``--determinism-check`` 连续运行两次并比较规范输出哈希。
"""
from __future__ import annotations

import argparse
import hashlib
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

from build_fd2_function_seeds import build as build_function_seeds
from fd2_analysis_bundle import build_bundle, canonical_json

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_EXE = ROOT / "original_game" / "FD2.EXE"
DEFAULT_BUNDLE = ROOT / "tools" / "fd2-analysis-bundle"
DEFAULT_CODE = ROOT / "tools" / "fd2_le_code0.bin"
DEFAULT_RELOCATIONS = ROOT / "tools" / "fd2_le_relocation_manifest.tsv"
DEFAULT_OUTPUT = ROOT / "docs" / "generated" / "ghidra-canonical"
DEFAULT_SEEDS = ROOT / "docs" / "generated" / "fd2-function-seeds.json"
DEFAULT_GHIDRA = Path("/tmp/ghidra_11.3.2_PUBLIC/support/analyzeHeadless")
DEFAULT_GHIDRA_RELEASE = Path("/tmp/ghidra_11.3.2_PUBLIC_20250415.zip")
EXPECTED_GHIDRA_VERSION = "11.3.2"
EXPECTED_GHIDRA_RELEASE_SHA256 = (
    "99d45035bdcc3d6627e7b1232b7b379905a9fad76c772c920602e2b5d8b2dac2")
SCRIPTS = ROOT / "tools" / "ghidra-scripts"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def write_checksum(path: Path) -> None:
    path.with_name(path.stem + ".sha256").write_text(
        f"{sha256(path)}  {path.name}\n", encoding="ascii")


def verify_ghidra_install(headless: Path, release_zip: Path | None) -> None:
    """Fail closed unless both installed version and official archive agree."""
    root = headless.parent.parent
    properties = root / "Ghidra" / "application.properties"
    if not properties.is_file():
        raise RuntimeError(f"Ghidra application.properties not found: {properties}")
    values = {}
    for line in properties.read_text(encoding="utf-8").splitlines():
        if "=" in line and not line.startswith("#"):
            key, value = line.split("=", 1)
            values[key.strip()] = value.strip()
    if values.get("application.version") != EXPECTED_GHIDRA_VERSION:
        raise RuntimeError(
            "unexpected Ghidra version: "
            f"{values.get('application.version')!r} != {EXPECTED_GHIDRA_VERSION!r}")
    if values.get("application.release.name") != "PUBLIC":
        raise RuntimeError("Ghidra install is not the expected PUBLIC release")
    if release_zip is None or not release_zip.is_file():
        raise RuntimeError(f"Ghidra release ZIP not found: {release_zip}")
    actual = sha256(release_zip)
    if actual != EXPECTED_GHIDRA_RELEASE_SHA256:
        raise RuntimeError(
            f"Ghidra release ZIP SHA-256 mismatch: {actual} != "
            f"{EXPECTED_GHIDRA_RELEASE_SHA256}")


def run_once(headless: Path, bundle: Path, output: Path, label: str,
             seeds: Path | None = None, decompile: bool = False) -> Path:
    output.mkdir(parents=True, exist_ok=True)
    expected_outputs = [
        "canonical-validation.ok", "structural-inventory.json",
        "structural-inventory.sha256", "function-analysis.json",
        "function-analysis.sha256", "ghidra-decomp-canonical.c",
        "ghidra-decomp-canonical.sha256", "decompile-manifest.json",
        "decompile-manifest.sha256",
    ]
    for name in expected_outputs:
        path = output / name
        if path.exists():
            path.unlink()
    with tempfile.TemporaryDirectory(prefix="fd2-ghidra-project-") as project_dir:
        holder = Path(project_dir) / "fd2-import-holder.bin"
        holder.write_bytes(b"\x00")
        command = [
            str(headless), project_dir, "FD2Canonical_" + label,
            "-import", str(holder),
            "-processor", "x86:LE:32:default", "-cspec", "gcc",
            "-scriptPath", str(SCRIPTS),
            "-preScript", "import_fd2_canonical.py", str(bundle),
            "-noanalysis",
        ]
        if seeds is not None:
            command.extend(["-postScript", "analyze_fd2_functions.py",
                            str(seeds), str(output)])
        if decompile:
            command.extend(["-postScript", "export_fd2_decomp.py", str(output)])
        command.extend([
            "-postScript", "validate_fd2_canonical.py", str(bundle), str(output),
            "-overwrite", "-deleteProject",
        ])
        env = os.environ.copy()
        env.setdefault("LC_ALL", "C.UTF-8")
        subprocess.run(command, cwd=ROOT, env=env, check=True)
    success_marker = output / "canonical-validation.ok"
    inventory = output / "structural-inventory.json"
    if not success_marker.is_file() or success_marker.read_text(encoding="ascii") != "PASS\n":
        raise RuntimeError("Ghidra canonical validator did not write a PASS marker")
    if not inventory.is_file():
        raise RuntimeError(f"Ghidra did not write {inventory}")
    if seeds is not None and not (output / "function-analysis.json").is_file():
        raise RuntimeError("Ghidra function analysis did not produce its ledger")
    if decompile:
        for name in ("ghidra-decomp-canonical.c", "decompile-manifest.json"):
            if not (output / name).is_file():
                raise RuntimeError(f"Ghidra decompile export did not produce {name}")
    return inventory


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    parser.add_argument("--bundle", type=Path, default=DEFAULT_BUNDLE)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--ghidra-headless", type=Path, default=DEFAULT_GHIDRA)
    parser.add_argument("--ghidra-release-zip", type=Path,
                        default=DEFAULT_GHIDRA_RELEASE,
                        help="official Ghidra 11.3.2 release ZIP used for hash identity")
    parser.add_argument("--seeds", type=Path, default=DEFAULT_SEEDS)
    parser.add_argument("--functions", action="store_true",
                        help="apply evidence-tiered function seeds and export the boundary ledger")
    parser.add_argument("--decompile", action="store_true",
                        help="export canonical C; implies --functions")
    parser.add_argument("--determinism-check", action="store_true")
    args = parser.parse_args()

    exe = args.exe.resolve()
    bundle = args.bundle.resolve()
    output = args.output.resolve()
    headless = args.ghidra_headless.resolve()
    seeds = args.seeds.resolve()
    if not headless.is_file():
        raise SystemExit(f"Ghidra headless not found: {headless}")
    verify_ghidra_install(headless, args.ghidra_release_zip.resolve())
    build_bundle(exe, bundle)
    if args.decompile:
        args.functions = True
    if args.functions:
        seed_manifest = build_function_seeds(DEFAULT_CODE, DEFAULT_RELOCATIONS)
        seeds.parent.mkdir(parents=True, exist_ok=True)
        seeds.write_bytes(canonical_json(seed_manifest))

    if args.determinism_check:
        with tempfile.TemporaryDirectory(prefix="fd2-ghidra-output-") as tmp:
            first_dir = Path(tmp) / "first"
            second_dir = Path(tmp) / "second"
            first = run_once(headless, bundle, first_dir, "first",
                             seeds if args.functions else None, args.decompile)
            second = run_once(headless, bundle, second_dir, "second",
                              seeds if args.functions else None, args.decompile)
            first_hash = sha256(first)
            second_hash = sha256(second)
            if first_hash != second_hash or first.read_bytes() != second.read_bytes():
                raise RuntimeError(
                    f"non-deterministic structural inventories: {first_hash} != {second_hash}")
            output.mkdir(parents=True, exist_ok=True)
            shutil.copy2(first, output / first.name)
            shutil.copy2(first.with_suffix(".sha256"),
                         output / first.with_suffix(".sha256").name)
            shutil.copy2(first_dir / "canonical-validation.ok",
                         output / "canonical-validation.ok")
            if args.functions:
                function_first = first_dir / "function-analysis.json"
                function_second = second_dir / "function-analysis.json"
                if (sha256(function_first) != sha256(function_second) or
                        function_first.read_bytes() != function_second.read_bytes()):
                    raise RuntimeError("non-deterministic function analysis")
                function_output = output / function_first.name
                shutil.copy2(function_first, function_output)
                write_checksum(function_output)
            if args.decompile:
                for name in ("ghidra-decomp-canonical.c", "decompile-manifest.json"):
                    artifact_first = first_dir / name
                    artifact_second = second_dir / name
                    if (sha256(artifact_first) != sha256(artifact_second) or
                            artifact_first.read_bytes() != artifact_second.read_bytes()):
                        raise RuntimeError(f"non-deterministic {name}")
                    artifact_output = output / name
                    shutil.copy2(artifact_first, artifact_output)
                    write_checksum(artifact_output)
            print(f"determinism=PASS sha256={first_hash}")
    else:
        inventory = run_once(headless, bundle, output, "single",
                             seeds if args.functions else None, args.decompile)
        write_checksum(inventory)
        if args.functions:
            write_checksum(output / "function-analysis.json")
        if args.decompile:
            write_checksum(output / "ghidra-decomp-canonical.c")
            write_checksum(output / "decompile-manifest.json")
        print(f"inventory_sha256={sha256(inventory)}")
    print(f"output={output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
