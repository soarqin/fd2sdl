"""实际输入producer的纯回归；合成PE/SDK，不构建、启动程序或调用Windows API。"""
import copy
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import package_windows_release as package
import record_windows_release as record


def pe(tag=b"App"):
    data = bytearray(256)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 60, 128)
    data[128:132] = b"PE\0\0"
    struct.pack_into("<H", data, 132, 0x8664)
    struct.pack_into("<H", data, 152, 0x20B)
    data[200:200 + len(tag)] = tag
    return bytes(data)


class ReleaseRecorderTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="fd2-release-record-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "source"
        self.sdk = self.root / "sdk"
        self.windows = self.root / "Windows"
        for name in package.REQUIRED_SOURCE:
            self.put(self.source / name, b"source fixture\n")
        self.put(self.source / "LICENSE", b"MIT License\nfixture only\n")
        self.put(self.source / "THIRD_PARTY_NOTICES.md", b"fixture notices\n")
        self.put(self.sdk / "include/SDL3/SDL.h", b"header fixture\n")
        self.put(self.sdk / "lib/SDL3-static.lib", b"!<arch>\nstatic fixture\n")
        self.put(self.sdk / "lib/cmake/SDL3/SDL3Config.cmake", b"# relocatable fixture\n")
        self.put(self.sdk / "licenses/SDL3-LICENSE.txt", b"SDL license fixture\n")
        self.exe = self.root / "build/Release/fd2sdl.exe"
        self.put(self.exe, pe())
        self.kernel = self.windows / "System32/KERNEL32.dll"
        self.put(self.kernel, pe(b"system"))
        self.ucrt = self.windows / "System32/ucrtbase.dll"
        self.put(self.ucrt, pe(b"UCRT"))
        self.vcruntime = self.root / "build/Release/VCRUNTIME140.dll"
        self.put(self.vcruntime, pe(b"VC runtime"))
        self.snapshot_file = self.root / "before-build.json"
        self.target_file = self.root / "release-target-Release.json"
        self.scan_file = self.root / "runtime-dependencies.json"
        self.output = self.root / "actual-release-inputs.json"
        self.target = {"schema": record.TARGET_SCHEMA, "configuration": "Release",
                       "architecture": "x64", "pointer_size": 8, "executable": str(self.exe),
                       "source_root": str(self.source), "sdl_target": "SDL3::SDL3-static",
                       "sdl_type": "STATIC_LIBRARY", "sdl_target_file": str(self.sdk / "lib/SDL3-static.lib"),
                       "sdl_link_file": str(self.sdk / "lib/SDL3-static.lib"),
                       "sdl_package": str(self.sdk / "lib/cmake/SDL3"),
                       "compiler": str(self.root / "toolchain/cl.exe"), "compiler_version": "fixture",
                       "generator": "Visual Studio 18 2026"}
        self.scan = {"schema": record.DEPENDENCIES_SCHEMA,
                     "resolved": [str(self.vcruntime)], "unresolved": [],
                     "executable": str(self.exe), "executable_sha256": package.sha256(self.exe.read_bytes()),
                     "direct_imports": ["KERNEL32.dll", "VCRUNTIME140.dll", "api-ms-win-crt-runtime-l1-1-0.dll"],
                     "system_directory": str(self.windows / "System32"),
                     "api_supported": [f"api-ms-win-crt-{name}-l1-1-0.dll" for name in
                         ("stdio", "heap", "runtime", "convert", "environment", "string", "math", "locale")],
                     "api_set_contracts": ["api-ms-win-crt-runtime-l1-1-0.dll"],
                     "backing_runtime": str(self.ucrt)}
        record.snapshot(self.source, self.snapshot_file)

    @staticmethod
    def put(path, data):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def run_record(self, output=None):
        self.target_file.write_text(json.dumps(self.target), encoding="utf-8")
        self.scan_file.write_text(json.dumps(self.scan), encoding="utf-8")
        return record.record_inputs(self.snapshot_file, self.target_file, self.sdk,
                                    self.scan_file, output or self.output, self.windows)

    def rejected(self, output=None):
        with self.assertRaises((ValueError, OSError, KeyError, TypeError)):
            self.run_record(output)
        if output is None:
            self.assertFalse(self.output.exists())

    def test_snapshot_uses_exact_source_inventory_and_does_not_include_resources(self):
        self.put(self.source / "original_game/FDOTHER.DAT", b"must not be read")
        snapshot = json.loads(self.snapshot_file.read_text(encoding="utf-8"))
        self.assertEqual(snapshot["source_files"], package.source_inventory(self.source))
        self.assertFalse(snapshot["build_executed_by_recorder"])
        self.assertFalse(any("original_game" in row["path"] for row in snapshot["source_files"]))

    def test_actual_static_record_is_consumable_by_packager_without_system_dll_copy(self):
        result = self.run_record()
        self.assertEqual(result["schema"], package.SCHEMA)
        self.assertEqual(result["system_dependencies"], self.scan["direct_imports"])
        self.assertEqual(result["redistributable_dependencies"], ["VCRUNTIME140.dll"])
        self.assertEqual(result["external_redistributable_paths"], [str(self.vcruntime.resolve())])
        self.assertEqual(result["runtime_scan"]["system_directory"], str((self.windows / "System32").resolve()))
        self.assertEqual(result["runtime_dependencies"], [])
        self.assertTrue(result["source_unchanged_since_snapshot"])
        self.assertFalse(result["build_executed_by_recorder"])
        self.assertTrue(any("Visual C++ x64" in item for item in result["prerequisites"]))
        packed = package.package(self.source, self.output, self.root / "package")
        names = {row["path"] for row in packed["files"]}
        self.assertEqual({name for name in names if name.startswith("bin/")}, {"bin/fd2sdl.exe"})

    def test_reject_changed_added_or_removed_source_since_snapshot(self):
        cases = ("changed", "added", "removed")
        for case in cases:
            with self.subTest(case=case):
                main = self.source / "src/main.c"
                old = main.read_bytes()
                extra = self.source / "src/new_runtime.c"
                if case == "changed":
                    main.write_bytes(old + b"changed\n")
                elif case == "added":
                    extra.write_bytes(b"new runtime source\n")
                else:
                    main.unlink()
                self.rejected()
                main.write_bytes(old)
                if extra.exists():
                    extra.unlink()

    def test_reject_wrong_configuration_architecture_pointer_size_or_target_type(self):
        for key, value in (("configuration", "Debug"), ("architecture", "Win32"),
                           ("pointer_size", 4), ("sdl_type", "INTERFACE_LIBRARY")):
            with self.subTest(key=key):
                old = self.target[key]
                self.target[key] = value
                self.rejected()
                self.target[key] = old

    def test_reject_mismatched_source_root_or_noninstalled_sdk_package(self):
        other = self.root / "other"
        other.mkdir()
        for key, value in (("source_root", str(other)), ("sdl_package", str(other))):
            with self.subTest(key=key):
                old = self.target[key]
                self.target[key] = value
                self.rejected()
                self.target[key] = old

    def test_reject_actual_link_binary_not_in_sdk(self):
        foreign = self.root / "actual/SDL3-static.lib"
        self.put(foreign, b"different actual linked library")
        self.target["sdl_target_file"] = self.target["sdl_link_file"] = str(foreign)
        self.rejected()

    def test_shared_records_only_the_actual_matching_sdl_dll(self):
        dll = self.sdk / "bin/SDL3.dll"
        library = self.sdk / "lib/SDL3.lib"
        self.put(dll, pe(b"SDL"))
        self.put(library, b"!<arch>\nimport fixture\n")
        self.target.update(sdl_target="SDL3::SDL3-shared", sdl_type="SHARED_LIBRARY",
                           sdl_target_file=str(dll), sdl_link_file=str(library))
        self.scan["resolved"].append(str(dll))
        self.scan["direct_imports"].append("SDL3.dll")
        result = self.run_record()
        self.assertEqual(result["runtime_dependencies"], [{"path": str(dll.resolve()),
                          "sha256": package.sha256(dll.read_bytes())}])
        self.assertNotIn("SDL3.dll", result["system_dependencies"])

    def test_reject_missing_shared_dll_or_unapproved_non_system_dependency(self):
        dll = self.sdk / "bin/SDL3.dll"
        self.put(dll, pe(b"SDL"))
        self.target.update(sdl_type="SHARED_LIBRARY", sdl_target_file=str(dll))
        self.rejected()
        unexpected = self.root / "actual/unrelated.dll"
        self.put(unexpected, pe(b"unrelated"))
        self.scan["resolved"].append(str(unexpected))
        self.rejected()

    def test_reject_unresolved_malformed_duplicate_or_debug_runtime_scan(self):
        original = copy.deepcopy(self.scan)
        debug = self.windows / "System32/VCRUNTIME140D.dll"
        self.put(debug, pe(b"debug"))
        cases = [dict(original, unresolved=["missing.dll"]), dict(original, resolved="not an array"),
                 dict(original, resolved=[str(self.kernel), str(self.kernel)]),
                 dict(original, resolved=[str(debug)]), dict(original, direct_imports=[]),
                 dict(original, direct_imports=["KERNEL32.dll", "kernel32.DLL"]),
                 dict(original, system_directory=str(self.root))]
        for scan in cases:
            with self.subTest(scan=scan):
                self.scan = scan
                self.rejected()

    def test_reject_bad_executable_before_output(self):
        self.exe.write_bytes(b"not a PE")
        self.rejected()

    def test_reject_dependency_scan_bound_to_another_or_old_executable(self):
        other = self.root / "old/fd2sdl.exe"
        self.put(other, pe(b"old"))
        self.scan["executable"] = str(other)
        self.scan["executable_sha256"] = package.sha256(other.read_bytes())
        self.rejected()
        self.scan["executable"] = str(self.exe)
        self.rejected()

    def test_no_overwrite_and_real_parent_and_original_directory_guards(self):
        self.output.write_bytes(b"retain existing output")
        with self.assertRaises(ValueError):
            self.run_record()
        self.assertEqual(self.output.read_bytes(), b"retain existing output")
        with self.assertRaises(ValueError):
            record.snapshot(self.source, self.snapshot_file)
        with self.assertRaises(OSError):
            record.snapshot(self.source, self.root / "missing-parent/snapshot.json")
        forbidden = self.source / "original_game"
        forbidden.mkdir()
        with mock.patch.object(package, "source_inventory", side_effect=AssertionError("must reject before source read")):
            with self.assertRaises(ValueError):
                record.snapshot(self.source, forbidden / "snapshot.json")

    def original_junction_resolution(self):
        """只替代junction那次系统解析；其余路径/文件I/O仍走真实临时目录。"""
        actual = self.root / "FD2-assets"
        actual.mkdir()
        alias = self.source / "original_game"
        original_resolve = Path.resolve
        def resolve(path, *args, **kwargs):
            return actual if path == alias else original_resolve(path, *args, **kwargs)
        return actual, mock.patch.object(Path, "resolve", autospec=True, side_effect=resolve)

    def test_actual_original_junction_target_rejects_both_outputs_before_inventory(self):
        actual, resolution = self.original_junction_resolution()
        sentinel = actual / "FDOTHER.DAT"
        sentinel.write_bytes(b"read-only resource fixture")
        with resolution, mock.patch.object(package, "source_inventory", side_effect=AssertionError("must reject before source read")):
            with self.assertRaises(ValueError):
                record.snapshot(self.source, actual / "source-snapshot.json")
            with self.assertRaises(ValueError):
                self.run_record(actual / "release-inputs.json")
        self.assertEqual(sorted(path.name for path in actual.iterdir()), ["FDOTHER.DAT"])
        self.assertEqual(sentinel.read_bytes(), b"read-only resource fixture")

    def test_actual_original_junction_target_is_not_a_metadata_source(self):
        actual, resolution = self.original_junction_resolution()
        snapshot = actual / "snapshot.json"
        target = actual / "target.json"
        scan = actual / "scan.json"
        snapshot.write_bytes(self.snapshot_file.read_bytes())
        target.write_text(json.dumps(self.target), encoding="utf-8")
        scan.write_text(json.dumps(self.scan), encoding="utf-8")
        self.target_file.write_text(json.dumps(self.target), encoding="utf-8")
        self.scan_file.write_text(json.dumps(self.scan), encoding="utf-8")
        before = {p.name: p.read_bytes() for p in actual.iterdir()}
        cases = ((snapshot, self.target_file, self.scan_file),
                 (self.snapshot_file, target, self.scan_file),
                 (self.snapshot_file, self.target_file, scan))
        with resolution:
            for inputs in cases:
                with self.subTest(metadata=inputs), self.assertRaisesRegex(ValueError, "actual original resource"):
                    record.record_inputs(*inputs[:2], self.sdk, inputs[2], self.output, self.windows)
                self.assertFalse(self.output.exists())
        self.assertEqual({p.name: p.read_bytes() for p in actual.iterdir()}, before)

    def test_windows_temp_is_not_a_system_dependency(self):
        vendor = self.windows / "Temp/vendor.dll"
        self.put(vendor, pe(b"not a system component"))
        self.scan["resolved"].append(str(vendor))
        self.scan["direct_imports"].append("vendor.dll")
        self.rejected()

    def test_direct_imports_and_virtual_contracts_need_actual_backing(self):
        original = copy.deepcopy(self.scan)
        for name in ("unresolved-vendor.dll", "api-ms-win-crt-filesystem-l1-1-0.dll"):
            with self.subTest(name=name):
                self.scan = copy.deepcopy(original)
                self.scan["direct_imports"].append(name)
                self.rejected()
        for update in ({"api_set_contracts": []},
                       {"api_supported": original["api_supported"] + ["api-ms-win-crt-filesystem-l1-1-0.dll"]},
                       {"backing_runtime": str(self.kernel)}):
            with self.subTest(update=update):
                self.scan = dict(original, **update)
                self.rejected()
        self.scan = original
        self.ucrt.unlink()
        self.rejected()

    def test_cli_snapshot_and_inputs_use_the_same_contract(self):
        snapshot_path = self.root / "cli-snapshot.json"
        self.assertEqual(record.main(["snapshot", "--source-root", str(self.source),
                                      "--output", str(snapshot_path)]), 0)
        self.target_file.write_text(json.dumps(self.target), encoding="utf-8")
        self.scan_file.write_text(json.dumps(self.scan), encoding="utf-8")
        with mock.patch.dict(record.os.environ, {"SystemRoot": str(self.windows)}):
            result = record.main(["inputs", "--snapshot", str(snapshot_path),
                                  "--target", str(self.target_file), "--sdk-root", str(self.sdk),
                                  "--dependencies", str(self.scan_file), "--output", str(self.output)])
        self.assertEqual(result, 0)
        self.assertEqual(json.loads(self.output.read_text(encoding="utf-8"))["schema"], package.SCHEMA)


if __name__ == "__main__":
    unittest.main()
