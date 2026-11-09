"""恢复原资源正证／真实PNG与WAV身份门；不创建窗口。"""
import hashlib
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock,patch
from types import SimpleNamespace
import wave

import numpy as np
from PIL import Image
from scipy import signal

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"tools"))
import fd2_g2_map_action_oracle as oracle
import run_g2_map_action_check as runner
from fd2_g2_battle_oracle import checksum
from fd2_sfx_oracle import references


class RecoveryOracle(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.prepared=oracle.prepare(ROOT/"original_game")

    def rows(self,directory,families,*,background=True):
        rows=[]
        for i,family in enumerate(families):
            frame=Image.new("RGB",(320,200))
            if background:
                tile=self.prepared["recovery_resources"]["background"]
                frame.paste(tile["image"],tile["at"],tile["mask"])
            anchor=self.prepared["recovery_resources"][family][0]
            frame.paste(anchor["image"],anchor["at"],anchor["mask"])
            path=directory/f"frame{i:05d}.png";frame.save(path)
            rows.append({"path":str(path),"index":i,"sha256":hashlib.sha256(path.read_bytes()).hexdigest(),"wgc_timestamp_100ns":10000*(i+1)})
        return rows

    def test_original_recovery_source(self):
        plan=self.prepared["recovery_plan"]
        self.assertEqual(plan["sfx"]["bank"],31)
        self.assertEqual(plan["sfx"]["cue"],4)
        self.assertEqual(plan["sfx"]["candidate_count"],13)
        self.assertEqual(plan["normal_frame_ids"],[108,109,110])
        self.assertEqual(plan["opaque_pixels"],[403,398,401])
        bg=plan["required_background_on_solid_and_normal_after"]
        self.assertEqual((bg["world"],bg["screen"],bg["tile"],bg["attributes_hex"],bg["checked_pixels"]),
            ([5,18],[4,172],156,"00000400",576))

    def test_three_actual_png_stages_and_order(self):
        with tempfile.TemporaryDirectory() as d:
            rows=self.rows(Path(d),["normal","solid","normal"])
            self.assertTrue(oracle.visual_gate(rows,self.prepared["recovery_resources"])["ok"])
            wrong=self.rows(Path(d),["solid","normal","normal"])
            self.assertFalse(oracle.visual_gate(wrong,self.prepared["recovery_resources"])["ok"])

    def test_actor_only_or_covered_background_is_not_complete_recovery(self):
        with tempfile.TemporaryDirectory() as d:
            rows=self.rows(Path(d),["normal","solid","normal"],background=False)
            self.assertFalse(oracle.visual_gate(rows,self.prepared["recovery_resources"])["ok"])
            rows=self.rows(Path(d),["normal","solid","normal"])
            for row in rows[1:]:
                with Image.open(row["path"]) as image:
                    image.load();image.paste((64,64,64),(4,172,28,196));image.save(row["path"])
                row["sha256"]=hashlib.sha256(Path(row["path"]).read_bytes()).hexdigest()
            self.assertFalse(oracle.visual_gate(rows,self.prepared["recovery_resources"])["ok"])

    def test_png_identity_and_missing_solid_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            rows=self.rows(Path(d),["normal","normal","normal"])
            self.assertFalse(oracle.visual_gate(rows,self.prepared["recovery_resources"])["ok"])
            Path(rows[1]["path"]).write_bytes(b"changed PNG")
            with self.assertRaisesRegex(AssertionError,"PNG身份"):
                oracle.visual_gate(rows,self.prepared["recovery_resources"])

    def wav(self,path,sample):
        ref=references(ROOT/"original_game/FDOTHER.DAT",[{"bank":31,"sample":sample}])[0]
        values=signal.resample_poly(ref["values"],4,1)
        values=np.concatenate((np.zeros(5000),values,np.zeros(5000)))
        pcm=np.rint(np.clip(values,-1,.9999)*32768).astype("<i2")
        with wave.open(str(path),"wb") as w:
            w.setnchannels(2);w.setsampwidth(2);w.setframerate(44100)
            w.writeframes(np.column_stack((pcm,pcm)).tobytes())
        return hashlib.sha256(path.read_bytes()).hexdigest()

    def test_voice4_positive_other_voice_negative_and_tamper(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/"capture.wav";digest=self.wav(path,4)
            self.assertTrue(oracle.audio_gate(ROOT/"original_game",path,digest)["ok"])
            wrong=self.wav(path,3)
            self.assertFalse(oracle.audio_gate(ROOT/"original_game",path,wrong)["ok"])
            with self.assertRaisesRegex(AssertionError,"WAV.*身份"):
                oracle.audio_gate(ROOT/"original_game",path,digest)

    def measured(self,engine):
        p=self.prepared
        expected,_=oracle.battle.oracle.project_final(p,p["base_final"])
        expected=bytearray(expected)
        expected[oracle.battle.META+5:oracle.battle.META+9]=oracle.measuring_focus(expected,[("LEFT",oracle.screen(expected,(6,14)))])
        classes=[]
        for raw in range(expected[oracle.battle.META+1]):
            at=oracle.battle.UNITS+raw*80
            identity=expected[at+7]
            if identity not in classes:classes.append(identity)
            expected[at+2]=classes.index(identity) if engine=="original" else p["source"][at+2]
        return checksum(expected)

    def test_full_save_cache_and_unknown_bytes(self):
        for engine in ("original","sdl"):
            measured=self.measured(engine)
            self.assertTrue(oracle.save_gate(self.prepared,measured,engine)["ok"])
            bad=bytearray(measured);bad[oracle.battle.UNITS+2]^=1
            with self.assertRaisesRegex(AssertionError,"cache"):
                oracle.save_gate(self.prepared,checksum(bad),engine)
            bad=bytearray(measured);bad[oracle.battle.UNITS+79]^=1
            with self.assertRaisesRegex(AssertionError,"22987B"):
                oracle.save_gate(self.prepared,checksum(bad),engine)
        with self.assertRaises(ValueError):oracle.save_gate(self.prepared,measured,"other")

    def test_real_measure_helper_consumes_source_guard_at_save_boundary(self):
        # 仅替换WGC等待；真实B6 measure→库存Save helper→SAV writer完成门、
        # 有限整容器检查、快照和更新后的guard均实际执行，不伪造measure返回值。
        p=self.prepared;measured=self.measured("sdl")
        encoded=bytes(a^b^c for a,b,c in zip(p["encoded"],p["source"],measured))
        with tempfile.TemporaryDirectory() as directory:
            directory=Path(directory);save=directory/"FD2.SAV";save.write_bytes(p["encoded"])
            original_stamp=save.stat().st_mtime_ns;guard=[p["encoded"],original_stamp]
            keys=[];waits=[]
            def key(value):
                keys.append(value)
                if value=="ENTER" and keys.count("ENTER")==4:
                    save.write_bytes(encoded)
                    os.utime(save,ns=(original_stamp+1_000_000,original_stamp+1_000_000))
            capture=SimpleNamespace(artifacts=directory,key=key)
            record={"kind":"sdl","checkpoints":[]}
            def wait(capture,name,predicate,rows,**options):
                self.assertNotIn("source_guard",options)
                waits.append(name)
                return None,{"ok":True}
            checkpoint=runner.make_checkpoint(capture,save,guard,record)
            with patch.object(runner,"wait_state",side_effect=wait):
                oracle.battle.measure(capture,p,save,guard,record,checkpoint)
            self.assertTrue(record["business_sequence_completed"])
            self.assertTrue(record["ai11_action_business_evidence"]["passed"])
            self.assertEqual(guard,[encoded,save.stat().st_mtime_ns])
            self.assertEqual(waits[-1],"b6-final-saved-browse")
            self.assertEqual(Path(record["completed_save"]["snapshot"]).read_bytes(),encoded)

    def test_capture_interval_optional_default_and_source_timestamp_receipt(self):
        collectors=oracle.battle
        self.assertEqual(collectors.CAPTURE_INTERVAL_MS,33)
        self.assertEqual(collectors.animation_capture_interval(SimpleNamespace()),33)
        for interval in (None,16,33,67):
            args=SimpleNamespace(driver=Path("owned-driver"))
            if interval is not None:args.animation_capture_interval_ms=interval
            with tempfile.TemporaryDirectory() as d:
                capture=SimpleNamespace(artifacts=Path(d),process=SimpleNamespace(pid=42,poll=lambda:None))
                record={"kind":"original"}
                with patch.object(collectors,"OwnedHelper") as helper,patch.object(collectors,"wait_ready",return_value={"pid":42,"hwnd":"0x11"}),patch.object(collectors,"audio_toggle"):
                    collector=collectors.AnimationCollectors(capture,args,record);collector.start()
                self.assertEqual(helper.call_args.args[1][-1],33 if interval is None else interval)
                self.assertEqual(record["animation_capture"]["requested_interval_ms"],33 if interval is None else interval)
        frames=[{"wgc_timestamp_100ns":stamp} for stamp in (100,667100,1334100)]
        actual=collectors.observed_series_intervals(frames)
        self.assertEqual(actual["median_ms"],66.7)
        self.assertEqual(actual["count"],2)
        self.assertNotEqual(actual["median_ms"],16)

    def test_invalid_interval_rejected_before_helper_or_audio(self):
        collectors=oracle.battle
        for value in (15,68,0,-1,True,None,"16",16.0):
            args=SimpleNamespace(driver=Path("owned-driver"),animation_capture_interval_ms=value)
            with tempfile.TemporaryDirectory() as d:
                capture=SimpleNamespace(artifacts=Path(d),process=SimpleNamespace(pid=42,poll=lambda:None))
                record={"kind":"original"}
                with patch.object(collectors,"OwnedHelper") as helper,patch.object(collectors,"audio_toggle") as audio:
                    with self.assertRaises(ValueError):collectors.AnimationCollectors(capture,args,record).start()
                helper.assert_not_called();audio.assert_not_called()
                self.assertNotIn("series_helper",record)
        for frames in ([],[{"wgc_timestamp_100ns":1}],[{"wgc_timestamp_100ns":2},{"wgc_timestamp_100ns":2}]):
            with self.assertRaises(ValueError):collectors.observed_series_intervals(frames)


if __name__=="__main__":unittest.main()
