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
  - 资源加载器：`FUN_0000e902(&DAT_00001a4d, old_ptr, index)`（@0x463ce）
    `&DAT_00001a4d` 是 FDOTHER.DAT 的句柄槽，index 为条目号
  - 全局调色板指针：`DAT_00003a65`（@0x3a65），指向当前调色板
  - 渲染时按 `(v<<2)|(v>>4)` 扩展 6-bit 为 8-bit
  - **标题画面的调色板选择条件**（源自 `FUN_0001cfe6` @0x44ab2，片头/标题序列函数）：
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

- 外层 `.DAT` 共 20 项：15 项有效 `FORM/XMID`，5 项为 `20 0d 0a` 哨兵。
- 块类型：`XDIRINFO`（目录信息）、`XMID`（XMIDI 序列）。
- stage 曲目表位于 `FD2.EXE` file `0x76e73`／corrected code0 `0x66073`，共两组连续 30 字节：
  - primary：`1,0,1,1,19,19,19,19,3,19,19,19,3,4,19,19,19,19,3,19,4,19,19,3,19,3,4,19,3,19`
  - alternate：`4,19,19,8,12,12,1,12,6,12,12,1,6,4,12,1,12,1,6,12,8,12,12,6,12,6,8,12,6,1`
- 机器码 `0x35687` 读取 `DS:0x1e63[stage]`，`0x3f78e` 读取 `DS:0x1e81[stage]`；地址差 30 与表长一致。stage 0 因而确认为 primary track 1、alternate track 4。
- 重写不再预设「转 OGG」或「直接交给 SDL3_mixer」；先以 libADLMIDI 的 XMI、AIL bank 和 Nuked OPL3 能力做保真原型，再按许可证与 DOSBox 对照结果决定正式后端。见 `docs/11-audio-plan.md`。

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

- `ANI.DAT`：9 个启动/场景动画，`FUN_0001db69 @0x45635` 直接读取该文件。
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

反编译 `FUN_00010580 @0x3804c` 中以 `cell & 0x03ff` 读取 `terrain_id`，并用 `DAT_00003a69 + terrain_id * 4` 读取 FDSHAP 奇数条目的地形属性表。组内第 1 个条目的首字节是 FDSHAP 地形包编号；原版以 `*LAB_00003a55 * 2` 加载 FDSHAP 偶数条目的 24×24 地形帧。

stage metadata 条目（`stage*3+1`）当前确认：

```
偏移   长度       字段
0x00   1          shape_index，FDSHAP 地形包编号
0x01   1          unknown_01，待确认
0x02   1          unknown_02，待确认
0x03   16*3       turn_events：u8 turn, u8 action, u8 phase
0x33   16*2       cell_lookup：u8 event_code, u8 match_arg
0x53   16*3       cell_actions：u8 mode, u16 param；具体语义仍待按调用路径细分
0x83   26*N       unit_templates，单位/对象模板记录，N=(size-0x83)/26
```

placement 条目（`stage*3+2`）当前确认：

```
偏移   长度       字段
0x00   2          count，u16 LE
0x02   6*count    records：u16 x, u16 y, u16 unit_id/template_key
```

placement 的 `unit_id` 与 26 字节模板的 offset `0x01`（actor/unit ID）匹配；模板 offset `0x00` 是 side/camp。相同 ID 多次出现时按模板表顺序消耗。例如 stage 32 前五个模板的 `(side,id)` 为 `(2,48),(2,66),(2,0),(0,0),(0,4)`，对应 placement `(7,5,48),(10,5,66),(8,42,0),(4,46,0),(13,47,4)`。stage 0 末尾四条 `unit_id=0` 是新游戏四名玩家的槽位 `(7,20),(10,21),(8,22),(11,23)`，其 unit ID 在进入关卡前由新游戏流程写入为 `0,9,4,30`。

SDL 运行时使用 `fd2_field_unit` 保留原版 `DAT_00003a45` 的 0x50 字节记录布局，单位数组也保持 0x50 字节步长；步态帧和模板来源序号属于 SDL 派生状态，存放在旁路数组中。DOSBox 运行时捕获的 `field_unit_stage_template_append @0x35e6e` 证明，26 字节模板不是直接复制到 record `0x06..0x1f`：placement 提供 `x/y`，模板 byte `0/1/4` 提供 side、unit ID、level，byte `5..12` 被重排为 8 个装备槽，byte `13..16` 写入 record `0x1a..0x1d`，其他已确认 AI 字段写入 `0x31..0x36/0x3d`。模板 byte `0x15` 只由 group loader 用于选择加入批次，不存入 0x50 字节记录；SDL 通过 `source_template_indices` 旁路数组保留来源和 group 查询。

`field_turn_event_check @0x3fa27` 将 turn event 的 byte 0 与当前回合 `DAT_00003bef` 比较，将 byte 2 与当前 phase 比较，再用 byte 1 对 `DS:0x1b91` 执行间接调用。该表已确认为战场 action 表：FD2 object1 fixup 目标换算到统一 code0 使用 `target-0x28b8+0x27acc`，前四项对应 `0x49745/0x4981f/0x49887/0x498e1` 的独立剧情入口。DOSBox 运行时表首项为 `0x190531`，其机器码与 `code0 0x49745` 一致。所有已检查调用点都按 cdecl 传入一个 32 位 actor index（回合事件固定传 0），调用后由调用者执行 `add esp,4`；handler 使用普通 `ret`，返回值未使用。

原版阶段顺序由 `field_turn_cycle_run @0x3f51f` 确认为 side 2（玩家）→ side 1 → side 0（敌方）→ 回合数加一。stage 0 与 stage 31 的四条有效记录均为 `(turn,action,phase)=(3,0,1),(4,1,0),(5,2,0),(6,3,1)`。SDL stage 0 先事务化提交 action `0..3` 的持久状态：action 0 依次加入 group 3/7 并执行 script 7/8，action 1 加入 group 4 并执行 script 3，action 2 加入 group 5 并执行 script 4，action 3 加入 group 6 并执行 script 6；任何分组或脚本校验失败都会回滚单位表和镜头。交互演出再从 notice 保存的提交前单位数、镜头和焦点建立临时 session，逐格重放镜头与移动：action 0 先加入 group 3、再平移镜头并播放 FDTXT fragment 11，随后加入 group 7、播放 fragment 3；action 1/2/3 播放 fragment 4/5/6；只有原版调用 `field_actor_group_arrival_effect @0x57bad` 的 action 1/2 使用 FDOTHER[9] 的 12 帧 LMI1，action 0/3 直接显示新增 group。演出终态必须逐记录匹配已提交单位表，成功后清除 `presentation_deferred`；未知 action 继续安全失败。

`field_cell_event_lookup @0x38c58` 读取 32 位 map cell 的 byte 2 低 5 位作为 1-based lookup ID；ID 为 0、超出 16 项、地形 `attr.flags & 0x60` 非零、event code 为 `0xff`，或 `match_arg` 不一致时均不触发。单格移动提交后使用 `match_arg=0`，单位动作完成路径使用 `match_arg=1`。

stage 0 的初始单位顺序为 4 名玩家，再加入模板 group 1 和 group 2，各 4 名敌军。`field_actor_group_append @0x35d6c` 按模板 byte `0x15` 选择 group，并为每个模板调用 `field_unit_stage_template_append @0x35e6e`；后者是已确认的通用 stage 单位构造器，会填写身份、装备、基础属性、HP/MP 和移动字段，再调用战斗派生重算。此前 `apply_stage0_baseline()` 只是捕获样本，现已由 `src/field_unit_base.c` 的通用表驱动派生替代。

战场底图映射：

```
shape_index = FDFIELD[stage*3 + 1][0]
frames      = FDSHAP[shape_index * 2]
attrs       = FDSHAP[shape_index * 2 + 1]
terrain_id  = cell & 0x03ff
base_frame  = frames[terrain_id]
```

标准基础层渲染 `field_view_render_tiles @0x3710c` 会按 `attrs[terrain_id].flags` 选择动画帧：`0x08` 优先并加 `DAT_00003a40*2`，否则 `0x10` 加 `DAT_00003c0b/2`，否则 `0x04` 加 `DAT_00003a40`。`DAT_00003a40` 每个 BIOS tick 翻转，`DAT_00003c0b` 约每 5 tick 推进一步。遮挡格重绘（`map_tile_blit_visible @0x37cda`）在 `flags & 0x80` 时绘制后一帧；若同时有 `0x08`，先加 `DAT_00003a40*2` 再取后一帧，形成与基础层配套的建筑、树冠等遮挡层。

### 战场选择框与移动范围

选择框资源已确认，不在 FDICON 或 FDSHAP 中：

- `FDOTHER.DAT[1]` 是 `24×24`、20 帧的 shape sheet，格式为 `u16 width, u16 height, u16 count, u32 offsets[count]`，帧流使用 24×24 shape RLE。`field_selection_sprite_blit @0x3790b` 按 `DAT_00003a4d + 6 + frame*4` 取帧，并以透明 SKIP 规则画到 `(cell-camera)*24`。普通战场模式 1 使用 frame 0；模式 2 使用 frame 1；模式 3..5 用 frame 1..18 组合十字、两格和三格范围轮廓。
- `FDOTHER.DAT[3]` 是 `LMI1` 容器，但其 23 帧不是图像：每帧严格为 256 字节，作为 palette index translation LUT。基础 tile 使用 `shape_blit_palette_lut_24x24 @0x7322a`：RUN/LITERAL 执行 `dst=lut[src]`，SKIP 也执行 `dst=lut[dst]`，因此整格现有像素都会变色；遮挡帧改用 `shape_blit_palette_lut_transparent_24x24 @0x732b6`，其 SKIP 才保留目标像素，避免映射下方单位。
- `field_view_render_tiles @0x3710c` 检查路径 node offset `0x07`。值为 `0xff` 时正常绘制；可达格则使用上述 LUT 重绘完整 24×24 地形。20 相位 LUT 索引表位于 `DS:0x1a97`，原始字节为 `08 01 00 00 00 04 02 05 01 00 00 00 04 02 05 01 00 00 00 04`，每 3 个 BIOS tick 前进一步。
- `field_stage_transition_effect @0x4982c` 不使用上述 phase 表，而是直接读取同一 FDOTHER[3] 容器的 LUT frame `9→1`。`field_transition_lut_mask @0x4725a` 对 312×192 field buffer 组合应用整圆、下半圆和上半中央矩形 mask；每帧增加 radius，帧间隔 5 ms，结束后等待 500 ms 并渐暗。
- `field_earthquake_effect @0x4673b` 调用 `field_view_render_scaled @0x44776` 构造 3 帧。`DS:0x2096..0x20b6` 确认的 `(x_offset,y_offset,step)` 为 `(72,-128,128)`、`(128,128,131)`、`(0,0,128)`；播放顺序是 `0,1,2,1` 循环 60 帧。
- 绘制顺序是基础地形及范围 LUT → FDOTHER[1] 选择框 → 单位 → 经过同一 LUT 的 FDSHAP 遮挡层。因此选择框可被单位和树冠遮挡；原版没有 SDL 旧实现中的稀疏范围内框或路径十字。

本地 DOSBox 第一关实机截图与 SDL 截图确认，frame 0 位于所选单位下方，移动范围表现为地形本身的循环色调变化；SDL 实现位于 `src/field_visual.[ch]` 和 `src/field_game.c`。

### 战场格子与单位信息面板

`field_cell_info_panel_draw_entry @0x3ff07` / `field_cell_info_panel_draw @0x3ff11` 在标准战场刷新中绘制常驻面板：

- FDOTHER[5] frame 130 是 `69×34` 底板，frame 131/132 是正负号。frame 31..40 是满 HP 数字 `0..9`；受伤时改用 frame 42..51 的另一组数字颜色。
- 面板默认位于逻辑坐标 `(5,161)`。光标进入视窗左下区域时移到 `(246,161)`；进入右下区域时移回左侧，避免遮挡光标。
- 左侧按 `map_cell_info_at` 返回的原始 terrain ID 绘制 24×24 FDSHAP 基础帧，不应用地图层动画相位。光标格存在可见单位时，再叠加该单位 `cache_class*12+idle_phase` 的 FDICON 帧；面板 sprite 不随单位朝向或移动步态变化。下方显示三位当前 HP，并以最大 HP 选择数字颜色组。
- 右侧两行显示 terrain 的 A/D 修正。stage 0 DOSBox 逐格截图确认 movement cost class `0/1/2` 分别显示 `(+5,0)/(-5,+10)/(+5,0)`；当前只登记这三个第一关实际类别，不外推其他 stage。

SDL 实现位于 `src/field_info.[ch]`，并保持面板在地形、选择框、单位及遮挡层之后绘制。

### 全屏角色详情页

`field_unit_detail_open @0x3d01f` 从战场选中单位后建立详情页；`field_unit_detail_draw @0x3d103` 和 `field_unit_detail_stats_draw @0x3d1de` 负责主体：

- `field_unit_detail_draw @0x3d103` 先调用 `dialog_box_draw_tiles @0x3baca`，以 `(x=5,y=7,w=5,h=5)` 使用 FDOTHER[5] frame 1..17 拼出 86×86 立绘边框；DATO 当前 unit ID 的首帧随后绘制到 `(8,10)`。边框不是单色线框，也不是 frame 137。
- FDOTHER[5] frame 20 是 `223×86` 属性框，位置 `(92,7)`；frame 21 是 `310×99` 装备框，位置 `(5,94)`。
- 姓名、种族、职业分别使用 FDTXT[0] fragment `text_id+1`、`record[0x1f]+0x8c`、`record[0x20]+0x96`。
- 数值字段为：LV=`0x21`、EX=`0x3c`、DX=`0x3e`、MV=`0x3b`、HIT/AP/EV/DP=`0x4c/0x48/0x4e/0x4a`，HP/MP 使用 `0x40..0x47`。HP/MP 条宽为 `current==0 ? 0 : current*101/max+1`；当前值等于最大值时使用 frame 31..40，否则当前值使用 frame 42..51，最大值固定使用 frame 31..40。三位溢出分别使用同组的 frame 41/52，两位溢出使用 frame 93。
- record `0x22/0x23/0x24` 非零时，AP/DP/HIT+EV 改用 FDOTHER[5] frame 119..128 的状态数字颜色；`0x25..0x27` 非零时在 `(194,68)` 起绘制 frame 55..57 状态图标。
- `field_unit_detail_equipment_draw @0x3d6d4` 遍历 `0x0a..0x19` 的 8 个槽；flag bit `0x80` 表示不显示，bit `0x40` 表示已装备。名称使用 FDTXT[0] fragment `item_id+0xb5`。
- `field_item_record_get @0x73ad0` 证明完整物品表是 `DS:0x02ad + item_id*0x17` 的 23 字节记录。连续表位于 corrected code0 `0x684c1` / `FD2.EXE` file offset `0x792c1`，共 215 条有效记录；SDL 将其内置于 `src/field_item.c`，详情页和战斗属性重算共用。
- `field_equipped_item_slot_find @0x40a51` 在 weapon 模式按槽位顺序查找 flag `0x40` 且 `item_id < 0x80` 的首件装备；`field_unit_item_id_at @0x40936` 返回该槽的 item id。玩家普通攻击读取对应 23 字节 item record 的 `+0x0b/+0x0c` 作为最小／最大范围。
- `field_target_range_build @0x39a2c` 以 movement profile 0 从攻击者格传播最大范围，再按曼哈顿距离排除小于最小范围的格。side filter 的完整语义为 `0→side 0`、`1→side != 0`、`2→side 1`、`3→side 2`；玩家普通攻击传 0。目标收集只保留未隐藏且匹配 filter 的 actor；单位不阻断范围传播。SDL 由 `src/field_attack.[ch]` 复现，并额外拒绝 HP 已为 0 的目标。

第一关新游戏索尔的详情页实机值为 HIT/AP/EV/DP=`97/16/2/12`，装备为短剑 `AP+10`、皮甲 `DP+8`、药草 `HP+40`。SDL 新游戏正式单位记录已应用同一组确认装备，并通过统一战斗属性重算生成最终数值。

详情页开合由 `field_unit_detail_transition_frame @0x3d61d` 组合三块区域：左上 86×86 在 phase `11..6` 从左侧滑入，右上 223×86 在 phase `8..3` 从上方滑入，下方 310×102 在 phase `5..0` 从底边滑入；关闭按 `0..11` 逆序。每帧先恢复保存的战场画面，再叠加当前裁剪区域。原版在打开 phase 11/5 播放 SFX 5，在部分菜单关闭 phase 0/7 播放 SFX 6；SDL 已复现视觉帧序列，音效系统尚未接入。

样本 FDFIELD.DAT[0]：24×24 网格，条目大小为 `4 + 24*24*4 = 2308` 字节，`shape_index=0`，对应 FDSHAP.DAT[0] 的 288 帧与 FDSHAP.DAT[1] 的 300 条属性记录。

**验证**：FDFIELD.DAT 的 33 个地图网格条目均满足 `size == 4 + grid_w * grid_h * 4`；metadata 均满足 `size >= 0x83 && (size-0x83)%26==0`；placement 均满足 `size == 2 + count*6`；各 stage 的 `terrain_id` 均落在对应 FDSHAP 帧数和属性表范围内。`tools/analyze_fdfield_events.py --stage 0` 可转储事件表与 placement；`--map-preview-once [stage]` 可显示指定 stage 的战场底图。

`new_game_opening_play @0x5752f` 的完整开场依次使用三个 stage：

| 段落 | FDFIELD stage | FDTXT entry / fragment | 主要动作 |
|------|---------------|------------------------|----------|
| 王宫与郊外 | 32 | `[33]` fragment `0..5` | 镜头 `(3,34)`，脚本 `0x63..0x69`，中途卷动到 `(0,43)` |
| 遇袭与同行 | 31 | `[32]` fragment `0..9` | 镜头 `(5,42)` / `(4,41)`，脚本 `0x5a..0x62`，依次加入 actor group 1/3/5，并隐藏 actor 2 |
| 第一关战前 | 0 | `[1]` fragment `0..2` | 镜头 `(4,12)` / `(0,0)` / `(0,15)`，脚本 `0,1,2,5`，两组 actor 登场特效 |

stage 32 网格为 `18×51`，`shape_index=32`；`(0,43)` 只是郊外段镜头，不是整个开场的固定视窗。stage 31 为 `20×50`，stage 0 为 `24×24`。先前将 stage 32 与 `FDTXT[1] fragment 0` 混合播放属于错误实现。

stage 31 的前五个单位模板在 offset `0x15` 分属 group `1,1,3,3,5`。`new_game_opening_play @0x5752f` 在初始镜头 `(5,42)` 只显示 actor 0/1；fragment 2 后加入 group 3 的 actor 2/3，再将镜头卷到 `(4,41)`。actor 2（unit 75）与 group 5 的 actor 4（unit 9）共用 placement `(5,44)`；原流程先隐藏 actor 2，之后才加入 group 5，因此两者不会同时绘制。一次性加载前五名 actor 会造成初始画面提前出现左侧两人，并让倒地与站立 sprite 重叠。

`field_view_render_tiles @0x3710c` 先清空 320×200 缓冲区，再只写 VGA `(4,4)` 起的 312×192 战场内区，因此四边固定保留 4 px 的调色板 index 0 黑边。SDL 镜头必须按 13×8 格内区钳制，并将镜头左上格放在 `(4,4)`；不能为消除黑边而向上、向左额外采样相邻地图格。

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

片段内容不是可直接按 Big5/GBK 解码的字节串，而是 u16 token 流。非负 token 是 FDOTHER.DAT[4] 的 16×16 字形编号，`FUN_0004c4c2 @0x73f8e` 按 `token * 0x20` 取 32 字节位图绘制。全部字形的最终 Unicode 映射见 `docs/font-glyph-map.tsv`，格式说明见 `docs/08-font-text-mapping.md`。

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

`dialog_box_open @0x3b7c0` 先将 FDOTHER.DAT[5] tile 0 的 24×24 空心框从说话角色所在格移动到对话框原点，再按 `4×2 → 8×3 → 12×4 → 16×5 → 19×5` 个内部块绘制边框，形成伪缩放弹出效果。移动空心框及五级展开之间调用的是 `delay_ms(10)`，不是 1 个约 54.9 ms 的 BIOS tick；对话结束或切换说话角色时，`dialog_box_close @0x3bd57` 也以 10 ms 步间延时反向收起。

从角色格弹框前，`dialog_box_open` 会自动调用 `field_focus_move_to @0x37efe`。该函数将焦点逐格移到说话角色；焦点继续向外移动且已达到左/右 `2/11`、上/下 `2/6` 的视窗阈值时，同步卷动镜头。因此，「说话角色在画面外时先移动视角」属于对话框系统行为，不是 FDTXT 控制码，也不要求过场脚本另发 `field_camera_pan_to` 指令。`-17/-18` 通过 `field_visible_actor_find_by_text_id @0x37e74` 查找未隐藏角色；只匹配到隐藏角色时，仍从该角色记录 offset `0x07` 读取立绘，但不移动镜头，也不从旧角色坐标弹框。`-19/-20` 直接使用 actor 索引，不经过该查找函数。

`bios_tick_delay @0x3ccbd` 读取 BIOS 计时器物理地址 `0x046c`，按调用参数等待计时器 tick；1 tick 约为 54.9 ms。`text_dialog_render_tokens @0x3b198` 每绘制一个普通字形后调用 `bios_tick_delay(1)`，因此 SDL 版逐字延时取整为 55 ms。

stage 0 两组敌方 actor 由 `FUN_000300e1 @0x57bad` 登场。该函数使用 FDOTHER.DAT[9]：`LMI1`、12 帧、每帧为 `u16 width/u16 height + LMI RLE`，0 色透明。原版先保留无新 actor 的战场底图，再在每个新 actor 格连续叠加 12 帧特效，最后进入移动脚本 1 或 2。

### 2.7 角色立绘（DATO.DAT）✅ 立绘帧包

136 个条目。每个条目是同一角色/单位的多帧 80×80 立绘包，供 `FUN_000136cc @0x3b198` 的 FDTXT 立绘控制码加载。

条目结构：

```
偏移   长度   字段
0x00   4*N    frame_offsets[N]，u32 LE；N = first_offset / 4
?      变长   frame_data，每帧为 u16 width/u16 height + LMI RLE
```

立绘帧 RLE 对应 `FUN_0004c347 @0x73e13` / `FUN_0004c379 @0x73e45`：

- `0x00..0xc0`：直接输出 1 个像素值；
- `0xc1..0xff`：run，长度 `byte - 0xc0`，下一字节为重复像素值。

**验证**：DATO.DAT 136 个条目的首帧均可按上述 RLE 解码；首批条目为 80×80、4 帧。

### 2.8 精灵/地形帧（FDSHAP.DAT）✅ 帧包 + 地形表

66 个条目。偶数条目（0,2,...,64）为 24×24 帧包；战场地图用这些帧包作为地形底图。奇数条目为 4 字节地形属性记录表，供 `FUN_00010580 @0x3804c` 与 `FUN_0001020e @0x37cda` 查询。

偶数条目包头：

```
偏移   长度   字段
0x00   2      frame_w      u16 LE，通常 24
0x02   2      frame_h      u16 LE，通常 24
0x04   2      frame_count  u16 LE，如 0x0120=288
0x06   4*N    frame_offsets[N]，相对条目起点的 u32 LE 偏移
?      变长   frame_data，逐帧 RLE 数据
```

每帧数据不再带宽高头，宽高取自包头；RLE 控制字节与 §7 / `FUN_0004c0d5 @0x73ba1` 相同。
例如 FDSHAP.DAT[0] 头为 `18 00 18 00 20 01 86 04 ...`：24×24，288 帧，首帧偏移 `0x486 = 6 + 0x120*4`。

奇数条目地形表：

```
偏移   长度   字段
0x00   1      flags                bit7=遮挡层可重绘；bit3=动态图块相位偏移
0x01   1      movement_cost_class  地形成本类别；索引单位 movement profile 的 20 字节成本表
0x02   1      attr2                用途待细分
0x03   1      attr3                用途待细分，当前样本多为 0
```

例：FDSHAP.DAT[1] 长 1200 字节，即 300 条属性记录，与 FDSHAP.DAT[0] 的 288 帧配套。FDFIELD stage 0 使用 `shape_index=0`，`terrain_id = cell & 0x03ff` 直接映射到 FDSHAP.DAT[0] 同号帧。

`field_reachable_cells_compute @0x735a4` 和 `field_path_find @0x7370a` 的内部松弛逻辑先读取 `attrs[terrain_id].movement_cost_class`，再从 `field_movement_profile_get(record[0x20])` 返回的 20 字节表中取实际消耗。也就是说 offset `0x01` 是成本类别索引，不是所有单位共用的直接移动成本。

movement profile 静态表位于原版数据段 `DS:0x1646`。相邻下一张静态表从 `DS:0x188a` 开始，因此表长严格为 `0x244 = 29 × 20` 字节，共 29 个 profile。相同的 580 字节可在 `FD2.EXE` 文件 offset `0x7a64a` 找到；DOSBox 新游戏 stage 0 运行时线性地址 `0x1b3646` 的内容与其逐字节一致。SDL 版将该已确认表内置于 `src/field_move_profile.c`，不在运行时依赖 `FD2.EXE`。

**验证**：FDSHAP.DAT 全部偶数条目共 8256 帧均可按上述头部和 §7 RLE 精确解码，且每帧数据刚好消耗完；奇数条目长度均为 4 的倍数，可按 4 字节属性记录解析。权威 code0 在 `0x3bc06` 明确执行 `cost = movement_profile[attr[1]]`，并从剩余移动预算中扣除。

## 3. 存档格式 ⚠️

### 3.1 FD2.SAV（22987 字节）✅ 初步解密与单位槽位

FD2.SAV 无魔数，整文件使用对称 XOR 流加/解密。`FUN_0004b670 @0x7313c` 从 `u16 state=0x00a5` 开始，对每个字节先执行：

```
state = rol16(state + 0x9014, 3)
byte ^= state & 0xff
```

解密后当前确认：

```
偏移      长度      字段
0x312b    0xa28*N   save slots；FUN_00027313 @0x27313 以 DAT_00003c57 选择 slot
slot+0    0xa00     单位表，最多 32 条 0x50 字节单位记录
slot+0xa00 1        stage_id，值随存档进度变化
slot+0xa01 1        unit_count，最多 32
slot+0xa02 4        DAT_00003bf3，语义待细分
slot+0xa06 4        若干进度/开关字段
```

单位记录当前确认字段：

```
偏移   长度   字段
0x00   1      map_x
0x01   1      map_y
0x02   1      map_sprite_cache_class，按 `class*12` 取 FD2.TMP；随 stage 重建，不是稳定职业号
0x03   1      direction，FUN_0000b168 @0x42c34 移动绘制时使用
0x05   1      flags，bit0 为不可见/失效；bit7 为本阶段已行动
0x06   1      side/camp，0 敌方，2 我方等
0x07   1      DATO 立绘编号
0x08   1      名称/文本编号；当前样本与 0x07 相同
0x20   1      movement_profile，`field_movement_profile_get` 按该值选择 20 字节地形成本表
0x3b   1      movement_points，传给可达范围与路径搜索作为本次移动预算
0x40   2      hp 当前值
0x42   2      hp 最大值
0x44   2      mp 当前值
0x46   2      mp 最大值
0x48   2      attack，角色/装备派生后的物理攻击
0x4a   2      defense，角色/装备派生后的物理防御
0x4c   2      accuracy，普通攻击命中值
0x4e   2      evasion，普通攻击回避值
```

单位记录的来源和派生链分为四类：

1. `field_unit_stage_template_append @0x35e6e` 将 placement 与 FDFIELD 26 字节模板重排到单位记录，并按 template level 与 unit ID 查询静态基础表。ID `<0x44` 使用 24 字节角色基础 record 和 11 字节成长 record：HP/MP 使用 `base + growth*(level-1)`，attack/defense/accuracy 基础值使用 `base + growth*level`；ID `>=0x44` 使用 10 字节敌军 record，各数值按 `coefficient*level` 生成。两条路径都写入种族索引 `0x1f`、movement/职业 profile `0x20`、level `0x21`、基础值 `0x37/0x39/0x3e`、movement points `0x3b` 和 HP/MP `0x40..0x47`。
2. 新游戏四名玩家没有 stage 模板构造步骤，但使用同一角色基础/成长表按 level 1 初始化，不再使用 stage 0 专用属性结构。
3. 存档加载按单位数逐字恢复完整 `0x50` 字节记录；恢复路径不能重新套用新建默认值，否则会覆盖 HP、MP、装备和剧情状态。
4. `field_unit_combat_stats_recompute @0x4096e（entry 0x40964）` 从 `0x37/0x39/0x3e` 读取三项基础值，从 `0x0a..0x19` 遍历 8 个装备槽，并写入 `0x48/0x4a/0x4c/0x4e`。装备槽 flag bit `0x40` 表示已装备；装备表 record `+1/+3/+5/+7` 分别累加 attack/accuracy/defense/evasion。record `0x24` 非零时，accuracy 和 evasion 的共同基础值加 15；`0x22/0x23` 分别触发攻击和防御的 x87 比例修正。

DOSBox 对 stage 构造器的 CPU 捕获确认了完整写入顺序，并以 unit 96 level 2 得到 `(profile,move,hp,mp,base attack,base defense,base accuracy)=(7,4,28,0,14,2,2)`，与既有 stage 0 捕获一致；unit 1 level 2 得到 HP 46，与本地 stage 2 存档一致。当前 `src/field_unit_base.c` 登记构造器已确认的完整同格式索引范围：角色 ID `0..31` 与敌军 ID `68..135`；没有对应基础 record 的 ID `32..67` 事务化失败。最终装备派生仍由 `field_unit_stats` 完成，装备查询或状态缩放回调缺失时不写 `0x48..0x4f`。

`--prologue-preview` 不再从 FD2.SAV 推断 sprite。新游戏流程直接建立 `DAT_00003a45`，稳定标识是 record offset `0x07` 的 unit id；offset `0x02` 只是 FD2.TMP cache slot。

**验证**：

- 解密后的本地 slot 单位记录中，unit ID `0/9/4/30` 的 `(movement_profile,movement_points)` 分别可见 `(1,4)/(5,4)/(3,7)/(25,4)`；该样本只用于印证字段会随单位记录持久化。
- DOSBox 从新游戏实际运行至 stage 0，战前过场结束后的单位表给出：unit ID `0/9/4/30` 的 `(movement_profile,movement_points,hp,mp)` 分别为 `(1,4,42,0)`、`(5,4,28,8)`、`(3,7,48,0)`、`(25,4,50,0)`；首批敌军 unit ID 96 为 `(7,4,28,0)`。该捕获是 SDL stage 0 基线的权威新游戏证据。
- 同一 DOSBox 运行时内存捕获确认四名玩家的已装备物品和最终 `(AP,DP,HIT,EV)`：unit 0 为短剑 `0`、皮甲 `132`、未装备药草 `192`，数值 `(16,12,97,2)`；unit 9 为长棍 `52`、长袍 `164`，数值 `(11,7,86,1)`；unit 4 为刺矛 `20`、布衣 `128`，数值 `(26,6,92,2)`；unit 30 为威力手臂 `72`、战斗装甲 `178`，数值 `(22,14,92,2)`。原版 flag `0x80` 的隐藏槽 item byte 含 `0xff` 和其他未初始化残值，SDL 统一规范为 `0xff`，不把残值解释为库存。
- map sprite cache class 仅描述当时的 FD2.TMP 布局，不能用于 stage 32/31。

普通攻击核心由 `field_physical_attack_resolve @0x43edb` 确认。原函数先根据地形、装备与特效修正数值；这些外围规则仍待细分。修正后的确定性核心为：

```text
hit = rand()%100 < accuracy - evasion
if hit:
    critical = rand()%100 < critical_chance
    effective_defense = critical ? defense/2 : defense
    base = max(0, (attack - effective_defense) * 9 / 10)
    damage = base + (base/9 != 0 ? rand()%(base/9) : 0)
    hp_after = max(0, hp - damage)
```

外围 `field_physical_attack_sequence @0x43a6a` 每轮必定先消费一次 RNG。武器 item `+0x09 == 3`，或该次 `rand()%100 < 3` 时执行两击，否则一击；每击都重新执行上述物理核心，目标 HP 归零即停止。`field_physical_exchange @0x3a6a2` 在进攻序列后检查目标仍存活，并调用 `field_counterattack_is_available @0x442f0`：反击者 record `+0x26` 必须为 0、双方曼哈顿距离必须为 1、反击者首件已装备武器最小射程必须为 1。满足条件时交换双方，再执行完整的一／二击序列；反击本身不消费反击者的行动标志。

每一击在命中判定前先处理地形。`map_cell_info_at @0x3804c` 返回结构的 `+0x05` 是 FDSHAP terrain attr byte 1，即 `movement_cost_class`；攻击者以该值索引 `DS:0x1a12`，防御者索引 `DS:0x1a2a`，均为 32 位百分比，计算 `stat += stat*percent/100` 后才进入暴击／伤害。`field_unit_ignores_terrain_combat_modifier @0x44397` 对 unit ID 28 强制返回 0；其他单位 movement profile 19 或 race 4/5 时返回 1 并跳过地形修正。战场信息面板在 `0x3ffe5/0x40005` 读取相同两张表；DOSBox debugger 在 battlefield overlay 已加载、`CS=0x0170`／flat `DS=0x0178` 时捕获到重定位后的访问地址 `0x1ada12/0x1ada2a`，完整六项分别为攻击 `{+5,0,-5,-5,-5,0}`、防御 `{0,0,+10,+10,-5,0}`。FDSHAP 全部奇数属性条目实际使用 class 0..5；stage 0 的 shape 0 只使用 0..2。SDL 已接入完整表，class >5 在消费 RNG 前拒绝。

`FUN_00073df7 @0x73df7` 已确认是战斗路径使用的 16 位伪随机步进：`state = rol16(state + 0x9014, 3)`，返回新状态的低 16 位。DOSBox debugger 在 `field_rng_next` 第一次执行前于 `CS:0x0170:0x1aabe3` 中断，并从 flat `DS:0x0178:0x1ae7b8` 捕获 loader 初值 `0x7a18`。SDL 版以 `src/field_rng.[ch]` 保存调用方持有的状态，确保序列次数、外围特效、命中、暴击、伤害浮动和反击共用同一 RNG 流；正式 field session 以 `0x7a18` 初始化。

暴击阈值来源分两层：基础值读取 `DS:0x24a8[attacker.record[0x20]-1]`；首件已装备武器 item record `+0x09 == 4` 时，再累加 `+0x0a`。同一次 DOSBox debugger capture 从重定位地址 `0x1ae4a8` 取得完整 30 字节基础表：`{5,3,3,5,3,3,0,18,5,3,3,12,3,3,12,10,6,3,3,7,3,3,30,18,0,0,0,0,0,0}`，索引对应 movement profile `1..30`。SDL 正式 session 已直接使用该表；测试仍可通过按攻击者 callback 覆盖。反击交换双方后会重新查询，`src/field_attack.[ch]` 再自动复现 type 4 武器加值。item `+0x09 == 2` 的路径会在命中判定前至少消耗一次 RNG，并可能再消耗一次后写防御者 `+0x25` 为 `2..5`。SDL 已在 `field_attack` 复现状态写入和 RNG 顺序；对应音效与闪烁演出仍省略。

`field_physical_attack_resolve @0x43edb` 的原始指令在完成命中、暴击和伤害浮动后，将钳制后的 HP 写入防御者 record `+0x40`（`mov [esi+0x40], ax`）。核心函数体不访问 record `+0x05`；外围 `field_physical_exchange @0x3a6a2` 在双方序列结束后调用 `field_defeated_units_finalize_entry @0x42d79`。其主体 `@0x42d83` 对视窗内 HP 0 actor 播放旋转／消散演出，随后遍历完整 actor 表，把每个 HP 0 record 的 `+0x05` **精确写为 1**。即使没有视窗内死亡单位，也走无演出的全表写入分支。SDL M6 省略死亡帧，但已复现该全表 flags 提交。

`src/field_combat.[ch]` 仅实现上述已确认核心，并要求调用方注入随机数。`field_attack` 已提供完整 profile 暴击基础表、terrain class 0..5 百分比、免疫判定、序列次数与 type 2 武器特效，`field_game` 在核心前应用这些外围规则并复现双击停止及邻接反击，再把同一流交给 `field_combat`。`damage` 对 HP 的实际扣减会钳制到当前 HP，同时保留公式产生的 `rolled_damage`；超出可保持原版 32 位中间运算语义的输入会被拒绝。

### 3.2 战场过场移动脚本（EXE 指针表）✅ 解释器格式

`field_movement_script_play @0x3887e` 通过 `field_movement_script_ptr @0x73d5c` 从 `DAT_000027d8 + script_id*4` 取脚本指针。脚本字节流格式已由反编译确认：

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

正式路径由 `field_actor_path_play @0x3869c` 按方向码 `0/1/2/3` 分派到下、左、上、右四个单格 helper：`field_actor_move_down_follow_camera @0x380be`、`field_actor_move_left_follow_camera @0x38221`、`field_actor_move_up_follow_camera @0x38399`、`field_actor_move_right_follow_camera @0x38529`。每个 helper 都先写 direction，再播放 `1..6` 相位，每相位移动 4 px 并等待一次 BIOS tick，第 6 相位后才提交单位格坐标。

单位向外移动且达到视窗阈值时，镜头不会等角色走完整格后跳动 24 px，而是在同一格的六个相位中同步移动。阈值为左/上 `<2`、右 `>=11`、下 `>=6`；镜头已到地图边界时只移动单位。修正后的 `tools/fd2_le_code0.bin` 中，五个入口为 `0x280be/0x28221/0x28399/0x28529/0x2869c`，与 corrected dual 地址逐字对应。

`field_cutscene_setup_units_camera @0x485da` 是过场 actor 初始摆放 helper。其 11 个参数依次为 `x_array, y_array, dir_or_array, first_actor, last_actor, special_actor, special_x, special_y, special_dir, camera_x, camera_y`；`dir_or_array < 4` 时作为固定 direction，否则作为 direction 数组指针。它写入 `DAT_00003a45` 的 `x/y/direction`，并同步 `DAT_00003aa9/03aad` 与 `DAT_00003ab1/03ab5` 镜头坐标。

`field_camera_pan_to @0x387f1` 先逐格完成 X 轴，再逐格完成 Y 轴，每格刷新一次并等待 vsync；确认键不终止镜头。开场在脚本 `0x64`、`0x69`、`0x62` 前将 `DAT_00003afb=1`：移动脚本每个 4 px 相位递增一次 DAC 暗度，边移动边渐出；新场景初始化随后调用 `palette_fade_in_light @0x44739` 渐亮。

`DAT_000027d8` 指针表位于 object3；LE fixup 的 `target_offset` 映射到 raw object3 数据时需扣除 `0x28b8`，再加 object3 relbase。按此映射可完整解码 script `0..10` 与开场脚本 `0x5a..0x69`；其中 script `3/4/6/7/8` 已用于 stage 0 回合事件，`0/1/2/5/0x5a..0x69` 与 `new_game_opening_play @0x5752f` 的直接调用顺序一致。

`DS:0x1d71` 是关卡过场分发表，当前按 30 项处理；相邻 `DS:0x1de9` 是进入战场前的另一张分发表，不应混入 stage dispatch。`tools/analyze_fd2_stage_code.py` 默认使用 `tools/fd2_le_code0.bin`；object1 fixup 使用 `target-0x28b8+0x27acc` 进入统一 code0，应直接采用 `dump_fd2_fixup_table.py` 输出的 `code0` 或 `dual` 地址。

第一关后续事件分支仍包含 fragment `3..5` 和 `field_camera_pan_to(5,0x11)` 等调用；它们不属于初始开场。`DAT_00003aa9/DAT_00003aad` 表示战场视窗左上角格坐标。`field_actor_is_hidden @0x59aa8` 读取 actor flags bit0。

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

`FUN_0000e761 @0x4622d` 按 actor record offset `0x07` 的 `unit_id`，从 FDICON.B24 复制 12 帧到 FD2.TMP，并把运行期 cache slot 写入 record offset `0x02`。`fd2tmp_map_sprite_load @0x53b6f` 再将固定大小 `0x32a00` 的 FD2.TMP 读入 `DAT_00003a61`。`map_actor_blit_24x24 @0x42c34` 对缓存使用：

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

移动阶段使用 actor record offset `0x04` 的 `1..6` 相位，每帧沿方向偏移 4 px，第 6 帧跨满 24 px。`field_animation_phase_update @0x37b91` 约每 275 ms 推进共享 idle 相位；`map_actor_blit_24x24 @0x42c34` 对 offset `0x04 == 0` 的所有静止 actor 使用该全局相位，并将 phase 3 映射回 phase 1，因此某个角色移动时，其他角色仍以 `0,1,2,1` 序列播放 idle 动画。

## 4. 音频相关文件

| 文件 | 用途 |
|------|------|
| `FDMUS.DAT` | 15 项有效 Miles XMIDI 音乐序列 |
| `FDOTHER.DAT[31]` | `DAT_00003eec` 固定加载的 13 项 unsigned 8-bit mono PCM bank |
| `FDOTHER.DAT[80]` | `DAT_00003b13` 固定加载的 16 项战场样本 bank；已确认 SFX 1=actor group flash、11=stage transition、13=earthquake |
| `*.MDI` | AIL 音乐驱动（AdLib/OPL3/SB/MPU401/...），不是音乐资源 |
| `*.DIG` | AIL 数字采样驱动，不是音效资源 |
| `SAMPLE.BNK` | 头为 `01 00 ADLIB-` 的乐器名称与 AdLib 元数据 |
| `SAMPLE.AD` / `SAMPLE.OPL` | 内容相同，开头为 Miles AIL `patch, bank, offset` 音色目录 |
| `DIG.INI` / `MDI.INI` | 驱动配置 |
| `AILDRVR.LST` | 可用驱动列表 |
| `FDICON.B24` | 140 组地图角色 24×24 帧，详见 §3.3 |

`ail_init_sample @0x5e735` 的默认 sample rate 为直接常量 `0x2b11=11025 Hz`，loop count 为 1，pan 为 64；`sfx_play` 先调用 `ail_end_sample @0x5ea19`，index `-1` 表示只停止不启动新样本，其他索引才设置地址、长度和调用方给出的循环次数。DOSBox 0.74 的 44100 Hz mixer capture 对 FDOTHER[31] SFX 5/8 的 11025 Hz 线性重采样相关系数为 0.950/0.968，而 8000/16000/22050 Hz 候选均低于 0.12，形成运行时交叉验证。`music_track_play @0x4ab8b` 从 FDMUS 加载 XMIDI，调用 AIL sequence init/start，并处理 2000 ms 淡入、4000 ms 停止淡出和循环次数。

重写不复刻 AIL 驱动层，但需要保留其资源与播放语义：SDL3 `SDL_AudioStream` 负责设备和最终 PCM，原始样本与 XMIDI/OPL 分别通过独立 source 后端进入统一混音器。完整计划见 `docs/11-audio-plan.md`。

## 6. 嵌套 .DAT 归档

FDOTHER.DAT 中部分条目本身也是 .DAT 归档（6 字节 "LLLLLL" 魔数开头）：

| 条目 | 用途 | 子条目数 | 关键子条目 |
|------|------|---------|-----------|
| [7] | 标题画面 | 7 | sub[0]=320×200标题图, sub[1-6]=菜单文字(61×7等) |
| [31] | UI／战场数字音效 bank | 13 | `sfx_play(DAT_00003eec,index,loop_count)` 按 offset 直接提交样本字节与长度 |
| [80] | 战场数字音效 bank | 16 | 固定载入 `DAT_00003b13`；调用点使用 SFX 1/11/13 等索引 |
| [0x4d] | 片头底图 | 4 | sub[0]=片头背景(非标准RLE) |

**验证**：FDOTHER[7] 用 `fd2_archive_open_mem()` 打开后，sub[0] 成功解码为 320×200，
用调色板[8]渲染得到 159 色标题图，主色 rgb(36,0,0) 与 DOSBox 实机一致。

## 7. RLE 解码方案修正（FUN_0004c0d5）

旧版 image.c 把 SKIP(0xC0-0xFF) 当成 RUN（多读一个字节），导致数据错位撕裂。

**正确方案**（源自反编译 FUN_0004c0d5 @0x73ba1，逐行解码）：

| 控制字节范围 | 类型 | 行为 | 宽度消耗 | 额外数据 |
|------------|------|------|---------|---------|
| 0x00-0x3F | RUN | val 重复 count 次 | count | 1字节(val) |
| 0x40-0x7F | RUN stride-2 | val 隔字节写 | count*2 | 1字节(val) |
| 0x80-0xBF | LITERAL | 逐字节复制 | count | count字节 |
| 0xC0-0xFF | SKIP | 透明跳过(dst保持0) | count | 无 |

count = (c & 0x3F) + 1

**验证**：FDOTHER[0x45-0x49] 片头5帧，逐行解码精确消耗100%输入字节，
输出恰好 320×147=47040 像素，147/147 行完整。
