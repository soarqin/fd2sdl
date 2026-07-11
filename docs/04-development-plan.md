# 开发计划：炎龙骑士团 2 SDL3 重写

> 目标：以 SDL3 为引擎重写《炎龙骑士团 2》，读取原版数据文件，
> 兼容原版存档（FD2.SAV / FD2.TMP）。
> 本计划基于 `docs/01-decompilation-report.md` 与 `docs/03-data-formats.md` 的逆向成果。

## 0. 项目结构

```
fd2sdl/
├── original_game/          # 原版文件（只读，不修改）
├── docs/                   # 文档
│   ├── 01-decompilation-report.md
│   ├── 02-decompilation-samples.md
│   ├── 03-data-formats.md
│   ├── 04-development-plan.md  ← 本文件
│   ├── 05-boot-flow.md
│   ├── 06-r2-ghidra-mapping.md
│   └── 07-decompilation-corrections.md
├── tools/                  # 辅助工具
│   └── dat_extract.py      # .DAT 解包工具
├── src/                    # SDL3 重写源码
│   ├── CMakeLists.txt
│   ├── main.c
│   ├── archive.c/.h        # .DAT 容器读取
│   ├── image.c/.h          # RLE 图像解码
│   ├── audio.c/.h          # XMID 播放
│   ├── save.c/.h           # FD2.SAV/TMP 读写
│   └── game/              # 游戏逻辑
└── README.md
```

## 1. 阶段划分

### 阶段 0：环境搭建（预计 0.5 天）

- [x] SDL3 获取改为系统优先、CPM.cmake 自动回退
- [x] CMake 构建脚手架，链接 SDL3
- [ ] 创建窗口、清屏的基本程序
- [ ] `tools/dat_extract.py`：实现 .DAT 解包（格式已破解，见 03-data-formats.md）

**验收**：`./fd2sdl` 打开窗口；`python tools/dat_extract.py TITLE.DAT` 列出 7 个条目。

### 阶段 1：数据层（预计 2 天） ✅ 核心完成

- [x] C 实现 .DAT 容器读取（`archive.c`）
- [x] 解码图像 RLE（方案已逆向确认，见 `docs/03-data-formats.md` §2.1，源自 FUN_0004e176）
- [x] 定位标题调色板（FDOTHER.DAT[8]，256 色 6-bit，见 `docs/03-data-formats.md`）
- [x] 渲染标题图（FDOTHER.DAT[7] sub[0]；与 TITLE.DAT[0] 同源）到 SDL framebuffer
- [x] 渲染 BG.DAT[0]（320×100 背景）
- [x] 实现 FDSHAP.DAT 精灵加载（偶数条目为 24×24 精灵帧包；头含帧数 + 帧偏移表；帧数据复用 FUN_0004c0d5 RLE 控制字节）

**验收**：屏幕显示原版标题画面与一张背景。 ✅ 已验证（`docs/title-rendered.png`）

### 阶段 2：文本与地图（预计 2 天）

- [x] FDTXT.DAT 文本加载（已确认为 u16 token 流，非直接 Big5/GBK；非负 token 映射到 FDOTHER[4] 16×16 字形）
- [x] TAI.DAT 图像集加载与渲染（通用 RLE 图像，使用 FDOTHER.DAT[0] 调色板）
- [x] FDFIELD.DAT 战场地图解析（每 3 条一组，组内第 0 条为 u32 cell 网格）
- [x] FDSHAP.DAT 地形帧与奇数条目地形表映射（`cell & 0x03ff` → 同号 24×24 帧；flags 0x80 为遮挡层后一帧）
- [x] 在地图上铺贴原版 24×24 地形帧（已接入 `--map-preview`）

**验收**：`./src/fd2sdl --map-preview` 显示一张完整战场地图预览；stage 0 已按 FDSHAP 地形表映射校正。

### 阶段 3A：完整新游戏初始过场 ✅

- [x] DATO.DAT 立绘、FDOTHER.DAT[4] 字模、FDOTHER.DAT[5] 对话框 UI 与 FDTXT 控制码解释
- [x] 追踪 `new_game_opening_play @0x2fa63`，确认完整流程为 stage 32 → 31 → 0
- [x] 播放 FDTXT `[33]` fragment `0..5`、`[32]` fragment `0..9`、`[1]` fragment `0..2`，不混入第一关后续事件对白
- [x] 复现移动脚本 `0,1,2,5,0x5a..0x69`、逐格 6 相位移动、actor 隐藏和镜头卷动
- [x] 按 placement 建立 stage 32 王宫角色、走廊卫兵和郊外角色，建立 stage 31 五名剧情 actor 与 stage 0 三组 actor
- [x] 解析 FDICON.B24 的 140 组地图 sprite，按 unit id 直接选帧；不再从 FD2.SAV 猜测职业，也不依赖会随 stage 改写的 FD2.TMP cache class
- [x] 用 FDOTHER.DAT[9] 的 12 帧 LMI1 动画复现 stage 0 两组 actor 登场特效
- [x] 复现对话框空心框移动、五级展开/收起、55 ms 字速、1500 ms 翻页和约 275 ms idle 动画
- [x] 移除统一四边裁剪；按地图像素边界钳制镜头并填满 320×200
- [x] 提供 `--prologue-preview` 与快速回归入口 `--prologue-preview-once`

**验收**：`./src/fd2sdl --prologue-preview` 从 stage 32 王宫开始，完整播放 stage 31 剧情和 stage 0 战前段，在 fragment 2 结束后保留第一关战场；画面无 SDL 额外黑框。`--prologue-preview-once` 无延时跑完整条调用链。

### 阶段 3：核心系统（预计 3 天）

- [ ] 角色结构体（位置、朝向、属性）—— 对照 FDFIELD 单位记录、DATO 立绘编号与反编译的 `FUN_000107b2`
- [ ] 角色精灵渲染（朝向 0x9/0x11/0x15/0x16/0x1b/0x17）
- [ ] 键盘/鼠标输入（对照 `FUN_00010780` 输入轮询）
- [ ] 光标与格子选择
- [ ] 移动范围计算（战棋核心，参考 `fcn.0001f0f5` 疑似 AI/寻路）

**验收**：可在地图上移动光标，选中格子显示坐标。

### 阶段 4：战斗系统（预计 4 天）

- [ ] 回合制流程状态机
- [ ] 角色移动动画（FIGANI.DAT 战斗动画）
- [ ] 攻击/施法/待机指令
- [ ] 伤害计算（DATO.DAT 中的属性/技能表）
- [ ] 经验/升级

**验收**：完成一场简单战斗（攻击、扣血、回合切换）。

### 阶段 5：存档兼容（预计 1.5 天）

- [ ] 逆向 FD2.SAV 读写函数，确认加密/校验方案
- [ ] 实现 FD2.SAV 读取（密码校验）
- [ ] 实现 FD2.TMP 读取（场景临时状态）
- [ ] 验证可加载原版存档进入游戏

**验收**：用原版 FD2.SAV 启动并恢复到存档时的场景。

### 阶段 6：音频与剧情（预计 2 天）

- [ ] XMID 转 MIDI/OGG 预处理工具
- [ ] SDL3_mixer 播放背景音乐（FDMUS.DAT）
- [ ] 音效（SAMPLE.BNK）
- [ ] 剧情脚本解释器（参考 `fcn.00019f67` 34493 字节大函数）

### 阶段 7：打磨（预计 2 天）

- [ ] 菜单系统（对照 `fcn.0001753f` 1377 块的 UI 函数）
- [ ] 物品/装备/职业
- [ ] 过场动画（ANI.DAT 的 AFM 格式）
- [ ] 原版分辨率适配（320×200 -> 现代分辨率，整数缩放）

**总预估**：约 17 个工作日。

## 2. 技术决策

### 2.1 引擎与依赖

| 组件 | 选择 | 理由 |
|------|------|------|
| 图形/输入 | SDL3 | 项目要求 |
| 音频 | SDL3_mixer 或 SDL3 audio + 预转换音频 | 原版用 AIL，复刻成本高 |
| 构建 | CMake | 跨平台，成熟 |
| 语言 | C（主）/ Python（工具） | 对照原版 Watcom C |

### 2.2 调色板处理

游戏为 8bpp 索引色。策略：
- 维护一个 256 色调色板
- 加载时转换为 SDL ARGB8888 Texture，或用 SDL_Surface 8bpp + palette
- FDSHAP.DAT 精灵可能各自带调色板，需合并/切换

### 2.3 坐标与分辨率

- 内部逻辑分辨率 320×200（原版 VGA Mode 13h）
- 窗口用整数缩放（2x/3x），保持像素清晰
- 鼠标坐标需反向映射到逻辑坐标

### 2.4 字符编码

原版 `FDTXT.DAT` 不是 Big5/GBK 文本，而是指向 `FDOTHER.DAT[4]` 的自定义 `u16` 字形索引流。重写应查阅 `docs/font-glyph-map.tsv` 的 `glyph ID → Unicode` 映射，再按需要序列化为 UTF-8 或 UTF-16；不能对原始 FDTXT 字节直接调用 iconv。映射格式与控制码边界见 `docs/08-font-text-mapping.md`。

## 3. 风险与待解问题

| 风险 | 影响 | 缓解 |
|------|------|------|
| LE fixup 误写入代码页 | 反编译指令被污染，启动流程误判 | ✅ 已解决：不 patch 代码页；用 `tools/rebuild_fd2_analysis.py` 生成未应用 fixup 的分析镜像，fixup 仅作索引 |
| 图像 RLE 方案未完全确认 | 阶段 1 阻塞 | ✅ 已解决：逆向 FUN_0004e176，见 §2.1 |
| 调色板选择误判 | 标题与片头颜色不一致 | ✅ 已解决：标题图用 FDOTHER.DAT[7] sub[0]，标题调色板用 FDOTHER.DAT[8] |
| FD2.SAV 加密 | 阶段 5 阻塞 | 反编译存档读写函数定位加密算法 |
| DATO.DAT 结构体未知 | 角色属性无法读取 | 阶段 3 结合反编译对照 |
| 无符号函数 | 逆向效率低 | 通过字符串引用、调用约定、FunctionID 逐步命名 |
| 大函数（5.5万字节）逆向 | 战斗系统理解慢 | 分块逆向，优先状态机骨架 |

## 4. 反编译优先级

按开发阶段需求排序的逆向目标：

1. ~~**图像 RLE 解码函数**（阶段 1，高优先）~~ ✅ 已完成（FUN_0004e176）

2. **存档读写函数**（阶段 5，高优先）
   - 搜索引用 "FD2.SAV"、"password" 的代码
   - 目标：FD2.SAV 结构与加密算法

3. **资源加载函数 `FUN_0001131a`**（阶段 1-2）
   - 已知：从资源包加载精灵帧
   - 目标：确认如何选择 .DAT 与条目

4. **角色结构体布局**（阶段 3）
   - 对照 DATO.DAT 与 `FUN_000107b2` 朝向字段 `DAT_00003c03`

5. **战场 AI/寻路**（阶段 3-4）
   - `fcn.0001f0f5`（34179 字节）

6. **战斗主循环**（阶段 4）
   - `fcn.0004784c`（55553 字节）

7. **剧情脚本**（阶段 6）
   - `fcn.00019f67`（34493 字节）

## 5. 验收标准（最终）

- [ ] 可读取全部 11 个 .DAT 文件
- [ ] 可加载原版 FD2.SAV 存档
- [ ] 可加载原版 FD2.TMP 临时数据
- [ ] 可显示标题、地图、角色、UI
- [ ] 可进行一场完整战斗
- [ ] 可播放背景音乐
- [ ] 不依赖原版 FD2.EXE，仅依赖 original_game/ 中的数据文件
