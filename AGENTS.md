# AGENTS.md - 项目协作规则

本项目（炎龙骑士团 2 SDL3 重写）基于对 1995 年 DOS 游戏 `FD2.EXE` 的逆向工程。
当前反编译结果在 `docs/ghidra-decomp-all.c`（1205 个函数），函数命名登记在 `docs/function-names.md`。
以下规则保证逆向成果可追溯、可验证。

## 1. 反编译函数命名与注释（强制）

**规则：每确认一个反编译函数的用途，必须在两处同步标注：**

1. **源码注释**：在 SDL3 实现代码（`src/*.c`）中，调用或复现某反编译函数逻辑时，
   注释里写明对应的反编译函数地址与原名，例如：
   ```c
   /* ANI.DAT 动画播放：复现 FUN_0001db69 @0x1db69 (animation_play)
    * 语义: animation_play(anim_idx, frame_delay_ms, check_input) */
   ```

2. **反编译文件标注**：在 `docs/ghidra-decomp-all.c` 中，在对应函数的
   `// FUNC 0x... ` 标记行后追加用途注释，例如：
   ```c
   // FUNC 0x1cfe6 boot_intro_title   /* 片头+标题主体 */
   ```

3. **函数重命名**：当函数用途确认且有把握时，应将 Ghidra 原名 `FUN_xxxxxx`
   改为语义命名（如 `boot_intro_title`）。重命名需在 `docs/function-names.md`
   登记映射表，供后续引用。

**判断「确认用途」的标准**：至少满足以下之一：
- 通过阅读反编译逻辑 + 交叉引用能明确语义
- 通过 SDL3 复现并对照 DOSBox 实机验证
- 通过数据文件格式印证

**禁止**：仅凭函数大小、调用次数、地址邻近等弱信号猜测命名。

## 2. r2 与 Ghidra 交叉印证

使用 r2 辅助 Ghidra 分析前，**必须**先按 `docs/06-r2-ghidra-mapping.md`
建立并验证地址映射。

当前权威流程：

```bash
python3 tools/rebuild_fd2_analysis.py
```

该脚本从 `original_game/FD2.EXE` 重建分析文件：

| 文件 | 用途 |
|------|------|
| `tools/fd2_le_raw.bin` | object 按 LE relbase 摆放，未应用 fixup |
| `tools/fd2_le_code0.bin` | object1 从 0 开始，便于按 near-call offset 反汇编 |
| `tools/fd2_le_ghidra_chkstk.bin` | 仅对 `__chkstk` 做分析 patch |
| `tools/fd2_le_dual_clean.bin` | Ghidra/r2 用镜像，0x0-0xffff 镜像 object1 前 64K，不应用 fixup |
| `tools/fd2_le_fixups.txt` | fixup 简单记录索引，只用于解释 DS/global 引用 |

**禁止**：把 LE fixup 直接写回代码页。旧的 `tools/fd2_dual_final.bin`、`/tmp/fd2_le_correct.bin`
以及任何「fixedup/patched」代码镜像都不是权威输入。

Ghidra 重新反编译使用：

```bash
/tmp/ghidra_11.3.2_PUBLIC/support/analyzeHeadless \
  /tmp/ghidra_fd2_clean_proj FD2Clean \
  -import tools/fd2_le_dual_clean.bin \
  -processor x86:LE:32:default -cspec gcc \
  -scriptPath tools/ghidra-scripts \
  -postscript decompile_clean.py \
  -overwrite -deleteProject
```

`tools/ghidra-scripts/decompile_clean.py` 是当前唯一保留的 Ghidra 重建脚本。

## 3. 数据结构与格式

- 所有逆向确认的数据格式记录在 `docs/03-data-formats.md`。
- 修改格式描述时，需附验证证据（反编译函数或实机对照）。
- `.DAT` 容器格式、FDOTHER 图像 RLE、调色板格式已确认。
- `FUN_0001db69 @0x1db69` 已确认为 ANI.DAT/AFM 动画播放器，不是调色板淡入淡出。
- 启动流程用到的 ANI.DAT opcode 1/2/4/5/6/7/8/9 已在 `src/animation.c` 复现。

## 4. 代码风格

- SDL3 C 代码，CMake 构建。
- 中文注释（与文档一致），代码标识符用英文。
- 每个源文件头部注释说明对应逆向依据。

## 5. 版本库与原版游戏文件

- `original_game/` 中的原版游戏文件、DOSBox 截图和本地运行数据不入库，由开发者自行复制。
- `src/build/`、`src/fd2sdl`、`tools/*.bin`、`.omo/`、`.pi-subagents/`、`.codegraph` 不入库。
- 可再生成的大型分析镜像不要提交；需要跨机器共享时提交脚本和文档即可。

## 6. 验证

每个可复现场景（启动、标题、战斗等）实现后，须与 DOSBox 实机截图对比。
DOSBox 配置和截图位于本地 `original_game/`，但不随 Git 提交。
