"""G2有限SAV门的合成负例；不是原版运行证据。"""
from pathlib import Path
import struct
import sys
import unittest
from unittest import mock

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import fd2_g2_battle_oracle as oracle


def fixture(magic_id=34):
    source=bytearray(oracle.SAVE_SIZE)
    source[oracle.META:oracle.META+10]=bytes((1,4,0,5,11,7,14,2,3,4))
    for raw,(identity,where) in enumerate(((0,(11,14)),(52,(7,14)),(4,(12,14)),(30,(2,21)))):
        at=oracle.UNITS+raw*80
        source[at:at+9]=bytes((*where,9,0,0,0,2,identity,9 if raw==1 else identity))
        source[at+32]=21 if raw==1 else 1
        source[at+33]=40 if raw==1 else 1
        struct.pack_into("<4H",source,at+64,42,42,1414 if raw==1 else 0,1414 if raw==1 else 0)
        struct.pack_into("<4H",source,at+72,16 if raw==0 else 25,12 if raw==0 else 5,97 if raw==0 else 92,2)
    source[oracle.UNITS+80+26+magic_id//8]=1<<(magic_id%8)
    return oracle.checksum(source)


def end_fixture():
    source=bytearray(fixture())
    source[oracle.META+1]=5
    at=oracle.UNITS+4*80
    source[at:at+9]=bytes((20,2,9,0,0,4,0,96,96))
    struct.pack_into("<2H",source,at+64,28,28)
    return oracle.checksum(source)


def teleport_fixture():
    source=bytearray(end_fixture())
    source[oracle.UNITS:oracle.UNITS+2]=bytes((9,14))
    source[oracle.UNITS+80+26:oracle.UNITS+80+31]=bytes((0,0,128,0,0))
    return oracle.checksum(source)


class G2BattleOracleTest(unittest.TestCase):
    def guard(self,source,scenario):
        manifest={"resource_contract":{"cost":28 if scenario=="composite34" else 20,
            "cost_count":int(scenario!="teleport23-cancel-destination"),"basis":"synthetic independent fixture"}}
        if scenario=="teleport23-success":
            manifest["teleport_contract"]={"normalized_gain":0,"destination":[10,14]}
        return mock.patch.object(oracle,"derive_fixture",return_value=(b"synthetic",source,
            manifest))

    def after(self,source,scenario,engine="sdl",*,after_end=False):
        query=oracle.expected_end_contract if after_end else oracle.expected_contract
        with self.guard(source,scenario):contract=query(None,source,scenario)
        result=bytearray(source)
        for row in contract["fixed_updates"]:
            data=bytes.fromhex(row["hex"]);result[row["offset"]:row["offset"]+len(data)]=data
        for row in contract["finite_bytes"]:result[row["offset"]]=3
        if engine=="original":
            for raw in range(source[oracle.META+1]):result[oracle.UNITS+raw*80+2]=raw
        return oracle.checksum(result)

    def test_direct_buff_not_x87_and_full_guard(self):
        source=fixture();after=self.after(source,"composite34")
        self.assertEqual(struct.unpack_from("<4H",after,oracle.UNITS+72),(19,14,112,17))
        self.assertEqual(struct.unpack_from("<H",after,oracle.UNITS+80+68),(1386,))
        with self.guard(source,"composite34"):
            result=oracle.check_result(None,source,"composite34",after,engine="sdl")
        self.assertTrue(result["ok"]);self.assertEqual(result["checked_container_bytes"],22987)
        self.assertEqual(result["finite_duration_bytes"],6)

    def test_no_arbitrary_observed_bytes_are_accepted(self):
        source=fixture();base=self.after(source,"composite34")
        for at in (0,0x8A3+123,oracle.UNITS+80+68,oracle.UNITS+10,oracle.UNITS+72,oracle.META,0x312B):
            changed=bytearray(base);changed[at]^=1;changed=oracle.checksum(changed)
            with self.subTest(offset=at),self.guard(source,"composite34"):
                self.assertFalse(oracle.check_result(None,source,"composite34",changed,engine="sdl")["ok"])

    def test_each_duration_has_its_own_finite_gate(self):
        source=fixture();base=self.after(source,"composite34")
        for raw in (0,2):
            for offset in (34,35,36):
                for value in (0,1,6,255):
                    changed=bytearray(base);changed[oracle.UNITS+raw*80+offset]=value
                    with self.subTest(raw=raw,offset=offset,value=value),self.guard(source,"composite34"):
                        self.assertFalse(oracle.check_result(None,source,"composite34",oracle.checksum(changed),engine="sdl")["ok"])

    def test_native_cache_is_computed_not_copied_from_measurement(self):
        source=fixture();after=self.after(source,"composite34","original")
        with self.guard(source,"composite34"):
            self.assertTrue(oracle.check_result(None,source,"composite34",after,engine="original")["ok"])
            changed=bytearray(after);changed[oracle.UNITS+80+2]=127
            self.assertFalse(oracle.check_result(None,source,"composite34",oracle.checksum(changed),engine="original")["ok"])

    def test_cancel_does_not_allow_mp_acted_status_or_position_writes(self):
        source=fixture(23);scenario="teleport23-cancel-destination";base=self.after(source,scenario)
        with self.guard(source,scenario):
            self.assertTrue(oracle.check_result(None,source,scenario,base,engine="sdl")["ok"])
            for at in (oracle.UNITS,oracle.UNITS+80+5,oracle.UNITS+80+68,oracle.UNITS+34):
                changed=bytearray(base);changed[at]^=1
                self.assertFalse(oracle.check_result(None,source,scenario,oracle.checksum(changed),engine="sdl")["ok"])

    def test_checksum_source_and_engine_fail_closed(self):
        source=fixture();after=self.after(source,"composite34")
        self.assertEqual(oracle.crypt(oracle.crypt(source)),source)
        for bad in (after[:-1],after[:-4]+bytes(4)):
            with self.assertRaises(ValueError):oracle.check_result(None,source,"composite34",bad,engine="sdl")
        with self.guard(source,"composite34"):
            with self.assertRaises(ValueError):oracle.check_result(None,source,"composite34",after,engine="other")
            altered=bytearray(source);altered[oracle.UNITS+72]+=1
            with self.assertRaisesRegex(ValueError,"完整已登记夹具"):
                oracle.expected_contract(None,oracle.checksum(altered),"composite34")

    def test_one_end_has_its_own_finite_gate_and_keeps_direct_stats(self):
        source=end_fixture();scenario="composite34"
        final=self.after(source,scenario,after_end=True)
        with self.guard(source,scenario):
            immediate=oracle.expected_contract(None,source,scenario)
            contract=oracle.expected_end_contract(None,source,scenario)
            self.assertFalse(immediate["formal_save_available"])
            self.assertTrue(contract["formal_save_available"])
            self.assertEqual(contract["measurement_phase"],"after_one_end")
            self.assertEqual(contract["expected_focus_before_save_navigation"],[11,14])
            self.assertTrue(all(row["values"]==[1,2,3,4] for row in contract["finite_bytes"]))
            self.assertTrue(oracle.check_result(None,source,scenario,final,engine="sdl",after_end=True)["ok"])
            self.assertFalse(oracle.check_result(None,source,scenario,final,engine="sdl")["ok"])
        self.assertEqual(final[oracle.META],2)
        self.assertEqual(final[oracle.UNITS+80+5],0)
        self.assertEqual(final[oracle.UNITS+4*80+5],4)
        self.assertEqual(struct.unpack_from("<4H",final,oracle.UNITS+72),(19,14,112,17))
        for at,value in ((oracle.META,1),(oracle.META,3),(oracle.UNITS+80+5,128),
                         (oracle.UNITS+34,0),(oracle.UNITS+34,5),(oracle.UNITS+72,16),
                         (oracle.UNITS+4*80+5,0),(oracle.UNITS+64,41)):
            changed=bytearray(final);changed[at]=value
            with self.subTest(offset=at,value=value),self.guard(source,scenario):
                self.assertFalse(oracle.check_result(None,source,scenario,oracle.checksum(changed),
                    engine="sdl",after_end=True)["ok"])

    def test_one_end_rejects_unmodelled_ai_recovery_timers_and_phase_events(self):
        source=end_fixture();scenario="composite34"
        for offset,value in ((oracle.UNITS+4*80+5,0),(oracle.UNITS+4*80+6,1),
                             (oracle.UNITS+4*80+5,5),(oracle.UNITS+5,1),
                             (oracle.UNITS+34,1),(oracle.UNITS+64,41),(oracle.META,2),
                             (3,1)):
            bad=bytearray(source);bad[offset]=value;bad=oracle.checksum(bad)
            with self.subTest(offset=offset,value=value),self.guard(bad,scenario):
                with self.assertRaises(ValueError):oracle.expected_end_contract(None,bad,scenario)
        source=fixture(23)
        with self.guard(source,"teleport23-cancel-destination"):
            with self.assertRaisesRegex(ValueError,"23取消不追加END"):
                oracle.expected_end_contract(None,source,"teleport23-cancel-destination")

    def test_original_range_cycle_uses_all_eleven_phases(self):
        sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"static_data"))
        import verify_unit_tables
        sequence=bytes([*range(11),*range(9,0,-1)])
        with mock.patch.object(Path,"read_bytes",return_value=b"source"), \
             mock.patch.object(oracle,"digest",return_value=oracle.EXE_SHA256), \
             mock.patch.object(verify_unit_tables,"source_range",return_value=sequence) as read:
            result=oracle.range_phase_contract(Path("unused"))
            self.assertEqual(result["sequence"],list(sequence))
            self.assertEqual(result["used_lut_frames"],list(range(11)))
            read.assert_called_once_with(b"source",0x51a97,20)
            read.return_value=bytes((0,1,4,5,8))
            with self.assertRaises(ValueError):oracle.range_phase_contract(Path("unused"))

    def test_teleport_success_has_distinct_immediate_and_end_contracts(self):
        source=teleport_fixture();scenario="teleport23-success"
        immediate=self.after(source,scenario);final=self.after(source,scenario,after_end=True)
        with self.guard(source,scenario):
            first=oracle.expected_contract(None,source,scenario)
            end=oracle.expected_end_contract(None,source,scenario)
            self.assertEqual(first["expected_focus_before_save_navigation"],[10,14])
            self.assertEqual(end["end_turn"]["focus_after_end"],[10,14])
            self.assertFalse(first["formal_save_available"]);self.assertTrue(end["formal_save_available"])
            self.assertEqual(first["finite_bytes"],[]);self.assertEqual(end["finite_bytes"],[])
            self.assertTrue(oracle.check_result(None,source,scenario,immediate,engine="sdl")["ok"])
            self.assertTrue(oracle.check_result(None,source,scenario,final,engine="sdl",after_end=True)["ok"])
            self.assertFalse(oracle.check_result(None,source,scenario,immediate,engine="sdl",after_end=True)["ok"])
        self.assertEqual(immediate[oracle.UNITS:oracle.UNITS+2],bytes((10,14)))
        self.assertEqual(immediate[oracle.UNITS+80+5],128)
        self.assertEqual(final[oracle.UNITS+80+5],0)
        self.assertEqual(struct.unpack_from("<H",final,oracle.UNITS+80+68),(1394,))
        self.assertEqual(final[oracle.UNITS+80+60],source[oracle.UNITS+80+60])

    def test_teleport_success_does_not_accept_cancel_intermediate_or_extra_writes(self):
        source=teleport_fixture();scenario="teleport23-success"
        final=self.after(source,scenario,after_end=True)
        for at,value in ((oracle.UNITS,9),(oracle.UNITS,255),(oracle.UNITS+1,255),
                         (oracle.UNITS+80+60,1),(oracle.UNITS+80+68,0),
                         (oracle.UNITS+10,255),(oracle.META,1),(0x8a3+42,1)):
            changed=bytearray(final);changed[at]=value
            with self.subTest(offset=at,value=value),self.guard(source,scenario):
                self.assertFalse(oracle.check_result(None,source,scenario,oracle.checksum(changed),
                    engine="sdl",after_end=True)["ok"])

    def test_actual_teleport_success_sources_and_empty_destination_probe(self):
        original=Path(__file__).resolve().parents[2]/"original_game"
        if not (original/"FD2.EXE").is_file():self.skipTest("无原资源，未执行真实传送来源正证")
        from run_original_end_turn import round_assets
        _,source,manifest=oracle.derive_fixture(original,"teleport23-success")
        transfer=manifest["teleport_contract"]
        self.assertEqual((transfer["raw_contribution"],transfer["caster_divisor"],transfer["normalized_gain"]),(10,70,0))
        self.assertEqual(manifest["resource_contract"]["cost"],20)
        self.assertEqual(manifest["resource_contract"]["cost_count"],1)
        self.assertEqual(manifest["derived_sha256"],oracle.derive_fixture(original,"teleport23-cancel-destination")[2]["derived_sha256"])
        probe=oracle.phase_probes(original,source,"teleport23-success",round_assets(original))
        destination=probe["destination_selected"]
        self.assertEqual(destination["world"],[10,14]);self.assertEqual(destination["position"],(124,76))
        self.assertEqual(destination["cursor_frame"],1)
        self.assertEqual(destination["composite"].size,(24,24))
        self.assertEqual(probe["original_range_cycle"]["used_lut_frames"],list(range(11)))
        changed=bytearray(source);changed[oracle.UNITS+4*80:oracle.UNITS+4*80+2]=bytes((10,14))
        with self.assertRaisesRegex(ValueError,"占用"):
            oracle.teleport_effect_contract(original,oracle.checksum(changed))


if __name__=="__main__":
    unittest.main()
