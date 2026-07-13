# r2 与 Ghidra 地址映射关系（修正版）

> 2026-07-11 修正：`LE+0x80` 的 data-page offset 相对文件开头，旧重建脚本却再次加上 `le_off`。当前 code0/dual、r2 函数清单和 Ghidra 输出已按 file-relative bound payload 重建；stage 单位构造链证据见 `docs/10-field-unit-constructor.md`。

## 当前权威文件

| 文件 | 说明 |
|------|------|
| `tools/fd2_le_raw.bin` | object 按 LE relbase 摆放；用于 DS object2/3 和 fixup 数据分析 |
| `tools/fd2_le_code0.bin` | bound payload 的 file-relative 视图：`code0 = file_offset - data_pages_off` |
| `tools/fd2_le_dual_clean.bin` | code-only：完整 code0 放在 `0x10000`，前 64 KiB 同时镜像到低地址 |
| `tools/fd2_le_ghidra_chkstk.bin` | 兼容文件名；与 raw 相同，不做 patch |
| `docs/le-fixups.txt` | fixup 说明，提醒不要直接 patch 代码 |
| `tools/fd2_le_fixups.txt` | 完整 fixup 简单记录索引；只用于辅助解释 DS/global 引用 |
| `tools/dump_fd2_fixup_table.py` | 按对象与偏移转储 relocation-backed 指针表；例如 `--ds 0x1d71` 查看 object2 关卡脚本分发表，`--object 3 --ds 0x27d8` 查看 object3 表 |
| `tools/analyze_fd2_stage_code.py` | 在关卡分发入口附近扫描可识别调用，辅助列出 FDTXT fragment、镜头/角色移动等待等过场线索 |

重建命令：

```bash
python3 tools/rebuild_fd2_analysis.py
```

## 地址约定

文档中的函数地址仍使用 relbase 线性地址：

| 功能 | 地址 | 说明 |
|------|------|------|
| entry0 | 0x3ccb4 | 程序入口（object1 offset 0x2ccb4 + relbase 0x10000） |
| __chkstk | dual `0x5c243` / code0 `0x4c243` | Watcom 栈检查 wrapper；不做字节 patch |
| res_load | dual `0x463ce` / code0 `0x363ce` | 资源加载器 |
| boot_intro_title_entry | 0x44aa8 | 调用入口；Watcom `push 0x88; call __chkstk` 栈检查前缀 |
| boot_intro_title | 0x44ab2 | 启动片头+标题主体；从 `push ebx` 开始 |
| animation_play | 0x45635 | ANI.DAT/AFM 动画播放器 |
| new_game_opening_play | dual `0x5752f` / code0 `0x4752f` | 新游戏完整开场 |

## 注意事项

- 不再使用 `tools/fd2_dual_final.bin` 作为权威输入。
- 不要把 LE fixup 记录机械写入代码页；这会把 `mov ecx,0xf` 等正常指令改坏。
- near call 的目标使用代码段 offset；DS/global 引用（如 `0x1a4d`、`0x3a65`）需要结合对象/段语义解释。
- 间接调用表不要直接按干净镜像中的原始 dword 解释；先用 `tools/dump_fd2_fixup_table.py --ds <offset> --count <n>` 查看 LE fixup 目标。若表位于 object3/BSS，显式传入 `--object 3`。
- FD2 object1 fixup 先按旧绑定窗口扣除 `0x28b8`，再加 LE header file offset `0x27acc`，才能进入统一 code0：`code0 = target_offset - 0x28b8 + 0x27acc`。例如 `DS:0x1d71` entry 0 的 `off=0x2231b` 对应 `new_game_opening_play @code0 0x4752f`；`DS:0x1b91` action 0 的 `off=0x24531` 对应 `code0 0x49745`。
- action 0 的 code0 `0x49745` 同时对应 `FD2.EXE` file `0x5a545`；机器码与 DOSBox 运行时 `DS:0x1b91[0]=0x190531` 指向的入口一致。这里修正的是 fixup 目标到 file-relative 分析视图的换算，不向代码页写入 fixup。
- 2026-07-13 使用 Ubuntu `dosbox-debug 0.74-3` 在 battlefield overlay 已加载后暂停：当前运行的 `CS=0x0170`、`DS=0x0178` 均为 flat selector。先从 `CS:0x16f003` 的 24 字节序列反查到 `code0 0x28217`，得到本次运行的 code0 relocation delta `0x146dec`；据此将 `field_physical_attack_resolve @code0 0x33edb` 定位到 runtime `0x17acc7`。重定位后的机器码在 `0x17ada7/0x17adec/0x17ae0c` 分别直接引用 `0x1ada12`、`0x1ada2a`、`0x1ae4a8`；`MEMDUMPBIN 178:<addr>` 捕获到完整 terrain attack、terrain defense 和 profile critical 表。另以 `DEBUG FD2.EXE` 启动，在 `BP 170:1AABE3` 设置首次 `field_rng_next` 入口断点；断点命中前 `MEMDUMPBIN 178:1AE7B8 2` 得到 loader 初值 `0x7a18`。selector 与 load base 可能随运行变化，后续捕获仍须先由 runtime 指令反推，不得把这些绝对地址当作跨运行常量。
- DOSBox debugger 中 selector-relative offset 与 `D 0:<linear>` 也不能混用。stage 单位构造器的 table helper 执行 `lea ..., [0x1b3af9]` 后，再以 DS 访问返回 offset；本次运行的 DS descriptor base 使实际敌军表首记录落在 debugger linear `0x1b3ae4`，与 `D 0:0x1b3af9` 相差 `0x15`。角色基础表和成长表有相同偏移关系。表首通过 record stride、unit 96 level 2 实机值和 unit 1 存档值交叉验证；不得把 `D 0:` 的显示位置直接当作 selector-relative 表首。
- `DAT_000027d8` 移动脚本表位于 object3。其 fixup `target_offset` 转到当前 raw object3 数据窗口时同样需扣除 `0x28b8`，再加 object3 relbase；例如 script 3 的 `off=0x29d1` 对应 `data_offset=0x119`、`raw=0x600119`。`dump_fd2_fixup_table.py --object 3` 会直接输出这两个地址。脚本 `0..10` 与 `0x5a..0x69` 均可按 `group_count / step_or_mode / actor pairs` 解码。
- 关卡分发目标来自 object1 内偏移。查看 `tools/fd2_le_code0.bin` 时使用 `dump_fd2_fixup_table.py` 输出的 `code0=...`；查看 `tools/fd2_le_dual_clean.bin` / relbase 视图时使用 `dual=...`。目标可能落在 Ghidra 未正确切分的函数边界，需结合运行时入口、反汇编窗口和实际分支判断，不能只依赖旧的自动函数边界。
- `docs/ghidra-decomp-all.c` 已从修正后的 code-only dual 镜像重新生成，共输出 978 个函数。18 个已确认入口被自动分析并入相邻大函数，文件头保留独立 corrected marker；读取这些入口时仍需结合 r2/Capstone 边界。
- Ghidra 反编译结果由 `tools/ghidra-scripts/decompile_clean.py` 重新生成到 `docs/ghidra-decomp-all.c`。
- Ghidra 反编译结果必须用 r2/Capstone 反汇编交叉核对，尤其是跨页函数和 Watcom 运行库函数。
