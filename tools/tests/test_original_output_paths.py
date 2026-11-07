"""原版目录经junction/符号链接访问时，必须先解析目标再允许写测试产物。"""
import importlib
from contextlib import ExitStack
import hashlib
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))


class MkdirReached(Exception):
    pass


class OriginalOutputPathTest(unittest.TestCase):
    def check_destination(self, inside):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            candidate = root / "candidate"
            original = root / "evidence"
            candidate.mkdir()
            original.mkdir()
            source = b"read-only path-test fixture"
            for name in ("START.SAV", "FD2.SAV"):
                (original / name).write_bytes(source)
            source_report = original / "source-report.json"
            source_report.write_text("{}", encoding="utf-8")
            tool = candidate / "tool.exe"
            tool.write_bytes(b"not executable; process calls are forbidden in this test")
            output = original / "new-artifacts" if inside else root / "safe-artifacts"
            actual_resolve = Path.resolve
            def resolve(path, *args, **kwargs):
                if path == candidate / "original_game":
                    return original
                return actual_resolve(path, *args, **kwargs)
            for script in sorted(TOOLS.glob("run_original_*.py")):
                module = importlib.import_module(script.stem)
                args = SimpleNamespace(root=candidate, artifacts=output,
                                       save_fixture=original / "START.SAV", source_report=source_report, sdl=None,
                                       planner=tool, driver=tool, dosbox=tool, max_turns=50,
                                       record_native_audio=False, original_evidence=None,
                                       scenario={"run_original_item_list": "controls",
                                                 "run_original_battle_animation": "physical-full-hit",
                                                 "run_original_magic_list": "single",
                                                 "run_original_loot": "chest-empty",
                                                 "run_original_natural": "natural-stage0",
                                                 "run_original_reward": "physical-cap",
                                                 "run_original_stage1": "turn3",
                                                 "run_original_stage1_complete": "victory-all-villagers",
                                                 "run_original_transition": "prepare-controlled"}.get(script.stem))
                with self.subTest(script=script.name, inside=inside), ExitStack() as stack, \
                        patch.object(Path, "resolve", resolve), \
                        patch.object(Path, "mkdir", side_effect=MkdirReached) as mkdir, \
                        patch("subprocess.Popen", side_effect=AssertionError("unexpected process")) as popen, \
                        patch("subprocess.run", side_effect=AssertionError("unexpected process")) as process_run:
                    # 本测试只覆盖输出路径；B4/B5/B7较早的只读资源预检另有专项。
                    # 参数与真实CLI一致，文件存在/读取/mtime仍使用本次临时文件。
                    values = {}
                    if script.stem in {"run_original_loot", "run_original_reward",
                                       "run_original_stage1", "run_original_transition"}:
                        values = {"prepare": {"manifest": {"source_sha256": hashlib.sha256(source).hexdigest()}},
                                  "hashes": {}}
                    elif script.stem == "run_original_natural":
                        values = {"round_assets": {}, "decode_save": {}, "build_pages": [],
                                  "geometry": {}, "title_templates": {}, "title_menu_counts": {},
                                  "load_menu_availability": {}, "hashes": {}}
                    elif script.stem == "run_original_battle_animation":
                        values = {"owned_job_required": None, "fingerprint": {},
                                  "prepare_runtime": {}, "dependencies": {}}
                    elif script.stem == "run_original_stage1_complete":
                        values = {"prepare": {"manifest": {"source_sha256": hashlib.sha256(source).hexdigest()},
                                              "origin": {}},
                                  "owned_job_required": None, "fingerprint": {}}
                    preflight = [stack.enter_context(patch.object(module, name, return_value=value))
                                 for name, value in values.items()]
                    if inside:
                        with self.assertRaises(ValueError):
                            module.run(args)
                        mkdir.assert_not_called()
                        for check in preflight:
                            check.assert_not_called()
                    else:
                        with self.assertRaises(MkdirReached):
                            module.run(args)
                        mkdir.assert_called_once_with(parents=True, exist_ok=False)
                    popen.assert_not_called()
                    process_run.assert_not_called()
                self.assertFalse(output.exists())

    def test_resolved_evidence_child_is_rejected_before_mkdir_for_every_runner(self):
        self.check_destination(True)

    def test_external_artifact_path_still_reaches_mkdir_for_every_runner(self):
        self.check_destination(False)


if __name__ == "__main__":
    unittest.main()
