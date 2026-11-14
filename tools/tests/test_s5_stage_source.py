"""S5扩展来源拒绝门：保S2基线、完整共享chunk、实际入口及逐字节身份。"""
import copy
import hashlib
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'static_data'))
import convert_stage_transitions as source


def function(image, start, end):
    raw = image[start:end]
    return {'va': start, 'chunks': [{'start': start, 'end': end, 'raw_hex': raw.hex(),
            'sha256': hashlib.sha256(raw).hexdigest()}],
            'blocks': [{'start': start, 'end': end, 'successors': []}],
            'instructions': [{'ea': va, 'size': 1, 'raw_hex': image[va:va + 1].hex(),
                              'disasm': 'retn' if va == end - 1 else 'nop'} for va in range(start, end)],
            'xrefs': []}


def extension(functions):
    return {'schema': 'fd2-s5-victory-domain-v1', 'image_sha256': source.IMAGE_SHA256,
            'database_saved': False, 'strict_byte_match': True,
            'S2_readset': {'sha256': source.S2_DOMAIN_SHA256}, 'functions': functions}


class S5SourceExtensionTests(unittest.TestCase):
    def setUp(self):
        self.image = bytes(range(64))
        self.base_function = function(self.image, 16, 19)
        self.new_function = function(self.image, 24, 27)
        self.base = {'functions': [self.base_function]}

    def merge(self, additions):
        instructions = {row['ea']: row for row in self.base_function['instructions']}
        with patch.object(source, 'source_index', return_value=(instructions, {})) as gate:
            result = source.extend_source_index(self.base, self.image, additions)
        gate.assert_called_once_with(self.base, self.image)
        return result

    def test_S2_original_gate_executes_first_and_cannot_be_replaced_by_extension(self):
        with self.assertRaisesRegex(ValueError, 'schema'):
            source.extend_source_index({}, b'not-original', [extension([self.new_function])])

    def test_additional_owner_preserves_actual_entry_and_never_claims_semantics(self):
        item = extension([self.base_function, self.new_function])
        item['requested_entries'] = [{'owner_va': 24, 'requested_va': 25, 'is_owner_start': False}]
        instructions, refs, receipt = self.merge([item])
        self.assertEqual(set(instructions), {16, 17, 18, 24, 25, 26})
        self.assertEqual(receipt['S2_base_owners'], 1)
        self.assertEqual(receipt['merged_owners'], 2)
        self.assertEqual(receipt['extensions'][0]['new_actual_owners'], ['0x18'])
        self.assertFalse(receipt['extensions'][0]['new_semantics_proven'])
        self.assertFalse(receipt['unknown_CFG_promoted'])

    def test_missing_instruction_bytes_and_shared_tail_cannot_be_inferred(self):
        for mutate in (lambda f: f['instructions'][0].pop('size'),
                       lambda f: f['instructions'][0].pop('raw_hex'),
                       lambda f: f['instructions'].pop(),
                       lambda f: f['instructions'].append(f['instructions'][0])):
            bad = copy.deepcopy(self.new_function); mutate(bad)
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                source.verified_extension_function(bad, self.image)
        bad = copy.deepcopy(self.new_function)
        tail = function(self.image, 40, 42)
        bad['chunks'] += tail['chunks']; bad['blocks'] += tail['blocks']
        with self.assertRaisesRegex(ValueError, 'complete owner chunks'):
            source.verified_extension_function(bad, self.image)

    def test_raw_chunk_instruction_width_and_duplicate_owner_rejected(self):
        for mutate in (lambda f: f['chunks'][0].update(raw_hex='00'),
                       lambda f: f['chunks'][0].update(sha256='0' * 64),
                       lambda f: f['instructions'][0].update(raw_hex='00'),
                       lambda f: f['instructions'][0].update(size=True),
                       lambda f: f['chunks'].append(f['chunks'][0])):
            bad = copy.deepcopy(self.new_function); mutate(bad)
            with self.subTest(bad=bad), self.assertRaises(ValueError):
                source.verified_extension_function(bad, self.image)
        with self.assertRaisesRegex(ValueError, 'duplicate actual owner'):
            self.merge([extension([self.new_function, self.new_function])])

    def test_existing_owner_CFG_xref_or_chunk_is_not_replaced(self):
        for mutate in (lambda f: f['blocks'][0].update(successors=[17]),
                       lambda f: f['xrefs'].append({'from': 16, 'to': 18, 'type': 'JUMP_NEAR'})):
            bad = copy.deepcopy(self.base_function); mutate(bad)
            with self.subTest(bad=bad), self.assertRaisesRegex(ValueError, 'existing owner'):
                self.merge([extension([bad])])
        with self.assertRaisesRegex(ValueError, 'existing owner'):
            self.merge([extension([function(self.image, 16, 20)])])

    def test_external_empty_exit_is_retained_but_not_promoted_to_instruction(self):
        valid = copy.deepcopy(self.new_function)
        valid['blocks'].append({'start': 48, 'end': 48, 'successors': []})
        instructions, _ = source.verified_extension_function(valid, self.image)
        self.assertNotIn(48, instructions)
        valid['blocks'][-1]['successors'] = [24]
        with self.assertRaisesRegex(ValueError, 'CFG'):
            source.verified_extension_function(valid, self.image)

    def test_unknown_schema_stale_base_image_and_fake_requested_entry_rejected(self):
        for mutate in (lambda d: d.update(schema='fd2-unreviewed-domain'),
                       lambda d: d.update(image_sha256='0' * 64),
                       lambda d: d.update(database_saved=True),
                       lambda d: d['S2_readset'].update(sha256='0' * 64),
                       lambda d: d.update(requested_entries=[{'owner_va': 24, 'requested_va': 16, 'is_owner_start': False}]),
                       lambda d: d.update(requested_entries=[{'owner_va': 24, 'requested_va': 25, 'is_owner_start': True}])):
            bad = extension([self.new_function]); mutate(bad)
            with self.subTest(bad=bad), self.assertRaises(ValueError): self.merge([bad])

    def test_original_constant_width_hash_and_initialized_span_rejected(self):
        table = {'va': 32, 'size': 2, 'raw_hex': self.image[32:34].hex(),
                 'sha256': hashlib.sha256(self.image[32:34]).hexdigest()}
        item = extension([self.new_function]); item['tables'] = [table]
        self.assertEqual(self.merge([item])[2]['extensions'][0]['source_tables'], 1)
        for mutate in (lambda t: t.update(size=True), lambda t: t.update(size=0),
                       lambda t: t.update(va=64), lambda t: t.update(raw_hex='00'),
                       lambda t: t.update(sha256='0' * 64)):
            bad = copy.deepcopy(item); mutate(bad['tables'][0])
            with self.subTest(table=bad['tables']), self.assertRaisesRegex(ValueError, 'constant'):
                self.merge([bad])


if __name__ == '__main__': unittest.main()
