# -*- coding: utf-8 -*-
"""Ghidra post-script：以 canonical VA 和结构化 marker 导出已建函数。"""
from __future__ import print_function

import json
import os

import java.io
from ghidra.app.decompiler import DecompInterface

OBJ1_BASE = 0x10000
OBJ1_END = 0x4ef29


def fail(message):
    raise RuntimeError("FD2 decompile export: " + message)


args = getScriptArgs()
if len(args) != 1:
    fail("expected output directory")
out_dir = os.path.abspath(args[0])
if not os.path.isdir(out_dir):
    os.makedirs(out_dir)
out_path = os.path.join(out_dir, "ghidra-decomp-canonical.c")
manifest_path = os.path.join(out_dir, "function-analysis.json")
seed_metadata = {}
if os.path.isfile(manifest_path):
    analysis = json.load(open(manifest_path, "r"))
    for row in analysis["seed_results"]:
        if row.get("result") in ("created", "already-exact"):
            seed_metadata[row["va"]] = row

functions = [fn for fn in currentProgram.getFunctionManager().getFunctions(True)
             if OBJ1_BASE <= fn.getEntryPoint().getOffset() < OBJ1_END]
functions.sort(key=lambda fn: fn.getEntryPoint().getOffset())
decomp = DecompInterface()
if not decomp.openProgram(currentProgram):
    fail("could not open program in decompiler")
pw = java.io.PrintWriter(java.io.OutputStreamWriter(
    java.io.FileOutputStream(out_path), "UTF-8"))
pw.println("/* AUTO-GENERATED canonical FD2 Ghidra export.")
pw.println(" * Address key: LE relbase linear VA. OBJ1 OFF = VA - 0x10000.")
pw.println(" * Input: original FD2.EXE object bytes + verified LE relocation manifest.")
pw.println(" * ABI: stock gcc compiler spec is provisional; signatures are not authoritative.")
pw.println(" */")
rows = []
for fn in functions:
    va = fn.getEntryPoint().getOffset()
    off = va - OBJ1_BASE
    meta = seed_metadata.get(va, {})
    entry_kind = meta.get("entry_kind", meta.get("kind", "analyzed"))
    tier = meta.get("tier", "analyzer")
    marker = ("// FUNC VA=0x%x OBJ=1 OFF=0x%x NAME=%s ENTRY_KIND=%s TIER=%s" %
              (va, off, fn.getName(), entry_kind, tier))
    pw.println(marker)
    result = decomp.decompileFunction(fn, 120, monitor)
    status = "failed"
    if result is not None and result.decompileCompleted() and result.getDecompiledFunction():
        for line in result.getDecompiledFunction().getC().splitlines():
            pw.println(line.rstrip())
        status = "ok"
    else:
        detail = result.getErrorMessage() if result is not None else "no result"
        pw.println("/* DECOMPILE FAILED: %s */" % detail.replace("*/", "* /"))
    pw.println("")
    rows.append({"va": va, "object": 1, "offset": off, "name": fn.getName(),
                 "entry_kind": entry_kind, "tier": tier,
                 "decompile_status": status})
pw.flush()
pw.close()
# PrintWriter 的函数间空行会使最终函数后多出一个空白行；规范导出只
# 保留单个结尾换行，避免 git diff --check 报告 generated artifact。
exported = open(out_path, "rb").read().rstrip(b"\n") + b"\n"
open(out_path, "wb").write(exported)
decomp.dispose()
json_payload = (json.dumps({"schema": "fd2-decompile-export-v1", "functions": rows},
                           ensure_ascii=False, sort_keys=True,
                           separators=(",", ":")) + "\n").encode("utf-8")
open(os.path.join(out_dir, "decompile-manifest.json"), "wb").write(json_payload)
print("FD2 decompile export: functions=%d output=%s" % (len(rows), out_path))
