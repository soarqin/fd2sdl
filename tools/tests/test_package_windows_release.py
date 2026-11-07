"""R1白名单/来源/重链接材料回归；只用合成PE与临时源码，不启动程序。"""
import json
import os
import re
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import package_windows_release as release


def pe(tag=b"app"):
    data = bytearray(256)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 60, 128)
    data[128:132] = b"PE\0\0"
    struct.pack_into("<H", data, 132, 0x8664)
    struct.pack_into("<H", data, 152, 0x20B)
    data[200:200 + len(tag)] = tag
    return bytes(data)


class ReleasePackageTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="fd2-release-unit-")
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name)
        self.source = self.base / "source"
        self.sdk = self.base / "sdk"
        for name in release.REQUIRED_SOURCE:
            self.put(self.source / name, b"test source\n")
        self.put(self.source / "LICENSE", b"MIT License\nfixture only\n")
        self.put(self.source / "THIRD_PARTY_NOTICES.md", b"Preserve third-party licenses\n")
        self.put(self.source / "src/third_party/LICENSE.opl-models-mit", b"MIT component fixture\n")
        self.put(self.sdk / "include/SDL3/SDL.h", b"SDL header fixture\n")
        self.put(self.sdk / "lib/SDL3-static.lib", b"!<arch>\nrelease static fixture")
        self.put(self.sdk / "lib/cmake/SDL3/SDL3Config.cmake", b"# relocatable package fixture\n")
        self.put(self.sdk / "licenses/SDL3-LICENSE.txt", b"SDL zlib license fixture\n")
        self.executable = self.base / "actual/Release/fd2sdl.exe"
        self.put(self.executable, pe())
        self.linked = self.base / "actual/SDL3-static.lib"
        self.put(self.linked, (self.sdk / "lib/SDL3-static.lib").read_bytes())
        self.inputs = self.base / "release-inputs.json"
        self.output = self.base / "package"
        self.manifest = {
            "schema": release.SCHEMA, "configuration": "Release", "architecture": "x64",
            "source_files": release.source_inventory(self.source),
            "executable": self.external(self.executable),
            "sdl": {"target": "SDL3::SDL3-static", "type": "STATIC_LIBRARY",
                    "target_file": self.external(self.linked), "link_file": self.external(self.linked),
                    "sdk_root": str(self.sdk), "sdk_files": release.sdk_inventory(self.sdk),
                    "license_files": ["licenses/SDL3-LICENSE.txt"]},
            "runtime_dependencies": [], "system_dependencies": ["KERNEL32.dll", "VCRUNTIME140.dll"],
            "unresolved_dependencies": [],
            "prerequisites": ["Windows x64 and official Visual C++ x64 Redistributable"],
            "toolchain": {"generator": "Visual Studio 18 2026"},
        }

    @staticmethod
    def put(path, data):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    @staticmethod
    def external(path):
        return {"path": str(path), "sha256": release.sha256(path.read_bytes())}

    def run_package(self, manifest=None, output=None):
        self.inputs.write_text(json.dumps(manifest or self.manifest), encoding="utf-8")
        return release.package(self.source, self.inputs, output or self.output)

    def rejected(self, manifest=None, output=None):
        with self.assertRaises((ValueError, OSError)):
            self.run_package(manifest, output)
        if output is None:
            self.assertFalse(self.output.exists())

    def test_static_package_has_real_rebuild_material_not_game_or_guessed_dll(self):
        self.put(self.source / "original_game/FD2.SAV", b"private save")
        self.put(self.source / "original_game/FDOTHER.DAT", b"private resource")
        self.put(self.source / "src/Release/SDL3.dll", pe(b"unrelated DLL"))
        result = self.run_package()
        paths = {row["path"] for row in result["files"]}
        self.assertIn("source/src/third_party/nukedopl3.c", paths)
        self.assertIn("source/src/third_party/LICENSE.nukedopl3", paths)
        self.assertIn("sdk/sdl/lib/SDL3-static.lib", paths)
        self.assertIn("sdk/sdl/include/SDL3/SDL.h", paths)
        self.assertNotIn("bin/SDL3.dll", paths)
        self.assertFalse(any("original_game" in path or path.endswith((".sav", ".dat")) for path in paths))
        rebuild = (self.output / "rebuild.ps1").read_text(encoding="utf-8-sig")
        self.assertIn("-DBUILD_TESTING=OFF", rebuild)
        self.assertIn("$kit/sdk/sdl/lib/cmake/SDL3", rebuild)
        self.assertFalse(result["offline_rebuild_verified_by_packager"])
        self.assertFalse(result["published"])
        self.assertEqual(result["inputs"]["sdl"]["type"], "STATIC_LIBRARY")
        launcher = (self.output / "play.cmd").read_text(encoding="ascii")
        self.assertIn('pushd "%~dp0"', launcher)
        self.assertIn('"bin\\fd2sdl.exe" %*', launcher)
        self.assertIn('set "fd2_exit=%errorlevel%"', launcher)
        self.assertLess(launcher.index('set "fd2_exit='), launcher.index("popd"))
        self.assertIn("endlocal & exit /b %fd2_exit%", launcher)
        readme = (self.output / "README-REBUILD.md").read_text(encoding="utf-8")
        self.assertIn("BUILD_TESTING=OFF", readme)
        self.assertEqual(set(re.findall(r"- `original_game/([^`]+)`", readme)), {
            "ANI.DAT", "BG.DAT", "DATO.DAT", "FDFIELD.DAT", "FDICON.B24", "FDMUS.DAT",
            "FDOTHER.DAT", "FDSHAP.DAT", "FDTXT.DAT", "FIGANI.DAT", "SAMPLE.AD", "TAI.DAT"})

    def test_root_notices_links_relocated_source_notice_bytes_preserved(self):
        original = (b"[MIT](LICENSE) [N](src/third_party/LICENSE.nukedopl3) "
                    b"[A](./src/third_party/LICENSE.opl-models-mit) "
                    b"[external](https://example.com/src/third_party/license)\n")
        self.put(self.source / "THIRD_PARTY_NOTICES.md", original)
        self.manifest["source_files"] = release.source_inventory(self.source)
        self.run_package()
        self.assertEqual((self.output / "source/THIRD_PARTY_NOTICES.md").read_bytes(), original)
        relocated = (self.output / "THIRD_PARTY_NOTICES.md").read_text(encoding="utf-8")
        self.assertIn("[N](source/src/third_party/LICENSE.nukedopl3)", relocated)
        self.assertIn("[A](source/src/third_party/LICENSE.opl-models-mit)", relocated)
        self.assertIn("[MIT](LICENSE)", relocated)
        self.assertIn("https://example.com/src/third_party/license", relocated)

    def test_source_whitelist_excludes_tests_build_decomp_and_capture(self):
        for name in ["src/tests/capture.png", "src/tests/local_test.c", "src/build-win/fake.c",
                     "docs/generated/ida-canonical/decomp.c", "src/capture.wav", "src/local.bin"]:
            self.put(self.source / name, b"not runtime source")
        names = {row["path"] for row in release.source_inventory(self.source)}
        self.assertTrue(release.REQUIRED_SOURCE <= names)
        self.assertFalse(any("tests/" in name or "build-win/" in name or "generated/" in name for name in names))

    def test_reject_debug_configuration(self):
        self.manifest["configuration"] = "Debug"
        self.rejected()

    def test_reject_changed_source_before_creating_output(self):
        self.put(self.source / "src/main.c", b"changed after Release build")
        self.rejected()

    def test_reject_resource_smuggled_into_source_manifest(self):
        self.manifest["source_files"].append({"path": "original_game/START.SAV", "sha256": "0" * 64})
        self.rejected()

    def test_reject_mismatched_installed_sdk(self):
        self.put(self.sdk / "lib/SDL3-static.lib", b"different SDK")
        self.manifest["sdl"]["sdk_files"] = release.sdk_inventory(self.sdk)
        self.rejected()

    def test_reject_absolute_build_tree_cmake_reference(self):
        self.put(self.sdk / "lib/cmake/SDL3/SDL3Config.cmake",
                 ('set(include "' + str(self.source) + '/include")').encode())
        self.manifest["sdl"]["sdk_files"] = release.sdk_inventory(self.sdk)
        self.rejected()

    def test_reject_unresolved_or_missing_dependency_scan(self):
        self.manifest["unresolved_dependencies"] = ["missing.dll"]
        self.rejected()
        del self.manifest["unresolved_dependencies"]
        self.rejected()

    def test_reject_copying_system_dll(self):
        path = self.base / "Windows/System32/vcruntime140.dll"
        self.put(path, pe(b"system"))
        self.manifest["runtime_dependencies"] = [self.external(path)]
        self.rejected()

    def test_static_rejects_unknown_non_system_dll(self):
        path = self.base / "private/plugin.dll"
        self.put(path, pe(b"unknown plugin"))
        self.manifest["runtime_dependencies"] = [self.external(path)]
        self.rejected()

    def test_shared_requires_actual_dll_and_preserves_target_identity(self):
        dll = self.sdk / "bin/SDL3.dll"
        self.put(dll, pe(b"SDL shared"))
        import_lib = self.sdk / "lib/SDL3.lib"
        self.put(import_lib, b"!<arch>\nshared import library fixture")
        self.manifest["sdl"].update(target="SDL3::SDL3-shared", type="SHARED_LIBRARY",
                                      target_file=self.external(dll), link_file=self.external(import_lib),
                                      sdk_files=release.sdk_inventory(self.sdk))
        self.rejected()
        wrong_bytes = self.base / "wrong/SDL3.dll"
        self.put(wrong_bytes, pe(b"not the linked SDL"))
        self.manifest["runtime_dependencies"] = [self.external(wrong_bytes)]
        self.rejected()
        wrong_name = self.base / "wrong/plugin.dll"
        self.put(wrong_name, dll.read_bytes())
        self.manifest["runtime_dependencies"] = [self.external(wrong_name)]
        self.rejected()
        self.manifest["runtime_dependencies"] = [self.external(dll)]
        result = self.run_package()
        self.assertIn("bin/SDL3.dll", {row["path"] for row in result["files"]})
        self.assertEqual(result["inputs"]["sdl"]["target"], "SDL3::SDL3-shared")

    def test_reject_actual_original_directory_alias(self):
        original = self.source / "original_game"
        original.mkdir()
        alias = self.base / "resource-alias"
        if os.name == "nt":
            subprocess.run(["cmd", "/c", "mklink", "/J", str(alias), str(original)],
                           check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        else:
            alias.symlink_to(original, target_is_directory=True)
        self.rejected(output=alias / "new-package")
        self.assertFalse((original / "new-package").exists())

    def test_existing_output_never_overwritten(self):
        self.output.mkdir()
        marker = self.output / "owned-marker"
        marker.write_bytes(b"keep")
        self.rejected(output=self.output)
        self.assertEqual(marker.read_bytes(), b"keep")

    def test_required_license_and_notices_are_not_optional(self):
        (self.source / "THIRD_PARTY_NOTICES.md").unlink()
        self.rejected()

    def test_windows_architecture_is_checked(self):
        data = bytearray(pe())
        struct.pack_into("<H", data, 132, 0x14C)
        self.put(self.executable, data)
        self.manifest["executable"] = self.external(self.executable)
        self.rejected()

    def test_relative_paths_and_script_metacharacters_are_rejected(self):
        for name in ["../unsafe", "C:/unsafe", "lib/$evil/SDL3Config.cmake", "lib/`evil/SDL3Config.cmake"]:
            with self.assertRaises(ValueError):
                release._relative(name)
        with self.assertRaises(ValueError):
            release._rebuild("lib/cmake/SDL3", "Visual Studio 18 2026'; unsafe")


if __name__ == "__main__":
    unittest.main()
