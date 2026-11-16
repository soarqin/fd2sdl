"""F4 bounded App/base FX comparison; no IDA, CPU, GUI or C expected.

Actual is produced by fd2_field_defeat_test --f4-defeat-pages D SEED OUT.
Legacy F4DFX001 keeps D=1,30..35; --prefix-wide accepts distinct F4DFX002
D36/96 initialized-page parameters. Both use two explicit before seeds.
Neither mode qualifies natural deaths, initial heap, parent/control outcomes.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent / "static_data"))
from audit_stage_data import archive_entries

ARCHIVE_SOURCE = Path(archive_entries.__code__.co_filename).resolve()

PAGE, VGA, FRAMES = 456 * 336, 320 * 200, 12
EXE_SHA = "bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd"
OTHER_SHA = "882580dfa31bd22e069eb151b7a86a2b8dbcd8194ec755b00036c96a4a0e0667"
IMAGE_SHA = "8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1"
CONTROL_SHA = "18617904b37d73b066eea7a78889d6da64fc6dad895b9292ae279df4b8f47517"
PIXEL_SHA = "683ec0e987b4fff9d515677df1fc0d9d1d77d9e95ab11f816135fd2bc06cdbb2"
HEADER = struct.Struct("<8s4I")


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def frozen(path, expected=None):
    path = Path(path)
    before = path.stat()
    raw = path.read_bytes()
    fact = {"path": str(path.resolve()), "sha256": digest(raw),
            "size": len(raw), "mtime_ns": before.st_mtime_ns}
    if path.stat().st_mtime_ns != before.st_mtime_ns or (expected and fact["sha256"] != expected):
        raise ValueError(f"source identity changed: {path}")
    return raw, fact


def strict_function(report, va, image):
    functions = [f for f in report["functions"] if f["va"] == va]
    if len(functions) != 1 or not functions[0].get("instructions"):
        raise ValueError(f"missing complete source {va:#x}")
    for chunk in functions[0]["chunks"]:
        raw = bytes.fromhex(chunk.get("bytes", chunk.get("raw_hex", "")))
        if len(raw) != chunk["end"] - chunk["start"] or image[chunk["start"]:chunk["end"]] != raw:
            raise ValueError(f"strict source bytes mismatch {va:#x}")
        if digest(raw) != chunk["sha256"]:
            raise ValueError(f"strict source hash mismatch {va:#x}")


def decode_lmi(raw):
    # Original4ec66: c1..ff => repeat c-c0, otherwise literal, including0/c0.
    if len(raw) < 4:
        raise ValueError("truncated LMI")
    width, height = struct.unpack_from("<HH", raw)
    if not 0 < width <= 72 or not 0 < height <= 71:
        raise ValueError("outside the qualified twelve-FX geometry")
    pixels, at = bytearray(), 4
    while len(pixels) < width * height:
        if at >= len(raw):
            raise ValueError("truncated LMI control")
        control = raw[at]
        at += 1
        if control >= 0xc1:
            if at >= len(raw):
                raise ValueError("truncated LMI run")
            pixels.extend(bytes([raw[at]]) * (control - 0xc0))
            at += 1
        else:
            pixels.append(control)
    if len(pixels) != width * height or at != len(raw):
        raise ValueError("LMI exact extent mismatch")
    return width, height, pixels


def blit(page, image, x, y):
    # Original4ebab: zero leaves the initialized destination byte unchanged.
    width, height, pixels = image
    if x < 0 or y < 0 or x + width > 456 or y + height > 336:
        raise ValueError("FX outside qualified page")
    for row in range(height):
        for col in range(width):
            color = pixels[row * width + col]
            if color:
                page[(y + row) * 456 + x + col] = color


def expected_states(images, count, seed):
    if count not in (1, 30, 31, 32, 33, 34, 35) or seed not in (0, 1) or len(images) != FRAMES:
        raise ValueError("outside fixed probe parameters")
    app = bytearray((i * 13 + seed * 29 + 5) & 255 for i in range(PAGE))
    base = bytearray((i * 17 + seed * 31 + 91) & 255 for i in range(PAGE))
    for frame, image in enumerate(images):
        # Source1de94 restore is before all destinations of frame74..79.
        if frame >= 6:
            app[:] = base
        for raw in range(count):
            # Source1dd61 replaces only index30; 1ddd6/1de43 still read D.
            if raw == 30:
                blit(base, image, 0, 0)
            else:
                blit(app, image, 96 + 24 * (raw % 7), 90 + 24 * (raw // 7))
        projected = bytearray([109]) * VGA
        for row in range(192):
            source = (72 + row) * 456 + 72
            target = (4 + row) * 320 + 4
            projected[target:target + 312] = app[source:source + 312]
        yield bytes(app), bytes(base), bytes(projected)


def expected_prefix_states(images, count, seed):
    if count not in (36, 96) or seed not in (0, 1) or len(images) != FRAMES:
        raise ValueError("outside initialized highD prefix parameters")
    app = bytearray((i * 13 + seed * 29 + 5) & 255 for i in range(PAGE))
    base = bytearray((i * 17 + seed * 31 + 91) & 255 for i in range(PAGE))
    for frame, image in enumerate(images):
        if frame >= 6:
            app[:] = base
        for raw in range(count):
            if raw == 30:
                blit(base, image, 0, 0)
            else:
                blit(app, image, 24 + 24 * (raw % 15), 18 + 24 * (raw // 15))
        projected = bytearray([109]) * VGA
        for row in range(192):
            source = (72 + row) * 456 + 72
            target = (4 + row) * 320 + 4
            projected[target:target + 312] = app[source:source + 312]
        yield bytes(app), bytes(base), bytes(projected)


def parameters_prefix(actual):
    if len(actual) != HEADER.size + FRAMES * (PAGE * 2 + VGA):
        raise ValueError("actual highD prefix exact length mismatch")
    magic, count, seed, page, frames = HEADER.unpack_from(actual)
    if magic != b"F4DFX002" or page != PAGE or frames != FRAMES or count not in (36,96) or seed not in (0,1):
        raise ValueError("actual highD prefix header/domain mismatch")
    return count, seed


def parameters(actual):
    if len(actual) != HEADER.size + FRAMES * (PAGE * 2 + VGA):
        raise ValueError("actual exact length mismatch")
    magic, count, seed, page, frames = HEADER.unpack_from(actual)
    if magic != b"F4DFX001" or page != PAGE or frames != FRAMES:
        raise ValueError("actual header mismatch")
    if count not in (1, 30, 31, 32, 33, 34, 35) or seed not in (0, 1):
        raise ValueError("actual parameter outside qualified domain")
    return count, seed


def verify(args):
    # One snapshot list/one late guard for resource, actual, tool and producer.
    # A guard error is secondary when a comparison already failed.
    facts, after, before_errors, late_errors = [], [], [], []
    primary, result = None, None

    def remember(path, role, expected=None):
        try:
            raw, fact = frozen(path)
        except (OSError, ValueError) as exc:
            before_errors.append({"path": str(path), "role": role, "error": str(exc)})
            raise
        fact["role"] = role
        facts.append(fact)
        if expected and fact["sha256"] != expected:
            raise ValueError(f"source identity changed: {path}")
        return raw

    try:
        remember(Path(__file__), "verifier_script")
        remember(ARCHIVE_SOURCE, "archive_entries_dependency")
        remember(args.producer, "producer_binary")
        sources = args.producer_source or [
            ROOT / "src/tests/field_defeat_test.c", ROOT / "src/field_defeat.c",
            ROOT / "src/field_defeat.h", ROOT / "src/tests/audio_probe_output.cpp",
            ROOT / "src/tests/audio_probe_output.h"]
        for source in sources:
            remember(source, "producer_source")
        image = remember(args.image, "strict_image", IMAGE_SHA)
        control = remember(args.control_source, "control_source", CONTROL_SHA)
        pixel = remember(args.pixel_source, "pixel_source", PIXEL_SHA)
        strict_function(json.loads(control), 0x1db65, image)
        for va in (0x4ebab, 0x4ec66):
            strict_function(json.loads(pixel), va, image)
        remember(args.original / "FD2.EXE", "original_EXE", EXE_SHA)
        resource = remember(args.original / "FDOTHER.DAT", "original_resource", OTHER_SHA)
        entry = archive_entries(resource)[5][1]
        if entry[:4] != b"LMI1":
            raise ValueError("FDOTHER5 is not original LMI")
        entries = struct.unpack_from("<H", entry, 4)[0]
        if entries < 80 or 6 + 4 * entries > len(entry):
            raise ValueError("LMI directory outside owner")
        offsets = struct.unpack_from(f"<{entries}I", entry, 6)
        if offsets[0] < 6 + 4 * entries or any(a >= b for a, b in zip(offsets, offsets[1:])):
            raise ValueError("LMI directory order changed")
        images = []
        for index in range(68, 80):
            start = offsets[index]
            end = offsets[index + 1] if index + 1 < entries else len(entry)
            if not start < end <= len(entry):
                raise ValueError("FX payload outside source owner")
            images.append(decode_lmi(entry[start:end]))
        if args.actual.stat().st_size != HEADER.size + FRAMES * (PAGE * 2 + VGA):
            raise ValueError("actual exact length mismatch")
        actual = remember(args.actual, "actual_pages")
        prefix = getattr(args,"prefix_wide",False)
        count, seed = parameters_prefix(actual) if prefix else parameters(actual)
        at, rows = HEADER.size, []
        for frame, pages in enumerate((expected_prefix_states if prefix else expected_states)(images, count, seed)):
            for owner, expected in zip(("app", "base", "vga"), pages):
                measured = actual[at:at + len(expected)]
                if measured != expected:
                    first = next(i for i, (a, b) in enumerate(zip(measured, expected)) if a != b)
                    raise ValueError(f"frame{68 + frame} {owner} first mismatch {first}: {measured[first]} != {expected[first]}")
                rows.append({"frame": 68 + frame, "owner": owner, "bytes": len(expected), "sha256": digest(expected)})
                at += len(expected)
        result = {"schema": "fd2-f4-defeat-two-page-prefix-v3" if prefix else "fd2-f4-defeat-two-page-compare-v2", "count": count, "seed": seed,
                  "equal": True, "inputs": facts, "frames": rows,
                  "original_CPU_executed": False, "native_executed": False,
                  "claims": ("initialized highD callee FX prefix only; no original control-frame, natural count, parent outcome or fresh heap qualification" if prefix else
                    "fixed initialized two-page/real-FX compositor only; producer identity is guarded here but its actual generation/build binding requires the runner receipt; no heap/parent/natural/D36+ qualification")}
    except (OSError, ValueError, KeyError, IndexError, struct.error) as exc:
        primary = exc

    for before in facts:
        try:
            _, current = frozen(before["path"])
            current["role"] = before["role"]
            after.append(current)
            if current != before:
                late_errors.append({"path": before["path"], "role": before["role"],
                                    "error": "late identity changed"})
        except (OSError, ValueError) as exc:
            after.append({"path": before["path"], "role": before["role"], "available": False})
            late_errors.append({"path": before["path"], "role": before["role"], "error": str(exc)})
    identity = {"before": facts, "after": after, "before_read_errors": before_errors,
                "late_errors": late_errors, "all_captured_unchanged": not late_errors,
                "business_completed": result is not None,
                "scope": "only captured snapshots were checked; missing/unreached identities remain unavailable"}
    if primary is not None or late_errors:
        error = primary if primary is not None else ValueError(
            f"source/producer late guard changed: {late_errors[0]['path']}")
        # Keep the original mismatch/read error. Late guard failures remain
        # explicit secondaries; they neither replace it nor become a PASS.
        error.receipt = {"schema": "fd2-f4-defeat-two-page-prefix-v3" if getattr(args,"prefix_wide",False) else "fd2-f4-defeat-two-page-compare-v2", "equal": False,
                         "primary_error": str(error), "identity": identity,
                         "original_CPU_executed": False, "native_executed": False}
        raise error
    result["identity"] = identity
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--actual", type=Path, required=True)
    parser.add_argument("--original", type=Path, default=ROOT / "original_game")
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--control-source", type=Path, required=True)
    parser.add_argument("--pixel-source", type=Path, required=True)
    parser.add_argument("--producer", type=Path, required=True,
                        help="exact C probe binary used for this actual artifact; not executed here")
    parser.add_argument("--producer-source", type=Path, action="append",
                        help="explicit corresponding C/build source snapshot; repeat to replace the five direct defaults")
    parser.add_argument("--prefix-wide",action="store_true",help="F4DFX002 only: D36/96 explicit initialized 15-column callee prefix")
    args = parser.parse_args()
    try:
        print(json.dumps(verify(args), ensure_ascii=False, indent=2))
    except (OSError, ValueError, KeyError, IndexError, struct.error) as exc:
        print(json.dumps(getattr(exc, "receipt", {"equal": False, "primary_error": str(exc),
                         "identity": {"status": "unavailable"}}), ensure_ascii=False, indent=2))
        print(f"F4双页核验失败：{exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
