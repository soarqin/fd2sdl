"""Item定向runner的独立cache拒绝门；不启动窗口或写原资源。"""
from pathlib import Path
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import fd2_g2_battle_oracle as oracle
import run_g2_item_check as runner


def fixture():
    data=bytearray(oracle.SAVE_SIZE)
    data[oracle.META+1]=5
    for raw,(identity,cache) in enumerate(((0,5),(9,7),(96,8),(96,8),(4,3))):
        data[oracle.UNITS+raw*80+2]=cache
        data[oracle.UNITS+raw*80+7]=identity
    return oracle.checksum(data)


def native_loaded(source):
    result=bytearray(source)
    # 本固定夹具的原first-seen序列为0,9,96,4；两条96共用class2。
    for raw,value in enumerate((0,1,2,2,3)):
        result[oracle.UNITS+raw*80+2]=value
    return oracle.checksum(result)


class G2ItemRunnerTest(unittest.TestCase):
    def test_each_engine_has_a_separate_cache_contract(self):
        source=fixture();native=native_loaded(source)
        self.assertTrue(runner.check_cache(source,native,"original")["ok"])
        self.assertTrue(runner.check_cache(source,source,"sdl")["ok"])
        self.assertFalse(runner.check_cache(source,source,"original")["ok"])
        self.assertFalse(runner.check_cache(source,native,"sdl")["ok"])

    def test_every_cache_byte_is_guarded_including_duplicate_identity(self):
        source=fixture()
        for engine,good in (("original",native_loaded(source)),("sdl",source)):
            for raw in range(5):
                bad=bytearray(good);bad[oracle.UNITS+raw*80+2]^=1
                with self.subTest(engine=engine,raw=raw):
                    self.assertFalse(runner.check_cache(source,oracle.checksum(bad),engine)["ok"])

    def test_identity_and_source_count_cannot_be_normalized_as_cache(self):
        source=fixture()
        for at,value in ((oracle.META+1,4),(oracle.UNITS+7,1),(oracle.UNITS+3*80+7,97)):
            bad=bytearray(source);bad[at]=value
            for engine in ("original","sdl"):
                with self.subTest(engine=engine,offset=at),self.assertRaises(ValueError):
                    runner.check_cache(source,oracle.checksum(bad),engine)

    def test_unknown_engine_invalid_count_and_bad_checksum_fail_closed(self):
        source=fixture()
        for engine in ("",None,"native","SDL"):
            with self.subTest(engine=engine),self.assertRaises(ValueError):
                runner.check_cache(source,source,engine)
        for count in (0,97,255):
            bad=bytearray(source);bad[oracle.META+1]=count;bad=oracle.checksum(bad)
            with self.subTest(count=count),self.assertRaises(ValueError):runner.check_cache(bad,bad,"sdl")
        with self.assertRaises(ValueError):runner.check_cache(source,source[:-1],"sdl")
        with self.assertRaises(ValueError):runner.check_cache(source,source[:-4]+bytes(4),"sdl")

    def test_actual_start_cache_tampering_is_rejected_without_observed_exemption(self):
        path=Path(__file__).resolve().parents[2]/"original_game/START.SAV"
        if not path.is_file():self.skipTest("未提供只读START来源，未执行真实来源负例")
        encoded=path.read_bytes()
        self.assertEqual(oracle.digest(encoded),oracle.START_SHA256)
        source=oracle.valid_plain(oracle.crypt(encoded))
        self.assertEqual([oracle.actor(source,raw)[7] for raw in range(12)],[0,9,4,30]+[96]*8)
        native=bytearray(source)
        for raw,value in enumerate((0,1,2,3)+tuple([4]*8)):
            native[oracle.UNITS+raw*80+2]=value
        for engine,good in (("original",oracle.checksum(native)),("sdl",source)):
            self.assertTrue(runner.check_cache(source,good,engine)["ok"])
            for raw in range(12):
                bad=bytearray(good);bad[oracle.UNITS+raw*80+2]^=0x40
                with self.subTest(engine=engine,raw=raw):
                    self.assertFalse(runner.check_cache(source,oracle.checksum(bad),engine)["ok"])


if __name__=="__main__":unittest.main()
