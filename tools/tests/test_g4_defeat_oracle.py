"""败亡原资源/有序代表/声音反候选；合成PNG/PCM仅测判据，不替代实机。"""
import contextlib
import copy
import io
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import wave

import numpy as np
from PIL import Image
from scipy import signal

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools'))
import fd2_g4_defeat_oracle as oracle
from run_g4_common_event_check import prepare
from fd2_sfx_oracle import references


class DefeatOracleTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = ROOT/'original_game'
        with contextlib.redirect_stdout(io.StringIO()): p = prepare(ROOT, 'child39-empty')
        cls.source = p['target_view']
        cls.resources = oracle.resources(cls.original, cls.source)

    def write_frames(self, directory, *, phase=1, steps=None):
        if steps is None: steps = [('rotation', j % 4) for j in range(13)] + [('dissolve', f) for f in range(68, 80)]
        rows = []; background = Image.new('RGB', (320, 200), (43, 47, 53))
        for index, (kind, value) in enumerate(steps):
            if kind == 'rotation':
                rule = next(r for r in self.resources[kind] if (r['phase'], r['direction']) == (phase, value))
            else: rule = next(r for r in self.resources[kind] if r['frame'] == value)
            if kind == 'rotation' or value >= 74: image = background.copy()
            anchor = rule['anchor']; image.paste(anchor['image'], anchor['at'], anchor['mask'])
            path = Path(directory)/f'frame{index:05d}.png'; image.save(path)
            rows.append({'index': index, 'path': str(path), 'sha256': oracle._digest(path.read_bytes())})
        return rows

    def test_real_source_camera_and_all_original_write_sets(self):
        c = self.resources['contract']
        self.assertEqual((c['target_raw'], c['unit_id'], c['camera'], c['world']), (11, 96, [5, 11], [8, 14]))
        self.assertEqual((c['sprite_at'], c['dissolve_at']), ([76, 70], [52, 46]))
        self.assertEqual([r['anchor']['checked_pixels'] for r in self.resources['dissolve']],
                         [18, 51, 82, 2166, 2487, 2539, 2417, 1331, 865, 464, 250, 111])
        self.assertFalse(c['whole_frame_or_exact_timing_claimed'])

    def test_three_effective_sprite_classes_not_four_raw_idle_values_and_25_representatives(self):
        with tempfile.TemporaryDirectory(prefix='fd2-defeat-oracle-') as directory:
            for phase in range(3):
                rows = self.write_frames(directory, phase=phase)
                result = oracle.check_visual(rows, self.resources)
                self.assertTrue(result['ok'], result)
                self.assertEqual(result['effective_sprite_phase'], phase)
                self.assertFalse(result['raw_idle_dword_measured'])
                self.assertEqual(result['contract']['raw_idle_to_effective_sprite_phase'], [0, 1, 2, 1])
                self.assertEqual((len(result['rotation_representatives']), len(result['dissolve_representatives'])), (13, 12))

    def test_missing_spin_lmi_or_reordered_tail_is_not_complete(self):
        canonical = [('rotation', j % 4) for j in range(13)] + [('dissolve', f) for f in range(68, 80)]
        with tempfile.TemporaryDirectory(prefix='fd2-defeat-missing-') as directory:
            for position in (1, 12, 13, 18, 19, 24):
                steps = canonical[:position] + canonical[position+1:]
                self.assertFalse(oracle.check_visual(self.write_frames(directory, steps=steps), self.resources)['ok'])
            steps = list(canonical); steps[-1], steps[-2] = steps[-2], steps[-1]
            self.assertFalse(oracle.check_visual(self.write_frames(directory, steps=steps), self.resources)['ok'])

    def test_no_animation_or_duplicate_png_cannot_supply_future_steps(self):
        with tempfile.TemporaryDirectory(prefix='fd2-defeat-static-') as directory:
            rows = self.write_frames(directory, steps=[('rotation', 0)]*25)
            result = oracle.check_visual(rows, self.resources)
            self.assertFalse(result['ok']); self.assertEqual(len(result['rotation_representatives']), 1)
            rows = self.write_frames(directory)
            duplicate = dict(rows[1], index=rows[0]['index'])
            with self.assertRaisesRegex(ValueError, '严格顺序'): oracle.check_visual([rows[0], duplicate], self.resources)

    def test_no_mask_deletion_anchor_shift_or_png_replacement(self):
        for edit in ('mask', 'at'):
            prepared = copy.deepcopy(self.resources); anchor = prepared['dissolve'][0]['anchor']
            if edit == 'mask': anchor['mask'] = Image.new('L', anchor['mask'].size)
            else: anchor['at'] = (0, 0)
            with self.assertRaisesRegex(ValueError, '不得修改'): oracle.check_visual([], prepared)
        with tempfile.TemporaryDirectory(prefix='fd2-defeat-identity-') as directory:
            rows = self.write_frames(directory); Path(rows[0]['path']).write_bytes(b'not the WGC original')
            with self.assertRaisesRegex(ValueError, 'PNG身份'): oracle.check_visual(rows, self.resources)

    def test_resource_gate_does_not_guess_moving_or_jitter_actor(self):
        for offset in (4, 0x26):
            changed = bytearray(self.source); changed[oracle.UNITS+11*80+offset] = 1
            changed = oracle.common.checksum(changed)
            with self.assertRaisesRegex(ValueError, '资源门只覆盖'): oracle.resources(self.original, changed)

    def recording(self, path, sample, gain=.3, rate=44100):
        refs = references(self.original/'FDOTHER.DAT', self.resources['sound']['candidates'])
        source = refs[sample]['values']
        divisor = __import__('math').gcd(rate, 11025)
        voice = signal.resample_poly(source, rate//divisor, 11025//divisor)
        values = np.r_[np.zeros(rate//10), voice*gain, np.zeros(rate//10)]
        pcm = np.rint(values*32768).astype('<i2')
        with wave.open(str(path), 'wb') as out:
            out.setnchannels(2); out.setsampwidth(2); out.setframerate(rate)
            out.writeframes(np.column_stack((pcm, pcm)).astype('<i2').tobytes())

    def test_whole_bank13_sound_candidates_with_fixed_expected_prefix(self):
        declaration = self.resources['sound']
        self.assertEqual(len(declaration['references']), 13)
        self.assertEqual(declaration['prefix_frames'], 661)
        self.assertEqual([r['comparison_prefix_frames'] for r in declaration['references']],
                         [160, 661, 661, 661, 661, 661, 661, 160, 661, 390, 661, 661, 661])
        with tempfile.TemporaryDirectory(prefix='fd2-defeat-sfx-') as directory:
            path = Path(directory)/'observed.wav'
            for rate in (44100, 48000):
                self.recording(path, 3, gain=.1, rate=rate)
                result = oracle.check_sound(self.original, path)
                self.assertTrue(result['ok'], result)
                self.assertEqual((result['expected_bank'], result['expected_sample']), (31, 3))
                self.assertFalse(result['exact_cue_count_latency_A7_or_runtime_bank_pointer_claimed'])
            self.recording(path, 12)
            self.assertFalse(oracle.check_sound(self.original, path)['ok'])


if __name__ == '__main__': unittest.main()
