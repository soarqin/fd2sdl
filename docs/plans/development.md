# 开发计划：炎龙骑士团 2 SDL3 重写

> 目标：以 SDL3 为引擎重写《炎龙骑士团 2》，读取原版数据文件，
> 兼容原版存档（FD2.SAV / FD2.TMP）。
> 本计划基于 `docs/reverse-engineering/decompilation-report.md` 与 `docs/formats/data-formats.md` 的逆向成果。

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
- [x] 解码图像 RLE（方案已逆向确认，见 `docs/formats/data-formats.md` §2.1，源自 FUN_0004e176）
- [x] 定位标题调色板（FDOTHER.DAT[8]，256 色 6-bit，见 `docs/formats/data-formats.md`）
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
- [x] 追踪 `new_game_opening_play @0x3231b`，确认完整流程为 stage 32 → 31 → 0
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

详细依赖、逆向阻塞项和分阶段验收见 `docs/plans/battlefield.md`。

- [x] 通用战场单位结构：保留 `DAT_00003a45` 的 0x50 字节布局，统一过场与正式战场状态
- [x] stage 0 初始单位槽位加载与精灵渲染：解析 metadata 模板与 placement，按 `unit_id` 读取 FDICON.B24；战斗属性初始化后续补齐
- [x] 正式战场 session：统一持有 stage 资源、单位、镜头、光标及回合骨架状态，预览入口已复用
- [x] 过场结束状态交接与正式 `--field-play`、`--new-game-play` 入口
- [x] 键盘光标、镜头阈值跟随、单位命中和最小确认/取消选择
- [x] 原版战场选择框与移动范围 LUT：FDOTHER[1] frame 0、FDOTHER[3] 23 张 palette translation table、`DS:0x1a97` 20 相位序列
- [x] 正式地形/单位常驻信息面板：FDOTHER[5] 原始底板、terrain、A/D 修正、单位 sprite 与 HP
- [x] 全屏角色详情页与装备列表：DATO 立绘、FDOTHER[5] frame 20/21、FDTXT 名称及确认属性
- [x] 提取 215×23 字节完整物品表及详情页主要加成
- [x] 通过 DOSBox 运行时单位记录确认其余三名初始玩家装备；隐藏槽残值不作为库存
- [x] 复现详情页 12 帧三段开合视觉
- [x] 接入详情页打开 phase 11/5 的 SFX 5 与关闭 phase 0/7 的 SFX 6
- [ ] 接入其余战场 SFX（图形指令菜单 SFX 8 已接入）
- [x] 不实现鼠标坐标映射：原版游戏仅支持键盘
- [x] 移动核心逆向：确认 record `0x20/0x3b`、FDSHAP 成本类别、敌我占格与控制区规则
- [x] 参数化 Dijkstra、路径重建和移动 policy 自动测试
- [x] 提取原版 29×20 字节 movement profile 表，以 DOSBox 新游戏捕获确认 stage 0 单位移动基线并接入真实范围查询
- [x] 在战场显示移动范围、预览路径并提交六相位移动；移动后取消可恢复原坐标和镜头

**当前验收**：`./src/fd2sdl --field-play 0` 可在第一关选择我方单位、查看移动范围，并以原版 6×4 px、每相位约 55 ms 的节奏移动；移动后显示原版四向图形指令菜单，attack 进入目标选择，wait 完成行动，取消恢复移动前状态。全部玩家完成后依次进入 side 1、side 0 和下一回合。`--field-play-once 0` 与 `--new-game-play-once` 验证移动、回退、四向镜头、已行动状态、phase 顺序及 stage 0 回合事件；CTest 验证 profile、寻路、占格、指令菜单、普通攻击和 FDFIELD 事件查询。选择框使用 FDOTHER[1] 原始 24×24 帧，指令菜单使用 FDOTHER[2] 前 12 帧，范围按 FDOTHER[3] LUT 循环映射完整地形与遮挡层。

### 已完成：战场图形指令菜单

- [x] 确认 `field_player_command_execute @0x3dfa0`、FDOTHER[2] 资源、四向布局、attack/magic/item/wait 顺序、禁用条件和返回值。
- [x] 将已完成的普通攻击和待机状态机接入菜单，不复制战斗结算逻辑。
- [x] 使用中央输入 FIFO 实现四方向直接选择、确认和返回；未重新引入分散的 `SDL_PollEvent`。
- [x] 无武器、已行动、无合法目标等状态在菜单可用性与执行前检查中保持一致。
- [x] 增加无窗口资源、绘制、输入和状态测试，覆盖 attack、wait、禁用方向、取消及 RNG 不推进。
- [ ] 补充 DOSBox 战场菜单截图，逐像素对照最终位置、调色板和四相位动画。

magic 和 item 的身份、原版入口及禁用条件已经确认；执行状态机尚未实现，因此当前显示 disabled 帧。敌方 phase AI 的已确认语义已在 M7 接入；下一优先阶段改为战场系统菜单、存档／读档、配置、退出和标题分派，详见 `docs/plans/battlefield.md` 的 M8。

### 战斗前置：原版输入系统

在接入攻击指令前，先完成 `docs/systems/input.md` 定义的输入层，避免继续向各 UI 循环散落 SDL 键盘事件。

- [x] 确认 BIOS 键盘缓冲检查、`INT 16h/AH=10h` 读取路径、标题菜单和当前战场控制器的扫描码
- [x] 建立唯一 SDL 事件所有者、有界 FIFO、非消费式「任意键」查询和可注入测试接口
- [x] 将标题、片头／ANI、战场和预览入口迁移到上下文动作映射；保留原版重复键的 FIFO 顺序
- [ ] 用 DOSBox 分别验证标题、战场确认／取消及 F1/F2 辅助动作

### 阶段 4：战斗系统（预计 4 天）

- [x] 最小回合制流程状态机：已行动 bit、phase `2→1→0`、自动待机和回合递增
- [x] 地图角色移动动画（复用 FDICON 六相位步态）
- [x] 移动／普通攻击／待机确定性流程
- [x] 原版战场图形指令菜单（attack/magic/item/wait；magic/item 暂为禁用）
- [x] 普通物理攻击核心、四项派生重算、地形／暴击表、双击、反击和 HP 0 隐藏提交
- [x] M7.0 敌方 AI 纯查询：record `+0x34..0x36`、最近敌对目标与原版可达目的地排序
- [x] M7.1 敌方攻击计划查询：移动后普通攻击候选、优先级 `0/8/18`、三类动作 signed 分数分派
- [x] M7.2 敌方法术／物品计划查询：36×7 字节法术表、`0x3ad8b/0x3afb6/0x3aa94` 评分、施法中心与普通／轴线物品候选
- [x] M7.3a 显式物理提交：完整 `0x3a104` 平价门控、计划重验、路径重建／播放和共享 `field_physical_exchange @0x3a6a2`；保持自动 phase 不变
- [x] 接入已确认的常规法术／物品 effect、mode-2 witness、mode-1 长路径 fallback、主要 behavior 和 side 1/0 自动 phase；特殊剧情 handler 保持显式失败
- [ ] 玩家法术／道具、经验和升级
- [ ] FIGANI.DAT 战斗演出；排在阶段 5 的战场系统菜单和存档流程之后

**验收**：第一关可以执行移动、普通攻击、扣血、敌方 AI 和回合切换；尚未支持的特殊 handler 必须明确停止。

### 阶段 5：战场系统菜单与存档流程（当前最高优先级）

该阶段先于 FIGANI。原版空焦点菜单先分派次级菜单、行军、配置和结束回合；
次级菜单再分派独立屏幕页、存档、读档和离开战场。它们与单位 action 菜单
共享 FDOTHER[2] 和通用四向绘制 primitive，但业务状态保持独立。详细拆分和
验收标准见 `docs/plans/battlefield.md` 的 M8。

- [x] 从 corrected code0 `0x17e7` 控制器确认空焦点入口、按键和独立
  F1/Page Up 辅助页；锁定空焦点、次级和设置三段 dispatcher 及 command IDs
- [ ] 请求开发者手工操作 DOSBox，提供菜单入口、各项选中、存档／读档槽、配置页和退出确认的截图或操作结果
- [x] 建立独立 system-menu 状态机、输入上下文、完整 78 帧资源渲染、四相位开合／独立高亮状态、SFX 8 和存档／acted 动态 disabled 门；未完成确认对话的动作保持 pending
- [x] 补齐 FD2.SAV XOR、末尾 checksum、四 slot／无效标记、未知字节保留式整文件后端，以及 stage 0 正式 session 的活动快照／手工槽事务导入导出；slot 未知尾部按原样保留，不在无机器证据时猜测字段
- [x] 实现正式战场 session 的活动快照事务化导入／导出，恢复 stage、turn、镜头／焦点、已确认 cell state 和完整单位记录；按原版 loader 控制流回到 side 2 browse。原版快照不保存 RNG，并在进入控制器前按计时器低 8 位扰动随机流
- [x] 将系统菜单 Save／Load 连接到正式文件编排与 FDTXT 结果片段；默认写入工作目录的 `FD2.SAV`，`FD2SDL_SAVE_PATH` 可覆盖路径，不写入只读 `original_game/`
- [x] 按原版四个配置全局实现 options 即时 toggle、活动快照和手工 slot 持久化，并将已确认的音乐／SFX 两项连接到音频 bus；手工 Load 后立即刷新 audio bus；第三、四项名称及最终可见效果仍待人工证据
- [x] 将游戏内离开战场确认映射为独立返回标题结果，并与宿主窗口关闭／Escape 退出分离；原版应用级调用者的最终去向仍待完整调用图核验
- [x] 将普通标题菜单分派到新游戏、活动快照读档和退出；依据 code0 `0xf894/0x15ebb` 将活动快照作为标题中间 action，严格验证 FD2.SAV，当前只接受 stage 0。该 Load 是 SDL 已支持的活动快照恢复，不冒充原版四槽手工 Load；命令行快速入口继续用于回归
- [x] 恢复原版四槽手工 Save／Load 的分派、纵向槽选择器和完整 `0xa00` 单位区写回证据；该流程属于独立战场 dispatcher，不是标题 action 或 secondary active-snapshot Save／Load。SDL 已补完整 slot codec、独立 picker／输入状态、事务后端及原版成功片段 `0x294/0x1de`；当前产品从空焦点 secondary 继续操作活动快照，手工槽 API 作为已验证兼容层保留，最终可见入口与视觉待 DOSBox 人工证据

**验收**：从正式战场打开系统菜单，完成「保存 → 改变状态 → 读取」并继续同一回合；取消菜单、配置或退出确认时，除明确允许的菜单临时字段外，不得改变单位、回合、事件、镜头、持久配置或推进 RNG。标题菜单可进入新游戏和受支持的存档。手工槽流程接入后，还必须验证四槽选择、空槽拒绝和完整单位 cache 重建。

### 阶段 6：音频与剧情（音频框架选型后重新估算）

- [x] 确认原版基于 Miles AIL，音乐为 FDMUS XMIDI，UI SFX 来自 FDOTHER 嵌套 PCM bank
- [x] 映射固定／动态 sample bank、11025 Hz U8 mono 默认参数和音乐控制调用链
- [x] 基于 SDL3 `SDL_AudioStream` 建立 device、music/sfx bus、有界命令队列、null backend 和离线 PCM capture 入口
- [x] 实现嵌套 bank、U8 mono、voice pool、循环与 rate conversion 的 PCM source
- [x] 完成 DOSBox SFX 5/8 波形验证并接入详情页 phase 音效
- [x] 加载 FDOTHER[80] 并登记 actor flash、stage transition、earthquake cue
- [x] 实现 actor flash、earthquake、stage transition 的 snapshot/cue 帧调度器
- [x] 实现 actor flash、earthquake、stage transition 的事务化 snapshot 生成器、原版定时 wrapper 和 `--field-effect-play` 可达验收入口
- [ ] 映射并接入 FIGANI metadata 驱动的动态战斗音效 bank
- [ ] 完成 libADLMIDI + AIL bank + Nuked OPL3 保真原型及许可证评审
- [ ] 选定并接入正式 XMIDI/OPL 音乐后端
- [ ] 剧情脚本解释器（参考 `fcn.00019f67` 34493 字节大函数）

音频详细计划和验收标准见 `docs/plans/audio.md`。

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
| 音频核心 | SDL3 `SDL_AudioStream` + 项目内 source/mixer 边界 | 当前 SDL 3.2.12 可直接使用；先统一 PCM、音乐与测试后端，不被具体解码器绑定 |
| XMIDI/OPL | libADLMIDI 原型，正式后端待许可证与 DOSBox 对照后决定 | 上游支持 AIL XMI、AIL FM bank 与 Nuked OPL3；不能在许可证评审前直接成为发布依赖 |
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
- 原版游戏仅支持键盘，不增加鼠标交互

### 2.4 字符编码

原版 `FDTXT.DAT` 不是 Big5/GBK 文本，而是指向 `FDOTHER.DAT[4]` 的自定义 `u16` 字形索引流。重写应查阅 `docs/formats/font-glyph-map.tsv` 的 `glyph ID → Unicode` 映射，再按需要序列化为 UTF-8 或 UTF-16；不能对原始 FDTXT 字节直接调用 iconv。映射格式与控制码边界见 `docs/formats/font-text-mapping.md`。

## 3. 风险与待解问题

| 风险 | 影响 | 缓解 |
|------|------|------|
| LE fixup 误写入代码页 | 反编译指令被污染，启动流程误判 | ✅ 已解决：不 patch 代码页；用 `tools/rebuild_fd2_analysis.py` 生成未应用 fixup 的分析镜像，fixup 仅作索引 |
| 图像 RLE 方案未完全确认 | 阶段 1 阻塞 | ✅ 已解决：逆向 FUN_0004e176，见 §2.1 |
| 调色板选择误判 | 标题与片头颜色不一致 | ✅ 已解决：标题图用 FDOTHER.DAT[7] sub[0]，标题调色板用 FDOTHER.DAT[8] |
| FD2.SAV 加密 | 阶段 5 阻塞 | 反编译存档读写函数定位加密算法 |
| 单位战斗属性来源仍不完整 | 正式攻击指令无法覆盖所有单位 | 移动字段、stage 0 HP/MP、四项派生链和普通攻击核心已确认；继续追踪基础属性、HP/MP 新建来源及职业/装备表；DATO.DAT 当前只作为立绘帧包 |
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
   - ✅ 已确认战场单位表为 0x50 字节步长，并确认坐标、显示、阵营、unit/text ID 与 HP/MP 偏移
   - ✅ 已确认 `0x48/0x4a/0x4c/0x4e` 派生链；待追踪基础字段、HP/MP 新建来源和职业/装备表，DATO.DAT 当前只作为立绘帧包

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

## 6. 暂不安排的验证项

以下输入项已完成静态逆向，当前不排入 M6 及后续已排期里程碑；在对应 UI 重写或具备 DOSBox 交互录制条件时再验证并实现。

- 战场 `Esc`、`Z`、数字小键盘 `5`、数字小键盘 `.`／`Delete`：`field_turn` 控制器将这四类扫描码送入同一 actor 遍历／焦点更新分支，不能按当前 SDL 版的习惯猜测为「取消」。
- 战场 `F1`／`Page Up`：进入同一辅助页面函数 `0x4521e`，页面名称、关闭键和分页规则尚未确认。
- 战场 `G`：在战场控制器和后期菜单均有直接比较，尚未确认可见语义。
- BIOS typematic 的首次延迟与重复间隔：SDL 版已保留 key-down repeat 的 FIFO 顺序，但不伪造未测得的时间参数。

静态地址、扫描码和调用路径见 `docs/systems/input.md`。
