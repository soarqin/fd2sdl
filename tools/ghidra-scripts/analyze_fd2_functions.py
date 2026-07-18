# -*- coding: utf-8 -*-
"""Ghidra script：按 machine-readable evidence manifest 建立 FD2 函数。

不读取 function-names.md 或 r2_funcs.txt，不静默清除/拆分现有函数。每个 seed
都输出确定 disposition；Watcom __chkstk body 默认只建 label。
"""
from __future__ import print_function

import hashlib
import json
import os

from ghidra.app.cmd.disassemble import DisassembleCommand
from ghidra.app.cmd.function import CreateFunctionCmd
from ghidra.program.model.symbol import SourceType

SCHEMA = "fd2-function-seeds-v1"
OBJ1_START = 0x10000
OBJ1_END = 0x4ef29


def fail(message):
    raise RuntimeError("FD2 function analysis: " + message)


def addr(value):
    return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(long(value))


def prefix_matches_raw(va, expected, relocation_sources):
    memory = currentProgram.getMemory()
    actual = [memory.getByte(addr(va + i)) & 0xff for i in range(len(expected) // 2)]
    wanted = [int(expected[i:i + 2], 16) for i in range(0, len(expected), 2)]
    # Program 已应用 relocation；expected_prefix 来自 raw object1。忽略 prefix
    # 内合法 relocation source 覆盖的字节，其余字节必须完全一致。
    covered = set()
    for source in relocation_sources:
        if va <= source < va + len(wanted):
            covered.update(range(source - va, min(source - va + 4, len(wanted))))
    return all(index in covered or actual[index] == wanted[index]
               for index in range(len(wanted)))


def stable_label(symbols, va, name):
    existing = symbols.getGlobalSymbol(name, addr(va))
    if existing is None:
        symbols.createLabel(addr(va), name, SourceType.IMPORTED)


args = getScriptArgs()
if len(args) != 2:
    fail("expected arguments: seed-manifest output-dir")
seed_path = os.path.abspath(args[0])
out_dir = os.path.abspath(args[1])
if not os.path.isdir(out_dir):
    os.makedirs(out_dir)
manifest = json.load(open(seed_path, "r"))
if manifest.get("schema") != SCHEMA:
    fail("unsupported seed schema")
relocation_sources = set(r.getAddress().getOffset()
                         for r in currentProgram.getRelocationTable().getRelocations())

listing = currentProgram.getListing()
fm = currentProgram.getFunctionManager()
symbols = currentProgram.getSymbolTable()
seed_results = []

# 强证据入口先创建。按 VA 排序使 overlap disposition 可重复。
for seed in sorted(manifest["seeds"], key=lambda row: row["va"]):
    va = seed["va"]
    result = dict(seed)
    if not OBJ1_START <= va < OBJ1_END:
        result["result"] = "outside-executable-object"
        seed_results.append(result)
        continue
    expected = seed.get("expected_prefix", "")
    if expected and not prefix_matches_raw(va, expected, relocation_sources):
        result["result"] = "byte-prefix-mismatch"
        seed_results.append(result)
        continue
    exact = fm.getFunctionAt(addr(va))
    containing = fm.getFunctionContaining(addr(va))
    if exact is not None:
        fn = exact
        result["result"] = "already-exact"
    elif containing is not None:
        result["result"] = "inside-existing"
        result["containing_entry_va"] = containing.getEntryPoint().getOffset()
        result["containing_name"] = containing.getName()
        # __chkstk pattern scan覆盖全部541个 callsite，但若前一个已确认入口的
        # flow 合法到达该地址，它就是同一函数内的栈扩展片段，不强拆第二函数。
        result["conflict_class"] = (
            "resolved-internal-chkstk-site" if seed.get("entry_kind") == "chkstk-wrapper"
            else "unresolved-overlap")
        seed_results.append(result)
        continue
    else:
        disasm = DisassembleCommand(addr(va), None, True)
        disasm.enableCodeAnalysis(False)
        if not disasm.applyTo(currentProgram, monitor):
            result["result"] = "decode-failed"
            result["detail"] = disasm.getStatusMsg()
            seed_results.append(result)
            continue
        command = CreateFunctionCmd(addr(va))
        if not command.applyTo(currentProgram, monitor):
            result["result"] = "create-failed"
            result["detail"] = command.getStatusMsg()
            seed_results.append(result)
            continue
        fn = fm.getFunctionAt(addr(va))
        if fn is None:
            result["result"] = "create-returned-without-function"
            seed_results.append(result)
            continue
        result["result"] = "created"
    name = seed.get("name")
    if name:
        try:
            fn.setName(name, SourceType.IMPORTED)
        except Exception as exc:
            result["name_result"] = "conflict: " + str(exc)
    if va == manifest["chkstk"]["helper_va"]:
        fn.setNoReturn(False)
    if seed.get("entry_kind") == "chkstk-wrapper":
        body_va = seed["body_va"]
        stable_label(symbols, body_va, "body_%x" % body_va)
        result["body_label_result"] = "created-or-existing"
    seed_results.append(result)

# 从强证据函数体递归提升「可达 direct call target」。这里使用 Ghidra 已建立
# 的 function body/flow，而不是把整段 object1 线性解码的假指令当证据。
reachable_call_results = []
scanned_entries = set()
while True:
    pending = sorted(
        (fn for fn in fm.getFunctions(True)
         if OBJ1_START <= fn.getEntryPoint().getOffset() < OBJ1_END and
         fn.getEntryPoint().getOffset() not in scanned_entries),
        key=lambda fn: fn.getEntryPoint().getOffset())
    if not pending:
        break
    for caller in pending:
        caller_va = caller.getEntryPoint().getOffset()
        scanned_entries.add(caller_va)
        instructions = listing.getInstructions(caller.getBody(), True)
        while instructions.hasNext():
            instruction = instructions.next()
            flow_type = instruction.getFlowType()
            if not flow_type.isCall() or flow_type.isComputed():
                continue
            for target in instruction.getFlows():
                target_va = target.getOffset()
                row = {"caller_va": caller_va,
                       "callsite_va": instruction.getAddress().getOffset(),
                       "target_va": target_va,
                       "kind": "reachable-direct-call-target"}
                if not OBJ1_START <= target_va < OBJ1_END:
                    row["result"] = "outside-executable-object"
                    reachable_call_results.append(row)
                    continue
                exact = fm.getFunctionAt(target)
                containing = fm.getFunctionContaining(target)
                if exact is not None:
                    row["result"] = "already-exact"
                elif containing is not None:
                    row["result"] = "inside-existing"
                    row["containing_entry_va"] = containing.getEntryPoint().getOffset()
                else:
                    disasm = DisassembleCommand(target, None, True)
                    disasm.enableCodeAnalysis(False)
                    if not disasm.applyTo(currentProgram, monitor):
                        row["result"] = "decode-failed"
                        row["detail"] = disasm.getStatusMsg()
                    else:
                        command = CreateFunctionCmd(target)
                        if command.applyTo(currentProgram, monitor) and fm.getFunctionAt(target):
                            row["result"] = "created"
                        else:
                            row["result"] = "create-failed"
                            row["detail"] = command.getStatusMsg()
                reachable_call_results.append(row)

# 未能创建 function 的 __chkstk body 仍只作为结构 label 记录，不升级为函数。
for wrapper in manifest["chkstk"]["wrappers"]:
    body_va = wrapper["body_va"]
    stable_label(symbols, body_va, "body_%x" % body_va)

function_rows = []
counts = {"1": 0, "2": 0, "3": 0, "outside": 0}
for fn in fm.getFunctions(True):
    va = fn.getEntryPoint().getOffset()
    if OBJ1_START <= va < OBJ1_END:
        obj = "1"
        offset = va - OBJ1_START
    elif 0x50000 <= va < 0x556b0:
        obj = "2"
        offset = va - 0x50000
    elif 0x60000 <= va < 0x634d2:
        obj = "3"
        offset = va - 0x60000
    else:
        obj = "outside"
        offset = None
    counts[obj] += 1
    function_rows.append({
        "va": va, "object": obj, "offset": offset, "name": fn.getName(),
        "body_min_va": fn.getBody().getMinAddress().getOffset(),
        "body_max_va": fn.getBody().getMaxAddress().getOffset(),
        "calling_convention": fn.getCallingConventionName(),
        "no_return": fn.hasNoReturn(),
    })
if counts["2"] or counts["3"] or counts["outside"]:
    fail("functions created outside object1: %r" % counts)

result = {
    "schema": "fd2-function-analysis-v1",
    "seed_manifest_sha256": hashlib.sha256(open(seed_path, "rb").read()).hexdigest(),
    "seed_results": seed_results,
    "result_counts": dict((status, sum(1 for row in seed_results if row["result"] == status))
                          for status in sorted(set(row["result"] for row in seed_results))),
    "functions": sorted(function_rows, key=lambda row: row["va"]),
    "function_counts_by_object": counts,
    "reachable_direct_calls": reachable_call_results,
    "reachable_direct_call_result_counts": dict(
        (status, sum(1 for row in reachable_call_results if row["result"] == status))
        for status in sorted(set(row["result"] for row in reachable_call_results))),
    "boundary_conflicts": [row for row in seed_results
                           if row["result"] in ("inside-existing", "decode-failed",
                                                "create-failed", "byte-prefix-mismatch")],
    "unresolved_boundary_conflicts": (
        [row for row in seed_results
         if row.get("conflict_class") == "unresolved-overlap" or
         row["result"] in ("decode-failed", "create-failed", "byte-prefix-mismatch")] +
        [row for row in reachable_call_results
         if row["result"] in ("decode-failed", "create-failed")]),
    "deferred_direct_call_candidates": len(manifest["direct_call_candidates"]),
    "deferred_relocation_code_candidates": len(manifest["relocation_code_candidates"]),
    "abi": {
        "compiler_spec": currentProgram.getCompilerSpec().getCompilerSpecID().toString(),
        "status": "provisional-stock-gcc-no-watcom-cspec",
        "watcom_register_order": "EAX,EDX,EBX,ECX (documented; not yet modeled)",
    },
}
payload = (json.dumps(result, ensure_ascii=False, sort_keys=True,
                      separators=(",", ":")) + "\n").encode("utf-8")
out_path = os.path.join(out_dir, "function-analysis.json")
open(out_path, "wb").write(payload)
print("FD2 function analysis: functions=%d seeds=%d conflicts=%d output=%s" %
      (len(function_rows), len(seed_results), len(result["boundary_conflicts"]), out_path))
