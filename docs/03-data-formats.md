# 数据文件格式规范

> 本文档详细记录原版游戏数据文件的二进制格式，供 SDL3 重写时读取。
> 已验证格式用 ✅ 标注，待确认用 ⚠️ 标注。

## 1. .DAT 归档容器格式 ✅

全部 11 个 `.DAT` 文件共享此容器格式。

### 1.1 结构

```
偏移        长度      字段              说明
────────────────────────────────────────────────────────
0x00        6         magic             固定 "LLLLLL"（0x4C × 6）
0x06        4         offset[0]         首条目数据偏移（u32 LE）
0x0A        4         offset[1]         第二条目偏移
...
0x06+4*N    4         gap               4 字节间隙（哨兵，值固定）
offset[0]   变长      entry[0] data     条目 0 数据
offset[1]   变长      entry[1] data     条目 1 数据
...
文件末尾             entry[N-1] data   末条目数据
```

### 1.2 读取算法

```python
def parse_dat(data):
    assert data[:6] == b"LLLLLL"
    fsize = len(data)
    offsets = []
    i = 0
    while True:
        pos = 6 + i * 4
        if pos + 4 > fsize:
            break
        o = u32_le(data[pos:pos+4])
        if o >= fsize:              # 越界 → 结束
            break
        if offsets and o <= offsets[-1]:  # 非递增 → 结束
            break
        offsets.append(o)
        i += 1
    # 条目大小
    entries = []
    for idx, o in enumerate(offsets):
        nxt = offsets[idx+1] if idx+1 < len(offsets) else fsize
        entries.append(data[o:nxt])
    return offsets, entries
```

### 1.3 关键性质

- **无独立 count 字段**：条目数通过"越界/非递增即终止"判定。
  （曾被误判的字段实为 offset[0]。）
- **4 字节间隙恒存在**：`6 + N*4 + 4 == offsets[0]`。
- 偏移为**绝对文件偏移**，严格递增。
- 末条目大小 = `文件大小 - offsets[-1]`。

### 1.4 各文件清单

| 文件 | 大小 | 条目数 | offset[0] | offset[-1] |
|------|------|--------|-----------|-----------|
| TITLE.DAT | 23377 | 7 | 0x26 | 0x5975 |
| TAI.DAT | 94917 | 56 | 0xea | 0x166d1 |
| FDTXT.DAT | 120502 | 34 | 0x92 | 0x1caea |
| BG.DAT | 624564 | 56 | 0xea | 0x95063 |
| FDMUS.DAT | 80367 | 20 | 0x5a | 0x133b9 |
| FDFIELD.DAT | 243169 | 99 | 0x196 | 0x3b52b |
| FDOTHER.DAT | 3382481 | 103 | 0x1a6 | 0x3399d1 |
| FDSHAP.DAT | 3557794 | 66 | 0x112 | 0x3644f2 |
| DATO.DAT | 1979029 | 136 | 0x22a | 0x1df7aa |
| ANI.DAT | 2437547 | 9 | 0x2e | 0x24a18e |
| FIGANI.DAT | 15310188 | 408 | 0x66a | 0xe99d69 |

## 2. 子格式规范

### 2.1 图像格式（TITLE.DAT / BG.DAT）✅ 头部 + RLE + 调色板

每个图像条目：

```
偏移   长度   字段
0x00   2      width   (u16 LE)   如 320
0x02   2      height  (u16 LE)   如 200 / 100
0x04   变长   pixels  (RLE 压缩，8bpp 索引色）
```

- 解压目标大小 = `width × height` 字节
- **调色板（已逆向精确确认）**：256 色 VGA 6-bit（0-63），768 字节，存于 FDOTHER.DAT。
  - 资源加载器：`FUN_0000e902(&DAT_00001a4d, old_ptr, index)`（@0xe902）
    `&DAT_00001a4d` 是 FDOTHER.DAT 的句柄槽，index 为条目号
  - 全局调色板指针：`DAT_00003a65`（@0x3a65），指向当前调色板
  - 渲染时按 `(v<<2)|(v>>4)` 扩展 6-bit 为 8-bit
  - **标题画面的调色板选择条件**（源自 `FUN_0001cfe6` @0x1cfe6，片头/标题序列函数）：
    片头循环结束后，写死执行：
    ```c
    title_bg = FUN_0000e902(&DAT_00001a4d, old, 7);          // 加载标题背景图(索引7)
    DAT_00003a65 = FUN_0000e902(&DAT_00001a4d, DAT_00003a65, 8);  // 加载调色板(索引8)
    FUN_00035058(0xa0000, 0, &DAT_0000fa00);   // 清屏
    FUN_0000f488(0, 0xff, 0);                   // 应用调色板
    FUN_0001db69(1, 0xf, 1);                    // 标题前脚本动画（animation_play）
    FUN_00013fce(0xa0000, 0x140, title_bg, 0);  // blit 标题图
    ```
    即**标题画面写死用 FDOTHER.DAT[8] 作调色板**，非配置驱动
  - 片头过程中调色板动态切换（均在 `FUN_0001cfe6` 内）：
    | 条件 | 调色板 index |
    |------|------------|
    | 片头起始 | 0x4c (76) |
    | iVar6==0x6e | 8 |
    | iVar6==0xd2 | 7 |
    | iVar6==0x14a | 5 |
    | iVar6==0x19 | 0 |
    | 字幕切换 | 0x66 (102) |
    | local_20==0xb | 0x65 (101) |
    | 片头->标题 | 8 |
  - DOSBox 实机验证：标题画面调色板与 FDOTHER.DAT[8] 完全一致
- **RLE 方案（已逆向确认，源自 FUN_0004e176）**：
  - 控制字节 `c`：
    - `bit7=0`（c < 0x80）：**RUN**，count = `(c & 0x3f) + 1`，下一字节为像素值，重复 count 次
    - `bit7=1, bit6=1`（c ≥ 0xC0）：**RUN（隔行平面）**，count = `(c & 0x3f) + 1`，下一字节为像素值
    - `bit7=1, bit6=0`（0x80-0xBF）：**LITERAL**，count = `(c & 0x3f) + 1`，复制后续 count 个像素字节
  - 原版 FUN_0004e176 还区分"隔行写入"（param_2+2，用于双平面/精灵掩码），
    普通背景图按连续写入即可
  - 验证：TITLE.DAT 全 7 条目、BG.DAT 主图 44/56 条目
    精确匹配（`consumed == 数据长度 && 输出 == width*height`）
- 样本：FDOTHER.DAT[7] sub[0] / TITLE.DAT[0] = 320×200（标题画面，已渲染验证），BG.DAT[0] = 320×100（背景）

### 2.2 音乐格式（FDMUS.DAT）✅

IFF/EA IFF 8510 容器，Miles AIL 的 XMIDI。

```
条目结构（交替）：
  3 字节哨兵: 20 0D 0A  (" \r\n")
  IFF 块: FORM <size:u32> XDIRINFO <data...>
          FORM <size:u32> XMID <data...>
```

- 块类型：`XDIRINFO`（目录信息）、`XMID`（XMIDI 序列）
- 重写时可用 SDL3_mixer 或将 XMID 转为标准 MIDI/OGG

### 2.3 动画格式（ANI.DAT / FIGANI.DAT）✅

Amiga AFM（Animation File Manager）V1.00。

```
条目头: "AFM - Animation File Manager Version 1.00 Copyri..."
0xa5  u16  frame_count
0xa7  u16  width       # 启动动画为 320
0xa9  u16  height      # 启动动画为 200
0xab  u16  screen_size # 启动动画为 0xfa00
随后每帧：
  u16 data_len
  u16 command_count
  u16 reserved0
  u16 reserved1
  u8  commands[data_len]
```

- `ANI.DAT`：9 个启动/场景动画，`FUN_0001db69 @0x1db69` 直接读取该文件。
- `FIGANI.DAT`：408 个战斗角色动作，含 3 字节空帧（`04 00 04 00 ...`）作分隔。
- 启动流程当前用到的 AFM opcode：

| opcode | 反编译 handler | 语义 |
|--------|----------------|------|
| 1 | `FUN_0003459f` | 原始 0x300 字节调色板 |
| 2 | `FUN_000345ad` | AFM 调色板 RLE |
| 4 | `FUN_00034628` | 单色填充 64000 字节显存 |
| 5 | `FUN_00034650` | 原始 64000 字节显存 |
| 6 | `FUN_0003466c` | AFM 显存 RLE：`0xc0-0xff` 为 run，低 6 位为重复次数 |
| 7 | `FUN_000346b1` | 稀疏单点写入：`(offset:u16, value:u8)` |
| 8 | `FUN_000346ca` | 稀疏等值 run：`(offset:u16, len:u8, value:u8)` |
| 9 | `FUN_000346f4` | 稀疏 literal：`(offset:u16, len:u8, bytes...)` |

### 2.4 战场地图（FDFIELD.DAT）✅ 头部

```
偏移   长度   字段
0x00   2      grid_w   (u16)  如 24
0x02   2      grid_h   (u16)  如 24
0x04   变长   cells    每格含属性（地形、单位、事件等）
```

样本 FDFIELD.DAT[0]：24×24 网格，单元含 `31 00 00 00`（地形类型 0x31）等。

### 2.5 文本格式（FDTXT.DAT）✅ 头部

```
偏移   长度   字段
0x00   变长   sub-index   u16 偏移表，指向后续文本片段
?      变长   text        文本片段（编码待确认，Big5/GBK）
```

### 2.6 数据表（DATO.DAT）⚠️

136 个条目，大小均匀（约 14KB/条）。疑似：
- 角色属性表、技能表、物品表、敌人表
- 需结合反编译确认每条记录的结构体布局

### 2.7 精灵图（FDSHAP.DAT）⚠️ 头部

66 个条目。首条目头 `18 00 18 00 20 01 86 04 ...`，疑似 24×24 精灵 + 调色板（`20 01`=0x120 色彩数?）。大小差异极大（400B ~ 185KB），含不同尺寸的角色/物品图。

## 3. 存档格式 ⚠️

### 3.1 FD2.SAV（22987 字节）

- 无魔数
- 前 32 字节看似随机，疑似加密或校验和前置
- 游戏含 "Sorry your password is wrong" → 存档有密码校验
- 22987 字节推测结构：全局进度 + N 个角色存档（固定大小结构体）
- 需反编译存档读写函数确认

### 3.2 FD2.TMP（207360 字节）

- 起始为 u32 偏移流（无 LLLLLL 魔数），递增
- 144 个偏移条目，末偏移 0xe9b3
- 用途：场景临时状态（当前地图、NPC、事件标志）
- 结构上类似 .DAT 但无魔数

## 4. 音频相关文件

| 文件 | 用途 |
|------|------|
| `*.MDI` | AIL 音乐驱动（AdLib/OPL3/SB/MPU401/...） |
| `*.DIG` | AIL 数字采样驱动 |
| `SAMPLE.BNK` | AIL 音色库（头 `01 00 ADLIB-`） |
| `SAMPLE.AD` / `SAMPLE.OPL` | 音色附加数据（二者内容相同） |
| `DIG.INI` / `MDI.INI` | 驱动配置 |
| `AILDRVR.LST` | 可用驱动列表 |
| `FDICON.B24` | B24 位图（图标，624010 字节） |

重写时建议：用 SDL3 audio 播放转换后的音频，不强制复刻 AIL 驱动层。

## 6. 嵌套 .DAT 归档

FDOTHER.DAT 中部分条目本身也是 .DAT 归档（6 字节 "LLLLLL" 魔数开头）：

| 条目 | 用途 | 子条目数 | 关键子条目 |
|------|------|---------|-----------|
| [7] | 标题画面 | 7 | sub[0]=320×200标题图, sub[1-6]=菜单文字(61×7等) |
| [0x4d] | 片头底图 | 4 | sub[0]=片头背景(非标准RLE) |

**验证**：FDOTHER[7] 用 `fd2_archive_open_mem()` 打开后，sub[0] 成功解码为 320×200，
用调色板[8]渲染得到 159 色标题图，主色 rgb(36,0,0) 与 DOSBox 实机一致。

## 7. RLE 解码方案修正（FUN_0004c0d5）

旧版 image.c 把 SKIP(0xC0-0xFF) 当成 RUN（多读一个字节），导致数据错位撕裂。

**正确方案**（源自反编译 FUN_0004c0d5 @0x4c0d5，逐行解码）：

| 控制字节范围 | 类型 | 行为 | 宽度消耗 | 额外数据 |
|------------|------|------|---------|---------|
| 0x00-0x3F | RUN | val 重复 count 次 | count | 1字节(val) |
| 0x40-0x7F | RUN stride-2 | val 隔字节写 | count*2 | 1字节(val) |
| 0x80-0xBF | LITERAL | 逐字节复制 | count | count字节 |
| 0xC0-0xFF | SKIP | 透明跳过(dst保持0) | count | 无 |

count = (c & 0x3F) + 1

**验证**：FDOTHER[0x45-0x49] 片头5帧，逐行解码精确消耗100%输入字节，
输出恰好 320×147=47040 像素，147/147 行完整。
