# -*- coding: utf-8 -*-
"""Ghidra headless post-script for FD2 clean decompilation.

Input program should be tools/fd2_le_dual_clean.bin imported as raw x86:LE:32.
The binary mirrors object1[0..64K) at 0x0 and maps complete LE object1 at
0x10000. DS object2/3 remain separate in fd2_le_raw.bin.
"""

import os
import re
import java.io
from ghidra.app.decompiler import DecompInterface
from ghidra.app.cmd.function import CreateFunctionCmd
from ghidra.app.cmd.disassemble import DisassembleCommand
from ghidra.program.model.address import AddressSet

ROOT = "/home/soar/src/fd2sdl"
FUNCS = os.path.join(ROOT, "docs", "generated", "r2_funcs.txt")
NAMES = os.path.join(ROOT, "docs", "reverse-engineering", "function-names.md")
OUT = os.path.join(ROOT, "docs", "generated", "ghidra-decomp-all.c")

fm = currentProgram.getFunctionManager()
af = currentProgram.getAddressFactory()
space = af.getDefaultAddressSpace()
listing = currentProgram.getListing()
mem = currentProgram.getMemory()

KNOWN = {
    0x3ccb4: "entry0",
    0x463ce: "res_load",
    0x44aa8: "boot_intro_title_entry",
    0x44ab2: "boot_intro_title",
    0x46f54: "dac_apply_darkness",
    0x45635: "animation_play",
    0x5c06b: "afm_opcode_palette_raw",
    0x5c079: "afm_opcode_palette_rle",
    0x5c0f4: "afm_opcode_fill_framebuffer",
    0x5c11c: "afm_opcode_copy_framebuffer",
    0x5c138: "afm_opcode_rle_framebuffer",
    0x5c17d: "afm_opcode_sparse_pixel",
    0x5c196: "afm_opcode_sparse_run",
    0x5c1c0: "afm_opcode_sparse_literal",
    0x5cb24: "vga_clear",
    0x5c243: "__chkstk",
    0x5c282: "mem_alloc",
    0x5c982: "mem_free",
    0x5c538: "file_open",
    0x5c5de: "file_read",
    0x5cb54: "file_seek",
    0x5c7b0: "file_close",
    0x73595: "vsync_wait",
    0x45834: "input_check",
    0x5d4ef: "coro_switch",
    0x5d4fd: "event_pump",
    0x5d4ea: "coro_yield",
    0x44a32: "intro_anim_with_palette",
    0x44a96: "palette_fade_out_dark",
    0x44739: "palette_fade_in_light",
    0x53115: "palette_fade_to",
    0x44953: "intro_cutaway",
    0x4518d: "title_menu_draw",
    0x4ad59: "pal_mask_set",
    0x4ab8b: "music_track_play",
    0x4acaa: "sfx_play",
    0x5e735: "ail_init_sample",
    0x5e8a8: "ail_set_sample_address",
    0x5e95c: "ail_start_sample",
    0x5e9ad: "ail_set_sample_loop_count",
    0x5ea19: "ail_end_sample",
    0x60009: "ail_init_sequence",
    0x60102: "ail_start_sequence",
    0x6016f: "ail_stop_sequence",
    0x60338: "ail_set_sequence_volume",
    0x603ba: "ail_set_sequence_loop_count",
    0x3ba9a: "blit_image",
    0x73ba1: "blit_image_clipped",
    0x470c4: "memcpy_vga",
    0x47006: "pal_partial_set",
    0x5c208: "anim_exec_bytecode",
    0x5c1e7: "anim_buffer_init",
    0x455db: "dac_fill_rgb",
    0x3f51f: "field_turn_cycle_run",
    0x414ee: "field_actor_group_flash",
    0x414f8: "field_actor_group_flash_core",
    0x4673b: "field_earthquake_effect",
    0x4725a: "field_transition_lut_mask",
    0x4982c: "field_stage_transition_effect",
    0x49836: "field_stage_transition_effect_core",
    0x3fa27: "field_turn_event_check",
    0x117e7: "field_controller_input",
    0x16f55: "field_empty_focus_menu_execute",
    0x1728c: "field_options_menu_execute",
    0x173e7: "field_command_first_enabled_select",
    0x1741c: "field_command_menu_open",
    0x176b4: "field_command_menu_close",
    0x177fc: "field_command_menu_input",
    0x17898: "field_command_menu_wait_key",
    0x179d5: "field_command_menu_draw",
    0x19df7: "field_secondary_menu_execute",
    0x4df09: "save_checksum_sum",
    0x4df28: "save_xor_crypt",
    0x40964: "field_unit_combat_stats_recompute_entry",
    0x4096e: "field_unit_combat_stats_recompute",
    0x35d6c: "field_actor_group_append",
    0x35e6e: "field_unit_stage_template_append",
    0x34531: "field_stage0_31_turn_action0",
    0x3460b: "field_stage0_31_turn_action1",
    0x34673: "field_stage0_31_turn_action2",
    0x346cd: "field_stage0_31_turn_action3",
    0x7333b: "map_sprite_solid_blit_24",
    0x3231b: "new_game_opening_play",
}

# function-names.md 是语义命名登记表；地址列已迁移为 corrected dual。
name_row = re.compile(r"^\|\s*0x([0-9a-fA-F]+)\s*\|[^|]*\|\s*([^|]+?)\s*\|")
for line in open(NAMES, "r"):
    match = name_row.match(line.strip())
    if match:
        KNOWN[int(match.group(1), 16)] = match.group(2).strip()

KNOWN_DESC = {
    0x3ccb4: u"程序入口",
    0x463ce: u"资源加载器",
    0x44aa8: u"片头+标题调用入口/栈检查前缀",
    0x44ab2: u"片头+标题主体",
    0x46f54: u"DAC 暗度应用",
    0x45635: u"ANI/AFM 动画播放器",
    0x5c06b: u"AFM opcode 1: 原始调色板",
    0x5c079: u"AFM opcode 2: 调色板 RLE",
    0x5c0f4: u"AFM opcode 4: 显存填充",
    0x5c11c: u"AFM opcode 5: 显存复制",
    0x5c138: u"AFM opcode 6: 显存 RLE",
    0x5c17d: u"AFM opcode 7: 稀疏单点",
    0x5c196: u"AFM opcode 8: 稀疏 run",
    0x5c1c0: u"AFM opcode 9: 稀疏 literal",
    0x44a32: u"片头动画+调色板切换",
    0x44a96: u"调色板淡出变暗",
    0x44739: u"调色板淡入变亮",
    0x53115: u"调色板向基色渐变",
    0x44953: u"片头切入画面",
    0x4518d: u"标题菜单绘制",
    0x5c208: u"动画字节码执行",
    0x5c1e7: u"动画缓冲初始化",
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

chk = fm.getFunctionAt(addr(0x5c243))
if chk:
    try:
        chk.setNoReturn(False)
    except Exception:
        pass

# Decompile the low 64 KiB mirror and complete object1.
ranges = [
    (0x0000, 0x10000),
    (0x10000, currentProgram.getMaxAddress().getOffset() + 1),
]

decomp = DecompInterface()
decomp.openProgram(currentProgram)
pw = java.io.PrintWriter(java.io.OutputStreamWriter(java.io.FileOutputStream(OUT), "UTF-8"))
pw.println("/* AUTO-GENERATED by tools/ghidra-scripts/decompile_clean.py")
pw.println(" * Input: tools/fd2_le_dual_clean.bin (real LE object1 pages; no fixups or __chkstk patch)")
pw.println(" * Do not edit semantic names here without updating docs/reverse-engineering/function-names.md.")
pw.println(" */")
missing_known = []
for entry in sorted(KNOWN):
    if fm.getFunctionAt(addr(entry)) is None:
        missing_known.append((entry, KNOWN[entry]))
if missing_known:
    pw.println("/* Confirmed entries folded into adjacent auto-analysis functions. */")
    for entry, name in missing_known:
        pw.println("// FUNC 0x%x %s" % (entry, name))
    pw.println("")
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
        # Ghidra 偶尔在空行输出空格；逐行去除尾随空白，保证重建后
        # `git diff --check` 稳定通过。
        for c_line in res.getDecompiledFunction().getC().splitlines():
            pw.println(c_line.rstrip())
        pw.println("")
        n += 1
pw.flush()
pw.close()
decomp.dispose()
print("FD2 clean decompile: wrote %d functions to %s" % (n, OUT))
