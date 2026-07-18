# r2 与 Ghidra 地址映射关系（修正版）

> 2026-07-18 规范化：FD2 是 bound executable。真正拥有 LE 头的是 embedded MZ `@file 0x25214`，其 `e_lfanew=0x28b8` 指向 LE `@file 0x27acc`。LE `+0x80=0x10e00` 相对该 module 开头，因此 enumerated page 1 位于 `0x25214+0x10e00=0x36014`，不是文件 `0x10e00`，也不是 `LE+0x10e00`。错误的 `0x10e00` 基址曾把外层 bound payload 当作 LE object pages。

Ghidra 的唯一规范主键现为 LE relbase linear VA。权威重建流程见 `docs/reverse-engineering/ghidra-reconstruction.md`。旧的 dual 镜像只保留给历史工具兼容，不再作为 Ghidra 输入。

## 当前权威文件

| 文件 | 说明 |
|------|------|
| `tools/fd2_le_raw.bin` | object 按 LE relbase 摆放；用于 DS object2/3 和 fixup 数据分析 |
| `tools/fd2_le_code0.bin` | object1 从 offset 0 开始的真实 LE page view |
| `tools/fd2_le_dual_clean.bin` | 历史兼容文件：含低地址镜像，禁止作为规范 Ghidra 输入 |
| `tools/fd2_le_ghidra_chkstk.bin` | 兼容文件名；与 raw 相同，不做 patch |
| `tools/fd2_le_relocated_relbase.bin` | 单独输出的 loader-relocated relbase 镜像；不得冒充 raw |
| `tools/fd2_le_relocation_manifest.tsv` | 全部 7959 条记录的 source/target/prevalue/duplicate 清单 |
| `docs/generated/le-fixups.txt` | fixup 生成摘要 |
| `tools/fd2_le_fixups.txt` | 完整 internal offset fixup 索引 |
| `tools/dump_fd2_fixup_table.py` | 按对象与偏移转储 relocation-backed 指针表；例如 `--ds 0x1d71` 查看 object2 关卡脚本分发表，`--object 3 --ds 0x27d8` 查看 object3 表 |
| `tools/analyze_fd2_stage_code.py` | 在关卡分发入口附近扫描可识别调用，辅助列出 FDTXT fragment、镜头/角色移动等待等过场线索 |

重建命令：

```bash
python3 tools/rebuild_fd2_analysis.py
python3 tools/validate_le_fixups.py
```

2026-07-15 的独立验证结果：主 parser 与独立 decoder 对 7959 条记录逐条一致；7948 个唯一 source、11 个同 target 重复 source；原始 file source 值全部等于 target offset；非 source 字节在静态 relocated 镜像中不变。另在 `BP 170:198CB4` 的 object1 EIP transfer 前用 `dosbox-debug` 转储 2 MiB，确认 object base `0x16c000/0x1ac000/0x1b2000`；全部 7959 个 runtime source 和三个 object 的 file-backed 非 source 字节逐字节通过。验证命令见 `docs/tools/dosbox-debug.md`。

## 地址约定

文档中的函数地址统一使用 relbase 线性 VA。结构化坐标写为 `VA=<linear> OBJ=<ordinal> OFF=<object offset>`：

| 功能 | 地址 | 说明 |
|------|------|------|
| entry0 | 0x3ccb4 | 程序入口（object1 offset 0x2ccb4 + relbase 0x10000） |
| __chkstk | VA `0x3702f` / code0 `0x2702f` | Watcom 栈检查 runtime helper；不做字节 patch |
| title_action_menu | VA `0x1f894` / code0 `0xf894` | 当前机器码确认的标题 action 菜单 wrapper |
| animation_play | VA `0x20421` / code0 `0x10421` | ANI.DAT/AFM 动画播放器 |
| music_track_play | VA `0x25977` / code0 `0x15977` | FDMUS 音乐 wrapper |
| sfx_play | VA `0x25a96` / code0 `0x15a96` | primary PCM wrapper |
| new_game_opening_play | VA `0x3231b` / code0 `0x2231b` | 新游戏完整开场；object1 fixup 直接给出 offset `0x2231b` |

## 注意事项

- 不再使用 `tools/fd2_dual_final.bin` 作为权威输入。
- 不得用错误 page base 或 target bias 写 relocation。合法 fixup 只写入独立 relocated 镜像，并由 manifest 限定 source。
- near call 的目标使用代码段 offset；DS/global 引用（如 `0x1a4d`、`0x3a65`）需要结合对象/段语义解释。
- 间接调用表不要直接按干净镜像中的原始 dword 解释；先用 `tools/dump_fd2_fixup_table.py --ds <offset> --count <n>` 查看 LE fixup 目标。若表位于 object3/BSS，显式传入 `--object 3`。
- LE internal fixup 的 `target_offset` 已经是目标 object 内偏移，不再应用 `-0x28b8+0x27acc`。例如 object1 `off=0x24531` 的 file offset 是 `0x36014+0x24531=0x5a545`；object1 code0 仍为 `0x24531`，dual/relbase 地址为 `0x34531`。
- `0x28b8` 只用于 embedded MZ 的 `e_lfanew`，不是 fixup target bias。此前把它用于 target 换算属于错误模型。
- 2026-07-13 使用 Ubuntu `dosbox-debug 0.74-3` 在 battlefield overlay 已加载后暂停：当前运行的 `CS=0x0170`、`DS=0x0178` 均为 flat selector。先从 `CS:0x16f003` 的 24 字节序列反查到 `code0 0x28217`，得到本次运行的 code0 relocation delta `0x146dec`；据此将 `field_physical_attack_resolve @code0 0x33edb` 定位到 runtime `0x17acc7`。重定位后的机器码在 `0x17ada7/0x17adec/0x17ae0c` 分别直接引用 `0x1ada12`、`0x1ada2a`、`0x1ae4a8`；`MEMDUMPBIN 178:<addr>` 捕获到完整 terrain attack、terrain defense 和 profile critical 表。另以 `DEBUG FD2.EXE` 启动，在 `BP 170:1AABE3` 设置首次 `field_rng_next` 入口断点；断点命中前 `MEMDUMPBIN 178:1AE7B8 2` 得到 loader 初值 `0x7a18`。selector 与 load base 可能随运行变化，后续捕获仍须先由 runtime 指令反推，不得把这些绝对地址当作跨运行常量。
- DOSBox debugger 中 selector-relative offset 与 `D 0:<linear>` 也不能混用。stage 单位构造器的 table helper 执行 `lea ..., [0x1b3af9]` 后，再以 DS 访问返回 offset；本次运行的 DS descriptor base 使实际敌军表首记录落在 debugger linear `0x1b3ae4`，与 `D 0:0x1b3af9` 相差 `0x15`。角色基础表和成长表有相同偏移关系。表首通过 record stride、unit 96 level 2 实机值和 unit 1 存档值交叉验证；不得把 `D 0:` 的显示位置直接当作 selector-relative 表首。
- `DAT_000027d8` 移动脚本表位于 object3。fixup `target_offset` 直接作为 object3 offset；例如 script 3 的 `off=0x29d1` 对应 raw `0x6029d1`。脚本 `0..10` 与 `0x5a..0x69` 均可按 `group_count / step_or_mode / actor pairs` 解码。
- 关卡分发目标来自 object1 内偏移。查看 `tools/fd2_le_code0.bin` 时使用 `dump_fd2_fixup_table.py` 输出的 `code0=...`；查看 `tools/fd2_le_dual_clean.bin` / relbase 视图时使用 `dual=...`。目标可能落在 Ghidra 未正确切分的函数边界，需结合运行时入口、反汇编窗口和实际分支判断，不能只依赖旧的自动函数边界。
- `docs/generated/ghidra-decomp-all.c` 是旧 dual 流程的历史产物，不能作为当前规范地址或边界的唯一证据。
- 规范 Ghidra 重建由 `tools/rebuild_fd2_ghidra.py` 编排。默认 structural inventory 不覆盖大型 C；需要候选 C 时显式使用 `--decompile`。
- Ghidra 反编译结果必须用 r2/Capstone 反汇编交叉核对，尤其是跨页函数和 Watcom 运行库函数。
