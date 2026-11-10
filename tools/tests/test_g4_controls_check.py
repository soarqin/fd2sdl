"""G4 controls来源/所有者/输入期望与资源负例；不启动GUI。"""
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from PIL import Image

TOOLS=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(TOOLS))
import run_g4_controls_check as check
ROOT=TOOLS.parent


class G4ControlsTest(unittest.TestCase):
    def test_original_instruction_contract(self):
        result=check.source_contract(ROOT/'original_game')
        self.assertEqual(len(result['instruction_spans']),7)
        self.assertFalse(result['original_runtime_claimed'])

    def test_fixture_declared_fields_and_roundtrip(self):
        original=ROOT/'original_game';before=check.decode_save(original/'START.SAV')
        for name in check.SCENARIOS:
            plain,encoded,manifest=check.fixture(original,name)
            allowed=set(range(check.META+5,check.META+9))
            if name=='browse-gates':allowed|={check.UNITS+i*80+5 for i in range(3)}|{check.UNITS+3*80+0x26}
            if name=='browse-cycle':allowed|={check.UNITS+2*80+5,check.UNITS+3*80+0x26}
            changed={i for i,(a,b) in enumerate(zip(before[:-4],plain[:-4])) if a!=b}
            self.assertTrue(changed and changed<=allowed)
            self.assertEqual(changed,{int(row['offset'],16) for row in manifest['controlled_changes_before_checksum']})
            self.assertEqual(plain[check.META+3:check.META+5],before[check.META+3:check.META+5])
            with tempfile.TemporaryDirectory() as directory:
                path=Path(directory)/'fixture.SAV';path.write_bytes(encoded)
                self.assertEqual(check.decode_save(path),plain)

    def test_focus_sequence_has_shared_index_no_G_advance_and_full_count_wrap(self):
        original=ROOT/'original_game'
        for scenario in ('browse-gates','browse-cycle'):
            plain,_,manifest=check.fixture(original,scenario)
            index=0;focused=None
            for key,wanted in manifest['cycle_inputs']:
                if key!='G':
                    found=None
                    for n in range(plain[check.META+1]):
                        raw=(index+n)%plain[check.META+1];at=check.UNITS+raw*80
                        if not plain[at+5]&0x85 and plain[at+6]==2:found=raw;break
                    if found is not None:index=(found+1)%plain[check.META+1];focused=found
                self.assertEqual(focused,wanted)

    def test_acted_save_gate_is_preserved_in_browse_fixture(self):
        gate,_,_=check.fixture(ROOT/'original_game','browse-gates')
        cycle,_,_=check.fixture(ROOT/'original_game','browse-cycle')
        self.assertFalse(check.save_available(gate));self.assertTrue(check.save_available(cycle))
        hidden=bytearray(gate);hidden[check.UNITS+2*80+5]|=1
        self.assertTrue(check.save_available(hidden))

    def test_panel_border_wrong_side_and_one_bad_pixel_rejected(self):
        assets=check.round_assets(ROOT/'original_game');border=assets['restoration_border'].convert('RGB')
        image=Image.new('RGB',(320,200));image.paste(border,(16,190))
        self.assertIsNotNone(check.panel_state(image,assets,'left'))
        self.assertIsNone(check.panel_state(image,assets,'right'))
        self.assertIsNone(check.panel_state(image,assets,'off'))
        pixel=image.getpixel((16,190));image.putpixel((16,190),tuple(255-c for c in pixel))
        self.assertIsNone(check.panel_state(image,assets,'left'))
        blank=Image.new('RGB',(320,200));self.assertIsNotNone(check.panel_state(blank,assets,'off'))

    def test_off_requires_same_original_full_two_panel_regions(self):
        assets=check.round_assets(ROOT/'original_game')
        with tempfile.TemporaryDirectory() as directory:
            # 纯判据测试，不把构造图登记为实际原版运行证据。
            image=Image.new('RGB',(320,200),(20,24,28))
            path=Path(directory)/'original-reference.png';image.save(path)
            reference={'path':str(path),'sha256':check.sha(path),'source_kind':'original',
                'capture_backend':'Windows.Graphics.Capture HWND','ROIs':[list(box) for box in check.PANEL_BOXES]}
            result=check.native_panel_off(image,assets,[reference])
            self.assertEqual([row['checked_pixels'] for row in result['full_both_ROIs']],[2346,2346])
            for point in ((6,162),(247,162)):
                damaged=image.copy();damaged.putpixel(point,(255,255,255))
                self.assertIsNotNone(check.panel_state(damaged,assets,'off'))
                self.assertIsNone(check.native_panel_off(damaged,assets,[reference]))
            with self.assertRaises(ValueError):check.native_panel_off(image,assets,[])
            for key,value in (('source_kind','sdl'),('sha256','0'*64),('ROIs',[[16,190,32,195]])):
                with self.assertRaises(ValueError):check.native_panel_off(image,assets,[dict(reference,**{key:value})])

    def test_partial_panel_cannot_pass_original_off_reference(self):
        assets=check.round_assets(ROOT/'original_game')
        with tempfile.TemporaryDirectory() as directory:
            off=Image.new('RGB',(320,200));path=Path(directory)/'original-off.png';off.save(path)
            reference={'path':str(path),'sha256':check.sha(path),'source_kind':'original',
                'capture_backend':'Windows.Graphics.Capture HWND','ROIs':[list(box) for box in check.PANEL_BOXES]}
            for at in ((16,190),(257,190)):
                partial=off.copy();partial.paste(assets['restoration_border'].convert('RGB'),at)
                partial.putpixel(at,(0,0,0))
                # 79/80边框残留符合旧弱OFF条件，完整原版OFF图块必须拒绝。
                self.assertIsNotNone(check.panel_state(partial,assets,'off'))
                self.assertIsNone(check.native_panel_off(partial,assets,[reference]))

    def test_off_cannot_combine_two_separate_reference_frames(self):
        assets=check.round_assets(ROOT/'original_game')
        with tempfile.TemporaryDirectory() as directory:
            refs=[]
            for index,box in enumerate(check.PANEL_BOXES):
                original=Image.new('RGB',(320,200));original.paste((40,50,60),box)
                path=Path(directory)/('reference-'+str(index)+'.png');original.save(path)
                refs.append({'path':str(path),'sha256':check.sha(path),'source_kind':'original',
                    'capture_backend':'Windows.Graphics.Capture HWND','ROIs':[list(area) for area in check.PANEL_BOXES]})
            combined=Image.new('RGB',(320,200),(40,50,60))
            self.assertIsNone(check.native_panel_off(combined,assets,refs))

    def test_options_uses_XOR_domain_display_boolean_without_normalizing_raw(self):
        assets=check.round_assets(ROOT/'original_game');oracle=check.RoundOracle(assets,(100,100))
        values=[1,1,0,0xfe];image=Image.new('RGB',(320,200))
        commands=(18,20,22,24)
        for item,command in enumerate(commands):
            at=tuple(a+b for a,b in zip(oracle.focus,check.MENU_OFFSETS[item]))
            frame=assets['commands'][command*3+(item==3)].convert('RGB')
            image.paste(frame,at)
        self.assertIsNotNone(check.options(image,oracle,values,3))
        self.assertIsNotNone(check.options(image,oracle,[1,1,0,0xff],3))
        self.assertIsNone(check.options(image,oracle,[1,1,0,0],3))
        self.assertIsNone(check.options(image,oracle,values,0))
        with self.assertRaises(ValueError):check.options(image,oracle,[1,1,0,256],3)

    def test_full_measurement_cannot_hide_business_or_rawoption_changes(self):
        plain,_,_=check.fixture(ROOT/'original_game','browse-cycle')
        measured=bytearray(plain)
        world=(9,15);measured[check.META+5:check.META+9]=bytes((9,15,4,4))
        measured[-4:]=sum(measured[:-4]).to_bytes(4,'little')
        check.measured_state(plain,measured,world)
        for offset in (check.UNITS+0x40,check.META+15,0x8a3,0x30a3):
            damaged=bytearray(measured);damaged[offset]^=1;damaged[-4:]=sum(damaged[:-4]).to_bytes(4,'little')
            with self.assertRaises(AssertionError):check.measured_state(plain,damaged,world)

    def test_original_output_and_duplicate_scene_rejected_before_write(self):
        original=(ROOT/'original_game').resolve()
        with self.assertRaises(ValueError):check.run(SimpleNamespace(root=ROOT,artifacts=original,run=False))
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'not-created'
            with self.assertRaises(ValueError):check.run(SimpleNamespace(root=ROOT,artifacts=path,run=False,scenario=['browse-cycle']*2))
            self.assertFalse(path.exists())

    def test_default_prepare_never_requests_display_or_process(self):
        with tempfile.TemporaryDirectory() as directory,patch.object(check,'running') as launch,patch.object(check,'windows_display_requirement') as display:
            path=Path(directory)/'prepared'
            result=check.run(SimpleNamespace(root=ROOT,artifacts=path,run=False))
            self.assertEqual(result['status'],'prepared_only');self.assertFalse(result['ok'])
            launch.assert_not_called();display.assert_not_called()
            self.assertEqual(len(list(path.glob('*.SAV'))),3)


if __name__=='__main__':unittest.main()
