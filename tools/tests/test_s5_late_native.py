"""S5后批独立原资源前置、精确输入及拒绝门；不启动GUI，不以模型签C。"""
from pathlib import Path
from types import SimpleNamespace
import copy
import struct
import sys
import tempfile
import unittest
from unittest.mock import Mock,patch

from PIL import Image
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import run_s5_late_check as runner
from native import s5_late_fixture as fixture
from native import s3_fixture as state
from run_graphics_tests import decode_save
from fd2_opening_oracle import page_match
from fd2_battle_animation_frames import figani_parts


class LateNative(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (ROOT/'build/s5').mkdir(parents=True,exist_ok=True)
        cls.p26=runner.prepare(ROOT,fixture.SCENARIOS[0]);cls.p29=runner.prepare(ROOT,fixture.SCENARIOS[1])
        cls.original=decode_save(ROOT/'original_game/FD2.SAV')

    def test_backend_scope_default_pair_and_explicit_SDL_only_are_not_interchanged(self):
        p=runner.prepare(ROOT,fixture.SCENARIOS[2])
        with tempfile.TemporaryDirectory(dir=ROOT/'build/s5')as directory:
            for index,kinds in enumerate((('original','sdl'),('sdl',))):
                args=SimpleNamespace(root=ROOT,artifacts=Path(directory)/str(index),run=False)
                report=runner.host.run_prepared(args,p,lambda *a:None,(26,),fixture.SCENARIOS[2],[],kinds=kinds)
                self.assertEqual(report['required_backends'],list(kinds))
                self.assertEqual(set(report['title_menu_policy']),set(kinds))
                self.assertEqual(report['status'],'prepared');self.assertFalse(report['ok'])
                self.assertNotIn('original_runtime_executed',report)
            for kinds in(('original',),('sdl','original'),['sdl'],(),('guess',)):
                args=SimpleNamespace(root=ROOT,artifacts=Path(directory)/'rejected',run=False)
                with self.assertRaises(ValueError):runner.host.run_prepared(args,p,lambda *a:None,(26,),fixture.SCENARIOS[2],[],kinds=kinds)
                self.assertFalse(args.artifacts.exists())

    def test_bad26_is_explicit_SDL_only_and_uses27_notice17_to23(self):
        p=runner.prepare(ROOT,fixture.SCENARIOS[2])
        self.assertEqual(p['source'][runner.META:runner.META+3],bytes((1,63,26)))
        self.assertTrue(all(fixture.item_prefix_slot(u,100)is None for u in state.units(p['source'])[:16]))
        self.assertEqual(sorted({page['fragment']for page in p['victory_pages']}),[8,13,14,15,16])
        self.assertEqual([page['fragment']for page in p['notice_pages']],[17,18,19,20,21,22,23])
        self.assertTrue(all(page['stage']==26 for page in p['notice_pages']))
        self.assertEqual(p['credits_pages'],[])
        units=fixture.victory_records(p,'sdl');self.assertEqual(units[1][:2],bytes((255,255)))
        expected=fixture.credits30(p,'sdl')[0]
        self.assertEqual(expected[0][1],18)
        self.assertEqual(p['source'][runner.META+2],26)

    def test_source26_full63_and29_original27_not_small_count_cap_bypass(self):
        p=self.p26;records=state.units(p['source'])
        self.assertEqual(p['source'][runner.META:runner.META+3],bytes((1,63,26)))
        self.assertEqual((p['manual_state']['count'],len(records)),(31,63))
        row=p['data']['stages'][26]
        self.assertEqual((row['metadata'][1],sum(t[21]==0 for t in row['templates'])),(16,47))
        self.assertEqual(records[0][:2],bytes(row['placement'][80]))
        self.assertTrue(all(u[5]&1 for u in records if u[6]==0))
        self.assertFalse(records[0][5]&0x85);self.assertFalse(records[1][5]&1)
        self.assertIsNotNone(fixture.item_prefix_slot(records[0],100))
        self.assertEqual(p['manual_state']['roster'],self.original[0x8a3:0x12a3])
        for raw,offset in p['manifest']['constructed_unknown_holes_preserved_from_same_SAV_slot']:
            if offset!=2:self.assertEqual(records[raw][offset],self.original[runner.state.UNITS+80*raw+offset])
        records29=state.units(self.p29['source']);self.assertEqual(len(records29),27)
        self.assertTrue(records29[20][5]&1);self.assertFalse(records29[0][5]&1);self.assertFalse(records29[1][5]&1)
        for prepared in(self.p26,self.p29):
            self.assertEqual(prepared['manifest']['plaintext_patches'],[
                {'offset':i,'before':a,'after':b}for i,(a,b)in enumerate(zip(self.original,prepared['source']))if a!=b])
            self.assertFalse(prepared['manifest']['natural_whole_stage_claimed'])

    def test_script84_counted5_and_explicit4byte_gap_not_an_extra_action(self):
        p=self.p26;script=fixture.script(p['data'],p['original'],84)
        self.assertEqual(script['blocks'],[(2,[(1,2)])]);self.assertEqual(script['length'],5)
        self.assertEqual(script['excluded_after_count'],{'start':'0x63221','raw_hex':'88010102'})
        bad=copy.deepcopy(p['data']);bad['raw']=bytearray(bad['raw']);bad['raw'][0x63221]^=1
        with self.assertRaisesRegex(ValueError,'gap'):fixture.script(bad,p['original'],84)

    def test_27_owner_restore_and_original_roster_tail_not_double_sync(self):
        p=self.p26;prepared=fixture.preparation27(p,'sdl')
        self.assertEqual((prepared['stage'],prepared['count']),(27,31))
        self.assertEqual(prepared['roster'][31*80:],p['manual_state']['roster'][31*80:])
        for i in range(31):
            u=prepared['roster'][i*80:(i+1)*80]
            self.assertEqual(u[5],0);self.assertEqual(u[64:66],u[66:68]);self.assertEqual(u[68:70],u[70:72])
        # Loaded cache policy remains per backend even when this fixed input happens equal.
        modified=copy.deepcopy(p);data=bytearray(modified['source']);data[state.UNITS+2]=63
        modified['source']=state.checksum(data)
        self.assertNotEqual(fixture.preparation27(modified,'original')['roster'],fixture.preparation27(modified,'sdl')['roster'])

    def test_27_no_town_reorder_and_real34_source_records(self):
        p=self.p26;prepared=fixture.preparation27(p,'sdl');view,records,masks,selected=fixture.entry27(p,prepared)
        self.assertEqual((len(records),len(masks)),(34,34));self.assertEqual(view[runner.META:runner.META+3],bytes((1,34,27)))
        self.assertEqual(records[0][:2],bytes((32,21)));self.assertEqual(selected['roster'][88],9)
        self.assertEqual([u[3]for u in records[:20]],[0]*20)
        self.assertEqual(selected['roster'][31*80:],prepared['roster'][31*80:])
        self.assertEqual(p['data']['raw'][0x523e7+27],1)
        # Explicitly remove text9, without relying on an incidental byte substring.
        bad_roster=bytearray(prepared['roster'])
        for i in range(31):
            if bad_roster[80*i+8]==9:bad_roster[80*i+8]=39
        with self.assertRaises(ValueError):fixture.selected27({**prepared,'roster':bytes(bad_roster)})

    def test_resource30_is31_deployment_internal_not_normal_stage30(self):
        records,masks,prepared=fixture.credits30(self.p29,'sdl')
        self.assertEqual((len(records),len(masks),prepared['stage']),(31,31,30))
        self.assertEqual(self.p29['source'][runner.META+2],29)
        # 两组143为高位wait，只写方向；随后mode5只移动raw2..30向下五格。
        self.assertEqual(records[0][1],self.p29['data']['stages'][30]['placement'][1][1])
        self.assertEqual(records[2][1],self.p29['data']['stages'][30]['placement'][3][1]+5)
        self.assertEqual(records[0][0],17)
        self.assertEqual(self.p29['ending_assets']['first_card']['raw'],30)
        self.assertEqual(self.p29['ending_assets']['last_card']['raw'],1)
        raw=(self.p29['original']/'FIGANI.DAT').read_bytes()
        for card in(self.p29['ending_assets']['first_card'],self.p29['ending_assets']['last_card']):
            parts,_=figani_parts(raw,3*card['unit_id'])
            self.assertGreaterEqual(min(part['at'][1]for part in parts),4)

    def test_notice_waits_use84_origin_and_real_fragment8_continuation(self):
        pages=self.p29['notice_pages'];self.assertEqual(len(pages),9)
        self.assertEqual([p['fragment']for p in pages],[2,3,4,5,6,7,8,8,9])
        self.assertEqual(sum(p['arrow_position']is not None for p in pages),1)
        self.assertEqual(next(p['arrow_position']for p in pages if p['arrow_position']), (191,181))
        page=pages[0];canvas=page['text'].copy()
        canvas.paste(self.p29['assets']['dialog_top'],page['border_position'],self.p29['assets']['dialog_top'].point(lambda x:255 if x else 0,mode='L'))
        canvas.paste(page['portraits'][0],page['portrait_position'])
        self.assertIsNotNone(page_match(canvas.convert('RGB'),page,self.p29['assets']))
        canvas.paste(0,(84,119,292,192))
        self.assertIsNone(page_match(canvas.convert('RGB'),page,self.p29['assets']))

    def test_notice_continuation_checks_full_arrow_and_retains_original_erase13(self):
        page=self.p29['notice_pages'][6];following=self.p29['notice_pages'][7]
        self.assertEqual((page['fragment'],page['token_offset'],page['arrow_position']),(8,41,(191,181)))
        self.assertEqual([image.size for image in page['arrows']],[(14,10),(14,10)])
        frame=page['text'].copy();border=self.p29['assets']['dialog_top']
        frame.paste(border,page['border_position'],border.point(lambda x:255 if x else 0,mode='L'))
        frame.paste(page['portraits'][0],page['portrait_position'])
        for arrow in page['arrows']:
            actual=frame.copy();actual.paste(arrow,(191,181));actual=actual.convert('RGB')
            self.assertIsNotNone(page_match(actual,page,self.p29['assets']))
            self.assertIsNone(page_match(actual,{**page,'arrow_position':(180,181)},self.p29['assets']))
            damaged=actual.copy();damaged.putpixel((191,181),(255,0,255))
            self.assertIsNone(page_match(damaged,page,self.p29['assets']))
        # 返键后1685c原frame13不透明写16x16全74，全部256像素仍是已知写集。
        self.assertEqual(set(following['text'].crop((191,176,207,192)).getdata()),{74})
        actual=following['text'].copy()
        actual.paste(border,following['border_position'],border.point(lambda x:255 if x else 0,mode='L'))
        actual.paste(following['portraits'][0],following['portrait_position']);actual=actual.convert('RGB')
        self.assertIsNotNone(page_match(actual,following,self.p29['assets']))
        actual.putpixel((206,191),(255,0,255))
        self.assertIsNone(page_match(actual,following,self.p29['assets']))

    def test_notice_scroll_uses_fixed95_base_while_glyphs_still_start84(self):
        p={**self.p29,'prepare_assets':dict(self.p29['prepare_assets'])}
        font=bytearray(p['prepare_assets']['font']);font[5*32:6*32]=bytes([255])*32
        font[6*32:7*32]=bytes(32);p['prepare_assets']['font']=bytes(font)
        # 15f84第四次换行触16e24；此受控glyph区分x84..93不动与x94开始滚动。
        with patch.object(runner,'text_fragments',return_value=[[5,-2,6,-2,6,-2,6,-2,-1]]):
            page=runner.notice_pages(p,32,0)[0]
        self.assertEqual(page['text'].getpixel((90,119)),205)
        self.assertEqual(page['text'].getpixel((94,119)),74)
        self.assertEqual(page['text'].getpixel((302,191)),74)
        self.assertEqual(page['text'].getpixel((303,191)),0)

    def test_notice_portrait_overdraw_checks_whole_portrait_and_unoccluded_text(self):
        # 31bdf实际文字原点84,119与肖像右缘重叠四列；portrait21右缘不是文字背景74。
        page=self.p29['notice_pages'][1];self.assertEqual((page['portrait_id'],page['fragment']),(21,3))
        self.assertEqual(set(page['text'].crop((8,115,88,195)).getdata()),{0})
        self.assertTrue(all(image.size==(80,80)for image in page['portraits']))
        frame=page['text'].copy()
        border=self.p29['assets']['dialog_top']
        frame.paste(border,page['border_position'],border.point(lambda x:255 if x else 0,mode='L'))
        frame.paste(page['portraits'][0],page['portrait_position'])
        actual=frame.convert('RGB')
        self.assertIsNotNone(page_match(actual,page,self.p29['assets']))
        # 重叠区不再同时要求文字背景，但仍由6400像素完整肖像门逐像素检查。
        damaged=actual.copy();damaged.putpixel((84,119),(255,0,255))
        self.assertIsNone(page_match(damaged,page,self.p29['assets']))
        at=next((x,y)for y in range(119,192)for x in range(88,292)if page['text'].getpixel((x,y)))
        damaged=actual.copy();damaged.putpixel(at,(255,0,255))
        self.assertIsNone(page_match(damaged,page,self.p29['assets']))
        shifted=frame.copy();shifted.paste(0,(8,115,89,195));shifted.paste(page['portraits'][0],(9,115))
        self.assertIsNone(page_match(shifted.convert('RGB'),page,self.p29['assets']))
        wrong_position={**page,'portrait_position':(9,115)}
        self.assertIsNone(page_match(actual,wrong_position,self.p29['assets']))

    def test_card_gate_forward_portrait_name_and_original_background_all_required(self):
        p=self.p29;card=p['ending_assets']['first_card'];frame=Image.new('P',(320,200));frame.putpalette(p['assets']['palette'])
        frame.paste(card['background_top'],(0,0));frame.paste(card['portraits'][0],(8,10))
        frame.paste(card['text'],(0,0),card['text'].point(lambda x:255 if x else 0,mode='L'))
        self.assertIsNotNone(runner.card_match(frame.convert('RGB'),card))
        for box in((8,10,88,90),(105,18,260,55),(0,0,320,4)):
            wrong=frame.copy();wrong.paste(1,box)
            self.assertIsNone(runner.card_match(wrong.convert('RGB'),card))

    def test_montage_caption_uses57_palette_then_final59_returns_original0(self):
        p=self.p29;raw=(p['original']/'FDOTHER.DAT').read_bytes();parts,_=figani_parts(raw,58)
        from run_original_comparison import archive_entry
        for anchor in p['ending_assets']['pair_anchors']:
            frame=Image.new('P',(320,200));mask=Image.new('L',(320,200))
            runner.animation._paste(frame,mask,parts[anchor['identity']['part']])
            frame.putpalette([v<<2 for v in archive_entry(raw,57)])
            self.assertTrue(runner.animation._anchor_match(frame.convert('RGB'),anchor))
            frame.putpalette(p['assets']['palette'])
            self.assertFalse(runner.animation._anchor_match(frame.convert('RGB'),anchor))
        self.assertEqual(p['ending_assets'][59].size,(320,200))

    def test_ending_has_one_skip_key_and_no_terminal_or_activity_save_key(self):
        p=self.p29;record={'kind':'sdl','inputs':[]};capture=Mock();capture.process.poll.return_value=None
        names=[]
        def checkpoint(name,predicate,**options):names.append(name);return None,None
        with patch.object(runner.shared,'checkpoint',return_value=checkpoint),patch.object(runner,'continue_result'),\
             patch.object(runner,'acknowledge')as acknowledgements,patch.object(runner.time,'sleep'):
            runner.ending29(capture,p,None,None,record)
        self.assertEqual([call.args for call in capture.key.call_args_list],[('ENTER',)])
        self.assertEqual(acknowledgements.call_count,2)
        self.assertLess(names.index('s5-credits-first-card30'),names.index('s5-credits-last-card1'))
        self.assertEqual(names[-2:],['s5-ending-final59','s5-ending-final59-held'])
        self.assertTrue(record['source_family']['no_manual_or_activity_Save_at_stage30'])

    def test_actual_montage_checkpoint_predicates_return_mutable_identity_dict_or_None(self):
        p=self.p29;record={'kind':'sdl','inputs':[]};capture=Mock();capture.process.poll.return_value=None
        raw=(p['original']/'FDOTHER.DAT').read_bytes();parts,_=figani_parts(raw,58)
        palette=runner.archive_entry(raw,57);seen=[]
        def checkpoint(name,predicate,**options):
            if not name.startswith('s5-montage-overlay-'):return None,None
            part=int(name.rsplit('-',1)[1]);frame=Image.new('P',(320,200));mask=Image.new('L',(320,200))
            runner.animation._paste(frame,mask,parts[part]);frame.putpalette([v<<2 for v in palette])
            result=predicate(frame.convert('RGB'))
            # shared.wait_state给命中结果附image，实际执行谓词，避免只mock检查点漏掉bool。
            result['image']='actual-resource-frame.png'
            self.assertIsInstance(result,dict)
            self.assertEqual(result['identity'],{'FDOTHER':58,'part':part,'palette_FDOTHER':57})
            self.assertGreaterEqual(result['checked_pixels'],256)
            self.assertIsNone(predicate(Image.new('RGB',(320,200),(255,0,255))))
            seen.append(part);return frame,result
        with patch.object(runner.shared,'checkpoint',return_value=checkpoint),patch.object(runner,'continue_result'),\
             patch.object(runner,'acknowledge'),patch.object(runner.time,'sleep'):
            runner.ending29(capture,p,None,None,record)
        self.assertEqual(seen,[0,19])

    def test_snapshot_qualification_requires_exact_actual_cli_line_not_cap(self):
        p=copy.deepcopy(self.p29)
        with tempfile.TemporaryDirectory()as directory:
            directory=Path(directory);exe=directory/'owner.exe';exe.write_bytes(b'probe identity')
            args=SimpleNamespace(root=ROOT,artifacts=directory/'evidence',snapshot_probe=exe,run=True)
            with patch.object(runner,'validate_output',side_effect=lambda p,**kw:p),\
                 patch.object(runner.subprocess,'run',return_value=SimpleNamespace(returncode=0,stdout=
                    'S5 snapshot fixture: stage=29 active=27 roster=31 complete_container_preserved=true RNG=0\n',stderr='')):
                runner.snapshot_preflight(args,p)
            self.assertIs(p['snapshot_import_preflight']['passed'],True)
            self.assertEqual(p['snapshot_import_preflight']['encoded_snapshot_sha256'],state.digest(p['encoded']))
            args.artifacts=directory/'second'
            with patch.object(runner,'validate_output',side_effect=lambda p,**kw:p),\
                 patch.object(runner.subprocess,'run',return_value=SimpleNamespace(returncode=0,stdout='all tests passed',stderr='')):
                with self.assertRaises(ValueError):runner.snapshot_preflight(args,p)

    def test_cli_keeps_default_pair_and_explicit_sdl_only_identity(self):
        for extra,wanted in (([],('original','sdl')),(['--sdl-only'],('sdl',))):
            report={'ok':False,'status':'prepared','scenario':fixture.SCENARIOS[1]}
            with patch.object(runner,'prepare',return_value={}),patch.object(runner,'snapshot_preflight'),\
                 patch.object(runner.host,'run_prepared',return_value=report)as run,patch('builtins.print'):
                self.assertEqual(runner.main(['--root',str(ROOT),'--artifacts','unused-new-output',
                    '--scenario',fixture.SCENARIOS[1],*extra]),0)
            self.assertEqual(run.call_args.kwargs['kinds'],wanted)


if __name__=='__main__':unittest.main()
