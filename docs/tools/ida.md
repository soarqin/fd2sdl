# IDA Domain API / idalib 操作说明

本文记录通过 `idalib` 对 `original_game/FD2.EXE` 做分析、复核和反编译的已验证命令。当前本机环境为 IDA Pro 9.5，重导出字节与局部机器码执行对照见 [`../reverse-engineering/ida-bytecheck-9.5.md`](../reverse-engineering/ida-bytecheck-9.5.md)；历史 9.4 分析结论仍见 [`../reverse-engineering/ida-crosscheck.md`](../reverse-engineering/ida-crosscheck.md)。

**不要用 `ida`（GUI）或 `idat` 做自动化。** 本机 GUI 会占用显示器且无法在无人值守下操作；全部流程走 `idalib`。

## 日常分析规范

- 先读取 `ida-domain-api` skill；编写或执行脚本时同时使用 `ida-domain-scripting` skill，按其执行器流程查询数据库。
- 新脚本优先使用 IDA Domain API。仅在 Domain API 缺少所需能力时局部调用低层 IDAPython，并说明原因；现有证据导出脚本不为统一 API 而重写。
- 优先打开已有数据库，只读查询，不重复建库；需保存数据库修改时先取得开发者确认。下文的建库、标注和全量发布流程仅在需要更新证据基线时执行。
- 尽量不使用 `llvm-objdump`、r2 或 Ghidra 做日常逆向。独立 loader／结构验收仍保留，但只在确需独立证据或 IDA 能力不足时使用并记录原因。

## 环境

IDA 当前安装在 `/home/soar/ida-pro-9.5`。Domain API 工具环境独立于项目运行依赖；首次使用时按 skill 初始化：

```bash
export IDADIR=/home/soar/ida-pro-9.5
cd /home/soar/.agents/skills/ida-domain-scripting
uv run python setup.py
```

返回项目根目录后，使用这个工具解释器运行下文的 IDA 脚本；纯数据校验仍可使用系统 `python3`：

```bash
IDA_PYTHON=/home/soar/.agents/skills/ida-domain-scripting/.venv/bin/python
"$IDA_PYTHON" -c "import idapro; import ida_domain; print('ok')"
```

本机已有许可证位于非默认目录。若打开程序报 `Cannot continue without a valid license`，可以按 IDA 官方的 `IDA_LICENSE` 机制选择该文件，仅对当前进程生效，不保存为偏好：

```bash
export IDA_LICENSE=keyfile=/home/soar/.config/Hex-Rays/idapro.hexlic
```

其他机器应使用自己的安装和有效许可证路径。不要复制、修改许可证，或自动代替开发者接受 EULA。

若报 `License not yet accepted, cannot run in batch mode`，停止并请开发者按官方方式完成 EULA；不要调用内部注册表接口写许可或 EULA 标志。旧 9.4 的内部地址与本机备份不是当前配置步骤。

## 规范流程

全部输出落在被 Git 忽略的 `tools/ida-out/`；需要提交的证据产物由最后一步复制到
`docs/generated/ida-canonical/`。

以下流程会新建数据库并写入命名／原型，只在获准更新基线时执行。若只是重导出、比较字节，使用 [9.5 字节复核报告](../reverse-engineering/ida-bytecheck-9.5.md#复现与本地证据) 的临时目录流程，避免 exporter 清空已有的 `db/`。

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
