# r2 与 Ghidra 地址映射关系（修正版）

> 2026-07-10 修正：上一版把 `tools/fd2_dual_final.bin` 称为最终镜像，但该文件已被错误 LE fixup 污染，会覆盖启动函数指令。详见 `docs/07-decompilation-corrections.md`。

## 当前权威文件

| 文件 | 说明 |
|------|------|
| `tools/fd2_le_raw.bin` | object 按 LE relbase 摆放，未应用 fixup，保留原始代码字节 |
| `tools/fd2_le_code0.bin` | object1 从 0 开始，便于按 near-call offset 反汇编 |
| `tools/fd2_le_ghidra_chkstk.bin` | 仅对 `__chkstk` 做分析 patch，供 Ghidra 使用 |
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
| __chkstk | 0x34777 | 栈检查；只在 `fd2_le_ghidra_chkstk.bin` 中为分析 patch |
| res_load | 0x0e902 / relbase 0x1e902 | 资源加载器（代码 near-call 使用 object offset） |
| boot_intro_title_entry | 0x1cfdc | 调用入口；Watcom `push 0x88; call __chkstk` 栈检查前缀 |
| boot_intro_title | 0x1cfe6 | 启动片头+标题主体；从 `push ebx` 开始 |
| animation_play | 0x1db69 | ANI.DAT/AFM 动画播放器 |
| new_game_opening_play | 0x2fa63 / code0 0x1fa63 | 新游戏完整开场；包含跨 64 KiB near call，优先直接反汇编 code0 |

## 注意事项

- 不再使用 `tools/fd2_dual_final.bin` 作为权威输入。
- 不要把 LE fixup 记录机械写入代码页；这会把 `mov ecx,0xf` 等正常指令改坏。
- near call 的目标使用代码段 offset；DS/global 引用（如 `0x1a4d`、`0x3a65`）需要结合对象/段语义解释。
- 间接调用表不要直接按干净镜像中的原始 dword 解释；先用 `tools/dump_fd2_fixup_table.py --ds <offset> --count <n>` 查看 LE fixup 目标。若表位于 object3/BSS，显式传入 `--object 3`。
- `DAT_000027d8` 移动脚本表位于 object3。该表的 relocation target 转到当前 raw object3 数据窗口时需加 `0x28b8`；按此映射，脚本 `0,1,2,5,0x5a..0x69` 均可按 `group_count / step_or_mode / actor pairs` 完整解码。`target_offset` 仍表示目标对象内偏移，不能直接当 object1 code0 地址。
- 关卡分发目标来自 object1 内偏移。查看 `tools/fd2_le_code0.bin` 时使用 `dump_fd2_fixup_table.py` 输出的 `code0=...`；查看 `tools/fd2_le_dual_clean.bin` / relbase 视图时使用 `dual=...`/`relbase_linear=...`。目标常落在编译器生成的标签/共享代码块附近，甚至可能靠近上一条指令的立即数字节；不要只按单个地址作为完整函数边界，需结合 `tools/analyze_fd2_stage_code.py`、反汇编窗口和实际分支一起判断。
- Ghidra 反编译结果由 `tools/ghidra-scripts/decompile_clean.py` 重新生成到 `docs/ghidra-decomp-all.c`。
- Ghidra 反编译结果必须用 r2/Capstone 反汇编交叉核对，尤其是跨页函数和 Watcom 运行库函数。
