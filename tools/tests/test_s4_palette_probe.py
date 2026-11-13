"""真实Scene探针协议与完整原调色尾来源守卫；不创建窗口或读取游戏资源。"""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[2]/"tools/ida"))
import verify_s2_fades as oracle


class PaletteProbeTest(unittest.TestCase):
    def trace(self):
        return [{"op":"palette_subtract","args":[0,255,64],"status":0,"dac_hex":"00"*768},
                {"op":"delay_ms","args":[2],"status":0,"dac_hex":"00"*768},
                {"op":"palette_subtract","args":[0,255,0],"status":0,"dac_hex":"7d"*768},
                {"op":"delay_ms","args":[2],"status":0,"dac_hex":"7d"*768}]

    def test_fast_keeps_actual_DAC_sequence_and_existing_source_upload_domain(self):
        trace=self.trace();before=copy.deepcopy(trace)
        for kind in ("public_in","public_out","legacy_entry","gameover_in","gameover_out"):
            with self.subTest(kind=kind):
                row=oracle.scene_expected(kind,1,17,trace)
                self.assertEqual([r["op"] for r in row["trace"]],["palette_subtract"]*2)
                self.assertEqual(row["uploads"],0);self.assertEqual(row["presents"],0)
        row=oracle.scene_expected("legacy_in",1,17,trace)
        self.assertEqual(row["presents"],1)
        for kind in ("source_in","source_out"):
            self.assertEqual(oracle.scene_expected(kind,1,17,trace)["uploads"],2)
        self.assertEqual(oracle.scene_expected("public_in",0,17,trace)["trace"],trace)
        self.assertEqual(trace,before)
        for kind,fast in (("unknown",0),("transition",1),("public_in",2)):
            with self.assertRaises(ValueError):oracle.scene_expected(kind,fast,17,trace)

    def test_actual_scene_protocol_rejects_missing_tail_wrong_DAC_order_owner_and_bool_type(self):
        good=oracle.scene_expected("gameover_in",0,17,self.trace())
        variants=[]
        for key,value in (("status",-1),("owner_unchanged",False),("owner_unchanged",1),("uploads",1),("presents",1)):
            bad=copy.deepcopy(good);bad[key]=value;variants.append(bad)
        bad=copy.deepcopy(good);bad["trace"].pop();variants.append(bad)
        bad=copy.deepcopy(good);bad["trace"][0]["dac_hex"]="01"*768;variants.append(bad)
        bad=copy.deepcopy(good);bad["trace"][1]["args"]=[12];variants.append(bad)
        bad=copy.deepcopy(good);bad["trace"][0],bad["trace"][1]=bad["trace"][1],bad["trace"][0];variants.append(bad)
        bad=copy.deepcopy(good);bad["extra_unchecked"]=True;variants.append(bad)
        with tempfile.TemporaryDirectory() as directory:
            probe=Path(directory)/"probe.exe";probe.write_bytes(b"probe")
            with patch.object(oracle.subprocess,"run",return_value=SimpleNamespace(stdout=json.dumps(good))):
                result=oracle.compare_probe(probe,["gameover_in 0 17"],[good],"--s4-palette-vectors")
                self.assertEqual(result["vectors"],1)
            for bad in variants:
                with patch.object(oracle.subprocess,"run",return_value=SimpleNamespace(stdout=json.dumps(bad))):
                    with self.assertRaises(ValueError):oracle.compare_probe(probe,["gameover_in 0 17"],[good],"--s4-palette-vectors")
            for output in ("",json.dumps(good)+"\n"+json.dumps(good)):
                with patch.object(oracle.subprocess,"run",return_value=SimpleNamespace(stdout=output)):
                    with self.assertRaises(ValueError):oracle.compare_probe(probe,["gameover_in 0 17"],[good],"--s4-palette-vectors")

    def test_probe_identity_must_survive_execution(self):
        good=oracle.scene_expected("public_out",0,0,self.trace())
        with tempfile.TemporaryDirectory() as directory:
            probe=Path(directory)/"probe.exe";probe.write_bytes(b"before")
            def mutate(*args,**kwargs):
                probe.write_bytes(b"after");return SimpleNamespace(stdout=json.dumps(good))
            with patch.object(oracle.subprocess,"run",side_effect=mutate):
                with self.assertRaisesRegex(ValueError,"变化"):
                    oracle.compare_probe(probe,["public_out 0 0"],[good],"--s4-palette-vectors")

    def test_transition_owner_not_an_arbitrary_slice_or_adjacent_call(self):
        image=bytes(0x64000);start,end=oracle.TRANSITION[:2];a,b=oracle.ADDITIVE[:2]
        digest=hashlib.sha256(image[start:end]).hexdigest()
        row={"va":start,"end":end,"chunks":[{"start":start,"end":end,"raw_hex":image[start:end].hex(),"sha256":digest}],
             "blocks":[1],"callers":[1],"callees":[0x11df2,0x3790a],
             "instructions":[{"ea":value} for value in (start,oracle.TRANSITION_ENTRY,oracle.TRANSITION_STOP)]}
        source={"functions":[row]}
        with patch.object(oracle,"IMAGE_SHA",hashlib.sha256(image).hexdigest()),patch.object(oracle,"TRANSITION",(start,end,digest)),\
                patch.object(oracle,"ADDITIVE",(a,b,hashlib.sha256(image[a:b]).hexdigest())):
            oracle.validate_transition_source(image,source)
            for key in ("blocks","callers","callees","instructions","chunks"):
                bad=copy.deepcopy(source);bad["functions"][0][key]=[]
                with self.subTest(key=key),self.assertRaises(ValueError):oracle.validate_transition_source(image,bad)
            bad=copy.deepcopy(source);bad["functions"][0]["chunks"][0]["start"]=oracle.TRANSITION_ENTRY
            with self.assertRaises(ValueError):oracle.validate_transition_source(image,bad)
            bad=copy.deepcopy(source);bad["functions"].append(row)
            with self.assertRaises(ValueError):oracle.validate_transition_source(image,bad)
            with self.assertRaises(ValueError):oracle.validate_transition_source(image+b"x",source)


if __name__=="__main__":unittest.main()
