"""G1 共享尾预检纯测；仅执行真实预检函数，不导入 IDA 或保存原指令。"""
import ast
from dataclasses import dataclass
import hashlib
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import Mock


SCRIPT = Path(__file__).resolve().parents[1] / "ida/ida_fd2_apply_registry_names.py"
EXPECTED_SHA256 = "cf7734a4bca09f70340a01c1b01bfc9ae3a80002b7f58c8f1aca7329dffc5ab0"
# 此字节串只表示假数据库中的身份；不是 FD2 指令或原版语义证据。
SYNTHETIC_BYTES = (b"synthetic shared tail identity;" * 2)[:60].ljust(60, b"!")


def load_preflight():
    """AST 只抽取被测函数；顶层 idapro/ida_domain 导入及 main 均不执行。"""
    source = ast.parse(SCRIPT.read_text(encoding="utf-8"), filename=str(SCRIPT))
    function = next(node for node in source.body
                    if isinstance(node, ast.FunctionDef) and node.name == "confirmed_shared_tail")
    namespace = {"hashlib": SimpleNamespace(sha256=Mock(side_effect=lambda raw:
        SimpleNamespace(hexdigest=lambda: EXPECTED_SHA256)
        if raw == SYNTHETIC_BYTES else hashlib.sha256(raw)))}
    module = ast.Module(body=[function], type_ignores=[])
    exec(compile(module, str(SCRIPT), "exec"), namespace)
    return namespace["confirmed_shared_tail"], namespace["hashlib"].sha256


@dataclass(frozen=True)
class Span:
    start_ea: int
    end_ea: int


class SharedTailPreflightTest(unittest.TestCase):
    def setUp(self):
        self.check, self.sha256 = load_preflight()
        self.entry = Span(0x34F38, 0x34F3D)
        self.owner = Span(0x350BE, 0x35120)
        self.tail = Span(0x34F3D, 0x34F74)
        self.functions = {0x34F38: self.entry, 0x350BE: self.owner}
        self.chunks = {self.entry: (self.entry,), self.owner: (self.owner, self.tail)}
        self.raw = SYNTHETIC_BYTES
        self.db = SimpleNamespace(functions=SimpleNamespace(
            get_at=Mock(side_effect=lambda va: self.functions.get(va)),
            get_chunks=Mock(side_effect=lambda function: iter(self.chunks[function]))),
            bytes=SimpleNamespace(get_bytes_at=Mock(side_effect=lambda va, *, size: self.raw)))

    def test_confirmed_tail_returns_all_original_owner_chunks_without_mutation(self):
        before = dict(self.chunks)
        entry, owner, original, present = self.check(self.db)
        self.assertIs(entry, self.entry)
        self.assertIs(owner, self.owner)
        self.assertEqual(original, [(0x350BE, 0x35120), (0x34F3D, 0x34F74)])
        self.assertFalse(present)
        self.assertEqual(self.chunks, before)
        self.db.bytes.get_bytes_at.assert_called_once_with(0x34F38, size=60)
        self.sha256.assert_called_once_with(SYNTHETIC_BYTES)

    def test_missing_or_changed_entry_boundaries_fail_before_reading_bytes(self):
        for entry in (None, Span(0x34F37, 0x34F3D), Span(0x34F38, 0x34F3C),
                      Span(0x34F38, 0x34F74)):
            with self.subTest(entry=entry):
                self.functions[0x34F38] = entry
                with self.assertRaisesRegex(ValueError, "入口/边界"):
                    self.check(self.db)
        self.db.bytes.get_bytes_at.assert_not_called()
        self.db.functions.get_chunks.assert_not_called()

    def test_missing_or_different_original_owner_fails_before_reading_bytes(self):
        for owner in (None, Span(0x350BD, 0x35120), Span(0x34F38, 0x34F3D)):
            with self.subTest(owner=owner):
                self.functions[0x350BE] = owner
                with self.assertRaisesRegex(ValueError, "入口/边界"):
                    self.check(self.db)
        self.db.bytes.get_bytes_at.assert_not_called()
        self.db.functions.get_chunks.assert_not_called()

    def test_changed_truncated_or_extended_instruction_identity_is_rejected(self):
        for raw in (b"", SYNTHETIC_BYTES[:-1], SYNTHETIC_BYTES + b"!",
                    b"?" + SYNTHETIC_BYTES[1:]):
            with self.subTest(length=len(raw)):
                self.raw = raw
                with self.assertRaisesRegex(ValueError, "指令身份"):
                    self.check(self.db)
        self.db.functions.get_chunks.assert_not_called()

    def test_owner_must_contain_the_exact_tail_span(self):
        for tail in (None, Span(0x34F3C, 0x34F74), Span(0x34F3D, 0x34F73),
                     Span(0x34F3D, 0x34F75)):
            with self.subTest(tail=tail):
                self.chunks[self.owner] = (self.owner,) + ((tail,) if tail else ())
                before = dict(self.chunks)
                with self.assertRaisesRegex(ValueError, "原owner关系"):
                    self.check(self.db)
                self.assertEqual(self.chunks, before)

    def test_already_shared_tail_is_idempotent_and_preserves_original_owner(self):
        first = self.check(self.db)
        self.chunks[self.entry] += (self.tail,)  # 模拟 SDK 成功新增引用后的数据库视图。
        before = dict(self.chunks)
        second = self.check(self.db)
        third = self.check(self.db)
        self.assertFalse(first[3])
        self.assertTrue(second[3])
        self.assertEqual(second, third)
        self.assertEqual(second[2], first[2])
        self.assertEqual(self.chunks, before)
        self.assertEqual(self.chunks[self.owner], (self.owner, self.tail))

    def test_target_only_has_shared_tail_does_not_replace_the_owner_requirement(self):
        self.chunks[self.entry] += (self.tail,)
        self.chunks[self.owner] = (self.owner,)
        with self.assertRaisesRegex(ValueError, "原owner关系"):
            self.check(self.db)

    def test_owner_snapshot_remains_independent_of_later_chunk_view(self):
        original = self.check(self.db)[2]
        self.chunks[self.owner] = (self.owner,)
        self.assertEqual(original, [(0x350BE, 0x35120), (0x34F3D, 0x34F74)])
        with self.assertRaisesRegex(ValueError, "原owner关系"):
            self.check(self.db)


if __name__ == "__main__":
    unittest.main()
