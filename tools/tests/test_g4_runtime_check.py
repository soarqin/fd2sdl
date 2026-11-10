"""G4运行验收工具的负门；不启动窗口，不算原版/SDL验收。"""
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest

from PIL import Image

TOOLS=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(TOOLS))
import run_g4_runtime_check as check

ROOT=TOOLS.parent


class RuntimeCheckTest(unittest.TestCase):
    def test_fixture_roundtrip_only_declared_business_bytes(self):
        original=ROOT/'original_game';source_bytes=(original/'START.SAV').read_bytes()
        before=check.decode_save(original/'START.SAV')
        plain,encoded,manifest=check.fixture(original)
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'fixture.SAV';path.write_bytes(encoded)
            self.assertEqual(check.decode_save(path),plain)
        allowed={check.UNITS+raw*80+5 for raw in range(12) if before[check.UNITS+raw*80+6]!=2}
        allowed.update(check.UNITS+raw*80+offset for raw in (0,1) for offset in (0x40,0x41))
        changed={i for i,(a,b) in enumerate(zip(before[:-4],plain[:-4])) if a!=b}
        self.assertTrue(changed and changed<=allowed)
        self.assertEqual(plain[:check.UNITS],before[:check.UNITS])
        self.assertEqual(len(manifest['recovery']),2)
        self.assertEqual((original/'START.SAV').read_bytes(),source_bytes)

    def test_round001_cannot_match002(self):
        one,two,_=check.frames(ROOT/'original_game')
        with tempfile.TemporaryDirectory() as directory:
            image=Image.new('RGB',(320,200));image.paste(one.convert('RGB'),(120,84))
            path=Path(directory)/'one.png';image.save(path)
            record={'frames':[{'path':str(path),'index':0,'sha256':check.sha(path),'timestamp_ms':0}]}
            check.banner_evidence(record,one,1)
            with self.assertRaises(AssertionError):check.banner_evidence(record,two,2)

    def test_original_path_rejected_before_write(self):
        original=(ROOT/'original_game').resolve();before=check.sha(original/'START.SAV')
        with self.assertRaises(ValueError):
            check.run(SimpleNamespace(root=ROOT,artifacts=original,run=False))
        self.assertEqual(check.sha(original/'START.SAV'),before)


if __name__=='__main__':unittest.main()
