import struct
import unittest

from audit_g4_render_source import archive_entries, rle_writes, text_fragments


class RenderSourceTests(unittest.TestCase):
    def test_rle_actual_write_mask_includes_zero_but_not_skip(self):
        writes=rle_writes(8,1,bytes((0xc0,0x81,0,9,0x41,7,0xc0)))
        self.assertEqual(writes,[(1,0,0),(2,0,9),(4,0,7),(6,0,7)])

    def test_bad_rle_does_not_become_source_evidence(self):
        for width,height,source in ((4,2,b"\x03\x01\x83\x02"),
                (4,1,b"\x04\x01"),(4,1,b"\x03\x01\xff"),
                (4,1,b"\x41"),(0,1,b"\xff")):
            with self.subTest(source=source):
                with self.assertRaises(ValueError):rle_writes(width,height,source)

    def test_text_fragments_preserve_dynamic_control_and_terminate(self):
        data=struct.pack("<HHhhhhhh",4,10,-4,12,-1,-6,-1,99)
        self.assertEqual(text_fragments(data),[(-4,12,-1),(-6,-1)])
        for bad in (b"\0",struct.pack("<HH",2,1),
                    struct.pack("<HHh",4,3,-1)):
            with self.assertRaises(ValueError):text_fragments(bad)

    def test_archive_uses_first_offset_not_fake_entry_count(self):
        data=b"LLLLLL"+struct.pack("<III",18,21,0)+b"abcde"
        self.assertEqual(archive_entries(data),[b"abc",b"de"])
        for bad in (b"LLLLLL\0\0\0\0", b"XXXXXX"+data[6:],
                    b"LLLLLL"+struct.pack("<II",30,0)):
            with self.assertRaises(ValueError):archive_entries(bad)


if __name__=="__main__":unittest.main()
