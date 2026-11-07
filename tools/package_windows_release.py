"""Windows Release本地白名单包；只复制已绑定输入，不构建、安装或运行程序。

源码/SDK清单由同一函数生成并在复制前后复核。实际SDL target与DLL依赖须来自
CMake Release记录；本工具不从文件名猜静态链接，也不复制Windows系统DLL。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import struct


SCHEMA = "fd2-windows-release-inputs-v1"
CODE_EXTENSIONS = {".c", ".cpp", ".h", ".hpp", ".inl", ".inc", ".ipp", ".tcc", ".cmake", ".in"}
REQUIRED_SOURCE = {"LICENSE", "README.md", "src/CMakeLists.txt", "src/main.c",
                   "src/third_party/nukedopl3.c", "src/third_party/nukedopl3.h",
                   "src/third_party/LICENSE.nukedopl3"}
# 仅发布说明：main启动/音乐路径与battle_animation的BG/TAI读取，不作第二套runtime校验。
DOCUMENTED_RESOURCE_FILES = ("ANI.DAT", "BG.DAT", "DATO.DAT", "FDFIELD.DAT", "FDICON.B24",
                             "FDMUS.DAT", "FDOTHER.DAT", "FDSHAP.DAT", "FDTXT.DAT", "FIGANI.DAT",
                             "SAMPLE.AD", "TAI.DAT")


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _link(path: Path) -> bool:
    return path.is_symlink() or (hasattr(path, "is_junction") and path.is_junction())


def _relative(value: str) -> str:
    path = PurePosixPath(value)
    if not value or any(c in value for c in '\\:"`$\r\n\0') or path.is_absolute() or any(
            part in {"", ".", ".."} for part in value.split("/")):
        raise ValueError(f"unsafe relative path: {value}")
    return path.as_posix()


def _read(path: Path, expected: str | None = None) -> bytes:
    if _link(path) or not path.is_file():
        raise ValueError(f"ordinary input file required: {path}")
    before = path.stat()
    data = path.read_bytes()
    after = path.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise ValueError(f"input changed while reading: {path}")
    if expected is not None and sha256(data) != expected:
        raise ValueError(f"input SHA256 mismatch: {path}")
    return data


def _tree(root: Path):
    for directory, dirs, files in os.walk(root, followlinks=False):
        parent = Path(directory)
        for name in dirs:
            if _link(parent / name):
                raise ValueError(f"linked directory is not a source/SDK input: {parent / name}")
        for name in files:
            yield parent / name


def _source_allowed(relative: str) -> bool:
    parts = PurePosixPath(relative).parts
    name = parts[-1]
    if len(parts) == 1:
        return name in {"LICENSE", "README.md", ".gitattributes", "THIRD_PARTY_NOTICES", "THIRD_PARTY_NOTICES.md"}
    if parts[0] != "src" or (len(parts) > 2 and parts[1] not in {"third_party", "cmake"}):
        return False
    return name == "CMakeLists.txt" or Path(name).suffix.lower() in CODE_EXTENSIONS or (
        "third_party" in parts and (name.startswith(("LICENSE", "COPYING", "NOTICE")) or name == "README.md"))


def _sdk_allowed(relative: str) -> bool:
    parts = PurePosixPath(relative).parts
    name = parts[-1]
    suffix = Path(name).suffix.lower()
    if parts[0] == "include":
        return suffix == ".h"
    if suffix == ".cmake":
        return parts[0] in {"lib", "cmake", "share"}
    if suffix in {".lib", ".dll"}:
        return parts[0] in {"lib", "bin"} and name.lower().startswith("sdl3")
    return (name.startswith(("LICENSE", "COPYING", "NOTICE")) or
            (parts[0] in {"licenses", "share"} and suffix in {".txt", ".md"}))


def _inventory(root: Path, paths) -> list[dict]:
    return [{"path": path.relative_to(root).as_posix(), "sha256": sha256(_read(path))}
            for path in sorted(paths)]


def source_inventory(root: Path | str) -> list[dict]:
    """完整App构建材料；测试/原版/反编译产物不属于BUILD_TESTING=OFF所需源码。"""
    root = Path(root).resolve(strict=True)
    paths = [root / name for name in ("LICENSE", "README.md", ".gitattributes") if (root / name).is_file()]
    paths += [root / name for name in ("THIRD_PARTY_NOTICES", "THIRD_PARTY_NOTICES.md") if (root / name).is_file()]
    src = root / "src"
    paths += [path for path in src.iterdir() if path.is_file() and _source_allowed(path.relative_to(root).as_posix())]
    for name in ("third_party", "cmake"):
        if (src / name).is_dir():
            if _link(src / name):
                raise ValueError("source subtree must not be linked")
            paths += [path for path in _tree(src / name) if _source_allowed(path.relative_to(root).as_posix())]
    entries = _inventory(root, paths)
    names = {entry["path"] for entry in entries}
    if not REQUIRED_SOURCE <= names or len(names & {"THIRD_PARTY_NOTICES", "THIRD_PARTY_NOTICES.md"}) != 1:
        raise ValueError("complete App source, MIT LICENSE and one THIRD_PARTY_NOTICES file are required")
    if "MIT License" not in _read(root / "LICENSE").decode("utf-8-sig"):
        raise ValueError("project LICENSE must be the selected MIT license")
    return entries


def sdk_inventory(root: Path | str) -> list[dict]:
    root = Path(root).resolve(strict=True)
    entries = _inventory(root, [path for path in _tree(root) if _sdk_allowed(path.relative_to(root).as_posix())])
    names = {entry["path"] for entry in entries}
    if "include/SDL3/SDL.h" not in names or not any(Path(name).name == "SDL3Config.cmake" for name in names):
        raise ValueError("installed SDL headers and CMake package are required")
    return entries


def _check_inventory(actual: list[dict], expected, label: str):
    if not isinstance(expected, list):
        raise ValueError(f"{label} inventory required")
    selected = []
    seen = set()
    for entry in expected:
        relative = _relative(entry["path"])
        if relative.casefold() in seen or not re.fullmatch("[0-9a-f]{64}", entry["sha256"]):
            raise ValueError(f"duplicate/invalid {label} entry")
        seen.add(relative.casefold())
        selected.append({"path": relative, "sha256": entry["sha256"]})
    if sorted(actual, key=lambda row: row["path"]) != sorted(selected, key=lambda row: row["path"]):
        raise ValueError(f"{label} whitelist/hash changed; freeze inputs again")


def _external(entry: dict) -> tuple[Path, bytes]:
    path = Path(entry["path"])
    if not path.is_absolute():
        raise ValueError("binary input must have an absolute path")
    return path.resolve(strict=True), _read(path, entry["sha256"])


def _pe_x64(data: bytes):
    if len(data) < 64 or data[:2] != b"MZ":
        raise ValueError("Windows x64 PE executable required")
    offset = struct.unpack_from("<I", data, 60)[0]
    if offset + 26 > len(data) or data[offset:offset + 4] != b"PE\0\0" or (
            struct.unpack_from("<H", data, offset + 4)[0] != 0x8664 or
            struct.unpack_from("<H", data, offset + 24)[0] != 0x20B):
        raise ValueError("Windows x64 PE executable required")


def _inside(path: Path, root: Path) -> bool:
    return path == root or root in path.parents


def _rebuild(config_dir: str, generator: str) -> bytes:
    if not re.fullmatch(r"Visual Studio [0-9]+ [0-9]{4}", generator):
        raise ValueError("this Windows kit requires the recorded Visual Studio generator")
    return ("# 修改 source/src/third_party/nukedopl3.c/.h 后可用同一SDK重新构建。\n"
            "$ErrorActionPreference = 'Stop'\n"
            "$kit = $PSScriptRoot\n"
            "& cmake -S \"$kit/source/src\" -B \"$kit/rebuild\" -G '" + generator + "' -A x64 "
            "-DBUILD_TESTING=OFF -DFD2SDL_USE_SYSTEM_SDL3=ON "
            '"-DSDL3_DIR=$kit/sdk/sdl/' + config_dir + '"\n'
            "if ($LASTEXITCODE) { exit $LASTEXITCODE }\n"
            "& cmake --build \"$kit/rebuild\" --config Release --target fd2sdl -- /nodeReuse:false\n"
            "exit $LASTEXITCODE\n").encode("utf-8-sig")


def _root_notices(data: bytes) -> bytes:
    # source中的通知保持原字节；这里只改包根通知中这类相对Markdown链接。
    text = data.decode("utf-8")
    return re.sub(r"(\]\(\s*<?)(?:\./)?src/third_party/", r"\1source/src/third_party/", text).encode("utf-8")


def _player_readme() -> bytes:
    resources = "\n".join(f"- `original_game/{name}`" for name in DOCUMENTED_RESOURCE_FILES)
    return ("# Windows Release：启动与重建\n\n"
            "## 启动\n\n"
            "运行前需要Windows x64及输入清单声明的官方Microsoft Visual C++ x64 Redistributable。"
            "普通运行不需要Visual Studio或Windows SDK。\n\n"
            "把合法取得的资源放到包根original_game目录，再双击play.cmd，或从PowerShell运行 "
            "`./play.cmd`。启动器临时切换到包根、运行bin/fd2sdl.exe、转交参数并保留退出码，"
            "之后恢复原工作目录；不修改系统策略或音频环境。\n\n"
            "例如：`./play.cmd --new-game-play`。直接运行bin/fd2sdl.exe时也须以包根为工作目录。"
            "程序不会下载原版资源，不需要FD2.EXE或预置SAV。默认Save写包根FD2.SAV；验证旧档请用副本。\n\n"
            "当前完整受支持流程使用以下12个资源（各入口实际检查的子集不同）：\n\n" + resources +
            "\n\n本包不包含这些资源、SAV、截图、录音或dump。\n\n"
            "## 重建与许可\n\n自有代码使用MIT；第三方许可见THIRD_PARTY_NOTICES.md及source/src/third_party。"
            "完整App构建源码在source，实际SDL开发SDK在sdk/sdl。另安装清单记录的Visual Studio C/C++工具链、"
            "Windows SDK和CMake后，可替换Nuked源码，再执行`./rebuild.ps1`；无需下载SDL。\n\n"
            "source/README.md保留仓库原文，但本包是App-only构建材料；必须使用BUILD_TESTING=OFF，"
            "不能照仓库默认测试命令构建未随包提供的tests。rebuild.ps1已设置该选项。\n\n"
            "打包器不运行构建或游戏；离线重建、替换库重链接与发布验收须另留实际收据。"
            "材料清单不代表这些验证已通过，不对外自动发布。\n").encode("utf-8")


def package(source_root: Path, manifest_path: Path, output: Path) -> dict:
    root = source_root.resolve(strict=True)
    raw = _read(manifest_path)
    manifest = json.loads(raw)
    if manifest.get("schema") != SCHEMA or manifest.get("configuration") != "Release" or manifest.get("architecture") != "x64":
        raise ValueError("actual Windows x64 Release manifest required")
    sources = source_inventory(root)
    _check_inventory(sources, manifest.get("source_files"), "source")
    sdl = manifest["sdl"]
    if not sdl.get("target") or sdl.get("type") not in {"STATIC_LIBRARY", "SHARED_LIBRARY"}:
        raise ValueError("actual CMake SDL target/type required")
    sdk = Path(sdl["sdk_root"]).resolve(strict=True)
    sdk_files = sdk_inventory(sdk)
    _check_inventory(sdk_files, sdl.get("sdk_files"), "SDL SDK")
    config_files = [row["path"] for row in sdk_files if Path(row["path"]).name == "SDL3Config.cmake"]
    if len(config_files) != 1:
        raise ValueError("one relocatable SDL CMake package required")
    licenses = sdl.get("license_files", [])
    if not licenses or any(_relative(name) not in {row["path"] for row in sdk_files} for name in licenses):
        raise ValueError("actual SDL and bundled-component license files required")
    for key in ("target_file", "link_file"):
        _, data = _external(sdl[key])
        if sha256(data) not in {row["sha256"] for row in sdk_files}:
            raise ValueError("installed SDK differs from the SDL binary actually linked")
    forbidden = [str(root), str(sdk)] + sdl.get("forbidden_prefixes", [])
    for row in sdk_files:
        if row["path"].endswith(".cmake"):
            text = _read(sdk / row["path"]).decode("utf-8-sig").replace("\\", "/").casefold()
            if any(prefix and prefix.replace("\\", "/").casefold() in text for prefix in forbidden):
                raise ValueError("build-tree/absolute SDL CMake reference; install a relocatable SDK")
    if manifest.get("unresolved_dependencies") or any(key not in manifest for key in (
            "runtime_dependencies", "system_dependencies", "unresolved_dependencies")):
        raise ValueError("complete actual runtime-dependency scan required")
    if not manifest.get("prerequisites"):
        raise ValueError("record Windows/VC++ runtime prerequisites")
    exe_path, exe = _external(manifest["executable"])
    _pe_x64(exe)
    payload = {"bin/fd2sdl.exe": exe}
    expected_dll = sdl["target_file"] if sdl["type"] == "SHARED_LIBRARY" else None
    dependencies = manifest["runtime_dependencies"]
    if len(dependencies) != (1 if expected_dll else 0) or (expected_dll and any(
            row["sha256"] != expected_dll["sha256"] or
            Path(row["path"]).name.casefold() != Path(expected_dll["path"]).name.casefold()
            for row in dependencies)):
        raise ValueError("this package supports only the actual typed SDL DLL; unknown non-system dependencies are refused")
    for row in dependencies:
        path, data = _external(row)
        if path.suffix.lower() != ".dll" or any(part.casefold() in {"system32", "syswow64", "winsxs"} for part in path.parts):
            raise ValueError("only resolved non-system DLLs belong in the runtime package")
        _pe_x64(data)
        name = "bin/" + path.name
        if name.casefold() in {key.casefold() for key in payload}:
            raise ValueError("duplicate runtime dependency basename")
        payload[name] = data
    if sdl["type"] == "SHARED_LIBRARY" and sdl["target_file"]["sha256"] not in {
            sha256(data) for name, data in payload.items() if name.lower().endswith(".dll")}:
        raise ValueError("actual shared SDL DLL is missing from runtime dependencies")
    for row in sources:
        payload["source/" + row["path"]] = _read(root / row["path"], row["sha256"])
    for row in sdk_files:
        payload["sdk/sdl/" + row["path"]] = _read(sdk / row["path"], row["sha256"])
    payload["LICENSE"] = _read(root / "LICENSE")
    notices = next(row for row in sources if Path(row["path"]).name.startswith("THIRD_PARTY_NOTICES"))
    payload["THIRD_PARTY_NOTICES.md"] = _root_notices(_read(root / notices["path"]))
    payload["rebuild.ps1"] = _rebuild(str(PurePosixPath(config_files[0]).parent), manifest["toolchain"]["generator"])
    payload["play.cmd"] = ("@echo off\r\nsetlocal\r\npushd \"%~dp0\"\r\n"
                           "if errorlevel 1 exit /b 1\r\n\"bin\\fd2sdl.exe\" %*\r\n"
                           "set \"fd2_exit=%errorlevel%\"\r\npopd\r\n"
                           "endlocal & exit /b %fd2_exit%\r\n").encode("ascii")
    payload["README-REBUILD.md"] = _player_readme()
    _check_inventory(source_inventory(root), sources, "source after read")
    _check_inventory(sdk_inventory(sdk), sdk_files, "SDK after read")
    if _read(manifest_path) != raw or _read(exe_path, manifest["executable"]["sha256"]) != exe:
        raise ValueError("release inputs changed")
    parent = output.parent.resolve(strict=True)
    destination = parent / output.name
    original = (root / "original_game").resolve()
    if destination.exists() or _inside(destination, original) or _inside(destination, sdk):
        raise ValueError("output must be new and outside actual original/SDK directories")
    destination.mkdir()
    for name, data in payload.items():
        path = destination / name
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("xb") as stream:
            stream.write(data)
    result = {"schema": "fd2-windows-release-package-v1", "status": "local_package_created",
              "input_manifest_sha256": sha256(raw), "project_license": "MIT (own code only)",
              "inputs": manifest, "files": [{"path": name, "sha256": sha256(data), "size": len(data)}
                                              for name, data in sorted(payload.items())],
              "game_resources_included": False, "offline_rebuild_verified_by_packager": False,
              "published": False}
    with (destination / "PACKAGE-MANIFEST.json").open("x", encoding="utf-8") as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    return result


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        result = package(args.source_root, args.inputs, args.output)
    except (OSError, ValueError, KeyError, TypeError) as error:
        print(json.dumps({"ok": False, "error": str(error)}, ensure_ascii=False))
        return 1
    print(json.dumps({"ok": True, "status": result["status"], "files": len(result["files"]),
                      "offline_rebuild_verified_by_packager": False, "published": False}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
