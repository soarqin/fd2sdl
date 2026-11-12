"""S0原生工具纯回归：原义、精确夹具写集、owner/输出边界与未知拒绝。"""
import copy
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from types import SimpleNamespace

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import fd2_s0_native as oracle
import run_s0_result_check as runner


def source(stage=0):
    plain = bytearray(0x59CB)
    plain[oracle.META + 1:oracle.META + 3] = bytes((12, stage))
    for raw in range(12):
        at = oracle.UNITS + raw * 80
        plain[at + 6] = 2 if raw < 5 else 1 if stage and raw < 11 else 0
        plain[at + 7] = raw
        struct.pack_into("<HH", plain, at + 0x40, 42, 42)
    plain[-4:] = sum(plain[:-4]).to_bytes(4, "little")
    return bytes(plain)


class NativeOracleTests(unittest.TestCase):
    def test_hp_is_not_result_predicate(self):
        before = source()
        for scenario in ("leader-hp0-visible", "hostile-hp0-visible"):
            plain, encoded, manifest = oracle.derive(before, scenario)
            self.assertEqual(oracle.original_result(plain), 0)
            self.assertEqual(manifest["expected_post_controller_result"], 0)
            self.assertEqual(len(encoded), len(before))
            self.assertEqual(manifest["controller_actor"], 0 if scenario == "leader-hp0-visible" else 1)
            self.assertTrue(any(row["offset"] < len(before) - 4 for row in manifest["changed_plain_bytes"]))

    def test_hp0_visible_leader_is_the_actual_controller_target(self):
        plain, _, manifest = oracle.derive(source(), "leader-hp0-visible")
        self.assertEqual(manifest["controller_actor"], 0)
        self.assertEqual(int.from_bytes(plain[oracle.UNITS + 0x40:oracle.UNITS + 0x42], "little"), 0)
        self.assertEqual(plain[oracle.UNITS + 6], 2)
        self.assertEqual(plain[oracle.UNITS + 5] & 0x85, 0)

    def test_hp0_target_does_not_bypass_original_flags_or_side_gate(self):
        for field, value in ((5, 4), (5, 128), (6, 1)):
            plain = bytearray(source())
            plain[oracle.UNITS + field] = value
            plain[-4:] = sum(plain[:-4]).to_bytes(4, "little")
            with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                oracle.derive(bytes(plain), "leader-hp0-visible")

    def test_hidden_leader_defeat_overrides_no_hostiles(self):
        plain, _, manifest = oracle.derive(source(), "hidden-defeat-priority")
        self.assertEqual(oracle.original_result(plain), 2)
        self.assertEqual(manifest["expected_post_controller_result"], 2)
        hp_offsets = {oracle.UNITS + raw * 80 + field for raw in range(12) for field in (0x40, 0x41)}
        self.assertTrue(all(row["offset"] not in hp_offsets for row in manifest["changed_plain_bytes"]))

    def test_hidden_hostiles_with_visible_leader_are_victory(self):
        plain, _, manifest = oracle.derive(source(), "hidden-victory")
        self.assertFalse(plain[oracle.UNITS + 5] & 1)
        self.assertEqual(oracle.original_result(plain), 1)
        self.assertEqual(manifest["expected_post_controller_result"], 1)

    def test_villager_defeat_overrides_victory_with_leader_visible(self):
        plain, _, _ = oracle.derive(source(1), "stage1-villager-priority")
        self.assertFalse(plain[oracle.UNITS + 5] & 1)
        self.assertEqual(oracle.original_result(plain), 2)
        changed = bytearray(plain)
        changed[oracle.UNITS + 5 * 80 + 5] &= ~1
        self.assertEqual(oracle.original_result(changed), 1)

    def test_bad_normal_domain_is_not_original_ub_emulation(self):
        for stage, count in ((0, 0), (1, 10), (2, 12)):
            plain = bytearray(source())
            plain[oracle.META + 1:oracle.META + 3] = bytes((count, stage))
            with self.assertRaises(ValueError):
                oracle.original_result(plain)

    def test_bad_source_checksum_and_unknown_scenario_rejected(self):
        plain = bytearray(source())
        plain[1] = 1
        with self.assertRaises(ValueError):
            oracle.derive(bytes(plain), "leader-hp0-visible")
        with self.assertRaises(ValueError):
            oracle.derive(source(), "unknown")

    def test_observation_rejects_early_result_and_hidden_hp_rewrite(self):
        plain, _, _ = oracle.derive(source(), "leader-hp0-visible")
        actual = {"stage": 0, "count": 12, "cached_result": 0,
                  "actors": oracle.active_fields(plain[oracle.UNITS:oracle.UNITS + 12 * 80], 12)}
        self.assertTrue(oracle.check_active(actual, plain)["ok"])
        for field, value in (("cached_result", 2), ("stage", 1), ("count", 11)):
            changed = copy.deepcopy(actual)
            changed[field] = value
            self.assertFalse(oracle.check_active(changed, plain)["ok"])
        for field, value in (("hp", 42), ("flags", 1), ("unit_id", 99)):
            changed = copy.deepcopy(actual)
            changed["actors"][0][field] = value
            self.assertFalse(oracle.check_active(changed, plain)["ok"])

    def test_active_owner_requires_full_counted_records(self):
        with self.assertRaises(ValueError):
            oracle.active_fields(bytes(79), 1)
        with self.assertRaises(ValueError):
            oracle.active_fields(bytes(97 * 80), 97)

    def test_battle_stack_owner_is_unique_and_pid_bound(self):
        lines = ["   0 Id: 1234.5678 Suspend: 1", "  0a 00000000`12345678 00000000`98765432 fd2sdl!fd2_field_play_run_context+0x123"]
        frame = oracle.battle_frame(lines, 0x1234, "fd2sdl")
        self.assertEqual(frame["thread_id"], 0x5678)
        self.assertEqual(frame["frame"], 10)
        for invalid in (lines + lines[-1:], lines[1:], lines):
            with self.assertRaises(ValueError):
                oracle.battle_frame(invalid, 0x9999 if invalid is lines else 0x1234, "fd2sdl")

    def test_output_must_be_new_research_directory_outside_resolved_source(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            original = root / "source"
            original.mkdir()
            good = root / "build/s0/new"
            runner.output_guard(root, original, good)
            for invalid in (original / "child", root / "docs/out", root / "build/s0/exists"):
                invalid.mkdir(parents=True)
                with self.assertRaises(ValueError):
                    runner.output_guard(root, original, invalid)


def snapshot_requests(*, before_cr0=0x11, after_cr0=0x10, after_flags=2):
    before = f"170 178 1ea245 1f15f8 1f1604 {before_cr0:x} 202"
    after = f"f71 f71 2aeb 1e2182 21c2 {after_cr0:x} {after_flags:x}"
    ev = "EXEC EV " + " ".join(oracle.rpc_api.REGISTERS)
    values = [(ev, [before]), ("EXEC SELINFO CS", ["CS: b:00000000 type:1A parbg", "l:FFFFFFFF dpl : 0 10011"]),
              ("EXEC SELINFO DS", ["DS: b:00000000 type:12 parbg", "l:FFFFFFFF dpl : 0 10011"]),
              ("EXEC MEMDUMPBIN 178:0 1000000 isolated.bin", ["DEBUG: Memory dump binary success."]),
              (ev, [after]), ("EXEC SELINFO 178", ["  : b:00000000 type:12 parbg", "l:FFFFFFFF dpl : 0 10011"])]
    return [{"command": command, "response": response, "ok": True} for command, response in values]


TRANSIENT_ERROR = "RPC前后protected descriptor改变；不拼接不同视图"


class SnapshotAttemptTests(unittest.TestCase):
    def test_only_identified_after_mode_transition_can_retry(self):
        self.assertIsNotNone(oracle.snapshot_retry_reason(snapshot_requests(), ValueError(TRANSIENT_ERROR)))
        self.assertIsNotNone(oracle.snapshot_retry_reason(snapshot_requests(after_cr0=0x11, after_flags=0x20202), ValueError(TRANSIENT_ERROR)))
        cases = [snapshot_requests(before_cr0=0x10), snapshot_requests(after_cr0=0x11, after_flags=0x202)]
        for changed_descriptor in ("b:00001000 type:12", "b:00000000 type:13"):
            requests = snapshot_requests()
            requests[-1]["response"][0] = changed_descriptor
            cases.append(requests)
        requests = snapshot_requests()
        requests[-1]["response"][-1] = "l:FFFFEFFF dpl : 0 10011"
        cases.append(requests)
        requests = snapshot_requests()
        requests[-1]["response"][-1] = "l:FFFFFFFF dpl : 3 11111"
        cases.append(requests)
        requests = snapshot_requests()
        requests[-1]["command"] = "EXEC SELINFO 180"
        cases.append(requests)
        requests = snapshot_requests()
        requests[3]["ok"] = False
        cases.append(requests)
        for requests in cases:
            with self.subTest(requests=requests):
                self.assertIsNone(oracle.snapshot_retry_reason(requests, ValueError(TRANSIENT_ERROR)))
        for error in (ValueError("MEMDUMPBIN实际长度不同"), ValueError("RPC本轮PID/连接归属或存活改变"), OSError(TRANSIENT_ERROR)):
            self.assertIsNone(oracle.snapshot_retry_reason(snapshot_requests(), error))

    def session(self, outcomes):
        record = {"requests": []}
        calls = []

        def snapshot(directory, label):
            calls.append(directory)
            outcome = outcomes[len(calls) - 1]
            record["requests"].extend(snapshot_requests() if outcome else snapshot_requests(after_cr0=0x11))
            data = b"discarded-data" if outcome else b"accepted-independent-data"
            path = directory / (label + "-ram.bin")
            path.write_bytes(data)
            if outcome:
                raise outcome
            return data, {"path": str(path), "sha256": oracle.digest(data)}

        return SimpleNamespace(record=record, snapshot=snapshot), calls

    def test_transient_dump_is_sealed_discarded_and_new_attempt_is_independent(self):
        session, calls = self.session([ValueError(TRANSIENT_ERROR), None])
        with tempfile.TemporaryDirectory() as directory:
            record = {}
            data, receipt = oracle.original_snapshot(session, directory, "active", record)
            self.assertEqual(data, b"accepted-independent-data")
            self.assertEqual(len(calls), 2)
            self.assertNotEqual(calls[0], calls[1])
            attempts = record["rpc_snapshot_attempts"]
            self.assertFalse(attempts[0]["accepted"])
            self.assertTrue(attempts[0]["retry_scheduled"])
            self.assertFalse(attempts[0]["discarded_dump"]["decoded_or_reused"])
            self.assertTrue(Path(attempts[0]["discarded_dump"]["path"]).is_file())
            self.assertTrue(Path(attempts[0]["requests"]["path"]).is_file())
            self.assertTrue(attempts[1]["accepted"])
            self.assertEqual(receipt["path"], attempts[1]["snapshot"]["path"])

    def test_three_transitions_exhaust_budget_without_accepting_any_dump(self):
        session, calls = self.session([ValueError(TRANSIENT_ERROR)] * 3)
        with tempfile.TemporaryDirectory() as directory:
            record = {}
            with self.assertRaisesRegex(ValueError, TRANSIENT_ERROR):
                oracle.original_snapshot(session, directory, "active", record)
            self.assertEqual(len(calls), 3)
            self.assertEqual(record["rpc_snapshot_attempt_limit"], 3)
            self.assertTrue(all(not row["accepted"] for row in record["rpc_snapshot_attempts"]))
            self.assertFalse(record["rpc_snapshot_attempts"][-1]["retry_scheduled"])

    def test_owner_or_short_read_failure_is_not_retried(self):
        for message in ("MEMDUMPBIN实际长度不同", "RPC本轮PID/连接归属或存活改变"):
            session, calls = self.session([ValueError(message)])
            with tempfile.TemporaryDirectory() as directory:
                record = {}
                with self.assertRaisesRegex(ValueError, message):
                    oracle.original_snapshot(session, directory, "active", record)
                self.assertEqual(len(calls), 1)
                self.assertIsNone(record["rpc_snapshot_attempts"][0]["retry_classification"])

    def test_real_descriptor_change_stops_after_first_rejected_attempt(self):
        session, calls = self.session([ValueError(TRANSIENT_ERROR)])
        original_snapshot = session.snapshot

        def unsafe_snapshot(directory, label):
            try:
                return original_snapshot(directory, label)
            except ValueError:
                session.record["requests"][-1]["response"][0] = "  : b:00001000 type:12 parbg"
                raise

        session.snapshot = unsafe_snapshot
        with tempfile.TemporaryDirectory() as directory:
            record = {}
            with self.assertRaisesRegex(ValueError, TRANSIENT_ERROR):
                oracle.original_snapshot(session, directory, "active", record)
            self.assertEqual(len(calls), 1)
            attempt = record["rpc_snapshot_attempts"][0]
            self.assertFalse(attempt["accepted"])
            self.assertFalse(attempt["retry_scheduled"])
            self.assertIsNone(attempt["retry_classification"])

    def ring_memory(self):
        return (b"\0\1" + bytes(65534)) * 256

    def test_only_complete_nonzero_exact_64k_ring_is_classified(self):
        error = ValueError("原码anchor在本轮snapshot没有唯一match：0x2615c []")
        ring = self.ring_memory()
        self.assertIsNotNone(oracle.wrapped_address_retry_reason(ring, error))
        changed = bytearray(ring)
        changed[-1] = 1
        for payload in (bytes(oracle.rpc_api.RAM_SIZE), ring[:-1], bytes(changed), b"not-a-RAM"):
            self.assertIsNone(oracle.wrapped_address_retry_reason(payload, error))
        for invalid_error in (ValueError("本轮object2合法fixup基址不一致"), ValueError("MEMDUMPBIN实际长度不同"),
                              OSError(str(error))):
            self.assertIsNone(oracle.wrapped_address_retry_reason(ring, invalid_error))

    def payload_session(self, payloads):
        record = {"requests": []}
        calls = []

        def snapshot(directory, label):
            payload = payloads[len(calls)]
            calls.append(directory)
            record["requests"].extend(snapshot_requests(after_cr0=0x11))
            path = directory / (label + "-ram.bin")
            path.write_bytes(payload)
            return payload, {"path": str(path), "sha256": oracle.digest(payload), "size": len(payload)}

        return SimpleNamespace(record=record, snapshot=snapshot), calls

    def test_rejected_ring_restarts_whole_capture_and_revalidates_owner(self):
        ring = self.ring_memory()
        independent = bytearray(ring)
        independent[-1] = 1
        independent = bytes(independent)
        session, calls = self.payload_session([ring, independent])
        decoded = []

        def owner_decoder(memory, receipt):
            decoded.append(oracle.digest(memory))
            if memory == ring:
                raise ValueError("原码anchor在本轮snapshot没有唯一match：0x2615c []")
            return {"full_owner_and_HP_flags_validated": True}

        with tempfile.TemporaryDirectory() as directory:
            record = {}
            memory, receipt = oracle.original_snapshot(session, directory, "active", record, owner_decoder=owner_decoder)
            self.assertEqual(memory, independent)
            self.assertEqual(len(calls), 2)
            self.assertEqual(len(decoded), 2)
            self.assertNotEqual(decoded[0], decoded[1])
            first, second = record["rpc_snapshot_attempts"]
            self.assertFalse(first["accepted"])
            self.assertTrue(first["retry_classification"]["bridge_snapshot_passed"])
            self.assertFalse(first["retry_classification"]["owner_anchor_validated"])
            self.assertFalse(first["discarded_dump"]["decoded_or_reused"])
            self.assertTrue(second["accepted"])
            self.assertTrue(second["owner_validated"])
            self.assertEqual(receipt["sha256"], decoded[1])
            self.assertTrue(record["validated_owner"]["full_owner_and_HP_flags_validated"])

    def test_unexplained_anchor_failure_zero_short_and_inexact_ring_do_not_retry(self):
        ring = self.ring_memory()
        nonperiodic = bytearray(ring)
        nonperiodic[-1] = 1
        for memory in (bytes(nonperiodic), bytes(oracle.rpc_api.RAM_SIZE), ring[:-1]):
            session, calls = self.payload_session([memory])

            def reject(memory, receipt):
                raise ValueError("原码anchor在本轮snapshot没有唯一match：0x2615c []")

            with tempfile.TemporaryDirectory() as directory:
                record = {}
                with self.assertRaises(ValueError):
                    oracle.original_snapshot(session, directory, "active", record, owner_decoder=reject)
                self.assertEqual(len(calls), 1)
                self.assertIsNone(record["rpc_snapshot_attempts"][0]["retry_classification"])
                self.assertNotIn("validated_owner", record)

    def test_three_complete_ring_attempts_fail_without_promoting_bridge_success(self):
        session, calls = self.payload_session([self.ring_memory()] * 3)

        def reject(memory, receipt):
            raise ValueError("原码anchor在本轮snapshot没有唯一match：0x2615c []")

        with tempfile.TemporaryDirectory() as directory:
            record = {}
            with self.assertRaises(ValueError):
                oracle.original_snapshot(session, directory, "active", record, owner_decoder=reject)
            self.assertEqual(len(calls), 3)
            self.assertTrue(all(not row["accepted"] for row in record["rpc_snapshot_attempts"]))
            self.assertFalse(record["rpc_snapshot_attempts"][-1]["retry_scheduled"])
            self.assertNotIn("validated_owner", record)


if __name__ == "__main__":
    unittest.main()
