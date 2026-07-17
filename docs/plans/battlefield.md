# 战场系统后续开发计划

## 1. 当前边界

当前实现已形成第一关可交互战场，包含移动、回合、原版图形指令菜单、无演出的确定性普通攻击，以及 M7 敌方 AI 的查询、提交、自动 phase 和已确认的 stage 0 结果门。玩家法术／道具状态机、特殊 handler 与关卡结束转场仍未完成。

已完成的基础能力：

- 解析 FDFIELD 地图、metadata、placement 与单位模板。
- 解析 FDSHAP 24×24 地形帧和遮挡层属性。
- 使用 FDICON.B24 绘制地图单位和静止、移动帧。
- 使用统一的 `fd2_field_unit` 表示过场与正式战场单位。
- 播放开场移动脚本、镜头移动、对话和单位登场效果。
- 使用正式 session 管理 stage 资源、单位、镜头和交互状态。
- 将开场结束后的动态单位和镜头状态交给正式战场。
- 使用键盘光标浏览地图、命中单位并确认或取消我方单位选择。
- 使用 FDOTHER[2] 绘制 attack/magic/item/wait 四向指令菜单，接入普通攻击、待机和取消回退。
- 完成无演出的确定性普通攻击、双击、反击、地形修正和死亡提交。
- 读取 FD2.SAV 的 XOR 加密数据、slot 元数据和 0x50 字节单位记录。

尚未完成的核心能力：

- 玩家法术、道具、经验和升级。
- 特殊 AI behavior／法术／物品 handler、通用 cell action handler。
- 已有 stage 0 结果门和增援 action；原版 stage 结束转场与下一关初始化仍未接入。
- FIGANI 战斗演出。
- 非 stage 0 的正式 session 恢复；stage 0 活动快照和手工槽恢复已完成，
  未确认的 slot 尾部字段继续保留式读改写。

## 2. 单位数据结构是否阻塞

战场 session 的建立当前不受单位结构阻塞。

`src/field_unit.h` 已定义严格为 0x50 字节的 `fd2_field_unit`，并确认以下字段：

- `0x00/0x01`：地图坐标。
- `0x02`：FD2.TMP 运行期精灵 cache class。
- `0x03/0x04`：朝向和移动相位。
- `0x05`：隐藏或失效标志。
- `0x06`：阵营。
- `0x07/0x08`：unit ID 和 text ID。
- `0x20`：20 字节地形移动成本 profile 的编号。
- `0x3b`：本次移动预算。
- `0x40..0x47`：当前和最大 HP、MP。

步态帧和模板来源等 SDL 派生状态已放在旁路数组，单位表仍保持原版 0x50 字节步长。因此它足以承载战场资源、单位列表、镜头、光标和回合骨架。

移动查询所需字段已经确认；后续战斗系统仍受其他字段语义约束：

- `0x48/0x4a/0x4c/0x4e` 的装备派生链已确认；职业和基础值 `0x37/0x39/0x3e` 的上游来源仍待确认。
- FDSHAP 地形表 `attr[2]/attr[3]` 的战斗修正或其他语义仍待确认。
- 当前证据不支持单一的完整 0x50 字节新建构造器：FDFIELD 模板、开场构造、完整存档恢复和派生重算是不同层。stage 0 的 profile、预算、HP 和 MP 捕获值仍只作为该场景验证夹具。

因此可以进入实际移动范围显示与移动提交，但不能把 stage 0 基线或未确认的战斗属性写成通用默认值。

## 3. 开发原则

1. 逻辑状态与画面演出分离。移动、伤害和回合结果必须能在无动画模式下验证。
2. 未确认字段保持原始字节，不根据数值分布猜测名称。
3. 地图移动继续使用 FDICON 六相位步态；FIGANI 只用于战斗演出。
4. DATO.DAT 当前只确认是立绘帧包，不能把它当作角色属性表。
5. FD2.TMP 的 cache class 随 stage 重建，不能作为职业或稳定单位类型。
6. 不直接翻译超大 AI 或战斗主循环；先从调用点和数据读写拆出小职责。
7. 每个里程碑保留一次性快速入口，并与 DOSBox 截图或行为记录对照。

## 4. 开发顺序

### M1：正式战场 session

当前进度：已完成。session 容器、stage 0 资源所有权、单位 roster、镜头与交互状态已经集中到 `src/field_game.c`；`--field-preview` 已改为复用该 session。`--field-play` 可直接进入正式循环，`--new-game-play` 会在 stage 32 → 31 → 0 过场结束后，把动态单位和镜头状态交给正式 session。

新增统一战场运行时，集中管理：

- FDFIELD、FDSHAP 和 FDICON 资源。
- 地图、metadata、placement 和地形表。
- `fd2_field_units` 单位表。
- 镜头、光标、当前选中单位。
- 当前阵营、回合数和交互阶段。
- idle/移动动画计时。

`--field-preview 0` 与 `--field-play 0` 已复用该 session。`--new-game-play` 会把 stage 0 过场结束后的坐标、朝向、隐藏状态、步态和镜头应用到正式单位表，同时保留模板中的阵营、group 和后续战斗属性。

验收标准：

- session 可以打开、渲染和关闭 stage 0，资源无泄漏。
- `--field-preview-once 0` 输出仍为 12 名单位、group 1/2 各 4 名。
- `--prologue-preview-once` 保持 stage 32 → 31 → 0 回归结果。
- session 已预留光标、选中单位、阵营和回合状态，但不提前实现战斗规则。

### M2：输入、光标和镜头

当前进度：键盘核心、原版选择框和战场常驻信息面板已完成。现已具备输入动作映射、格子光标、`2/11、2/6` 阈值镜头跟随、可见单位命中、我方单位确认和取消选择；选择框使用 FDOTHER[1] frame 0，并按原版顺序在单位之前绘制。`field_cell_info_panel_draw @0x3ff11` 的 FDOTHER[5] 面板已显示 terrain、A/D 修正、单位 sprite 与当前 HP；Enter 选中单位后的全屏详情页也已接入 DATO 立绘、属性框、装备框、FDTXT 名称和确认属性。原版不支持鼠标，因此不增加鼠标映射；完整 215 条物品表和四名初始玩家的 DOSBox 实机装备均已接入，12 帧三段开合视觉已复现；音效接入改为依赖 `docs/plans/audio.md` 的 A0/A1 前置工作，不在战场代码内单独建立临时播放器。

范围：

- [x] 建立战场输入动作，使 SDL 键值不直接进入 session 状态逻辑。
- [x] 从 FDOTHER[1] 解析 24×24、20 帧选择框 shape sheet，普通模式使用 frame 0。
- [x] 实现确认、取消和单位命中查询。
- [x] 按 `field_focus_move_to @0x37efe` 的左右 `2/11`、上下 `2/6` 阈值跟随镜头。
- [x] 在确认操作时输出调试坐标、terrain ID 和单位索引。
- [x] 确认 `field_selection_overlay_draw @0x374f0` 与 `field_selection_sprite_blit @0x3790b` 的模式、帧索引、透明规则和绘制顺序。
- [x] 接入 FDOTHER[5] frame 130/131/132/31..40，绘制正式格子/单位常驻信息面板。
- [x] 接入 Enter 选中单位后的全屏角色详情页：DATO 立绘、FDOTHER[5] frame 20/21、完整确认属性和装备列表。
- [x] 提取 `field_item_record_get @0x73ad0` 的 215×23 字节完整物品表，并复现类型、装备图标、主要效果和值。
- [x] 通过 DOSBox 运行时 `0x50` 字节记录确认其余三名初始玩家装备；flag `0x80` 的隐藏槽规范为 item `0xff`，不解释原版未初始化残值。
- [x] 复现 `field_unit_detail_transition_frame @0x3d61d` 的 12 帧左上、右上、下方三段开合视觉。
- [x] 完成音频 A0/A1：确认固定／动态 bank、11025 Hz U8 mono 默认路径和音乐控制链；建立 SDL3 device、stream、bus、null backend 与离线 PCM capture 入口。
- [x] 基于统一 PCM 后端复现打开 phase 11/5 与关闭 phase 0/7 的 SFX 5/6 时序，并通过 DOSBox capture 验证 11025 Hz 波形。

验收标准：

- 光标可遍历完整 24×24 地图且不会越界。
- 镜头在阈值处滚动，四周 4 px 战场边框保持正确。
- 单位格返回稳定 actor index，空格返回未命中。
- `--field-play-once` 不等待输入，并自动验证四向阈值、地图边界、单位命中和选择规则。

### M3：移动数据逆向与查询接口

当前进度：已完成。stage 0 已使用原版 profile 表和实机确认的单位移动字段计算真实移动范围。

已确认：

- FDSHAP `attr[1]` 是 `movement_cost_class`，用于索引单位的 20 字节 movement profile；它不是直接成本。
- 单位记录 `0x20` 是 movement profile ID，`0x3b` 是本次移动预算。
- 敌方所在格不可进入；敌方四邻控制区可进入但会停止继续传播。
- 友方单位格可穿过，但范围传播后不能作为终点。
- 隐藏单位不参与上述占格与控制区标记。
- `src/field_path.c` 已实现参数化、确定性的四邻域 Dijkstra 和路径重建。
- `src/field_move.c` 已实现地形成本类别、重复坐标单位、敌我占格和控制区 policy。
- `src/field_move_profile.c` 内置原版 29×20 字节 profile 表。表边界由 `DS:0x1646..0x1889` 确认；原始 580 字节位于 `FD2.EXE` 文件 offset `0x7a64a`，并与 DOSBox stage 0 运行时线性地址 `0x1b3646` 逐字节一致。
- DOSBox 新游戏 stage 0 单位表确认四名玩家 `(profile, budget, HP, MP)` 分别为 `(1,4,42,0)`、`(5,4,28,8)`、`(3,7,48,0)`、`(25,4,50,0)`；首批敌军 unit ID 96 为 `(7,4,28,0)`。
- `fd2_field_game_compute_move_range()` 已将 stage 资源、单位 profile、移动预算、占格与控制区连接到路径查询；快速入口会实际计算四名玩家范围。

仍需确认：

- FDSHAP `attr[2]/attr[3]` 的战斗地形或其他语义。
- 行动状态、职业和其他战斗属性字段。
- 上述 stage 0 基线值在原版新游戏初始化调用链中的通用表来源；这不再阻塞 stage 0 移动查询。

当前验收：

- [x] 每个已确认字段均有反编译读写点、原版静态表或 DOSBox 新游戏实机证据。
- [x] `ctest --test-dir src/build --output-on-failure` 覆盖加权绕路、预算边界、平价路径顺序、只可穿越/只可停留、敌方阻挡、控制区、友方穿越、隐藏单位、重复坐标和原版 profile 边界。
- [x] `--field-play-once 0` 与 `--new-game-play-once` 会计算 stage 0 四名玩家的实际范围并报告 `move-check=ok`。
- [x] Ghidra 标注、`docs/reverse-engineering/function-names.md` 与源码逆向注释已同步。

进入 M4 的前置条件已经满足：stage 0 玩家和首批敌军拥有来源明确的 movement profile、movement points、HP 和 MP，且证据来自原版静态表与新游戏 DOSBox 捕获，不使用后期存档替代。

### M4：移动范围和路径执行

当前进度：已完成。可达范围不使用独立图案，而是以 FDOTHER[3] 的 256 字节 LUT 重绘 FDSHAP 地形；20 相位索引来自 `DS:0x1a97`。旧调试内框与路径十字已移除，范围形状、移动和回退继续使用正式逻辑。

范围：

- [x] 计算可达范围，并按 `field_view_render_tiles @0x3710c` 对完整 24×24 地形与遮挡层应用动画 LUT。
- [x] 随光标重建目标格路径；原版只显示范围 LUT 与焦点选择框，不额外绘制路径线。
- [x] 处理地图边界、不可通行地形、敌方阻挡、友方穿越与控制区。
- [x] 支持确认移动，以及在临时指令状态取消并恢复移动前坐标与镜头。
- [x] 使用 FDICON 六相位逐格执行路径，并同步更新单位记录和镜头。

实现依据：

- `field_actor_path_play @0x3869c` 按方向码分派四个单格移动 helper。
- `field_actor_move_down_follow_camera @0x380be`、`field_actor_move_left_follow_camera @0x38221`、`field_actor_move_up_follow_camera @0x38399`、`field_actor_move_right_follow_camera @0x38529` 均使用六个 4 px 相位；达到 `2/11、2/6` 阈值时镜头同步滚动。
- SDL 版使用 `{1,2,1,0,1,2}` 步态帧和 55 ms 相位时长，完成一格后才提交唯一的单位格坐标。

当前验收：

- [x] 不可达格不能确认。
- [x] 移动选择取消，以及移动后的取消回退，均恢复原坐标、方向、镜头与交互状态。
- [x] 确认后每格更新方向和移动相位，结束时 `frame_phase=0`，不存在第二份单位坐标。
- [x] `--field-play-once 0` 与 `--new-game-play-once` 自动执行路径，独立覆盖四向单格动画与镜头阈值，并测试方向、终点、回退与最终提交，报告 `move-exec-check=ok`。
- [x] 本地 DOSBox 第一关截图确认选择框位于单位下方、范围表现为地形循环色调变化；SDL 使用同一 FDOTHER[1]/[3] 原始资源，并以 CTest 覆盖资源结构、相位表、基础/遮挡 LUT 透明差异、clipping 及地形 flags `0x04/0x08/0x10` 动画选帧。

### M5：回合骨架和 FDFIELD 事件

当前进度：最小回合、事件查询、stage 0 action `0..3` 的持久状态和演出层已完成。side 1/0 仍按单位顺序执行确定性待机；持久状态先事务化提交，交互模式随后从提交前快照重放镜头、移动、LMI1 和对话，未知事件仍记录为 `unhandled, skipped`。

已建立流程：

```text
玩家选择 → 移动/原地待机 → 标记已行动 → 下一单位
→ side 1 自动 AI → side 0 自动 AI → 回合数加一 → 玩家阶段
```

实现依据与范围：

- [x] `field_actor_mark_acted @0x38726` 确认并实现 flags bit `0x80` 已行动标志。
- [x] `field_all_actors_clear_acted @0x3874a` 确认阶段切换时清除进入阶段单位的标志。
- [x] 按 `field_turn_cycle_run @0x3f51f` 建立 phase `2 → 1 → 0 → turn+1 → 2` 顺序。
- [x] `field_turn_event_check @0x3fa27` 的 turn/action/phase 查询已在 `src/field_event.c` 实现。
- [x] `field_cell_event_lookup @0x38c58` 的 cell byte 2 事件 ID、地形 flags 与 match 参数过滤已实现；单格移动后查询 match 0，动作完成后查询 match 1。
- [x] `DS:0x1b91` 已确认为战场 action 表。object1 fixup target 直接作为 object1 code0 offset；DOSBox 运行时 `action 0 -> 0x190531` 与 `code0 0x24531` 机器码一致。
- [x] 回调 ABI 已确认：cdecl 单一 32 位 actor index 参数，回合事件传 0，调用者清理 4 字节栈，返回值不使用。
- [x] stage 0/31 action `0..3` 入口按真实 LE object1 修正为 dual `0x34531/0x3460b/0x34673/0x346cd`（code0 `0x24531/0x2460b/0x24673/0x246cd`）；action 1/2 分别加入 group 4/5，其余镜头、移动和对话调用也已静态确认。
- [x] stage 0 action `0..3` 的持久状态已接入：依次加入 group 3/7、4、5、6，执行 script 7/8、3、4、6，并更新镜头；失败时回滚，不留下部分状态。
- [x] action `0..3` 已接入逐格镜头、移动重放和 FDTXT fragment `11/3、4、5、6`；action 1/2 使用 FDOTHER[9] 的 12 帧 LMI1 登场特效，action 0/3 按原版直接追加 group。
- [x] 将 side 1/0 的自动待机替换为正式 AI 行动。

当前验收：

- [x] 原地确认可作为最小待机，单位完成后不能在同阶段重复选择。
- [x] 仅使用待机即可完成玩家、side 1、敌方阶段并进入下一回合。
- [x] stage 0 四条 turn event 在 `(3,1)/(4,0)/(5,0)/(6,1)` 条件下各执行一次，对应 action `0/1/2/3`；无动画验证保留 `presentation_deferred`，交互播放成功后清除该标记。
- [x] `field_event` CTest 覆盖回合/phase 匹配、cell ID、match 参数、地形 `0x60` 禁止位，以及移动脚本成功提交和失败回滚。
- [x] 快速入口自动推进至第 7 回合，验证 27 名 actor、group 3/7/4/5/6 数量、关键脚本终点与最终镜头 `(11,11)`；同时在独立 session 副本中验证四条演出顺序的终态，并实际读取、解码 action 1/2 使用的 12 帧 LMI1。快速模式不验证逐帧时序和像素对照。

### M5.5：原版输入系统

M6 之前先替换分散的 SDL 事件循环。原版不按 SDL 键名分派，而是通过 BIOS 键盘缓冲和 `INT 16h/AH=10h` 取得扫描码；完整证据和已确认键表见 `docs/systems/input.md`。

范围：

- 单一 SDL 事件所有者、有界 FIFO 和独立的窗口退出请求。
- 非消费式「任意键」检查，保持 ANI/AFM 跳过后键仍可由下一个 UI 读取的原版语义。
- 标题、通用二选一框、战场和调试预览使用各自的动作映射，不再直接调用 `SDL_PollEvent`。
- 保留 `SDL_EVENT_KEY_DOWN` 的 repeat 事件顺序；不延续当前战场循环丢弃 repeat 的行为。
- 可注入的纯 C 输入测试，覆盖扫描码规范化、FIFO、上下文限制和 title／field 基础映射。

验收标准：

- `src/` 中只有输入模块调用 `SDL_PollEvent`。
- 标题仅接受原版已确认的 Up/Down 与确认键，不再将 H/P/R/Escape 当作标题快捷键。
- 输入检查不会吞掉 ANI/片头后的首个菜单键。
- CTest 不依赖 SDL 窗口即可覆盖输入顺序与上下文映射。

### M6：无演出的确定性战斗

当前进度：已确认 stage 单位基础构造器、普通物理攻击的四个派生字段、装备重算链、武器范围和无演出核心公式，并新增 `src/field_unit_base.[ch]`、`src/field_combat.[ch]`、`src/field_attack.[ch]` 与 `src/field_unit_stats.[ch]`。一／二击序列、敌方邻接反击、HP 0 全表隐藏提交及原版四向图形指令菜单均已接入；RNG loader 初值和完整战斗表已捕获接入；敌方 phase 主动决策仍待完成。

已确认：

- [x] `field_unit_stage_template_append @0x35e6e` 已确认：重排 FDFIELD 模板，并按 unit ID/level 从角色成长表或敌军系数表生成 `0x1f/0x20/0x21/0x37/0x39/0x3e/0x3b/0x40..0x47`。
- [x] `field_unit_base` 已覆盖构造器确认的完整角色 ID `0..31` 与敌军 ID `68..135` 表；unit 96 level 2 与 DOSBox 捕获一致，unit 1 level 2 HP 与存档一致。
- [x] record `0x48/0x4a/0x4c/0x4e` 分别为 attack、defense、accuracy、evasion。
- [x] `field_unit_combat_stats_recompute @0x4096e（entry 0x40964）` 从 `0x37/0x39/0x3e` 和 8 个装备槽重算四项派生值；DOSBox CPU trace 确认共享尾部写入 `0x4e`。
- [x] `field_unit_stats` 以装备查询和状态缩放回调实现通用派生，不修改 movement、HP/MP 或未确认字节，失败时保持派生字段不变。
- [x] `field_physical_attack_resolve @0x43edb` 使用 `accuracy-evasion` 命中判定。
- [x] 暴击先将防御减半；伤害基础值为 `(attack-defense)*9/10`，浮动上限由 `base/9` 给出。
- [x] HP 扣减钳制到 0；纯逻辑模块同时返回公式伤害和实际 HP 扣减。
- [x] 随机数由调用方注入，固定序列可完全复现结果。
- [x] `field_rng_next @0x73df7` 已确认并以 `src/field_rng.[ch]` 实现：`rol16(state + 0x9014, 3)`；DOSBox debugger 在第一次调用前捕获 loader 初值 `0x7a18`，正式 field session 已采用该值。
- [x] 暴击基础值读取 `DS:0x24a8[record[0x20]-1]`；DOSBox debugger 已从 battlefield overlay 重定位地址 `0x1ae4a8` 捕获 profile 1..30 的完整 30 字节表。正式 session 直接读取该表，测试可按攻击者 callback 覆盖；反击交换双方后重新查询。武器 item `+0x09==4` 时再累加 `+0x0a`。item `+0x09==2` 的命中前 RNG 顺序和防御者 `+0x25=2..5` 状态写入也已接入；音效／闪烁演出仍省略。
- [x] `field_physical_attack_sequence @0x43a6a` 每次必定先消费一次 RNG；weapon effect 3 或固定 3% 判定触发两击。`field_physical_exchange @0x3a6a2` 在目标存活且 `field_counterattack_is_available @0x442f0` 满足 `+0x26==0`、距离 1、武器最小射程 1 时交换双方执行反击。SDL 已复现双击中止、共享 RNG 和反击不占行动标志。
- [x] 每击先按所在格 movement cost class 从 `DS:0x1a12/0x1a2a` 取攻防百分比，执行 `stat += stat*percent/100`；`field_unit_ignores_terrain_combat_modifier @0x44397` 确认 unit 28 强制使用地形，其他 unit 的 profile 19 或 race 4/5 跳过。DOSBox debugger 已从重定位地址 `0x1ada12/0x1ada2a` 捕获 class 0..5 完整表并接入。
- [x] `field_equipped_item_slot_find @0x40a51`、`field_unit_item_id_at @0x40936` 与 `field_target_range_build @0x39a2c` 已确认普通攻击使用首件已装备武器 item record `+0x0b/+0x0c` 的最小／最大范围，以 profile 0 传播；side filter 映射为 `0→side 0`、`1→非 0`、`2→side 1`、`3→side 2`，玩家攻击使用 filter 0。`src/field_attack.[ch]` 和 `field_attack_test` 覆盖短兵／刺矛范围、最小距离、地形成本阻断和阵营过滤。
- [x] `fd2_field_game` 已提供可测试覆盖的目标规则、注入 RNG、`COMMAND → TARGETING → COMMAND → BROWSE` 攻击状态与 HP 提交接口。`field_physical_attack_resolve @0x43edb` 本体只写 `+0x40`；exchange 后的 `field_defeated_units_finalize @0x42d83` 会把全表所有 HP 0 actor 的 `+0x05` 精确写为 1。`fd2_field_game_attack_test` 用固定 RNG、按单位注入基础暴击率，验证非法目标不消耗 RNG、取消 targeting、已行动攻击者拒绝结算、命中、HP 归零、全表隐藏、死亡反击者行动完成、未命中、状态复位、地形攻防、双击和敌方反击；`--field-play-once 0` 再以 stage 0 roster 和正式武器范围做集成验证。
- [x] `--field-play 0` 在移动后打开 FDOTHER[2] 四向指令菜单：上／左／右／下依次为 attack/magic/item/wait。attack 仅在存在合法目标时可选，确认后进入 `TARGETING`；wait 直接完成行动；magic/item 的原版身份与禁用条件已确认，但执行状态机未完成，当前显示原版 disabled 帧。

范围：

- [x] 移动、攻击、待机指令菜单。
- [x] 攻击范围和合法目标过滤。
- 命中、伤害、HP 扣减、死亡和行动完成。
- 经验和升级先定义接口，确认公式后实现。

需要先定位真正的职业、属性、装备、技能和伤害数据来源；不能继续沿用「DATO.DAT 是属性表」的旧假设。

验收标准：

- [x] 固定输入和随机序列时，攻击核心结果可自动断言。
- [x] HP 不下溢；exchange 结束后全表 HP 0 actor 的 flags 精确写 1。死亡演出仍延后到 M8。
- [x] 已行动单位不能重复行动。
- [x] 至少完成一次玩家主动攻击和一次敌方邻接反击；敌方 phase 的移动／主动攻击决策归入 M7。

### M6.5：原版图形指令菜单

当前进度：逻辑、资源、输入和绘制已完成；DOSBox 战场菜单截图的逐像素对照仍待补充。

- [x] `field_player_command_execute @0x3dfa0` 的四项表已确认为 attack/magic/item/wait，方向位置固定为上／左／右／下。
- [x] FDOTHER[2] 已确认为 78 帧 raw indexed offset-table sheet；前 12 帧按四项命令的 normal/highlight/disabled 排列。
- [x] 打开和关闭动画均使用四相位与 5／6／6／5 px 步长，并播放 FDOTHER[31] SFX 8；关闭使用机器码确认的独立初值，不把打开坐标简单倒序。
- [x] 指令选择状态由 `fd2_field_game` 管理；菜单绘制不修改单位、HP、行动标志或 RNG。
- [x] attack 继续复用已验证的范围、目标、RNG、反击和行动完成接口；wait 直接完成行动；取消恢复移动前坐标、方向和镜头。
- [x] magic 和 item 的原版执行入口及禁用条件已确认。执行状态机尚未移植，当前保留图标并使用 disabled 帧，不能确认。
- [x] `field_command_test` 覆盖资源解析、帧状态、四方向坐标、展开／收起相位和禁用选择；输入测试覆盖 command/targeting 的确认与取消上下文；`field_game_attack_test` 覆盖 attack/wait 分派、目标取消重建菜单、无合法目标和 RNG 不推进。
- [ ] 补充原版战场菜单截图，与 SDL 最终位置、调色板和四相位动画逐帧对照。

下一开发优先级回到 M7 的敌方 phase AI、胜负与关卡推进。

### M7：敌方 AI、胜负和关卡推进

当前进度：M7 查询／计划层、物理事务、可确认的 magic/item 效果事务、mode-2 witness、mode-1 长路径 fallback、主要 behavior、side 1/0 自动 phase 和 stage 0 结果门已接入。自动 phase 在 battle result 终止后不再推进；特殊 magic/item handler 和跨关卡推进仍保持显式失败；behavior 2 的 candidate-query／恢复顺序、behavior 5 的 cell action 提交、behavior 7 到达后隐藏 actor、behavior 11 的 magic → physical 连续执行已接入。

已确认：

- [x] `field_turn_cycle_run @0x3f51f` 在 phase event/status 处理后分别调用 `field_side1_phase_execute @0x42a1f` 与 `field_side0_phase_execute @0x42ace`。
- [x] `field_ai_unit_execute @0x38cb3` 使用 actor record `+0x34 & 0x0f` 分派 behavior `0..5、7..11`；`+0x35/+0x36` 是随模式解释的参数。stage 0 初始 group 1/2 均为 mode 0、参数 0/0。
- [x] `field_ai_move_toward_nearest @0x390b0` 的阵营过滤、曼哈顿距离与首项平价规则已在 `src/field_ai.[ch]` 实现。
- [x] `field_ai_move_toward_cell @0x39d8c` 最终按行优先扫描可停留格，先最小化曼哈顿距离，再最小化 `abs(dx-abs(dy))`；纯目的地查询已实现并由 CTest 覆盖。
- [x] `--field-play-once` 对 stage 0 八名初始敌军执行 mode/参数、最近目标、真实移动范围与目的地查询，报告 `ai-query-check=ok`；快速验证会快照并比较单位与 phase 状态，纯查询 API 不接收 RNG。

后续范围：

- [x] M7.1：拆分 mode 0 的 `field_ai_try_attack @0x3a104` 与三类候选评分。corrected code0 确认 `field_ai_physical_candidate_evaluate @0x3944b` 枚举「可达目的地 × 武器范围内目标」，以优先级 `0/8/18` 和次分数作稳定比较，且次分数会在同一 destination 内跨 target 累积；`field_ai_magic_candidate_evaluate @0x3ab9e`、`field_ai_item_candidate_evaluate @0x3a892` 分别构造法术／物品候选。分派阈值是任一 signed score `>=6`；B==C>A 选法术，三项完全相等时返回成功但不调用提交 helper。SDL 已加入物理候选纯查询与三分数分派函数，并用 stage0 快速校验和独立测试锁定只读性、平价稳定性和阈值怪癖。
- [x] M7.2：复现法术 `field_ai_magic_targets_score @0x3ad8b`、`field_ai_magic_zero_byte_targets_score @0x3afb6` 与物品 `field_ai_item_targets_score @0x3aa94`。SDL 内置 `field_magic_record_get @0x4e866` 的 36×7 字节表，并实现已知法术位图、MP、施法中心、目标范围、物品普通／轴线范围及稳定候选比较。`text_id==0` 的法术伤害项按原版 `1.5` 倍评分；评分和候选查询不修改单位、范围或 RNG。
- [x] M7.3a：复现 `field_ai_try_attack @0x3a104` 的数据相关平价门控，并新增 `fd2_field_game_commit_ai_physical()` 显式事务。提交前按当前状态重算法定移动范围和物理候选，要求计划完全一致，再重建方向路径、复用 `field_actor_path_play @0x3869c` 的六相位 SDL 路径播放器，最后进入共享 `field_physical_exchange @0x3a6a2`。AI 路径不刷新玩家 COMMAND 菜单；失败的 stale plan 和 callback 在移动／RNG／呈现前拒绝；受限 mode0 只在物理分支已确认时执行。
- [x] M7.3b：移植 `0x3a525` 已确认的 IDs `0..21、25..27` effect 核心、`0x3a269→正确 dual 0x20c6f` 的 item code `5/13/20/21/24`、`0x39335` mode-2 witness 与 `0x39d8c` mode-1 长路径。item code `20/21/24` 的 value 是 magic ID，最终进入同一 damage core；ID 22 的 50% gate／固定伤害／状态已接入；IDs `23/24、28..35` 的 handler 仍包含剧情资源/专用状态机，当前在 RNG 和状态修改前显式拒绝，并有回归测试。
- [x] 核对 `field_ai_magic_action_execute @0x3a525` 的常规伤害分派：record `+0x06` 是目标阵营 filter；IDs `0..9` 即使进入原 `field_magic_battle_animation @0x55115`，也会由动画主体通过 `DS:0x24c6` effect 表逐目标调用同一伤害 core。只有 IDs `10..12` 在命中 RNG 前额外执行 `0x1f183` 免疫门。SDL 测试已锁定 ID 9 不受该门影响、ID 10 免疫不消费 RNG，以及 unit ID 28 的例外。
- [x] 核对 item code `20/21/24` 的包装与消费边界：正确 dual `0x20c6f` 中 code `20/24` 共用 `0x20f6d`，code `21` 使用 `0x2111a`；三者都把 item value 作为 magic ID 逐目标调用 `0x1c75e`。统一 inventory remove 仅位于 code `5` 分支，code `20/21/24` 均保留原 slot。测试覆盖 code `20/21/24` 的伤害、RNG 和非消耗行为。
- [x] 替换 side 1/0 自动待机；按 raw actor index 调度，并保持玩家 command 状态机隔离。
- [x] 接入已确认的 phase 状态前置处理：进入每个 side phase 时，对 raw actor index 0 起的可见本阵营 actor 按 `maxHP/10` 执行中毒伤害，并递减 record `+0x22..+0x27` 六个状态计时；计时归零时复用 `field_unit_combat_stats_recompute @0x4096e`，按 DS:0x018d 的 `1.15` nearest-even 比例恢复 AP/DP 派生值。死亡统一写入 hidden/acted flags。该路径不消费 RNG，测试覆盖 side 隔离、属性恢复和状态边界。
- [x] 修正逐 actor 自动 phase 的 acted 生命周期：side 2→1 不清除全表 acted；side 1→0 在 side 0 scheduler 首次进入时复现全表清除；side 0→新回合 side 2 在进入前清除全表 acted。这样不会在 SDL 的分步 scheduler 中擦除上一 actor 的 common tail。
- [x] 接入 AI 入口 `flags & 0x05` gate：hidden bit `0x01` 与 AI-ineligible bit `0x04` 的单位不会进入 side 1/0 自动行动；玩家可行动性与 battle result 仍只按 hidden/HP 语义判断。
- [x] 修正 AI cell event 时机：玩家路径每步使用 match `0`，玩家行动结束与 AI common tail 使用 match `1`；AI 移动复用 SDL path player 时丢弃沿途 match `0` notice，只在最终格记录一次 common-tail cell event。测试覆盖两步路径中间格／最终格分离。
- [x] 锁定 AI cell handler 的 outer-loop 分派顺序：side 1/0 每个 actor 前把 `DS:0x1a8f` 复位为 `0xff`，AI common tail 只写事件码；AI 返回后 outer side loop 才调用 `DS:0x1b91[event_code](actor_index)`，随后调用 stage handler。SDL 尚未恢复 stage-private cell handlers；命中未知 handler 时保留已提交动作和 acted，追加 unhandled notice 并显式停机，不能继续下一个 actor。
- [x] 核对状态伤害法术共享 core：IDs `22/26/27` 的 wrappers 只分别选择 `+0x27/+0x25/+0x26` 状态 byte，随后共用 `0x22cda` 的状态非零门、profile `25/26` 免疫、50% gate、固定基础伤害 `10` 和 duration RNG。测试覆盖成功、已有状态、profile 免疫与 50% miss 的 RNG 边界。
- [x] 修正 AI common tail 的全表方向清理：`field_all_actor_directions_reset @0x386f8`（corrected dual `0x134e4`）确实遍历完整 actor 表写 `record +0x03=0`，并在原版延迟 20 ms；该字段是默认朝下的方向，不是独立 selection 缓冲。SDL 在 cell lookup 和 mark acted 后同步归零全部单位方向，测试覆盖移动 actor 和非当前 actor。
- [x] 锁定 stage handler 的 raw-actor 边界：side 1/0 两轮均在每个 raw actor 后调用 `DS:0x1b19[stage](actor_index)`，即使 actor 因阵营、hidden、acted、状态或 AI gate 被跳过；cell handler 位于其前。SDL 正式 session 只开放 stage 0，并在 scheduler 入口及已提交动作后刷新已确认的 stage 0 全表结果门；其他 stage 仍不得用通用全灭判断冒充私有脚本。
- [x] 锁定 behavior 8 的双层返回语义：AI core 在 corrected dual `0x13d9a` 直接返回，不执行 cell lookup、acted 或全表方向归零；outer side loop 仍执行 stage handler 并继续 raw actor 扫描。SDL 依靠单调 phase cursor 越过该 unacted actor，side 0 第二轮不会重复调度或卡住。测试覆盖 side 1/0、match 1 格、方向保持、RNG 不变与 phase 推进。
- [x] 补齐辅助状态法术边界：IDs `17/18/19` 仅在各自状态 byte 为零时消费一次 duration RNG，并分别增加 attack、defense 或 accuracy/evasion；已有状态不续期、不叠加且不消费 RNG，HIT/EV 的 16 位加法保留回绕。IDs `20/21` 仅清除 `+0x25/+0x26` 非零状态，ID `25` 仅清 acted bit并保留其他 flags；三者均不消费 RNG。测试覆盖成功、noop、字段隔离、flags 保留和溢出。
- [x] 锁定恢复法术 IDs `13..16`：对应 `DS:0x1d01` wrappers 最终逐目标调用 `0x1c8ed→0x1c916`，直接使用 magic record `u16 +0`；不做命中判定或 profile 免疫，每个目标消费一次 spread RNG，按与恢复物品相同的两段整数截断公式增加 HP并 cap 到 maxHP。满 HP 仍消费 RNG。测试覆盖 IDs 13/14 数值、15/16 cap和满 HP RNG。
- [x] 锁定恢复物品 code `5/13`：两者经 `0x211a4` 逐目标调用同一恢复 core，每个目标（包括满 HP）消费一次 spread RNG。code 13 保留原物品槽；code 5 在效果后调用 `field_unit_inventory_slot_remove @0x1b8e7`，左移后续 flag/item 双字节槽，并只将末槽 flag 写为 `0x80`，保留 stale item ID。测试覆盖多目标 RNG、满 HP、code 13 非消耗和 code 5 中间槽压紧／stale ID。
- [x] 复用已确认的路径和战斗计算，实现目标选择、移动、攻击、可确认法术／物品或恢复；behavior 11 的 magic→physical 在 magic 已消费 RNG 后若 physical 失败则保留 magic 结果并完成 common tail，不伪造 RNG 回滚。
- [x] 接入已确认的 stage 0 基础结束门：`DS:0x1b19[0]` 指向正确 dual `0x205b4`，其可观察条件为「存在可见 side 0 actor 时继续；无可见 side 0 actor 时结束；stage 0 主角 actor 0 隐藏/HP 0 时失败」。SDL 的 `battle_result` 已复现该 stage 0 门及其他 stage 的通用 side 0/2 全灭基础结果；这仍不包含所有剧情对白、经验结算和下一关初始化。
- [ ] 接入原版 stage 结束 callback、转场与下一关资源初始化；当前 session 仍只允许 stage 0，不能猜测后续关卡 roster。

验收标准：

- 第一关可以持续运行到胜利或失败。
- AI 不越界、不进入非法地形、不与单位重叠。
- 相同状态和随机种子产生相同决策。

### M8：战场系统菜单、存档和退出流程

当前优先级：最高，先于 FIGANI 战斗演出。原版的单位 action、空焦点、
次级菜单和设置页共用 FDOTHER[2] 及通用四向 open/input/close/draw primitive；
SDL 必须共享这些资源与绘制逻辑，但不能复用 attack/magic/item/wait 的 enum、
业务分派或 `FD2_FIELD_INTERACTION_COMMAND` 状态。`src/save.c` 也只完成整文件
XOR 和基础 slot 读取，尚未接入正式战场 session。

#### M8.0：确认原版入口、资源和分派

- [x] 从 corrected code0 `0x17e7`（dual `0x117e7`）控制器追踪焦点确认、
  F1/Page Up、G、Esc/Z/数字小键盘 5：Enter/Space 且焦点无可见 actor 时
  唯一调用空焦点菜单；F1/Page Up 是独立缩放战术图页面。
- [x] 确认通用四向菜单按上／左／右／下选择 selector `0..3`，确认／取消键、
  open/close primitive、SFX 8，以及 FDOTHER[2] 78 帧 command ID 规则。
  可见标签与逐帧 DOSBox 对照仍待人工证据。
- [x] 定位空焦点 dispatcher code0 `0x6f55`（dual `0x16f55`）、次级
  dispatcher code0 `0x9df7`（dual `0x19df7`）和设置页 code0 `0x728c`
  （dual `0x1728c`）。次级 selector 1/2/3 已分别确认存档、读档和离开战场；
  selector 0 的 code0 `0xb1e7` 已确认为 12 相位展开／收起的全屏部队状态
  总览：每页绘制 stage、turn、资源值、三阵营单位计数及逐行 actor 属性。
  用户可见标签和精确页名仍待 DOSBox 人工证据，因此代码保持中性命名。
- [ ] 由开发者手工操作 DOSBox，提供空地触发、各菜单项选中、存档槽、
  读档槽、配置页和退出确认的截图或操作结果；自动化不得代替该交互验证。

#### M8.1：独立系统菜单状态机

- [x] 新增独立的 `field_system_menu` 模块、`SYSTEM_MENU` 交互状态和输入
  上下文；`FD2_FIELD_INTERACTION_COMMAND` 继续只表示单位 action 菜单。
- [x] 仅在玩家回合、browse 且焦点无可见 actor 时打开；支持方向选择、
  分层取消和返回战场。接入 FDTXT `0x19a..0x1a4` 的确认片段索引及独立
  yes/no 选择页；确认前不执行动作。Save／Load 已连接正式后端；march、
  结束回合和 leave 仍只报告 pending，不执行不可逆状态修改。
- [x] 将 FDOTHER[2] loader 扩展到完整 78 帧，并按原版 command ID 和
  四向坐标绘制；frame `48/49/51/52` 的 `24×16` 例外已显式校验。
- [x] 增加无窗口菜单和输入测试，覆盖两层 command ID、options bit 图标、
  禁用项、分层取消和动作分派；宿主 `SDL_EVENT_QUIT` 保持独立路径。
- [x] 补充正式 session 集成测试：玩家 phase／browse／焦点无可见 actor
  入口 gate，parent／secondary／options 分层取消，以及未接后端动作对单位、
  回合／phase、事件、RNG 和镜头无副作用；options 只修改持久配置 owner。
- [x] 接入系统菜单独立 highlight／animation 状态、通用 command-ID
  四相位开合绘制和 FDOTHER[31] SFX 8；parent／secondary／options 页面
  切换遵循 close → dispatcher → open 边界。
- [x] 接入 code0 `0x9df7` 的动态 disabled 条件：读取项要求
  `FD2.SAV` 严格资源可用，存档项在任一未隐藏 actor 带 acted `0x80` 时禁用；
  hidden actor 的 stale acted 不触发该门，并由正式 session 集成测试覆盖。

#### M8.2：存档恢复和写回

- [x] 确认 0x59cb 文件大小、末尾 4 字节明文 checksum、四个 slot、
  `0x312b + slot*0xa28` 边界、slot 3 尾 4 字节与 checksum 重叠，以及
  `stage_id/unit_count == 0xff` 无效标记。
  slot 尾部 `0x28` 字节的细分语义仍待恢复。
- [x] 实现保留未知字节的整文件读改写后端：严格验证长度与 checksum，
  解密完整文件，只更新目标 slot 的已提供记录／完整 meta，重算 checksum 后
  对称加密；同目录临时文件采用独占创建，拒绝预置符号链接并避免并发 writer
  共用固定 `.tmp` 路径。单元测试逐字比较所有非目标区域。
- [x] 恢复活动战场快照区 `0x12a3/0x30a3/0x30c3`，并在 session 与
  存档之间无损导入／导出完整 0x50 字节单位记录、stage、turn、镜头、
  焦点和已确认 cell action 字节；未知 meta／unit tail 保留。原版 loader
  随后直接进入玩家控制器，因此导入回到 side 2 browse；SDL 的分步
  phase cursor/pass 归零。原版快照不保存 RNG，并在进入控制器前按计时器
  低 8 位扰动随机流；读档不伪造精确 RNG checkpoint。
- [x] 地图渲染继续直接按稳定 unit ID 使用 FDICON；导入时忽略持久记录中
  仅供 FD2.TMP 使用的 cache class，不建立持久 TMP 依赖。
- [x] 读档 codec 在全部 stage／unit count／坐标／镜头验证通过后才提交；
  失败时当前 session 逐字不变。成功后清理旧的选择、路径、
  菜单和临时演出状态，再从恢复状态继续战场循环。
- [x] 将正式 `fd2_field_play_run` 的 Save／Load 确认动作连接到活动快照
  后端：宿主默认使用工作目录下的 `FD2.SAV`，可用环境变量
  `FD2SDL_SAVE_PATH` 显式覆盖。保存结果显示 FDTXT `0x19b/0x19c`，
  读取成功显示 `0x19e`；不存在、损坏或当前 stage 不可导入的文件会禁用
  Load，失败不会改写当前 session 或原文件。

验收标准：

- 已知存档解密再加密后逐字节一致；只更新目标 slot 时，其余字节不变。
- 截断文件、错误密码／校验、无效 slot 和越界单位数均安全拒绝。
- 加载后 stage、单位数、坐标、方向、HP、MP、flags、回合和 phase
  与原始记录一致。
- 在战场中执行「保存 → 改变状态 → 读取」后能恢复已确认的单位、回合、
  镜头、焦点和 cell action 状态；原版不保存 RNG，因此不以读档后的
  完全相同随机序列作为兼容验收条件。

#### M8.3：配置、退出和标题流程

- [x] 按 options loop @code0 `0x735b..0x73e2` 实现四项即时 bit toggle
  和 command-ID 重算；第一项已确认控制音乐音量，第二项控制 SFX 播放门。
  四字节随活动快照保存；原版手工 slot 对应映射已由
  code0 `0x16064..0x16098/0x19757..0x19791` 确认；SDL 手工槽 codec 已恢复
  四个配置 owner，后三项可见名称仍待人工确认。
- [x] 将 leave-battle 确认结果映射为独立 `FD2_FIELD_PLAY_RETURN_TITLE`，
  与 SDL 窗口关闭／Escape 的宿主退出结果分离；原版调用
  `0x15977(-1,1)` 后返回 `-1` 的应用级去向仍待完整入口调用图核验。
- [x] 将普通标题 action 菜单分派到新游戏、活动快照读档和退出；依据
  code0 `0xf894/0x15ebb`，无可用活动快照时只显示两项，有快照时插入中间
  Load；保留现有
  `--new-game-play`、`--field-play` 快速入口作为回归工具。标题读档先验证
  完整 FD2.SAV 和 stage，当前只接受 stage 0，再由正式 field session 导入；
  该 Load 只恢复 SDL 活动快照，不冒充原版四槽手工 Load。
- [x] 逆向确认战场内四槽手工 Save／Load：code0 `0x19300` selector
  `0..3` 分派部队页、手工 Save、手工 Load、离开确认；`0x19bcb` 是独立的
  纵向四槽 picker。手工 Save 无条件复制完整 `0xa00` 单位区，手工 Load 以
  `slot+0xa00 == 0xff` 判空并在成功后重建 runtime cache；SDL 已提供完整
  slot codec，并接入独立纵向 picker 状态机、输入上下文、事务后端和空槽边界
  测试；成功片段使用原版 `0x294/0x1de`。该路径属于独立 dispatcher
  `0x19300`，不能替换 secondary `0x9df7` 的活动快照 Save／Load；SDL 当前将
  手工槽作为已验证兼容 API 保留，正式可见入口待 DOSBox 人工证据。原版
  FD2.TMP cache 由 SDL 的完整 FDICON bank 替代；调色板、装饰帧和配置最终
  可见效果仍待人工对照。

验收标准：

- 系统菜单四类动作均由正式 `fd2_field_play_run` 到达，不依赖调试命令行。
- 取消配置／退出确认后，除 system-menu 的 page/selector/highlight/timing
  等临时 UI 字段外，单位记录、回合／phase、事件、RNG、镜头和持久配置
  逐字段不变。
- 标题新游戏可以完成 stage 32 → 31 → 0 交接并进入正式战场；标题读档
  可以进入存档指定 stage，遇到尚未支持的 stage 时明确拒绝而不是套用 stage 0。

### M9：FIGANI 战斗演出

范围：

- 安全扫描 408 个 FIGANI 条目。
- 确认单位、动作和 entry 的映射。
- 实现攻击、受击和施法的背景、调色板与帧合成。
- 演出结束后返回地图。

验收标准：

- 跳过演出和完整播放产生相同战斗结果。
- 动画解析失败时可回退到无演出流程，不破坏战场状态。
- 至少一组攻击和受击动作与 DOSBox 顺序一致。

## 5. 自动验证矩阵

每个里程碑至少保留以下验证：

```bash
cmake --build src/build --parallel
SDL_VIDEODRIVER=dummy ./src/fd2sdl --field-preview-once 0
SDL_VIDEODRIVER=dummy ./src/fd2sdl --prologue-preview-once
```

纯逻辑模块应补充不依赖 SDL 窗口的测试：

- 单位占格和命中查询。
- 地形成本与可达范围。
- 路径重建与取消恢复。
- 回合状态转换和事件幂等性。
- 固定随机种子的战斗计算。
- 存档导入、导出和异常输入。

交互里程碑还需要 DOSBox 对照：

- 镜头阈值和四周边框。
- 光标、移动范围颜色和地形信息。
- 单位移动节奏、遮挡和绘制顺序。
- 事件触发时点、战斗演出和胜负流程。
