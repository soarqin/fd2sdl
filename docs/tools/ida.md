# IDA Pro 9.4 / idalib 操作说明

本文记录用 IDA Pro 9.4 的 `idalib`（headless Python 库）对 `original_game/FD2.EXE` 做分析、复核和反编译的已验证命令。分析结论见 [`../reverse-engineering/ida-crosscheck.md`](../reverse-engineering/ida-crosscheck.md)。

**不要用 `ida`（GUI）或 `idat` 做自动化。** 本机 GUI 会占用显示器且无法在无人值守下操作；全部流程走 `idalib`。

## 环境

IDA 安装在 `/home/soar/ida-pro-9.4`。

`idalib` Python 模块的安装（一次性）：

```bash
# 本机没有 pip / venv 模块，直接展开 wheel 到用户 site-packages
cd /tmp && rm -rf idapro_whl && mkdir idapro_whl && cd idapro_whl
unzip -o /home/soar/ida-pro-9.4/idalib/python/idapro-0.0.9-py3-none-any.whl
cp -r . /home/soar/.local/lib/python3.12/site-packages/

# 告诉 idapro 模块 IDA 的安装位置（写 ~/.idapro/ida-config.json）
python3 /home/soar/ida-pro-9.4/idalib/python/py-activate-idalib.py -d /home/soar/ida-pro-9.4
```

验证：

```bash
python3 -c "import idapro; import ida_funcs, ida_fixup; print('ok')"
```

### EULA 标志

`idalib` 在 batch 模式下会先检查注册表项 `EULA 90`，未接受时直接
`FATAL ERROR: License not yet accepted, cannot run in batch mode`。检查逻辑位于
`libidalib.so@0x42160`：

```c
if (reg_int_op("EULA 90", /*save=*/false, 0, NULL) != 0) return ok;
if (batch) fatal("License not yet accepted, cannot run in batch mode");
/* 交互确认后：*/ reg_int_op("EULA 90", /*save=*/true, 1, NULL);
```

本机已由开发者授权写入该标志（等价于 GUI 点击 “I accept”）：

```bash
python3 - <<'EOF'
import ctypes
lib = ctypes.CDLL("/home/soar/ida-pro-9.4/libida.so", mode=ctypes.RTLD_GLOBAL)
f = lib.reg_int_op; f.restype = ctypes.c_int
f.argtypes = [ctypes.c_char_p, ctypes.c_bool, ctypes.c_int, ctypes.c_char_p]
print("EULA 90 =", f(b"EULA 90", False, 0, None))   # 只读检查
EOF
```

写入前的 `~/.idapro/ida.reg` 备份为 `~/.idapro/ida.reg.bak-before-agent`。
**接受 EULA 属开发者的授权行为，自动化流程不得代为决定。**

## 规范流程

全部输出落在被 Git 忽略的 `tools/ida-out/`；需要提交的证据产物由最后一步复制到
`docs/generated/ida-canonical/`。

```bash
# 1. 从原始 FD2.EXE 建库并跑自动分析，导出 segment / fixup / 函数 / 字符串
python3 tools/ida/ida_fd2_export.py

# 2. 用 IDA 的 1308 个函数入口审计历史命名档案，产出逐行判定账本
python3 tools/ida/fd2_name_audit.py --json tools/ida-out/ida-name-audit.json

# 3. 在一次性副本上定位 Hex-Rays 失败函数，按调用点证据推导原型
python3 tools/ida/ida_fd2_annotate.py --probe-only

# 4. 在权威库写入原型 + 语义命名 + 历史注释（不做反编译，避免存入猜测类型）
python3 tools/ida/ida_fd2_annotate.py

# 5. 从已标注的库重新导出函数表（此时带语义名）
python3 tools/ida/ida_fd2_export.py --idb tools/ida-out/db/FD2.EXE.i64

# 6. 与项目自研 LE parser 对照（21 项检查，不需要 IDA）
python3 tools/ida/verify_ida_vs_project.py --json tools/ida-out/ida-verify.json

# 7. 生成规范命名登记表并发布证据产物（含第 6 步的对照结果）
python3 tools/ida/build_fd2_name_registry.py \
    --markdown docs/reverse-engineering/function-names.md

# 8. 导出全量伪 C（当前开发参考基线）
python3 tools/ida/ida_fd2_decompile.py --all --xrefs \
    --out docs/generated/ida-canonical/fd2-ida-decomp-all.c
```

第 6 步依赖 `python3 tools/rebuild_fd2_analysis.py` 已生成
`tools/fd2_le_raw.bin`、`tools/fd2_le_relocated_relbase.bin` 和
`tools/fd2_le_relocation_manifest.tsv`。

## 常用单点查询

```bash
# 反编译指定 VA，并列出调用者/被调用者
python3 tools/ida/ida_fd2_decompile.py --xrefs 0x25bf4 0x1f894

# 同时输出逐指令反汇编（含原始字节）
python3 tools/ida/ida_fd2_decompile.py --asm 0x3702f
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
- **`function-names.md` 是生成文件。** 它的输入是 `function-names-legacy.md`（只读历史档案），两者不得互为输入。修正历史结论请改 `build_fd2_name_registry.py` 的 `CORRECTIONS` 表。
- IDA 用 `0xFF` 表示未初始化字节。object2 的 `0x54000..0x556b0` 是 BSS，`get_bytes` 返回 `0xFF` 并不代表数据差异，应改用 `ida_bytes.is_loaded()` 判断边界。
- IDA 把 segment 末尾按页对齐（object1 到 `0x4f000`、object3 到 `0x64000`），项目按 LE `vsize` 截断。引用地址一律以 `vsize` 为准。
- `ida_funcs.get_frame_size` 不存在，正确位置是 `ida_frame.get_frame_size`；`func_type_data_t` 用 `get_cc()` 而不是属性 `cc`。

## 不适用范围

- `idalib` 只做静态分析。runtime 逐字节对照仍走 `docs/tools/dosbox-debug.md` 的流程。
- 伪 C 不等于原始源码。3 个手写汇编函数（`sub_34F38`、`__int7`、`__int7_pl3`）无法反编译，只能看反汇编。
- 本流程不产生任何 DOSBox 行为、视觉或音频对照结论。
