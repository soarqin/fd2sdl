"""S2视觉叶工具的完整来源、尾／scratch及真实C协议守卫；不需原资源。"""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"tools/ida"))
import s2_visual_source as source
import verify_s2_visual as oracle


class VisualSourceTest(unittest.TestCase):
    def fixture(self):
        image=bytearray(0x64000);image[source.TAIL[0]:source.TAIL[1]]=source.TAIL_RAW;image=bytes(image)
        chunks={va:tuple((a,b,hashlib.sha256(image[a:b]).hexdigest()) for a,b,_ in spans)
                for va,spans in source.CHUNKS.items()}
        rows=[{"va":va,"chunks":[{"start":a,"end":b,"raw_hex":image[a:b].hex(),"sha256":sha}
               for a,b,sha in spans],"instructions":[1],"blocks":[1],"callers":[]} for va,spans in chunks.items()]
        sha=hashlib.sha256(image).hexdigest()
        return image,{"schema":"fd2-s2-visual-domain-v1","image_sha256":sha,"functions":rows,
            "palette_shared_return":{"start":source.TAIL[0],"end":source.TAIL[1],"raw_hex":source.TAIL_RAW.hex(),"instructions":[1]},
            "tick_scratch_xrefs":[{"from":va,"type":kind} for va,kind in
                                  ((0x17abf,"WRITE"),(0x17acc,"READ"),(0x17ae7,"WRITE"))]},sha,chunks

    def test_complete_source_requires_real_shared_tail_and_local_scratch(self):
        image,report,sha,chunks=self.fixture()
        with patch.object(source,"RELOCATED_SHA256",sha),patch.object(source,"CHUNKS",chunks):
            self.assertEqual(len(source.validate_source(image,report)),4)
            for key in ("palette_shared_return","tick_scratch_xrefs"):
                bad=copy.deepcopy(report);bad.pop(key)
                with self.assertRaises(ValueError):source.validate_source(image,bad)
            bad=copy.deepcopy(report);bad["functions"][2]["chunks"].pop()
            with self.assertRaises(ValueError):source.validate_source(image,bad)
            bad=copy.deepcopy(report);bad["tick_scratch_xrefs"].append({"from":0x20000,"type":"READ"})
            with self.assertRaises(ValueError):source.validate_source(image,bad)
            with self.assertRaises(ValueError):source.validate_source(image+b"x",report)

    def test_all_base_values_and_raw_domains_are_explicit(self):
        rows=list(oracle.palette_cases())
        self.assertEqual(len(rows),335)
        self.assertEqual({seed for _,_,_,seed in rows},set(range(64)))
        self.assertEqual({add for _,_,add,_ in rows},{0,1,32,63,255})
        self.assertTrue(any(first>last for first,last,_,_ in rows))
        self.assertEqual(len(oracle.TICK_CASES),11)
        self.assertTrue({-2147483648,-1,0,65535}<={r[1] for r in oracle.TICK_CASES})
        self.assertTrue(any(r[2]==0x7fff and r[3][0]==0x8000 for r in oracle.TICK_CASES))
        self.assertTrue(any(r[2]==0xffff and r[3][0]==0 for r in oracle.TICK_CASES))

    def test_c_diff_rejects_mutated_binary_missing_rows_and_bad_owner(self):
        expected=[{"kind":"palette","status":0,"dac_hex":"a5","other_unchanged":True}]
        good=json.dumps(expected[0])+"\n"
        with tempfile.TemporaryDirectory() as directory:
            probe=Path(directory)/"probe.exe";probe.write_bytes(b"probe")
            with patch.object(oracle.subprocess,"run",return_value=SimpleNamespace(stdout=good)):
                identity=oracle.compare_probe(probe,["palette 5 5 0 0"],expected)
                self.assertEqual(identity["before_sha256"],identity["after_sha256"])
            for result in ("",good+good,json.dumps(dict(expected[0],other_unchanged=False))):
                with patch.object(oracle.subprocess,"run",return_value=SimpleNamespace(stdout=result)):
                    with self.assertRaises(ValueError):oracle.compare_probe(probe,["palette 5 5 0 0"],expected)
            def changed(*args,**kwargs):
                probe.write_bytes(b"changed");return SimpleNamespace(stdout=good)
            with patch.object(oracle.subprocess,"run",side_effect=changed):
                with self.assertRaisesRegex(ValueError,"changed"):oracle.compare_probe(probe,["palette 5 5 0 0"],expected)

    def test_c_tick_completion_and_clock_consumption_are_compared(self):
        row={"kind":"ticks","status":0,"clock_reads":3,"frames":0,"owner_unchanged":True,"script_exact":True}
        with tempfile.TemporaryDirectory() as directory:
            probe=Path(directory)/"probe.exe";probe.write_bytes(b"probe")
            for key,value in (("clock_reads",2),("frames",1),("script_exact",False),("status",-2)):
                bad=dict(row);bad[key]=value
                with patch.object(oracle.subprocess,"run",return_value=SimpleNamespace(stdout=json.dumps(bad))):
                    with self.subTest(key=key),self.assertRaises(ValueError):oracle.compare_probe(probe,["ticks 0 100 1 100 101"],[row])

    def test_new_operation_abi_keeps_signed_goal_and_additive_domain(self):
        doc=json.loads((ROOT/"data/static/flows/common_event.json").read_text(encoding="utf-8"))
        self.assertEqual([a["max"] for a in doc["operations"]["palette_add"]["args"]],[255,255,255])
        self.assertEqual(doc["operations"]["delay_ticks"]["args"],[{"name":"ticks","min":-2147483648,"max":65535}])


if __name__=="__main__":unittest.main()
