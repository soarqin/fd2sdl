"""B5准备页模型/真实资源回归；不运行窗口或游戏进程。"""
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import wave
from contextlib import contextmanager
from types import SimpleNamespace
from unittest.mock import Mock, patch

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import fd2_transition_oracle as oracle
import run_original_transition as runner
import run_original_battle_menu as shared
from fd2_opening_oracle import build_pages, page_match
from run_original_end_turn import META, STRIDE, round_assets
from run_graphics_tests import decode_save


class TransitionOracleTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = ROOT / "original_game"
        cls.prepared = runner.prepare(cls.original)

    def test_fixture_uses_complete_authentic_records_and_preserves_other_slots(self):
        prepared = self.prepared
        start = decode_save(self.original / "START.SAV")
        late = decode_save(self.original / "FD2.SAV")
        roster = prepared["state"]["roster"]
        self.assertEqual(roster[:4 * STRIDE], start[0x8a3:0x8a3 + 4 * STRIDE])
        self.assertEqual(roster[4 * STRIDE:5 * STRIDE], late[0x8a3 + 5 * STRIDE:0x8a3 + 6 * STRIDE])
        self.assertEqual(roster[5 * STRIDE:], start[0x8a3 + 5 * STRIDE:0x12a3])
        self.assertEqual(prepared["plain"][:oracle.SLOT_BASE], start[:oracle.SLOT_BASE])
        self.assertEqual(prepared["plain"][oracle.SLOT_BASE + oracle.SLOT_STRIDE:-4], start[oracle.SLOT_BASE + oracle.SLOT_STRIDE:-4])
        self.assertFalse(prepared["manifest"]["natural_stage0_completion"])
        self.assertEqual(prepared["manifest"]["supplement_identity"], {"unit_id": 51, "text_id": 1, "level": 27})
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "controlled.SAV"
            path.write_bytes(prepared["encoded"])
            self.assertEqual(decode_save(path), prepared["plain"])

    def test_manual_slot_full_container_and_last_slot_checksum_overlap(self):
        source, state = self.prepared["plain"], self.prepared["state"]
        for index in range(4):
            changed = oracle.write_manual_slot(source, state, index)
            start = oracle.SLOT_BASE + index * oracle.SLOT_STRIDE
            allowed = set(range(start, start + oracle.ROSTER_SIZE + 10)) | set(range(len(source) - 4, len(source)))
            self.assertTrue(all(a == b or i in allowed for i, (a, b) in enumerate(zip(source, changed))))
            self.assertEqual(changed[start:start + oracle.ROSTER_SIZE], state["roster"])
            self.assertEqual(sum(changed[:-4]), int.from_bytes(changed[-4:], "little"))
            self.assertEqual(changed[start + oracle.ROSTER_SIZE + 10:min(start + oracle.SLOT_STRIDE, len(source) - 4)],
                             source[start + oracle.ROSTER_SIZE + 10:min(start + oracle.SLOT_STRIDE, len(source) - 4)])

    def test_manual_writer_rejects_bad_checksum_and_parameters(self):
        source = bytearray(self.prepared["plain"]); source[17] ^= 1
        with self.assertRaises(ValueError): oracle.write_manual_slot(bytes(source), self.prepared["state"], 0)
        for index in (-1, 4):
            with self.assertRaises(ValueError): oracle.write_manual_slot(self.prepared["plain"], self.prepared["state"], index)
        with self.assertRaises(ValueError):
            oracle.write_manual_slot(self.prepared["plain"], dict(self.prepared["state"], stage=0), 0)

    def test_slot_page_all_rows_and_selected_pixel_are_required(self):
        prepared = self.prepared
        for selected in range(4):
            image = oracle.picker_image(prepared["assets"], prepared["plain"], selected)
            frame = Image.new("RGB", (320, 200)); frame.paste(image.convert("RGB"), (5, 112))
            self.assertEqual(oracle.picker_match(frame, image)["checked_pixels"], 26660)
            for wrong in range(4):
                if wrong != selected:
                    self.assertIsNone(oracle.picker_match(frame, oracle.picker_image(prepared["assets"], prepared["plain"], wrong)))
            # 第四行任意底板像素也必须比较，包括palette0/黑色。
            frame.putpixel((310, 195), tuple(255 - c for c in frame.getpixel((310, 195))))
            self.assertIsNone(oracle.picker_match(frame, image))

    def test_town_full_resource_and_wrong_selector_are_rejected(self):
        prepared = self.prepared
        for selected in range(5):
            for template in prepared["towns"][selected]:
                frame = Image.new("RGB", (320, 200)); frame.paste(template.convert("RGB"), (4, 4))
                found = runner.town_match(frame, prepared, selected)
                self.assertEqual(found["checked_pixels"], 59904)
                self.assertIsNone(runner.town_match(frame, prepared, (selected + 1) % 5))
                frame.putpixel((311, 190), tuple(255 - c for c in frame.getpixel((311, 190))))
                self.assertIsNone(runner.town_match(frame, prepared, selected))

    def test_hotel_must_have_one_correct_highlight_and_complete_background(self):
        assets = self.prepared["assets"]
        for selected in range(4):
            frame = assets["hotel_background"].convert("RGB")
            for index in range(4):
                frame.paste(assets["commands"][2 * index + (index == selected)].convert("RGB"), (201 + index * 26, 169))
            self.assertIsNotNone(oracle.hotel_menu_match(frame, assets, selected))
            for wrong in range(4):
                if wrong != selected: self.assertIsNone(oracle.hotel_menu_match(frame, assets, wrong))
            frame.putpixel((300, 30), tuple(255 - c for c in frame.getpixel((300, 30))))
            self.assertIsNone(oracle.hotel_menu_match(frame, assets, selected))

    def test_entry_contract_uses_stage2_bank_and_real_promoted_portrait(self):
        pages, geometry = self.prepared["pages"], self.prepared["geometry"]
        self.assertEqual(len(pages), 25)
        promoted = [p for p in pages if p["speaker_control"] == -18 and p["speaker_argument"] == 1]
        self.assertTrue(promoted)
        self.assertEqual({p["portrait_id"] for p in promoted}, {51})
        self.assertEqual([a["position"] for a in geometry["actors"][:5]], [[20,14],[23,16],[22,13],[23,14],[24,15]])
        self.assertEqual(geometry["active_count"], 21)
        self.assertTrue(all(not a["hidden"] and a["direction"] == 0 for a in geometry["actors"]))

    def test_shared_opening_default_remains_117_pages(self):
        pages = build_pages(self.original, self.prepared["field_assets"])
        self.assertEqual(len(pages), 117)
        self.assertEqual({p["stage"] for p in pages}, {0, 31, 32})

    def test_every_entry_page_has_positive_resource_gate(self):
        assets = self.prepared["field_assets"]
        previous = None
        for page in self.prepared["pages"]:
            frame = page["text"].convert("RGB")
            frame.paste(assets["dialog_top"].convert("RGB"), page["border_position"])
            frame.paste(page["portraits"][0].convert("RGB"), page["portrait_position"])
            if page["arrow_position"]: frame.paste(page["arrows"][0].convert("RGB"), page["arrow_position"])
            self.assertIsNotNone(page_match(frame, page, assets))
            if previous is not None:
                self.assertIsNone(page_match(previous, page, assets), "前页合成完整已知写集不能误ACK后页")
            previous = frame

    def test_entry_measurement_rejects_roster_tail_metadata_and_last_actor_errors(self):
        prepared = self.prepared
        before, state, expected = prepared["plain"], prepared["state"], prepared["geometry"]
        measured = bytearray(before)
        measured[:1171] = prepared["metadata"]
        measured[0x8a3:0x12a3] = state["roster"]
        measured[0x30a3:0x30c3] = bytes(32)
        measured[META:META + 10] = bytes((1, 21, 1, 14, 11, 19, 14, 5, 3, 5))
        struct.pack_into("<I", measured, META + 10, state["resource"])
        for index, offset in enumerate((15, 14, 16, 17)): measured[META + offset] = state["options"][index]
        for actor in expected["actors"]:
            at = 0x12a3 + actor["raw_actor"] * STRIDE
            wanted, mask = prepared["initialized"][actor["raw_actor"]]
            for index, flag in enumerate(mask):
                if flag: measured[at + index] = wanted[index]
        first = {"inferred_camera": [14, 11]}
        def check(value): return oracle.check_entry_measurement(value, before, state, expected, first,
            prepared["metadata"], prepared["initialized"])
        self.assertEqual(check(measured)["active_count"], 21)
        # native fresh1171B资源之外是未定义heap，不把旧SAV值硬当初值。
        heap_changed = measured.copy(); heap_changed[1200] ^= 1
        self.assertEqual(check(heap_changed)["native_metadata_heap_tail"]["changed_from_previous_file"], 1)
        cache_changed = measured.copy(); cache_changed[0x12a3 + 2] ^= 1
        self.assertEqual(check(cache_changed)["active_count"], 21)
        for offset in (0x12a3 + 20 * STRIDE, 0x8a3 + 31 * STRIDE + 79, 1170,
                       oracle.SLOT_BASE + 0xa00 + 35, META + 25, 0x12a3 + 21 * STRIDE + 5):
            bad = measured.copy(); bad[offset] ^= 1
            with self.assertRaises(AssertionError, msg=f"offset{offset:x}"):
                check(bad)


class TransitionProtocolTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.prepared = TransitionOracleTest.prepared if hasattr(TransitionOracleTest, "prepared") else runner.prepare(ROOT / "original_game")

    def exercise(self, failure=None):
        prepared, seen = self.prepared, []
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            save = work / "FD2.SAV"; save.write_bytes(prepared["encoded"])
            guard = [save.read_bytes(), save.stat().st_mtime_ns]
            capture = SimpleNamespace(artifacts=work, process=SimpleNamespace(pid=77), key=Mock(), wait=Mock())
            report = {"checkpoints": []}
            state = {"plain": prepared["plain"], "slot": 0}
            def wait(capture, name, predicate, rows, **kwargs):
                seen.append((name, capture.key.call_count))
                if name == failure: raise TimeoutError(name)
                result = {"kind": "model"}
                if name == "entry-first-browse": result = {"focus_screen": [148, 76], "inferred_camera": [14, 11]}
                return Image.new("RGB", (320, 200)), result
            def measure(path, previous, stamp):
                index = state["slot"]
                plain = oracle.write_manual_slot(state["plain"], prepared["state"], index) if index < 4 else state["plain"]
                encoded = bytes(a ^ b ^ c for a, b, c in zip(prepared["encoded"], prepared["plain"], plain))
                path.write_bytes(encoded)
                state.update(plain=plain, slot=index + 1)
                return plain
            with patch.object(runner, "wait_title"), patch.object(runner, "wait_state", side_effect=wait), \
                    patch.object(runner, "wait_measured_save", side_effect=measure), \
                    patch.object(runner, "check_entry_measurement", return_value={"model_only": True}):
                try: runner.exercise(capture, prepared, save, guard, report)
                except TimeoutError:
                    if failure is None: raise
            return capture, report, seen

    def test_four_legitimate_slot_writes_then_load_and_entry_ack(self):
        capture, report, seen = self.exercise()
        self.assertEqual([row["slot"] for row in report["manual_saves"]], [0, 1, 2, 3])
        self.assertEqual(len(report["entry_acknowledgements"]), 25)
        self.assertLess([n for n, _ in seen].index("hotel-load-notice478"), [n for n, _ in seen].index("entry-wait-00-fragment0"))
        self.assertEqual(capture.key.call_args_list[-1].args, ("ENTER",))
        self.assertEqual([n for n, _ in seen][-1], "entry-save-returned-browse")

    def test_failed_resource_gate_sends_no_ack_or_further_keys(self):
        for failure in ("town-initial-0", "hotel-load-notice478", "entry-wait-06-fragment2"):
            capture, report, seen = self.exercise(failure)
            self.assertEqual(seen[-1][0], failure)
            self.assertEqual(capture.key.call_count, seen[-1][1])
            self.assertNotIn("completed_save", report)

    def test_sdl_and_readonly_alias_are_rejected_before_mkdir(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "not-created"
            args = SimpleNamespace(root=ROOT, artifacts=output, sdl=Path("unsupported.exe"))
            with self.assertRaises(ValueError): runner.run(args)
            self.assertFalse(output.exists())
        args = SimpleNamespace(root=ROOT, artifacts=(ROOT / "original_game").resolve() / "forbidden", sdl=None)
        with patch.object(Path, "mkdir", side_effect=AssertionError("must reject before mkdir")):
            with self.assertRaises(ValueError): runner.run(args)

    def test_real_low_color_picker_requires_opt_in_and_full_match(self):
        expected = oracle.picker_image(self.prepared["assets"], self.prepared["plain"], 0)
        frame = Image.new("RGB", (320, 200)); frame.paste(expected.convert("RGB"), (5, 112))
        self.assertEqual(len(frame.getcolors(64000)), 15)
        with tempfile.TemporaryDirectory() as temporary:
            capture = SimpleNamespace(frame=Mock(return_value=frame), artifacts=Path(temporary),
                process=SimpleNamespace(pid=77), last=frame)
            predicate = lambda image: oracle.picker_match(image, expected)
            with self.assertRaisesRegex(ValueError, "颜色不足"):
                shared.wait_state(capture, "default-still-rejects", predicate, [])
            records = []
            _, found = shared.wait_state(capture, "picker", predicate, records, allow_low_color_resource=True)
            self.assertTrue(found["low_color_complete_resource"])
            self.assertEqual(found["checked_pixels"], 26660)
            self.assertEqual(len(records), 1)
            self.assertTrue((Path(temporary) / "picker.png").is_file())

    def test_low_color_opt_in_does_not_accept_black_bad_pixel_or_permission_error(self):
        expected = oracle.picker_image(self.prepared["assets"], self.prepared["plain"], 0)
        good = Image.new("RGB", (320, 200)); good.paste(expected.convert("RGB"), (5, 112))
        damaged = good.copy(); damaged.putpixel((300, 195), (255, 0, 0))
        for image in (Image.new("RGB", (320, 200)), damaged):
            with tempfile.TemporaryDirectory() as temporary:
                capture = SimpleNamespace(frame=Mock(return_value=image), artifacts=Path(temporary),
                    process=SimpleNamespace(pid=77), last=image)
                records = []
                with patch.object(shared.time, "monotonic", side_effect=(0, 0, 2, 2)), patch.object(shared.time, "sleep"):
                    with self.assertRaises(TimeoutError):
                        shared.wait_state(capture, "bad", lambda f: oracle.picker_match(f, expected), records,
                                          timeout=1, allow_low_color_resource=True)
                self.assertFalse(any(row.get("kind") == "manual_slot_picker" for row in records))
        with tempfile.TemporaryDirectory() as temporary:
            capture = SimpleNamespace(frame=Mock(side_effect=RuntimeError("capture access denied")),
                artifacts=Path(temporary), process=SimpleNamespace(pid=77), last=None)
            with self.assertRaisesRegex(RuntimeError, "access denied"):
                shared.wait_state(capture, "denied", lambda f: oracle.picker_match(f, expected), [],
                                  allow_low_color_resource=True)
            self.assertEqual(capture.frame.call_count, 1)

    def test_original_failure_or_late_file_drift_never_starts_sdl(self):
        for late in (False, True):
            with tempfile.TemporaryDirectory() as temporary:
                work = Path(temporary); binary = work / "fake.exe"; binary.write_bytes(b"model")
                args = SimpleNamespace(root=ROOT, artifacts=work / "out", sdl=binary, driver=binary, dosbox=binary)
                calls, closed = [], []
                @contextmanager
                def owned(command, cwd, env, log, driver, receipts):
                    receipts.append({"pid": 77, "cleaned": False, "forced_cleanup": False, "exit_code": None})
                    try: yield SimpleNamespace(pid=77)
                    finally:
                        receipts[-1].update(cleaned=True, exit_code=0); closed.append(True)
                        if late:
                            path = Path(cwd) / "game/FD2.SAV"
                            data = bytearray(path.read_bytes()); data[10] ^= 1; path.write_bytes(data)
                def execute(capture, prepared, save, guard, report):
                    calls.append(report["kind"])
                    if not late: raise TimeoutError("resource gate did not arrive")
                def copy(source, target): Path(target).mkdir()
                with patch.object(runner, "running", owned), patch.object(runner, "exercise", execute), \
                        patch.object(runner.shutil, "copytree", side_effect=copy):
                    report = runner.run(args)
                self.assertFalse(report["ok"])
                self.assertEqual(calls, ["original"])
                self.assertEqual(len(closed), 1)
                self.assertFalse((args.artifacts / "sdl").exists())

    def pair_fixture(self, directory):
        prepared = self.prepared
        def encode(plain):
            plain = oracle._checksum(plain)
            return bytes(a ^ b ^ c for a,b,c in zip(prepared["encoded"], prepared["plain"], plain))
        entry = bytearray(prepared["plain"])
        entry[:1171] = prepared["metadata"]; entry[0x8a3:0x12a3] = prepared["state"]["roster"]
        for index, (wanted, mask) in enumerate(prepared["initialized"]):
            for offset, known in enumerate(mask):
                if known: entry[0x12a3 + index * 80 + offset] = wanted[offset]
        entry[0x30a3:0x30c3] = bytes(32)
        entry[META:META+10] = bytes((1,21,1,14,11,19,14,5,3,5))
        struct.pack_into("<I",entry,META+10,prepared["state"]["resource"])
        for i,at in enumerate((15,14,16,17)):entry[META+at]=prepared["state"]["options"][i]
        cases=[]
        for ordinal, role in enumerate(("original", "sdl")):
            folder = directory / role; folder.mkdir()
            case={"kind":role,"probe_cases_passed":True,"checkpoints":[],"manual_saves":[],"capture_hashes":{}}
            plain=prepared["plain"]
            for slot in range(4):
                plain=oracle.write_manual_slot(plain,prepared["state"],slot)
                save=folder/f"slot-{slot}.SAV";save.write_bytes(encode(plain))
                case["manual_saves"].append({"slot":slot,"snapshot":str(save)})
                image=Image.new("RGB",(320,200));image.paste(oracle.picker_image(prepared["assets"],plain,slot).convert("RGB"),(5,112))
                image.save(folder/f"picker-{slot}.png")
                case["checkpoints"].append({"name":f"picker-{slot}","kind":"manual_slot_picker","pid":ordinal+1,"time":slot+1,"wgc_transient_retries":0})
            image=Image.new("RGB",(320,200));image.paste(prepared["towns"][0][ordinal].convert("RGB"),(4,4));image.save(folder/"town.png")
            case["checkpoints"].append({"name":"town","kind":"town_selector","selected":0,"pid":ordinal+1,"time":5,"wgc_transient_retries":0})
            value=bytearray(entry);value[1171] ^= ordinal;value[0x12a3+2] ^= ordinal
            value[oracle.SLOT_BASE:-4]=plain[oracle.SLOT_BASE:-4]
            snapshot=folder/"entry.SAV";snapshot.write_bytes(encode(value))
            case["completed_save"]={"snapshot":str(snapshot)}
            case["entry_measurement"]=oracle.check_entry_measurement(oracle._checksum(value),plain,prepared["state"],prepared["geometry"],
                {"inferred_camera":[14,11]},prepared["metadata"],prepared["initialized"])
            case["capture_hashes"]={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in folder.iterdir() if p.is_file()}
            cases.append(case)
        return cases, encode

    def test_pair_preserves_exact_four_saves_and_only_declared_entry_holes(self):
        with tempfile.TemporaryDirectory() as temporary:
            cases,_=self.pair_fixture(Path(temporary))
            result=runner.compare_endpoints(cases,Path(temporary),self.prepared)
            self.assertTrue(result["manual_four_saves_full_encoded_equal"])
            self.assertFalse(result["full_entry_save_bytes_equal_claimed"])
            self.assertEqual(result["common_checkpoints"],5)

    def test_pair_rejects_manual_unknown_byte_initialized_stat_and_pixel(self):
        for fault in ("manual", "stat", "pixel"):
            with tempfile.TemporaryDirectory() as temporary:
                directory=Path(temporary);cases,encode=self.pair_fixture(directory)
                if fault=="manual":
                    path=Path(cases[1]["manual_saves"][3]["snapshot"]);data=bytearray(decode_save(path));data[0x3000]^=1;path.write_bytes(encode(data))
                elif fault=="stat":
                    path=Path(cases[1]["completed_save"]["snapshot"]);data=bytearray(decode_save(path));data[0x12a3+20*80+0x48]^=1;path.write_bytes(encode(data))
                else:
                    path=directory/"sdl/picker-3.png";image=Image.open(path).convert("RGB");image.putpixel((300,195),(255,0,0));image.save(path)
                with self.assertRaises(AssertionError):runner.compare_endpoints(cases,directory,self.prepared)

    def test_audio_is_native_opt_in_and_only_two_host_keys(self):
        capture=SimpleNamespace(key=Mock())
        report={"native_audio":{"enabled":True,"started":False,"stopped":False}}
        runner.audio_toggle(capture,report,cleanup=True)
        self.assertEqual(capture.key.call_count,0)
        runner.audio_toggle(capture,report)
        runner.audio_toggle(capture,report)
        runner.audio_toggle(capture,report,cleanup=True)
        self.assertEqual([r.args for r in capture.key.call_args_list],[("HOST_W",),("HOST_W",)])
        self.assertTrue(report["native_audio"]["stopped"])
        disabled={"native_audio":{"enabled":False}}
        runner.audio_toggle(capture,disabled)
        self.assertEqual(capture.key.call_count,2)

    def test_recorded_wav_identity_does_not_claim_audio_equivalence(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary);(directory/"captures").mkdir()
            path=directory/"captures/native.wav"
            with wave.open(str(path),"wb") as stream:
                stream.setnchannels(2);stream.setsampwidth(2);stream.setframerate(44100);stream.writeframes(bytes(400))
            case={"business_sequence_completed":True,"native_audio":{"enabled":True,"started":True,"stopped":True}}
            runner.collect_native_audio(case,directory)
            self.assertEqual(case["native_audio"]["files"][0]["sha256"],hashlib.sha256(path.read_bytes()).hexdigest())
            self.assertFalse(case["native_audio"]["a5_equivalence_validated"])
            path.unlink()
            with self.assertRaises(AssertionError):runner.collect_native_audio(case,directory)

    def test_inherited_contract_has_real_exercise_order_and_exact_business_keys(self):
        capture, _, seen = self.exercise()
        self.assertEqual([name for name, _ in seen], [row["name"] for row in runner.reference_protocol(self.prepared)])
        self.assertEqual([row.args[0] for row in capture.key.call_args_list], runner.reference_keys(25))
        self.assertEqual(len(runner.reference_keys(25)), 81)

    def test_failed_reference_endpoint_or_cleanup_cannot_be_reused(self):
        for fault in ("endpoint", "cleanup", "keys"):
            with tempfile.TemporaryDirectory() as temporary:
                directory = Path(temporary)
                case = {"kind":"original", "status":"passed", "probe_cases_passed":True,
                    "business_sequence_completed":True,"original_unchanged":True,"fixture_unchanged":True,
                    "copy_save_matches_completed_state":True,"cleanup_passed":True,
                    "processes":[{"pid":77,"exit_code":0,"cleaned":True,"forced_cleanup":False}],
                    "inputs":[{"pid":77,"key":key} for key in runner.reference_keys(25)]}
                if fault == "endpoint": case["probe_cases_passed"] = False
                elif fault == "cleanup": case["processes"][0]["forced_cleanup"] = True
                else: case["inputs"].append({"pid":77,"key":"ENTER"})
                document = {"scenario":"prepare-controlled","capture_backend":"Windows.Graphics.Capture HWND",
                    "derived_fixture":self.prepared["manifest"],"geometry":self.prepared["geometry"],
                    "original_before":runner.hashes(ROOT/"original_game"),"original_unchanged":True,
                    "original_all_mtimes_unchanged":True,"cases":[case]}
                path = directory / "comparison.json";path.write_text(json.dumps(document),encoding="utf-8")
                with self.assertRaisesRegex(ValueError, {"endpoint":"未完成", "cleanup":"正常退出", "keys":"逐键协议"}[fault]):
                    runner.validate_original_reference(path,self.prepared,ROOT/"original_game")

    def test_reference_needs_sdl_and_cannot_write_inside_original_evidence(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary);binary=directory/"fake.exe";binary.write_bytes(b"pure")
            args=SimpleNamespace(root=ROOT,artifacts=directory/"out",sdl=None,original_evidence=directory/"comparison.json")
            with self.assertRaisesRegex(ValueError,"SDL续跑"):runner.run(args)
            args.sdl=binary
            with self.assertRaisesRegex(ValueError,"继承原版证据目录"):runner.run(args)
            self.assertFalse(args.artifacts.exists())

    def test_inherited_copy_hashes_keep_old_pid_and_original_paths_without_launch(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary);cases,_=self.pair_fixture(directory);case=cases[0]
            source=directory/"original";(source/"game").mkdir()
            snapshot=Path(case["completed_save"]["snapshot"])
            (source/"game/FD2.SAV").write_bytes(snapshot.read_bytes())
            case.update(processes=[{"pid":77,"exit_code":0,"cleaned":True}],inputs=[{"pid":77,"key":"ENTER"}])
            destination=directory/"inherited"
            result=runner.copy_original_reference(case,{"source_endpoint":str(source)},destination)
            self.assertEqual(result["execution_origin"],"inherited_original")
            self.assertEqual(result["processes"],case["processes"])
            self.assertEqual(result["inputs"],case["inputs"])
            self.assertEqual(result["completed_save"]["original_snapshot"],case["completed_save"]["snapshot"])
            self.assertEqual(Path(result["completed_save"]["snapshot"]).read_bytes(),snapshot.read_bytes())
            self.assertEqual(result["capture_hashes"],case["capture_hashes"])
            # 不能只信JSON声称hash：实际附件的一个像素被改也不能复制成功。
            path=source/"town.png";image=Image.open(path).convert("RGB");image.putpixel((4,4),(255,0,255));image.save(path)
            with self.assertRaisesRegex(ValueError,"复制前后hash"):
                runner.copy_original_reference(case,{"source_endpoint":str(source)},directory/"broken")

    def test_inherited_run_executes_only_sdl_and_labels_composition(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory=Path(temporary);old=directory/"old";old.mkdir();cases,_=self.pair_fixture(old)
            case=cases[0];source=old/"original";(source/"game").mkdir()
            snapshot=Path(case["completed_save"]["snapshot"])
            (source/"game/FD2.SAV").write_bytes(snapshot.read_bytes())
            case["completed_save"].update(sha256=hashlib.sha256(snapshot.read_bytes()).hexdigest(),mtime_ns=(source/"game/FD2.SAV").stat().st_mtime_ns)
            case.update(status="passed",processes=[{"pid":77,"exit_code":0,"cleaned":True,"forced_cleanup":False}],inputs=[])
            binary=directory/"fake.exe";binary.write_bytes(b"pure")
            args=SimpleNamespace(root=ROOT,artifacts=directory/"new",sdl=binary,driver=binary,dosbox=binary,
                original_evidence=old/"comparison.json")
            launched=[]
            @contextmanager
            def running(argv,cwd,env,log,driver,receipts):
                launched.append(argv);receipts.append({"pid":88,"exit_code":0,"cleaned":True,"forced_cleanup":False})
                yield SimpleNamespace(pid=88)
            with patch.object(runner,"validate_original_reference",return_value=(case,{"source_endpoint":str(source)})), \
                    patch.object(runner,"reference_unchanged"),patch.object(runner,"running",side_effect=running), \
                    patch.object(runner,"exercise"),patch.object(runner,"compare_endpoints",return_value={"model_only":True}):
                result=runner.run(args)
            self.assertTrue(result["ok"],result.get("error"))
            self.assertEqual(launched,[[str(binary.resolve())]])
            self.assertEqual(result["composition"],{"inherited_original":1,"executed_original":0,"executed_sdl":1,"fresh_pair":False})
            self.assertEqual([row["kind"] for row in result["cases"]],["original","sdl"])
            self.assertEqual(result["cases"][0]["processes"][0]["pid"],77)
            self.assertTrue(result["inherited_original_unchanged_after_sdl"])


if __name__ == "__main__": unittest.main()
