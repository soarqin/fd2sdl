"""原notice wait完整字节/调用点及独立向量的拒绝测试；不运行窗口。"""
import copy
import importlib.util
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
from rebuild_fd2_analysis import parse_le,parse_objects,build_relbase_image,parse_fixups,apply_fixups
spec=importlib.util.spec_from_file_location('notice_wait',ROOT/'tools/ida/verify_fd2_notice_wait.py')
source=importlib.util.module_from_spec(spec);spec.loader.exec_module(source)


class NoticeWaitSourceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        data=(ROOT/'original_game/FD2.EXE').read_bytes();le=parse_le(data);objects=parse_objects(data,le)
        cls.image=bytes(apply_fixups(build_relbase_image(data,le,objects),parse_fixups(data,le,objects)))
        # 纯测试用连续单条替身；真实CLI必须消费Domain完整指令导出。
        cls.readset={'functions':[{'va':start,'end':end,'chunks':[{'start':start,'end':end}],
            'instructions':[{'va':start,'bytes':cls.image[start:end].hex()}]}
            for start,(end,sha) in source.FUNCTIONS.items()]}

    def test_seven_complete_original_functions_and_callers(self):
        self.assertEqual(len(source.validate_source(self.image,self.readset)),7)

    def test_wrong_image_missing_chunk_and_instruction_gap_rejected(self):
        image=bytearray(self.image);image[0x1e5d9]^=1
        with self.assertRaisesRegex(ValueError,'镜像'):source.validate_source(image,self.readset)
        for change in (lambda f:f.update(end=f['end']-1),lambda f:f.update(chunks=[]),
            lambda f:f['instructions'][0].update(va=f['va']+1),
            lambda f:f['instructions'][0].update(bytes='00')):
            readset=copy.deepcopy(self.readset);change(readset['functions'][0])
            with self.assertRaises(ValueError):source.validate_source(self.image,readset)

    def test_nine_vectors_signed_wrap_key_keeps_second_read(self):
        models={c[0]:source.model(c) for c in source.CASES};self.assertEqual(len(models),9)
        self.assertEqual(models['queued_key'][1],['water','input','clock_first','clock_second','clear'])
        self.assertEqual(models['zero_do_once'][1],['water','input','clock_first','clear'])
        self.assertEqual(models['ffff_to_zero'][0]['water'],3)
        self.assertEqual(models['signed_half_wrap'][0]['water'],1)
        self.assertEqual(models['second_read_backwards'][0]['clock_reads'],2)
        with self.assertRaises(ValueError):source.model(('unfinished',10,100,((100,100,0),)))

    def test_missing_duplicate_or_wrong_c_trace_rejected(self):
        cases=[dict(source.model(c)[0],events=source.model(c)[1]) for c in source.CASES]
        document={'schema':'fd2.g4.notice-wait-c.v1','cases':[source.model(c)[0] for c in source.CASES]}
        self.assertEqual(source.compare_c(cases,document)['cases'],9)
        for kind in ('missing','duplicate','secondread'):
            bad=copy.deepcopy(document)
            if kind=='missing':bad['cases'].pop()
            elif kind=='duplicate':bad['cases'].append(bad['cases'][0])
            else:bad['cases'][2]['clock_reads']=1
            with self.subTest(kind=kind),self.assertRaises(ValueError):source.compare_c(cases,bad)


if __name__=='__main__':unittest.main()
