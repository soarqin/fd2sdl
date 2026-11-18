"""Fixed target producer gates with owned synthetic tlogs, never a compiler/PE."""
import copy
import importlib.util
from pathlib import Path
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('pcm_build', HERE / 'pcm_asset_build.py')
build = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(build)


class Producer(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.base = Path(self.temp.name)
        self.root, self.output = self.base / 'source', self.base / 'build'
        self.root.mkdir(); self.output.mkdir()
        def create(p, raw=b'owned fixture'):
            p.parent.mkdir(parents=True, exist_ok=True); p.write_bytes(raw); return p
        self.create = create
        for rel in build.MINIMUM:
            create(self.root / rel)
        create(self.root / 'src/CMakeLists.txt')
        self.log = create(self.base / 'build.log')
        self.exe = create(self.output / 'Debug' / (build.TARGET + '.exe'))
        self.clock = create(self.output / 'Debug/fd2_clock.lib')
        self.native = create(self.output / 'Debug/fd2_audio_native.lib')
        self.external = create(self.base / 'sdk/owned-external.lib')
        compiler = create(self.base / 'compiler/cl.exe')
        create(compiler.with_name('link.exe')); create(compiler.with_name('lib.exe'))
        create(self.output / 'CMakeFiles/1/CMakeCCompiler.cmake',
               ('set(CMAKE_C_COMPILER "'+compiler.as_posix()+'")').encode())
        create(self.output / 'CMakeCache.txt')
        self.logs = {}
        for target in (build.TARGET, 'fd2_clock', 'fd2_audio_native'):
            directory = self.output / (target + '.dir') / 'Debug' / 'owned.tlog'
            for name, content in {
                'CL.read.1.tlog': '\n'.join(str((self.root / rel).resolve()) for rel in sorted(build.MINIMUM)),
                'CL.command.1.tlog': '/c fixture',
                ('link.read.1.tlog' if target == build.TARGET else 'Lib-link.read.1.tlog'):
                    '\n'.join(str(p) for p in (self.clock, self.native, self.external)),
                ('link.command.1.tlog' if target == build.TARGET else 'Lib.command.1.tlog'): 'fixture /out',
            }.items():
                self.logs[target, name] = create(directory / name, content.encode('utf-16'))
        # Synthetic successful-build order; no compiler or executable is run.
        self.exe.write_bytes(b'owned fixture final PE')

    def tearDown(self):
        self.temp.cleanup()

    def collect(self):
        return build.evidence(self.root, self.output, 'Debug', self.log)

    def validate(self, value, portable=True):
        facts = []
        result = build.validate(value, self.exe, facts, build.freeze, self.root, portable)
        return result, facts

    def test_fixed_actual_closure_and_roles(self):
        m = self.collect(); result, facts = self.validate(m)
        self.assertEqual(m['actual_project_targets'], ['fd2_audio_native', 'fd2_clock', build.TARGET])
        self.assertEqual(m['actual_files'], len(m['files']))
        self.assertEqual(m['libraries'], 3)
        self.assertTrue(result['fresh_actual_build_closure'])
        self.assertFalse(result['compiler_consumed_hash_claim'])
        self.assertTrue({'build_log', 'tlog', 'static_library', 'producer_source', 'executable'} <=
                        {role for r in m['files'] for role in r['roles']})
        self.assertEqual(len(facts), len(m['files']))

    def test_empty_manifest_rejected(self):
        m = self.collect(); m['files'] = []
        with self.assertRaisesRegex(ValueError, 'empty producer'):
            self.validate(m)

    def test_duplicate_file_rejected(self):
        m = self.collect(); m['files'].append(m['files'][0])
        with self.assertRaisesRegex(ValueError, 'duplicate producer'):
            self.validate(m)

    def test_actual_file_count_rejected(self):
        m = self.collect(); m['actual_files'] += 1
        with self.assertRaisesRegex(ValueError, 'file count'):
            self.validate(m)

    def test_unbound_or_missing_read_tlog_rejected(self):
        for value in ([], [str(self.base / 'other.read.1.tlog')]):
            m = self.collect(); m['actual_read_tlogs'] = value
            with self.subTest(value=value), self.assertRaises(ValueError):
                self.validate(m)

    def test_wrong_target_or_root_rejected(self):
        for field, value in (('target', 'fd2sdl'), ('source_root', str(self.base))):
            m = self.collect(); m[field] = value
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, 'target/root'):
                self.validate(m)

    def test_roles_counts_and_target_list_cannot_self_certify(self):
        for field in ('libraries', 'actual_project_targets'):
            m = self.collect(); m[field] = 0 if field == 'libraries' else [build.TARGET]
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, 'actual dependency closure'):
                self.validate(m)
        m = self.collect(); m['files'][0]['roles'] = ['made-up']
        with self.assertRaisesRegex(ValueError, 'actual dependency closure'):
            self.validate(m)

    def test_direct_source_minimum_missing_rejected(self):
        for p in self.logs.values():
            if p.name == 'CL.read.1.tlog':
                text = p.read_text(encoding='utf-16').replace(str(self.root / 'src/audio_pcm.h'), '')
                p.write_text(text, encoding='utf-16')
        with self.assertRaisesRegex(ValueError, 'direct actual source'):
            self.collect()

    def test_missing_link_or_library_command_log_rejected(self):
        for target, name in ((build.TARGET, 'link.command.1.tlog'), ('fd2_clock', 'Lib.command.1.tlog')):
            p = self.logs[target, name]; saved = p.read_bytes(); p.unlink()
            with self.subTest(target=target), self.assertRaisesRegex(ValueError, 'CL/LINK/Lib'):
                self.collect()
            p.write_bytes(saved)

    def test_changed_source_and_binary_rejected(self):
        import os
        for p in (self.root / 'src/audio_dig.c', self.exe):
            m = self.collect(); old = p.read_bytes(); stamp = p.stat(); p.write_bytes(b'changed')
            with self.subTest(path=p), self.assertRaisesRegex(ValueError, 'artifact changed'):
                self.validate(m)
            p.write_bytes(old)
            os.utime(p, ns=(stamp.st_atime_ns, stamp.st_mtime_ns))

    def test_swapped_executable_metadata_rejected(self):
        m = self.collect(); m['exe'] = build.freeze(self.clock)[1]
        with self.assertRaisesRegex(ValueError, 'actual dependency closure|PE producer'):
            self.validate(m)

    def test_actual_input_missing_is_not_silently_dropped(self):
        p = self.logs[build.TARGET, 'CL.read.1.tlog']
        p.write_text(str(self.base / 'absent.c'), encoding='utf-16')
        with self.assertRaisesRegex(ValueError, 'missing/nonabsolute'):
            self.collect()

    def test_source_or_linked_archive_newer_than_PE_is_stale(self):
        import os
        for p in (self.root / 'src/audio_dig.h', self.native):
            old = p.stat()
            os.utime(p, ns=(old.st_atime_ns, self.exe.stat().st_mtime_ns + 1000000))
            with self.subTest(path=p), self.assertRaisesRegex(ValueError, 'stale producer'):
                self.collect()
            os.utime(p, ns=(old.st_atime_ns, old.st_mtime_ns))

    def test_mixed_case_deps_library_is_external_not_project_target(self):
        sdk = self.create(self.output / '_DePs/sdk/Debug/SDL3-static.lib')
        for p in self.logs.values():
            if '.read.' in p.name:
                p.write_text(p.read_text(encoding='utf-16').replace(str(self.external), str(sdk)), encoding='utf-16')
        self.exe.write_bytes(b'completed owned fixture after SDK')
        m = self.collect(); self.validate(m)
        self.assertEqual(m['actual_project_targets'], ['fd2_audio_native', 'fd2_clock', build.TARGET])

    def test_legacy_replay_remains_distinct(self):
        m = self.collect(); m['schema'] = 'fd2.f5.pcm-assets-build-inputs.v1'
        m['files'] = [build.facts_only(r) for r in m['files']]
        result, _ = self.validate(m, portable=False)
        self.assertEqual(result, dict(mode='legacy-v1-replay', fresh_actual_build_closure=False))
        with self.assertRaisesRegex(ValueError, 'requires producer v2'):
            self.validate(m)

    def test_actual_original_junction_is_protected_before_manifest_creation(self):
        import os
        original = self.base / 'actual-owned-original'; original.mkdir()
        alias = self.root / 'original_game'
        if os.name == 'nt':
            import _winapi
            _winapi.CreateJunction(str(original), str(alias))
        else:
            alias.symlink_to(original, target_is_directory=True)
        for p in (alias / 'new-manifest.json', original / 'direct-manifest.json'):
            with self.subTest(path=p), self.assertRaisesRegex(ValueError, 'resolved original_game'):
                build.output_path(p, self.root)
        self.assertEqual(list(original.iterdir()), [])

    def test_fixed_tool_original_protects_wrong_declared_root(self):
        from unittest.mock import patch
        tool = self.base / 'owned-tool'; original = tool / 'original_game'; original.mkdir(parents=True)
        with patch.object(build, '__file__', str(tool / 'tools/testing/pcm_asset_build.py')):
            with self.assertRaisesRegex(ValueError, 'resolved original_game'):
                build.output_path(original / 'new-manifest.json', self.root)
        self.assertEqual(list(original.iterdir()), [])

    def test_existing_manifest_still_rejected_and_fresh_nonoriginal_allowed(self):
        existing = self.create(self.base / 'existing.json')
        with self.assertRaisesRegex(ValueError, 'fresh'):
            build.output_path(existing, self.root)
        fresh = self.base / 'new-manifest.json'
        self.assertEqual(build.output_path(fresh, self.root), fresh.resolve())
        self.assertFalse(fresh.exists())


if __name__ == '__main__':
    unittest.main()
