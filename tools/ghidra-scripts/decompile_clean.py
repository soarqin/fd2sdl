# -*- coding: utf-8 -*-
"""Ghidra headless post-script for FD2 clean decompilation.

Input program should be tools/fd2_le_dual_clean.bin imported as raw x86:LE:32.
The binary intentionally mirrors object1[0..64K) at 0x0 so near calls such as
`call 0xf488` are resolvable, but it does not apply LE fixups to code bytes.
"""

import os
import re
import java.io
from ghidra.app.decompiler import DecompInterface
from ghidra.app.cmd.function import CreateFunctionCmd
from ghidra.app.cmd.disassemble import DisassembleCommand
from ghidra.program.model.address import AddressSet

ROOT = "/home/soar/src/fd2sdl"
FUNCS = os.path.join(ROOT, "docs", "r2_funcs.txt")
OUT = os.path.join(ROOT, "docs", "ghidra-decomp-all.c")

fm = currentProgram.getFunctionManager()
af = currentProgram.getAddressFactory()
space = af.getDefaultAddressSpace()
listing = currentProgram.getListing()
mem = currentProgram.getMemory()

KNOWN = {
    0x3ccb4: "entry0",
    0x0e902: "res_load",
    0x1cfdc: "boot_intro_title_entry",
    0x1cfe6: "boot_intro_title",
    0x0f488: "dac_apply_darkness",
    0x1db69: "animation_play",
    0x3459f: "afm_opcode_palette_raw",
    0x345ad: "afm_opcode_palette_rle",
    0x34628: "afm_opcode_fill_framebuffer",
    0x34650: "afm_opcode_copy_framebuffer",
    0x3466c: "afm_opcode_rle_framebuffer",
    0x346b1: "afm_opcode_sparse_pixel",
    0x346ca: "afm_opcode_sparse_run",
    0x346f4: "afm_opcode_sparse_literal",
    0x35058: "vga_clear",
    0x347b6: "mem_alloc",
    0x34eb6: "mem_free",
    0x34a6c: "file_open",
    0x34b12: "file_read",
    0x35088: "file_seek",
    0x34ce4: "file_close",
    0x4bac9: "vsync_wait",
    0x0dd68: "input_check",
    0x35a23: "coro_switch",
    0x35a31: "event_pump",
    0x35a1e: "coro_yield",
    0x1cf66: "intro_anim_with_palette",
    0x1cfca: "palette_fade_out_dark",
    0x1cc6d: "palette_fade_in_light",
    0x2b649: "palette_fade_to",
    0x1ce87: "intro_cutaway",
    0x1d6c1: "title_menu_draw",
    0x2328d: "pal_mask_set",
    0x231de: "sfx_play",
    0x13fce: "blit_image",
    0x4c0d5: "blit_image_clipped",
    0x0f5f8: "memcpy_vga",
    0x0f53a: "pal_partial_set",
    0x3473c: "anim_exec_bytecode",
    0x3471b: "anim_buffer_init",
    0x1db0f: "dac_fill_rgb",
}

KNOWN_DESC = {
    0x3ccb4: u"程序入口",
    0x0e902: u"资源加载器",
    0x1cfdc: u"片头+标题调用入口/栈检查前缀",
    0x1cfe6: u"片头+标题主体",
    0x0f488: u"DAC 暗度应用",
    0x1db69: u"ANI/AFM 动画播放器",
    0x3459f: u"AFM opcode 1: 原始调色板",
    0x345ad: u"AFM opcode 2: 调色板 RLE",
    0x34628: u"AFM opcode 4: 显存填充",
    0x34650: u"AFM opcode 5: 显存复制",
    0x3466c: u"AFM opcode 6: 显存 RLE",
    0x346b1: u"AFM opcode 7: 稀疏单点",
    0x346ca: u"AFM opcode 8: 稀疏 run",
    0x346f4: u"AFM opcode 9: 稀疏 literal",
    0x1cf66: u"片头动画+调色板切换",
    0x1cfca: u"调色板淡出变暗",
    0x1cc6d: u"调色板淡入变亮",
    0x2b649: u"调色板向基色渐变",
    0x1ce87: u"片头切入画面",
    0x1d6c1: u"标题菜单绘制",
    0x3473c: u"动画字节码执行",
    0x3471b: u"动画缓冲初始化",
}


def addr(x):
    return space.getAddress(long(x))


def in_memory(a):
    try:
        return mem.contains(addr(a))
    except:
        return False


def create_func(a, size_hint):
    if not in_memory(a):
        return False
    start = addr(a)
    end = addr(min(a + max(size_hint, 32), currentProgram.getMaxAddress().getOffset()))
    try:
        DisassembleCommand(AddressSet(start, end), None).applyTo(currentProgram)
    except Exception as e:
        pass
    fn = fm.getFunctionAt(start)
    if fn is None:
        try:
            CreateFunctionCmd(start).applyTo(currentProgram)
        except Exception as e:
            pass
        fn = fm.getFunctionAt(start)
    if fn is not None:
        if a in KNOWN:
            try:
                fn.setName(KNOWN[a], ghidra.program.model.symbol.SourceType.USER_DEFINED)
            except Exception as e:
                pass
        return True
    return False

# Seed from r2-discovered functions, plus known hand-confirmed entries.
addrs = {}
pat = re.compile(r"^0x([0-9a-fA-F]+)\s+(\d+)")
for line in open(FUNCS, "r"):
    m = pat.match(line.strip())
    if not m:
        continue
    a = int(m.group(1), 16)
    sz = int(m.group(2))
    # Skip the huge synthetic mirror function at 0; create smaller known mirror funcs instead.
    if a == 0 and sz > 4096:
        continue
    addrs[a] = sz
for a in KNOWN:
    addrs.setdefault(a, 256)

created = 0
for a in sorted(addrs):
    if create_func(a, addrs[a]):
        created += 1
print("FD2 clean decompile: functions seeded=%d" % created)

# __chkstk was patched to ret 4 in the imported image; make sure Ghidra does not treat it as no-return.
chk = fm.getFunctionAt(addr(0x34777))
if chk:
    try:
        chk.setNoReturn(False)
    except Exception:
        pass

# Decompile only meaningful code ranges. Avoid duplicate low mirror except its called helpers.
ranges = [
    (0x0000, 0x10000),
    (0x10000, 0x4ef29),
]

decomp = DecompInterface()
decomp.openProgram(currentProgram)
pw = java.io.PrintWriter(java.io.OutputStreamWriter(java.io.FileOutputStream(OUT), "UTF-8"))
pw.println("/* AUTO-GENERATED by tools/ghidra-scripts/decompile_clean.py")
pw.println(" * Input: tools/fd2_le_dual_clean.bin (no LE fixups applied to code; __chkstk analysis patch only)")
pw.println(" * Do not edit semantic names here without updating docs/function-names.md.")
pw.println(" */")
n = 0
for fn in fm.getFunctions(True):
    off = fn.getEntryPoint().getOffset()
    keep = False
    for lo, hi in ranges:
        if lo <= off < hi:
            keep = True
            break
    if not keep:
        continue
    res = decomp.decompileFunction(fn, 120, None)
    if res and res.getDecompiledFunction():
        nm = fn.getName()
        desc = KNOWN_DESC.get(off)
        if desc:
            pw.println(u"// FUNC 0x%x %s   /* %s */" % (off, nm, desc))
        else:
            pw.println("// FUNC 0x%x %s" % (off, nm))
        pw.println(res.getDecompiledFunction().getC())
        n += 1
pw.flush()
pw.close()
decomp.dispose()
print("FD2 clean decompile: wrote %d functions to %s" % (n, OUT))
