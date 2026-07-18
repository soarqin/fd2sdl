# -*- coding: utf-8 -*-
"""Ghidra pre-script：从已验证 bundle 构造 FD2 canonical relbase Program。

调用：-preScript import_fd2_canonical.py <bundle-dir>
必须在自动分析前运行。本脚本不解析 LE，不创建函数，也不导入语义名称。
"""
from __future__ import print_function

import hashlib
import java.io
import json
import os
import struct

from ghidra.program.model.reloc import Relocation
from ghidra.program.model.symbol import RefType, SourceType

SCHEMA = "fd2-ghidra-bundle-v1"
EXPECTED_EXE_SHA256 = "bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd"
EXPECTED_BLOCKS = (
    ("OBJ1.init", 0x10000, 0x3ef29, True, False, True, True),
    ("OBJ2.init", 0x50000, 0x4000, True, True, False, True),
    ("OBJ2.bss", 0x54000, 0x16b0, True, True, False, False),
    ("OBJ3.init", 0x60000, 0x34d2, True, True, False, True),
)
RELOCATION_TYPE_FD2_LE_INTERNAL_OFFSET32 = 0x46443207


def fail(message):
    raise RuntimeError("FD2 canonical import: " + message)


def read_bytes(path):
    stream = java.io.FileInputStream(path)
    try:
        out = java.io.ByteArrayOutputStream()
        buf = bytearray(65536)
        while True:
            count = stream.read(buf)
            if count < 0:
                break
            out.write(buf, 0, count)
        return bytes(out.toByteArray())
    finally:
        stream.close()


def sha256_file(path):
    digest = hashlib.sha256()
    f = open(path, "rb")
    try:
        while True:
            chunk = f.read(1024 * 1024)
            if not chunk:
                break
            digest.update(chunk)
    finally:
        f.close()
    return digest.hexdigest()


def addr(value):
    return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(long(value))


args = getScriptArgs()
if len(args) != 1:
    fail("expected exactly one argument: bundle directory")
bundle_dir = os.path.abspath(args[0])
manifest_path = os.path.join(bundle_dir, "manifest.json")
if not os.path.isfile(manifest_path):
    fail("missing manifest.json: " + manifest_path)
manifest = json.load(open(manifest_path, "r"))
if manifest.get("schema") != SCHEMA:
    fail("unsupported bundle schema")
if manifest["evidence"]["exe_sha256"] != EXPECTED_EXE_SHA256:
    fail("unexpected FD2.EXE SHA-256")
if (manifest["relocation_summary"]["record_count"] != 7959 or
        manifest["relocation_summary"]["unique_source_count"] != 7948 or
        manifest["relocation_summary"]["duplicate_record_count"] != 11):
    fail("unexpected relocation totals")

memory = currentProgram.getMemory()
for block in list(memory.getBlocks()):
    memory.removeBlock(block, monitor)
for file_bytes in list(memory.getAllFileBytes()):
    if not memory.deleteFileBytes(file_bytes):
        fail("could not delete temporary import FileBytes")

object_by_index = dict((row["object"], row) for row in manifest["objects"])
for row in manifest["objects"]:
    init_path = os.path.join(bundle_dir, row["init_file"])
    if sha256_file(init_path) != row["init_sha256"]:
        fail("object payload SHA-256 mismatch: " + init_path)
    stream = java.io.FileInputStream(init_path)
    try:
        file_bytes = memory.createFileBytes(
            row["init_file"], 0,
            long(row["initialized_size"]), stream, monitor)
    finally:
        stream.close()
    block_name = "OBJ%d.init" % row["object"]
    block = memory.createInitializedBlock(
        block_name, addr(row["relbase"]), file_bytes, 0,
        long(row["initialized_size"]), False)
    perms = row["permissions"]
    block.setPermissions(perms[0] == "r", perms[1] == "w", perms[2] == "x")
    block.setSourceName("FD2.EXE object%d initialized pages" % row["object"])
    block.setComment(
        "OBJ=%d OFF=0 VA=0x%x VSize=0x%x EXE_SHA256=%s FIRST_PAGE_FILE=0x%x" %
        (row["object"], row["relbase"], row["vsize"], EXPECTED_EXE_SHA256,
         row["first_page_file_offset"]))
    if row["bss_size"]:
        bss_start = row["relbase"] + row["initialized_size"]
        bss = memory.createUninitializedBlock(
            "OBJ%d.bss" % row["object"], addr(bss_start),
            long(row["bss_size"]), False)
        bss.setPermissions(perms[0] == "r", perms[1] == "w", perms[2] == "x")
        bss.setSourceName("FD2.EXE object%d zero-fill" % row["object"])
        bss.setComment(
            "OBJ=%d OFF=0x%x VA=0x%x BSS_SIZE=0x%x EXE_SHA256=%s" %
            (row["object"], row["initialized_size"], bss_start,
             row["bss_size"], EXPECTED_EXE_SHA256))

actual_blocks = []
for block in memory.getBlocks():
    actual_blocks.append((
        block.getName(), block.getStart().getOffset(), block.getSize(),
        block.isRead(), block.isWrite(), block.isExecute(), block.isInitialized()))
if tuple(actual_blocks) != EXPECTED_BLOCKS:
    fail("canonical block inventory mismatch: %r" % (actual_blocks,))

relocation_table = currentProgram.getRelocationTable()
unique_writes = {}
# 先验证全部 raw prevalue。relocation source 可能相互重叠；边遍历边写会让
# 后续 source 看到已重定位字节，从而错误失败。
for row in manifest["relocations"]:
    source = long(row["source_va"])
    target = long(row["target_va"])
    previous = unique_writes.get(source)
    if previous is not None and previous != target:
        fail("conflicting duplicate source 0x%x" % source)
    if previous is None:
        original = bytearray(4)
        original[0] = memory.getByte(addr(source)) & 0xff
        original[1] = memory.getByte(addr(source + 1)) & 0xff
        original[2] = memory.getByte(addr(source + 2)) & 0xff
        original[3] = memory.getByte(addr(source + 3)) & 0xff
        raw_u32 = struct.unpack("<I", bytes(original))[0]
        if raw_u32 != row["raw_u32"] or raw_u32 != row["target_offset"]:
            fail("source prevalue mismatch at 0x%x: program=0x%x manifest=0x%x target_offset=0x%x bytes=%s" %
                 (source, raw_u32, row["raw_u32"], row["target_offset"],
                  "".join("%02x" % (b & 0xff) for b in original)))
        unique_writes[source] = target
for source in sorted(unique_writes):
    memory.setBytes(addr(source), struct.pack("<I", unique_writes[source] & 0xffffffff))
for row in manifest["relocations"]:
    source = long(row["source_va"])
    target = long(row["target_va"])
    original = struct.pack("<I", row["raw_u32"] & 0xffffffff)
    values = [target, long(row["target_object"]), long(row["target_offset"])]
    symbol = ("FD2_LE_OBJ%d_OFF_%x_REC_%d" %
              (row["target_object"], row["target_offset"], row["record_index"]))
    relocation_table.add(
        addr(source), Relocation.Status.APPLIED,
        RELOCATION_TYPE_FD2_LE_INTERNAL_OFFSET32, values, original, symbol)

if len(unique_writes) != 7948 or relocation_table.getSize() != 7959:
    fail("applied relocation totals mismatch")

entry = addr(manifest["le"]["entry_va"])
symbols = currentProgram.getSymbolTable()
symbols.createLabel(entry, "fd2_le_entry", SourceType.IMPORTED)
symbols.addExternalEntryPoint(entry)
currentProgram.setImageBase(addr(0), False)
currentProgram.setExecutableFormat("FD2 bound Linear Executable (canonical bundle)")
currentProgram.setExecutablePath(manifest["evidence"]["exe_path"])
currentProgram.setExecutableSHA256(EXPECTED_EXE_SHA256)
currentProgram.setCompiler("Open Watcom 32-bit DOS/4G; ABI provisional")

# 每条 relocation 都有 source→target reference。此阶段只声明 DATA 关系，
# 不在缺少 operand/table schema 时伪造 CALL/READ/WRITE 类型。
references = currentProgram.getReferenceManager()
for row in manifest["relocations"]:
    references.addMemoryReference(
        addr(row["source_va"]), addr(row["target_va"]),
        RefType.DATA, SourceType.IMPORTED, 0)

print("FD2 canonical import: blocks=4 objects=3 relocations=7959 unique_sources=7948 references=7959")
