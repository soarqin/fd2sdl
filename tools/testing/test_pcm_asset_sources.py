"""Portable source gates only; synthetic bytes, no C/IDA/guest."""
import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('pcm_sources', HERE / 'pcm_asset_sources.py')
source = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(source)
sha = lambda raw: hashlib.sha256(raw).hexdigest()


class Sources(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.exe = self.root / 'FD2.EXE'; self.exe.write_bytes(b'owned EXE fixture')
        self.image = b'x' * 300
        self.catalog = dict(sources={'strict_image': sha(self.image),
                            **{'pcm-contract-source0'+str(i): sha(bytes([i])) for i in (1, 2, 3)}},
                            chunks=[dict(start=i*10, end=(i+1)*10, sha256=sha(b'x'*10)) for i in range(30)])
        self.profile = dict(schema='fd2.f5.pcm-source-profile.v1',
            original_exe=dict(size=self.exe.stat().st_size, sha256=sha(self.exe.read_bytes())),
            strict_image=dict(size=300, sha256=sha(self.image)),
            inherited_semantics=[dict(id=k, sha256=v) for k, v in self.catalog['sources'].items() if k != 'strict_image'])

    def tearDown(self):
        self.temp.cleanup()

    def invoke(self, change=None, digest=None):
        if change:
            change()
        p = self.root / 'profile.json'; p.write_text(json.dumps(self.profile), encoding='utf-8')
        facts = []
        def freeze(path):
            path = Path(path).resolve(); raw = path.read_bytes(); s = path.stat()
            return raw, dict(path=str(path), size=len(raw), mtime_ns=s.st_mtime_ns, sha256=sha(raw))
        with patch.object(source, 'image_from_exe', return_value=(self.image, dict(unique_sources=7948, duplicate_records=11))) as image:
            self.last_image_call = image
            result = source.portable(p, digest or sha(p.read_bytes()), self.exe, self.catalog, self.root, freeze, facts)
        return result, facts, image

    def test_fresh_bytes_does_not_claim_fresh_semantics(self):
        result, facts, image = self.invoke()
        self.assertTrue(result['fresh_original_bytes'])
        self.assertEqual(result['fresh_chunk_count'], 30)
        self.assertFalse(result['fresh_semantic_or_IDA_review'])
        self.assertEqual(result['semantic_review'], 'inherited-qualified-source')
        self.assertFalse(result['runtime_or_BSS_qualification'])
        self.assertEqual(len(facts), 2)
        image.assert_called_once()

    def test_no_legacy_export_file_is_needed(self):
        result, facts, _ = self.invoke()
        self.assertEqual({Path(f['path']).name for f in facts}, {'profile.json', 'FD2.EXE'})
        self.assertEqual(len(result['inherited_semantics']), 3)

    def test_profile_pin(self):
        with self.assertRaisesRegex(ValueError, 'profile pin'):
            self.invoke(digest='0'*64)

    def test_original_pin_before_decoder(self):
        with self.assertRaisesRegex(ValueError, 'EXE identity'):
            self.invoke(lambda: self.profile['original_exe'].update(sha256='0'*64))
        self.last_image_call.assert_not_called()

    def test_renamed_semantic_role_rejected(self):
        with self.assertRaisesRegex(ValueError, 'semantic source identities'):
            self.invoke(lambda: self.profile['inherited_semantics'][0].update(id='wrong'))

    def test_duplicate_semantic_role_rejected(self):
        with self.assertRaisesRegex(ValueError, 'role count/duplicate'):
            self.invoke(lambda: self.profile['inherited_semantics'].append(self.profile['inherited_semantics'][0]))

    def test_wrong_full_image_rejected(self):
        with self.assertRaisesRegex(ValueError, 'strict image identity'):
            self.invoke(lambda: self.profile['strict_image'].update(size=301))

    def test_chunk_wrong_missing_or_duplicate_rejected(self):
        for mutation, message in ((lambda c: c[0].update(sha256='0'*64), 'strict source chunk'),
                                  (lambda c: c.pop(), 'denominator'),
                                  (lambda c: c.__setitem__(1, dict(c[0])), 'range/duplicate'),
                                  (lambda c: c[0].update(start=-1), 'range/duplicate')):
            with self.subTest(message=message):
                old = json.loads(json.dumps(self.catalog['chunks']))
                mutation(self.catalog['chunks'])
                with self.assertRaisesRegex(ValueError, message):
                    self.invoke()
                self.catalog['chunks'] = old


if __name__ == '__main__':
    unittest.main()
