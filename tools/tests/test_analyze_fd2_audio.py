#!/usr/bin/env python3
"""Regression tests for FD2 audio resource inventory."""

import struct
import sys
import tempfile
import unittest
import wave
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from analyze_fd2_audio import (  # noqa: E402
    analyze,
    archive_entries,
    archive_offsets,
    extract_music_xmi,
    extract_sfx_wavs,
    stage_music_tables,
)


def make_archive(entries: list[bytes]) -> bytes:
    start = 6 + len(entries) * 4
    offsets = []
    payload = bytearray()
    for entry in entries:
        offsets.append(start + len(payload))
        payload.extend(entry)
    return b"LLLLLL" + b"".join(struct.pack("<I", x) for x in offsets) + payload


class AudioArchiveTest(unittest.TestCase):
    def test_archive_offset_stream(self) -> None:
        data = make_archive([b"abc", b"defg"])
        self.assertEqual(archive_offsets(data), [14, 17])
        self.assertEqual(archive_entries(data), [b"abc", b"defg"])

    def test_music_and_fixed_sfx_banks(self) -> None:
        fdmus = make_archive([
            b" \r\n",
            b"FORM\x00\x00\x00\x00XDIRINFO----XMID",
            b"FORM\x00\x00\x00\x00XDIRINFO----XMID-extra",
        ])
        fdother_entries = [b"x"] * 81
        fdother_entries[31] = make_archive([bytes([0x80, 0x81]),
                                            bytes([0x7f])])
        fdother_entries[77] = make_archive([bytes([0x81, 0x81])])
        fdother_entries[80] = make_archive([bytes([0x80, 0x80, 0x80])])
        result = analyze(fdmus, make_archive(fdother_entries))
        self.assertEqual(result["music"]["entry_count"], 3)
        self.assertEqual(len(result["music"]["xmid_tracks"]), 2)
        self.assertEqual(result["music"]["sentinel_entries"], [0])
        self.assertEqual(result["sfx_banks"][0]["fdother_entry"], 31)
        self.assertEqual(
            result["sfx_playback_defaults"]["sample_rate"], 11025)
        self.assertIn("ail_init_sample",
                      result["sfx_playback_defaults"]["evidence"])
        self.assertEqual([x["bytes"] for x in
                          result["sfx_banks"][0]["samples"]], [2, 1])
        self.assertEqual(result["sfx_banks"][1]["fdother_entry"], 77)
        self.assertEqual(result["sfx_banks"][1]["samples"][0]["mean"],
                         129.0)
        self.assertEqual(result["sfx_banks"][2]["samples"][0]["mean"],
                         128.0)

    def test_stage_music_tables(self) -> None:
        data = bytearray(0x76E73 + 60)
        data[0x76E73:0x76E73 + 30] = bytes(range(30))
        data[0x76E73 + 30:0x76E73 + 60] = bytes(range(30, 60))
        tables = stage_music_tables(bytes(data))
        self.assertEqual(tables["file_offset"], 0x76E73)
        self.assertEqual(tables["code0_offset"], 0x66073)
        self.assertEqual(tables["primary"], list(range(30)))
        self.assertEqual(tables["alternate"], list(range(30, 60)))

    def test_extract_music_xmi(self) -> None:
        fdmus = make_archive([
            b" \r\n",
            b"FORM\x00\x00\x00\x00XDIRINFO----XMID",
        ])
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            self.assertEqual(extract_music_xmi(fdmus, output), 1)
            self.assertEqual((output / "track01.xmi").read_bytes(),
                             archive_entries(fdmus)[1])

    def test_extract_sfx_wavs(self) -> None:
        entries = [b"x"] * 81
        entries[31] = make_archive([bytes([0x80, 0x81])])
        entries[77] = make_archive([bytes([0x80])])
        entries[80] = make_archive([bytes([0x7f])])
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            self.assertEqual(extract_sfx_wavs(make_archive(entries), output), 3)
            with wave.open(str(output / "fdother031_00.wav"), "rb") as wav:
                self.assertEqual(wav.getframerate(), 11025)
                self.assertEqual(wav.getnchannels(), 1)
                self.assertEqual(wav.getsampwidth(), 1)
                self.assertEqual(wav.readframes(2), bytes([0x80, 0x81]))

    def test_rejects_non_archive(self) -> None:
        with self.assertRaises(ValueError):
            archive_offsets(b"not-an-archive")


if __name__ == "__main__":
    unittest.main()
