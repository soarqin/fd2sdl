# IDA Pro 9.4 独立复核报告

本文记录用 IDA Pro 9.4 的 `idalib`（headless）对 `original_game/FD2.EXE` 重做一次完整分析，并与既有 radare2 / Ghidra 结果逐项对照的结果。

复核目的是回答一个具体问题：**LE/LX fixup 的可靠性**。结论是既有 fixup 模型完全正确；真正需要修正的是函数地址坐标系、`__chkstk` wrapper/body 拆分和 Watcom ABI 假设。

## 输入身份

| 项 | 值 |
|----|----|
| `original_game/FD2.EXE` SHA-256 | `bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd` |
| IDA | Pro 9.4，`idalib` Python 模块 `idapro-0.0.9` |
| loader | IDA 自带 `loaders/lx.so`，自动识别为 `Linear Executable (LE)` |
| processor | `metapc`，32 位 |
| compiler | IDA 自动识别为 `Watcom C++`（`inf_get_cc_id()==3`） |
| FLIRT | `Watcom v9-*1.5 32bit DOS runtime`、`Watcom v9-*1.5 32bit common runtime` |
| Hex-Rays | 9.4.0.260714 |

IDA 是与项目自研 parser、radare2、Ghidra 相互独立的第四个实现。它自己解析 bound MZ / LE 头、object table、page map 和 fixup 记录，不读取本项目生成的任何镜像或清单。

## 一、fixup 与内存布局：既有模型完全正确

### object 布局

| IDA segment | 范围 | 大小 | 类别 | 对应 LE object |
|-------------|------|------|------|----------------|
| `cseg01` | `0x10000..0x4f000` | `0x3f000`（63 页） | CODE，32 位 | object1，relbase `0x10000`，vsize `0x3ef29` |
| `dseg02` | `0x50000..0x556b0` | `0x56b0` | BSS/STACK，32 位 | object2，relbase `0x50000`，vsize `0x56b0` |
| `dseg03` | `0x60000..0x64000` | `0x4000`（4 页） | DATA，32 位 | object3，relbase `0x60000`，vsize `0x34d2` |

- 三个 object 的 relbase 与项目模型一致。
- IDA 把段尾按页对齐（`0x4f000` / `0x64000`），项目按 LE `vsize` 截断（`0x4ef29` / `0x634d2`）。差异只是尾部填充零，字节内容一致。
- IDA 的 `is_loaded()` 报告 `dseg02` 只有 `0x50000..0x54000` 有初始化字节，`0x54000..0x556b0` 未初始化 —— **与 `ghidra-reconstruction.md` 的 `OBJ2.init` / `OBJ2.bss` 切分逐字节一致**。
- IDA 另外给出 LE 头里的初始栈信息：`SS` 指向 object2，`ESP = 0x56b0`。即 object2 尾部同时是运行时栈，这解释了 IDA 把该段类别标成 `STACK`。
- IDA 在 object2 / object3 内**没有创建任何函数**，与项目验收条件一致。

### relocation

| 项 | 项目 parser | IDA `lx.so` |
|----|-------------|-------------|
| 唯一 relocation source 数 | 7,948 | 7,948 |
| source 集合 | — | 与项目**完全相同**（对称差为 0） |
| 每个 source 的 target linear 地址 | — | 与项目 manifest **全部 7,948 条一致** |
| fixup 类型 | source type `0x07`（32-bit offset） | 全部 `FIXUP_OFF32`，宽度 4 |
| target 种类 | 全部 internal | 全部带段选择子的 internal 引用 |

项目 parser 另外报告 7,959 条**记录**（11 条是同 source 同 target 的重复记录）。IDA 按地址去重，因此看到 7,948 个 —— 两者不矛盾。

### 镜像字节

用 IDA 加载后的段字节与项目 oracle 对照：

- `cseg01[0..0x3ef29)` 与 `tools/fd2_le_relocated_relbase.bin[0x10000..0x4ef29)` **零字节差异**；
- `dseg03[0..0x34d2)` 与 `[0x60000..0x634d2)` **零字节差异**；
- `dseg02[0..0x4000)` 一致；`0x4000..0x56b0`（即 BSS）在 IDA 中显示为未初始化，`get_bytes` 返回 `0xFF` 填充，属 IDA 表示约定，不是数据差异；
- 把 IDA 镜像按 manifest 的 `raw_u32` 反写回 relocation source 后，与 `tools/fd2_le_raw.bin` 在 BSS 之外**零字节差异**。

**结论：`tools/rebuild_fd2_analysis.py` 与 `tools/validate_le_fixups.py` 的 LE 解析、page 映射、relocation source/target 与镜像输出全部通过第四个独立实现的复核。fixup 不存在可靠性问题，此前的疑虑可以关闭。**

未变更的限制：本次复核是静态 loader 层面的。runtime 逐字节对照仍由 `tools/verify_le_runtime_dump.py` 配合 `dosbox-debug` 转储完成，且 selector / object base / relocation delta 仍须每次运行重新取得。

## 二、entry 与 `main`

| 语义 | VA | OFF | 依据 |
|------|----|-----|------|
| LE transfer entry | `0x3ccb4` | `0x2ccb4` | IDA 唯一 entry point，命名 `start`，被 FLIRT 标为 runtime |
| `main` | `0x25bf4` | `0x15bf4` | IDA 由 Watcom runtime FLIRT 识别；唯一调用者 `0x4609b`（`__CMain` 链） |

`main` 是本次复核的新结论。它的机器码解释了此前分散在多处的观察：

- 依次以 `res_load` 载入 `FDOTHER.DAT` 记录 31/1/2/3/4/5/6 与 `FDTXT.DAT` 记录 0；
- `word_53A8D = 19; int386(0x10, …)` 设置 VGA mode 13h；
- 读物理地址 `0x46c` 的 BIOS 计时器，再 `rand() % 256` 次调用 `field_rng_next @0x4ebe3` 做 RNG 预热；
- 主循环：`music_track_play(18, 0)` → `title_action_dispatch @0x25ebb`，再按返回值进入战场循环；
- 战场循环里按 `dword_53C03`（stage 序号）查两张分派表 `funcs_25E23 @VA 0x51de9`（object2 `DS:0x1de9`）与 `funcs_25E3A @VA 0x51d71`（object2 `DS:0x1d71`），并用 `byte_51E63[stage]`（`DS:0x1e63`）选择该 stage 的音乐 track。

历史档案曾把「片头+标题主体」记在 `0x44ab2`，其证据文字却引用 `code0 0x15db1`。前者归约到 `0x1f894`，后者落在 `main` 体内 `0x25db1`。两处描述的是两个不同函数，此前被混为一条记录。

## 三、历史函数地址混用两套 legacy 坐标系

`function-names.md` 的地址列此前已知「混有旧 Ghidra 窗口、dual、code0 和少量 canonical VA」。用 IDA 的 1,308 个函数入口做 oracle 后，混用规律可以精确写出：

| 规则 | 换算 | 命中 exact 入口的行数 |
|------|------|----------------------|
| 旧 dual 镜像地址 | `canonical_VA = legacy − 0x25214` | 55 |
| 旧 Ghidra 窗口地址（`FUN_xxxxxxxx` 名字里的数字） | `canonical_VA = FUN_addr + 0x28b8` | 94 |
| 已修正过的 `code0` | `canonical_VA = code0 + 0x10000` | 21 |
| 已是 canonical VA | 原值 | 14 |

`0x25214` 的来源：旧流程把 LE 头字段 `+0x80 = 0x10e00` 当成**文件**偏移，而真实 page 1 在 `0x25214 + 0x10e00 = 0x36014`（`0x25214` 是 embedded MZ 的文件位置）。`0x28b8` 是该 embedded MZ 的 `e_lfanew`。

两套规则在 86 行上**同时命中且给出同一个 VA**，互为独立佐证；只有 2 行冲突（`field_selection_overlay_draw`、`field_selection_sprite_blit`），已标记待人工复核。全部 210 行现在都能归约到 object1 内的 IDA 函数：

- 181 行落在 exact 函数入口；
- 3 行 exact 但多候选（`exact-conflict`）；
- 26 行落在函数入口 `+0xa`，即 `__chkstk` body（见下节）；
- 0 行无法归约。

逐行账本：`tools/ida-out/ida-name-audit.json`（由 `tools/ida/fd2_name_audit.py` 生成）。

## 四、`__chkstk` wrapper/body 拆分是旧流程的产物

Watcom 的栈检查序言固定为：

```asm
push  imm32          ; 5 字节，帧大小
call  watcom_stack_check ; 5 字节
<body>               ; 入口 +0xa
```

`watcom_stack_check @VA 0x3702f`（历史文档称 `__chkstk`）的机器码是：

```asm
xchg  eax, [esp+4]
call  watcom_stack_check_probe
mov   eax, [esp+4]
retn  4
```

即**保留 EAX、自行清 4 字节参数、不做动态栈分配**。519 个直接调用点。

IDA 把 `push N; call watcom_stack_check; <body>` 归为**同一个函数**，序言后的寄存器压栈被正确识别为 callee-saved 保存。因此：

- 旧 Ghidra 流程的 541 组 wrapper/body 双函数、以及由此派生的 `*_entry` / `*_core` / `*_runtime` 命名，是分析工具的产物，不是原程序结构；
- 历史档案中 25 条「body」登记行的地址正好等于对应函数入口 `+0xa`，全部归一化到入口；
- `docs/generated/ghidra-decomp-all.c` 需要 patch `__chkstk` 字节才能生成，本次 IDA 流程**不修改任何机器码字节**。

## 五、Watcom ABI：确认为栈传参（caller-cleans），不是寄存器约定

`ghidra-reconstruction.md` 此前把「Open Watcom 默认寄存器约定 `EAX, EDX, EBX, ECX`」列为未验证假设。IDA 分析给出直接证据：

- object1 共 6,757 个 near call 调用点；
- 其中 **4,660 个调用点在 `call` 之后紧跟 `add esp, N`**，即由调用方清栈；
- 821 个被调用者中 756 个的栈参数个数在全部调用点上一致；
- 参数个数分布（按主导值）：0 参 233、1 参 177、2 参 130、3 参 112、4 参 49，最多 11 参。

**结论：FD2 的游戏代码用栈传参、调用方清栈（`__cdecl`），不是 Watcom 的寄存器默认约定。** IDA 自动分析也按 `__cdecl` 处理，日志明确写 `Assuming __cdecl calling convention by default`。

例外区域：`VA >= 0x37000` 的 Watcom runtime 与 Miles AIL 库中确有寄存器约定和手写汇编（例如 `IF@DSQRT`、`__int7`），Hex-Rays 对这部分给出寄存器形参属预期现象。

## 六、函数边界与 Ghidra 基线对照

| 项 | 值 |
|----|----|
| IDA 函数总数（全部在 object1） | 1,308 |
| Ghidra canonical 基线函数数 | 858 |
| 两者相同的函数入口 | 836 |
| 仅 Ghidra 有 | 22（其中 19 个落在某个 IDA 函数体内部，3 个 IDA 未覆盖：`0x16f0b`、`0x212b9`、`0x21bd0`） |
| 仅 IDA 有 | 472 |
| IDA 函数最大结束地址 | `0x4ef29`，正好是 object1 vsize 末尾 |
| FLIRT 识别的 runtime 函数 | 170 |

IDA 额外识别出 Watcom runtime 与 Miles AIL 的具体符号（`malloc`、`fopen`、`fseek`、`memset`、`memmove`、`printf`、`int386`、`rand`、`sqrt`、`sin`、`cos` 等），这直接修正了几条历史命名（见第八节）。

Watcom 会让多个函数共用同一段 `add esp,N; pop …; retn` 尾码，被调用者用 `jmp` 跳进别的函数体。object1 共有 308 处跨函数 `jmp`、157 个不同目标。伪 C 里表现为 `JUMPOUT(0x…)`，属真实代码结构，不是反编译错误。

## 七、Hex-Rays 反编译成功率

| 阶段 | 成功 | 失败 |
|------|------|------|
| 自动分析后直接反编译 | 1,282 | 26 |
| 按调用点证据补 9 个显式原型后 | **1,305** | **3** |

26 个失败全部是 `call analysis failed`：被调用者的尾码在别的函数里，Hex-Rays 无法推断其栈效果，于是猜出形如 `void __fastcall(__int32 eax0, int edx0, int ebx0, int ecx0, int, …)` 的伪原型并自我否决。

修复方式是**调用点证据**，不是猜测：`call` 之后的 `add esp, N` 直接给出该被调用者的栈参数个数，且只有当全部调用点一致时才采用。全过程不修改任何机器码字节。

剩余 3 个失败：

| 函数 | 失败原因 | 性质 |
|------|---------|------|
| `sub_34F38 @0x34f38` | `positive sp value has been found` | 连续压 7 个参数后跳转的尾调用形式 |
| `__int7_pl3 @0x4a0c4` | `call analysis failed` | Miles AIL 中断处理，手写汇编 |
| `__int7 @0x4a0e8` | `call analysis failed` | 同上 |

两个中断处理最终调用 `sub_4CD98 @0x4cd98`：入口即 `push edi/esi/edx/ecx/ebx` 保存，参数经 `EAX` 指针传入，是纯手写汇编例程。这三处只能看反汇编，不能期待伪 C。

### 一个必须避免的坑

**不要把 `0x3702f` 命名为 `__chkstk` / `alloca` / `_alloca_probe`。** IDA 对这些名字有内置的 alloca 语义，会把 `push N; call` 当成动态栈分配。实测数据：

| `0x3702f` 的名字 | 伪 `alloca()` 调用 | 带寄存器幽灵参数的签名 |
|------------------|-------------------|----------------------|
| 不命名 | 2 | 220 |
| `__chkstk` | 500 | 434 |
| `watcom_stack_check` | 2 | 220 |

因此规范流程使用中性名 `watcom_stack_check`。同理，给 `0x3702f` 写显式原型（`__stdcall`、`__spoils<>` 都试过）也会把幽灵参数从 220 涨到 434，故不写类型。

## 八、需要修正的历史结论

| 位置 | 旧结论 | 复核后 | 依据 |
|------|--------|--------|------|
| `res_load` | `0x463ce` | VA `0x111ba` / OFF `0x11ba` | `FUN_0000e902 + 0x28b8`；机器码为 `fopen` → `fseek(idx*4+6)` → `fread`，39 个调用点；`main` 直接用它加载 FDOTHER/FDTXT |
| `vga_clear`「清屏(memset 0xa0000)」 | 自定义清屏函数 `0x5cb24` | VA `0x37910`，是 Watcom runtime 的通用 `memset` | FLIRT `Watcom v9-*1.5 32bit DOS runtime` 命名；调用点传入任意目标，不只显存 |
| `mem_alloc` / `mem_free` / `file_open` / `file_read` / `file_seek` / `file_close` | 「libc wrapper」 | 分别就是 `malloc @0x3706e`、`free @0x3776e`、`fopen @0x37324`、`fread @0x373ca`、`fseek @0x37940`、`fclose @0x3759c` | 同上，FLIRT 直接给出 Watcom runtime 符号名 |
| `music_track_play` 证据文字 | 「从 FDMUS handle `DS:0x1a79` 加载 entry」 | `DS:0x1a79` 是字符串 `"FDMUS.DAT"`（IDA 命名 `aFdmusDat`），传给 `res_load` 的是文件名而非 handle | IDA 字符串识别 + `res_load` 调用形态 |
| `boot_intro_title_entry` / `boot_intro_title` / `title_action_menu` | 三条独立登记 | 同一个函数 VA `0x1f894`（`0x44aa8 − 0x25214 = 0x1f894`，`0x44ab2 − 0x25214 = 0x1f89e` 即其 `__chkstk` body） | 规范名取 `boot_intro_title_menu` |
| `boot_intro_title` 证据里的 `code0 0x15db1` | 与上一行同函数 | 属 `main @0x25bf4` 体内 `0x25db1` | 该描述（`music_track_play(18,0)` 后进入 `title_action_dispatch`）实为 `main` 的主循环 |
| `field_actor_hide` / `field_ai_behavior7_hide_actor` | 两条独立登记 | 同一函数 VA `0x32975`，只写 `actor[idx*0x50 + 5] = 1`；规范名 `field_actor_hide` | 两条历史描述在 IDA 反编译上完全吻合，互为佐证 |
| 22 组 `*_entry` / `*_core` / `*_runtime` | 视为独立函数 | 均为同一函数的 `__chkstk` wrapper 与 body | 见第四节 |
| 关卡分派表 | 只记 object2 `DS:0x1d71` | 另有 `DS:0x1de9`；`DS:0x1e63` 为每 stage 音乐 track 表 | `main` 的 `funcs_25E3A[]` / `funcs_25E23[]` / `byte_51E63[]` |
| Watcom ABI | 「默认寄存器约定，未验证」 | 游戏代码为栈传参、调用方清栈 | 见第五节 |
| `docs/generated/ghidra-decomp-all.c` | 历史基线 | 由 patch 过 `__chkstk` 的镜像生成，函数边界含 wrapper/body 伪拆分，**不再作为开发参考** | 改用 `docs/generated/ida-canonical/fd2-ida-decomp-all.c` |

`function-names.md` 已按本节结论重写为以 canonical VA 为主键的登记表；机器可读版本见 `docs/generated/ida-canonical/fd2-ida-function-registry.json`。

## 九、仍然不能宣称的结论

- 伪 C 不等于 1995 年的原始 C 源码；参数、返回类型、局部变量与结构体仍需按需复核。
- 1,308 个函数不等于「全部函数已发现」。间接调用、跳表目标和纯数据驱动的入口仍可能缺失。
- 3 个 Hex-Rays 失败函数只能按反汇编理解。
- object1 内 200 个函数签名仍带寄存器形参（其中 171 个在 `VA >= 0x37000` 的 runtime/AIL 区），未逐个确认。
- 本次复核没有增加任何 DOSBox 实机行为、视觉或音频对照。需要进入指定关卡、触发剧情或截取特殊场景的验证仍须开发者手工操作。

## 复现命令

```bash
python3 tools/ida/ida_fd2_export.py
python3 tools/ida/fd2_name_audit.py --json tools/ida-out/ida-name-audit.json
python3 tools/ida/ida_fd2_annotate.py --probe-only
python3 tools/ida/ida_fd2_annotate.py
python3 tools/ida/ida_fd2_export.py --idb tools/ida-out/db/FD2.EXE.i64
python3 tools/ida/verify_ida_vs_project.py --json tools/ida-out/ida-verify.json
python3 tools/ida/build_fd2_name_registry.py \
    --markdown docs/reverse-engineering/function-names.md
python3 tools/ida/ida_fd2_decompile.py --all --xrefs \
    --out docs/generated/ida-canonical/fd2-ida-decomp-all.c
```

操作前置与环境说明见 `docs/tools/ida.md`。
