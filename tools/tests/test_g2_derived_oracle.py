"""派生28来源／关联结果门的纯回归；不是原版窗口运行。"""
from pathlib import Path
import struct
import sys
import unittest
from unittest import mock

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import fd2_g2_derived_oracle as oracle


class DerivedOracleTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original=Path(__file__).resolve().parents[2]/"original_game"
        cls.encoded,cls.source,cls.manifest=oracle.derive_fixture(cls.original)

    def guard(self):
        return mock.patch.object(oracle,"derive_fixture",return_value=(self.encoded,self.source,self.manifest))

    def candidates(self,engine="sdl",after_end=False):
        with self.guard():
            return oracle.expected_sources(self.original,self.source,engine=engine,after_end=after_end)

    def check(self,measured,engine="sdl",after_end=False):
        with self.guard():
            return oracle.check_result(self.original,self.source,oracle.SCENARIO,measured,engine=engine,after_end=after_end)

    def test_real_sources_template_and_unknown_owner(self):
        self.assertEqual(oracle.common.crypt(self.encoded),self.source)
        template=self.manifest["template"]
        self.assertEqual((template["stage"],template["index"],template["side"],template["unit_id"]),(22,1,2,19))
        caster=oracle.common.actor(self.source,1)
        self.assertEqual(caster[26:31],bytes((0,0,0,16,0)))
        self.assertEqual(struct.unpack_from("<8H",caster,64),(500,500,114,114,600,450,180,90))
        raw=oracle.common.crypt((self.original/"START.SAV").read_bytes())
        seed=oracle.common.actor(raw,1)
        for at in range(40,49):
            self.assertEqual(caster[at],seed[at])
        self.assertEqual(self.source[:oracle.UNITS],raw[:oracle.UNITS])
        self.assertTrue(all(oracle.common.actor(self.source,i)[5]&4 for i in range(4,12)))

    def test_real_fixed_slot_markers_and_bad_source(self):
        clip=self.manifest["resource_contract"]
        self.assertEqual((clip["entry"],clip["part_count"],clip["intro_end"]),(59,19,7))
        self.assertEqual(clip["cost_marker_parts"],[6])
        self.assertEqual(clip["effect_marker_parts"],list(range(7,15)))
        self.assertTrue(all(clip["parts"][i]["duration"]==2 for i in range(15)))
        data=bytearray((self.original/"FIGANI.DAT").read_bytes());data[-1]^=1
        with self.assertRaisesRegex(ValueError,"身份"):
            oracle.clip_contract(data)

    def test_independent_boundaries_and_one_reward_page_alternative(self):
        candidates,contract=self.candidates()
        first,last=contract["outcomes"][0],contract["outcomes"][-1]
        self.assertEqual((first["power"],first["raw_damage"],first["hp_after"],first["gain"],first["mp_after"]),(656,590,2210,16,92))
        self.assertEqual((last["raw_damage"],last["hp_after"],last["gain"]),(654,2146,18))
        self.assertEqual(first["marker_hp"],[2727,2653,2579,2505,2432,2358,2284,2210])
        self.assertEqual(len(candidates),65)
        self.assertEqual(contract["gains"],[16,17,18])
        self.assertTrue(all(p["fragment"]==488 and not p["requires_key"] and p["final_wait_ticks"]==11 for p in contract["reward_pages"]))

    def test_exact_native_cache_and_end_phase(self):
        for engine in ("original","sdl"):
            for after_end in (False,True):
                with self.subTest(engine=engine,after_end=after_end):
                    candidates,contract=self.candidates(engine,after_end)
                    self.assertTrue(self.check(candidates[0],engine,after_end)["ok"])
                    self.assertEqual(candidates[0][oracle.META],2 if after_end else 1)
                    self.assertEqual(candidates[0][oracle.UNITS+80+5]&128,0 if after_end else 128)
                    if engine=="original":
                        self.assertEqual([candidates[0][oracle.UNITS+i*80+2] for i in range(6)],[0,1,2,3,4,5])
                    self.assertEqual(contract["formal_save_available"],after_end)

    def test_hp_and_ex_must_share_one_spread_and_full_container(self):
        candidates,_=self.candidates(after_end=True)
        base=bytearray(candidates[0]);base[oracle.UNITS+80+60]=18
        self.assertFalse(self.check(oracle.common.checksum(base),after_end=True)["ok"])
        for at in (0,0x8a3,oracle.UNITS+80+68,oracle.UNITS+4*80+2,oracle.META,oracle.META+3,0x312b):
            changed=bytearray(candidates[0]);changed[at]^=1
            with self.subTest(offset=at):
                self.assertFalse(self.check(oracle.common.checksum(changed),after_end=True)["ok"])
        immediate,_=self.candidates()
        self.assertFalse(self.check(immediate[0],after_end=True)["ok"])

    def test_source_engine_checksum_and_phase_fail_closed(self):
        candidates,_=self.candidates()
        with self.guard():
            changed=bytearray(self.source);changed[oracle.UNITS+72]^=1
            with self.assertRaisesRegex(ValueError,"完整已登记"):
                oracle.expected_contract(self.original,oracle.common.checksum(changed))
            with self.assertRaises(ValueError):
                oracle.expected_sources(self.original,self.source,engine="other")
            with self.assertRaises(ValueError):
                oracle.expected_contract(self.original,self.source,after_end=1)
        with self.assertRaises(ValueError):
            self.check(candidates[0][:-4]+bytes(4))

    def test_signed_core_cap_and_marker_divisor_without_clamping_power(self):
        caster=bytearray(80);target=bytearray(80)
        caster[33]=20;target[33]=1
        struct.pack_into("<H",target,74,100)
        struct.pack_into("<HH",target,64,50,50)
        rows=oracle.derived_outcomes(caster,target,30,8,0)
        self.assertEqual((rows[0]["power"],rows[0]["raw_damage"],rows[0]["hp_after"]),(-100,-90,140))
        self.assertEqual((rows[-1]["raw_damage"],rows[-1]["hp_after"]),(-99,149))
        self.assertEqual(rows[0]["marker_hp"][0],61)  # -90/8朝零=-11。
        struct.pack_into("<H",caster,72,200)
        struct.pack_into("<H",target,74,0)
        rows=oracle.derived_outcomes(caster,target,30,8,0)
        self.assertGreater(rows[0]["raw_damage"],50)
        self.assertEqual((rows[0]["signed_capped_damage"],rows[0]["hp_after"]),(50,0))

    def test_start_zoc_does_not_hide_safe_one_step_resource_gate(self):
        from run_original_end_turn import round_assets
        assets=round_assets(self.original)
        with self.guard():
            probe=oracle.movement_probe(self.original,self.source,assets)
        self.assertEqual(probe["world"],(6,14))
        self.assertEqual(probe["actor_position"],(52,76))
        self.assertTrue(probe["movement_source"]["start_adjacent_enemy"])
        self.assertFalse(probe["movement_source"]["destination_zoc"])
        self.assertTrue(probe["ranges"])
        changed=bytearray(self.source);changed[oracle.UNITS+4*80:oracle.UNITS+4*80+2]=bytes((6,13))
        changed=oracle.common.checksum(changed)
        with mock.patch.object(oracle,"expected_contract",return_value={}):
            with self.assertRaisesRegex(ValueError,"ZOC"):
                oracle.movement_probe(self.original,changed,assets)


if __name__=="__main__":
    unittest.main()
