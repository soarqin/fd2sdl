"""G2 runner结果身份与阶段正证的纯负例；不启动窗口或读取原资源。"""
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock
from PIL import Image

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import fd2_g2_battle_oracle as oracle
import run_g2_battle_check as runner


def valid_plain(value):
    data=bytearray(oracle.SAVE_SIZE)
    data[oracle.META+1]=1
    data[oracle.UNITS+7]=value
    return oracle.checksum(data)


class G2BattleRunnerTest(unittest.TestCase):
    def save_case(self,directory,*,different_snapshot):
        capture=SimpleNamespace(artifacts=Path(directory),key=mock.Mock())
        save=Path(directory)/"FD2.SAV"
        before=oracle.crypt(valid_plain(1));measured=valid_plain(2)
        actual=oracle.crypt(valid_plain(3) if different_snapshot else measured)
        save.write_bytes(before)
        guard=[before,save.stat().st_mtime_ns]
        prepared={"assets":{},"source":b"", "original":Path("unused"),
                  "scenario":"teleport23-cancel-destination"}
        record={"kind":"sdl"}
        view=SimpleNamespace(**{name:mock.Mock(return_value={"kind":name})
            for name in ("closed_browse","menu","secondary","buttons")})
        def complete(*args):
            save.write_bytes(actual)
            return measured
        return capture,save,guard,prepared,record,view,complete,actual

    def test_snapshot_must_be_the_same_encoded_bytes_as_checked_plain(self):
        with tempfile.TemporaryDirectory(prefix="fd2-g2-runner-test-") as directory:
            capture,save,guard,prepared,record,view,complete,_=self.save_case(directory,different_snapshot=True)
            before_guard=guard[:]
            with mock.patch.object(runner,"RoundOracle",return_value=view), \
                 mock.patch.object(runner,"screen",return_value=(4,4)), \
                 mock.patch.object(runner,"wait_measured_save",side_effect=complete), \
                 mock.patch.object(runner.oracle,"check_result",return_value={"ok":True}) as compare:
                with self.assertRaisesRegex(AssertionError,"同一份encoded快照"):
                    runner.save_measurement(capture,prepared,save,guard,record,mock.Mock())
                compare.assert_not_called()
            self.assertEqual(guard,before_guard)
            self.assertNotIn("completed_save",record)

    def test_successful_save_binds_artifact_guard_and_result_to_one_snapshot(self):
        with tempfile.TemporaryDirectory(prefix="fd2-g2-runner-test-") as directory:
            capture,save,guard,prepared,record,view,complete,actual=self.save_case(directory,different_snapshot=False)
            with mock.patch.object(runner,"RoundOracle",return_value=view), \
                 mock.patch.object(runner,"screen",return_value=(4,4)), \
                 mock.patch.object(runner,"wait_measured_save",side_effect=complete), \
                 mock.patch.object(runner.oracle,"check_result",return_value={"ok":True}) as compare:
                runner.save_measurement(capture,prepared,save,guard,record,mock.Mock())
            self.assertEqual(Path(record["completed_save"]["path"]).read_bytes(),actual)
            self.assertEqual(guard,[actual,save.stat().st_mtime_ns])
            self.assertEqual(compare.call_args.args[3],oracle.crypt(actual))
            self.assertTrue(record["state_comparison"]["ok"])

    def test_teleport_destination_needs_actor_and_complete_restored_base(self):
        probe={"target":"target-resource","first_layer_colored_range":"complete-ROI"}
        prepared={"scenario":"teleport23-cancel-destination","assets":{},"probe":probe}
        for actor,roi in ((None,{"kind":"base"}),({"kind":"actor"},None),(None,None)):
            with self.subTest(actor=actor,roi=roi), \
                 mock.patch.object(runner,"focused_actor",return_value=actor), \
                 mock.patch.object(runner,"range_match",return_value=roi) as match:
                self.assertIsNone(runner.target_match(object(),prepared,destination=True))
                self.assertTrue(match.call_args.kwargs["base"])
        with mock.patch.object(runner,"focused_actor",return_value={"kind":"actor"}), \
             mock.patch.object(runner,"range_match",return_value={"kind":"base"}):
            self.assertTrue(runner.target_match(object(),prepared,destination=True)["destination"])

    def test_composite_target_requires_every_selector_sprite(self):
        prepared={"scenario":"composite34","assets":{},"probe":{},"target":{},"target_range":{},
                  "selector_checks":[((4,4),"sprite1",1),((28,4),"sprite2",2)]}
        with mock.patch.object(runner,"focused_actor",return_value={"kind":"actor"}), \
             mock.patch.object(runner,"range_match",return_value={"kind":"range"}), \
             mock.patch.object(runner,"match_indexed",side_effect=[{"ok":True},{"ok":False}]):
            self.assertIsNone(runner.target_match(object(),prepared))

    def test_banner_requires_opaque_pixels_and_preserves_transparent_background(self):
        banner=Image.frombytes("P",(2,2),bytes((0,1,2,0)))
        banner.putpalette([0,0,0,255,0,0,0,255,0]+[0]*759)
        frame=Image.new("RGB",(320,200),(40,80,120))
        frame.putpixel((121,84),(255,0,0));frame.putpixel((120,85),(0,255,0))
        prepared={"banner":banner}
        hit=runner.banner_match(frame,prepared)
        self.assertIsNotNone(hit)
        self.assertFalse(hit["transparent_background_compared"])
        self.assertEqual(hit["opaque_resource_pixels"]["checked_pixels"],2)
        frame.putpixel((120,84),(200,100,50))
        self.assertIsNotNone(runner.banner_match(frame,prepared))
        frame.putpixel((121,84),(0,0,255))
        self.assertIsNone(runner.banner_match(frame,prepared))

    def test_teleport_success_destination_needs_both_complete_positive_tiles(self):
        destination=Image.new("P",(24,24),20)
        base=Image.new("P",(24,24),30)
        palette=[channel for value in range(256) for channel in (value,value,value)]
        destination.putpalette(palette);base.putpalette(palette)
        for x in range(24):destination.putpixel((x,3),100)
        frame=Image.new("RGB",(320,200),(0,0,0))
        frame.paste(destination.convert("RGB"),(124,76));frame.paste(base.convert("RGB"),(28,28))
        prepared={"probe":{"destination_selected":{"composite":destination,"position":(124,76)},
            "first_layer_colored_range":{"base":base,"position":(28,28)}}}
        self.assertIsNotNone(runner.destination_match(frame,prepared))
        for position in ((130,80),(30,30)):
            changed=frame.copy();changed.putpixel(position,(255,0,0))
            with self.subTest(position=position):self.assertIsNone(runner.destination_match(changed,prepared))

    def reward_material(self,directory):
        """合成完整资源区域，实际走PNG/SHA/normalize/page_match，不替换身份门。"""
        palette=[channel for value in range(256) for channel in (value,value,value)]
        border=Image.new("P",(310,2),220);border.putpalette(palette)
        portrait=Image.new("P",(80,80),40);portrait.putpalette(palette)
        alternatives=[];frames=[]
        for gain in (1,2):
            text=Image.new("P",(320,200),0);text.putpalette(palette)
            text.paste(40+gain*70,(95,119,111,135))
            page={"fragment":488,"ordinal":0,"requires_key":False,"text":text,
                "portrait_frames":(0,),"portraits":[portrait],"portrait_position":(8,115)}
            frame=Image.new("RGB",(320,200),(0,0,0))
            frame.paste(border.convert("RGB"),(5,112));frame.paste(portrait.convert("RGB"),(8,115))
            frame.paste(text.convert("RGB"),(0,0),text.point(lambda pixel:255 if pixel else 0,mode="L"))
            alternatives.append((gain,page));frames.append(frame)
        frames.append(Image.new("RGB",(320,200),(0,0,0)))
        rows=[]
        for index,frame in enumerate(frames):
            path=Path(directory)/f"frame{index}.png";frame.save(path)
            rows.append({"path":str(path),"index":index,"sha256":runner.sha(path)})
        return {"assets":{"dialog_top":border},"reward_alternatives":alternatives},rows

    def test_derived_reward_gate_requires_one_gain_value_and_not_any_page(self):
        with tempfile.TemporaryDirectory(prefix="fd2-g2-reward-test-") as directory:
            prepared,paths=self.reward_material(directory)
            for rows in ((paths[2],),(paths[0],paths[1])):
                with self.subTest(rows=len(rows)),self.assertRaisesRegex(AssertionError,"唯一"):
                    runner.observed_reward(prepared,{"continuous_wgc":{"frames":list(rows)}})
            record={"continuous_wgc":{"frames":[paths[0],paths[2],paths[0]]}}
            runner.observed_reward(prepared,record)
            self.assertEqual(record["reward_page"]["gain"],1)
            self.assertEqual(record["reward_page"]["sha256"],paths[0]["sha256"])
            self.assertFalse(record["reward_page"]["key_sent"])

    def test_derived_reward_rejects_changed_png_before_matching(self):
        with tempfile.TemporaryDirectory(prefix="fd2-g2-reward-test-") as directory:
            prepared,rows=self.reward_material(directory)
            record={"continuous_wgc":{"frames":[rows[0]]}}
            runner.observed_reward(prepared,record)
            self.assertEqual(record["reward_page"]["gain"],1)
            # 换成另一幅有效PNG而保留原登记摘要，不能因新画面仍有奖励文字就接受。
            Path(rows[0]["path"]).write_bytes(Path(rows[1]["path"]).read_bytes())
            record={"continuous_wgc":{"frames":[rows[0]]}}
            with self.assertRaisesRegex(AssertionError,"PNG.*身份不符"):
                runner.observed_reward(prepared,record)
            self.assertNotIn("reward_page",record)


if __name__=="__main__":
    unittest.main()
