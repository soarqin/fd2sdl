"""只用独立固定表/合成pair；不含音乐、原资产、模拟器或捕获会话。"""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location('dro', Path(__file__).resolve().parents[1] / 'inspect_dro_v2.py')
dro = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(dro)
TABLE = bytes.fromhex('01040508bd20406080e021416181e122426282e223436383e324446484e425456585e528486888e829496989e92a4a6a8aea2b4b6b8beb2c4c6c8cec2d4d6d8ded30507090f031517191f132527292f233537393f334547494f435557595f5a0b0c0a1b1c1a2b2c2a3b3c3a4b4c4a5b5c5a6b6c6a7b7c7a8b8c8')


def capture(pairs, milliseconds=0, **changes):
    fields = dict(magic=b'DBRAWOPL', high=2, low=0, count=len(pairs), ms=milliseconds,
                  hardware=0, layout=0, compression=0, short=122, long=123, table_size=122)
    fields.update(changes)
    return struct.pack('<8sHHIIBBBBBB', *fields.values()) + TABLE + bytes(value for pair in pairs for value in pair)


class DroV2Test(unittest.TestCase):
    def test_header_boundaries_and_ordered_banks_without_mode_inference(self):
        data = capture([(0x82, 1), (0x81, 0), (97, 0x30), (225, 0x10), (96, 0x20)])
        result = dro.parse_dro(data)
        self.assertTrue(result['ok'])
        self.assertEqual([row['register'] for row in result['commands']], [0x105, 0x104, 0xc0, 0x1c0, 0xb0])
        self.assertEqual([row['file_offset'] for row in result['commands']], [148, 150, 152, 154, 156])
        self.assertEqual(result['header']['hardware_label'], 0)
        self.assertFalse(result['evidence_scope']['header_label_proves_OPL_mode'])
        self.assertEqual(result['header_bytes'], data[:26].hex())
        self.assertEqual(result['boundaries']['payload_end'], len(data))

    def test_short_and_long_delays_are_exact_integer_ms(self):
        result = dro.parse_dro(capture([(96, 32), (122, 0), (0, 1), (123, 0), (122, 255), (96, 0)], 513))
        self.assertTrue(result['ok'])
        writes = [row for row in result['commands'] if row['kind'] == 'write']
        self.assertEqual([row['time_ms'] for row in writes], [0, 1, 513])
        self.assertEqual([row['delay_ms'] for row in result['commands'] if row['kind'] == 'delay'], [1, 256, 256])

    def test_identical_encoded_records_remain_in_original_order(self):
        result = dro.parse_dro(capture([(0, 1), (0, 1), (96, 32)]))
        self.assertEqual(result['decoded_writes'], 3)
        self.assertEqual([row['pair_index'] for row in result['commands']], [0, 1, 2])
        self.assertTrue(result['evidence_scope']['initial_nonzero_cache_prefix_synthesized_by_writer'])
        self.assertFalse(result['evidence_scope']['t0_cache_and_live_boundary_encoded'])

    def test_all_truncated_header_and_table_boundaries_reject(self):
        valid = capture([(96, 32)])
        for length in (0, 7, 25, 26, 147, 149):
            with self.subTest(length=length), self.assertRaises(ValueError):
                dro.parse_dro(valid[:length])

    def test_wrong_header_types_or_mapping_reject(self):
        for changes in (dict(magic=b'OTHEROPL'), dict(high=1), dict(low=1), dict(hardware=3),
                        dict(layout=1), dict(compression=1), dict(short=123), dict(long=122), dict(table_size=121)):
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                dro.parse_dro(capture([(96, 32)], **changes))
        data = bytearray(capture([(96, 32)]));data[27] = data[26]
        with self.assertRaisesRegex(ValueError, 'mapping'):
            dro.parse_dro(bytes(data))

    def test_out_of_map_and_banked_delay_are_not_writes(self):
        for opcode in (124, 127, 250, 251, 255):
            with self.subTest(opcode=opcode), self.assertRaisesRegex(ValueError, 'outside conversion'):
                dro.parse_dro(capture([(opcode, 0)]))

    def test_pair_count_zero_overflow_and_trailing_bytes_reject(self):
        for data in (capture([], count=0), capture([(96, 32)], count=2),
                     capture([(96, 32)], count=0xffffffff), capture([(96, 32)]) + b'X',
                     capture([(96, 32)]) + b'\0\0'):
            with self.subTest(size=len(data)), self.assertRaises(ValueError):
                dro.parse_dro(data)

    def test_duration_mismatch_preserves_encoded_clock_without_repair(self):
        result = dro.parse_dro(capture([(96, 32), (122, 9), (96, 0)], 40011))
        self.assertTrue(result['structurally_valid'])
        self.assertFalse(result['ok'])
        self.assertEqual(result['commands'][-1]['time_ms'], 10)
        self.assertEqual(result['declared_minus_encoded_ms'], 40001)
        self.assertTrue(result['possible_unencoded_30s_close_gap'])

    def test_trailing_delay_or_excessive_delay_is_not_writer_consistent(self):
        for pairs, ms in (([(96, 32), (122, 0)], 1), ([(96, 32), (123, 117), (96, 0)], 30208)):
            with self.subTest(pairs=pairs):
                result = dro.parse_dro(capture(pairs, ms))
                self.assertTrue(result['structurally_valid'])
                self.assertFalse(result['writer_consistent'])

    def test_cli_unicode_new_output_is_read_only_and_cannot_overwrite(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary);source = root / '原始.dro';source.write_bytes(capture([(96, 32)]))
            before = (source.read_bytes(), source.stat().st_mtime_ns)
            output = root / '检查.json'
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(dro.main([str(source), '--output', str(output)]), 0)
            self.assertTrue(json.loads(output.read_text(encoding='utf-8'))['source']['unchanged_during_read'])
            saved = output.read_bytes()
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(dro.main([str(source), '--output', str(output)]), 1)
            self.assertEqual(output.read_bytes(), saved)
            self.assertEqual((source.read_bytes(), source.stat().st_mtime_ns), before)

    def test_output_inside_resolved_original_or_input_rejects_before_read(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary);protected = root / 'original_game';protected.mkdir()
            source = root / 'input.dro';source.write_bytes(capture([(96, 32)]))
            for output in (protected / 'new.json', source):
                with self.subTest(output=output), mock.patch.object(dro, 'inspect', side_effect=AssertionError('must not read')), contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(dro.main([str(source), '--output', str(output)]), 1)
            self.assertFalse((protected / 'new.json').exists())

    def test_file_changing_during_read_rejects(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / 'input.dro';source.write_bytes(capture([(96, 32)]))
            original = Path.read_bytes
            def changed(path):
                data = original(path)
                path.write_bytes(data + b'\0\0')
                return data
            with mock.patch.object(Path, 'read_bytes', changed), self.assertRaisesRegex(ValueError, 'changed while reading'):
                dro.inspect(source)


if __name__ == '__main__':
    unittest.main()
