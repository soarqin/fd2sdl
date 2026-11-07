"""记录当前Windows x64 Release实际输入；不构建、复制DLL或执行程序。"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import re

import package_windows_release as package


SNAPSHOT_SCHEMA = "fd2-windows-release-source-snapshot-v1"
TARGET_SCHEMA = "fd2-release-cmake-target-v1"
DEPENDENCIES_SCHEMA = "fd2-windows-runtime-dependencies-v1"
UCRT_CONTRACTS = tuple(f"api-ms-win-crt-{name}-l1-1-0.dll" for name in
                       ("stdio", "heap", "runtime", "convert", "environment", "string", "math", "locale"))


def _path(value, *, file=False, original=None) -> Path:
    path = Path(value)
    if not path.is_absolute():
        raise ValueError("absolute input path required")
    if any(part.casefold() == "original_game" for part in path.parts):
        raise ValueError("original_game is not a release input")
    resolved = path.resolve(strict=True)
    if any(part.casefold() == "original_game" for part in resolved.parts):
        raise ValueError("original_game is not a release input")
    if original is not None and package._inside(resolved, original):
        raise ValueError("actual original resource directory is not a release input")
    if file and (package._link(path) or not resolved.is_file()):
        raise ValueError("ordinary input file required")
    if not file and not resolved.is_dir():
        raise ValueError("input directory required")
    return resolved


def _output(value, root) -> Path:
    path = Path(value).absolute()
    if any(part.casefold() == "original_game" for part in path.parts):
        raise ValueError("output cannot be inside original_game")
    parent = _path(path.parent, original=(root / "original_game").resolve())
    result = parent / path.name
    if result.exists() or package._link(result):
        raise ValueError("output must be new with an existing real parent")
    return result


def _fact(path: Path) -> dict:
    return {"path": str(path), "sha256": package.sha256(package._read(path))}


def _json(path, *, original=None) -> tuple[Path, bytes, dict]:
    path = _path(path, file=True, original=original)
    raw = package._read(path)
    value = json.loads(raw.decode("utf-8-sig"))
    if not isinstance(value, dict):
        raise ValueError("JSON object required")
    return path, raw, value


def _write(path: Path, value: dict):
    with path.open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2)
        stream.write("\n")


def snapshot(source_root: Path, output: Path) -> dict:
    root = _path(source_root)
    destination = _output(output, root)
    files = package.source_inventory(root)
    package._check_inventory(package.source_inventory(root), files, "source before snapshot")
    result = {"schema": SNAPSHOT_SCHEMA, "source_root": str(root), "source_files": files,
              "build_executed_by_recorder": False}
    _write(destination, result)
    return result


def _vc_runtime(name: str) -> bool:
    return name == "ucrtbase.dll" or re.fullmatch(r"(?:vcruntime|msvcp|concrt)[0-9][a-z0-9_]*\.dll", name) is not None


def record_inputs(snapshot_path: Path, target_path: Path, sdk_root: Path,
                  dependencies_path: Path, output: Path, windows_root: Path,
                  forbidden_prefixes=()) -> dict:
    snap_path, snap_raw, snap = _json(snapshot_path)
    if snap.get("schema") != SNAPSHOT_SCHEMA:
        raise ValueError("before-build source snapshot required")
    root = _path(snap["source_root"])
    original = (root / "original_game").resolve()
    # snapshot自己的root只能在读该JSON后得知；此后不读任何原资源中的metadata。
    _path(snap_path, file=True, original=original)
    destination = _output(output, root)
    target_path, target_raw, target = _json(target_path, original=original)
    scan_path, scan_raw, scan = _json(dependencies_path, original=original)
    sources = package.source_inventory(root)
    package._check_inventory(sources, snap.get("source_files"), "source since before-build snapshot")
    if (target.get("schema") != TARGET_SCHEMA or target.get("configuration") != "Release" or
            target.get("architecture") != "x64" or target.get("pointer_size") != 8):
        raise ValueError("actual CMake Windows x64 Release target record required")
    if _path(target["source_root"], original=original) != root:
        raise ValueError("CMake source root differs from the before-build snapshot")
    if not re.fullmatch(r"Visual Studio [0-9]+ [0-9]{4}", target.get("generator", "")):
        raise ValueError("the recorded Visual Studio x64 generator is required")
    for key in ("compiler", "compiler_version", "sdl_target"):
        if not isinstance(target.get(key), str) or not target[key]:
            raise ValueError("complete actual compiler/SDL target metadata required")
    kind = target.get("sdl_type")
    if kind not in {"STATIC_LIBRARY", "SHARED_LIBRARY"}:
        raise ValueError("actual typed SDL library target required")

    sdk = _path(sdk_root, original=original)
    windows = _path(windows_root, original=original)
    sdk_files = package.sdk_inventory(sdk)
    configs = [row["path"] for row in sdk_files if Path(row["path"]).name == "SDL3Config.cmake"]
    if len(configs) != 1 or _path(target["sdl_package"], original=original) != (sdk / configs[0]).parent:
        raise ValueError("CMake must use the single installed SDK package")
    license_files = [row["path"] for row in sdk_files if Path(row["path"]).name.startswith(
        ("LICENSE", "COPYING", "NOTICE")) or "licenses" in Path(row["path"]).parts]
    if not license_files:
        raise ValueError("installed SDL/component license notices required")
    binaries = {}
    for key, field in (("executable", "executable"), ("target_file", "sdl_target_file"),
                       ("link_file", "sdl_link_file")):
        binaries[key] = _fact(_path(target[field], file=True, original=original))
    package._pe_x64(package._read(Path(binaries["executable"]["path"])))
    sdk_hashes = {row["sha256"] for row in sdk_files}
    if any(binaries[key]["sha256"] not in sdk_hashes for key in ("target_file", "link_file")):
        raise ValueError("installed SDK differs from the actual SDL target/link files")
    target_file = Path(binaries["target_file"]["path"])
    link_file = Path(binaries["link_file"]["path"])
    if link_file.suffix.casefold() != ".lib":
        raise ValueError("actual Windows SDL static/import library required")
    if kind == "STATIC_LIBRARY":
        if target_file != link_file:
            raise ValueError("static SDL target and link file must be the same actual library")
    else:
        if target_file.suffix.casefold() != ".dll":
            raise ValueError("shared SDL target must be the actual DLL")
        package._pe_x64(package._read(target_file))

    if (scan.get("schema") != DEPENDENCIES_SCHEMA or
            not isinstance(scan.get("resolved"), list) or not isinstance(scan.get("unresolved"), list) or
            not isinstance(scan.get("direct_imports"), list) or not scan["direct_imports"]):
        raise ValueError("complete actual CMake dependency scan required")
    if scan["unresolved"]:
        raise ValueError("unresolved runtime dependencies remain")
    if (_path(scan["executable"], file=True, original=original) != Path(binaries["executable"]["path"]) or
            scan.get("executable_sha256") != binaries["executable"]["sha256"]):
        raise ValueError("dependency scan does not bind the current actual executable")
    system_directory = _path(scan["system_directory"], original=original)
    if system_directory != _path(windows / "System32", original=original):
        raise ValueError("dependency scan System32 differs from the actual Windows SystemRoot")
    direct = scan["direct_imports"]
    if (any(not isinstance(name, str) or not re.fullmatch(r"[A-Za-z0-9_.-]+\.[dD][lL][lL]", name)
            for name in direct) or len({name.casefold() for name in direct}) != len(direct)):
        raise ValueError("direct PE import basenames must be complete and unique")
    supported, contracts = scan.get("api_supported"), scan.get("api_set_contracts")
    required_contracts = {name.casefold() for name in direct} & set(UCRT_CONTRACTS)
    if (not isinstance(supported, list) or any(not isinstance(name, str) for name in supported) or
            len(supported) != len(UCRT_CONTRACTS) or set(supported) != set(UCRT_CONTRACTS) or
            not isinstance(contracts, list) or any(not isinstance(name, str) for name in contracts) or
            len(contracts) != len(set(contracts)) or set(contracts) != required_contracts):
        raise ValueError("exact supported UCRT contracts and actual direct-import subset required")
    backing = _path(scan["backing_runtime"], file=True, original=original)
    if backing.parent != system_directory or backing.name.casefold() != "ucrtbase.dll":
        raise ValueError("supported UCRT contracts require actual System32/ucrtbase.dll")
    package._pe_x64(package._read(backing))
    system_facts = {str(backing): _fact(backing)}
    runtime, resolved_system, external_redist, external_facts, seen = [], [], [], [], set()
    for name in scan["resolved"]:
        if not isinstance(name, str):
            raise ValueError("resolved dependency paths must be strings")
        path = _path(name, file=True, original=original)
        identity = str(path).casefold()
        if identity in seen:
            raise ValueError("duplicate resolved dependency")
        seen.add(identity)
        basename = path.name.casefold()
        if basename.endswith("d.dll") and (_vc_runtime(basename) or basename == "ucrtbased.dll"):
            raise ValueError("Debug MSVC runtime in a Release dependency scan")
        if path.parent == system_directory:
            resolved_system.append(str(path))
            system_facts[str(path)] = _fact(path)
        elif _vc_runtime(basename):
            package._pe_x64(package._read(path))
            external_redist.append(str(path))
            external_facts.append(_fact(path))
        elif kind == "SHARED_LIBRARY" and basename == target_file.name.casefold():
            entry = _fact(path)
            if entry["sha256"] != binaries["target_file"]["sha256"] or runtime:
                raise ValueError("ambiguous or different SDL runtime DLL")
            package._pe_x64(package._read(path))
            runtime.append(entry)
        else:
            raise ValueError(f"non-system dependency is not the actual SDL DLL: {path}")
    if kind == "SHARED_LIBRARY" and not runtime:
        raise ValueError("actual shared SDL DLL is absent from the dependency scan")
    runtime_names = {Path(row["path"]).name.casefold() for row in runtime}
    system = [name for name in direct if name.casefold() not in runtime_names]
    external_names = {Path(path).name.casefold() for path in external_redist}
    for name in system:
        basename = name.casefold()
        if basename.endswith("d.dll") and (_vc_runtime(basename) or basename == "ucrtbased.dll"):
            raise ValueError("Debug MSVC direct import in a Release executable")
        if basename in required_contracts:
            continue
        if basename.startswith(("api-ms-", "ext-ms-")):
            raise ValueError("unsupported virtual API contract: " + name)
        if _vc_runtime(basename) and basename in external_names:
            continue  # 已实际解析的VC运行库：包只声明官方redist前置，不复制该DLL。
        path = _path(system_directory / name, file=True, original=original)
        if path.parent != system_directory:
            raise ValueError("direct system dependency must be an ordinary actual System32 file")
        system_facts[str(path)] = _fact(path)

    result = {"schema": package.SCHEMA, "configuration": "Release", "architecture": "x64",
              "source_root": str(root), "source_files": sources,
              "executable": binaries["executable"],
              "sdl": {"target": target["sdl_target"], "type": kind,
                      "target_file": binaries["target_file"], "link_file": binaries["link_file"],
                      "sdk_root": str(sdk), "sdk_files": sdk_files, "license_files": license_files,
                      "forbidden_prefixes": list(forbidden_prefixes)},
              "runtime_dependencies": runtime, "system_dependencies": system,
              "unresolved_dependencies": [],
              "redistributable_dependencies": [name for name in system if _vc_runtime(name.casefold())],
              "external_redistributable_paths": external_redist,
              "resolved_system_paths": resolved_system,
              "runtime_scan": {"system_directory": str(system_directory), "direct_imports": direct,
                               "api_supported": supported, "api_set_contracts": contracts,
                               "backing_runtime": _fact(backing), "system_file_facts": list(system_facts.values()),
                               "external_redistributable_facts": external_facts,
                               "rule": "ordinary actual System32 files, eight explicit UCRT contracts with actual backing, or actually resolved VC redistributable prerequisites"},
              "prerequisites": ["Windows x64", "Official Microsoft Visual C++ x64 Redistributable compatible with the recorded MSVC toolchain"],
              "toolchain": {key: target[key] for key in ("generator", "compiler", "compiler_version")},
              "source_snapshot": {"path": str(snap_path), "sha256": package.sha256(snap_raw)},
              "cmake_target_record": {"path": str(target_path), "sha256": package.sha256(target_raw)},
              "runtime_dependency_scan": {"path": str(scan_path), "sha256": package.sha256(scan_raw)},
              "source_unchanged_since_snapshot": True, "build_executed_by_recorder": False}
    package._check_inventory(package.source_inventory(root), sources, "source after recording")
    package._check_inventory(package.sdk_inventory(sdk), sdk_files, "SDK after recording")
    for path, raw in ((snap_path, snap_raw), (target_path, target_raw), (scan_path, scan_raw)):
        if package._read(path) != raw:
            raise ValueError("release recording input changed")
    for entry in [*binaries.values(), *runtime, *system_facts.values(), *external_facts]:
        package._read(Path(entry["path"]), entry["sha256"])
    _write(destination, result)
    return result


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    modes = parser.add_subparsers(dest="mode", required=True)
    before = modes.add_parser("snapshot", help="before-build App source inventory")
    before.add_argument("--source-root", type=Path, required=True)
    before.add_argument("--output", type=Path, required=True)
    inputs = modes.add_parser("inputs", help="record actual Release target/SDK/dependency inputs")
    for name in ("snapshot", "target", "sdk-root", "dependencies", "output"):
        inputs.add_argument("--" + name, type=Path, required=True)
    inputs.add_argument("--forbidden-prefix", action="append", default=[])
    args = parser.parse_args(argv)
    try:
        if args.mode == "snapshot":
            result = snapshot(args.source_root, args.output)
        else:
            if not os.environ.get("SystemRoot"):
                raise ValueError("Windows SystemRoot is required; this records Windows Release inputs only")
            result = record_inputs(args.snapshot, args.target, args.sdk_root, args.dependencies,
                                   args.output, Path(os.environ["SystemRoot"]), args.forbidden_prefix)
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(json.dumps({"ok": False, "error": str(error)}, ensure_ascii=False))
        return 1
    print(json.dumps({"ok": True, "mode": args.mode, "schema": result["schema"],
                      "output": str(args.output), "build_executed_by_recorder": False}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
