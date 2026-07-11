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

### 2.4 地形图块（TAI.DAT）✅ RLE 图像

56 个条目，每个条目均可按 §7 的通用 RLE 图像格式解码：

```
偏移   长度   字段
0x00   2      width   u16 LE，多数为 154/155
0x02   2      height  u16 LE，多数为 39/41/42
0x04   变长   RLE 像素流
```

TAI.DAT 使用 FDOTHER.DAT[0] 调色板显示。条目内容是战斗/地形相关的大尺寸图像，不是战场网格底图的直接来源。战场网格底图已确认来自 FDSHAP.DAT 的 24×24 帧包，见 §2.5/§2.8。

### 2.5 战场地图（FDFIELD.DAT）✅ 地图条目

99 个条目，每 3 个条目为一组，共 33 张战场地图：

| 组内条目 | 用途 |
|----------|------|
| `stage*3 + 0` | 地图网格 cell 表 |
| `stage*3 + 1` | stage metadata：地形包编号、阶段事件表、cell 事件表、单位模板 |
| `stage*3 + 2` | 单位/对象 placement：坐标 + unit_id（模板查找键） |

地图网格条目：

```
偏移   长度   字段
0x00   2      grid_w   u16 LE，如 24
0x02   2      grid_h   u16 LE，如 24
0x04   4*N    cells    每格 1 个 u32 LE，N = grid_w * grid_h
```

反编译 `FUN_00010580 @0x10580` 中以 `cell & 0x03ff` 读取 `terrain_id`，并用 `DAT_00003a69 + terrain_id * 4` 读取 FDSHAP 奇数条目的地形属性表。组内第 1 个条目的首字节是 FDSHAP 地形包编号；原版以 `*LAB_00003a55 * 2` 加载 FDSHAP 偶数条目的 24×24 地形帧。

stage metadata 条目（`stage*3+1`）当前确认：

```
偏移   长度       字段
0x00   1          shape_index，FDSHAP 地形包编号
0x01   1          unknown_01，待确认
0x02   1          unknown_02，待确认
0x03   16*3       turn_events，FUN_00017f5b @0x17f5b 遍历
0x33   16*2       cell_lookup，FUN_0001118c @0x1118c 按 cell_event 查询
0x53   16*3       cell_actions，FUN_000111e7 路径按 cell_event 执行动作
0x83   26*N       unit_templates，单位/对象模板记录，N=(size-0x83)/26
```

placement 条目（`stage*3+2`）当前确认：

```
偏移   长度       字段
0x00   2          count，u16 LE
0x02   6*count    records：u16 x, u16 y, u16 unit_id/template_key
```

placement 的 `unit_id` 与 26 字节模板的 offset `0x01`（actor/unit id）匹配；模板 offset `0x00` 是 side/camp。相同 id 多次出现时按模板表顺序消耗。例如 stage 32 前五个模板的 `(side,id)` 为 `(2,48),(2,66),(2,0),(0,0),(0,4)`，对应 placement `(7,5,48),(10,5,66),(8,42,0),(4,46,0),(13,47,4)`。stage 0 末尾四条 `unit_id=0` 是新游戏四名玩家的槽位 `(7,20),(10,21),(8,22),(11,23)`，其 unit id 在进入关卡前由新游戏流程写入为 `0,9,4,30`。

战场底图映射：

```
shape_index = FDFIELD[stage*3 + 1][0]
frames      = FDSHAP[shape_index * 2]
attrs       = FDSHAP[shape_index * 2 + 1]
terrain_id  = cell & 0x03ff
base_frame  = frames[terrain_id]
```

遮挡格重绘（`FUN_0001020e @0x1020e`）还会读取 `attrs[terrain_id].flags`：若 `flags & 0x08`，图块编号加 `DAT_00003a40 * 2`；若 `flags & 0x80`，绘制 `frames[terrain_id + 1]` 作为建筑、树冠等遮挡层。

样本 FDFIELD.DAT[0]：24×24 网格，条目大小为 `4 + 24*24*4 = 2308` 字节，`shape_index=0`，对应 FDSHAP.DAT[0] 的 288 帧与 FDSHAP.DAT[1] 的 300 条属性记录。

**验证**：FDFIELD.DAT 的 33 个地图网格条目均满足 `size == 4 + grid_w * grid_h * 4`；metadata 均满足 `size >= 0x83 && (size-0x83)%26==0`；placement 均满足 `size == 2 + count*6`；各 stage 的 `terrain_id` 均落在对应 FDSHAP 帧数和属性表范围内。`tools/analyze_fdfield_events.py --stage 0` 可转储事件表与 placement；`--map-preview-once [stage]` 可显示指定 stage 的战场底图。

`new_game_opening_play @0x2fa63` 的完整开场依次使用三个 stage：

| 段落 | FDFIELD stage | FDTXT entry / fragment | 主要动作 |
|------|---------------|------------------------|----------|
| 王宫与郊外 | 32 | `[33]` fragment `0..5` | 镜头 `(3,34)`，脚本 `0x63..0x69`，中途卷动到 `(0,43)` |
| 遇袭与同行 | 31 | `[32]` fragment `0..9` | 镜头 `(5,42)` / `(4,41)`，脚本 `0x5a..0x62`，隐藏 actor 2 |
| 第一关战前 | 0 | `[1]` fragment `0..2` | 镜头 `(4,12)` / `(0,0)` / `(0,15)`，脚本 `0,1,2,5`，两组 actor 登场特效 |

stage 32 网格为 `18×51`，`shape_index=32`；`(0,43)` 只是郊外段镜头，不是整个开场的固定视窗。stage 31 为 `20×50`，stage 0 为 `24×24`。先前将 stage 32 与 `FDTXT[1] fragment 0` 混合播放属于错误实现。

`field_view_render_from_cache @0x1cca0` 先清空 320×200 缓冲区，再只写 VGA `(4,4)` 起的 312×192 战场内区，因此四边固定保留 4 px 的调色板 index 0 黑边。SDL 镜头必须按 13×8 格内区钳制，并将镜头左上格放在 `(4,4)`；不能为消除黑边而向上、向左额外采样相邻地图格。

### 2.6 文本格式（FDTXT.DAT）✅ token 片段

34 个条目。每个条目以 u16 偏移表开头；第一个偏移值即偏移表长度，因此：

```
fragment_count = first_offset / 2
```

条目结构：

```
偏移   长度   字段
0x00   2*N    offsets[N]，u16 LE，指向后续片段
?      变长   fragments，u16 token 流
```

片段内容不是可直接按 Big5/GBK 解码的字节串，而是 u16 token 流。非负 token 是 FDOTHER.DAT[4] 的 16×16 字形编号，`FUN_0004c4c2 @0x4c4c2` 按 `token * 0x20` 取 32 字节位图绘制。

已确认控制码：

| signed u16 | 含义 |
|------------|------|
| `-1` (`0xffff`) | 片段结束 |
| `-2` (`0xfffe`) | 换行 |
| `-3` (`0xfffd`) | 分页/等待后继续 |
| `-4` (`0xfffc`) | 动态嵌入：递归渲染 `DAT_00003ad9` 指向的文本片段 |
| `-5` (`0xfffb`) | 动态嵌入：递归渲染 `DAT_00003add` 指向的文本片段 |
| `-6` (`0xfffa`) | 动态嵌入：将 `PTR_FUN_00003ae1` 指向的数值转十进制并逐位绘制 |
| `-17` (`0xffef`) | 顶部对话框，下一 token 为 actor 文本编号；按单位记录 offset `0x08` 查找角色，再从 offset `0x07` 读取 DATO 立绘编号 |
| `-18` (`0xffee`) | 底部对话框，下一 token 为 actor 文本编号；查找与立绘读取规则同 `-17` |
| `-19` (`0xffed`) | 顶部对话框，下一 token 为战场单位索引；原版读取 `DAT_00003a45 + idx*0x50 + 7` 作为 DATO 立绘编号 |
| `-20` (`0xffec`) | 底部对话框，下一 token 为战场单位索引；原版读取 `DAT_00003a45 + idx*0x50 + 7` 作为 DATO 立绘编号 |

**验证**：FDTXT.DAT 全部 34 个条目的偏移表严格递增，共解析 1016 个片段。完整开场的直接调用只到达 `[33]` fragment `0..5`、`[32]` fragment `0..9` 和 `[1]` fragment `0..2`；`[1]` 的后续 fragment 属于第一关回合或事件分支，不在新游戏初始过场中播放。

`dialog_box_open @0x13cf4` 先将 FDOTHER.DAT[5] tile 0 的 24×24 空心框从说话角色所在格移动到对话框原点，再按 `4×2 → 8×3 → 12×4 → 16×5 → 19×5` 个内部块绘制边框，形成伪缩放弹出效果。移动空心框及五级展开之间调用的是 `delay_ms(10)`，不是 1 个约 54.9 ms 的 BIOS tick；对话结束或切换说话角色时，`dialog_box_close @0x1428b` 也以 10 ms 步间延时反向收起。

从角色格弹框前，`dialog_box_open` 会自动调用 `field_focus_move_to @0x10432`。该函数将焦点逐格移到说话角色；焦点继续向外移动且已达到左/右 `2/11`、上/下 `2/6` 的视窗阈值时，同步卷动镜头。因此，「说话角色在画面外时先移动视角」属于对话框系统行为，不是 FDTXT 控制码，也不要求过场脚本另发 `field_camera_pan_to` 指令。`-17/-18` 通过 `field_visible_actor_find_by_text_id @0x103a8` 查找未隐藏角色；只匹配到隐藏角色时，仍从该角色记录 offset `0x07` 读取立绘，但不移动镜头，也不从旧角色坐标弹框。`-19/-20` 直接使用 actor 索引，不经过该查找函数。

`bios_tick_delay @0x151f1` 读取 BIOS 计时器物理地址 `0x046c`，按调用参数等待计时器 tick；1 tick 约为 54.9 ms。`text_dialog_render_tokens @0x136cc` 每绘制一个普通字形后调用 `bios_tick_delay(1)`，因此 SDL 版逐字延时取整为 55 ms。

stage 0 两组敌方 actor 由 `FUN_000300e1 @0x300e1` 登场。该函数使用 FDOTHER.DAT[9]：`LMI1`、12 帧、每帧为 `u16 width/u16 height + LMI RLE`，0 色透明。原版先保留无新 actor 的战场底图，再在每个新 actor 格连续叠加 12 帧特效，最后进入移动脚本 1 或 2。

### 2.7 角色立绘（DATO.DAT）✅ 立绘帧包

136 个条目。每个条目是同一角色/单位的多帧 80×80 立绘包，供 `FUN_000136cc @0x136cc` 的 FDTXT 立绘控制码加载。

条目结构：

```
偏移   长度   字段
0x00   4*N    frame_offsets[N]，u32 LE；N = first_offset / 4
?      变长   frame_data，每帧为 u16 width/u16 height + LMI RLE
```

立绘帧 RLE 对应 `FUN_0004c347 @0x4c347` / `FUN_0004c379 @0x4c379`：

- `0x00..0xc0`：直接输出 1 个像素值；
- `0xc1..0xff`：run，长度 `byte - 0xc0`，下一字节为重复像素值。

**验证**：DATO.DAT 136 个条目的首帧均可按上述 RLE 解码；首批条目为 80×80、4 帧。

### 2.8 精灵/地形帧（FDSHAP.DAT）✅ 帧包 + 地形表

66 个条目。偶数条目（0,2,...,64）为 24×24 帧包；战场地图用这些帧包作为地形底图。奇数条目为 4 字节地形属性记录表，供 `FUN_00010580 @0x10580` 与 `FUN_0001020e @0x1020e` 查询。

偶数条目包头：

```
偏移   长度   字段
0x00   2      frame_w      u16 LE，通常 24
0x02   2      frame_h      u16 LE，通常 24
0x04   2      frame_count  u16 LE，如 0x0120=288
0x06   4*N    frame_offsets[N]，相对条目起点的 u32 LE 偏移
?      变长   frame_data，逐帧 RLE 数据
```

每帧数据不再带宽高头，宽高取自包头；RLE 控制字节与 §7 / `FUN_0004c0d5 @0x4c0d5` 相同。
例如 FDSHAP.DAT[0] 头为 `18 00 18 00 20 01 86 04 ...`：24×24，288 帧，首帧偏移 `0x486 = 6 + 0x120*4`。

奇数条目地形表：

```
偏移   长度   字段
0x00   1      flags        bit7=遮挡层可重绘；bit3=动态图块相位偏移
0x01   1      attr1        地形属性 1，用途待细分
0x02   1      attr2        地形属性 2，用途待细分
0x03   1      attr3        地形属性 3，当前样本多为 0
```

例：FDSHAP.DAT[1] 长 1200 字节，即 300 条属性记录，与 FDSHAP.DAT[0] 的 288 帧配套。FDFIELD stage 0 使用 `shape_index=0`，`terrain_id = cell & 0x03ff` 直接映射到 FDSHAP.DAT[0] 同号帧。

**验证**：FDSHAP.DAT 全部偶数条目共 8256 帧均可按上述头部和 §7 RLE 精确解码，且每帧数据刚好消耗完；奇数条目长度均为 4 的倍数，可按 4 字节属性记录解析。

## 3. 存档格式 ⚠️

### 3.1 FD2.SAV（22987 字节）✅ 初步解密与单位槽位

FD2.SAV 无魔数，整文件使用对称 XOR 流加/解密。`FUN_0004b670 @0x4b670` 从 `u16 state=0x00a5` 开始，对每个字节先执行：

```
state = rol16(state + 0x9014, 3)
byte ^= state & 0xff
```

解密后当前确认：

```
偏移      长度      字段
0x312b    0xa28*N   save slots；FUN_00027313 @0x27313 以 DAT_00003c57 选择 slot
slot+0    0xa00     单位表，最多 32 条 0x50 字节单位记录
slot+0xa00 1        stage_id，当前本地初始 slot 为 1
slot+0xa01 1        unit_count，当前本地初始 slot 为 5
slot+0xa02 4        DAT_00003bf3，语义待细分
slot+0xa06 4        若干进度/开关字段
```

单位记录当前确认字段：

```
偏移   长度   字段
0x00   1      map_x
0x01   1      map_y
0x02   1      map_sprite_cache_class，按 `class*12` 取 FD2.TMP；随 stage 重建，不是稳定职业号
0x03   1      direction，FUN_0000b168 @0xb168 移动绘制时使用
0x05   1      flags，bit0 为不可见/失效
0x06   1      side/camp，0 敌方，2 我方等
0x07   1      DATO 立绘编号
0x08   1      名称/文本编号；当前样本与 0x07 相同
0x40   2      hp 当前值
0x42   2      hp 最大值
0x44   2      mp 当前值
0x46   2      mp 最大值
```

`--prologue-preview` 不再从 FD2.SAV 推断 sprite。新游戏流程直接建立 `DAT_00003a45`，稳定标识是 record offset `0x07` 的 unit id；offset `0x02` 只是 FD2.TMP cache slot。

**验证**：解密后 slot 0 前 5 个单位记录 cache class 为 `0,1,2,3,5`，立绘编号为 `0,9,4,30,1`。该 class 仅描述存档当时的 FD2.TMP 布局，不能用于 stage 32/31。

### 3.2 战场过场移动脚本（EXE 指针表）✅ 解释器格式

`field_movement_script_play @0x10db2` 通过 `field_movement_script_ptr @0x4c290` 从 `DAT_000027d8 + script_id*4` 取脚本指针。脚本字节流格式已由反编译确认：

```
u8 group_count
repeat group_count:
  u8 step_or_mode
  u8 actor_count
  repeat actor_count:
    u8 actor_id
    u8 direction
```

若 `step_or_mode & 0x80 == 0`，解释器播放移动：每个 step 内将 actor 的 direction 写入单位记录 offset `0x03`，frame_phase(offset `0x04`) 依次设为 `1..6` 并刷新画面，随后按 direction 更新一格坐标：`0 => y+1`、`1 => x-1`、`2 => y-1`、`3 => x+1`。若最高位置位，`step_or_mode & 0x7f` 表示原地朝向/等待帧数；值为 0 时走特殊重绘路径，用于遮挡关系变化。

新游戏开场另有直接角色移动调用。`field_actor_move_up_follow_camera @0x108cd` 在角色接近视窗上缘时，不会等角色走完整格后再把镜头跳动 24 px；它在同一格的 6 个步态相位中同步调整角色屏幕偏移和镜头偏移，每相位 4 px，第 6 相位后才提交角色与镜头的格坐标。权威 `tools/fd2_le_code0.bin` 的 code0 `0x08cd` 与 Ghidra relbase `0x108cd` 对应，未发现该处地址映射偏差。

`field_cutscene_setup_units_camera @0x20b0e` 是过场 actor 初始摆放 helper。其 11 个参数依次为 `x_array, y_array, dir_or_array, first_actor, last_actor, special_actor, special_x, special_y, special_dir, camera_x, camera_y`；`dir_or_array < 4` 时作为固定 direction，否则作为 direction 数组指针。它写入 `DAT_00003a45` 的 `x/y/direction`，并同步 `DAT_00003aa9/03aad` 与 `DAT_00003ab1/03ab5` 镜头坐标。

`field_camera_pan_to @0x10d25` 先逐格完成 X 轴，再逐格完成 Y 轴，每格刷新一次并等待 vsync；确认键不终止镜头。开场在脚本 `0x64`、`0x69`、`0x62` 前将 `DAT_00003afb=1`：移动脚本每个 4 px 相位递增一次 DAC 暗度，边移动边渐出；新场景初始化随后调用 `palette_fade_in_light @0x1cc6d` 渐亮。

`DAT_000027d8` 指针表位于 object3；LE fixup 的 target 映射到 raw object3 数据时需加 `0x28b8` 偏移。按此映射可完整解码开场脚本 `0,1,2,5,0x5a..0x69`，并与 `new_game_opening_play @0x2fa63` 的直接调用顺序一致。

`DS:0x1d71` 是关卡过场分发表，当前按 30 项处理；相邻 `DS:0x1de9` 是进入战场前的另一张分发表，不应混入 stage dispatch。`tools/analyze_fd2_stage_code.py` 默认使用 `tools/fd2_le_code0.bin`，因为 fixup 的 `target_offset` 是 object1/code0 偏移；查看 dual/relbase 视图时需使用 `relbase_linear`。

第一关后续事件分支仍包含 fragment `3..5` 和 `field_camera_pan_to(5,0x11)` 等调用；它们不属于初始开场。`DAT_00003aa9/DAT_00003aad` 表示战场视窗左上角格坐标。`field_actor_is_hidden @0x31fdc` 读取 actor flags bit0。

### 3.3 FDICON.B24 与 FD2.TMP ✅ 地图角色帧源和运行期缓存

`FDICON.B24` 是完整地图角色帧源，不是普通 B24 图标。文件结构与 FDSHAP 偶数条目相同：

```
偏移   长度       字段
0x00   2          width = 24
0x02   2          height = 24
0x04   2          frame_count = 1680
0x06   4*1680     frame_offsets
?      变长       24×24 RLE frames
```

共 `140×12` 帧。稳定帧号为：

```
frame_idx = unit_id * 12 + direction * 3 + frame_phase
```

`FUN_0000e761 @0xe761` 按 actor record offset `0x07` 的 `unit_id`，从 FDICON.B24 复制 12 帧到 FD2.TMP，并把运行期 cache slot 写入 record offset `0x02`。`fd2tmp_map_sprite_load @0x2c0a3` 再将固定大小 `0x32a00` 的 FD2.TMP 读入 `DAT_00003a61`。`map_actor_blit_24x24 @0xb168` 对缓存使用：

```
frame_idx = cache_class * 12 + direction * 3 + frame_phase
frame_ptr = DAT_00003a61 + *(u32 *)(DAT_00003a61 + frame_idx * 4)
```

FD2.TMP 会在 stage 初始化时原地重写，不能作为跨 stage 的稳定资源。原版实机捕获到的 cache 映射为：

| stage | cache class → FDICON unit id |
|-------|------------------------------|
| 32 | `0→48, 1→66, 2→0, 3→4, 4→68, 5→69` |
| 31 | `0→0, 1→4, 2→75, 3→30, 4→9` |
| 0 | `0→0, 1→9, 2→4, 3→30, 4→96` |

三份缓存中的压缩帧与 FDICON.B24 对应 unit id 的帧字节一致。SDL 版因此直接按 unit id 读取 FDICON.B24，不依赖 DOSBox 最后一次运行遗留的 FD2.TMP。

移动阶段使用 actor record offset `0x04` 的 `1..6` 相位，每帧沿方向偏移 4 px，第 6 帧跨满 24 px。`field_animation_phase_update @0x100c5` 约每 275 ms 推进共享 idle 相位；`map_actor_blit_24x24 @0xb168` 对 offset `0x04 == 0` 的所有静止 actor 使用该全局相位，并将 phase 3 映射回 phase 1，因此某个角色移动时，其他角色仍以 `0,1,2,1` 序列播放 idle 动画。

## 4. 音频相关文件

| 文件 | 用途 |
|------|------|
| `*.MDI` | AIL 音乐驱动（AdLib/OPL3/SB/MPU401/...） |
| `*.DIG` | AIL 数字采样驱动 |
| `SAMPLE.BNK` | AIL 音色库（头 `01 00 ADLIB-`） |
| `SAMPLE.AD` / `SAMPLE.OPL` | 音色附加数据（二者内容相同） |
| `DIG.INI` / `MDI.INI` | 驱动配置 |
| `AILDRVR.LST` | 可用驱动列表 |
| `FDICON.B24` | 140 组地图角色 24×24 帧，详见 §3.3 |

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
