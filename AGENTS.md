# AGENTS.md - 项目协作规则

本项目（炎龙骑士团 2 SDL3 重写）基于对 1995 年 DOS 游戏 `FD2.EXE` 的逆向工程。
当前反编译结果在 `docs/generated/ghidra-decomp-all.c`，函数命名登记在 `docs/reverse-engineering/function-names.md`。文档分类和索引见 `docs/README.md`。
以下规则保证逆向成果可追溯、可验证。常用工具的已验证命令集中记录在 `docs/tools/README.md`；使用工具前先查该目录和对应专题文档，不要仅为回忆语法而反复请求 `--help`。

## 1. 反编译函数命名与注释（强制）

**规则：每确认一个反编译函数的用途，必须在两处同步标注：**

1. **源码注释**：在 SDL3 实现代码（`src/*.c`）中，调用或复现某反编译函数逻辑时，
   注释里写明对应的反编译函数地址与原名，例如：
   ```c
   /* ANI.DAT 动画播放：复现 FUN_0001db69 @0x45635 (animation_play)
    * 语义: animation_play(anim_idx, frame_delay_ms, check_input) */
   ```

2. **反编译文件标注**：在 `docs/generated/ghidra-decomp-all.c` 中，在对应函数的
   `// FUNC 0x... ` 标记行后追加用途注释，例如：
   ```c
   // FUNC 0x44ab2 boot_intro_title   /* 片头+标题主体 */
   ```

3. **函数重命名**：当函数用途确认且有把握时，应将 Ghidra 原名 `FUN_xxxxxx`
   改为语义命名（如 `boot_intro_title`）。重命名需在 `docs/reverse-engineering/function-names.md`
   登记映射表，供后续引用。

**判断「确认用途」的标准**：至少满足以下之一：
- 通过阅读反编译逻辑 + 交叉引用能明确语义
- 通过 SDL3 复现并对照 DOSBox 实机验证
- 通过数据文件格式印证

**禁止**：仅凭函数大小、调用次数、地址邻近等弱信号猜测命名。

## 2. r2 与 Ghidra 交叉印证

使用 r2 辅助 Ghidra 分析前，**必须**先按 `docs/reverse-engineering/address-mapping.md`
建立并验证地址映射。

当前权威流程：

```bash
python3 tools/rebuild_fd2_analysis.py
```

该脚本从 `original_game/FD2.EXE` 重建分析文件：

| 文件 | 用途 |
|------|------|
| `tools/fd2_le_raw.bin` | object 按 LE relbase 摆放；用于 DS object2/3 与 fixup 数据 |
| `tools/fd2_le_code0.bin` | 真实 LE object1 从 offset 0 开始的代码视图 |
| `tools/fd2_le_ghidra_chkstk.bin` | 兼容文件名；保留原始 `__chkstk`，不做 patch |
| `tools/fd2_le_dual_clean.bin` | code-only：完整 code0 放在 0x10000，并镜像低 64K |
| `tools/fd2_le_fixups.txt` | fixup 简单记录索引，只用于解释 DS/global 引用 |

**禁止**：把 loader-relocated 镜像覆盖 raw-unrelocated／Ghidra clean 输入，或在没有 manifest 和完整验证时直接 patch。合法的 loader relocation 可以写入 object1 代码中的绝对地址 source，但必须单独输出到 `tools/fd2_le_relocated_relbase.bin`。旧的 `tools/fd2_dual_final.bin`、`/tmp/fd2_le_correct.bin` 以及无 manifest 的 patched 镜像都不是权威输入。

### 完整 fixup 的严格验收

若后续生成 loader-relocated 运行时镜像，不得凭反汇编外观、少量地址命中或游戏经验宣称 fixup 正确。必须同时满足以下条件：

1. fixup parser 消费 fixup record table 的全部字节；每条记录的长度、source type、target flags、source list、additive 和 16/32 位字段均有 LE 格式规范依据。未知类型、剩余字节或越界记录一律使构建失败。
2. 使用两个相互独立的实现解码同一 fixup 表，并逐条比较 page、source、target、addend、写入宽度和记录边界。两个实现共享解析函数或复制同一分支逻辑不算独立验证。
3. raw-unrelocated 镜像与 loader-relocated 镜像必须分开输出。只允许在 relocation source 覆盖的字节范围写入；所有非 relocation 字节必须与 raw 镜像逐字节一致。重叠或冲突写入必须显式验证，不能按后写覆盖处理。
4. 使用 `dosbox-debug` 仅启动并转储已经加载的原版可执行对象；在进入任何游戏逻辑或 overlay 改写前，逐字节比较全部已提交 object page、BSS 零填充区和所有 relocation source。selector、object base 与 relocation delta 必须从该次运行重新取得。
5. 静态 loader 输出与运行时转储必须对全部对象逐字节一致；同时逐条验证每个 relocation source 的运行时值。只验证程序入口、几个函数或个别表不构成通过。
6. 任一字节不一致、任一记录未覆盖或任一验证步骤无法执行时，只能报告未通过；禁止把镜像用于 Ghidra 权威重建，也禁止声称「完全正确」。

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

`tools/ghidra-scripts/decompile_clean.py` 是当前唯一保留的 Ghidra 重建脚本。2026-07-15 已确认 `LE+0x80` 相对拥有 LE 头的 embedded MZ module `@0x25214`，真实 page 1 为 `0x36014`。旧的文件 `0x10e00` 基址和 `LE+0x10e00` 基址均错误；自动函数边界仍须与 r2/Capstone 和 DOSBox 转储交叉验证。

### 在线资料检索降级规则

- 查询外部格式规范、工具源码或公开文档时，优先使用已配置的 `ketch`。
- 若 `ketch` 因缺少后端配置、限流、网络错误或上游失败而无法完成检索，必须继续改用 Pi 自带的 Web Search 工具；不得因 `ketch` 失败就停止搜索。
- 若当前会话没有暴露 Pi Web Search，才可记录工具不可用，并改用已安装工具源码、系统包源码或其他可独立复核的本地资料。不得把搜索失败当作格式结论。
- 外部资料只作为规范和独立实现依据；FD2 的最终结论仍须由原始 `FD2.EXE`、完整 parser 验证和运行时逐字节对照确认。

## 3. 数据结构与格式

- 所有逆向确认的数据格式记录在 `docs/formats/data-formats.md`。
- 修改格式描述时，需附验证证据（反编译函数或实机对照）。
- `.DAT` 容器格式、FDOTHER 图像 RLE、调色板格式已确认。
- `FUN_0001db69 @0x45635` 已确认为 ANI.DAT/AFM 动画播放器，不是调色板淡入淡出。
- 启动流程用到的 ANI.DAT opcode 1/2/4/5/6/7/8/9 已在 `src/animation.c` 复现。

## 4. 代码风格

- SDL3 C 代码，CMake 构建。
- 中文注释（与文档一致），代码标识符用英文。
- 每个源文件头部注释说明对应逆向依据。

## 5. 版本库与原版游戏文件

- `original_game/` 中的原版游戏文件、DOSBox 截图和本地运行数据不入库，由开发者自行复制。
- `src/build/`、`src/fd2sdl`、`tools/*.bin`、`.omo/`、`.pi-subagents/`、`.codegraph` 不入库。
- 可再生成的大型分析镜像不要提交；需要跨机器共享时提交脚本和文档即可。

## 6. DOSBox 使用与验证

- 自动化流程应尽量避免启动 DOSBox 做交互测试。当前环境无法可靠操作 DOSBox 内的菜单、战场和特殊剧情，不能把无人值守运行当作有效的实机验证。
- `dosbox-debug` 仅用于转储已经加载的原版游戏可执行映像、运行时内存、表或其他二进制证据。具体命令和操作流程见 `docs/tools/dosbox-debug.md`；禁止用 `dosbox-debug --help` 探测帮助，因为本机版本会直接启动界面。使用前仍须按 `docs/reverse-engineering/address-mapping.md` 重新确认 selector、relocation delta 和目标地址；不得把一次运行的绝对地址当作固定常量。
- 不得使用 `dosbox-debug` 自动操作游戏流程、模拟特殊场景或据此声称完成视觉／行为对照。
- 当验证需要进入指定关卡、触发剧情、选择菜单、布置单位、执行特定攻击／法术／物品，或截取特殊场景时，必须停止自动化工作，并请开发者手工操作 DOSBox 后提供截图、存档、运行时内存转储或操作结果。
- 已有 DOSBox 截图、存档和转储位于本地 `original_game/`，只作只读证据，不随 Git 提交。
- 每个可复现场景（启动、标题、战斗等）实现后仍需与 DOSBox 实机结果对比；若当前缺少可操作的实机证据，必须明确记录为「待人工对照」，不得用 SDL 自测代替原版对照。
