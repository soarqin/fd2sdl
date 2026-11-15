"""F1：完整分派目标、单位 owner 与拒绝边界的只读 Domain 来源。

依据 canonical 1a813、10c50/112a5、1366a，以及五张 relocation-backed
分派表。仅导出原证，不保存数据库、不推导未知 ABI、不宣称语义闭合。
通过 ida-domain-scripting/run.py 的隔离 driver 调用 collect(db, ...)。
含原字节的输出只留 ignored 证据目录，不得提交或分发。
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import struct
import sys

EXE_SHA = "bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd"
IMAGE_SHA = "8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1"
DISPATCH = (("result", 0x51b19, 30), ("callback", 0x51b91, 90),
            ("magic", 0x51d01, 28), ("entry", 0x51d71, 30),
            ("victory", 0x51de9, 30))
OWNERS = (0x53a45, 0x53bf7, 0x53beb, 0x53bfb, 0x53a51, 0x53a55, 0x53c03)
REQUIRED = (0x10010, 0x1088d, 0x10b4e, 0x10c50, 0x112a5, 0x11506,
            0x117e7, 0x11cac, 0x13185, 0x134e4, 0x135dd, 0x1366a,
            0x145cd, 0x15f84, 0x16c57, 0x190ac, 0x1a813, 0x1acf3,
            0x1b653, 0x1b6b7, 0x1b750, 0x3231b, 0x32999,
            0x4df4c, 0x4e390, 0x4e42c, 0x4e4be, 0x4e4f6, 0x4e5cc,
            0x4e680, 0x4e703, 0x4e71f, 0x4e751, 0x4e821, 0x4e838, 0x4e84f)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def collect(db, executable, image_path, output):
    executable, image_path, output = map(Path, (executable, image_path, output))
    output = output.resolve()
    if output.exists() or output.is_relative_to(executable.resolve().parent):
        raise ValueError("refusing to replace F1 source evidence")
    original, image = executable.read_bytes(), image_path.read_bytes()
    if sha(original) != EXE_SHA or sha(image) != IMAGE_SHA:
        raise ValueError("F1 source identity mismatch")
    project = Path(__file__).resolve().parents[2]
    sys.path.insert(0, str(project / "tools"))
    from dump_fd2_fixup_table import collect_internal_offset_fixups
    fixups = collect_internal_offset_fixups(executable)
    targets, tables = set(REQUIRED), []
    for name, va, count in DISPATCH:
        raw = bytes(db.bytes.get_bytes_at(va, size=count * 4))
        if image[va:va + len(raw)] != raw:
            raise ValueError("F1 dispatch source bytes mismatch")
        addresses = struct.unpack(f"<{count}I", raw)
        if any(va + i * 4 not in fixups for i in range(count)):
            raise ValueError("F1 dispatch entry lacks independent fixup")
        targets.update(db.functions.get_at(ea).start_ea for ea in addresses)
        tables.append({"name": name, "va": va, "count": count,
                       "targets": list(addresses), "sha256": sha(raw)})
    refs = []
    for owner in OWNERS:
        rows = []
        for ref in db.xrefs.to_ea(owner):
            function = db.functions.get_at(ref.from_ea)
            rows.append({"from": ref.from_ea, "type": ref.type.name,
                         "function": function.start_ea if function else None,
                         "read": ref.is_read, "write": ref.is_write})
            if function:
                targets.add(function.start_ea)
        refs.append({"owner": owner, "xrefs": sorted(rows, key=lambda row: row["from"])})
    # 全部过场可达静态调用另作闭包；不以顶层“没 call controller”代替
    # text/wait/renderer/音频的间接依赖。未知间接 call 明确保留。
    opening_graph, pending, seen = [], [0x3231b], set()
    while pending:
        ea = pending.pop()
        if ea in seen:
            continue
        seen.add(ea)
        function = db.functions.get_at(ea)
        if function is None:
            raise ValueError(f"F1 opening closure has unknown entry {ea:#x}")
        if ea >= 0x37000:
            opening_graph.append({"va": ea, "targets": [], "unresolved_calls": [],
                                  "system_boundary": True,
                                  "scope": "Watcom/AIL/hardware or shared low-level helper; not expanded as gameplay"})
            continue
        targets.add(function.start_ea)
        destinations, unknown = set(), []
        # 必须从真实 entry 遍历 PC，跨 owner 的 shared return tail 不能把
        # 另一个 owner 的不可达前缀并入，否则会伪造到 defeat collector 的边。
        pc_pending, pc_seen = [ea], set()
        while pc_pending:
            pc = pc_pending.pop()
            if pc in pc_seen:
                continue
            pc_seen.add(pc)
            if len(pc_seen) > 65536:
                raise ValueError("F1 opening PC walk exceeded finite source scope")
            insn = db.instructions.get_at(pc)
            if insn is None:
                unknown.append({"va": pc, "reason": "undecoded successor"})
                continue
            owner = db.functions.get_at(pc)
            if owner:
                targets.add(owner.start_ea)
            text = db.instructions.get_disassembly(insn)
            if text.startswith("call"):
                references = list(db.xrefs.code_refs_from_ea(pc, flow=False))
                if not references:
                    unknown.append({"va": pc, "asm": text})
                destinations.update(references)
                pc_pending.append(pc + insn.size)
            elif text.startswith(("ret", "iret")):
                continue
            elif text.startswith(("j", "loop")):
                references = list(db.xrefs.code_refs_from_ea(pc, flow=False))
                if not references:
                    unknown.append({"va": pc, "asm": text})
                for target in references:
                    if target >= 0x37000:
                        destinations.add(target)
                    else:
                        pc_pending.append(target)
                if not text.startswith("jmp"):
                    pc_pending.append(pc + insn.size)
            else:
                pc_pending.append(pc + insn.size)
        opening_graph.append({"va": ea, "owner": function.start_ea,
                              "walked_instruction_vas": sorted(pc_seen),
                              "targets": sorted(destinations), "unresolved_calls": unknown})
        pending.extend(destinations - seen)
    functions = []
    for ea in sorted(targets):
        function = db.functions.get_at(ea)
        if not function or function.start_ea != ea:
            raise ValueError(f"F1 non-entry target {ea:#x}")
        chunks = []
        for chunk in db.functions.get_chunks(function):
            raw = bytes(db.bytes.get_bytes_at(chunk.start_ea, size=chunk.end_ea-chunk.start_ea))
            if raw != image[chunk.start_ea:chunk.end_ea]:
                raise ValueError(f"F1 chunk source mismatch {chunk.start_ea:#x}")
            chunks.append({"start": chunk.start_ea, "end": chunk.end_ea,
                           "sha256": sha(raw), "bytes": raw.hex()})
        # Domain get_instructions(function) 在本机只迭代主跨度；显式逐 chunk
        # 读取，不能漏掉共享返回尾后仍称“完整 CFG/指令”。
        instructions = []
        for chunk in chunks:
            covered = 0
            for insn in db.instructions.get_between(chunk["start"], chunk["end"]):
                if insn.ea != chunk["start"] + covered:
                    raise ValueError(f"F1 undecoded chunk byte {insn.ea:#x}")
                instructions.append({"va": insn.ea, "size": insn.size,
                                     "asm": db.instructions.get_disassembly(insn),
                                     "code_refs": sorted(db.xrefs.code_refs_from_ea(insn.ea, flow=False))})
                covered += insn.size
            if covered != chunk["end"] - chunk["start"]:
                raise ValueError(f"F1 incomplete instruction chunk {chunk['start']:#x}")
        functions.append({"va": ea, "name": db.functions.get_name(function),
                          "chunks": chunks,
                          "instructions": instructions,
                          "blocks": [{"start": block.start_ea, "end": block.end_ea,
                                      "successors": [b.start_ea for b in block.get_successors()]}
                                     for block in db.functions.get_flowchart(function)],
                          "callers": sorted(f.start_ea for f in db.functions.get_callers(function)),
                          "callees": sorted(f.start_ea for f in db.functions.get_callees(function))})
    va = 0x51b91 + 255 * 4
    value = bytes(db.bytes.get_bytes_at(va, size=4))
    if value != image[va:va + 4]:
        raise ValueError("F1 callback255 bytes mismatch")
    report = {"schema": "fd2-f1-domain-source-v1", "exe_sha256": EXE_SHA,
              "image_sha256": IMAGE_SHA, "tables": tables, "owner_refs": refs,
              "opening_callgraph": sorted(opening_graph, key=lambda row: row["va"]),
              "functions": functions, "callback255": {"va": va, "bytes": value.hex(),
              "value": int.from_bytes(value, "little"),
              "fixup_sources_touching": [s for s in fixups if va - 3 <= s < va + 4]},
              "strict_byte_match": True, "semantic_closure_proven": False,
              "runtime_executed": False}
    if executable.read_bytes() != original or image_path.read_bytes() != image:
        raise ValueError("F1 source changed during collection")
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x", encoding="utf8") as stream:
        stream.write(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    return {"output": str(output), "sha256": sha(output.read_bytes()),
            "functions": len(functions), "dispatch_slots": sum(t["count"] for t in tables),
            "targets": len({v for t in tables for v in t["targets"]}),
            "callback255": report["callback255"]}
