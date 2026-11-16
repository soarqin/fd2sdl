"""Only bounded protocol/identity negatives; no C/original/resource execution."""
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zlib
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import verify_f4_physical_composition as p

class Protocol(unittest.TestCase):
    def test_exact_payload(self):
        self.assertEqual(p.unpack(zlib.compress(b'abc'),3),b'abc')
    def test_truncated(self):
        with self.assertRaises(ValueError):p.unpack(zlib.compress(b'abc')[:-1],3)
    def test_trailing(self):
        with self.assertRaises(ValueError):p.unpack(zlib.compress(b'abc')+b'x',3)
    def test_overrun(self):
        with self.assertRaises(ValueError):p.unpack(zlib.compress(b'abcd'),3)
    def test_short(self):
        with self.assertRaises(ValueError):p.unpack(zlib.compress(b'ab'),3)
    def test_frozen_single_read(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'catalog';path.write_bytes(b'{"key":1}\n');calls=[];read=Path.read_bytes
            def spy(current):calls.append(current);return read(current)
            with patch.object(Path,'read_bytes',spy):
                facts=[];raw=p.freeze(path,facts,p.sha(b'{"key":1}\n'))
                self.assertEqual(raw,b'{"key":1}\n');self.assertEqual(len(calls),1)
                self.assertEqual(len(facts),1)
    def test_wrong_pin(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'catalog';path.write_bytes(b'new');facts=[]
            with self.assertRaises(ValueError):p.freeze(path,facts,p.sha(b'old'))
            self.assertEqual(facts,[])
    def test_actual_original_output_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);target=root/'original_game'/'new'
            with self.assertRaises(ValueError):p.fresh(root,target)
            self.assertFalse(target.exists())

if __name__=='__main__':unittest.main()
