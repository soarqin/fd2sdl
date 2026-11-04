"""原版目录经junction/符号链接访问时，必须先解析目标再允许写测试产物。"""
import importlib
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))


class OriginalOutputPathTest(unittest.TestCase):
    def check_destination(self, inside):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            candidate = root / "candidate"
            original = root / "evidence"
            candidate.mkdir()
            original.mkdir()
            output = original / "new-artifacts" if inside else root / "safe-artifacts"
            actual_resolve = Path.resolve
            def resolve(path, *args, **kwargs):
                if path == candidate / "original_game":
                    return original
                return actual_resolve(path, *args, **kwargs)
            for script in sorted(TOOLS.glob("run_original_*.py")):
                module = importlib.import_module(script.stem)
                args = SimpleNamespace(root=candidate, artifacts=output,
                                       save_fixture=original / "START.SAV")
                with self.subTest(script=script.name, inside=inside), \
                        patch.object(Path, "resolve", resolve), \
                        patch.object(Path, "mkdir", side_effect=RuntimeError("safe mkdir reached")) as mkdir:
                    if inside:
                        with self.assertRaisesRegex(ValueError, "original_game"):
                            module.run(args)
                        mkdir.assert_not_called()
                    else:
                        with self.assertRaisesRegex(RuntimeError, "safe mkdir reached"):
                            module.run(args)
                        mkdir.assert_called_once_with(parents=True, exist_ok=False)
                self.assertFalse(output.exists())

    def test_resolved_evidence_child_is_rejected_before_mkdir_for_every_runner(self):
        self.check_destination(True)

    def test_external_artifact_path_still_reaches_mkdir_for_every_runner(self):
        self.check_destination(False)


if __name__ == "__main__":
    unittest.main()
