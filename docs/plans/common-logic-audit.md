# 通用逻辑完整性审计与缺口矩阵

2026-11-13当前入口：G1—G5及§13的原完成域保留，完成摘要和历史收据见[归档](completed-stages.md)。本轮源码／既有snapshot测试确认结果HP门差异，独立修正归[当前计划S0](development.md#4-新优先级与依赖)；审计表达同步归S1，新图／别名有效域和未来身份能力归S2，各只有一个所属阶段，不重新打开旧G。来源分类数不是当前缺口或完成率，原版Load动态后果仍待验证；本页不另排优先级。

2026-11-09 的只读审计基线为 `c1b6598`。本文保留原审计发现，并登记其后 G1—G5 的实现、证据与验收；**开发顺序只由 [development.md §4](development.md#4-新优先级与依赖) 决定**。原审计阶段只交付计划，后续实施已另获授权；不能将基线缺口当作当前待办，也不据此开放未知玩法。

实施授权补记（2026-11-09—10）：计划 `b4c4bfa` 交付后，用户已授权在同一任务、同一工作树连续完成 G1—G5。本页原「本次」为审计历史口径，不限制后续已授权开发。§3 保留原审计基线，后续实现与证据按 §9—§13 更新，不将已补缺口重新当作待实现。G1 新证据见[通用战斗语义](../reverse-engineering/common-battle-semantics.md)、[准备服务](../reverse-engineering/common-preparation-semantics.md)、[共享控制与呈现](../reverse-engineering/common-runtime-semantics.md)；静态合同、实际实现、原版运行和阶段完成分别记账。

**当前结论（2026-11-11）：G5 的代码修复、当前 Windows 统一回归与 C01—C28 来源域内通用完成门已闭合，最终依据见 [§13](#13-g5-通用完成门与最终验收2026-11-11)。** §3 是原审计基线，§9—§12 保留各阶段／中间候选的历史状态，不把旧「待审」「进行中」重新当作当前阻塞。S 尚未启动；完整 G5 提交、安全本地集成与重建以实际 Git 和 `build/g5/local-integration.json` 为准，不在本文预填提交哈希。

## 1. 审计范围与边界

通用逻辑包括跨关复用的领域规则、领域操作、交互流程及共享运行服务。关卡专用内容包括地图、单位编成、触发绑定、剧情顺序、条件参数及确有证据的关卡特例。两者按语义划分，不按文件目录或是否数据化划分。

- `flows/progress.json` 的经验、升级、学习、状态到期与 ACK 写点是通用规则；`flows/loot.json` 的空槽、满包换物、取消和装备重算也是通用规则。
- `flows/stage.json` 同时含支持能力、结果规则及特定关卡绑定；`flows/scene.json` 同时含可复用动作／等待与自然开场、具体结算编排。不能把全部 flow 留到关卡阶段。
- 同一类行动的目标枚举、费用、命中／效果、死亡、奖励、acted、pending 格事件及演出是通用能力；具体关卡调用谁、在哪一回合调用、说哪段文字属于绑定。
- stage 0／1 可继续作为通用机制的代表性夹具。后期关卡若提供转职、特殊范围或条件的唯一证据，允许提前做最小只读取证或隔离夹具；不顺带交付该关完整剧情，也不因此提前开放该关。
- 现有逐 stage／unit ID 判断不一概删除：先辨明原版固有特例、当前 SDL 验收门、真正缺失的可复用规则。后两者必须登记，不能改成数据参数后就宣称机制已补齐。

## 2. 核实的基线与证据口径

本工作树从 `88b47d7` 快进到本地 `master@c1b6598` 后审计。Git 已核实 SD1 `6b24b33`、SD2 `ecb154a`、SD3 `c1b6598` 均在本地 master；没有沿用 SD3 等待显示或未集成的中途状态。只读集成收据为 `E:/Projects/fd2sdl/.cindy-worktrees/tranquil-mendeleev/build/sd3-local-integration.json`：主程序已重建，主树用户 `FD2.SAV` 保留，未远程推送。收据是既有结果，不是本次重新运行游戏的证明。

已读取 `AGENTS.md`、`README.md`、现有全部计划与相关阶段记录，并按模块核对 session、输入、数据格式及工具规范。本工作树未发现 `CLAUDE.md` 或 `.cindy/project-knowledge/TOC.md`，不补造其内容。源码、测试判据和冷定义的覆盖登记如下；函数地址沿用源码已有 canonical 登记，本次不新命名函数、不更改生成伪 C。

### 2.1 来源复核

本次实际执行既有 `tools/static_data/audit_stage_data.py`，只读使用主项目 `original_game/`，输出在本工作树 ignored 的 `build/common-logic-audit/source-inventory.json`。结构检查返回成功，但 `semantic_complete=false`，各项与 SD3 记录一致：

| 来源集合 | 核实结果 | 不能据此推导的结论 |
|---|---|---|
| 原资源 | 99 项／33 组；turn、lookup、action 各 528 槽；1,899 模板、2,322 placements；12 条 placement 未配对 | 结构读完不等于所有单位初始化或事件语义已确认 |
| 分发表 | 90 callback＋result／entry／victory 各 30＝180 主槽，另有独立 28 magic 槽 | 208 槽不是 208 种独立机制；函数 chunk 覆盖不等于 CFG／业务语义覆盖 |
| 移动脚本 | 106 指针槽、2,898 byte payload；36 条 runtime 支持，70 条仅结构提取 | 不可把原始 actor 字段到 83、direction 5 自动判为坏数据或提高容量 |
| 保守潜在引用 | 1,171 条，含重复：51 当前支持定义、468 已知类型／定义未开放、652 未绑定或未解码，其中 450 条默认 turn255 候选 | 这些数不是独立功能数或覆盖率；未开放不一定仅缺绑定，未知不等于关卡专用 |
| 严格支持 | 已解码引用错误 0；排除默认候选后的绑定闭合 stage 0／1；严格支持 stage 集合为空 | turn255 可命中性未决；现有受支持场景通过不证明全域可达语义完整 |

复现命令及上述字段的准确含义见[冷定义工具](../tools/static-data.md#批量来源与覆盖审计)。原始报告含原版字节，不提交报告或数据切片。这里的结构复核不执行 IDA、原机器码、SDL 或 DOSBox。

### 2.2 状态和缺口分类

矩阵的「已实现」只覆盖同格所列范围。状态可并列：**部分实现、未实现、受支持门限制、已实现但证据不足、尚未审计**。缺口分别标为 **L 逻辑、P 呈现／正式入口接线、S 原版语义未确认、T 测试证据、D 仅场景数据／绑定**；同项可含多类。S 不得改写成 L 已知需求，T 不得冒充尚无算法，D 必须有执行器和语义已足够的依据。

现有证据统一引用 [阶段记录](stage-log.md)：B1—B7、A5—A8、R1、SD1—SD3 的各自条目及原候选来源；不能把累计通过改写为本次或最终二进制单轮全过。矩阵中的测试名是 `src/tests/<name>_test.c` 或已注明的 Python 文件及其现有判据，不表示本次执行过。

## 3. 通用机制矩阵

**本节为 2026-11-09 的审计基线。** 保留当时缺口与验收要求，后续实现状态见 §9—§12 及其实际收据；已补齐项不重新排为待实现。

后续按下列稳定 ID 关联取证、实现与验收。每项先列当前证据与状态，再列具体缺口、依赖和通过条件；原工作项是历史归属，不重新打开其已完成范围。C 编号之间的互引表示规则／owner 合同和联验范围，不是要求整项先完成的硬依赖；阶段硬依赖只按总计划。已有 C13—C17 基线可直接复用，新增同步或原语随 G2／G3 交付，不形成实现循环。

### 3.1 资源、冷定义与单位基础

| ID／通用能力 | 文件、关键符号与现有依据 | 基线实现与真实状态 | 缺口、依赖、原工作项与可执行验收 |
|---|---|---|---|
| C01 资源与二进制接口 | `src/archive.c`、`image.c`、`font.c`、`field.c`、`save.c`；`fd2_field_map_open_mem`；`resource_test`、`save_test`；数据格式 §1／2 | DAT／嵌套、RLE、字体、地图和存档 codec 已实现，含截断／损坏输入检查；不是所有资源消费组合均验过 | **T/S**：新增能力引用的资源子格式与边界必须随所属阶段取证；不重写已确认解包器。原基线／SD1；坏长度、索引、嵌套及分配失败不得返回成功或改输出，合法资源独立解码一致。尚未消费资源不自动认定需要新系统 |
| C02 冷定义、生成与通用流程 ABI | `data/static/manifest.json`、`tables/`、`flows/`；`src/static_data.c`、`static_flow.h`；`tools/static_data/generate.py`；`test_static_data.py`、`test_static_flows.py` | SD1—SD3 完成当前 79 表、四域 30 流程／518 节点、6 共享绑定；定义和领域算法分离；schema／端口／外部 label 有约束 | **S/T/D**：新增通用语义优先复用领域节点；确需新原语时扩 ABI／独立来源与取消失败测试，禁止大 native handler／任意 VM。原 SD；变更一个分支必须改变实际执行轨迹，未知 op／出口／必需引用构建或预检拒绝，热 owner／SAV 布局不变 |
| C03 单位构造、装备派生与成长 | `field_unit_base.c`、`field_unit_stats.c`、`field_unit.c`；`fd2_field_unit_base_apply`、`fd2_field_unit_combat_stats_recompute`；`unit_base.json`、`progress.json`；`field_unit_base`、`field_growth`、`field_progress` 测试；B1.2／B4／SD | `0x50` 单位记录、角色 0..31 与敌军 68..135 构造、装备及已确认成长／学习写序已实现；32..67 不支持新建，成长／学习表却覆盖 0..67；持久表与活动表分 owner | **L/S/T**：不能以构造器代替运行中转职／成长；转职已确认复用原单位做增量，不能为 32..67 发明新基础表；stage32 的 48／66 确实经原 getter 读取相邻原字节，当前 scene fallback 丢失属性是 G2 缺口。核对原入口和实际构造有效域，边界／异常字段与后期来源未逐一运行。原 B4／B7；同一注入 RNG 比较精确写集、升级／学习提示 ACK 前后、等级边界及装备重算，失败不改原单位／RNG；转职见 C19 |
| C04 状态、死亡与 phase 计时 | `field_game.c` 的 phase／progress 执行、`field_unit_stats.c`；`flows/progress.json`；`field_turn`、`field_progress`、`field_ai_commit`；B4 六组配对 | 已有毒伤、状态到期／提示、恢复、隐藏／acted 和已确认状态派生；数据化的是通用规则 | **S/T/L（待逐分支确定）**：清状态／附加状态的新法术和服务要覆盖同一计时、恢复、死亡、控制资格规则，不能因有状态字节就认定全部完成。依赖 C08—C12／C19；原 B4；逐状态进入、重复施加、到期、死亡／复活候选分别核实，比较写点、RNG、通知顺序及取消，未证复活语义先取证 |
| C05 原始未知项与容量域 | `tools/static_data/audit_stage_data.py` 的 `audit_stage`／`read_dispatch`／`read_movement_table`；`field_unit.h`、`field_magic.h` 及各入口容量门；SD3 最终记录 | 已完整提取并隔离上述原始集合；**G1 来源族分类完成，产品修复未交付**。1497项已映射（622绑定、388通用缺口、450原表外、1默认无操作、36已支持脚本），见[来源账](../reverse-engineering/common-event-semantics.md)；不等于全部回调冷图还原。新指令依据确认原 actor 缓冲 96 条、模板有效计数与同索引 placement 合同；当前 SDL 64 条／ID-ordinal 策略是通用缺口 | **S**：G1 对 652 未知、468 未开放、70 移动和 12 配对问题按源槽／目标去重，标明可复用语义、纯绑定、不可达／填充证据或未决；450 turn255 必须有到达性／原调用合同结论。原 SD3 的后续未知范围／B7；分类账每条有源位置、调用者、阻塞项及目标阶段，无归属未知阻塞通用完成门；不能只将所有未知转 S 阶段 |

### 3.2 控制、移动与战斗动作

| ID／通用能力 | 文件、关键符号与现有依据 | 基线实现与真实状态 | 缺口、依赖、原工作项与可执行验收 |
|---|---|---|---|
| C06 移动、寻路与共享 pending | `field_path.c`、`field_move.c`、`field_move_profile.c`、`field_game.c`；`fd2_field_move_range_compute`、`fd2_field_move_path_find`；`movement.json`；`field_path`／`field_game_system_menu`／`field_player_controller`；B1.4c／SD | 成本、占格、阵营、ZOC、六相位、取消回退、match0 pending 保留／覆盖及完整动作后分派已实现；START 行军有配对 | **S/T**：近敌起点离开 ZOC 仅静态／合成证据；未运行的 profile／路径排序域、70 原脚本见 C05；script24 的 0x80 零步分支会原样写 direction5 后重画，不能屏蔽为普通四方向或把跨指针跨度直接判损坏。原 B1／B7；用两种地图／阵营与近敌夹具比较可达格、终点／路径、pending 与取消／错误写集，不把远距终点配对扩大到所有 ZOC |
| C07 回合、阵营与玩家／AI 控制器 | `field_game.c` 的 `fd2_field_game_remaining_units`、自动 phase／AI 分派；`field_ai.c`；`field_turn`、`field_ai`、`field_ai_commit`、`field_player_controller`；B1／B6／SD | END、side 2→1→0、新玩家回合、休息恢复、acted、AI behavior 0..5／7..11 已有实现；行为 11 保留分段提交 | **S/T/L**：behavior 6 已有注释说明无有效分支，不发明为新玩法；其它 raw 位及后续合法控制域仍按原分派核实；AI 新效果的评分／候选／提交分别验；phase 呈现见 C24。依赖 C04／C06／C08—C12，原 B1／B7；回合事件、状态、死亡、无候选、脚本失败、同分排序与 RNG 次数逐类差分，正式 END／March 可达，不能待机回退掩盖未知 |
| C08 普通攻击与通用收尾 | `field_attack.c`、`field_combat.c`、`field_game.c`；`attack.json`；`field_attack`、`field_combat`、`field_game_attack` 测试；B0／B4／B6 | 命中／暴击、双击／反击、物理伤害、奖励、死亡、acted 和已确认格事件收尾已实现；部分算术／terrain／descriptor 在 RNG 前拒绝 | **S/T**：对有效原数据域逐一确认拒绝是坏输入保护还是未覆盖能力，不删除 signed 溢出保护冒充完整。依赖 C05／C14／C24；原 M6／B0／B4／B6；每种合法路径比较效果、次数、奖励与 RNG，未支持必需尾在提交前拒绝；提交后错误保留事实且不可重放 |
| C09 玩家法术、范围与效果 | `field_magic.c`、`field_attack.c` 的 `fd2_field_target_range_compute`、`field_game.c` 的 `player_basic_magic_list_is_supported`／`fd2_field_game_magic_is_available`；`magic.json`；`field_magic_*`／`field_attack`／`field_item_target` 测试；B2／B4／B6 | **部分实现＋支持门**：列表、目标与取消、MP／效果框架已有；正式资格仍为 13，或 unit9 的 0。36 条冷记录和可见学习项不等于 36 种可用法术 | **L/P/S/T**：逐 magic ID 将记录、目标类型／范围、效果、caster 例外、MP 写点、经验／状态／收尾、FULL／SHORT 连成真实能力；12 项列表门与 40 bit／36 record 域先取证，不猜分页。依赖 C03／C04／C07／C08／C10／C24；原 B2 剩余项；按语义等价类代表性原版取证，逐 ID 源引用／参数校验；空目标、MP 不足、移动后例外、每级取消／确认与多目标顺序无副作用门通过 |
| C10 AI 法术、物品评分及提交 | `field_ai.c` 的 `fd2_field_ai_magic_targets_score`／`fd2_field_ai_choose_magic_candidate`／item 对应函数；`field_game.c` 效果提交；`field_ai_commit`；B6 的 15 项 AI 各 2 组 casting 帧 pair 与 SD3 18 项 AI 范围 | 已实现多种攻击／治疗／状态效果与评分；magic 0..22、25..27 有已确认提交能力；**23／24 及 ≥28 明确不提交**，25 AI 评分为零不能伪造候选 | **L/S/T**：原评分为零不等于没有玩家语义；未知效果、行为与原表有效 ID 的交集须 C05 分类。与 C09 共享 effect／range，不共享玩家费用／取消契约。原 M7／B2／B6；逐候选比较中心、目标顺序、tie、评分、MP／物品消耗、随机调用和 common tail；已有 casting 图形证据不证明全部 FX／SFX |
| C11 物品 Use、装备与派生 | `field_item.c`、`field_game.c`；`items.json`；`field_item_commit`／`field_item_flow`／`field_item_select`；B3 | Use 的 20 类已确认玩家效果、职业许可 Equip、Drop、取消重算及保留已提交库存已完成所列范围；普通装备不能 Use | **S/T／支持门**：未知 ID、稀疏库存与必需尾仍拒绝；先核实原库存不变量和所有合法 item 记录消费，不把密集前缀门当原版规则，也不凭门存在断言稀疏库存合法。依赖 C05／C08／C14，原 B3／B7；全合法记录按等价类映射到现有效果，满 HP、状态清除、传送 MP、非消耗、装备删除与错误／取消精确写集可复现 |
| C12 Give、满包交换与行动提交 | `field_game.c` 的 `fd2_field_game_cancel_item_give` 等 Give 路径，`field_item.c`；`field_item_commit`／`field_item_flow`；B3 35 类 WGC 场景 | 相邻目标、空槽转交、满包交换、各层取消、外层取消保留成功结果并结束行动已实现；不是只做接口 | **T/S**：跨队伍／后续服务复用须保留职业重算、稀疏域结论与移动提交点；不重新开发已确认分支。复用 C11；与 C18 新队伍服务联验，原 B3；空槽／满槽／已装备双方交换及关闭／失败不重试，sender／recipient 派生重算保持各自原写序，不擅自改成双方即时重算；完整活动与持久表在适当同步边界一致 |

### 3.3 事件、准备服务与保存交接

| ID／通用能力 | 文件、关键符号与现有依据 | 基线实现与真实状态 | 缺口、依赖、原工作项与可执行验收 |
|---|---|---|---|
| C13 事件触发与动作执行 | `field_event.c`、`field_game.c` 的 turn／cell flow 执行，`scene.c` 的场景 flow；`flows/stage.json`／`scene.json`；`field_event`／`field_turn`／`scene_handoff`；B0／B7／SD2—3 | 16 槽查询、阶段／格条件、有限领域动作与已确认 entry／result／victory 已实现；必需未知动作明确失败，stage 2 战斗禁用 | **S/L/D**：按 C05 分解未知 callback 的跨关条件／动作／等待与具体剧情绑定；公共结果查询不能用通用全灭代替特例；append／move／属性／资金／隐藏／转场等既有原语覆盖以真实调用合同为准。原 B7／SD；逐新增节点和边作有序 trace、错误／重复／取消回归；至少两个不同绑定证明复用，纯绑定留 S，不预做完整新关 |
| C14 等待、ACK、取消与一次提交 | `static_flow.h` 的 NEXT／YES／NO／CANCEL／ACK／TEXT_READY／SLOT／ERROR；`field_loot_flow.h`、`field_reward_flow.h`、`field_game.c`、`scene.c`；`field_loot`／`field_progress_play`／`field_game_failure`；SD2 | 现有 loot／progress／scene 的分支、满包取消、text_ready／ACK 写点及热游标已外置；notice 容量和首错冻结有测试 | **T/S/L（新合同）**：新服务／效果的等待不能无条件套既有 ACK；未解流程可能有新合同。G2／G3 所需新增能力在各自阶段完成，不等 G4 才能执行。原 B0／SD2；每条等待／取消边断言可见反馈、业务写点、RNG、重复输入、窗口关闭、容量／资源错误；提交后错误不伪造回滚，不新增通用事务框架 |
| C15 掉落、箱子、特殊格与奖励编排 | `flows/loot.json`、`progress.json`；`field_loot_flow.h`／`field_reward_flow.h`；`field_loot`／`field_progress`；B3／B4／B7／SD | 空槽／满包换物、取消、装备派生、已确认箱子与 mode 分支已有；奖励及状态通知通用，具体格绑定专用 | **S/L/D**：已支持 descriptor mode0 物品／1 金钱／2 回调／3 剧情，但冷图 preflight 仍含 stage<2、特定 callback 与 cell mode<2 支持门；G1 已确认 descriptor mode4..255 的执行 default 是继续队列、无业务效果（collector 对255另有哨兵合同），源中 mode203 不是未知 callback；unit `+0x31..33` descriptor 与特殊格条件逐类分类；已知原语足够时只缺绑定，不重写算法。依赖 C05／C11—C14，原 B7；取消／交换装备／无合法奖励／重复触发／一次性完成位与存档恢复逐字段对照；来源未确认不可仅放宽支持门 |
| C16 活动快照与四槽准备存档 | `save.c`、`field_save.c`、`field_manual_slot.c`；`fd2_field_game_export_battle_snapshot`、`stage_snapshot_supported`；`save`／`field_save`／`field_manual_slot`；B1／B5／B7／SD | codec、checksum、未知尾与完整 roster 保留、失败 Load 不换 owner 已实现；活动支持 stage 0／1 与 stage 2 入场；标题／酒店四槽恢复准备状态而非战斗 | **T/S／支持门**：新通用状态写回哪些原字段、哪些只是临时 UI／flow 游标须随能力确认；未开放 stage 的资源／事件重建合同不因 codec 通用而通过。复用 C03／C14 已有合同，C18—C19 新状态随 G3 联验；原 B1／B5；非当前 stage 恢复、损坏／空槽／写失败、未知字节、32 人 roster、交易／转职后 roundtrip 与两端互操作；不加自有 RNG／UI 存档字段 |
| C17 结果、应用与跨场景资源 owner | `app_flow.c` 的 `fd2_app_flow_after_battle`、`main.c`、`field_play.c`、`scene.c`；`app_flow`／`field_play_result`／`field_stage_transition`／`scene_handoff`；B1／B5／B7 | 胜／败／离场／宿主退出／错误区分，stage 0→准备→1→准备→2 入场已接；单独调试入口不代替正式应用链 | **D/S/T**：关卡结算顺序留 S；可复用同步持久表、准备交接、资源借用和错误恢复合同保留，发现新通用动作先补 G。复用 C13—C16 基线，C18 新同步合同随 G3 联验，原 B5／B7；准备取消、失败加载、胜败分派、跨场景 BGM 与资源释放、宿主关闭一次退出，不能 `stage++` 或复用 stage 0 捕获初值 |
| C18 持久队伍与出阵选择 | `field_prepare_play.c` 的 `fd2_field_prepare_run`／出城分支，`field_game.c` 的准备导入，`save.h` 的 `fd2_save_manual_state`；`field_prepare_play`／`field_stage_transition`；B5 | **部分实现＋支持门**：酒店／准备和完整 32 人持久表已保留；出城只接受 1..16 人，没有 >16 人选出阵界面／流程；原版 stage<27 选16人、其后20人，另有 NoTown 准备路径 | **L/P/S/T**：G1 已确认选满即提交稳定排序，之后的最终 NO／Esc 不撤销；据原写点实现选择、排序、强制／可选成员、16／20 上限及未出阵状态同步；不是扩大数组或截断 roster。依赖 C03／C16／C17，原 B5 剩余／B7；满 32 持久表、边界人数、合法选阵、重复／取消／关闭和保存重载，未出阵单位及未知字节不丢失；选阵结果以原版代表夹具确认 |
| C19 城镇交易、教会与转职 | `field_prepare_play.c` 中城镇 selector 1／3／4 明确拒绝（0 为酒店、2 为出城），`field_item.c` 装备／物品记录、`save.c` 准备 owner；B5 未覆盖项；原入口／写集已获 G1 静态证据，原版运行待各服务定向验收 | **未实现，静态合同已确认**：普通／隐藏商店、教会复活／转职和 NoTown 正式服务未交付；资金／物品／职业字段存在不能代替服务。见准备服务和战斗语义读集，尚无新增服务运行通过 | **S/L/P/T**：G1 已取得城镇入口、职业变更、商品／价格／库存／服务条件及状态写点；仍按全源账核实有效域；G3 实现有证的买卖、教会服务、转职、准备态库存管理及对应取消／资金／装备重算。原 B4 转职、B5／B7；逐服务成功／不足金钱／满包／无资格／取消／坏资源／重复确认及 Load 比较精确钱／单位／库存／RNG／原档写集；商品与关卡可售表本身属 D，通用执行能力不能后移 |

### 3.4 共享呈现、输入与音频

| ID／通用能力 | 文件、关键符号与现有依据 | 基线实现与真实状态 | 缺口、依赖、原工作项与可执行验收 |
|---|---|---|---|
| C20 通用菜单、列表、详情与辅助页 | `field_command.c`、`field_system_menu.c`、`field_info.c`、`field_play.c`；`field_info`／`field_detail_flow`／`field_play_overview`／`field_play_auxiliary`；B1—B3／B7 | 已支持指令页、二选一、详情双页、列表、总览与缩放页正式路径；支持 stage／列表容量有门，详情与列表不等于所有服务菜单完整 | **P/T/S**：新增领域页随 G2／G3交付；后期总览的实际通用显示分支先核实，stage16 特例文字属后置候选；长列表见 C09。原 B1／B2／B7；边界选项、层级返回、任意普通键翻页、不可选项、资源失败及宿主关闭，查询／浏览不提交玩法 |
| C21 设置后两项与 SHORT／FULL | `field_game.c` 的 `refresh_system_menu_disabled` 固定 `{0,0,1,1}`；`option_values[2/3]`；`field_system_menu`、`field_save`、`battle_animation.c`；B1／B6 | **部分实现＋支持门**：Music／SFX 可用；第 3 项（`option_values[2]`）已有 SHORT／FULL 后端消费但 UI 禁用，第 4 项（`option_values[3]`）仅见持久化／标签、未见业务消费 | **S/L/P/T**：G1 核实两项原义、raw 非零值与默认／Load 行为；G2 补动作所需 SHORT／FULL 差异，G4 补设置正式入口和第 4 项效果。不能以灰按钮关闭缺口。原 B1 剩余／B6；切换后实际可见行为、各动作模式、跨标题／活动／手工 Load、取消与无效值保留分别验 |
| C22 宿主输入、未解键与确认异常 | `input.c` 的 `fd2_input_begin_frame`／`fd2_input_take_action`，`field_play.c` 的 `field_action_from_input`；`input_test`、输入规范、阶段记录旧 END／跳页 | 单宿主逐 present 脉冲、一次消费、失焦清理、持久 quit 已实现；500／92 ms 为明确近似。G 映射 `FIELD_SPECIAL_G`，未见后续消费；G1 原指令证明 browse 中 G 本就是无操作，Esc／Z／KP5 共享环扫规则。旧 END 默认 NO／跳页未定位 | **S/P/T**：G1 已确认 G 无操作，不新增功能；按已确认 Esc／Z／KP5 环扫、输入／等待 RNG 和历史症状完成剩余合同；G4 接通确认的通用功能并补输入接收→首呈现→消费证据。原 B1／输入 V1；长按、失焦、未读取脉冲丢弃、层间不泄漏、关闭优先和取消无副作用；若 G 是真正关卡特例，以证据转 S。保持逐帧模型，不恢复 BIOS FIFO |
| C23 对话、通知与共享等待 | `dialog_flow.c`、`text.c`、`scene.c`、`field_play.c` 的 `field_progress_box`；`dialog_flow`／`field_progress_play`／`field_system_message`；B4／SD | 已有 token、换人／滚动／结束独立等待与 progress ACK；消费者的控制码域不同；`mouth_phase` 仍有跨框 shortcut，完整等待 RNG 尚缺证据；G1 确认 scene 通用文本会跳过原 -4／-5／-6 动态 token，属实际接线缺口 | **S/P/T**：G1 建实际控制码／消费者清单，G4 按现有 owner 处理跨框相位与共享等待；新业务通知由所属阶段补。原 B4／B6／SD；每种有效 token、换人、滚屏、末尾 ACK、动态数值、重复／关闭与资源错误写点对照，呈现重播不重复成长／扣费 |
| C24 战斗演出、范围 FX 与回合横幅 | `battle_animation.c` 的 physical／magic present、`figani.c`、`magic_fx.c`、`field_play.c`；`battle_animation`／`figani`／`magic_fx`／`field_magic_full`；B6／SD3 | 物理 FULL、FULL0..9 与基础13已完成所列门，已确定结果重放；ID9 限 side0 单目标；结构可解析不等于可播放。新回合编号横幅未实现，日志并非横幅；`field_unit.c` 的 raw26 状态横向抖动尚未补 | **L/P/S/T**：G2 随完整动作族补 caster／side／多目标、费用 marker、地图 FX；G4 补通用 Round 横幅与 raw26 状态抖动。SHORT 不强求与 FULL 业务等价；FULL skip 应保持自身业务／RNG。原 B6 剩余／B1；逐族合法播放、skip、静音／null、坏 selector／样本、退出／晚错误、费用零／多 marker及一次提交对照；横幅编号、只显示一次、输入隔离与等待写序有原版配对 |
| C25 VGA、透明底图、水色与 AFM | `vga.c`、`field_visual.c` 的 `fd2_field_water_tick`、`animation.c` 的 `afm_exec_frame`／`fd2_animation_play_hooked`、`scene.c`；`field_water`／`field_visual`／`resource`；B0—B7／R1 | 调色板、层序、present／deadline、共享水色 owner、已确认 AFM 0/1/2/4..9 已实现，未知 opcode 拒绝；55 ms 为近似。G1 已证明13个 RLE SKIP 像素此前在同一共享工作页被合法资源写过，属于页历史／绘制顺序缺口；最终各像素颜色及修复尚待 G4 验证 | **S/P/T**：G1 已确认共享页 owner 与先前写入下界；准确有序覆盖留 G4。实际 AFM 域继续按来源账核对，不能凭缺 opcode3 发明功能，不能填 stage32 捕获色常量；G4 修通用缓冲／相位机制，确属场景特例才转 S。原 B6／V1；Load／跨场景 owner、透明／前景层、合法水色调用、坏流、跳过不泄漏、退出验证；完整原始 RGB／墙钟同相未验另列 C28 |
| C26 音频队列、PCM 与设备生命周期 | `audio.c`、`audio_pcm.c`、`field_audio.c`；bus／retirement、24 voices、bank 借用、replace／stop；`audio`／`audio_pcm`／`audio_lifecycle`；A0—A2／A6／A8 | 共享框架与 null／discard 已实现；打开／resume 失败维持 null 至销毁，非自动设备恢复承诺；bank 活到 retire | **T／新增 cue 的 P**：随新动作服务补 cue 与 bank 预检，不重建播放器。原 A6／A8；替换／Load／退出、队列边界、坏 bank、无设备、晚错误无双重释放或逻辑重复；物理拔插与默认路由迁移另列未验 C28 |
| C27 BGM、XMIDI 与通用场景调度 | `bgm.cpp`、`field_audio.c`、`main.c`、`scene.c`；`fd2_bgm_play`／stop／set_music_enabled；`bgm_native_sequence` 等测试；A3—A7 | 原 EVNT／32 duration、18 硬件 voice、loop／整数 fade、同曲不重启、配置／Load 和已支持场景调度已实现；15 首机器码向量范围与阶段证据保留 | **P/T（新场景调度）**：新通用效果／服务在实际业务边界接 cue，不能从绘制／回调推进游戏。原 A5／A7；请求顺序、次数、循环、stop／fade／配置和 Load 恢复有可追溯捕获；后端原始响度／音色保留，不重新做补偿 |
| C28 证据残余与精度声明 | [阶段记录](stage-log.md) B0—B7／A7／A8／R1／SD3；[音频计划](audio.md)、[输入设计](../systems/input.md)、[LE 验收](../reverse-engineering/le-validation.md) | **已实现机制的未验范围及未决语义（具体状态见 C22—C25）**：完整音画／波形、严格 PIC／绝对时序、完整共享 RNG 同轨、水色／嘴型整帧相位、物理拔插／路由迁移及严格运行时 BSS 未验；其他平台未执行 | **T/S**：C22—C25 中影响业务写序、必要反馈或 RNG 的问题必须在 G 中解决或明确仍阻塞；不能全塞保真残余。用户已接受的是 Nuked 原始响度／音色，其余只是未验范围。原 V1／V2／A7／A8；G5 精确列仍未验的测量域及影响，不能宣称全游戏绝对完整；不为普通功能重新打开已通过的静态 fixup，严格 BSS 单独登记 |

## 4. 旧工作项与后续归属

下表只映射范围，不建立另一套优先级。G 阶段的顺序、依赖和完成门见总计划。

| 原工作项／保留历史 | 通用残项归属 | 关卡专用残项 |
|---|---|---|
| B0／B1 已完成的错误、控制、输入和菜单 | C07／C13—C14／C20—C25：新增语义的错误边界、未开放设置、未解键与共享呈现 | 具体关卡条件文字／真正特例绑定 |
| B2 基础玩家法术已完成 | C09—C10／C24：其余合法法术、caster、范围、费用、AI／演出 | 获得时机、具体敌人配置 |
| B3 已确认库存操作已完成 | C11—C12／C15：有效记录域与通用库存合同核实；C19 交易服务复用 | 箱子、掉落、商品表的具体绑定 |
| B4 已确认经验／成长／状态已完成 | C03—C04／C19／C23：职业变更、有效状态域、共享提示与等待 | 招募名单、条件参数 |
| B5／B7 纵向范围已完成 | C13—C19：通用未知动作、出阵／服务、存档交接 | stage2 战斗及后续 entry／result／victory／剧情顺序 |
| B6／A6 当前演出与最低声音门已完成 | C09—C10／C21／C24／C26：完整通用动作族与模式分支 | 新关独有镜头／cue 绑定 |
| A0—A8、R1 已完成支持范围 | C26—C28：新增能力调度／必要证据，平台与波形未验保留 | 新场景曲目／声音参数；新发布另定范围 |
| SD1—SD3 已完成并本地集成 | C02／C05／C13—C15：未知项语义分类与新增领域原语 | 已知原语的后续静态组合和场景数据 |

## 5. 取证、依赖与阶段验收规则

G1 的产物须把原服务／控制入口、208 分发表槽、全部结构级未知项与本矩阵建立双向映射。它不是要求对每个重复槽做一次原版运行，也不要求提前还原所有关卡剧情：先按目标、条件、效果、写集与执行器归并语义类，再为每类补来源和最小代表证据。能证明隔离于共享规则的关卡私有序列，可登记尚未解完的内部顺序并留 S；可能影响共享规则却无法分类的内容仍阻塞。关卡身份只用于找前置，不能决定它属于通用还是专用。

取证账至少包含：原始源槽／canonical 函数、调用者／条件／有效域、复用机制 ID、已支持／仅缺绑定／缺原语／未决、静态与运行证据、依赖和验收。450 turn255 若证实不在正常运行范围，必须说明原计数／到达性／哨兵合同；不得仅以通常不到 255 回合忽略。70 条移动脚本中的特殊字段若代表通用原语或有效容量，先补机制；若仅供不可达或专用序列，也须有依据。G1 已完成本来源域内服务与控制／设置语义族的分类，G2—G4 已交付对应有限机制；G5 仍按正式入口、支持门及本轮证据逐项复审。新发现的未决通用语义继续阻塞，不能将原历史 common_gap 标签当作当前未实现项，也不能写成没有需求。

G2／G3 需要的新节点、菜单、资源与演出随本阶段完成，C14 的现有执行器是基础；G4 负责剩余共享合同，不能把 G2／G3 必需的执行能力拖后产生依赖环。原版证据来自后期场景时，先取隔离最小夹具，不交付整关。任何新发现的可复用语义回到所属 G 包；无法确认通用性的未知项留在通用阻塞清单，不能默认转 S。

每个 G 编号为一个完整成果与提交单元，规模遵循 B3／SD1，不拆微提交或逐步骤等待。阶段内只验证新增、改动及共享代码直接受影响路径，补对应原版代表性对照；G5 统一审查并全面回归所有已支持历史功能，修复和消冗后验证提交。本次计划写作不运行这套历史实机测试。

各阶段报告必须分列：SDL 自动测试／正式输入，静态字节或局部机器码执行，DOSBox 原版运行。每项记录具体场景、判据、来源和未覆盖域。Windows 使用 WGC HWND 单窗、owned PID 后台投递、实际显示 ON／活动桌面与同线程释放临时显示需求；串行启动并在成功／失败／超时均清理自己的进程。2026-11-09 开发授权允许主动唤醒及必要的临时电源设置：先记录原计划／改动项，结束恢复并核验，异常中断后先恢复遗留；请求成功不代替 WGC 实测，不周期轮询或改混音器，详见图形规范。Linux 保留 X11／Xvfb 说明，未执行如实标注。

## 6. 从通用机制进入关卡专用工作的完成门

G5 必须逐项回答并附清单：

1. C01—C27 的所有已识别通用需求均已实现、正式路径可达并完成适用证据；仅有接口、单测、一个 stage 通过或继续禁用均不能过门。
2. C05 及 G1 扩审的未知项均有归属；通用语义未决、尚未审计的服务／控制分支或可能要求新通用能力的来源项为零。可证的纯场景数据／真正特例可以留下，但每项有源引用、分类理由、共享依赖的已验原语及后续 S 验收标准；真正特例自身待实现的私有算法单列，不混入已验原语声明。
3. 领域规则、费用／奖励／状态／AI、冷流程分支／顺序／等待／取消、正式 UI 与必要反馈形成完整链；热单位／库存／完成位／游标维持现有 owner 与存档边界，没有引入 VM、事件总线或第二套热状态。
4. 全部已支持历史功能完成当前平台统一回归，错误／取消／文件保护／RNG／资源清理有覆盖，提交范围经过审查和精简；其它平台未执行不冒称通过。
5. C28 残余表逐项说明是否影响领域正确性：必要业务／共享 RNG 写序／通用可见反馈未决继续阻塞；绝对硬件计时、全波形、平台外延与严格 BSS 单列，不能改写成通过或用户接受，也不能用这些精度外延无限扩大领域阶段。

允许进入 S 时仍可留下：新地图／编成／已验原语的触发绑定和剧情顺序、确有证据的关卡特例待实施、明确排除出领域完整声明的物理平台与精度测量。不能留下未归属未知、未完成的通用交易／转职／法术／输入合同，或用「后期才用」推迟的通用执行能力。该门只承诺**审计来源和已识别需求范围内的通用机制完整**，不承诺未取证的全游戏绝对完整；S 中发现新通用语义须暂停依赖场景，回补机制和定向验收再继续。

## 7. 本次审计的自审与验证边界

已从三个方向交叉核对：源码／实际支持门与冷定义；测试判据／历史证据范围；总计划／旧工作包／阶段日志的范围与依赖。明确保留反例：36 法术记录不等于玩家全部可用，408 FIGANI 条目不等于全部可播放，结构提取成功不等于语义完整，B6／SD3 已完成不等于通用机制完成，阶段日志中的中途失败不覆盖最终集成结论。

本次仅实际运行只读结构审计和文档一致性／链接／Git 检查，未运行 SDL、局部原机器码、DOSBox 或历史 WGC，也未新建／更新 IDA 数据库。引用既有函数语义不提升其证据级别。不存在已确认且需要用户先决定的重大技术取舍；推荐继续有限领域执行器、冷流程和现有 owner。若未来取证出现无法兼容原 SAV 的新持久状态或同等可行的重大产品范围选择，再附具体方案决策，不以猜测提前阻塞此次计划交付。

## 8. G1 实施更新（2026-11-10）

G1 已完成本审计来源范围的语义族归属；原计划文档交付的验证边界仍按 §7 保留，不改写为当时已执行。新增依据见[战斗](../reverse-engineering/common-battle-semantics.md)、[准备](../reverse-engineering/common-preparation-semantics.md)、[事件来源](../reverse-engineering/common-event-semantics.md)及[共享运行](../reverse-engineering/common-runtime-semantics.md)。来源账的 `classification_complete` 仅表示已归属，不提升 `all_game_semantic_complete`／`original_runtime_executed`／`new_gameplay_enabled`；三者仍为 false。正常资源／START／升级／合法转职来源的法术列表最多12项已闭合，不推广任意外部 SAV。

G2 当前须处理原96 actor域、有效模板计数及同索引位置、stage32有效ID48／66构造窗、零步方向5、完整合法战斗效果／模式及其必要呈现。G3 服务写集已获静态依据，原版服务运行待阶段内完成；G4 共享页历史、动态文本环境及设置／等待等实现与运行门保留。G1 的 Windows 唤醒／WGC核验、纯测、真实 IDA 发布及未执行边界见[阶段记录](stage-log.md#g1-通用语义取证与来源分类2026-11-10)。没有因来源已分类而开放新关或新玩法。

## 9. G2 实施与验收收口（2026-11-10）

本节覆盖§3的初始审计状态，不改写当时的验证历史。当前隔离树已完成下列 G2 范围及本阶段定向验收，接续 G3。阶段提交与本地集成以阶段记录及本地收据为准。顺序继续由总计划规定。

| 关联项 | 当前实现及来源 | 已有验证／仍需完成 |
|---|---|---|
| C02／C03／C05／C06／C16 | 原96 actor域、metadata有效前缀／同索引位置、48／66构造alias、106计数脚本、raw5跨cache读取和Load按当前身份重建旁侧cache；原80B／SAV不扩字段 | 原9表8528B＋alias48B、1887有效模板、106脚本2896消费／2898原payload独立检查；27项定向CTest包含相关域。12原尾仍保留，不是有效模板未解；不宣称106脚本逐条原版运行 |
| C04／C07—C12／C14／C21／C24 | 正常来源玩家0..30、32..35，AI候选与FULL／MAP分派、范围0／轴线30、复合32..35、传送23四段、Item20类及必要呈现／费用／奖励／提交尾；冷图进入magic域，效果选项2正式可切换 | 27项当前阶段定向CTest、正常ID/20类Item和失败冻结回归；原1c81f与真实C 23040向量差分。23两层取消、34业务、28的关联HP／EX与自动奖励页均有双端原生输入／全22987B存档证据；23实际成功、Item05／19／20代表和34有序资源门已双端通过；SHORT／恢复已接新冷图及有限呈现，FULL9原码17例闭合；恢复背景红绿与04双端门、SHORT03正式AI动作双端门均已闭合 |
| C15 | descriptor执行default4..255继续队列、无业务／RNG，不把collector255哨兵当已执行；撤除generic0／1／3的无依据stage门，具体callback绑定仍留其范围 | 760组原机器码默认分支、771次真实BIOS清队与SDL冷图回归；不把此证据提升为全部callback已实现 |
| C03／C04／C09／C23 | signed恢复贡献、DWORD累计／归一、HP超过max、状态治疗后存活预测及负数安全显示 | 复审修复三项预检错误；负号使用已有资源安全投影，明确原字体owner越界的安全扩展，不宣称原版负号像素等价 |
| C24／C26 | FULL／派生／复合／MAP及Item有限呈现消费，primary尾停、caster/cache资源身份、raw26共享时钟 | 新34离线资源门发现旧SDL实体flash为192；首次按caller表改色又被完整callee反证推翻。原4e127忽略arg_C，按原stride低byte填色；180组独立机器码通过。fixed456域应200，动态stride域另核；最终共用getter／Game真像素回归和34-04双端11阶段原件门已通过，不沿用旧业务pass作呈现通过 |

本次续接已补 SHORT AI 物理面板／HP条／六武器FX及声音、WAIT／AI静止恢复三wait写序与反馈，均进入现有有限呈现器；玩家物理经完整caller核实始终FULL，已修正算法与呈现分派。`1e611`是面板重画，不是反击箭头；未提供第四实参的HP条读取采用最小安全省略，保有证固定55及第二条。FULL9撤销多target整动作门，只省不存在的beam part绘制，保原跨target HP／贡献／RNG；17组真实原机器码与public Game差分已通过，95目标另四例仅C容量。冷定义现171表、五域39流程／606节点／6绑定。当前候选重建后六项直接相关CTest通过；早前27项、112项Python及7组双端保留各自来源，不能冒充当前候选全量或G5回归。

G2 的 SHORT 正式 AI 动作代表已补齐，完整动作域／正式入口／冷定义／支持门已复审并修复本次发现的问题。23实际成功、Item05／19／20及34有序资源门已通过，不重新列为待办。新原义和安全边界见[通用战斗语义§9](../reverse-engineering/common-battle-semantics.md#9-g2-short恢复与-full9-跨目标补充)。G3的转职／复活／交易／出阵，以及G4的共享菜单／条件／等待／底图／水色仍为待实现，不随G2机制接口存在而通过。

原`0x51a97`20步范围色周期为0..10..1，当前SDL的历史子集是G4共享呈现缺口；23目的地层用全部原实际LUT均异底图的完整ROI证明范围已经撤除，未据此宣称周期等价。现有G1分类账仍保留622绑定、388通用缺口、450原表外等原分类计数；这是来源归属，不能自动当作当前产品实现覆盖率。

恢复代表`native-recovery-04/`已双端通过：实体有序200→normal、bank31/cue4、576px独立地图tile及全22987B源域存档。旧03 SDL信息板由纯原资源门拒绝；AI恢复背景使用纯背景，玩家WAIT按已证caller及option3保留信息板，恢复不额外tick。counter按11eee只推进地形两字段，再按127a9推进App raw26抖动；不推进idle/walk或水DAC。SHORT_MAP原11cac点保留。此结论仍是已列资源／声音／业务代表，不扩大为整帧或精确时序一致。

正常caster的专门源门由`tools/static_data/audit_caster_domain.py`可复跑：32base／68growth／20learning及合法转职、1887有效模板×408FIGANI，50个玩家FULL组合、238个模板FULL实例／30独立组合均非empty；stage13 unit103／30的原AI零分／MAP分派排除单列。当前收据`build/g2/caster-source-domain-final.json`；不外推到任意SAV或未来新增来源，也不替代演出／声音。

`native-short-03/`在默认960×600窗口双端通过：面板→实体64与FX6完整合成→后续FX→HP条，bank64声3与全22987B；完整实体454个不透明点中441点为solid64、13点为FX覆盖。SHORT01的漏采、02的外部resize失败与CPU首帧诊断均保留，不作为通过。初始HP条按原1e7f6先写CPU、与首次STRIKE一并上传，未加延时或改80ms。

最终counter修正后4项直接CTest通过，battle_animation为31组；真实Game字段级测试核对地形55ms边界、时间倒退／长间隔、无追赶及组合时钟等价。最终Python与IDA发布校验另见阶段记录。原生代表按各自实际App身份保留，SHORT03是无反击场景，不被扩成最终counter原生验收。G2闭合的是已识别战斗域与本阶段新增反馈，不是G5全历史回归、所有动作逐像素／完整声音或Linux通过。

## 10. G3 实现与定向验收

G2已提交`61d5856`并集成本地master、重建主程序，收据`build/g2/local-integration.json`。G3在同一工作树完成已证准备领域、正式服务入口及本阶段Windows定向验收，以下按实际来源分别登记。**G3阶段实现与验收门已关闭，已以 `08f61f6` 提交并集成本地 master、重建主程序**，收据为 `build/g3/local-integration.json`。当前依[总计划](development.md#4-新优先级与依赖)进入G4，再G5，最后S；本节不替代§6通用完成门或G5全历史回归。早期未接线检查点保留在阶段记录。

| 关联项 | 当前实现与已核范围 | 保留的证据界限 |
|---|---|---|
| C16／C18／C19 | `field_prepare.c`保留32条80B及完整`0xa00`；31个同伴位、16／20稳定重排、必须成员／前置重排、metadata部署前缀与stage<13第6槽text2规则由有限原语及冷条件消费。17人最终NO、21人NoTown最终NO及stage22缺text24已有双端完整owner对照 | 上述量测为原RPC／SDL同App PDB＋owned RPM的运行态，不是NoTown之前Save冒充排序后态；也不等于整entry／settlement原生验收。32为保留容量，不等于所有32人组合均有正常游戏来源；晚期准备／部署能力不开放其战场或私有剧情 |
| C11／C12／C18／C19 | `field_prepare_item.c`与战场共享原raw移除／追加／装备及1b750派生；signed资金门、DWORD回绕、28632预览、自给alias合同。正式购买／出售／装备／转交、复活、转职均已接有限服务图；十项服务及新增独立装备重选／Esc、不同成员转交均有业务／页面／整档代表 | 未单列的确认NO／取消、满包等分支按C回归、局部原码及代表语义类覆盖，不宣称每个分支都独立实机运行；不将原未定义读取或无正常来源组合扩大为已支持域 |
| C01／C24／C26 | 当前187张静态表；准备`services`247节点、`deployment`85节点和76项有限op由正式`prepare_ui`消费。金额、立绘、转职与成长ACK复用原owner；已修原零透明调用并完成直接像素回归、隐藏入口及离线序列内容门，未建立VM／事件总线／第二队伍状态 | 4ed34→4ed4f零透明与4ed0b零色复制严格分开；隐藏入口仅一个ShiftF1代表及有限bank63资源运行通过，其余29项映射保留静态／纯测范围。离线有序内容命中不等于全动态逐帧／绝对时序一致 |
| C16／C17 | 四槽codec保整容器、未知meta尾及未用roster尾；手工准备资源预检独立于整关战场支持。真实对端`measured.SAV`已双向正式LOAD、成员／库存查看，再Save到新槽1；21人NoTown亦已正式Save槽1并直接返回picker／选阵，全22987B通过 | 交叉来源为已实测五人SelfGive样本，两producer字节相同但路径／运行收据独立；NoTown样本未执行19次选择。不是模型另造同fixture，也不是所有SAV／活动档／晚期场景互操作承诺。writer先于可中断收框，晚错／退出保留已写结果且不自动重试 |

准备入口按实际源码开放有资源和合法记录的stage1—29：普通准备行覆盖1—26，NoTown定义为22／23／24／27／28／29。NoTown不访问城镇商店／酒店；酒店Load拒绝NoTown目标，标题LOAD可进入其准备选择流程。资源／codec／服务就绪与整关战场支持分别检查；stage2战斗提交／END及后续私有场景仍按原门保留。

**Windows原版／SDL实际证据：**以下每项保留各自App／工具／来源身份，不能合称最新候选单轮全历史通过。十个服务报告的状态仍为`business_pages_save_passed_audio_identity_pending`，其中`ok`只覆盖已声明业务、页面、整档和清理；新增音乐owner结果另列。

| 组 | 实际通过报告（均在`build/g3/`） | 已确认范围 |
|---|---|---|
| P1 | `native-p1-buy-yes-05`、`native-p1-buy-equip-no-06`、`native-p1-buy-insufficient-06`、`native-p1-buy-full-06` | 购买／立即装备YES与NO、钱不足、收件满包的正式页面与完整22987B后态 |
| P2 | `native-p2-sell-equipped-06`、`native-p2-self-give-07` | 已装备物出售与自给；3/4列表价、raw槽／派生／钱及完整整档。未将自给代表扩大为所有转交分支 |
| P3 | `native-p3-revive-09`、`native-p3-revive-insufficient-09` | 复活成功／不足费、flags整byte清0、HP=max且MP保留；原页面／动作及整档 |
| P4 | `native-p4-promote-normal-10`、`native-p4-promote-special90-10` | 普通与90优先目标、耗物／身份／成长／MV与终态。normal09的SDL曾错把MV再卷到y157；10修为源y176，旧失败保留 |
| P5／P6 | `native-deploy17-01`、`native-deploy21-sdl-06`、`native-notown22-01` | 选满后最终NO与缺text24返回，双方完整`0xa00`及stage/count/钱/options零差异；整磁盘22987B及mtime未变。P521原端严格继承original01，02—05观察器适配失败不改写为通过 |
| 真实交叉SAV | `cross-save-native-01` | 原版实际SAV→SDL、SDL实际SAV→原版，正式LOAD／完整成员页与首成员库存／新槽1 Save，两方向全22987B写集、源槽／active／未知尾保护及正常退出通过 |
| 返回曲owner | `native-audio-p3-01`、`native-audio-p4-01` | 双方真实FDMUS／序列owner：曲11，P3 loop1／P4 loop0，playing状态4；原RPC与SDL匹配PDB／owned读取、整档不写和清理通过 |
| 独立装备／转交 | `native-eqt-reselect-01`、`native-give-different-01` | 正式装备服务重选／Esc及不同成员转交的完整源页面、派生和实际Save；两组各双端全22987B及正常清理通过 |
| 隐藏入口 | `hidden-entry-native-03` | 后台ShiftF1实际进入Town5，500标题、bank63背景／四图标／头像／440问候、六格商品及取消返回；完整SAV与mtime不写。01保留旧错误图标合成判据，02保留SDL组合键未响应失败；03经首按modifier元数据修正后双方通过 |
| NoTown正式Save | `native-notown-save-02` | 21人stage27：410 YES→picker0→DOWN→picker1单次ENTER Save→不发660 ACK直接已更新picker1→Esc完整选阵label／网格；槽0、active及未知尾受保护。01在低色picker标记缺失时失败，尚未Save；仅补三picker低色声明后02双端全22987B通过 |
| 转职离线内容 | `promotion-series-content-05` | 普通／90特殊的原版与SDL共四端实际WGC原件：旧FIGANI→白色阶段→新FIGANI→恢复服务有序命中；身份／缺sprite／白化负例拒绝。旧／新完整320×200，白化为有证320×50区域，不称完整白帧或所有时刻相等 |
| 金额离线内容 | `money-series-content-02` | 买入／出售／复活三组六端已有WGC原件，原26a7a／26b91／26c9b及数字条独立投影的非终态phase1..7内容门通过；不是新GUI、完整周期或时序等价 |

本阶段必要曲目／loop行为按源调用、实际返回曲owner及已有捕获候选分开核对后收口。返回11为真实观察；瞬时17／16仍仅为原必经调用路径推断，不写成PC命中或在播快照。WAV只按实际捕获窗口和候选登记，存在本身不证明身份；不声明完整PCM cue身份、播满循环、声卡输出、全波形、音色或所有逐字等待RNG等价。Nuked原始响度／音色沿既定用户选择保留。

**无窗口、局部原码及发布分开登记：**`latest-scoped-ctest.log`为9项、`movement-layout-ctest-01.log`为MV修正后2项通过，后续候选仅以其直接回归收据增量登记。`preparation-source-verification.json`核13张输入表337原字节及派生来源；`preparation-machine-comparison-final.json`为47个原码—C向量通过，含完整a00／钱及分步写点，不含真实RNG、UI或原版运行。`latest-scoped-python.log`的82项汇总含1个导入错误，不能称82全绿；修后独立运行及后续新工具测试另记，不相加冒充单轮全绿。

最终必要共享修改另有`save-edge-modifier-build-01.log`／`save-edge-modifier-ctest-01.log`的App构建及2项CTest、`final-shared-header-build.log`／`final-shared-header-ctest.log`的构建及5项CTest通过：覆盖Save确认后先写档再收框、hostquit／晚错不回滚不重试，以及F键首按modifier交付；原NoTown零等待RNG／无129与660仍保留独立合同。NoTown工具最终6项纯测、转职与金额内容工具各4项纯测通过，均不替代上表真实运行或扩大为G5全量回归。

`ida/publication/receipt.json`记录实际Domain隔离库发布：347规范名中新增33项，1312函数边界／签名不变，21项发布检查和14项crosscheck通过，原G2库未改；1310函数成功反编译，两个历史FLIRT `__int7`失败保留。此为命名与静态证据更新，不是新ABI或严格运行时BSS通过。

原未定义读取的处理仍须明列：原32人选择只清30字节，现将31个同伴位全零初始化；原负钱滚动越出数字条的部分只省未定义切片，资金signed／回绕保留；合法empty转职clip只省不存在的绘制。以上均不声称原未定义像素等价，也不通过截断队伍或禁用全部服务缩小完成门。

**G3完成门：**已识别的持久队伍／准备服务、必要图形／输入／声音合同和正式入口均已实现；成功、不满足条件、满包、取消、晚错、重复确认及存档边界按C／局部原码与上述原版代表共同闭合，未留下待实现的G3通用服务给G4。尚未逐分支原生运行的范围及精度界限保留，不承诺未取证的全游戏绝对完整。G3 已完成整阶段提交和本地主树安全集成／重建，当前按 **G4 → G5 → S** 连续推进；G5仍须统一审查并全面回归所有已支持历史功能。Linux及其它平台本轮未执行。

## 11. G4 实施与定向验收（2026-11-10）

G4 已完成本阶段实现、来源复核、定向验收和实质审查修复；整阶段提交／本地集成另见阶段记录及 `build/g4/local-integration.json`。接着执行 G5，S 不启动。当前为 193 张表、8 个流程域／62 图／1,390 节点／12 绑定；结构数量不是玩法覆盖率，common 的 15 图仍无生产 stage/action 绑定。

| 关联项 | 候选实现与依据 | 已核范围和未闭合边界 |
|---|---|---|
| C13—C15 通用事件、嵌套掉落与特殊格 | `field_common_event_*` 消费 47 项有限操作；条件、顺序、ACK／取消在冷图，原 Game／loot owner 保存真实 caller 和父子 PC。普通 cell 去掉非原义 stage<2 门；mode0 物品／1 金钱以外均按完整 WORD 查 90 项 callback 表，预检与执行叶均拒绝越界。YES 的 secondary UI31／cue12、300 ms，以及回调分支 200 ms→关闭→dispatch 均在冷图 | common30 例局部原码、190ac 完整原 CPU9 例、跨 stage2/6 普通格与 mode2/3/255／晚错 C 回归分别通过。common39 空槽／满包取消、普通 chest-swap 新呈现的本轮双端代表已通过。cue12 由实际 secondary 调用 C／源证明，未宣称该窗口 WAV 身份通过。真实绑定仍属 S，不把没有初始格引用的三条 `(2,0)` 改称无需求 |
| C16—C17、C20—C23 菜单、输入与存取 | 信息板选项3和 App 双门、G／Z／小键盘5等已证键、共享确认图、文本动态码／跨框 mouth、活动 Load 普通光标恢复已接。详情 `runtime.detail` 包含两次 16c57(0)，沿原 detail owner 借 0／3 头像；export／active Save／manual Save 拒绝详情忙态 | controls-native-02 三组完整配对通过。详情原版15的两个成功端点与 SDL 连续 WGC18 两端点分别通过，离线重核同夹具／完整页及 F2 整份 SAV 相同；`fresh_pair=false`，acted Enter 只核磁盘不写／Save 禁用，不冒称完整热 owner、PC 或精确 RNG 测量。忙态拒绝另有缓冲、整档及精确 mtime C 回归 |
| C24—C25 工作页、视觉时钟与 Scene | `field_page` 是 App 唯一 456×336 工作页；RLE SKIP 保旧值、opaque0 实写，Scene／Game／Load 共享。范围 20 步、七 DWORD、raw26、菜单／mouth／水色分别按原 caller 推进。arrival32999 为 FDOTHER9 的12帧，i6/7/8 投影后重建下一底页；refresh0 推水色、refresh3 对应原参数1跳水色 | 实际 App 页、LUT 与 clock 的 C／原码、Scene water10 例及完整 arrival 原调用各自通过。自然 START32→31→0 双端117等待点、12单位／首焦点／镜头／显式Save，teleport23 必要 MAP 四阶段和总览稳态四区域原生对照通过；不扩大为全场景逐像素一致。最终列表无 raw26、总览入口 T→J→R／稳态 A→W→T→J→R 另有真实 Game／Play C 回归与16源调用点审查 |
| C24、C26 战斗动作与死亡反馈 | MAP LMI、染色／双色／震动、恢复／传送、浮字实际写 App；正常 MAP 保最后页，SHORT／恢复 FINISH 只释放，外层刷新由冷图决定。`runtime.defeat` 外置13旋转＋12消散／25等待，Game 逐 raw 写方向、后写 flags1；status／loot／common 三 caller 都停止在真实错误／退出前缀 | SHORT12 双端、common18 双端死亡13方向／12LMI与 cue3 最低身份、完整 SAV 均通过。死亡 CPU 两路径和完整 App 页／资源错误／quit／30锚边界 C 回归另列；超过30只省未定义 FX 锚，不截断96条业务。未声称所有动作逐帧、全音画或硬件时间相同 |
| C23—C25 通知与固定等待 | 普通 loot 的100／200／300 ms为原3790a纯延时，不推进 raw26／water；16c57 则保实际 blink／RNG／water。`field_notice_wait_ticks @1e5c0` 至少一轮 water→查键→signed WORD 两次计时／回绕→尾清键；STATUS 显式入口清键，CLOSE 不清 | 1e5c0 完整81 B 原码与真实 Play C 的9类轨迹差分通过；固定延时末帧／TEXT_READY 键保留、无 clock 写及状态 SDL 正式路径通过。较早多帧按键仍存在宿主帧脉冲与 BIOS 缓存差异，作为 G5 首项业务／RNG 阻塞继续修复，不能放进 C28 或 S |

当前 Windows 证据保留各自候选身份：

- `shared-review-ctest-08.log` 的23项、`arrival-physical-tail-ctest-09.log` 的7项、`cell-busy-review-ctest-15.log` 的7项、`host-word-review-ctest-16.log` 的3项和 `fixed-delay-ctest-18.log` 各自通过；12候选两项夹具失败及修复后的13记录保留，不相加为单轮全绿。
- `common-defeat-native-18` 原23／SDL22检查点通过，13旋转和12消散按同一有效 sprite 帧类匹配；原 DWORD3与1同图，不推断 native DWORD。cue3 声音 score约0.994／0.997，原 BGM 只在隔离夹具显式关闭；两端实际全22987 B SAV `raw_equal=true`。非 shipping alias 不进入产品，不是 S 自然绑定。
- `details-native-15` 原版两端点成功、SDL单帧采样超时的整体失败保持；`details-sdl-series-18` 只报 SDL `endpoint_passed`。`detail-endpoint-comparison.json` 联合复核同夹具／完整函数域、保留的两页0→3→0原件及实际 F2 SAV，明确不是一轮 fresh pair。
- native16 的死亡画面通过而声音未达身份阈值、17 的原画面／声音通过但 HOST_W 停录推进 ACK，均保持失败。18 把正常 stop 移到无 ACK 浏览点后完整通过；不降低判据或用后端静音、禁用能力掩盖缺口。
- `notice-wait-machine-comparison-19.json` 为9类真实 C／完整原码差分；`modal-clock-fixture-ctest-22.log` 两项通过。21 的4/6通过、两项旧夹具失败保留：分别漏拦正式 preview 呈现口、纯路由测试缺实际资源，22 仅修夹具后通过，生产未为测试回退。
- `common-full-native-20`、`teleport-native-20` 和 `src/build-win/graphics-artifacts/20261110-165741-106746` 的 chest-swap 均为完整配对通过；`overview-native-22` 和 `opening-native-22` 亦为完整配对，后者每端117等待点。所有窗口按本轮 PID 串行清理、来源守卫和显示需求释放通过，无电源计划或混音器修改。
- `status-endpoint-comparison.json` 联合原版 `20261110-170222-694154` 成功端点和 SDL `status-sdl-native-22`：同夹具、真实 WAIT→Load→Save、完整22987 B相同，`fresh_pair=false`。旧整体失败来自只排除y112弹框的工具过早向仍在y164关闭帧发键；新增实际Round／多高度关闭检查后SDL端通过，原失败不重写。
- `current-g4-python-22.log` 48项、`current-source-python-22.log` 38项分别全通过；不是全部Python或G5。源码887处函数引用规范地址审计无差异；IDA 发布372项命名、23项G4改名／33注释、1312函数边界和签名不变，21静态／14发布检查及11项发布SHA通过，不等于ABI或严格运行时BSS通过。

**G4 阶段门：**本阶段所列共享能力和必要定向代表已闭合；G5 尚未通过，通用总门与 S 入口保持阻塞。G4 收尾将跨帧输入差异明确调整为 G5 第一项跨系统审查修复：10620 只观察、16c57 消费一键，190ac 入口清键后的300 ms／文本／wait0之间无新的clear；更早键在多次present中丢失会改变等待和blink RNG。旧帧脉冲策略没有可追溯的用户原义豁免，必须在现输入owner内按caller完成保留／清键、取消、重入和原版代表验证后再跑全历史，不能降级、灰掉合法能力或转交 S。之后逐 C01—C28 签核源码／正式入口／证据、全面历史回归、修复精简并提交。

Linux／其它平台、严格 BSS、完整波形、绝对硬件时序及全游戏同轨等未验范围继续独立登记；已接受的 Nuked 原始响度／音色不扩大为其它未验项的豁免。G4 的实机代表、CPU向量和SDL自测互不替代，也不冒充G5统一回归。

## 12. G5 统一审查修复进行中（2026-11-10）

**历史快照：**本节的「进行中」「尚待」与较早候选身份均保留在记录时点；后续修复、原生／离线门、逐项结论及最终守卫见 §13。本节不是当前开发优先级或未完成清单。

G4 `4ada76b` 已集成本地 master。G5 尚未提交或集成，统一回归进行中，S 不启动。新候选已在同一轮通过 112 项无窗口 CTest（`final-ctest-29.log`），另通过 8 组局部原码检查及当前 C 探针对照（`machine-rerun-01/comparison.json`）；完整 Python 和正式图形历史门仍未签。本节记录审查新增发现及其完成门，不把 G1—G4 的历史证据升格为 G5 通过。

| 关联项 | 实审发现与当前处理 | 尚待关闭的验收门 |
|---|---|---|
| C20／C14／C15 | 原 BIOS 队列保留固定等待中的早到键，旧逐帧 pulse 会丢弃。候选改在同一 input owner 保存有限 FIFO，读一、peek、源 caller 的显式 clear 分开；物理 held、repeat、quit 和首按 modifier 合同保留。已实际取得四项旧实现反例与新 input 定向通过 | 新候选的正式早键、菜单／准备／文本路径及统一历史回归；不能仅凭 input 单测签所有业务 |
| C24／C26 | 2000a 被非原义 stage0／shape0／尺寸门限制，并遗漏关闭时 INT16 读一与 1f558 对 App raw-prefix 的写入。候选按原 64×64 cache 域、地图高度选择缩放，复用 App 工作页桥；没有开放后期 battle | 已支持非首关正式路径及高图最小资源别名代表，完整源像素、真实读一返回与存档保护；别名不是自然后期关卡验收 |
| C14／C17／C24 | CLEAR_KEYS 原先夹带 INFO 恢复；原 1a30b 不写该标志，13565／16f55 caller 返回才写。候选把清键与 caller_return 有限节点分开，保原热状态槽。新 clear 采样还揭示 HOST=-2 被 footer 的 result<0 当成 ERROR 回复，现已区分并保原 PC／前缀、拒绝退出后的再次回复 | 精确 caller、真 ERROR 与 HOST 的直接回归和正式 END／March／菜单路径，不以 finally 补写未到达的业务 |
| C13／C14／C17 | 六个既有 scene 绑定先整段 LOGIC 写 live Game，再重放呈现：action0 在 story11 等待前已追加后半组，action6 在 story4 返回前已写 AI；关闭窗口会保留尚未到达的后缀。失败时整段 rollback 也违背晚错前缀合同 | 迁入既有 common/event_resume，冷图逐叶决定顺序、等待与取消；保原六绑定，移除整段预提交／重放。arrival 资源准备→现 Game append→12 帧的投影／底图／清键／等待→释放沿有限操作消费，空组也保原等待。不新增 VM、第二 Game 或持久 owner；源写序、HOST／晚错、父子尾与正式代表必须通过 |
| C11／C12 | 新来源账读取 205 个完整函数／99,318 字节、active／roster xref 262／28，并以 23,049 个原 x86 helper 向量核 append／remove／equip 的正常密集槽保持。已将完整 writer、整记录复制及 count 写点闭包登记 | 限原模板／START／正常操作及由其产生的 SAV，不扩成任意外部 C0／洞状态。来源闭包与模型／原生范围分开；本轮统一业务回归仍未通过 |
| C06 | 原矩阵「近敌起点仅静态／合成」漏记了 G2 后续的 derived28：实际正式选择页匹配邻敌起点向安全邻格的完整有色 LUT，profile9／成本及原 4e390 起点规则独立核。历史 `native-derived28-01` 为通过的配对；它没有实际移动该 actor | G5 同代表须以最终候选再跑。范围正证、原路径／运动源与直接 C、普通移动原生分层核对；不将范围截图扩大为已实测从 ZOC 走出或所有地图路径等价，也不把已确认的起点规则重新列为未知语义 |

上述均为通用机制缺口或必要取证，不挪到 S 残余。已完成的阶段历史不重写；G5 仍按一个完整阶段修复、定向验证、统一历史回归、逐 C01—C28 签核、精简后独立提交。当前没有需要用户重选的重大方案。

后续实审还修复两处有真实旧序反例的边界：Scene 的同次资源绘制错误与 HOST 必须保 ERROR 优先；F1 关闭尾须完整消费 `11cac(0)`，共享刷新按 actor／cursor→水色→terrain／range→raw26→绘制推进，不能只恢复水色或在水色失败前提前推进 terrain。`review-red-ctest-25.log`、`refresh-order-red-ctest-27.log` 保留失败，`review-green-ctest-28.log` 四项直接用例通过。此后新 App 的 F1 尾已变化，较早 stage1／高图原生结果保留原候选身份，须补其直接影响路径，不能自动标为新候选全绿。

## 13. G5 通用完成门与最终验收（2026-11-11）

**G5 的代码、回归与通用完成门已在所列来源域内闭合，C01—C28 逐项通过；S 未启动。** 这里的通过覆盖已识别的跨关机制、正式入口及对应证据，不承诺未取证的全游戏绝对完整。完整阶段提交、安全本地集成和重建仍以实际 Git／`build/g5/local-integration.json` 为准；验收通过不代填提交或集成结果。

§3 保留 2026-11-09 的来源／模块审计基线，其旧「部分实现／待审」标签由本节当前结论补充。§9—§12 保留 G2—G5 候选、失败与未验范围的原身份；本节不把旧失败改成通过。开发顺序仍只由 [development.md](development.md) 维护。原生运行、SDL 自动测试、局部原机器码与只读离线补证分开登记。

### 13.1 最终证据状态

| 分账 | 已核事实 | 有效域与限制 |
|---|---|---|
| A47／A33 | 当前生产候选 112/112 无窗口 CTest，77.15 s，含 Python 注册项；Python 3.12 全量 1,376 项、0 失败／0 错误／2 skip。准备负例另有严格补测。 | 后续验证工具的直接测试不加算成一轮全量；legacy 本地 seed 仍未执行。 |
| A30／A4—A6 | 四个当前 C 探针、八组局部原码及独立原表；库存 writer／copy、方向 caller、六事件与 arrival 分别有来源／轨迹收据。 | 保留每组替身、原函数及参数域；不是全部 28 项完整 CPU、像素或声音等价证明。 |
| N84／N48 | 默认 84 项登记为 61 严格继承＋23 新实做；overview48 以精确来源替代对应门，baseline／newline 两 fresh pair。 | 不是一次 84 项全 fresh；旧 32／37／43 失败保留。 |
| N52／MISS | 旧 post52 失败前 23 项为 8 B7＋15 AI FULL；MISS54 补足合法 item56 已执行并 miss 的有限外部证据。 | 旧 `HP_positive=false`／`inconclusive` 保持；补证不是新 GUI，不证明命中、RAM score、全部 FX／SFX 或全 RNG 同轨。 |
| N61 | 前 10 项真实通过：普通／90 转职、17／21 finalNO、NoTown 缺员、库存、hidden、NoTownSave 与双向 Save 保各自业务／owner／整档门。 | 整体仍 failed：第 11 音频 P3 原端在打开 588 前因缺 scope 停止，SDL 未启、无 RAM 取样或新 Save；前缀只按精确来源影响继承。 |
| W58／W60／W62 | backlog、前键 baseline／QPC／唯一 trace、音频专用视觉 scope 依次修复；26／30／42 项直接测试分别通过。 | 588／591 全文＋完整 6,400 像素 portrait3 不放宽；正常 stop／双绑定后才原单 RAM／CDB。无新增 ACK、HOST_W、采集上限或产品改动。旧 56／59／61 失败不补签。 |
| N63／I63／W64 | N63 的 T01—T11 登记通过：9 个原生登记＋2 个既有闭帧离线内容登记。第 12 在旧 schema 预检失败；W64 复用 B7 来源、8 guard、15 原 entry ACK、manual0 同槽 2,570 B 和 18 source 封存，15 pure 与实际只读 prepare 通过。 | N63 仍 failed；I63 只证明失败命令从未进入 display／owned scope，旧 display closure=false 不改。只读 prepare 不充作原生运行；T12 随后由 N65 关闭。 |
| N65／W66 | N65 的 F1stage2 新 pair 实际通过 64.079 s。W66 仅修旧 SDL 灰门，四设置复用原正像素；23 pure、完整模块精确投影及 390 闭帧有限红绿通过。 | N65 整体仍 failed；其 audio-five 只完成 options Original，SDL 旧门误拒后未完成后四场，不升格业务。 |
| N67／W68 | N67 options 原／SDL 均通过；active-load 原端过、SDL 旧 browse 观察器过早，后 3 场未启。W68 仅改成功 Load 观察：ready→原唯一 ENTER→完整 Round 五合法相位之一→返场正资源／六框边与五 Round 相位消失→现有 2 s host 稳定窗→正常 stop／闭帧绑定；31 pure 与旧 370 闭帧有限重判通过。 | N67 整体仍 failed；W68 不改按键序列、产品或音频协议，不以稳定窗声称 guest PC／原 tick 测量。直接测试命令的 metadata 笔误由独立 erratum 更正，冻结日志／31 项结果不变。 |
| N69 最终联合门 | 新报告只实际运行 active-load／opening／end-turn／sfx-reenable 四场、原／SDL 八端，全部通过。联合 N67 options 两端的原 qualification、各端 5 份 SAV、WAV 和 1,124 份封存事实，原五场十角色门通过。 | 精确 2 old＋8 new；不是伪造的十端新 GUI 报告，不继承 N67 的 active-load 原端代替新配对。现后续登记为 60 既有实际项＋MISS 外补 1＋音频联合组 1＝62/62，含离线／继承项，不与默认 84 相加冒称 fresh 次数。 |
| L53／D | Loaded checker 按本端 preload 的完整已拥有字节强核；28 pure、22 旧红新绿及实际两端离线复验通过。设备收据仅 quiet／preOS sealed。 | fresh 未写字段与 Loaded owner 分层；NULL 是旧 App 有限入口，Release 是 frozen 材料／旧 Job125 强清理，不扩签当前 Release native 或设备可听。 |

### 13.2 C01—C28 逐项结论

每行均已通过**所列来源域内**的实现、正式消费者及相应验证门。`S` 是真正关卡数据／绑定／私有逻辑；「未验」是测量或外部来源域限制，两者不互换。源码／关键符号用于定位真实消费者；依据与本地原件路径／SHA 见 §13.5，不以接口存在、某一关或节点数量证明全域。

| ID／能力 | 正式实现／关键符号 | 已核证据及有效域 | 结论 | S／未验边界 |
|---|---|---|---|---|
| C01 资源／codec | [src/archive.c](../../src/archive.c)／[field.c](../../src/field.c)；`fd2_field_map_open_mem` | A47／A4，N84／N52／N61／N63／N65／N69；DAT／RLE／font／map 与完整 22,987 B 存档分别核边界。 | 来源域内通过 | 新S具体资源引用；未验：坏外部格式/未消费组合不视为已覆盖，也不据此发明系统 |
| C02 冷定义／ABI | [src/static_data.c](../../src/static_data.c)／[static_flow.h](../../src/static_flow.h)；`fd2_static_flow_node` | A47／A4／A5；194 表、65 flow／1,476 node 的严格 ABI 与有限消费者；N84／N52／N61／N63／N65／N69 按实际入口验证。 | 来源域内通过 | 具体图/触发/条件绑定；未验：定义/节点数量不作完成率；新未知引用仍须归类 |
| C03 构造／派生／成长 | [src/field_unit_base.c](../../src/field_unit_base.c)／[stats](../../src/field_unit_stats.c)；`fd2_field_unit_base_apply`／`fd2_field_unit_combat_stats_recompute` | A47、A30 派生／准备与 A6 库存有限 CPU；N61 两转职全 Save；N63 转职闭帧离线内容。 | 来源域内通过 | 编成、招募/转职时机和表参数；未验：外部异常字段/未证成长组合不外推正常源 |
| C04 状态／死亡／phase | [src/field_game.c](../../src/field_game.c)／[field_defeat.c](../../src/field_defeat.c)；`finalize_zero_hp_units`／`runtime.defeat` | A47／A30 原 1db65；13 旋转→flags→12 消散，N84／N52 与 N63 runtime／child 代表。 | 来源域内通过 | 剧情私有死者/初始状态；未验：30仅未定义FX锚绘制界，96业务不截；自然差异不归因RNG |
| C05 来源归属／容量 | [tools/static_data/audit_common_scope.py](../../tools/static_data/audit_common_scope.py)／[src/field_game.h](../../src/field_game.h)；`FD2_FIELD_MAX_UNITS` | A47／A5 来源分类、N61 缺员真实 owner；1,497 行／20 族是历史归属，不是当前缺口数。 | 来源域内通过 | 622具体绑定与已识别真正私有引用；未验：未知不默认S；450表外引用按原有效表边界拒，不发明callee |
| C06 移动／寻路／pending | [src/field_move.c](../../src/field_move.c)／[path](../../src/field_path.c)；`fd2_field_move_range_compute`／`fd2_field_move_path_find` | A47、原 counted 脚本／方向 caller 来源；N84／N52 真实本端规划、移动与交接；derived28 是范围正证。 | 来源域内通过 | 具体路线、镜头、组成员；未验：非全图每条路径/每类ZOC原生离开证明 |
| C07 回合／AI控制器 | [src/field_game.c](../../src/field_game.c)／[field_ai.c](../../src/field_ai.c)；`fd2_field_game_end_active_phase`／`turn_post_caller` | A47／A6 caller 来源，N84／N52／N63 runtime／controls 与 N69 END；side2→1→0、AI11 续态。 | 来源域内通过 | 关卡具体turn/phase触发；未验：不承诺两端全RNG或自然生存轨迹等同 |
| C08 物理／共同尾 | [src/field_game.c](../../src/field_game.c)／[combat](../../src/field_combat.c)；`resolve_physical_exchange`／`physical.full/short` | A47、原 18d8c／1548e 公式与调用顺序，N84 物理及 N63 child 代表；FULL／SHORT 不合并。 | 来源域内通过 | 敌人与战场绑定；未验：无新增完整物理CPU/全部溢出外部态或全FX证明 |
| C09 玩家法术／范围／效果 | [src/field_magic.c](../../src/field_magic.c)／[Game](../../src/field_game.c)；`fd2_field_game_magic_is_available` | A47／A30 derived／FULL9、N84；正常 0..35 除 31、最多 12 项列表来源域及移动后例外。 | 来源域内通过 | 获得时机/具体法术配置；未验：31无正常赋值、36..39无record；外SAV位图不扩大合法来源域 |
| C10 AI法术／物品计划 | [src/field_ai.c](../../src/field_ai.c)；`fd2_field_ai_magic_targets_score`／`fd2_field_ai_choose_magic_candidate` | A47／A30 有限效果、N52／MISS；15 FULL 仅两 casting 完整页与全 Save，合法 miss 独立补证。 | 来源域内通过 | AI编成与参数；未验：两casting非全targetFX/SFX/RNG；MISS非命中或RAM score |
| C11 Use／EQT／Drop | [src/field_item.c](../../src/field_item.c)／[Game](../../src/field_game.c)；`fd2_field_game_confirm_cursor`的ITEM分派 | A47／A6 库存 23,049 向量，N84／N52／N61；215 记录与 20 Use 族分层，不将领取当 Use。 | 来源域内通过 | 箱子、商品与物品获得；未验：外部C0/洞不纳正常dense库存源；领取不等同Use |
| C12 Give／交换 | [src/field_game.c](../../src/field_game.c)／[prepare_item](../../src/field_prepare_item.c)；`fd2_field_game_cancel_item_give`及准备Give | A47／A30 准备与 A6 writer，N84／N61；两 Give caller 独立，recipient 不擅自重算。 | 来源域内通过 | 队伍配置；未验：战场/准备Give的acted、邻接和重算不能合并 |
| C13 触发／有限动作 | [src/field_common_event_runtime.h](../../src/field_common_event_runtime.h)／[冷图](../../data/static/flows/common_event.json)；`event_resume` | A47／A6 六 caller、同源旧红新绿；N84／N52 与 N63 child；原业务／呈现按同一父 PC 交错。 | 来源域内通过 | 新stage/action组合、私有剧情编排；未验：15机制图不等于15自然绑定；common39非shipping alias |
| C14 等待／取消／once | [src/field_common_event_present.h](../../src/field_common_event_present.h)／[field_loot_flow.h](../../src/field_loot_flow.h)；父PC／ACK／child | A47／A30 common／notice，N84／N52／N61／N63；HOST／ERROR、重复回复、父子尾与正式等待；W62／W68 的观察修复另账。 | 来源域内通过 | 具体实例的剧情ACK绑定；未验：任意非线性未来身份/资源全部写前预检未承诺；现有限两边与每叶晚验 |
| C15 descriptor／cell／奖励 | [src/field_loot_flow.h](../../src/field_loot_flow.h)／[loot冷图](../../data/static/flows/loot.json)；`execute_loot_callback` | A47、原 190ac 九类 CPU／90 callback 表、N84／N52／N63；mode>=2 按完整 WORD 分派，不整域禁用。 | 来源域内通过 | 地图格/descriptor/回调identity；未验：mode>=2 stock无初始地图引用不等于不需要；unknown callback不可猜 |
| C16 活动／四槽SAV | [src/save.c](../../src/save.c)／[field_save.c](../../src/field_save.c)；`fd2_field_game_export_battle_snapshot` | A47、L53 实际两端强核＋22 红绿；N61 NoTown／双向 Save、N63／N65 及 N69 活动 Load 与整档门。 | 来源域内通过 | 后续关卡资源/场景域；未验：fresh未写与Loaded已拥有未知保留分层，详共用边界；SAV unused不是热active初值 |
| C17 应用／场景资源 | [src/app_flow.c](../../src/app_flow.c)／[main](../../src/main.c)；`fd2_app_flow_after_battle` | A47、A6 事件调用边；N52／N61 的 0→准备→1→准备→2 实际交接，N63 runtime／details 与 N69 开场／Load。 | 来源域内通过 | 后续entry/result/victory具体私有序列；未验：N1两端载N0原端实档，不是各自START全程单进程链 |
| C18 持久队伍／选阵 | [src/field_prepare.c](../../src/field_prepare.c)／[prepare_play](../../src/field_prepare_play.c)；`fd2_field_prepare_run`／`fd2_save_manual_state` | A47／A30 准备，N52／N61；17／21 finalNO 核真实 owner，非选择前 Save；完整 32×80 roster 保留。 | 来源域内通过 | 必需成员、quota和晚关entry参数；未验：B7六人不能替代17/21人实际owner；finalNO不撤销已完成排序 |
| C19 交易／教会／转职 | [src/field_prepare_play.c](../../src/field_prepare_play.c)／[preparation冷图](../../data/static/flows/preparation.json)；`prepare_ui` | A47／A30 准备 47 正／6 负有限 CPU；N61 服务／库存／NoTown，N63 两服务返回曲 owner 与金额／转职离线内容。 | 来源域内通过 | 城市、商品、条件表及私有剧情；未验：两服务返回 owner 已实测，中段曲 16／17 只限 caller 推断；合法服务与必要呈现不推 S。 |
| C20 通用菜单／详情／总览／F1 | [src/field_info.c](../../src/field_info.c)／[field_play.c](../../src/field_play.c)；`fd2_field_overview_draw`／`runtime.detail`／F1 | A47／N48：newline 真红 44→45／46／47 绿→48 baseline／newline 两 pair；N63 controls／details／stage1／高图及 N65 stage2 F1。 | 来源域内通过 | stage16私有33499(18)条件选择；未验：normal0..29非16，657/658已有prepare consumer；不开放stage9 battle |
| C21 四设置／FULL SHORT | [src/field_game.c](../../src/field_game.c)／[field_play.c](../../src/field_play.c)；`refresh_system_menu_disabled`／`option_values[0..3]` | A47／N84／N63 controls；四设置真实消费，FULL／SHORT 各循原义；N67 options＋N69 精确联合门通过，W66 只是观察器修复。 | 来源域内通过 | 具体场景请求设置的时机；未验：Save byte不证明实际声音/信息板/模式行为 |
| C22 输入／宿主退出 | [src/input.c](../../src/input.c)／[field_play.c](../../src/field_play.c)；`fd2_input_clear_keys`／`play_command_transition` | A47／N84／L56；真实早键／同 owner FIFO、clear／HOST、COMMAND 关闭返回屏障，N63 相关正式菜单与 N69 输入门。 | 来源域内通过 | 无新增通用玩法；仅关卡键绑定；未验：DOSV/全部字符编码/物理typematic/跨窗外延未验；旧首次NO根因不作归因 |
| C23 文本／动态码／通知 | [src/text.c](../../src/text.c)／[scene.c](../../src/scene.c)／[field_play.c](../../src/field_play.c)；`fd2_text_expand_banked`／`progress_wait` | A47／A30 原 1e5c0 九类，N84／N52／N61／N63；W62 保全文＋portrait3，N69 开场原 29 ACK 后停真实 wait。 | 来源域内通过 | 对白/说话者/剧情绑定；未验：消费者各自token合法域；不声明完整文字/RNG同轨 |
| C24 演出／范围FX／横幅 | [src/battle_animation.c](../../src/battle_animation.c)／[magic_fx](../../src/magic_fx.c)／[defeat](../../src/field_defeat.c)；`fd2_scene_arrival_visual_play` | A47／A30 defeat／A6 arrival，N84／N52／N63；arrival 53 节点、空组也 12 轮；转职内容和 child／死亡各按有限原生门。 | 来源域内通过 | 镜头、动画与资源绑定；未验：未证全部目标FX/声音/像素时序；31+只省未定义FX锚不省业务 |
| C25 App页／clock／water／AFM | [src/field_page.c](../../src/field_page.c)／[field_visual.c](../../src/field_visual.c)／[Game](../../src/field_game.c)；`fd2_field_game_runtime_refresh` | A47／A30 water／effect-owner，N84／N63／N65；F1 返场复用共享刷新原序，单 App 工作页；N69 活动 Load 返场。 | 来源域内通过 | 新场景具体资源引用；未验：共享刷新只在原caller调用点推进；列表无raw26、19953无water，不能机械套所有页 |
| C26 PCM／队列／设备 | [src/audio.c](../../src/audio.c)／[audio_pcm.c](../../src/audio_pcm.c)／[field_audio.c](../../src/field_audio.c)；`fd2_field_audio_play` | A47／原 cue 来源／C 生命周期，D 有限域；N63 服务返回 owner，N67＋N69 五场功能／实际音频封包及声明的有限 SFX 身份门。 | 来源域内通过 | cue/bank具体绑定；未验：preOS非设备可听/路由/拔插/全波形，Release125不是正常结束 |
| C27 BGM／XMIDI／调度 | [src/bgm.cpp](../../src/bgm.cpp)／[field_audio.c](../../src/field_audio.c)；`fd2_bgm_play` | A47／既有 15 曲有限原向量、N63 两服务返回曲 11 owner，N67＋N69 设置／Load／开场／END／SFX 五场；D 另保有限域。 | 来源域内通过 | 新场景曲目参数；未验：中段17/16必经caller推断非瞬时owner；保原Nuked响度，不把音色选择当统括豁免 |
| C28 总完成门／残余 | [development](development.md)／本审计；C01—C28、来源账及完成门 | A47／A33／A30，N84／N48 与后续 62/62 联合账、源码／正式入口／支持门及残余实审；已识别通用阻塞闭合，未借禁用合法能力缩范围。 | 来源域内通过 | 具体关卡数据／绑定／真正私有逻辑；S 未启动。全游戏未知字节读者、精度／平台测量见 §13.4；提交与集成按实际收据登记。 |

### 13.3 尾验与完成门

| 编号 | 实际登记 | 最终有限结论 |
|---|---|---|
| T01／T02 | N63 两服务音乐 owner | 真实 RPC／PDB 返回曲 11，P3 loop1／P4 loop0、status4；588／591 全文＋portrait3 闭帧，整档不写。中段 17／16 仅 caller 推断。 |
| T03／T04 | N63 金额／转职内容 | 既有原／SDL 闭帧的原资源金额非终态、old→white→new→服务返回有序重判；是离线内容，不是新 GUI。 |
| T05／T06／T07 | N63 runtime／controls／details | 恢复、完整 Round／活动 Load、四设置／信息板／原 option 恢复、F2／acted Enter 两页两 wait 分别过原门；不以磁盘未写证明完整热 owner／RNG。 |
| T08／T09 | N63 common39 空包／满包 Cancel | 原 callback39 与 SDL 非 shipping alias；child211→父 PC／story11／全 Save 与取消门通过，不冒新 S 自然绑定。 |
| T10／T11 | N63 F1 stage1／高图 alias7 | 两页完整 64,000 像素、marker 相位、真实读一返场和整档保护；高图是最小资源 alias，不开放 map7 战斗。 |
| T12 | N65 F1stage2 新 pair | 64.079 s；原／SDL 两页、marker、返场、整 SAV／mtime、来源／owned／display 通过，不开放 stage2 battle。 |
| T13 | N67 options 两端＋N69 四场八端 | 原五场十角色功能／采集／声明的有限 SFX 门通过；原始 N69 报告仅四场，旧 N67 保持 failed，不合成十端新运行。 |

**最终总守卫：**N69 parent `ok/status=passed`、`effective_qualified_count=expected_effective_count=62`，`audio_remainder_guards` 与 late guard 均通过；App、源码、工具、测试、reader、证书及旧／新闭帧来源封存未变。主树用户 `FD2.SAV` 为 22,987 B、SHA `b0dd9349f449d252910994d796434efa14a1149de8a4ea116de22e0ec62965d7`、mtime_ns `1794076388211866700`，保持原值。outer Job29520 的 worker／timeout／residual／forced 均 0、cleaned1；子 owned 正常结束、无遗留窗口。显示需求已在原线程释放，原 balanced 电源计划未改，未改混音器。

逐项复审后，当前已识别的正常来源通用机制、正式消费、必要呈现及错误／取消／一次提交边界没有剩余实施阻塞。原报告中的 `full_G5_signed_off=false` 是 runner 不自行签全阶段的边界，保持原件；本节依据源码、无窗口、局部原码与分账原生门作通用结论。提交、安全集成和主程序重建由后续实际 Git／`build/g5/local-integration.json` 记录，不预填 self hash，不远程推送，不自动启动 S。

### 13.4 共用限制与精简结论

- fresh未写9B/heap尾没有确定初值；这不证明其全游戏无语义。Loaded已拥有容器以本端preload为准，仅允许有源cache重建与独立view写集。原10010只导入count×80热active；SAV unused另owner，未证明同owner减count后重建正常反例，不以unused SAV播种10c50。
- Natural0原14轮/SDL10轮、Natural1各21轮是各端实测；N1双方载N0原端实际entry。各自队伍/部署交接守卫通过，不推断生存差异仅RNG，也不称跨端全部Save或随机轨迹相同。
- 旧事件whole walk／回滚／Scene重放和waitingLegacy死API已去；已知正式事件由event_resume逐叶驱动。event_log保未绑定来源诊断/停止边界；坏external deferred由common预检和Play诊断门拒，不能声称全库再无容量防御。
- 同input owner FIFO与有限caller read/clear、CLEAR_KEYS/INFO_VISIBLE、turn_post_caller及共享reset20/arrival/defeat/App页复用既有owner；删除重复尾与测试专用scratch，不新增VM/事件总线/第二热状态。W62 借既有视觉 scope，不录音、不发 HOST_W；W64 复用 stage2 storage 来源；W66／W68 复用原资源与既有连续 WGC／返场稳定观察。均不新造采集／继承框架或改变游戏输入，旧失败与各原件保持。
- 首END已采YES/唯一Enter/后继Round2是有限独立资源证据；历史默认NO根因未归因，未采首present不补签。列表无raw26、19953无water等caller差异保留，不机械套共享刷新。
- N69 声音身份只关闭声明的 sample8 有限正证；原端首 PCM 时钟仍未直接量测，绝对 OFF 静音、播放次数／唯一实例、单个 ESC 对应的声音、幅度与绝对时序均未据此证明。原报告的 `negative_absolute_silence_proven=false`、`A7_fidelity_validated=false` 和设备输出未验保持。
- 设备可听/热插拔/路由、全targetFX/SFX、严格PIC绝对时钟/全随机同轨/严格BSS、其它平台没有因本稿扩签。用户选择保Nuked后端响度/音色不扩大其它未验豁免；legacy seed skip、Release125与NULL旧二进制身份继续保留。

### 13.5 本仓库来源与本地收据索引

源码链接用于定位正式消费者，不单以符号存在作完备证明。仓库内可阅读的原义依据如下，完整函数／指令、替身与来源域分别保留：

| 覆盖项 | 已发布来源依据 | 当前结论与旧章节关系 |
|---|---|---|
| C01—C06 | [数据格式](../formats/data-formats.md)、[事件来源分类](../reverse-engineering/common-event-semantics.md)、[来源族清单](../reverse-engineering/common-scope-review.json)、[冷定义工具](../tools/static-data.md) | §3原模块/证据索引继续有效，历史“未实现/未审”标签以本节已证状态补充；1497归属不是全游戏语义完整。 |
| C03—C12 | [通用战斗语义](../reverse-engineering/common-battle-semantics.md)、[规范函数名](../reverse-engineering/function-names.md)、[权威伪C](../generated/ida-canonical/fd2-ida-decomp-all.c) | §9的G2来源/阶段历史保留；完整caller与原字节/局部CPU分开，不将伪C当原源码或猜测ABI。 |
| C13—C17 | [事件语义§7](../reverse-engineering/common-event-semantics.md)、[session与动作提交](../systems/battle-session.md)、[共享运行语义](../reverse-engineering/common-runtime-semantics.md) | §11 G4历史事件能力仍为原候选；§12旧wholewalk问题现由单event_resume逐叶修复，保原六shipping映射。 |
| C18—C19 | [准备服务原义](../reverse-engineering/common-preparation-semantics.md)、[数据格式](../formats/data-formats.md)、[session](../systems/battle-session.md) | §10 G3历史有效域保留；本节补当前17/21/NoTown/库存/双向Save和音频观察修复状态。 |
| C20—C25 | [共享运行语义§8—10](../reverse-engineering/common-runtime-semantics.md)、[输入合同](../systems/input.md)、[图形验收](../tools/gui-testing.md) | §12旧输入/F1/INFO/HOST/clock缺口的有源修复不列精度豁免；总览换行44→48另有直接门。 |
| C26—C28 | [音频逆向](../reverse-engineering/audio.md)、[音频计划](audio.md)、[统一优先级](development.md) | §9—12阶段结果不自动升为G5；设备、NULL、Release与未验测量域单独保留。 |

下表是本地验收收据（**未提交，不随仓库分发**）。`build/...` 从项目根解析；`F:/...` 是本机隔离原生证据。路径＋SHA 封存已经产生的事实，不要求提交大型原件或工作 JSON。父报告封存各子报告／输入／页面／Save 的来源，以下不重复展开每个端点。

| 引用 | 本地收据路径 | SHA-256／有限范围 |
|---|---|---|
| A47 | `build/g5/overview-final-ctest-47.log` | `5f7ca74f1a0ea29816eb35ee12712bdeee8232514c381d7be77e65e968f3e761`；当前生产候选112/112无窗口CTest/77.15s；包含Python注册项，不称112纯C |
| A33 | `build/g5/all-python-33.log` | `de87c248808f952da7f2a347ff0b3155c37c2b1371e2b605bfe206a15ed98868`；Python3.12.10，1376项/819.535s/0failure/0error/2skip；准备6负例另strict路径补过，legacy本地seed未执行；后工具direct不加成一轮全量 |
| A30 | `build/g5/machine-rerun-01/comparison.json` | `77f35d891c933a518a7d08a26972d33929f3000aad286fb88bbdef2ad6b38b75`；四当前C探针+八组局部原码；source/user真，显式替身/参数/完整chunk范围各分列，不是GUI全像素/PCM |
| A4 | `build/g5/final-common-event-source.json` | `d0e36213e47b8e23f0cebb8166c985d02876a395bb689309f4fc83e12991d3ba`；194静态表及独立原资源/域ABI/实际finite消费来源；不是覆盖率 |
| A5 | `build/g5/source-kind-receipt.json` | `8f08926a4c30c2b742f3f78664afa37d87bd13bb1570a25ed6eb8b225e71584c`；1497row/20family（15历史gap/5known rule），622binding/388gap引用/450表外/1default/36script；all_game_semantic_complete仍false |
| A6 | `build/g5/inventory-source-review.md` | `cfb63e04f6d62f9f791657a242d4d9cf2566a350033ee0f22b21c9a1b15ce25e`；205func99318B/库存23049CPU、20parent23callsite、六caller/arrival各限源/显式替身域，旧review快照身份不覆盖全部最新源 |
| N84 | `F:/fd2sdl-evidence/g5-reward-tail-20261111-0104/comparison.json` | `244183c765f52a816fdb0e6d978139b0c6685990b3b1d6d8731f737c77b21895`；61严格继承+23新实做=84，overview48外部精确替代；守卫/正常Job0/display释放真；工具53后只精确validator-only源影响继承 |
| N48 | `F:/fd2sdl-evidence/g5-overview-20261111-0053/comparison.json` | `c1b1312e2f7ccb8a9d4bd1f9f61f56ec37b462c996bcae661cc2aea340b30556`；baseline+原stage9fragment616资源alias两freshpair；4矩形25289px、4退出键、全SAV；不开放stage9battle或stage16private |
| N52 | `F:/fd2sdl-evidence/g5-post-20261111-0305/comparison.json` | `4d87fd6d43f631bcab978a95414a4d142c4fba7aaa5b677e82eb396e3a397144`；已结束failed；前23=8B7+15AI FULL命中原reader有限门，随后item20inconclusive；不是post/G5全过。旧B7review的running是旧时点，不覆写 |
| L53 | `build/g5/loaded-checker-fix-53/receipt.json` | `5ddcd901d132ec768531c5dbde2762a4597c4cfa91a27e0ec460570828cbcb12`；两tool/两pure精确delta，28direct PASS，原actual2 full22987+22旧误接受新拒；原件不写、GUI/业务AST保持，无新GUI |
| Dquiet | `build/g5/device-final-01/comparison.json` | `477325d724f89e4928f3c1c8ba34313d261ba4864fbb56168b6551e56244209e`；sealed WASAPI quiet/preOS 1500ms/153callbacks/153put、retire1/1/1、正常Job/源/用户真；非可听/拔插/全A8 |
| DNULL | `build/g5/native-null-01/comparison.json` | `9e64aa76e1215b022726b4de5e8538f2c489f68ed36fdfc57637a0a2dcb4c132`；native video/无可用音频时title→CONTINUE→Round/browse→emptySYSTEM/Esc正常结束；same_App_binary=false，按精确源影响保有限历史，不新claim |
| DRelease | `build/g5/release-material-01/facts/final-receipt.json` | `f37dd363c41bffcc787c3bdf0836a9e49cb85544ca692ce0418858c4bccf0f15`；仅frozen材料/10producer命令0，oldownedJob125强清理，normal_owned_job_exit_passed=false；不升本候选Release原生/发布 |
| L56 | `build/g5/closing-boundary-review-56.json` | `e5a2fcf6cd6b48729b9f81f2ffb60ce3c2d42bd5a5c3789839b6c44d4594ec1d`；COMMAND close source/10-vector/47回归；首END原37首开已采YES＋唯一Enter到Round2及50严格继承，历史NO未归因，未采首present不补签 |
| W56 | `build/g5/g3-p4-wait-observer-review-56.json` | `21f4140115af0cb56cc6c49b595d8c2bc8d44850c048d1dc7e75be7b017fc605`；56原版591真实blink在216闭帧206/208；单截图超时，旧整场failed/未SDL/无Save保留 |
| W58 | `build/g5/prepare-ready-series-58/receipt.json` | `8159fafd8fe1866d9e591b175aea29b408593603da20c1a0acec28400a17d594`；只换real_wait为同collector backlog并stop封口，26direct；旧失败不补签 |
| W59 | `build/g5/prepare-ready-boundary-review-59.json` | `3a38faf8e41ea532f44caeb68b223fba04ba02e041c7ffff2dba468e9b51565d`；59Original真实Save全22987B零差、228正常stop；SDL命中452后600cap已退出，未ACK/未Save，聚合failed保留 |
| W60 | `build/g5/prepare-ready-boundary-60/receipt.json` | `d35bcf3baf70a144dd43726f53eb0406c15c9fa52de319220ea2817f215d5737`；前键baseline/QPC/唯一trace→只读后继backlog→closed双绑定；30direct/47.452s，prepare/finalizer/save/run/inherit AST与旧常量不变，App不变 |
| MISS | `build/g5/item-miss-proof-54/certificate.json` | `66c40924056ee33dbbaf4a54e1a4ccf5b2ca39beb9d70e71f9f062b7738fde6d`；旧52合法item56/MAP0源帧50..56和完整四glyph MISS两姿态，整Save/sourceguard有限证书；HP_positive仍false，不是新GUI/命中/全FX 永久observer/source准入已完成；不再列作未实施阻塞。 |
| N61 | `F:/fd2sdl-evidence/g5-promotion-retry-20261111-0815/comparison.json` | `012bb3d8f52f109e946b4136aa9266f7b1f8346e947ee08304582f0bb6949bbf`；61已结束failed；前10真实PASS，audioP3原端在打开588的ENTER之前因缺collector停止，SDL未启，无RAM/新Save；旧阶段不升通过。前10按W62默认false精确观察投影与最终reader独立准入继承。 |
| W62 | `build/g5/audio-ready-visual-62/receipt.json` | `3c95585f22c7b15763265a2167e9fbfe031c7ba248a71e88c6a9d28b793b763c`；两个tool＋三个pure精确5delta；两完整module AST观察投影，42direct/73.129s/0skip；App及181生产静态源不变。audio专用33ms视觉scope在原ENTER前ready，baseline/QPC/唯一trace/588591全文+6400portrait3/closed双绑定，正常stop后原单RAM/CDB；不是新GUI或声音身份PASS。 |
| N63 | `F:/fd2sdl-evidence/g5-audio-tail-20261111-0900/comparison.json` | `aef4b88e40bc0d03f2fb23f2de5917440b758ad878423105219c3dac0a0e057f`；已结束failed；T01—T11登记通过（9个原生登记+2个闭帧离线内容登记）；第12 F1stage2仅旧schema预检失败，未建产物/child窗口，audio-five未启。旧false/failed保持，不称一次全fresh。 |
| I63 | `build/g5/f1-preflight-review-63.json` | `5e151cb16768c584bc37e0ae5e21400eb865960e8925cc31925ee3b7eeffd0cb`；准确代码顺序+实际prepare ValueError证明失败发生于output.mkdir/owned/display之前；旧display closure false保持。前11均正常收尾，source/user晚守卫真；不合成未获得的显示释放/child报告。 |
| W64 | `build/g5/f1-source-64/receipt.json` | `851152c72064bf6c6885d513fb4f362e021a5378e13b0e2737a01ae9430c7c34`；仅F1 oracle/runner/pure三delta：stage2显式B7 8guard/positive owner→既有storage.prepare；15ACK对原entry_pages、manual0同槽2570B、18source facts进freeze/late守卫；15direct与真实B7只读prepare通过。stage1/tall及输入/图像/Save/RNG执行AST不变，App/181源不变；不是新F1 stage2原生通过。 |
| N65 | `F:/fd2sdl-evidence/g5-f1-tail-20261111-0950/comparison.json` | `b2f10c5d24c8a80a8a21f3441b38c3823e68871042dedac9dbfc8c5d2a880b0c`；旧65已结束failed；T12 F1stage2新pair实际PASS64.079s/源/全档mtime/owned/display真。audio-five仅options Original通过，SDL在旧灰门拒合法四行而超时，未后4场，旧报告失败不改。 |
| N65-01 | `F:/fd2sdl-evidence/g5-f1-tail-20261111-0950/g5-f1-stage2/comparison.json` | `723a99da45893b0154ad3c4b645e28ccbc39fda0af000647f76074b365a764a5`；同组独立原件，保各自有限域，不自动升总门。 |
| W66 | `build/g5/audio-options-66/receipt.json` | `dbdd2bd65f46f642ba3c51eb8316d34cf329bf5dd5fce8fab34df12df2eaeebd`；仅audio predicate与pure两delta，移除旧SDL index>=2禁用透明分支，四行复用原Original正像素路径；23pure，OriginalOnly完整module相等，旧390闭帧metadata/owner/SHA重核。原frame0/SDLframe2各1904px新门过；不升级未完成SDL业务；App/181源、keys/15s/33ms/30s/600/audio/SAV/RNG不变。 |
| N67 | `F:/fd2sdl-evidence/g5-audio-final-20261111-1030/comparison.json` | `869c3d3fbdd3245e00cd467818fb0a74a8624b35d43168368edc77a3dfffd56c`；旧 aggregate failed：options 两端过，active-load 原端过／SDL 观察过早，后 3 未启；来源／用户／owned／display 收尾通过。 |
| N67 子报告 | `F:/fd2sdl-evidence/g5-audio-final-20261111-1030/audio-five/comparison.json` | `d16caa2a2ec4d3d57a307262f4d0076b0dead9a1d8dbb470230952a4eaddc690`；旧失败原件保持；只将两个 options 已通过角色与 1,124 封存事实按原门严格继承。 |
| W68 | `build/g5/audio-load-observer-68/receipt.json` | `cc6dcc9282c9010e81996630e7600d6ea4a270d5fd5ed58056f10a88378d97e0`；仅工具／pure 两 delta，31/31、4.781 s、原 23 pure AST 保留；完整 module 精确观察投影相同、旧 370 闭帧来源核验。原 45 s 单 deadline／33 ms／600／既有 2 s host quiet；其它 bright15 s／30 s／PCM120 s 与键序不变。 |
| W68 命令勘误 | `build/g5/audio-load-observer-68/command-erratum.json` | `ad497ab35a1cb0d31087004979153093b36e71fc6fabe50104062206dfe8161e`；冻结收据误写 `--python3.12`，实际为 `--python 3.12`；原日志／31 项结果不改、不重跑、不改实现。 |
| N69 最终 parent | `F:/fd2sdl-evidence/g5-audio-remainder-20261111-1115/comparison.json` | `27a2126e602d7e8739f6fca2e0b75a8ef2a87dc11fa5917eb5b34266135a6359`；62/62 与精确五场联合门，source／user／closed evidence／owned／display 全闭。仍保 runner 的 `full_G5_signed_off=false`，不由 runner 代签整个项目。 |
| N69 四场新报告 | `F:/fd2sdl-evidence/g5-audio-remainder-20261111-1115/audio-five/comparison.json` | `d81ab28b28dae012f904e122c6d55094cc4fb0695efecf0d4607780e1b2742b1`；active-load／opening／end-turn／sfx-reenable 各原／SDL 共 8 新端均 passed；音频实际采集与声明的有限 sample8 身份按原门通过，设备可听／波形／绝对时序不扩签。 |
| N69 outer | `build/g5/post-audio-remainder-69.log` | `10e54ed0b062c7888b600f647cbc470562483c05f1476c775664ff4b0b85330d`；Job29520 正常 worker0／timeout0／residual0／forced0／cleaned1，无 owned 遗留。 |
