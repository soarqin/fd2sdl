#!/usr/bin/env python3
"""G2：原 FULL9 跨目标 HP／贡献／RNG 写序与真实 Game 提交差分。

执行严格 relocated 镜像中的 2ff01 目标循环、完整 31266、2ce1a、
1c75e/1c81f 和真实 4ebe3。只替换显存／资源绘制、声音、等待和栈探测；
不替换过渡、effect 或 RNG。不对原资源目录外读取宣称像素等价。
C JSONL 只提供输入与待比较结果，原码不导入 SDL 冷表或 Python 结算公式。
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
IMAGE_SHA = "8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1"
FIGANI_SHA = "c0cef326c5f8bc61252f03394140dbeed2280ddba0da99bfacdee1bf568846f8"
OTHER_SHA = "882580dfa31bd22e069eb151b7a86a2b8dbcd8194ec755b00036c96a4a0e0667"
READSET_SHA = "a3845664b68aa28bd0e7133242c2921bcfd431d994d04815c7fb0a835f0fdc86"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def cast_cost(original, image, raw):
    sys.path.insert(0, str(ROOT / "tools/static_data"))
    from audit_stage_data import archive_entries
    figani = (original / "FIGANI.DAT").read_bytes()
    if digest(figani) != FIGANI_SHA:
        raise ValueError("FIGANI identity mismatch")
    entries = [payload for _, payload in archive_entries(figani)]
    selected = raw[7] * 3 + 2
    if struct.unpack_from("<H", entries[selected])[0] == 0:
        selected -= 1
    clip = entries[selected]
    # +0 part_count 与 +1 split_mode 是两个 BYTE；原普通回退的首WORD零
    # 检查只识别原 empty 哨兵，不能把非空目录的这两字段合成数量。
    count, split, intro = clip[0], clip[1], clip[2]
    if not 0 < count or split > 1 or intro > count or 8 + count * 4 > len(clip):
        raise ValueError("caster intro is outside this bounded comparison")
    markers = []
    for i in range(intro):
        start = struct.unpack_from("<I", clip, 8 + i * 4)[0]
        end = struct.unpack_from("<I", clip, 12 + i * 4)[0] if i + 1 < count else len(clip)
        if not 8 + count * 4 <= start < end <= len(clip) or end - start < 9:
            raise ValueError("caster part directory extent differs")
        if clip[start + 4] == 1:
            markers.append(i)
    record = image[0x619FD + 9 * 7:0x619FD + 10 * 7]
    if record.hex() != "e7033203001e00":
        raise ValueError("original Magic9 record differs")
    return record[5] * len(markers), {"entry": selected, "entry_sha256": digest(clip),
                                    "part_count": count, "split_mode": split,
                                    "intro_end": intro, "cost_marker_parts": markers,
                                    "record_hex": record.hex()}


def beam_directory(original):
    sys.path.insert(0, str(ROOT / "tools/static_data"))
    from audit_stage_data import archive_entries
    data = (original / "FDOTHER.DAT").read_bytes()
    if digest(data) != OTHER_SHA:
        raise ValueError("FDOTHER identity mismatch")
    clip = [payload for _, payload in archive_entries(data)][44]
    count = clip[0]
    if count != 31 or digest(clip) != "455338136fa971c2b61ebc5548a03ecadb77847811db467654682daba14e712e":
        raise ValueError("actual hostile9 part directory differs")
    previous = 8 + count * 4
    for i in range(count):
        at = struct.unpack_from("<I", clip, 8 + i * 4)[0]
        if not previous <= at < len(clip):
            raise ValueError("actual hostile9 part offset differs")
        previous = at
    return {"file_sha256": OTHER_SHA, "entry": 44, "entry_sha256": digest(clip),
            "part_count": count, "part_directory_hex": clip[8:8 + count * 4].hex()}


def run_original(image, case, original):
    import unicorn
    from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBP,
                                  UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP)
    cpu = unicorn.Uc(unicorn.UC_ARCH_X86, unicorn.UC_MODE_32)
    cpu.mem_map(0x10000, 0x54000)
    cpu.mem_write(0x10000, image[0x10000:0x64000])
    cpu.mem_map(0x100000, 0x10000)
    cpu.mem_map(0x200000, 0x10000)
    cpu.mem_map(0x300000, 0x40000)
    units, targets, base = 0x100000, 0x108000, 0x208000
    records = [bytearray.fromhex(r) for r in case["input_units_hex"]]
    indices, actor = case["target_indices"], case["caster_index"]
    if not (1 <= case["count"] <= 8 and len(indices) == case["count"] and
            all(len(r) == 80 for r in records) and len(records) == len(indices) + 1 and
            sorted(indices + [actor]) == list(range(len(records)))):
        raise ValueError("fixture is outside declared original local-array domain")
    fee, cast = cast_cost(original, image, records[actor])
    struct.pack_into("<H", records[actor], 68,
                     (struct.unpack_from("<H", records[actor], 68)[0] - fee) & 65535)
    cpu.mem_write(units, b"".join(bytes(r) for r in records))
    cpu.mem_write(targets, bytes(indices))
    cpu.mem_write(0x53A45, struct.pack("<I", units))
    cpu.mem_write(0x53EC8, struct.pack("<I", 0))
    cpu.mem_write(0x627B8, struct.pack("<H", case["seed"]))
    cpu.mem_write(0x5412E, b"\x01\x00")  # original phase0 postcondition, no RNG
    # 2ff01 fixed ESP view: 140h locals/saved-reg area, then return and four args.
    stack_values = {0x0: 0x310000, 0x4: 0x310100, 0x98: 0x06060807,
                    0x9C: 0x05050606, 0xA0: 0x1410, 0xF8: 0,
                    0x100: 0x310200, 0x104: 0, 0x108: 0, 0x10C: 0,
                    0x114: 0x310300, 0x118: 0x320000, 0x11C: 0,
                    0x120: 8, 0x124: 3, 0x128: 19360,
                    0x144: actor, 0x148: 9, 0x14C: len(indices), 0x150: targets}
    for offset, value in stack_values.items():
        cpu.mem_write(base + offset, struct.pack("<I", value))
    # Original denominator bytes, copied directly by the actual wrapper prologue.
    cpu.mem_write(base + 0x98, image[0x526BC:0x526C6])
    cpu.reg_write(UC_X86_REG_ESP, base)
    cpu.reg_write(UC_X86_REG_EBP, 9)
    cpu.reg_write(UC_X86_REG_ESI, 0x300000)
    cpu.reg_write(UC_X86_REG_EAX, 36)

    def u32(at):
        return struct.unpack("<I", cpu.mem_read(at, 4))[0]

    def u16(at):
        return struct.unpack("<H", cpu.mem_read(at, 2))[0]

    def target_state():
        return [u16(units + i * 80 + 64) for i in indices]

    def call_target(at):
        if image[at] != 0xE8:
            raise ValueError(f"declared call is not rel32: {at:#x}")
        return at + 5 + struct.unpack_from("<i", image, at + 1)[0]

    stubs = {0x3702F: "Watcom stack probe RET4", 0x2EB9F: "resource draw boundary",
             0x311E5: "actor draw", 0x2FACD: "HP panel draw",
             call_target(0x305DD): "VGA copy", call_target(0x30808): "BIOS wait",
             call_target(0x2CEF2): "primary audio", call_target(0x2CF08): "secondary audio"}
    allowed = [(0x3059D, 0x30994), (0x31266, 0x314DE), (0x2FE0C, 0x2FE14),
               (0x2CE1A, 0x2CF30), (0x1C75E, 0x1C8ED), (0x14230, 0x14237),
               (0x10B46, 0x10B4E), (0x4E84F, 0x4E87D), (0x4EBE3, 0x4EBFF)]
    trace, events, writes, draws, phases = [], [], [], [], []
    pending_rng = {}
    current = None

    def hook(machine, address, size, data):
        nonlocal current
        if address in pending_rng:
            snapshot = pending_rng.pop(address)
            snapshot["value"] = machine.reg_read(UC_X86_REG_EAX)
            trace.append(snapshot)
        if address in stubs:
            sp = machine.reg_read(UC_X86_REG_ESP)
            if address == 0x2EB9F:
                # Only draws whose return PC is the actual FX9 handler address its parts.
                ret = u32(sp)
                if 0x2CE1A <= ret < 0x2CF30:
                    part = u32(sp + 8)
                    draws.append({"part": part, "defined": part < 31,
                                  "frame": cpu.mem_read(0x5412E, 1)[0],
                                  "target_ordinal": u32(base + 0x10C)})
            machine.reg_write(UC_X86_REG_EIP, u32(sp))
            machine.reg_write(UC_X86_REG_ESP, sp + (8 if address == 0x3702F else 4))
            return
        if not any(start <= address < end for start, end in allowed):
            raise RuntimeError(f"original execution escaped at {address:#x}")
        if address == 0x4EBE3:
            ret = u32(machine.reg_read(UC_X86_REG_ESP))
            if ret in pending_rng:
                raise RuntimeError("overlapping RNG returns")
            pending_rng[ret] = {"mp_before": u16(units + actor * 80 + 68),
                                "contribution_before": u32(0x53EC8), "hp": target_state()}
        elif address == 0x30956:
            ordinal = u32(base + 0x10C)
            current = {"target_index": indices[ordinal], "hp_before": target_state()[ordinal],
                       "contribution_before": u32(0x53EC8), "impact_jitter": [],
                       "start_frame": cpu.mem_read(0x5412E, 1)[0]}
        elif address == 0x3095D:
            value = machine.reg_read(UC_X86_REG_EAX)
            current.update(hit=int(value != 0), raw_damage=value,
                           core_after=u16(units + current["target_index"] * 80 + 64),
                           contribution=(u32(0x53EC8) - current["contribution_before"]) & 0xFFFFFFFF)
        elif address == 0x307A3:
            value = u32(base + 0x104)
            current["impact_jitter"].append(value if value < 0x80000000 else value - 0x100000000)
        elif address == 0x3085F:
            current["hp_after"] = u16(units + current["target_index"] * 80 + 64)
            current["end_frame"] = cpu.mem_read(0x5412E, 1)[0]
            events.append(current)
            current = None
        elif address == 0x2CE24:
            sp = machine.reg_read(UC_X86_REG_ESP)
            phases.append({"phase": u32(sp + 20), "frame": cpu.mem_read(0x5412E, 1)[0],
                           "target_ordinal": u32(base + 0x10C)})

    def write_hook(machine, access, address, size, value, data):
        if 0x200000 <= address < 0x210000:
            return
        if address in (0x5412E, 0x5412F) and size == 1:
            return
        if address == 0x627B8 and size == 2:
            return
        if address == 0x53EC8 and size == 4:
            writes.append({"kind": "contribution", "value": value})
            return
        for i in indices:
            if address == units + i * 80 + 64 and size == 2:
                writes.append({"kind": "HP", "target_index": i, "value": value,
                               "pc": machine.reg_read(UC_X86_REG_EIP)})
                return
        raise RuntimeError(f"unexpected original write {address:#x}/{size}")

    cpu.hook_add(unicorn.UC_HOOK_CODE, hook)
    cpu.hook_add(unicorn.UC_HOOK_MEM_WRITE, write_hook)
    cpu.emu_start(0x308CE, 0x30994, count=500000)
    if cpu.reg_read(UC_X86_REG_EIP) != 0x30994 or cpu.reg_read(UC_X86_REG_ESP) != base or pending_rng:
        raise RuntimeError("original wrapper did not reach the declared post-target boundary")
    actual = {"finish_units_hex": [bytes(cpu.mem_read(units + i * 80, 80)).hex() for i in range(len(records))],
              "mp": u16(units + actor * 80 + 68), "contribution": u32(0x53EC8),
              "rng_final": u16(0x627B8), "rng_trace": trace,
              "target_events": [{k: v for k, v in e.items() if k in
                                 ("target_index", "hit", "hp_before", "hp_after", "core_after",
                                  "raw_damage", "contribution", "impact_jitter")} for e in events]}
    for key, value in actual.items():
        if case[key] != value:
            mismatch = {"case_id": case["case_id"], "field": key,
                        "original": value, "C": case[key]}
            raise ValueError(json.dumps(mismatch, ensure_ascii=False))
    return {"case_id": case["case_id"], "matched": list(actual), "cast": cast,
            "original": actual, "original_writes": writes, "original_fx_phase_trace": phases,
            "original_draw_boundary": draws, "target_frame_boundaries": events,
            "original_undefined_draw_requests": sum(not d["defined"] for d in draws),
            "explicit_stubs": {hex(k): v for k, v in stubs.items()}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, default=ROOT / "tools/fd2_le_relocated_relbase.bin")
    parser.add_argument("--original", type=Path, default=ROOT / "original_game")
    parser.add_argument("--c-results", type=Path, required=True)
    parser.add_argument("--readset", type=Path, default=ROOT / "build/g2/runtime-continuation-domain.json")
    parser.add_argument("--json", type=Path, required=True)
    args = parser.parse_args()
    image, readset_bytes = args.image.read_bytes(), args.readset.read_bytes()
    C_bytes = args.c_results.read_bytes()
    if digest(image) != IMAGE_SHA or digest(readset_bytes) != READSET_SHA:
        raise ValueError("strict image or complete Domain readset identity mismatch")
    readset = json.loads(readset_bytes.decode("utf-8"))
    functions = {}
    for f in readset["functions"]:
        if f["va"] not in (0x2FF01, 0x31266, 0x2CE1A, 0x1C75E, 0x1C81F):
            continue
        for chunk in f["chunks"]:
            if image[chunk["start"]:chunk["end"]] != bytes.fromhex(chunk["raw_hex"]):
                raise ValueError("strict image and Domain complete function bytes differ")
        functions[hex(f["va"])] = [c["sha256"] for c in f["chunks"]]
    cases = [json.loads(line) for line in C_bytes.decode("utf-8-sig").splitlines() if line]
    results = [run_original(image, c, args.original) for c in cases if c["count"] <= 8]
    if len(results) != 17 or {(c["count"], c["seed"]) for c in cases if c["count"] <= 8} != {
            (n, s) for n in (1, 2, 5, 8) for s in (0, 1, 0x7A18, 0xFFFF)} | {(2, 9)}:
        raise ValueError("declared original comparison coverage differs")
    report = {"ok": True, "schema": "fd2-g2-full9-wrapper-differential-v1",
              "image_sha256": IMAGE_SHA, "readset_sha256": READSET_SHA,
              "actual_beam_directory": beam_directory(args.original),
              "C_results_sha256": digest(C_bytes), "function_chunks": functions,
              "original_machine_code_cases": len(results),
              "original_RNG_instructions_executed": True, "original_transition_31266_executed": True,
              "original_individual_HP_contribution_writes_observed": True,
              "C_individual_memory_writes_observed": False,
              "C_observation_boundary": "every RNG pre-state, TARGET core/result, FINISH all 80B; renderer is not invoked by this probe",
              "C_business_entry": "fd2_field_game_commit_ai_magic -> execute_full_magic -> FINISH",
              "C_capacity_only_cases": [c["case_id"] for c in cases if c["count"] > 8],
              "native_DOSBox_acceptance": False, "pixel_or_audio_equivalence": False,
              "scope": "post-intro target loop through original transition, source-derived caster fee; all 80B, every RNG pre-state, HP/contribution/core/impact order",
              "safe_render_extension": "missing-beam draw omission is verified by separate FX/renderer tests; this probe records original undefined draw requests, not C draw emissions",
              "cases": results}
    if (args.c_results.read_bytes() != C_bytes or args.readset.read_bytes() != readset_bytes or
            args.image.read_bytes() != image):
        raise ValueError("original/C input files changed after the captured comparison snapshot")
    report["input_files_unchanged"] = True
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({k: v for k, v in report.items() if k != "cases"}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
