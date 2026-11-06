"""B6帧oracle的纯模型/原资源回归；所有PNG均为显式synthetic，不是WGC运行证据。"""
import hashlib
import os
from pathlib import Path
import struct
import sys
import tempfile
import unittest

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import fd2_battle_animation_frames as probe
from run_graphics_tests import decode_save


class BattleAnimationFramesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = Path(os.environ.get("FD2_TEST_ORIGINAL", ROOT / "original_game"))
        base = bytearray(decode_save(cls.original / "START.SAV"))
        cls.sources = {}
        for name, caster, targets, kind, magic in (
                ("physical", 0, [11], "physical", None), ("zero", 1, [11], "magic", 0),
                ("heal", 1, [0], "magic", 13), ("ai3", 1, [0], "magic", 3)):
            source = bytearray(base)
            for raw, xy in ((0, (8, 14) if name == "heal" else (7, 14)), (1, (7, 14)), (11, (8, 14))):
                source[probe.UNITS + raw * 80:probe.UNITS + raw * 80 + 2] = bytes(xy)
            source[probe.META + 3:probe.META + 9] = bytes((0, 9, 7, 14, 7, 5))
            source[-4:] = sum(source[:-4]).to_bytes(4, "little")
            cls.sources[name] = bytes(source)
        cls.resources = {name: probe.prepare_resources(cls.original, cls.sources[name], caster, targets,
            kind=kind, magic_id=magic) for name, caster, targets, kind, magic in (
                ("physical", 0, [11], "physical", None), ("zero", 1, [11], "magic", 0),
                ("heal", 1, [0], "magic", 13), ("ai3", 1, [0], "magic", 3))}

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="fd2-b6-frame-model-")
        self.addCleanup(self.directory.cleanup)
        self.path = Path(self.directory.name)

    def png(self, name, frame, timestamp=0, scale=1):
        path = self.path / (name + ".png")
        frame = frame.resize((320 * scale, 200 * scale), Image.Resampling.NEAREST)
        frame.save(path)
        return {"path": str(path.resolve()), "timestamp_ms": timestamp,
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "width": frame.width, "height": frame.height}

    @staticmethod
    def model(resources, phase, excluded=()):
        # 显式合成测试图，只测试资源分类/完整帧比较；不执行或模拟C结算/FX引擎。
        for rule in resources["rules"][phase]:
            frame = Image.new("RGB", (320, 200))
            for y in range(200):
                for x in range(320):
                    frame.putpixel((x, y), ((x * 3) % 256, y, (x + y) % 256))
            for anchor in rule["anchors"]:
                width = anchor["image"].width
                for i, color in anchor["pixels"]:
                    frame.putpixel((anchor["at"][0] + i % width, anchor["at"][1] + i // width), color)
            detail = probe.classify_frame(frame, resources).get(phase)
            if detail:
                pose = detail["variant"][:2] if phase == "map-heal" else detail["variant"]
                if pose not in excluded: return frame
        raise AssertionError("pure synthetic positive unavailable for " + phase)

    def test_real_sources_and_resources_have_independent_phase_rules(self):
        for name, phases in (("physical", ("physical-attack", "physical-impact")),
                ("zero", ("casting", "full-fx")), ("heal", ("map-heal",))):
            for phase in phases:
                self.assertGreater(len(self.resources[name]["rules"][phase]), 0)
                identity = probe.classify_frame(self.model(self.resources[name], phase), self.resources[name])[phase]
                self.assertGreaterEqual(len(identity["anchors"]), 3)
                self.assertTrue(all(a["nonblack_pixels"] >= 24 for a in identity["anchors"]))

    def physical_frame(self, part, target, offset=(0, 0), tint=None):
        """显式synthetic：原BG/姓名/LV和原parts；不读C或实际截图拟合。"""
        resources = self.resources["physical"]
        palette = [x << 2 for x in probe.archive_entry((self.original / "FDOTHER.DAT").read_bytes(), 0)]
        bg_entry = resources["rules"]["physical-impact"][0]["anchors"][1]["identity"]["entry"]
        bg, bg_mask = probe.rle_material(probe.archive_entry((self.original / "BG.DAT").read_bytes(), bg_entry))
        canvas, mask = Image.new("P", (320, 200)), Image.new("L", (320, 200))
        probe._paste(canvas, mask, {"image": bg, "mask": bg_mask, "at": (0, 50)})
        if part["layer"] & 1: probe._paste(canvas, mask, part)
        probe._paste(canvas, mask, target, offset, tint)
        if not part["layer"] & 1: probe._paste(canvas, mask, part)
        canvas.putpalette(palette); frame = canvas.convert("RGB")
        title = resources["rules"]["physical-impact"][0]["anchors"][0]
        for i, color in title["pixels"]:
            frame.putpixel((title["at"][0] + i % title["image"].width,
                            title["at"][1] + i // title["image"].width), color)
        return frame

    def test_physical_all_nonzero_hit_shakes_are_resource_gated_not_only_first_tint(self):
        resources = self.resources["physical"]
        self.assertEqual({rule["variant"][2] for rule in resources["rules"]["physical-impact"]}, set(range(1, 6)))
        figani = (self.original / "FIGANI.DAT").read_bytes()
        attack, _ = probe.figani_parts(figani, 1); target, _ = probe.figani_parts(figani, 96 * 3)
        for part, tick, shake, tint in probe._physical_hit_ticks(attack):
            frame = self.physical_frame(part, target[0], (-probe.PHYSICAL_SHAKE_X[shake], -probe.PHYSICAL_SHAKE_Y[shake]), tint)
            detail = probe.classify_frame(frame, resources)["physical-impact"]
            self.assertEqual(detail["variant"][2:], (shake, tint))
            self.assertEqual(detail["anchors"][-1]["identity"]["part_tick"], tick)

    def test_hit_tick_metadata_zero_duration_retrigger_and_original_offset_tables(self):
        raw = (self.original / "FD2.EXE").read_bytes()
        le = probe.parse_le(raw)
        image = probe.build_relbase_image(raw, le, probe.parse_objects(raw, le))
        self.assertEqual(struct.unpack_from("<6i", image, 0x5266c), probe.PHYSICAL_SHAKE_X)
        self.assertEqual(struct.unpack_from("<6i", image, 0x52684), probe.PHYSICAL_SHAKE_Y)
        figani = (self.original / "FIGANI.DAT").read_bytes()
        attack, _ = probe.figani_parts(figani, 1)
        self.assertEqual([(p["index"], tick, shake, tint) for p, tick, shake, tint in probe._physical_hit_ticks(attack)],
            [(6, 0, 5, 33), (6, 1, 4, None), (7, 0, 3, None), (7, 1, 2, None), (8, 0, 1, None)])
        synthetic = [{"index": i, "marker": marker, "duration": duration}
            for i, (marker, duration) in enumerate(((1, 0), (0, 2), (2, 1), (0, 4)))]
        self.assertEqual([(p["index"], tick, shake, tint) for p, tick, shake, tint in probe._physical_hit_ticks(synthetic)],
            [(1, 0, 5, 33), (1, 1, 4, None), (2, 0, 5, 33), (3, 0, 4, None),
             (3, 1, 3, None), (3, 2, 2, None), (3, 3, 1, None)])
        self.assertEqual(list(probe._physical_hit_ticks([{"marker": 0, "duration": 10}])), [])

    def test_zero_shake_miss_and_ordinary_idle_never_stand_in_for_hit(self):
        figani = (self.original / "FIGANI.DAT").read_bytes()
        attack, _ = probe.figani_parts(figani, 1); idle, _ = probe.figani_parts(figani, 0)
        targets, _ = probe.figani_parts(figani, 96 * 3)
        for part in [attack[6], attack[7], attack[8], *idle]:
            for target in targets:
                frame = self.physical_frame(part, target)
                self.assertNotIn("physical-impact", probe.classify_frame(frame, self.resources["physical"]))

    def test_followthrough_pair_still_requires_all_64000_pixels(self):
        figani = (self.original / "FIGANI.DAT").read_bytes()
        attack, _ = probe.figani_parts(figani, 1); idle, _ = probe.figani_parts(figani, 96 * 3)
        a = self.physical_frame(attack[6], idle[2], (-18, -8))
        a.putpixel((319, 199), (0, 0, 0)); b = a.copy()
        result = probe.compare_sequences([self.png("follow-a", a)], [self.png("follow-b", b)],
            self.resources["physical"], required_phases=["physical-impact"])
        self.assertTrue(result["ok"])
        pair = result["paired_phases"]["physical-impact"]["pairs"][0]
        self.assertEqual((pair["checked_pixels"], pair["bad_pixels"], pair["max_channel_delta"]), (64000, 0, 0))
        b.putpixel((319, 199), (4, 0, 0))
        self.assertIn("physical-impact", probe.classify_frame(b, self.resources["physical"]))
        result = probe.compare_sequences([self.png("follow-c", a)], [self.png("follow-d", b)],
            self.resources["physical"], required_phases=["physical-impact"])
        self.assertFalse(result["ok"])

    def test_rle_written_black_and_255_are_not_skip(self):
        raw = bytes((6, 0, 1, 0, 0, 0, 0xc0, 0x80, 255, 0x40, 9, 0xc0))
        image, mask = probe.rle_material(raw)
        self.assertEqual(list(image.getdata()), [0, 0, 255, 0, 9, 0])
        self.assertEqual(list(mask.getdata()), [255, 0, 255, 0, 255, 0])
        with self.assertRaises(ValueError): probe.rle_material(raw[:-1])

    def test_synthetic_pair_checks_full64000_and_keeps_timestamps_and_raw_png(self):
        resources = self.resources["zero"]; frame = self.model(resources, "casting")
        native, sdl = self.png("native", frame, 17), self.png("sdl", frame, 310, 2)
        result = probe.compare_sequences([native], [sdl], resources, required_phases=["casting"])
        self.assertTrue(result["ok"]); pair = result["paired_phases"]["casting"]["pairs"][0]
        self.assertEqual((pair["checked_pixels"], pair["bad_pixels"], pair["max_channel_delta"]), (64000, 0, 0))
        self.assertEqual(pair["original"]["timestamp_ms"], 17)
        self.assertEqual(pair["sdl"]["timestamp_ms"], 310)
        self.assertFalse(result["whole_animation_equivalence"])
        self.assertEqual(hashlib.sha256(Path(native["path"]).read_bytes()).hexdigest(), native["sha256"])

    def test_idle_and_fixed_tail_pose_cannot_stand_in_for_casting(self):
        resources = self.resources["zero"]
        figani = (self.original / "FIGANI.DAT").read_bytes()
        idle, _ = probe.figani_parts(figani, 27)
        casting, _ = probe.figani_parts(figani, 28)
        other = (self.original / "FDOTHER.DAT").read_bytes()
        palette = [x << 2 for x in probe.archive_entry(other, 0)]
        common = resources["rules"]["casting"][0]["anchors"][:2]
        for part in idle + [casting[-1]]:
            frame = Image.new("RGB", (320, 200))
            for y in range(200):
                for x in range(320): frame.putpixel((x, y), (x % 256, y, (x + y) % 256))
            actor = probe._part_anchor(part, palette, {"test": "idle/tail"})
            for anchor in common + [actor]:
                for i, color in anchor["pixels"]:
                    frame.putpixel((anchor["at"][0] + i % anchor["image"].width,
                                    anchor["at"][1] + i // anchor["image"].width), color)
            self.assertNotIn("casting", probe.classify_frame(frame, resources))

    def test_unanchored_black_corner_pixel_is_still_compared(self):
        resources = self.resources["zero"]; a = self.model(resources, "casting")
        a.putpixel((319, 199), (0, 0, 0)); b = a.copy(); b.putpixel((319, 199), (4, 0, 0))
        self.assertIn("casting", probe.classify_frame(b, resources))
        result = probe.compare_sequences([self.png("a", a)], [self.png("b", b)], resources, required_phases=["casting"])
        self.assertFalse(result["ok"]); self.assertFalse(result["paired_phases"])
        b.putpixel((319, 199), (3, 0, 0))
        result = probe.compare_sequences([self.png("a2", a)], [self.png("b2", b)], resources, required_phases=["casting"])
        self.assertTrue(result["ok"]); self.assertEqual(result["paired_phases"]["casting"]["pairs"][0]["max_channel_delta"], 3)

    def test_wrong_name_or_lv_and_missing_background_reject_phase(self):
        resources = self.resources["zero"]; a = self.model(resources, "casting")
        for target in ("title", "background"):
            b = a.copy()
            anchors = resources["rules"]["casting"][0]["anchors"]
            anchor = anchors[0 if target == "title" else 1]
            for i, _ in anchor["pixels"]:
                b.putpixel((anchor["at"][0] + i % anchor["image"].width,
                            anchor["at"][1] + i // anchor["image"].width), (255, 0, 255))
            self.assertNotIn("casting", probe.classify_frame(b, resources))

    def test_black_and_unsampled_phase_are_pending_not_pass(self):
        resources = self.resources["physical"]; black = Image.new("RGB", (320, 200))
        result = probe.compare_sequences([self.png("a", black)], [self.png("b", black)], resources,
                                         required_phases=["physical-attack", "physical-impact"])
        self.assertFalse(result["ok"]); self.assertEqual(len(result["pending_phases"]), 2)
        result = probe.compare_sequences([], [], resources, required_phases=["physical-impact"])
        self.assertFalse(result["ok"])

    def test_attack_is_not_impact_and_ai_cast_is_not_complete_fx(self):
        resources = self.resources["physical"]; frame = self.model(resources, "physical-attack")
        result = probe.compare_sequences([self.png("a", frame)], [self.png("b", frame)], resources,
                                         required_phases=["physical-attack", "physical-impact"])
        self.assertIn("physical-attack", result["paired_phases"])
        self.assertFalse(result["ok"])
        resources = self.resources["ai3"]; frame = self.model(resources, "casting")
        result = probe.compare_sequences([self.png("c", frame)], [self.png("d", frame)], resources,
                                         required_phases=["casting", "full-fx"])
        self.assertIn("casting", result["paired_phases"])
        self.assertFalse(result["ok"]); self.assertEqual(result["pending_phases"][0]["phase"], "full-fx")

    def test_hash_timestamp_dimensions_and_same_file_are_strict(self):
        resources = self.resources["zero"]; frame = self.model(resources, "casting")
        a, b = self.png("a", frame), self.png("b", frame)
        for field, value in (("sha256", "0" * 64), ("timestamp_ms", -1), ("width", 640)):
            broken = dict(b, **{field: value})
            with self.assertRaises(ValueError): probe.compare_sequences([a], [broken], resources, required_phases=["casting"])
        with self.assertRaises(ValueError): probe.compare_sequences([a], [a], resources, required_phases=["casting"])
        with self.assertRaises(ValueError): probe.compare_sequences([a], [b], resources, required_phases=[])

    def test_source_guard_and_unsupported_modes_do_not_fabricate_frames(self):
        source = self.sources["zero"]; before = bytes(source)
        wrong = bytearray(source); wrong[-1] ^= 1
        with self.assertRaises(ValueError): probe.prepare_resources(self.original, wrong, 1, [11], kind="magic", magic_id=0)
        with self.assertRaises(ValueError): probe.prepare_resources(self.original, source, 1, [11], kind="magic", magic_id=9)
        clip, info = probe.figani_parts((self.original / "FIGANI.DAT").read_bytes(), 29)
        self.assertTrue(info["empty"]); self.assertEqual(clip, [])
        self.assertEqual(source, before)

    def test_phase_order_and_distinct_spell_frames_cannot_be_repeated_snapshots(self):
        resources = self.resources["physical"]
        attack, impact = self.model(resources, "physical-attack"), self.model(resources, "physical-impact")
        native = [self.png("native-attack", attack, 1), self.png("native-impact", impact, 2)]
        wrong_order = [self.png("sdl-impact", impact, 10), self.png("sdl-attack", attack, 20)]
        result = probe.compare_sequences(native, wrong_order, resources,
            required_phases=["physical-attack", "physical-impact"])
        self.assertFalse(result["ok"])
        result = probe.compare_sequences(native, [self.png("good-a", attack, 10), self.png("good-b", impact, 20)], resources,
            required_phases=["physical-attack", "physical-impact"])
        self.assertTrue(result["ok"])
        resources = self.resources["zero"]; frame = self.model(resources, "casting")
        result = probe.compare_sequences([self.png("repeat-a", frame, 1), self.png("repeat-b", frame, 2)],
            [self.png("repeat-c", frame, 3), self.png("repeat-d", frame, 4)], resources,
            required_phases=["casting"], minimum_pairs=2)
        self.assertFalse(result["ok"]); self.assertEqual(len(result["pending_phases"][0]["matched_pairs"]), 1)
        for name, phase in (("zero", "casting"), ("heal", "map-heal")):
            resources = self.resources[name]
            first = self.model(resources, phase)
            pose = probe.classify_frame(first, resources)[phase]["variant"]
            if phase == "map-heal": pose = pose[:2]
            second = self.model(resources, phase, {pose})
            result = probe.compare_sequences([self.png(name + "-a", first, 1), self.png(name + "-b", second, 2)],
                [self.png(name + "-c", first, 10), self.png(name + "-d", second, 20)], resources,
                required_phases=[phase], minimum_pairs=2)
            self.assertTrue(result["ok"])
            self.assertEqual(len(result["paired_phases"][phase]["pairs"]), 2)


if __name__ == "__main__":
    unittest.main()
