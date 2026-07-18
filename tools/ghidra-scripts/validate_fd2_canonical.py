# -*- coding: utf-8 -*-
"""Ghidra post-script：验证并导出 FD2 canonical Program 的结构 inventory。"""
from __future__ import print_function

import hashlib
import json
import os
import struct

EXPECTED_EXE_SHA256 = "bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd"
EXPECTED_RAW_SHA256 = "37bceab29c9ee9089b5c5f9c4abefdee26a0bb34ae47bb917783dcc23ee30fc2"
EXPECTED_RELOCATED_SHA256 = "8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1"
EXPECTED_BLOCKS = (
    ("OBJ1.init", 0x10000, 0x3ef29, "r-x", True),
    ("OBJ2.init", 0x50000, 0x4000, "rw-", True),
    ("OBJ2.bss", 0x54000, 0x16b0, "rw-", False),
    ("OBJ3.init", 0x60000, 0x34d2, "rw-", True),
)


def fail(message):
    raise RuntimeError("FD2 canonical validation: " + message)


def addr(value):
    return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(long(value))


def read_memory(start, length):
    # Jython ``bytearray`` 不会由 Java 的 byte[] overload 回写；显式读 byte
    # 保持脚本在 Ghidra 11.3.2/Jython 2.7.4 上确定。
    memory = currentProgram.getMemory()
    return bytes(bytearray((memory.getByte(addr(start + i)) & 0xff)
                           for i in range(length)))


def canonical_json(data):
    return (json.dumps(data, ensure_ascii=False, sort_keys=True,
                       separators=(",", ":")) + "\n").encode("utf-8")


def sha256(data):
    return hashlib.sha256(data).hexdigest()


args = getScriptArgs()
if len(args) != 2:
    fail("expected arguments: bundle-dir output-dir")
bundle_dir = os.path.abspath(args[0])
out_dir = os.path.abspath(args[1])
if not os.path.isdir(out_dir):
    os.makedirs(out_dir)
manifest = json.load(open(os.path.join(bundle_dir, "manifest.json"), "r"))
if currentProgram.getExecutableSHA256() != EXPECTED_EXE_SHA256:
    fail("program executable SHA-256 mismatch")

memory = currentProgram.getMemory()
blocks = []
for block in memory.getBlocks():
    perms = ("r" if block.isRead() else "-") + \
            ("w" if block.isWrite() else "-") + \
            ("x" if block.isExecute() else "-")
    blocks.append((block.getName(), block.getStart().getOffset(),
                   block.getSize(), perms, block.isInitialized()))
if tuple(blocks) != EXPECTED_BLOCKS:
    fail("block inventory mismatch: %r" % (blocks,))
if memory.contains(addr(0)):
    fail("address zero is mapped; low mirror/import block leaked")

raw = bytearray(0x634d2)
relocated = bytearray(0x634d2)
initialized_ranges = []
for row in manifest["objects"]:
    start = row["relbase"]
    size = row["initialized_size"]
    payload = open(os.path.join(bundle_dir, row["init_file"]), "rb").read()
    if len(payload) != size or sha256(payload) != row["init_sha256"]:
        fail("bundle payload mismatch for object%d" % row["object"])
    raw[start:start + size] = payload
    relocated[start:start + size] = payload
    initialized_ranges.append((start, start + size))
unique_writes = {}
for row in manifest["relocations"]:
    source = row["source_va"]
    target = row["target_va"]
    previous = unique_writes.get(source)
    if previous is not None and previous != target:
        fail("manifest conflicting duplicate")
    unique_writes[source] = target
for source, target in unique_writes.items():
    relocated[source:source + 4] = bytearray((
        target & 0xff, (target >> 8) & 0xff,
        (target >> 16) & 0xff, (target >> 24) & 0xff))

for start, end in initialized_ranges:
    actual = read_memory(start, end - start)
    expected = bytes(relocated[start:end])
    if actual != expected:
        for i, pair in enumerate(zip(actual, expected)):
            if pair[0] != pair[1]:
                fail("program byte mismatch at 0x%x" % (start + i))
        fail("program byte mismatch at initialized range")
if sha256(bytes(raw)) != EXPECTED_RAW_SHA256:
    fail("reconstructed raw relbase hash mismatch")
if sha256(bytes(relocated)) != EXPECTED_RELOCATED_SHA256:
    fail("reconstructed relocated relbase hash mismatch")

relocation_table = currentProgram.getRelocationTable()
if relocation_table.getSize() != 7959:
    fail("Ghidra relocation table count mismatch")
seen_records = []
it = relocation_table.getRelocations()
while it.hasNext():
    relocation = it.next()
    if relocation.getStatus().toString() != "APPLIED":
        fail("non-applied relocation at %s" % relocation.getAddress())
    values = list(relocation.getValues())
    seen_records.append({
        "source_va": relocation.getAddress().getOffset(),
        "status": relocation.getStatus().toString(),
        "type": relocation.getType(),
        "target_va": values[0],
        "target_object": values[1],
        "target_offset": values[2],
        "raw_bytes": "".join("%02x" % (b & 0xff) for b in relocation.getBytes()),
        "symbol": relocation.getSymbolName(),
    })
seen_records.sort(key=lambda r: (r["source_va"], r["symbol"]))

relation_rows = []
materialized_relation_count = 0
reference_manager = currentProgram.getReferenceManager()
for row in manifest["relocations"]:
    source_va = row["source_va"]
    target_va = row["target_va"]
    reference = reference_manager.getReference(addr(source_va), addr(target_va), 0)
    materialized = reference is not None
    if materialized:
        materialized_relation_count += 1
    relation_rows.append({
        "record_index": row["record_index"],
        "source_va": source_va,
        "target_va": target_va,
        "target_object": row["target_object"],
        "target_offset": row["target_offset"],
        "materialized": materialized,
        "reference_type": reference.getReferenceType().toString() if materialized else None,
        "precision": "source-address-data-relation",
    })
if materialized_relation_count != 7959:
    fail("materialized relocation relation count mismatch: %d" % materialized_relation_count)

functions_by_object = {1: 0, 2: 0, 3: 0, "outside": 0}
function_rows = []
fm = currentProgram.getFunctionManager()
for fn in fm.getFunctions(True):
    va = fn.getEntryPoint().getOffset()
    if 0x10000 <= va < 0x4ef29:
        obj = 1
    elif 0x50000 <= va < 0x556b0:
        obj = 2
    elif 0x60000 <= va < 0x634d2:
        obj = 3
    else:
        obj = "outside"
    functions_by_object[obj] += 1
    function_rows.append({"va": va, "object": obj, "name": fn.getName()})
if functions_by_object[2] or functions_by_object[3] or functions_by_object["outside"]:
    fail("functions exist outside executable object: %r" % functions_by_object)

inventory = {
    "schema": "fd2-ghidra-structural-inventory-v1",
    "program": {
        "language": currentProgram.getLanguageID().toString(),
        "compiler_spec": currentProgram.getCompilerSpec().getCompilerSpecID().toString(),
        "compiler_note": currentProgram.getCompiler(),
        "executable_format": currentProgram.getExecutableFormat(),
        "executable_sha256": currentProgram.getExecutableSHA256(),
        "image_base": currentProgram.getImageBase().getOffset(),
    },
    "blocks": [
        {"name": row[0], "start_va": row[1], "size": row[2],
         "permissions": row[3], "initialized": row[4]}
        for row in blocks
    ],
    "relocations": {
        "record_count": len(seen_records),
        "unique_source_count": len(set(r["source_va"] for r in seen_records)),
        "records": seen_records,
    },
    "references": {
        "relation_count": len(relation_rows),
        "materialized_source_data_reference_count": materialized_relation_count,
        "materialized_precise_operand_reference_count": 0,
        "unresolved_precise_operand_relation_count": len(relation_rows),
        "records": relation_rows,
        "note": "Every relocation has a source-address DATA reference. Precise instruction operand/table READ/WRITE/CALL classification is a gated semantic phase.",
    },
    "functions": {
        "count_by_object": dict((str(k), v) for k, v in functions_by_object.items()),
        "records": sorted(function_rows, key=lambda row: row["va"]),
    },
    "byte_oracles": {
        "raw_relbase_sha256": sha256(bytes(raw)),
        "relocated_relbase_sha256": sha256(bytes(relocated)),
    },
}
inventory_bytes = canonical_json(inventory)
out_path = os.path.join(out_dir, "structural-inventory.json")
open(out_path, "wb").write(inventory_bytes)
open(os.path.join(out_dir, "structural-inventory.sha256"), "w").write(
    "%s  structural-inventory.json\n" % sha256(inventory_bytes))
open(os.path.join(out_dir, "canonical-validation.ok"), "w").write("PASS\n")
print("FD2 canonical validation: PASS blocks=4 relocations=7959 unique_sources=7948 functions=%d output=%s" %
      (len(function_rows), out_path))
