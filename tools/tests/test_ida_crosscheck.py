#!/usr/bin/env python3
"""IDA 复核产物的回归测试。

不依赖 idalib：只校验已发布到 `docs/generated/ida-canonical/` 的证据产物自洽，
以及 `src/` 注释里的函数地址已全部迁移到 canonical relbase VA。
"""

import hashlib
import json
import sys
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CANON = ROOT / "docs" / "generated" / "ida-canonical"
sys.path.insert(0, str(ROOT / "tools" / "ida"))

OBJ1_LO, OBJ1_HI = 0x10000, 0x4EF29


class IdaCrosscheckArtifactTest(unittest.TestCase):
    def test_published_checksums_cover_current_artifacts(self):
        names = set()
        for line in (CANON / "SHA256SUMS").read_text(encoding="utf-8").splitlines():
            digest, name = line.split("  ", 1)
            self.assertEqual(hashlib.sha256((CANON / name).read_bytes()).hexdigest(),
                             digest, msg=name)
            self.assertNotIn(name, names)
            names.add(name)
        self.assertEqual(names, {path.name for path in CANON.iterdir()
                                 if path.is_file() and path.name != "SHA256SUMS"})

    def test_checksums_only_needs_no_local_ida_exports(self):
        import build_fd2_name_registry as registry
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "b.bin").write_bytes(b"second")
            (root / "a.bin").write_bytes(b"first")
            (root / "SHA256SUMS").write_text("outdated")
            for content in (b"first", b"updated"):
                (root / "a.bin").write_bytes(content)
                with patch.object(sys, "argv", ["registry", "--checksums-only",
                                               "--publish", directory]):
                    self.assertEqual(registry.main(), 0)
                expected = (f"{hashlib.sha256(content).hexdigest()}  a.bin\n"
                            f"{hashlib.sha256(b'second').hexdigest()}  b.bin\n")
                self.assertEqual((root / "SHA256SUMS").read_text(), expected)

    def test_verify_report_all_checks_passed(self):
        data = json.loads((CANON / "fd2-verify.json").read_text(encoding="utf-8"))
        self.assertEqual(data["failed"], 0, msg=str(
            [c for c in data["checks"] if not c["ok"]]))
        self.assertGreaterEqual(data["passed"], 21)
        # 关键项必须在报告里出现，防止检查被悄悄删掉
        names = {c["check"] for c in data["checks"]}
        for required in ("relocation source 集合完全相同",
                         "每个 source 的 target linear 地址一致",
                         "object1 已初始化区与 relocated oracle 逐字节一致",
                         "反写 relocation 前值后与 raw oracle 一致（BSS 除外）"):
            self.assertIn(required, names)

    def test_program_export_matches_le_model(self):
        p = json.loads((CANON / "fd2-program.json").read_text(encoding="utf-8"))
        self.assertEqual(p["file_type_name"], "Linear Executable (LE)")
        self.assertIn("Watcom", p["compiler_name"])
        self.assertEqual(p["fixup_count"], 7948)
        self.assertEqual(p["fixup_type_histogram"], {"FIXUP_OFF32": 7948})
        self.assertEqual(p["start_ea"], 0x3CCB4)
        segs = {s["name"]: s for s in p["segments"]}
        self.assertEqual(segs["cseg01"]["start_ea"], 0x10000)
        self.assertEqual(segs["dseg02"]["start_ea"], 0x50000)
        self.assertEqual(segs["dseg03"]["start_ea"], 0x60000)
        for s in segs.values():
            self.assertEqual(s["bits"], 32)

    def test_name_audit_resolves_every_legacy_row(self):
        a = json.loads((CANON / "fd2-name-audit.json").read_text(encoding="utf-8"))
        self.assertEqual(sum(a["stats"].values()), len(a["rows"]))
        self.assertNotIn("unresolved", a["stats"])
        self.assertNotIn("interior-mismatch", a["stats"])
        for row in a["rows"]:
            self.assertIsNotNone(row["canonical_va"], msg=str(row["line_no"]))
            self.assertTrue(OBJ1_LO <= row["canonical_va"] < OBJ1_HI)
        # `__chkstk` body 行必须落在函数入口 +0xa
        for row in a["rows"]:
            if row["resolution"] == "chkstk-body":
                self.assertEqual(row["legacy_body_va"] - row["canonical_va"], 0xA)

    def test_registry_entries_are_real_ida_function_entries(self):
        import build_fd2_name_registry as registry
        reg = json.loads(
            (CANON / "fd2-ida-function-registry.json").read_text(encoding="utf-8"))
        import csv
        starts = set()
        with (CANON / "fd2-functions.tsv").open(encoding="utf-8") as f:
            for row in csv.DictReader(f, delimiter="\t"):
                starts.add(int(row["start_ea"], 16))
        self.assertEqual(len(starts), 1308)
        self.assertTrue(set(registry.CORRECTIONS) <= {e["va"] for e in reg["entries"]})
        for e in reg["entries"]:
            self.assertIn(e["va"], starts, msg=f"{e['va']:#x}")
            self.assertEqual(e["off"], e["va"] - OBJ1_LO)
            self.assertTrue(e["ida_name"])

    def test_hexrays_failures_are_documented(self):
        rep = json.loads(
            (CANON / "fd2-annotate-report.json").read_text(encoding="utf-8"))
        remaining = rep["prototypes"]["remaining_hexrays_failures"]
        self.assertLessEqual(len(remaining), 4)
        # 只允许手写汇编区（正 sp / AIL 中断处理）残留
        for f in remaining:
            self.assertTrue(
                "positive sp" in f["desc"] or int(f["func"], 16) >= 0x34F38,
                msg=str(f))
        # 修原型时不得修改任何机器码字节
        self.assertIn("不修改机器码", rep["prototypes"]["method"])


class SrcAddressMigrationTest(unittest.TestCase):
    def test_registry_corrections_have_unique_function_addresses(self):
        import ast
        source = ROOT / "tools/ida/build_fd2_name_registry.py"
        tree = ast.parse(source.read_text(encoding="utf-8"))
        value = next(node.value for node in tree.body
                     if isinstance(node, ast.AnnAssign) and
                     isinstance(node.target, ast.Name) and node.target.id == "CORRECTIONS")
        addresses = [ast.literal_eval(key) for key in value.keys]
        self.assertEqual(len(addresses), len(set(addresses)),
                         "同一规范函数的更正应合并到一个列表，重复dict键会静默覆盖证据")

    def test_build_candidates_and_third_party_are_outside_the_source_audit(self):
        import check_src_addresses as chk
        with tempfile.TemporaryDirectory() as directory:
            # 导出的真实源码本身位于build之下，只有src内部构建副本应排除。
            root = Path(directory) / "build" / "candidate" / "src"
            paths = ("main.c", "include/main.h", "support/build_options.c",
                     "build/cache.c", "build-win/candidate/font.c",
                     "nested/build-linux/cache.h", "third_party/vendor.c")
            for relative in paths:
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("/* @0x4edc0 candidate instruction */", encoding="utf-8")
            self.assertEqual({path.relative_to(root).as_posix() for path in chk.source_files(root)},
                             {"main.c", "include/main.h", "support/build_options.c"})

    def test_no_stale_legacy_addresses_left_in_src(self):
        import check_src_addresses as chk
        ranges, names, starts = chk.load_funcs()
        stale = []
        for path in chk.source_files(ROOT / "src"):
            for lineno, line in enumerate(
                    path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                if not chk.COMMENTISH.search(line):
                    continue
                for m in chk.TOKEN.finditer(line):
                    ea = int(m.group(1), 16)
                    i = __import__("bisect").bisect_right(starts, ea) - 1
                    ok = (OBJ1_LO <= ea < OBJ1_HI and i >= 0
                          and ranges[i][0] == ea)
                    if not ok:
                        stale.append(f"{path.relative_to(ROOT)}:{lineno} {m.group(1)}")
        self.assertEqual(stale, [], msg="发现未迁移的函数地址注释")


if __name__ == "__main__":
    unittest.main()
