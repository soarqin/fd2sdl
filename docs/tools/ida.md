# IDA Domain API / idalib 操作说明

本文记录通过 `idalib` 对 `original_game/FD2.EXE` 做分析、复核和反编译的流程，分别列出 Windows PowerShell 与 Linux bash 命令。已有 IDA Pro 9.5 重导出和局部机器码执行证据来自 Linux，见 [`../reverse-engineering/ida-bytecheck-9.5.md`](../reverse-engineering/ida-bytecheck-9.5.md)；历史 9.4 分析结论仍见 [`../reverse-engineering/ida-crosscheck.md`](../reverse-engineering/ida-crosscheck.md)。Windows 环境需使用本机安装与工具解释器，不能沿用历史 `/home/soar/` 路径或将命令改写记为重新通过验收。

**不要用 `ida`（GUI）或 `idat` 做自动化。** 两个平台全部分析流程均走 `idalib`；Windows 桌面可用不改变数据库与证据的操作边界。

## 日常分析规范

- 先读取 `ida-domain-api` skill；编写或执行脚本时同时使用 `ida-domain-scripting` skill，按其执行器流程查询数据库。
- 新脚本优先使用 IDA Domain API。仅在 Domain API 缺少所需能力时局部调用低层 IDAPython，并说明原因；现有证据导出脚本不为统一 API 而重写。
- 优先复用已有数据库。2026-11-05 用户已撤销因旧 idat 流程而设的逐次授权要求：当前项目所需的建库、标注、保存、导出及数据库更新，通过 IDA Domain API／idalib 自主执行。使用隔离工作目录保留来源、日志和前后校验，不修改应用全局权限配置，不保存猜测类型。
- 尽量不使用 `llvm-objdump`、r2 或 Ghidra 做日常逆向。独立 loader／结构验收仍保留，但只在确需独立证据或 IDA 能力不足时使用并记录原因。

## 环境

Domain API 工具环境独立于项目运行依赖。先按本机会话提供的 skill 路径读取 `ida-domain-api` 与 `ida-domain-scripting`，不要假设 skill 总在 `~/.agents/skills/`。首次使用按 skill 初始化；以下安装目录、skill 目录与许可证均为示例，应替换成本机实际路径。

Windows PowerShell：

```powershell
$env:IDADIR = 'C:/Tools/IDA Professional 9.5'
$idaSkill = 'C:/path/to/ida-domain-scripting'
Push-Location $idaSkill
uv run python setup.py
Pop-Location
$idaPython = Join-Path $idaSkill '.venv/Scripts/python.exe'
& $idaPython -c "import idapro; import ida_domain; print('ok')"
```

Linux bash：

```bash
export IDADIR=/path/to/ida-pro-9.5
ida_skill=/path/to/ida-domain-scripting
pushd "$ida_skill"
uv run python setup.py
popd
IDA_PYTHON="$ida_skill/.venv/bin/python"
"$IDA_PYTHON" -c "import idapro; import ida_domain; print('ok')"
```

返回项目根目录后，使用这个工具解释器运行下文的 IDA 脚本；纯数据校验可使用 Windows `python`／Linux `python3`。临时目录、哈希与清单校验见[跨平台命令](platforms.md)。

若已有有效许可证位于非默认目录，打开程序报 `Cannot continue without a valid license` 时，可按 `IDA_LICENSE` 机制选择该文件，仅对当前 shell 及其子进程生效，不保存为偏好：

```powershell
$env:IDA_LICENSE = 'keyfile=C:/path/to/idapro.hexlic'
```

```bash
export IDA_LICENSE=keyfile=/path/to/idapro.hexlic
```

其他机器应使用自己的安装和有效许可证路径。不要复制、修改许可证，或自动代替开发者接受 EULA。

若报 `License not yet accepted, cannot run in batch mode`，停止并请开发者按官方方式完成 EULA；不要调用内部注册表接口写许可或 EULA 标志。旧 9.4 的内部地址与本机备份不是当前配置步骤。

## 规范流程

全部输出落在被 Git 忽略的 `tools/ida-out/`；需要提交的证据产物由最后一步复制到
`docs/generated/ida-canonical/`。

以下流程会新建数据库并写入已确认命名／原型，属于当前项目开发可自主执行的操作。若只是重导出、比较字节，使用 [9.5 字节复核报告](../reverse-engineering/ida-bytecheck-9.5.md#复现与本地证据) 的临时目录流程，避免 exporter 清空已有的 `db/`。重新生成前先检查脚本的真实输出位置，工作区、probe、缓存和临时目录均指向本轮隔离目录；历史路径不得冒充本轮运行证据。

Windows PowerShell：

```powershell
# 1. 建库并导出
& $idaPython tools/ida/ida_fd2_export.py
# 2. 审计历史命名
python tools/ida/fd2_name_audit.py --json tools/ida-out/ida-name-audit.json
# 3. 临时副本原型取证
& $idaPython tools/ida/ida_fd2_annotate.py --probe-only
# 4. 写入已确认命名和原型
& $idaPython tools/ida/ida_fd2_annotate.py
# 5. 从标注库重新导出
& $idaPython tools/ida/ida_fd2_export.py --idb tools/ida-out/db/FD2.EXE.i64
# 6. 静态字节对照
python tools/ida/verify_ida_vs_project.py --json tools/ida-out/ida-verify.json
# 7. 发布命名登记
python tools/ida/build_fd2_name_registry.py --markdown docs/reverse-engineering/function-names.md
# 8. 导出伪 C
& $idaPython tools/ida/ida_fd2_decompile.py --all --xrefs --out docs/generated/ida-canonical/fd2-ida-decomp-all.c
# 9. 最后刷新清单；校验命令见 platforms.md
python tools/ida/build_fd2_name_registry.py --checksums-only
```

Linux bash：

```bash
# 1. 从原始 FD2.EXE 建库并跑自动分析，导出 segment / fixup / 函数 / 字符串
"$IDA_PYTHON" tools/ida/ida_fd2_export.py

# 2. 用本次数据库识别的函数入口审计历史命名，产出逐行判定账本
python3 tools/ida/fd2_name_audit.py --json tools/ida-out/ida-name-audit.json

# 3. 在一次性副本上定位 Hex-Rays 失败函数，按调用点证据推导原型
"$IDA_PYTHON" tools/ida/ida_fd2_annotate.py --probe-only

# 4. 在权威库写入原型 + 语义命名 + 历史注释（不做反编译，避免存入猜测类型）
"$IDA_PYTHON" tools/ida/ida_fd2_annotate.py

# 5. 从已标注的库重新导出函数表（此时带语义名）
"$IDA_PYTHON" tools/ida/ida_fd2_export.py --idb tools/ida-out/db/FD2.EXE.i64

# 6. 与项目自研 LE parser 对照（21 项检查，不需要 IDA）
python3 tools/ida/verify_ida_vs_project.py --json tools/ida-out/ida-verify.json

# 7. 生成规范命名登记表并发布证据产物（含第 6 步的对照结果）
python3 tools/ida/build_fd2_name_registry.py \
    --markdown docs/reverse-engineering/function-names.md

# 8. 导出全量伪 C（当前开发参考基线）
"$IDA_PYTHON" tools/ida/ida_fd2_decompile.py --all --xrefs \
    --out docs/generated/ida-canonical/fd2-ida-decomp-all.c

# 9. 所有发布产物写完后再刷新校验和，避免第 8 步使清单过期
python3 tools/ida/build_fd2_name_registry.py --checksums-only
(cd docs/generated/ida-canonical && sha256sum -c SHA256SUMS)
```

第 6 步需要已重建的 raw／relocated／manifest 和第 1／5 步的完整本地 IDA 导出；版本库的精简证据不能替代字节输入。产物和验收边界见 [LE 手册](../reverse-engineering/le-validation.md#重建与静态复核)。自动识别数量可能随版本变化，不能据此覆盖历史函数或把未标注伪 C 提升为新基线。

`--checksums-only` 只为现有发布文件重建清单，不读取数据库或本地 IDA 导出，也不改写证据内容。全量伪 C、源码地址迁移报告或其他发布产物发生变化后，必须最后执行该命令。刷新校验和不等于重新验证证据语义；发布产物完整性及已有报告的结构检查由 `tools/tests/test_ida_crosscheck.py` 覆盖。

## 常用单点查询

Windows PowerShell：

```powershell
& $idaPython tools/ida/ida_fd2_decompile.py --xrefs 0x25bf4 0x1f894
& $idaPython tools/ida/ida_fd2_decompile.py --asm 0x3702f
```

Linux bash：

```bash
# 反编译指定 VA，并列出调用者/被调用者
"$IDA_PYTHON" tools/ida/ida_fd2_decompile.py --xrefs 0x25bf4 0x1f894

# 同时输出逐指令反汇编（含原始字节）
"$IDA_PYTHON" tools/ida/ida_fd2_decompile.py --asm 0x3702f
```

`ida_fd2_decompile.py` 默认打开 `tools/ida-out/db/FD2.EXE.i64` 且 `save=False`，
不会改动权威库。

## 脚本分层

| 脚本 | 职责 |
|------|------|
| `tools/ida/ida_fd2_export.py` | 建库/打开库，导出 segment、fixup、函数、命名、字符串和段字节 |
| `tools/ida/fd2_name_audit.py` | 以 IDA 函数入口为 oracle 归约历史命名档案的两套 legacy 坐标 |
| `tools/ida/ida_fd2_annotate.py` | `--probe-only` 推导原型；默认写入命名、注释与原型 |
| `tools/ida/build_fd2_name_registry.py` | 生成规范登记表、corrections 表和 `SHA256SUMS` |
| `tools/ida/ida_fd2_decompile.py` | 导出 Hex-Rays 伪 C（可选反汇编与 xref） |
| `tools/ida/verify_ida_vs_project.py` | 纯数据对照，不依赖 idalib |

## 已知陷阱

- **不要把 `0x3702f` 命名为 `__chkstk` / `alloca` / `_alloca_probe`。** IDA 对这些名字有内置 alloca 语义，会把 Watcom 栈检查序言 `push N; call` 当成动态栈分配。实测全量反编译会多出 500 处伪 `alloca()`，带寄存器幽灵参数的函数签名从 220 涨到 434。规范流程使用中性名 `watcom_stack_check`。
- **不要给 `0x3702f` 写显式原型。** `__stdcall`、`__stdcall __spoils<>` 都试过，幽灵参数同样从 220 涨到 434。留空最好。
- **不要先做全量反编译再保存 IDB。** Hex-Rays 会把猜测原型写回数据库，污染后续所有调用者签名。probe 阶段因此必须在临时副本上跑。
- **`function-names.md` 是生成文件。** 输入为 `function-names-legacy.md`，不得反向导入生成表。历史条目不改写；新语义按 AGENTS 追加登记，纠正旧结论用 `build_fd2_name_registry.py` 的 `CORRECTIONS`，再生成发布。
- IDA 对 object2 BSS `0x54000..0x556b0` 可能返回 `0xff`；用 Domain API `db.bytes.is_value_initialized_at()` 检查初始化标记，不按字节值猜 BSS。静态规范化不豁免运行时差异。
- IDA 把 segment 末尾按页对齐（object1 到 `0x4f000`、object3 到 `0x64000`），项目按 LE `vsize` 截断。引用地址一律以 `vsize` 为准。
- `ida_funcs.get_frame_size` 不存在，正确位置是 `ida_frame.get_frame_size`；`func_type_data_t` 用 `get_cc()` 而不是属性 `cc`。

## 不适用范围

- `idalib` 只做静态分析；runtime 逐字节对照走 [DOSBox 转储](dosbox-debug.md)。
- 伪 C 不等于原始源码。3 个手写汇编函数（`sub_34F38`、`__int7`、`__int7_pl3`）无法反编译，只能看反汇编。
- 本流程不产生任何 DOSBox 行为、视觉或音频对照结论。

## 2026-11-05 Windows B3 重导出

本轮通过本机 IDA Pro 9.5／Hex-Rays 9.5.0.261026 的 Domain API／idalib，在独立 C: 目录创建、标注并重导出真实数据库；没有运行 GUI `ida`／`idat`。已发布产物记录实际 `win32`、`loaders/lx.dll` 来源，原有 Linux 9.5 与历史 9.4 报告仍分别保留。本地完整日志、数据库和读集入口为 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b3-20261105/ida-equipment/run-20261105_111831_688108/` 的 `final-validation.json`、`db-type-write-audit.json` 与 `b3-contract/README.md`；不提交数据库、镜像或临时报告。

新库识别 1,307 个函数，比历史 9.4 少 `putchar_0 @0x47678` 的 21 字节函数登记；其余入口与结束地址完全相同。这是自动分析的识别差异，没有手工补造函数或豁免机器码：独立 LE／fixup／镜像检查仍为 21/21。已标注库与干净库的持久签名差异恰为历史已确认的 9 条原型，均重新核对本机全部调用点的清栈证据；新旧已发布 TSV 的持久签名无差异。`0x3702f` 保持中性名、无显式或猜测原型。Hex-Rays 临时副本与只读导出中的局部变量／参数推断不因此成为已确认 ABI，也没有保存回标注库。

`ida_fd2_apply_registry_names.py` 将已确认 registry／`CORRECTIONS` 真正同步到数据库，必须显式指定 `--idb` 与 `--report`；按上述 skill 执行器调用后，再从该库重导出 TSV 与伪 C。它只改名字和注释，先检查完整入口／边界，不设置类型。仅运行登记表生成器不能代替这一步。生成器按实际 program export 写版本与 loader，文本固定 LF，TSV 对原字符串的 CR／TAB／LF 转义；全部文件完成后最后刷新、校验 `SHA256SUMS`。

本轮详情显示复核又确认 `field_stat_bar_draw @0x18795`（原 `sub_18795`）与 `field_stat_bar_columns_draw @0x17d6f`（原 `sub_17D6F`）。两函数完整指令、调用者、signed WORD读取及 FDOTHER 原图支持新增命名；数据库实际应用两项更名后重新导出，registry 为230项，21项静态检查及12项发布／地址专项通过。持久类型仍仅原9条，未将本次推断签名保存为 ABI。详细收据由上述目录的 `current-stat-bars.txt` 指向 `stat-bars-20261105_141143_939626/`，与初次 B3 发布分开保存；只读分析和真实发布不增加 WGC 组数。

AP／DP 临时增益另有可复跑的局部原码验收。`verify_fd2_x87_boost.py` 只接受已验证的 relocated relbase 镜像，执行原始算术块与 `__CHP`，不修指令或挂模拟 handler。在原初始化控制字 `0x127f` 下，两块代码分别穷举全部 65,536 个 u16 输入，均等价于整数增量 `(15 * old + 100) / 100`；这与属性重算的独立 `1.15` 舍入规则不同。Windows 本轮已执行；Linux 保留同参数复现方式，未在本轮执行：

```powershell
uv run --no-project --with unicorn python tools/ida/verify_fd2_x87_boost.py --json C:/Temp/fd2-x87.json
```

```bash
uv run --no-project --with unicorn python tools/ida/verify_fd2_x87_boost.py --json /tmp/fd2-x87.json
```

该局部执行不增加原版玩法、完整 UI RNG、FIGANI 或声音的运行验收范围。


### B4 奖励／状态语义与1.15纠正（2026-11-05）

本阶段复用上述9.5隔离库，通过Domain真正应用11项完整函数语义名和17条注释，再真实全量导出；registry为240项、TSV／伪C为1,307函数。静态LE检查21/21、发布／地址专项13/13通过；9条持久类型保持，3702f仍无类型。新全量导出实际剩3项Hex-Rays失败，历史4项记录另保留；减少失败不表示新增猜测原型或改变机器码。`current-b4-publication.txt` 指向 `b4-publication-20261105_155414_534319/` 的命名应用、类型审计、发布校验及日志，全部产物最后刷新SHA256SUMS。此发布不增加WGC场景数。

本轮原码复核推翻旧属性重算「1.15 nearest-even」说明。`field_unit_combat_stats_recompute @0x1b750` 的指令VA `0x1b7ef..0x1b802`、`0x1b80a..0x1b81d` 使用原binary64倍率 `666666666666f23f`；原 `__CHP` 临时将RC设为向0。原CW127f下，两块分别完整执行 `-65536..65535` 的131,072输入，无指令patch或模拟handler，均等于乘积强制binary64后向0cast。12→13、100→114，负数对称向0；不能改成整数115/100。每块向量SHA256为 `55dafb59f3c3576c68a43d3f241cc735c5160a4b3eb48e362fb54315d0ea32c2`。独立MSVC C11／volatile double模型已实际编译并逐项比对通过；AP／DP直接增益的0.15加1仍保持前节独立公式。

Windows本轮已执行，Linux仅保留复现命令、未执行：

```powershell
uv run --no-project --with unicorn python tools/ida/verify_fd2_x87_stat_scale.py --json C:/Temp/fd2-stat-scale.json --write-c-model C:/Temp/fd2-stat-scale.c
```

```bash
uv run --no-project --with unicorn python tools/ida/verify_fd2_x87_stat_scale.py --json /tmp/fd2-stat-scale.json --write-c-model /tmp/fd2-stat-scale.c
cc -std=c11 -ffp-contract=off /tmp/fd2-stat-scale.c -o /tmp/fd2-stat-scale
/tmp/fd2-stat-scale /tmp/fd2-stat-scale.bin
uv run --no-project --with unicorn python tools/ida/verify_fd2_x87_stat_scale.py --c-results /tmp/fd2-stat-scale.bin --json /tmp/fd2-stat-scale-with-c.json
```

`--write-c-model`只输出独立模型源；没有传入实际编译结果时，报告不会把Python binary64对照冒称C验证。实际本机收据在 `C:/Users/Soar/AppData/Local/Temp/ida-x87-stat-scale-20261105_154802_431559/`，包括CMake／MSVC构建日志和 `actual-c-native-verified.json`。原版奖励消息、完整等待随机流和FIGANI／音画仍按各自运行证据登记。

### B5 掉落、结算与准备页取证（2026-11-05）

本阶段继续使用同一隔离IDA9.5数据库，通过Domain实际应用21项完整函数语义与22条注释，再从该库全量重导；最终registry为257项，TSV／伪C仍为1,307函数。18项主合同与随后确认的1145a、206c5、34738都有完整函数、xref及strict LE字节读集；名称应用的各次原始收据分别保留，没有用字符串替换伪C。原9条持久类型保持不变，3702f仍为中性名且无类型，最终全量导出剩余3项Hex-Rays失败。

本地入口为上述隔离目录的 `current-b5-publication.txt`，指向 `b5-publication-20261105_190021_653020/`。该目录保留 `input-manifest.json`、`actual-name-sync-complete.json`、`types-after.json`、完整Domain读集和 `publication-validation.json`；生成输入SHA为 `d9052c7d25cb2bdf8981b4d8a9669a16f906d341c42bc70846f6a5da18f0467e`。11项发布产物以LF写入，全部完成后刷新并逐项验证SHA256SUMS。Windows本轮静态LE检查21/21、发布专项14/14通过；Linux未执行。源码地址检查另按当时工作树登记，不能把它与冻结程序或WGC结果混为同一份验收。

`field_roster_detail_select @0x29620` 保留历史误挂Save及撤回为中性名的记录；当前名称取最后一条已确认语义决议，并与真实IDA TSV逐项一致。`sub_4E809` 的证据修正明确先减stage再乘31，stage1取6238d首行，完整已确认表为26行，未据相邻数据扩成29行。本次静态命名发布不表示B5自然通关、酒店四槽、掉落或下一关运行已通过；运行范围由对应原版／SDL收据另行验收。

本阶段随后将 12c60 的 portrait 查找优先级、16c57 的精确 `a1 == 1` 箭头门、190ac／1aa1d 的外部无箭头等待及累计文字、1B41D 的 stage 1 总览依据同步回同一数据库，再真实导出。此次新增更名为 0 项，更新注释为 5 项，登记数仍为 257。最终收据为 `C:/Users/Soar/AppData/Local/Temp/ida-domain-20261105_212128_586897-b5-final-publication/`；输入生成器 SHA256 为 `ebf3075ea7cf47ff030b758516c6d952dc9788ce142bb9f6860dc7645de46083`，实际伪 C SHA256 为 `e678fb673d8aeb686e103dbe58187120d62ed3ce4306f7d64a1403ab440353f0`。

本次全量重导的三个段字节、fixup、函数／名字表与上一批实际导出逐字节一致；去除注释与空行后，伪 C 函数体也一致。原 9 条持久类型未变，3702f 仍无类型。11 项发布文件全部为 LF，最后刷新并校验 SHA256SUMS；清单 SHA256 为 `e8cdfb66f9cd62b672a4dd5a6b7202b4918ead09fe5fe982bf343e7cd39e3e09`。发布回归 14/14 通过；当时工作树的源码地址检查为 605/605 规范入口。没有重跑静态 LE 基线或新增图形运行，既有 21 项静态检查与各冻结程序的实机收据仍保留各自来源。

stage 1 实际坐标差异随后触发一次有因增量复核：4e680 的 mode 2 通过预算与已访问门后记录 witness，并以 CLC 继续穿过敌格；剩余 0 也是已访问值。14b78 的长路探测则在 4df4c 清除 40/80 标记后、重建 145cd 之前执行原 mode 1，因此该次探测没有单位占格和控制区限制。两条依据通过 Domain 实际写回原函数注释并再次全量重导，未新增函数名或类型。局部向量和同一受控存档的三 END 无窗口对照已分别保留旧实现失败与修复后六终点一致；这不替代后续真实双端重验。

此增量收据为 `C:/Users/Soar/AppData/Local/Temp/ida-domain-20261105_215803_421053-b5-ai-publication/`。原始段字节、fixup 和函数／名字表仍与上一批一致，伪 C 除新增的两条依据注释外函数体不变；实际伪 C SHA256 为 `8de077dcf831d324815fd8513c5e902d7e66f81b2889e0bc8630328858eac78b`，最后生成的 SHA256SUMS 为 `5b65fb2306d29f3495269763d187d41df1839971cfbb48b3f0b493feecfc49e0`。发布回归 14/14 通过，增量后工作树地址检查为 606/606 规范入口；原 9 条类型和 3702f 保护继续通过。

随后 B5 C20 受控运行暴露 AI 动作聚焦遗漏：原版完成，SDL 在新回合 quiet 门停下；独立地形资源确定两端相机不同，不能改 oracle 代替修复。完整 Domain 指令与既有伪 C 确认物理 `154ad→154c6→154de` 的攻击者聚焦／移动／目标聚焦，法术 `15360→153c2` 的施术者／中心聚焦，以及物品普通范围 `150ba→1515b` 的使用者／中心聚焦。原 `12d7b` 通过 `12cea` 维护镜头留边，直接写 cursor 或调度层统一补焦点不能替代这些调用；物品特殊范围分支不借此泛化。

这次复用既有259项数据库的隔离副本，真实写入3条证据注释、0更名，并从该副本全量导出；原共享库未改。收据目录为 `C:/Users/Soar/AppData/Local/Temp/ida-domain-20261106_065218_232439-b5-ai-focus-publication/`，保留完整函数／raw读集、命名应用、类型审计和发布校验。三个段字节、fixup、函数／名字表及持久原型与前批相同，9条旧类型和中性无类型 `3702f` 保持；去注释后的伪 C 函数体不变，仍有3项既有反编译失败。发布14/14、当时工作树614/614地址检查通过；实际伪 C SHA256 为 `07281c6e3e5a6e933047f58158ca447e3854c3246627f91a835a711b6f1941df`，最后清单为 `b0c41fa382c28ce733b77f5bc9d2edc147ee0b66425422165efb9216565e3169`。本次只补静态调用合同，不重跑静态LE基线、不增加WGC次数，也不将待验修复记为实机通过。

C22 的实际空格光标遮挡反例随后促成 `field_actor_terrain_overlays_draw @0x129ec`（原 `sub_129EC`）首次命名：完整218字节函数、4个调用者、`map_tile_blit_visible @0x12ac6` 和免地形 helper `@0x1f183` 的读集共同确认重画范围属于角色局部，不是全地图前景层。只处理非隐藏且不免地形的角色，恢复自身和上格，行走时再按方向恢复附加格；没有 HP 或 acted 门。`12ac6` 保留已有名称，只补 viewport、attr80前景、范围 LUT 与调用者负责选格的证据。数据格式文档中该免地形 helper 的旧 dual 地址 `0x44397` 同步标明 canonical `0x1f183`，历史运行时 CS／DS 捕获不改。

本次隔离副本实际完成1项更名、2条注释，登记数为260，函数数仍为1,307；收据目录为 `C:/Users/Soar/AppData/Local/Temp/ida-domain-20261106_072421_533992-b5-actor-overlay-publication/`。三个段字节、fixup、字符串及原型导出不变；函数 TSV 只改129ec名称及默认名标志，名字表只新增该语义名。9条既有类型、129ec既有签名和中性无类型3702f均保留，伪 C 除注释及该确认名称外函数体不变，仍有3项既有反编译失败。最终发布专项14/14、当时工作树617/617地址检查通过；中间未发布完的登记数不一致失败另留日志，不合并计为通过。实际伪 C SHA256 为 `b083a64ca81eefef880282086dc60621c89c32e7bc779e3cd997e2f225960ca3`，最后清单为 `42887bb98da1420a2ffe64bf79d81e521267c4fe52f1ed2a01fde491255220b9`。本次无新 LE 基线或 WGC 执行，静态发布不表示呈现修复已通过实机。
