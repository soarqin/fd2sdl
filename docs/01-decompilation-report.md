# 反编译报告：炎龙骑士团 2（Flame Dragon 2: Legend of Golden Castle）

> 本文档记录对 `original_game/FD2.EXE` 及其数据文件的静态逆向分析结果。
> 工具链：Ghidra 11.3.2、radare2 5.5、capstone、Python。
> 分析日期：2026-07-09。

## 1. 游戏识别

| 项目 | 内容 |
|------|------|
| 中文名 | 炎龙骑士团 2 |
| 英文名 | Flame Dragon 2: Legend of Golden Castle |
| 类型 | 回合制战棋 RPG（SRPG） |
| 平台 | MS-DOS |
| 开发商 | 汉堂国际信息（Dynasty International Information，台湾） |
| 发行年 | 1995 |
| 玩法 | 在方格地图上移动角色、攻击/施法/待机，含队伍编成、职业、剧情推进 |

## 2. 主可执行文件 FD2.EXE

### 2.1 容器格式

`FD2.EXE` 是 **MZ stub + 内嵌 LE 可执行 + DOS4GW 扩展器** 的三段式结构：

```
0x000000  MZ 16 位引导 stub（负责加载 DOS4GW 并跳转到 LE）
0x025214  内嵌的第二个 MZ stub（DOS4GW 加载器镜像）
0x027acc  真正的 LE（Linear Executable）头，含 32 位保护模式代码
```

关键判定证据：
- `file` 报告 `MZ for MS-DOS, LE for MS-DOS, DOS4GW DOS extender (embedded)`
- 字符串含 `WATCOM C/C++32 Run-Time system` 与 `DOS4GW.EXE`
- MZ 头的 `e_lfanew=0x09b40000` 超出文件大小（DOS4GW stub 不用该字段定位 LE）

因此**必须按 32 位 x86 反汇编**，而非 16 位。`objdump` 不识别该格式；`rabin2` 默认仅识别 16 位 MZ stub。

### 2.2 LE 头（@文件偏移 0x27acc）

| 字段 | 偏移 | 值 | 说明 |
|------|------|-----|------|
| magic | +0x00 | `LE` | Linear Executable 魔数 |
| cpu_type | +0x08 | 0x2 | 80386 |
| os_type | +0x0a | 0x1 | OS/2（Watcom 借用此值表示 DOS4GW） |
| num_pages | +0x14 | 0x47 (71) | 总页数 |
| eip_object | +0x18 | 1 | 入口在 object 1 |
| eip | +0x1c | 0x2ccb4 | 入口偏移 |
| esp_object | +0x20 | 2 | 栈在 object 2 |
| esp | +0x24 | 0x56b0 | 栈偏移 |
| page_size | +0x28 | 0x1000 | 4KB/页 |
| objtab_off | +0x40 | 0xc4 | object 表相对 LE 头偏移 |
| objcnt | +0x44 | 3 | 3 个 object |
| data_pages_off | +0x78 | 0xe3e8 | 数据页相对 LE 头偏移 |

### 2.3 内存段布局（3 个 object）

| Object | relbase | vsize | flags | 页数 | 角色 |
|--------|---------|-------|-------|------|------|
| 1 | 0x10000 | 0x3ef29 | R-X | 63 | 代码 + 只读数据（含字符串表） |
| 2 | 0x50000 | 0x56b0 | R-- | 4 | 只读数据段 |
| 3 | 0x60000 | 0x34d2 | R-W | 4 | 可读写全局变量 / BSS |

- **EIP = 0x10000 + 0x2ccb4 = 0x3ccb4**（程序入口）
- **ESP = 0x60000 + 0x56b0**（栈顶，注意栈位于 object 2/3 边界）
- 页面映射表（object page map）为恒等映射：逻辑页 i → 物理页 i，数据页在文件中顺序存储于 `LE头 + 0xe3e8` 处。

### 2.4 重定位（Fixup）

LE 通过 fixup table 把代码中的"段内偏移占位符"在加载时改写为绝对虚拟地址。
- fixup page table：71+1 个 u32，位于 `LE头 + 0x22f`
- fixup record table：位于 `LE头 + 0x34f`
- 单条记录布局（7 字节）：

```
[0]   srec    = 0x07（source type = 7 = 32 位 OFFSET）
[1:3] srcoff  = 页内待打补丁的偏移
[3]   tflags  = 目标重定位类型
[4]   tobj    = 目标 object 号（1 基）
[5:7] toff    = 目标 object 内偏移
```

> 注意：`srcoff`/`toff` 的字节序与位宽存在 Watcom 私有编码，需逐字段验证。
> radare2 的 LE plugin **能正确解析段映射并标记 RELOC 位置，但未自动应用 fixup**。
> Ghidra 的 BinaryLoader 同样不应用 LE fixup。因此反编译时，代码中形如
> `cmp dword [0x3afb], 0` 的地址需手动加上所属段 relbase 还原真实虚拟地址。

### 2.5 函数与字符串

| 分析器 | 识别函数数 | 识别字符串数 |
|--------|-----------|-------------|
| Ghidra（flat image, x86:LE:32, gcc） | 547 | 205 |
| radare2（LE plugin） | 827 | — |

Ghidra 可输出可读 C 伪代码（见 `docs/decompilation-samples.md`）。
r2 可输出带交叉引用的汇编。

### 2.6 关键字符串（位于代码段末尾只读数据区，r2 虚拟地址）

| 虚拟地址 | 字符串 | 用途 |
|---------|--------|------|
| 0x4e18b | `FDTXT.DAT` | 文本资源文件名 |
| 0x4e1a1 | `FDOTHER.DAT` | 杂项资源 |
| 0x4e1ad | `FDFIELD.DAT` | 战场地图 |
| 0x4e1b8 | `FDSHAP.DAT` | 精灵图 |
| 0x4e1c1 | `DATO.DAT` | 数据表 |
| 0x4e1d? | `FDMUS.DAT` | 音乐 |
| 0x4ebdd | `BG.DAT` | 背景图 |
| 0x4ebe8 | `FIGANI.DAT` | 战斗动画 |
| 0x4e... | `TAI.DAT` / `ANI.DAT` / `TITLE.DAT` | 图块/动画/标题 |
| 0x4e1ca+ | `01 00 00 00 0d 00 00 00 08 00 00 00 ff ...` | 资源 ID 索引表 |
| 0x50164 | ` Out of Memory !!!` | 内存分配失败 |
| 0x50241 | `File not found %s!!!` | 文件缺失 |
| 0x5025c | `Out of Memory at Load %s Number:%d!!` | shape 加载 |
| 0x502f4 | `Out of memory at Get_EasyMagic` | 简易魔法 |
| 0x5032f | `Out of memory at Earth Quack` | 地震技能 |
| 0x50425 | `Sorry your password is wrong !!!` | 存档密码校验 |
| 0x501ae+ | `FDICON.B24` | 图标资源 |

游戏字符串中可见大量 AIL（Audio Interface Library）诊断函数名，说明音频子系统基于 AIL（Miles Sound System 前身）。

## 3. 数据文件总览

`original_game/` 共 47 个文件，分类如下：

| 类别 | 文件 | 说明 |
|------|------|------|
| 主程序 | `FD2.EXE` | 见上 |
| 音频驱动 | `*.MDI`（AdLib/OPL3/PAS/MPU401/MT32/SBAWE32/...） | AIL 驱动模块 |
| 数字音频 | `*.DIG`、`DIG.INI`、`MDI.INI` | 数字采样驱动 |
| 音乐样本 | `SAMPLE.BNK`、`SAMPLE.AD`、`SAMPLE.OPL` | AIL 音色库 |
| 音频配置 | `AILDRVR.LST` | AIL 驱动列表 |
| 资源包 | 11 个 `.DAT` | 见下 |
| 存档 | `FD2.SAV` | 主存档（22987 字节） |
| 临时 | `FD2.TMP` | 场景临时数据（207360 字节） |
| 图标 | `FDICON.B24` | B24 位图（624010 字节） |
| 配置 | `SETSOUND.EXE` | 音频设置程序 |

## 4. .DAT 归档格式（已破解）

全部 11 个 `.DAT` 共享同一容器格式：

```
偏移        长度      内容
0x00        6         魔数 "LLLLLL"（0x4C×6）
0x06        4×N       条目偏移表：N 个 u32，严格递增，均 < 文件大小
6+N*4       4         4 字节间隙（哨兵/版本，恒为固定值）
offs[0]     变长      条目 0 数据
offs[1]     变长      条目 1 数据
...
文件末尾              最后一条目数据
```

- **没有独立的 count 字段**：条目数 = 偏移流长度，通过"遇到非递增或越界即终止"读取。
- 偏移表后与首个条目数据之间恒有 **4 字节间隙**（`tbl_end + 4 == offs[0]`）。
- 条目大小 = `offs[i+1] - offs[i]`；末条目 = `文件大小 - offs[-1]`。

### 4.1 各 .DAT 文件清单

| 文件 | 大小 | 条目数 | 末偏移 | 子格式 | 内容 |
|------|------|--------|--------|--------|------|
| TITLE.DAT | 23377 | 7 | 0x5975 | RLE 图像 | 标题画面（320×200） |
| TAI.DAT | 94917 | 56 | 0x166d1 | 图块 | 图块集（tile atlas） |
| FDTXT.DAT | 120502 | 34 | 0x1caea | 文本+索引 | 文本资源 |
| BG.DAT | 624564 | 56 | 0x95063 | RLE 图像 | 背景图（320×100） |
| FDMUS.DAT | 80367 | 20 | 0x133b9 | IFF/EA | 音乐（FORM...XMID） |
| FDFIELD.DAT | 243169 | 99 | 0x3b52b | 地图 | 战场地图（24×24 网格） |
| FDOTHER.DAT | 3382481 | 103 | 0x3399d1 | 混合 | 杂项资源 |
| FDSHAP.DAT | 3557794 | 66 | 0x3644f2 | 精灵 | 精灵图（含调色板） |
| DATO.DAT | 1979029 | 136 | 0x1df7aa | 数据表 | 游戏数据（角色/技能/物品） |
| ANI.DAT | 2437547 | 9 | 0x24a18e | AFM | 动画（"AFM - Animation File Manager V1.00"） |
| FIGANI.DAT | 15310188 | 408 | 0xe99d69 | AFM | 战斗动画 |

### 4.2 已识别的子格式

**FDMUS.DAT（音乐）**：标准 IFF/EA IFF 8510 容器。每个条目交替 3 字节哨兵 `20 0D 0A`（" \r\n"）与 IFF 块。块头 `FORM....XMID`（XMIDI，Extended MIDI）。使用 Miles AIL 的 XMID 序列。

**ANI.DAT / FIGANI.DAT（动画）**：Amiga AFM（Animation File Manager）格式。首条目头：
`AFM - Animation File Manager Version 1.00 Copyri...`

**图像（TITLE.DAT/BG.DAT 条目）**：每个图像条目结构：
```
[0:2] width  (u16 LE)   例如 320
[2:4] height (u16 LE)   例如 200/100
[4:]  RLE 压缩像素流（8bpp 索引色）
```
解压目标 = `width × height` 字节。RLE 方案待进一步确认（0x3F 字节高频出现，疑似行程标记，但朴素方案解码后字节数不匹配，可能为每像素 2 字节或含调色板前置）。

**FDFIELD.DAT（战场地图）**：条目头 `18 00 18 00` = 24×24 网格，其后为格子数据（含 `31 00 00 00` 等单元属性）。

**FDTXT.DAT（文本）**：条目开头为 u16 子索引表，指向文本片段。

## 5. 存档格式（待深入）

### 5.1 FD2.SAV（22987 字节）

- 无魔数，前 32 字节看似随机（`cf 07 89 a8 0d d0 d3 fb ...`）
- 疑似**加密或校验和前置**：游戏有 "Sorry your password is wrong" 字符串，提示存档含密码校验
- 22987 字节 / 若干固定结构体（角色、道具、进度）需结合反编译的读写函数确认

### 5.2 FD2.TMP（207360 字节）

- 起始为 u32 偏移流（`80 07 00 00 41 09 00 00 03 0b 00 00 ...`），递增
- 144 个偏移条目，末偏移 0xe9b3
- 207360 = 0x32A00，结构上与 .DAT 类似但无 LLLLLL 魔数
- 用途：场景临时状态（当前地图、NPC 位置、事件标志等）

## 6. 音频子系统

- 基于 **AIL（Audio Interface Library）**，Miles Sound System 前身
- 支持多种声卡驱动：AdLib、OPL3、Sound Blaster（含 SB16/SBPro）、Pro Audio Spectrum、Ultrasound、MT-32、SoundScape、MPU-401
- `FDMUS.DAT` 含 15 项有效 XMIDI 序列
- `FDOTHER.DAT[31]` 是 `sfx_play @0x4acaa` 使用的 13 项数字样本 bank
- `SAMPLE.BNK` 头为 `01 00 ADLIB-`；`SAMPLE.AD/.OPL` 使用 Miles AIL `patch, bank, offset` 音色目录
- 重写不复刻 AIL 硬件驱动，但先以 SDL3 `SDL_AudioStream` 建立统一框架，再分别接入 PCM 音效与 XMIDI/OPL source。见 `docs/11-audio-plan.md`

## 7. 反编译已知限制

1. **LE fixup 未自动应用**：r2/Ghidra 都不自动应用，需手动加段 relbase 还原绝对地址
2. **函数无符号**：Watcom LE 不导出函数名（仅 entry table 可能有入口名）
3. **图像 RLE 方案**：朴素方案字节数不匹配，需对更多样本或反编译解码函数确认
4. **存档加密**：FD2.SAV 疑似有密码/校验，需定位存档读写函数

## 8. 启动画面流程（详见 docs/05-boot-flow.md）

### 8.1 架构：Watcom 协程

游戏用**协作式协程**组织逻辑（非传统状态机 switch）：

| 函数 | 作用 |
|------|------|
| `fcn.00035a1e` (0x5d4ea) | **yield**：让出 CPU（jmp 0x3c7f0） |
| `fcn.00035a23` (0x5d4ef) | 创建/切换协程（call 0x3c7f4 栈切换原语） |
| `0x35a12` | 协程恢复点（21 处汇聚，`dec [0x4178]; ret`） |
| `fcn.00036766` / `fcn.00035a31` | 事件泵：`4178++`，处理输入 `4174`/`4170`，调 `fcn.0003c972`(事件出队) |

入口 `entry0 @0x3ccb4`：`a1e`(yield) -> `a31(mode=0x302)`(事件泵) -> `a23`(切主协程)。

### 8.2 实机画面序列（DOSBox 每 4s 截图验证）

| 时刻 | 来源 | 阶段 |
|------|------|------|
| 0-34s | FDOTHER.DAT[74]->[59]/[60]->[100] | 片头（连续图片 + 淡入淡出） |
| 38s | **FDOTHER.DAT[7] sub[0] / TITLE.DAT[0] 同源图 + FDOTHER.DAT[8] 调色板** | 标题画面（含选项） |
| (空闲) | 回 FDOTHER 片头 | 循环 |

**关键修正**：
- 片头图片在 **FDOTHER.DAT**（非 TITLE.DAT）
- 标题画面实际加载点 = **FDOTHER.DAT[7] sub[0]**，图像与 TITLE.DAT[0] 同源；调色板为 **FDOTHER.DAT[8]**（经 DOSBox 实机确认，c0xdf=36,0,0）
- 启动过程调色板动态切换（淡入淡出动画），曾用 [0]/[8] 等多套

### 8.3 LE fixup 与反编译限制（修正）

Ghidra/r2 都不自动应用 LE fixup，但也不能把 fixup 记录机械写入代码页。
上一版这样做会污染启动函数，例如把 `mov ecx,0xf` 覆盖成无效的大立即数。

当前流程：
- 使用 `tools/rebuild_fd2_analysis.py` 重建未应用 fixup 的分析镜像；
- `docs/le-fixups.txt` 仅保留 fixup 说明，完整索引在 `tools/fd2_le_fixups.txt`；
- Ghidra 中只允许为分析目的 patch `__chkstk`，不 patch 业务代码；
- 启动序列已拆分为调用入口 `FUN_0001cfdc @0x44aa8`（栈检查前缀）和主体 `FUN_0001cfe6 @0x44ab2`。

当前限制：
- `FUN_0001db69` 动画播放器已接入 SDL 版，启动流程用到的 ANI.DAT opcode 已实现；
- `FUN_0001ce87` 静态切入画面的等待、渐变细节仍需逐帧核对；
- 片头脚本动画与 DOSBox 逐帧对照仍是下一步重点。
