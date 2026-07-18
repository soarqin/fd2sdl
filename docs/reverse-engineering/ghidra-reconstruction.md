# FD2.EXE 的 Ghidra 规范重建

本文说明从原始 `FD2.EXE` 重建 Ghidra 11.3.2 分析基线的当前权威流程。该流程统一使用 LE relbase linear VA，不再把低地址镜像、object1 offset、旧 bound-file 窗口地址或历史 Ghidra 地址混作函数主键。

## 适用范围

该流程可重复验证以下内容：

- bound MZ、LE header、object table、page map 和 entry；
- 三个 LE object 的地址、有效长度、权限和 object2 BSS；
- 7,959 条 LE internal offset relocation 记录及其 7,948 个唯一 source；
- relocation 前字节、应用后的字节和 source → target provenance；
- 证据分层的函数 seed、Watcom `__chkstk` wrapper/body 关系和边界冲突账本；
- 规范地址下的结构化函数清单和可选 C 反编译。

该流程不保证恢复 1995 年的原始 C 源码，也不保证所有未引用函数、间接调用、共享尾部、跳表、函数签名或 Watcom ABI 已完全恢复。

## 输入身份

当前基线固定为：

| 输入 | SHA-256 |
|------|---------|
| `original_game/FD2.EXE` | `bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd` |
| Ghidra 11.3.2 release ZIP | `99d45035bdcc3d6627e7b1232b7b379905a9fad76c772c920602e2b5d8b2dac2` |
| raw relbase oracle | `37bceab29c9ee9089b5c5f9c4abefdee26a0bb34ae47bb917783dcc23ee30fc2` |
| relocated relbase oracle | `8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1` |

`tools/fd2_le_relocated_relbase.bin` 只是验证 oracle。规范输入仍是原始 object 字节和已验证 relocation manifest。

## 规范内存模型

FD2 有三个 LE object。object2 包含 file-backed 区和 BSS，因此 Ghidra 中使用四个 memory block：

| Block | 地址范围 | 大小 | 权限 | 来源 |
|-------|----------|------|------|------|
| `OBJ1.init` | `0x10000..0x4ef28` | `0x3ef29` | R-X | object1 pages |
| `OBJ2.init` | `0x50000..0x53fff` | `0x4000` | RW- | object2 pages |
| `OBJ2.bss` | `0x54000..0x556af` | `0x16b0` | RW- | uninitialized zero-fill |
| `OBJ3.init` | `0x60000..0x634d1` | `0x34d2` | RW- | object3 pages |

object2 不是只读数据。LE flags 为 `0x2043`，且机器码会直接写 object2；例如 `new_game_opening_play @VA 0x3231b` 在 `VA 0x32326` 写入 `VA 0x53c03`。

禁止创建以下内容：

- 地址 0 的临时 import block；
- object1 前 64 KiB 的低地址镜像；
- object vsize 之外的 page padding；
- object2 或 object3 中的函数。

## 地址格式

函数和导出 marker 使用以下结构：

```text
VA=0x3231b OBJ=1 OFF=0x2231b
```

其中：

- `VA` 是唯一规范主键；
- `OBJ` 是 LE object ordinal；
- `OFF` 是 object 内 offset；
- object1 的 `code0` 只等于 `OFF`，不能作为跨 object 主键；
- file offset 由 page map 和 bound module owner 推导，不能用旧常数偏移替代。

当前关键入口：

| 语义 | VA | object1 offset |
|------|----|----------------|
| LE transfer entry | `0x3ccb4` | `0x2ccb4` |
| `title_action_menu` | `0x1f894` | `0x0f894` |
| `animation_play` | `0x20421` | `0x10421` |
| `music_track_play` | `0x25977` | `0x15977` |
| `sfx_play` | `0x25a96` | `0x15a96` |
| `new_game_opening_play` | `0x3231b` | `0x2231b` |
| `__chkstk` | `0x3702f` | `0x2702f` |

`entry @VA 0x3ccb4` 是 LE transfer entry。其前方和相邻区域包含 Watcom runtime 数据；它不能与应用初始化 callback 或 `main` 等价。

## 重建步骤

先重建并独立验证 LE 数据：

```bash
python3 tools/rebuild_fd2_analysis.py
python3 tools/validate_le_fixups.py
```

生成 canonical bundle：

```bash
python3 tools/fd2_analysis_bundle.py
```

bundle 位于被 Git 忽略的 `tools/fd2-analysis-bundle/`。它包含三个 object 初始化字节文件、版本化 `manifest.json` 和 `SHA256SUMS`。Ghidra Jython 脚本只读取 bundle，不复制第三套 LE parser。

运行 structural baseline，并对两个全新 project 做确定性比较：

```bash
python3 tools/rebuild_fd2_ghidra.py --determinism-check
```

加入证据分层函数 seed 和边界账本：

```bash
python3 tools/rebuild_fd2_ghidra.py --functions --determinism-check
```

可选导出规范地址下的 C：

```bash
python3 tools/rebuild_fd2_ghidra.py --decompile --determinism-check
```

默认 Ghidra 位置为 `/tmp/ghidra_11.3.2_PUBLIC/support/analyzeHeadless`，官方 release ZIP 为 `/tmp/ghidra_11.3.2_PUBLIC_20250415.zip`。runner 启动前同时检查安装目录的 `application.version=11.3.2`、`application.release.name=PUBLIC`，以及 release ZIP SHA-256；任一不符即 fail-closed。其他位置分别通过 `--ghidra-headless` 和 `--ghidra-release-zip` 显式指定。

## 脚本分层

| 脚本 | 职责 |
|------|------|
| `tools/fd2_analysis_bundle.py` | 从原版 EXE 和双重验证结果构建 versioned bundle |
| `tools/ghidra-scripts/import_fd2_canonical.py` | 重建 blocks、权限和 BSS，应用 relocation，登记 Ghidra relocation records |
| `tools/build_fd2_function_seeds.py` | 生成函数 seed、`__chkstk` wrapper 和 deferred candidate ledger |
| `tools/ghidra-scripts/analyze_fd2_functions.py` | 按 seed 建函数，记录每项 disposition，不静默强拆 overlap |
| `tools/ghidra-scripts/export_fd2_decomp.py` | 纯导出 canonical marker、C 和 decompile manifest |
| `tools/ghidra-scripts/validate_fd2_canonical.py` | 验证 blocks、权限、字节 oracle、relocation 计数和非代码 object 函数数 |
| `tools/rebuild_fd2_ghidra.py` | 在新临时 project 中编排上述步骤并检查确定性 |

旧的 `tools/ghidra-scripts/decompile_clean.py` 仅保留为历史脚本，不再是权威重建入口。它使用 dual 镜像、旧 `r2_funcs.txt` 和混址名称表，不能用于规范重建。

## Relocation 规则

当前原版包含：

- 7,959 条记录；
- 7,948 个唯一 source；
- 11 条相同 source、相同 target 的重复记录；
- source type 全部为 `0x07`；
- flags `0x00` 共 7,067 条，flags `0x10` 共 892 条。

import script 在写入前核对全部 raw prevalue。每个唯一 source 只写一次，但每条原始 record 都登记为 Ghidra `Relocation.Status.APPLIED`，并保留：

- source VA；
- target object、offset 和 VA；
- relocation 前 4 字节；
- 原始 record 序号和稳定 symbol 名。

structural inventory 将每条 relocation 保留为 source → target relation。精确的 instruction operand reference 或数据表 pointer reference 属于后续语义阶段；未分类关系必须有账，不能为了达到 100% materialized reference 而伪造 `CALL` reference。

## 函数证据分层

### Tier A：确认入口

- LE transfer entry；
- 已按当前机器码、xref 或已验证 dispatch table 重新确认的少量 semantic anchor；
- 从上述已建函数体递归解码并实际可达的 direct call target。

最后一类由 Ghidra function body 的 flow 逐层发现，不等价于对完整 object1 做线性扫描。当前确定性基线共创建 858 个 object1 函数，其中 315 个由可达 direct call 递归新增；函数数量只作本次构建统计，不是质量阈值。

### Tier C：Watcom `__chkstk` wrapper

当前 `__chkstk @VA 0x3702f` 有 541 个直接 callsite，全部匹配：

```asm
push frame_size
call __chkstk
body:
```

默认规则：

- `push` 地址是函数入口候选；
- `call` 后地址只建 body label；
- body 只有在存在独立 call、指针或运行时入口证据时，才可升级为 secondary function；
- 如果前一个已确认函数的合法 flow 已包含后续 `__chkstk` site，则记为 resolved internal site，不强拆第二个函数。

### Tier D：延后候选

以下信息只进入 candidate ledger，不自动创建函数：

- 对 object1 的 relocation target；
- 对完整 object1 线性解码得到的 direct-call-like target；
- Ghidra 或 r2 analyzer-only function starts；
- 函数序言模式、地址邻近或旧文档地址。

线性解码会把嵌入数据误判为指令，因此不能等价于「可达 direct call」。relocation target 落在 executable object 也不能自动证明它是函数。

## Watcom ABI 状态

Ghidra 11.3.2 没有内置 Open Watcom compiler spec。当前 structural/semantic baseline 使用 stock `gcc` cspec，并明确标为 provisional。

Open Watcom 32 位文档说明默认 register-based convention 的参数寄存器顺序为 `EAX, EDX, EBX, ECX`，随后才使用 stack。该事实尚未在本项目中形成完整 callsite fixture 和 custom cspec 验收，因此当前伪 C 的参数、callee-saved register、stack purge 和签名均不能视为权威。

参考：

- Open Watcom C/C++ User’s Guide：<https://open-watcom.github.io/open-watcom-1.9/cguide.html>
- 32-bit Assembly Language Considerations：<https://www.mikecramer.com/qnx/qnx_4.25_docs/watcom/compiler-tools/ccall32.html>
- Ghidra Open/Watcom support issue：<https://github.com/NationalSecurityAgency/ghidra/issues/156>

## 名称迁移

`docs/reverse-engineering/function-names.md` 当前仍是人类研究档案，包含旧 Ghidra 窗口、dual、code0 和少量 canonical VA 的混合历史。规范脚本不得解析该文件，也不得对全部行机械加减常数。

名称只有在以下条件全部满足后，才能进入 machine-readable confirmed registry：

1. candidate VA 位于 object1 vsize 内；
2. 当前 raw／relocated bytes 可复核；
3. xref、dispatch table、数据格式或 runtime evidence 可确认入口和语义；
4. wrapper/body 关系已分类；
5. semantic name 和 VA 一一对应。

未审计行应保留原始文字和 legacy address，但不能自动写成 Ghidra `USER_DEFINED` name。

当前逐行账本由 `python3 tools/build_fd2_name_migration.py` 生成：

- `docs/generated/fd2-name-migration.json`：保留全部 209 行历史记录和 disposition；
- `docs/generated/fd2-confirmed-function-names.json`：只含 7 个重新确认的 canonical anchor。

历史表中只有 5 行的 semantic name 可直接关联这 7 个 anchor；`fd2_le_entry` 与 `__chkstk` 没有可靠的同名历史行，因此 confirmed registry 仍独立记录。其余 204 行保持 `legacy-unresolved`。

## 验收条件

structural baseline 必须满足：

- 四个 block 的边界、权限、initialized/BSS 状态完全一致；
- 地址 0 未映射，无低地址镜像；
- 初始化字节除 relocation source 外不变；
- 应用 relocation 后与 relocated oracle 全对象逐字节一致；
- relocation 总数为 7,959，唯一 source 为 7,948，重复记录为 11；
- object2、object3 和 object 外函数数为 0；
- 两个全新 project 的 normalized inventory SHA-256 一致；
- host wrapper 只接受本次 post-script 写出的 `canonical-validation.ok`，避免 headless script 异常后把陈旧输出误判为成功。

function baseline 还必须满足：

- 每个 seed 有明确 disposition；
- overlap、decode failure 和 byte mismatch 不被静默吞掉；
- `__chkstk` helper 固定为 `VA 0x3702f`；
- wrapper/body 不出现系统性双函数；
- 上述七个关键 anchor 地址唯一；
- unresolved boundary conflict 明确列出。

函数数量只作为观测值，不是「越多越好」的质量指标。

## 当前不能宣称的结论

即使全部验收通过，也不能宣称：

- 伪 C 等价于原始源码；
- 所有函数和间接调用均已发现；
- 所有 relocation-to-code target 都是函数；
- Watcom ABI、参数、返回类型和局部变量完全正确；
- overlay、自修改代码或运行时生成行为不存在；
- SDL 重写已经完成 DOSBox 行为、视觉或音频对照。
