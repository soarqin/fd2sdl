# LE、文件与运行时地址映射

唯一规范函数主键是 **LE relbase VA**，不是 file offset、旧 dual 或 Ghidra 窗口地址。函数边界与语义以 [IDA 命名表](function-names.md)及已标注伪 C 为准；镜像用途、静态复核与运行时通过条件见 [LE 验收](le-validation.md)。

## 原版文件与 object

`FD2.EXE` 是 bound executable：真正拥有 LE 头的 embedded MZ 位于 file `0x25214`，`e_lfanew=0x28b8` 指向 LE `0x27acc`。LE 字段 `+0x80=0x10e00` **相对 embedded module 开头**，所以 page 1 为：

```text
0x25214 + 0x10e00 = 0x36014
```

文件 `0x10e00` 和 `LE+0x10e00` 均不是正确 page base；不能沿用旧 page extraction 或向 fixup target 再加减 header bias。

| object | relbase | virtual size | 边界注意 |
|---|---:|---:|---|
| 1，代码 | `0x10000` | `0x3ef29` | IDA segment 到 `0x4f000`，有效 VA 止于 `0x4ef29` |
| 2，数据／栈 | `0x50000` | `0x56b0` | `0x54000..0x556b0` 为 BSS，LE 初始 ESP 为 `0x56b0` |
| 3，数据 | `0x60000` | `0x34d2` | IDA segment 到 `0x64000`，有效 VA 止于 `0x634d2` |

引用以 LE virtual size 为准，不把 IDA 页尾填充算入对象。IDA 未初始化字节可能读为 `0xff`，须检查初始化标记；该静态表示不能用于豁免运行时 BSS 差异。

```text
canonical VA   = object.relbase + object offset
code0          = object1 offset
object1 file   = 0x36014 + code0
runtime linear = 当次 object runtime base + object offset
               = selector descriptor base + selector-relative offset
```

file 换算式只适用于本原版的 object1 page view；其他 object 按各自 page map 解释。LE internal fixup 的 `target_offset` 已是目标 object 内偏移，`0x28b8` 不是 target bias。例如 object1 off `0x24531` 对应 file `0x5a545`、VA `0x34531`。

## 历史坐标

历史 IDA 9.4 审计用 1,308 个函数入口归约了两套旧登记；9.5 自动识别差异见[复核报告](ida-bytecheck-9.5.md#函数识别差别不等于字节差别)。

| 历史坐标 | 到 canonical VA 的换算 |
|---|---|
| 旧错误 page view 的 dual 地址 | `VA = legacy - 0x25214` |
| 旧 Ghidra 窗口 `FUN_xxxxxxxx` | `VA = FUN_addr + 0x28b8`，按归约账本确认入口／低地址镜像，不能只按名字猜 |
| 当前真实 object1 的 code0 | `VA = code0 + 0x10000` |

两套 legacy 规则在 86 行登记上同时命中同一入口，账本为本地 `tools/ida-out/ida-name-audit.json`。旧登记和正文中的历史指令窗口必须先确认属于哪套视图；不要把所有名为 `dual` 的兼容输出都当同一坐标系。新查询直接按规范命名表定位，不再从旧别名造入口。

## 常用规范入口

| 函数 | VA | code0 |
|---|---:|---:|
| `start` | `0x3ccb4` | `0x2ccb4` |
| `main`，Watcom FLIRT | `0x25bf4` | `0x15bf4` |
| `watcom_stack_check` | `0x3702f` | `0x2702f` |
| `boot_intro_title_menu` | `0x1f894` | `0xf894` |
| `title_action_dispatch` | `0x25ebb` | `0x15ebb` |
| `animation_play` | `0x20421` | `0x10421` |
| `music_track_play` | `0x25977` | `0x15977` |
| `sfx_play` | `0x25a96` | `0x15a96` |
| `new_game_opening_play` | `0x3231b` | `0x2231b` |
| `field_rng_next` | `0x4ebe3` | `0x3ebe3` |

Watcom 的 `push imm32; call watcom_stack_check` 是 10 字节序言，不是 wrapper/body 两个函数。IDA 必须保留中性 helper 名，不能写 `__chkstk`／`alloca` 或猜测原型；原因与操作见 [IDA 陷阱](../tools/ida.md#已知陷阱)。

## DS、表与间接调用

- DS/global 引用如 `0x1a4d`、`0x3a65` 必须结合所属 object 和 selector 解释，不能直接当 file offset。
- 未重定位镜像中的 dword 不是运行时指针；先查 manifest／`dump_fd2_fixup_table.py` 的目标。表在 object3 时显式传 `--object 3`；BSS 内容另需运行时证据。
- `DS:0x27d8` 移动脚本表属于 object3，例如 target off `0x29d1` 映射 VA／raw 镜像偏移 `0x629d1`。脚本布局见[数据格式](../formats/data-formats.md)。
- 用 IDA 查询间接目标时，以 target 的规范 VA 校验调用点与边界；不能从旧 r2 清单或 Ghidra 拆分直接命名。

## 历史运行时取证注意事项

以下只用于追溯旧捕获，**不是新运行的默认地址**。每次都按 [DOSBox 手册](../tools/dosbox-debug.md)重取 selector、descriptor 和 object base。

- 2026-07-13 的 battlefield overlay 捕获使用 `CS=0x0170`、`DS=0x0178`。runtime `0x16f003` 的 24 字节签名曾对应旧窗口 `0x28217`；换为当前 code0 是 `0x3003`，本次 object1 base 为 `0x16c000`。旧记录的 delta `0x146dec` 只适用于旧窗口。
- 同次 `field_physical_attack_resolve @VA 0x1ecc7`（code0 `0xecc7`）位于 runtime `0x17acc7`。其指令直接访问 `0x1ada12/0x1ada2a/0x1ae4a8`，用于转储 terrain attack、防御和 profile critical 表。
- 另一次首次 `field_rng_next` 断点 `BP 170:1AABE3` 前，`MEMDUMPBIN 178:1AE7B8 2` 捕获到 loader 初值 `0x7a18`；不能从这个样本推定每次进入战场的 RNG 状态。
- 单位构造器表 helper 曾返回 `0x1b3af9`，但该次 DS descriptor 使表首落在 debugger linear `0x1b3ae4`，相差 `0x15`。角色基础／成长表也有同类关系；表首由 stride、unit 96 实机值和 unit 1 存档值交叉验证，不能将 `D 0:<linear>` 直接等同 `D DS:<offset>`。
