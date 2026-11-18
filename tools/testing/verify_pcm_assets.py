"""Independent fixed-profile DIG asset oracle; no SDL/device/guest operation.

Original 40240/404c0/49690/49891/49923 defines the integer formulas.
The catalog pins independently qualified sources; it is not an output oracle.
"""
import argparse
import hashlib
import json
import struct
import subprocess
import sys
import threading
import types
import time
from pathlib import Path

RAW_SIZE = 3382481
RECORD_SIZE = 23 * 4 + 8192 + 4096


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def freeze(path):
    path = Path(path).resolve()
    a = path.stat()
    raw = path.read_bytes()
    b = path.stat()
    if (a.st_size, a.st_mtime_ns) != (b.st_size, b.st_mtime_ns) or len(raw) != a.st_size:
        raise ValueError("input changed during read: " + str(path))
    return raw, dict(path=str(path), size=len(raw), mtime_ns=a.st_mtime_ns, sha256=sha(raw))


def directory(raw):
    if raw[:6] != b"LLLLLL" or len(raw) < 10:
        raise ValueError("archive signature")
    end, = struct.unpack_from("<I", raw, 6)
    if not 10 <= end < len(raw):
        raise ValueError("archive table extent")
    offsets = []
    for at in range(6, end - 3, 4):
        value, = struct.unpack_from("<I", raw, at)
        if value >= len(raw) or offsets and value <= offsets[-1]:
            break
        offsets.append(value)
    if not offsets or offsets[0] != end:
        raise ValueError("archive first offset")
    return [(v, offsets[i + 1] if i + 1 < len(offsets) else len(raw))
            for i, v in enumerate(offsets)]


def output_path(path, original, resource):
    path = Path(path).resolve()
    protected = (Path(original).resolve(), Path(resource).resolve().parent,
                 (Path(__file__).resolve().parents[2] / "original_game").resolve())
    if path.exists() or any(path == root or root in path.parents for root in protected):
        raise ValueError("output must be fresh and outside resolved original/resource directories")
    return path


def catalog_inputs(raw, catalog):
    if len(raw) != RAW_SIZE or sha(raw) != catalog["resource_sha256"]:
        raise ValueError("resource pin")
    if catalog["schema"] != "fd2.f5.pcm-assets-catalog.v1":
        raise ValueError("catalog schema")
    top = directory(raw)
    rows = {}
    for row in catalog["rows"]:
        key = row["bank"], row["child"]
        if key in rows:
            raise ValueError("duplicate catalog binding")
        lo, hi = top[key[0]]
        a, z = directory(raw[lo:hi])[key[1]]
        payload = raw[lo + a:lo + z]
        if (lo + a, len(payload), sha(payload)) != (row["offset"], row["bytes"], row["sha256"]):
            raise ValueError("catalog actual source span")
        rows[key] = dict(row, payload=payload)
    if len(rows) != 111 or len({k[0] for k in rows}) != 26 or len({v["sha256"] for v in rows.values()}) != 78:
        raise ValueError("catalog denominator")
    return rows


def cases(rows):
    result = []
    def play(h, b, c, loops=1):
        return (1, h, b, c, loops, 0)
    def services(n, start=0):
        return [(3, (start + i) % 2, 0, 0, 0, 0) for i in range(n)]
    for (b, c), row in rows.items():
        n = (2 * row["bytes"] - 1 + 2047) // 2048
        for h in (0, 1):
            result.append((f"{b}/{c}/P{h}", [play(h, b, c)] + services(n)))
    result += [
        ("two-P-real-both-sign-clip", [play(0, 53, 1), play(1, 53, 1)] + services(3)),
        ("unequal-duration-three-empty", [play(0, 31, 0), play(1, 77, 0)] + services(44)),
        ("stop8-replace-submitted-tail", [play(0, 77, 0), play(1, 53, 1)] + services(1) +
         [(2, 0, 0, 0, 0, 0), play(0, 31, 0)] + services(5, 1)),
        ("source-bound-town-loop3", [play(0, 31, 1, 3)] + services(13))]
    if len(result) != 226 or sum(e[0] == 3 for _, events in result for e in events) != 1556:
        raise ValueError("request denominator")
    return result


class Model:
    def __init__(self, rows):
        self.rows = rows
        self.p = [dict(status=2, position=0, loops=1, length=0, offset=0xffffffff, payload=b"") for _ in range(2)]
        self.build = [0] * 2048
        self.dma = [bytes([128]) * 2048, bytes([128]) * 2048]
        self.playing = self.empty = self.entered = 0
        self.last = -1
        self.starts = self.reads = self.pauses = 0
        self.syncs = 1

    def event(self, event):
        op, h, b, c, loops, _ = event
        if op == 1:
            self.syncs += 1
            row = self.rows[b, c]
            self.p[h] = dict(status=4, position=0, loops=loops, length=row["bytes"],
                             offset=row["offset"], payload=row["payload"])
            if not self.playing:
                self.starts += 1
                self.playing = 1
            return 0
        if op == 2:
            self.syncs += 1
            if self.p[h]["status"] == 4:
                self.p[h]["status"] = 8
            return 0
        if op != 3 or not self.playing or h == self.last:
            raise ValueError("unexpected oracle service input")
        self.reads += 1
        self.last, self.build, self.entered = h, [0] * 2048, 0
        for sample in self.p:
            if sample["status"] != 4:
                continue
            self.entered += 1
            written = 0
            while written < 2048:
                remaining = sample["length"] - sample["position"]
                take = min(2048 - written, 2 * remaining - 1)
                if take < 0:
                    raise ValueError("bad independent sample state")
                # floor(Fraction(1,2)+j/2), independent closed source relation.
                for j in range(take):
                    at = sample["position"] + (1 + j) // 2
                    self.build[written + j] += (sample["payload"][at] - 128) * 202
                sample["position"] += (1 + take) // 2
                written += take
                if sample["position"] < sample["length"]:
                    break
                if sample["loops"] == 1:
                    sample["status"] = 2
                    break
                if sample["loops"]:
                    sample["loops"] -= 1
                sample["position"] = 0
        self.dma[h ^ 1] = bytes((max(-32768, min(32767, v)) + 32768) // 256 for v in self.build)
        if self.entered:
            self.empty = 0
        else:
            previous = self.empty
            self.empty += 1
            if previous == 2:
                self.pauses += 1
                self.playing = 0
        return 1

    def packet(self, ci, sequence, op, result):
        values = [ci, sequence, op, result]
        for p in self.p:
            values += [p[k] for k in ("status", "position", "loops", "length", "offset")]
        values += [self.playing, self.last & 0xffffffff, self.entered, self.empty, 0,
                   self.starts, self.reads, self.pauses, self.syncs]
        return struct.pack("<23I2048i", *values, *self.build) + b"".join(self.dma)


def request(raw, requests):
    data = bytearray(b"D2AS01\0\0" + struct.pack("<II", len(raw), len(requests)) + raw)
    for ci, (_, events) in enumerate(requests):
        data += struct.pack("<II", ci, len(events))
        for event in events:
            data += struct.pack("<6I", *event)
    return data


def exact(stream, size):
    out = bytearray()
    while len(out) < size:
        part = stream.read(size - len(out))
        if not part:
            raise ValueError(f"truncated stdout: wanted {size}, got {len(out)}")
        out += part
    return bytes(out)


def compare_packet(actual, expected):
    if len(actual) != RECORD_SIZE or len(expected) != RECORD_SIZE:
        raise ValueError("packet size")
    if actual != expected:
        at = next(i for i, (a, b) in enumerate(zip(actual, expected)) if a != b)
        raise ValueError(f"packet first difference byte={at} actual={actual[at]} expected={expected[at]}")


def check_facts(facts):
    errors = []
    for before in facts:
        try:
            if freeze(before["path"])[1] != before:
                errors.append(dict(path=before["path"], error="late input guard changed"))
        except Exception as error:
            errors.append(dict(path=before["path"], error=str(error)))
    return errors


def write_report(path, report):
    try:
        with Path(path).open("x", encoding="utf-8", newline="\n") as output:
            json.dump(report, output, ensure_ascii=False, indent=2)
            output.write("\n")
        return None
    except Exception as error:
        return dict(stage="report", error=str(error))


def close_owned(process):
    """Cleanup failures remain secondary; every OS operation is independently guarded."""
    result, errors = {}, []
    try:
        live = process.poll() is None
    except Exception as error:
        errors.append(dict(stage="close_poll", error=str(error)))
        live = True
    if live:
        try:
            process.terminate()
        except Exception as error:
            errors.append(dict(stage="close_terminate", error=str(error)))
    try:
        result["exit_code"] = process.wait(timeout=5)
    except Exception as error:
        errors.append(dict(stage="close_wait", error=str(error)))
        try:
            process.kill()
        except Exception as late:
            errors.append(dict(stage="close_kill", error=str(late)))
        try:
            result["exit_code"] = process.wait(timeout=5)
        except Exception as late:
            errors.append(dict(stage="close_final_wait", error=str(late)))
    try:
        result["owned_ended"] = process.poll() is not None
    except Exception as error:
        errors.append(dict(stage="close_final_poll", error=str(error)))
        result["owned_ended"] = False
    return result, errors


def helper(name, facts):
    path = Path(__file__).resolve().with_name(name + ".py")
    raw, fact = freeze(path); facts.append(fact)
    module = types.ModuleType("_fd2_pcm_" + name)
    module.__file__ = str(path)
    sys.modules[module.__name__] = module
    exec(compile(raw, str(path), "exec"), module.__dict__)
    return module


def run(args):
    report = dict(schema="fd2.f5.pcm-real-assets-C.v1", passed=False, actual_C_cases=0,
                  actual_services=0, actual_packets=0, first_error=None, secondary_errors=[],
                  original_CPU=0, GUI=0, devices=0, natural_cue=False, PIC_timing=False,
                  full_waveform=False, full_A18_or_F5=False)
    facts = []
    process = timer = writer = None
    writer_errors, stderr_data = [], bytearray()
    output = None
    actual = expected = b""
    try:
        output = output_path(args.output, args.original_directory, args.resource)
        catalog_raw, fact = freeze(args.catalog)
        facts.append(fact)
        if sha(catalog_raw) != args.catalog_sha256:
            raise ValueError("catalog pin")
        catalog = json.loads(catalog_raw)
        if args.source_mode == "portable-bytes":
            if args.source or not args.original_exe or not args.source_profile_sha256:
                raise ValueError("portable source arguments; no legacy bundle fallback")
            provider = helper("pcm_asset_sources", facts)
            report["source_qualification"] = provider.portable(
                args.source_profile, args.source_profile_sha256, args.original_exe,
                catalog, Path(args.project_root).resolve() / "tools", freeze, facts)
            report["schema"] = "fd2.f5.pcm-real-assets-C.v2"
        else:
            source_paths = dict(value.split("=", 1) for value in args.source)
            if len(source_paths) != len(args.source) or set(source_paths) != set(catalog["sources"]):
                raise ValueError("source role set/duplicate")
            for role, pin in catalog["sources"].items():
                blob, fact = freeze(source_paths[role]); facts.append(fact)
                if sha(blob) != pin:
                    raise ValueError("source pin: " + role)
                if role == "strict_image":
                    for chunk in catalog["chunks"]:
                        if sha(blob[chunk["start"]:chunk["end"]]) != chunk["sha256"]:
                            raise ValueError("strict source chunk")
            report["source_qualification"] = dict(mode="legacy-local-bundle",
                exact_historical_bundle_replay=True, fresh_semantic_or_IDA_review=False)
        raw, fact = freeze(args.resource); facts.append(fact)
        rows = catalog_inputs(raw, catalog)
        requests = cases(rows)
        manifest_raw, fact = freeze(args.build_manifest); facts.append(fact)
        manifest = json.loads(manifest_raw)
        producer = helper("pcm_asset_build", facts)
        report["producer_qualification"] = producer.validate(
            manifest, args.exe, facts, freeze, args.project_root,
            portable=args.source_mode == "portable-bytes")
        if check_facts(facts):
            raise ValueError("pre-run build/source guard")
        exe_raw, fact = freeze(args.exe); facts.append(fact)
        _, own = freeze(__file__); facts.append(own)
        output.mkdir(parents=True)
        report.update(resource=dict(size=len(raw), sha256=sha(raw)),
                      catalog_sha256=sha(catalog_raw), PE=fact, build_manifest_sha256=sha(manifest_raw),
                      child_bindings=111, unique_payloads=78, histories=226,
                      given_half_not_measured_IRQ=True)
        process = subprocess.Popen([str(Path(args.exe).resolve()), "--assets-stream"],
                                   cwd=output, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        report["owned_PID"] = process.pid
        timed_out = []
        def timecap():
            timed_out.append(True)
            if process.poll() is None:
                process.kill()
        timer = threading.Timer(args.timeout, timecap); timer.daemon = True; timer.start()
        payload = request(raw, requests)
        def feed():
            try:
                process.stdin.write(payload); process.stdin.close()
            except Exception as error:
                writer_errors.append(str(error))
        def drain_error():
            while True:
                block = process.stderr.read(4096)
                if not block:
                    return
                if len(stderr_data) + len(block) > 65536:
                    writer_errors.append("stderr cap")
                    process.kill()
                    return
                stderr_data.extend(block)
        writer = threading.Thread(target=feed, daemon=True); writer.start()
        error_reader = threading.Thread(target=drain_error, daemon=True); error_reader.start()
        if exact(process.stdout, 12) != b"D2AO01\0\0" + struct.pack("<I", len(raw)):
            raise ValueError("loaded snapshot header")
        for at in range(0, len(raw), 65536):
            if exact(process.stdout, min(65536, len(raw) - at)) != raw[at:at + 65536]:
                raise ValueError("loaded snapshot byte mismatch")
        report["loaded_snapshot_same_frozen_bytes"] = True
        all_cases = []
        for ci, (name, events) in enumerate(requests):
            model, actual_hash, expected_hash = Model(rows), hashlib.sha256(), hashlib.sha256()
            for sequence, event in enumerate([None] + events):
                op = event[0] if event else 0
                result = model.event(event) if event else 0
                expected = model.packet(ci, sequence, op, result)
                report["watch"] = dict(case=ci, name=name, sequence=sequence, op=op)
                actual = exact(process.stdout, RECORD_SIZE)
                compare_packet(actual, expected)
                actual_hash.update(actual); expected_hash.update(expected)
                report["actual_packets"] += 1
                report["actual_services"] += op == 3
            all_cases.append(dict(index=ci, name=name, packets=1 + len(events),
                                  actual_sha256=actual_hash.hexdigest(), expected_sha256=expected_hash.hexdigest()))
            report["actual_C_cases"] += 1
        footer = b"D2AE01\0\0" + struct.pack("<III", 226, report["actual_packets"], 1556)
        if exact(process.stdout, 20) != footer or process.stdout.read(1):
            raise ValueError("exact footer/EOF")
        code = process.wait(timeout=5)
        writer.join(5); error_reader.join(5)
        if code or writer_errors or timed_out or stderr_data:
            raise ValueError(f"child exit={code}, timeout={bool(timed_out)}, IO={writer_errors}, stderr={stderr_data.decode(errors='replace')}")
        report["cases"] = all_cases
        report["compared_packet_bytes"] = report["actual_packets"] * RECORD_SIZE
        report["compared_build_and_DMA_bytes"] = report["actual_packets"] * 12288
        report["state_words_per_packet"] = 23
        report["request_sha256"] = sha(payload)
    except Exception as error:
        report["first_error"] = dict(type=type(error).__name__, error=str(error))
        if output and output.exists() and actual and expected:
            try:
                # Only one bounded mismatch packet, never a complete recording.
                (output / "first-actual.bin").write_bytes(actual)
                (output / "first-expected.bin").write_bytes(expected)
            except Exception as late:
                report["secondary_errors"].append(dict(stage="difference", error=str(late)))
    finally:
        if timer:
            try:
                timer.cancel()
            except Exception as error:
                report["secondary_errors"].append(dict(stage="timer_cancel", error=str(error)))
        if process:
            closed, late = close_owned(process)
            report.update(closed)
            report["secondary_errors"] += late
        if writer:
            try:
                writer.join(5)
            except Exception as error:
                report["secondary_errors"].append(dict(stage="writer_join", error=str(error)))
        report["secondary_errors"] += [dict(stage="input_writer", error=e) for e in writer_errors]
        try:
            report["secondary_errors"] += [dict(stage="late_guard", **e) for e in check_facts(facts)]
        except Exception as error:
            report["secondary_errors"].append(dict(stage="late_guard", error=str(error)))
        report["guards"] = facts
        report["all_guards_unchanged"] = not any(e["stage"] == "late_guard" for e in report["secondary_errors"])
        report["passed"] = (report["first_error"] is None and not report["secondary_errors"] and
                            report["actual_C_cases"] == 226 and report["actual_services"] == 1556)
        if output and output.exists():
            late = write_report(output / "report.json", report)
            if late:
                report["secondary_errors"].append(late); report["passed"] = False
        print(json.dumps({k: report.get(k) for k in ("passed", "actual_C_cases", "actual_services", "actual_packets", "first_error", "secondary_errors")}, ensure_ascii=False))
    return 0 if report["passed"] else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("resource", "catalog", "catalog-sha256", "exe", "build-manifest", "output", "original-directory"):
        parser.add_argument("--" + name, required=True)
    parser.add_argument("--source", action="append", default=[], help="ROLE=PATH, exact catalog roles")
    parser.add_argument("--source-mode", choices=("legacy-local-bundle", "portable-bytes"), default="legacy-local-bundle")
    parser.add_argument("--project-root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--original-exe", type=Path)
    parser.add_argument("--source-profile", type=Path, default=Path(__file__).with_name("pcm_asset_source_profile.json"))
    parser.add_argument("--source-profile-sha256")
    parser.add_argument("--timeout", type=float, default=60)
    args = parser.parse_args()
    if not 0 < args.timeout <= 120:
        parser.error("timeout must be in (0,120]")
    raise SystemExit(run(args))


if __name__ == "__main__":
    main()
