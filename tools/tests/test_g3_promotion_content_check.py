"""P4离线内容门的原资源/完整像素/候选身份与输入范围；不启动GUI。"""
from pathlib import Path
import json
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import run_g3_promotion_content_check as content


class PromotionContentCheckTest(unittest.TestCase):
    def test_full_original_function_hashes_require_no_ignored_ida_database(self):
        source=content.verify_source(ROOT)
        self.assertEqual([(row['cva'],row['size']) for row in source['functions']], [('0x2fb2c',744),('0x2fe14',237)])
        self.assertTrue(all(row['whole_EXE_legal_relocated_bytes_equal'] for row in source['functions']))

    def test_identity_and_white_band_have_real_source_negative_controls(self):
        for old,new in ((4,36),(9,52)):
            expected,facts,negative=content.templates(ROOT,old,new)
            self.assertEqual((facts['old']['entry'],facts['new']['entry']),(old*3,new*3))
            self.assertTrue(all(negative.values()))
            self.assertEqual(expected['white'][0]['blank_band'].size,(320,50))
            old_image=expected['old'][0]['image'];new_image=expected['new'][0]['image']
            self.assertGreater(content.maximum(old_image,new_image),3)
            white=expected['white'][-1]['blank_band'];bad=white.copy();bad.putpixel((319,49),(0,0,0))
            self.assertGreater(content.maximum(white,bad),3)
            self.assertEqual(content.maximum(white,white.copy()),0)

    def test_success_flag_without_original_resource_identity_cannot_be_reused(self):
        with tempfile.TemporaryDirectory(prefix='g3-promotion-source-') as tmp:
            path=Path(tmp)/'comparison.json'
            path.write_text(json.dumps({'scenario':'p4-promote-normal','ok':True,'final_sources_unchanged':True}),encoding='utf-8')
            with self.assertRaisesRegex(ValueError,'原生收据冻结'):
                content.analyze(ROOT,path,'p4-promote-normal',Path(tmp),4,36,2)

    def test_cli_rejects_unrelated_or_duplicate_scenarios_before_creating_outputs(self):
        with tempfile.TemporaryDirectory(prefix='g3-promotion-cli-') as tmp:
            root=Path(tmp);report=root/'report.json';out=root/'new'
            for scenario,repeat in (('p3-revive',False),('p4-promote-normal',True)):
                report.write_text(json.dumps({'scenario':scenario}),encoding='utf-8')
                argv=['--root',str(ROOT),'--reports',str(report)]
                if repeat:argv.append(str(report))
                argv.extend(['--artifacts',str(out)])
                with self.assertRaises(ValueError):content.main(argv)
                self.assertFalse(out.exists())


if __name__=='__main__':unittest.main()
