"""Synthetic expected writer boundaries only; no game/device/original execution."""
from pathlib import Path
import importlib.util
import tempfile
import unittest
from unittest.mock import patch

source=Path(__file__).resolve().parents[1]/'testing/sb_channel_vectors.py'
spec=importlib.util.spec_from_file_location('sb_vector_oracle',source)
oracle=importlib.util.module_from_spec(spec)
spec.loader.exec_module(oracle)

class OutputBoundaries(unittest.TestCase):
    def test_new_identical_and_different_expected(self):
        with tempfile.TemporaryDirectory() as directory:
            target=Path(directory)/'new.h'
            oracle.new_or_identical(target,b'owned')
            before=target.stat().st_mtime_ns
            oracle.new_or_identical(target,b'owned')
            self.assertEqual(target.stat().st_mtime_ns,before)
            with self.assertRaises(FileExistsError):oracle.new_or_identical(target,b'other')
            self.assertEqual(target.read_bytes(),b'owned')
            self.assertEqual(target.stat().st_mtime_ns,before)

    def test_original_directory_and_resolved_alias_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            original=Path(directory)/'Original_Game'
            original.mkdir()
            output=original/'new.h'
            with self.assertRaises(ValueError):oracle.new_or_identical(output,b'never')
            self.assertFalse(output.exists())
            # API-resolution boundary; not a fresh physical Windows junction test.
            with patch.object(Path,'resolve',return_value=output):
                with self.assertRaises(ValueError):oracle.new_or_identical(Path(directory)/'alias.h',b'never')
            self.assertFalse(output.exists())

    def test_two_outputs_cannot_overwrite_each_other(self):
        with tempfile.TemporaryDirectory() as directory:
            output=Path(directory)/'same.h'
            with patch('sys.argv',['vectors','--header',str(output),'--json',str(output)]):
                with self.assertRaises(ValueError):oracle.main()
            self.assertFalse(output.exists())

if __name__=='__main__':unittest.main()
