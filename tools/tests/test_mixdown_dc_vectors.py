"""Output/protocol checks only; no original/C/device run or numeric requalification."""
from pathlib import Path
import importlib.util,sys,tempfile,unittest
from unittest.mock import patch
TESTING=Path(__file__).resolve().parents[1]/'testing'
spec=importlib.util.spec_from_file_location('mixdown_dc_vectors',TESTING/'mixdown_dc_vectors.py')
vectors=importlib.util.module_from_spec(spec);spec.loader.exec_module(vectors)

class OutputGuards(unittest.TestCase):
    def test_new_and_identical_do_not_replace(self):
        with tempfile.TemporaryDirectory(prefix='fd2-mixdown-')as d:
            root=vectors.project_root();build=Path(d)
            out=vectors.checked_output(root,build,build/'mixdown_dc_vectors.bin')
            self.assertTrue(vectors.new_or_identical(out,b'fixture'))
            old=out.stat().st_mtime_ns
            self.assertFalse(vectors.new_or_identical(out,b'fixture'))
            self.assertEqual(out.stat().st_mtime_ns,old)
            with self.assertRaises(FileExistsError):vectors.new_or_identical(out,b'changed')
            self.assertEqual(out.read_bytes(),b'fixture')
    def test_wrong_name_escape_source_and_root(self):
        root=vectors.project_root()
        with tempfile.TemporaryDirectory(prefix='fd2-mixdown-')as d:
            build=Path(d)
            for output in(build/'other.bin',build.parent/'mixdown_dc_vectors.bin',root/'tools/mixdown_dc_vectors.bin'):
                with self.assertRaises(ValueError):vectors.checked_output(root,build,output)
            with self.assertRaises(ValueError):vectors.checked_output(root,root,root/'mixdown_dc_vectors.bin')
            with self.assertRaises(ValueError):vectors.checked_output(build,build,build/'mixdown_dc_vectors.bin')
    def test_resolved_original_alias_is_protected(self):
        # Controlled resolver seam tests exact exclusion. Not a real Windows
        # junction, privilege operation, or write into any original directory.
        root=vectors.project_root()
        with tempfile.TemporaryDirectory(prefix='fd2-mixdown-')as d:
            build=Path(d).resolve()
            with patch.object(vectors,'original_target',return_value=build):
                with self.assertRaises(ValueError):vectors.checked_output(root,build,build/'mixdown_dc_vectors.bin')
            with self.assertRaises(ValueError):
                vectors.checked_output(root,build/'original_game',build/'original_game/mixdown_dc_vectors.bin')
    def test_no_original_source_or_C_runtime_dependency(self):
        raw=(TESTING/'mixdown_dc_vectors.py').read_text(encoding='utf8')
        for forbidden in('F:/','SB16.DIG','FD2.EXE','subprocess','ctypes','unicorn'):
            self.assertNotIn(forbidden,raw)

if __name__=='__main__':unittest.main(verbosity=2)
