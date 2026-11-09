# 静态数据层与事件流程数据化：调研及实施计划

状态：**SD1—SD3 实施及当前 Windows 支持范围验收完成，实际来源与边界见[阶段记录](stage-log.md#sd3-最终验收与提交门2026-11-09)**。调研日期：2026-11-08；调研代码基线：`88b47d7`，包含 R1 `678ee13`。2026-11-09 用户批准在本会话完成三个阶段及本地集成；优化完成后不自动启动后续玩法开发。优先级入口见[总计划](development.md#4-新优先级与依赖)。

本页保留调研时的盘点与设计样例；已落地的 JSON 字段、命令和执行边界以[冷定义工具](../tools/static-data.md)及代码为准。示例用于说明完整编排范围，不作为第二份可编辑定义。

## 1. 目标、推荐与完成边界

冷数据包括不可变的游戏定义，也包括**事件驱动本身：触发条件、分支、执行顺序、交互等待、取消路径和动作编排**。脚本描述的是运行时如何变化，脚本本身仍是冷数据。例如满包换物的「满包判断 → 是否交换 → 选槽／取消 → 换物 → 提示」应可通过修改静态流程定义改变；不能仅将物品 ID 外置，或用一个 `native_full_inventory_handler` 名称包住原有分支就算完成。

热数据包括背包内容、当前箱内物品、玩家选择、完成标记、脚本执行位置及等待状态。保留现有 session、单位记录、loot／progress、存档 owner 和提交方式，不重写热状态系统。算法与宿主能力仍由 C/C++ 提供，如伤害公式、寻路、库存单步操作、资源解码、输入等待和呈现。

推荐采用 **受控 JSON 定义 → Python 标准库校验／生成 → C11 只读定义及有限流程节点**。运行时使用现有执行接口消费节点，不引入 JSON 解析依赖、不依赖原版 EXE、不要求运行中热重载。修改流程文件后重新生成／构建即可；需要校验的不是只有数值，还包括分支、时点与动作顺序。原本已从 DAT 等加载的数据继续直接读原资源，不复制为第二份 JSON 真值。

实施完成的含义：现有受支持事件的编排都能在冷定义中定位、修改、生成和验证；新增已知组合的关卡事件不再新增逐关 C 分支。全游戏尚未确认的事件先完整登记为未解码／不支持，不承诺本优化阶段解完全部关卡。新增语义仍须取证；静态化不能推导原版未确认玩法。

## 2. 当前基线与证据身份

- 本工作树初始无未提交修改，分支为 `cindy/tranquil-mendeleev`。`AGENTS.md`、`README.md`、总计划、战场／音频计划及[阶段记录](stage-log.md)是现行依据。指定的 `CLAUDE.md`、`.cindy/project-knowledge/TOC.md` 在本树及跟踪文件中均不存在，没有从主工作区复制替代文件。
- R1 已完成 Windows x64 本地发布、MIT 与第三方重建材料范围；不是待续的 B3。仓库没有已命名的 R2／B8。stage 2 目前只有真实入场、浏览、详情、总览和存取，战斗提交与 END 未开放，见[战场计划 §9](battlefield.md#9-b7按场景扩展的剩余能力p2必经项随时前移)。
- 逆向只引用[当前 IDA 伪 C](../generated/ida-canonical/fd2-ida-decomp-all.c)、[命名表](../reverse-engineering/function-names.md)和实现中登记的证据。已读取 `ida-domain-api` 与[IDA 规范](../tools/ida.md)，本次复用既有导出，不操作数据库、不重新确认函数命名。地址均按[canonical relbase VA](../reverse-engineering/address-mapping.md)理解，旧分析工具的 dual／code0 注释不能直接抄作新规范地址。

调研时本树尚无 `original_game/`，只读使用 `E:/Projects/fd2sdl/original_game/` 的共享证据；实施时在隔离树建立只读使用的目录联接供现有测试定位。原资源、存档及数据库不写入、不提交：

| 文件 | SHA-256 | 本次用途 |
|---|---|---|
| `FDFIELD.DAT` | `81310211c6e96ebd89e79648650696691c740229fa4095ece37e7226bc27b74d` | 目录／metadata 结构及真实触发表核对 |
| `FD2.EXE` | `bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd` | 来源身份核对；语义沿用现有 IDA 证据 |

FDFIELD 有 99 个有效条目，即 33 组 stage `0..32`；目录后的 EOF 值不是第 100 个有效条目。按当前布局可读全部 metadata 和 placement；只统计 `turn != 0xff` 的回合记录共 67 条，引用 40 个不同 action。**这个统计不是有效事件全集**：如 stage 8 有两个 `(255,31,0)`，后期还有非空的 turn 255 记录，须保留全部 16 槽，不能因 turn 为 `0xff` 就删除。stage 7 在多个回合重复引用 action 27，也反证不能默认给每个 handler 加一次性标记。这里证明的是数据结构存在，未证明这些后期分支的执行语义。

## 3. 冷数据盘点与迁移收益

### 3.1 已经由资源驱动，继续复用

| 文件／符号 | 原始来源 | 处理 |
|---|---|---|
| `field.c`：`fd2_field_map_open_stage`、`fd2_field_metadata_open_mem`、placements | `FDFIELD.DAT[stage*3 + 0/1/2]` | 地图、触发表、格动作描述和单位模板已加载；增加完整性／语义覆盖校验，不能重新手抄 |
| `shape.c`／`tile.c`、`map_sprite.c` | FDSHAP 地形帧／属性、FDICON.B24 | 定义与引用留在原资源；当前可变 map、TMP／sprite cache 是热或派生数据 |
| `text.c`：`fd2_text_entry_open_mem`、`fd2_text_entry_get_fragment` | FDTXT 的片段和 token | 流程引用 entry／fragment，不复制对白正文 |
| `figani.c`、`audio_pcm.c`、`bgm.cpp` | FIGANI、FDOTHER PCM bank、FDMUS EVNT、SAMPLE.AD | 保留解码／播放器与资源层；静态化 cue 和流程编排，不再转换一套资源包 |

### 3.2 真正需迁移或消除重复的定义

本节 C/C++ 文件均位于 `src/`，表中省略此前缀，函数名缩写处注明所属模块。P0／P1 是本优化阶段交付；P2 为已确认参数的同批整理；P3 不阻塞后续开发。

| 优先级 | 文件／符号与来源 | 迁移内容与收益 | 保留的热状态／算法 |
|---|---|---|---|
| P0 | `field_game.c`：`execute_known_turn_event`、`record_turn_events`、`dispatch_pending_cell_event`；EXE action 表 VA `0x51b91` | 触发绑定、条件和有序动作；消除逐 stage/action 手写编排 | pending 单槽、notice、失败冻结及提交边界 |
| P0 | `field_game.c`：`begin_wait_cell`、`advance_loot`、`fd2_field_game_loot_text_ready/reply`；当前 `LOOT_STEP_*` 分支 | 特殊格、满包换物、掉落／资源／回调／剧情的完整交互流程，不只 mode／param | loot 游标、选择结果、运行 descriptor、库存与完成位 |
| P0 | `field_game.c`：`begin_reward`、`reward_next_level`、`reward_next_field`、`fd2_field_game_progress_ack`、`apply_phase_status_tick`／`advance_status_progress` | 经验／升级／学习提示和 ACK 写点、毒伤／状态到期提示的条件与顺序进入冷流程；不能把现有 progress 状态机整体留作黑盒 | 成长／伤害计算、EX／属性／状态计数和 progress 执行位置 |
| P0 | `scene.c`：`fd2_scene_play_field_event`、新游戏／关卡 entry／victory／defeat；`field_game.c`：`fd2_field_game_prepare_stage1/prepare_stage2`、`fd2_field_game_settlement_project_units`、结果判断；`field_play.c` 实际编排调用点 | 开场、胜负条件及优先级、奖励／结算／跨关事件分支和演出编排；同一事件在 Game 与 Scene 的两套手写顺序合为一个来源 | 应用场景 owner、结算投影／提交、呈现快照与热续态；`app_flow.c` 返回值映射和 `main.c::run_field_chain` 应用 owner／支持门保留 |
| P0 | `field_event.c` 的 `k_stage*_event_move_*`，`scene.c` 的 `k_move_00/01/02/05/5a..69`；EXE object3 `DS:0x27d8` fixup 指针表 | 统一按原 script ID 提取／生成，逻辑与演出共用，不再各处抄数组 | actor 坐标／朝向、六相位播放、相机与音频计数 |
| P1 | `field_unit_base.c`：`k_player_records`／`k_player_initial`／`k_enemy_records`／`k_growth_records`／`k_learning_records`；VA `0x61da1/0x61af9/0x620a1/0x626b3` | 同一基础表与 initial 投影只保留一个来源，生成角色、初始库存、敌军、68×11 成长及 20×12 学习定义 | 创建与运行成长的不同写序／ID 门，现有 HP／MP／EX／已学位 |
| P1 | `field_item.c`：`ITEM_RECORDS[215][23]`、装备 `profiles[29][7]`；getter VA `0x4e8bc/0x4e88e` | 全字节／引用校验，保留未知 byte 和装备许可第 7 byte；数值与效果算法分开 | raw 库存槽、装备 flag、选择及派生值 |
| P1 | `field_magic.c`：`MAGIC_RECORDS[36][7]`、`MAGIC_DAMAGE_PROFILE_SCALES`；getter VA `0x4e866` | 法术定义、能力门与执行语义分列，表有 36 项不等于玩家全支持 | MP、目标、贡献、随机数及伤害公式 |
| P1 | `field_move_profile.c`：`k_profiles[29][20]`，VA `0x61646`；`field.c`：`k_stage_prepare[26][31]`、`k_stage_music[30]`，VA `0x6238d/0x51e63/0x51e81` | 首批可全表精确校验的定义；统一来源、ID 和 getter，降低后续按关接入成本 | 路径缓存、准备页选择、当前音乐及 handle |
| P2 | `field_attack.c`：terrain attack／defense、`critical_by_profile`；`field_audio.c`：`k_footstep_classes`、cue 映射；`magic_fx.c` clip／bank／sample／坐标序列 | 迁移已确认数值和序列，保留原捕获／原函数来源；事件使用这些定义而不嵌常量 | 伤害／舍入、RNG 次序、footstep counter、FX phase 和设备生命周期 |
| P3 | `field_visual.c` 水色、`field_prepare_play.c` 城镇布局、UI 尺寸等 | 有原表来源、复用收益的逐项登记；暂缓无收益的机械外置 | painter 固定几何、格式偏移、stride、C enum／ABI 常量不强行外置 |

本阶段不是把所有数字都放入配置。第三方 Nuked／AIL 内部表、协议和文件格式常量继续由原模块及许可管理。战斗／成长算法不改写为表达式语言；**属于事件选择与编排的条件分支则必须进入冷定义**，不能以「这是逻辑」为由继续留在逐关代码里。

## 4. 关卡事件的完整链路与不确定性

### 4.1 表能直接提取，handler 通常不能直接抄表

```text
FDFIELD[stage*3] map + FDSHAP attrs
    ├─ 普通格 lookup ID → metadata[0x33] → action code
    └─ 箱子／特殊格 action slot → metadata[0x53] → mode、param
FDFIELD[stage*3+1] metadata[0x03] → turn、action、phase
    ↓ 原 slot 顺序与实际调用时点
EXE action 表 VA 0x51b91 / stage 私有 entry、result、victory 分发表
    ↓ fixup 目标 + 原控制流、文本引用、移动脚本
当前 field_event 查询 → field_game 提交与 notice → scene 呈现 → app_flow 交接
    ↓
units／roster／metadata运行副本／completed／pending／loot与progress → save codec
```

`field_turn_event_check @0x1a813`、`field_cell_event_lookup @0x13a44`、`field_movement_script_play @0x1366a` 为规范入口。metadata 固定前缀为 shape／未知两 byte、16×3 回合记录（`0x03`）、16×2 lookup（`0x33`）、16×3 动作（`0x53`）、从 `0x83` 开始的 26 byte 模板；布局详见[格式规范](../formats/data-formats.md)。不能假设 action 记录含有完整剧情，它只是通向控制流的入口。

原始表、fixup 目标与脚本字节可批量提取；if／循环、条件下追加单位、提示后的写点等须从 IDA 控制流和 caller 恢复。现有 `analyze_fdfield_events.py` 是逆向 dump，目录终止和模板匹配有启发式；`analyze_fd2_stage_code.py` 只识别附近 CALL／push，还有旧别名与问号，不能作为完整 CFG 或自动翻译器。复用其已核解析部件，但生成输入只能来自严格校验的表或经过确认的流程转换。

### 4.2 必须保持的顺序与冷热边界

- `fd2_field_turn_events_find` 按 16 槽原序查 turn／phase，不消费记录。一次性／重复触发由实际规则决定，不自动增加 `once=true`。`enter_phase` 当前先回合事件再状态／actor 流程；节点数据化不能挪到绘制回调触发。
- 移动每步 match0 lookup 只在命中时覆盖共享 pending，未命中保留；最终 match1 可能覆盖它。controller／AI outer 在各自原有边界分派／清除，不改成 FIFO，不把 handler 前移到每格移动时。
- 普通 lookup 用地图 byte2 低 5 位的 **1-based** ID 减 1；箱子／特殊格依 terrain `flags & 0x60` 分流，以低 5 位直接作为 **0-based** action／completed 下标。两者不得统一减 1。
- `field_save.c` 的 `export_metadata` 和 `import_current_stage` 保存／恢复 metadata 的已用前缀；`field_game.c` 满包换物将 `metadata.cell_actions[index].param` 改成换出的旧物品。**换物分支是冷脚本，param 的当前值是热数据**，二者不矛盾。新关初值来自静态资源，活动 Load 以 SAV 中有效运行值为准，不用新表覆盖旧档。
- `cell_action_completed[32]`、开箱地图、运行模板副本和 notice／loot 执行进度继续由现有 owner 管理。完成态后 16 byte 当前未用仍保留；不新建第二份 param／HP／装备真值，也不为本次迁移改写原版存档。
- `fd2_field_game_battle_result` 的 stage 0／1 专用判断及覆盖优先级也属于冷规则；不能以通用全灭替代。stage 2 action9 `@0x34818` 含条件控制流，当前未接入；仅提取记录不解除 stage 2 战斗门。

### 4.3 证据等级

| 等级 | 可据此做什么 | 不可据此声称什么 |
|---|---|---|
| 已证实 | 资源字节／fixup、现有 IDA 逻辑与 caller、现有实现及对应测试分别列来源；迁移当前已支持定义 | 静态读取或 SDL 测试不能单独证明原版运行一致 |
| 合理推断 | 同构表可共用提取器，有限节点可复用当前条件与动作；作为待验证设计 | 不能据相似函数、邻近地址或相同 action ID 自动发布语义 |
| 待查 | 后期 turn255、未知 handler、间接跳转、特殊循环、分支写点／RNG 与全流程音画 | 必须登记未解码／隔离项，不能当空事件、无副作用或成功跳过 |

## 5. 静态表示、目录和接口契约

### 5.1 唯一来源与生成加载

拟新增目录（均为未来实施项）：

```text
data/static/manifest.json              格式版本、源身份、定义集合与覆盖状态
data/static/tables/*.json              当前已内置且已确认的冷定义
data/static/flows/*.json               可修改的事件流程与条件／分支／动作
data/static/stages/*.json              stage 到原资源／流程的绑定，不复制 DAT 表
tools/static_data/                     来源提取、严格校验与 C 生成
src/static_data.[ch]                   只读查询及类型接口
<build>/generated/static_data/         生成 C／头和本地提取／覆盖报告，不提交
```

JSON 是项目定义的唯一编辑输入，生成 C 不是另一份可编辑真值。CMake 使用显式依赖生成固定顺序、固定宽度整数的 `const` 数据；源输入、生成器或 schema 变更必须触发重编。Python 成为开发／离线重建的构建依赖，Release 运行不需要 Python。同步更新 Windows PowerShell 与 Linux Bash 构建说明、R1 源码包白名单和离线重建材料，不以开发目录中已有生成物掩盖缺依赖。无需额外 JSON／YAML 库。

`static_data` 提供按原 ID 查询 table／stage／flow 的只读接口；旧 item／magic／profile getter 保留签名并转接。stage 资源打开后做绑定／引用预检，再将初值按现有方式交给 session 的运行副本；不在每帧重读文件，不让静态模块拥有单位或 SAV。`fd2sdl_core` 和 CMake 中独立列源文件的测试目标都必须链接同一数据目标。

来源 manifest 至少记录源文件 SHA-256、archive entry／offset／length 或 LE object／offset／canonical VA、原 getter／callback、提取器版本、定义摘要、语义依据和支持状态。file／object／runtime 地址分开，runtime base 不入静态 ID。全部批量资源转储、EXE 切片和未知字节留本地；只迁移当前项目已携带的定义及项目流程描述，不扩大原资源分发范围，第三方许可保持原样。

### 5.2 稳定标识与兼容

- 原 stage、unit、item、magic、script ID 保持原数值；`0xff`、空槽、重复学习等按原域解释，不能通用归一化。
- 事件实例如 `stage.001.turn.00` 标识原 slot；共享流程如 `stage.001.action.006` 标识 stage/action 绑定；同一个 action 的多个触发保留独立 slot／顺序。流程节点用稳定语义名，不能依 JSON 数组下标保存进度。
- `schema_version` 表示 JSON／生成 ABI，定义摘要用于构建和测试来源；二者不写进 SAV。当前热 enum／step 可由节点显式映射，保留现有 loot／progress 的字段与 owner；先验证映射，不先迁移整个热状态结构。`loot.step` 为 `uint8_t`，生成器必须检查每个流程的编码范围／容量和保留值；容量不足须明确失败、调整流程组织，不能截断。内部编码可按构建生成，不进入 SAV；节点语义 ID 与数据边仍是来源，不能为保留旧 enum 把边写回 C。
- 活动 Load 保留有效 metadata、完整 roster、单位与未知尾字节。四槽准备档仍在入场时按既有入口生成战场，不给其增加活动事件进度。旧档无法表达的新中途保存能力不在本阶段新增。

### 5.3 有限流程节点，不另造通用语言

节点仅覆盖已存在的领域能力：纯条件（库存是否可加入、actor hidden／side、descriptor mode、completed）、有序动作（追加 group、移动、修改 AI 目标、库存单步操作、写回当前格 descriptor、完成格子／刷新图、调用成长预计算与在原写点提交）、分支、交互等待、提示／延时、调用已登记子流程、结束／明确不支持。条件和动作各有固定参数类型；不能写任意地址、嵌入 C／Python、任意表达式、动态函数名或自定义字节码。

分支与边是数据，领域操作是 C。不得新增「执行 stage1 整关」「满包换物全流程」等大原语来藏分支。已有复杂算法可以作为原语，但它不能再检查 stage/action 决定剧情。新原语必须说明为何现有组合不能表达，并附自身语义证据与测试；不为潜在关卡提前增加类型系统、通用 VM、事件总线或插件框架。

同一冷流程供 Game 提交与 Scene 呈现使用：节点标明逻辑／呈现角色和实际提交点，由生成器生成引用同一参数／节点的执行视图。呈现消费已经选定的分支与 notice 快照，不重新评估热条件、不再次扣物品或提交移动。现有有界 notice、text_ready、ACK、失败冻结及取消合同必须保持。已知流程中的等待／循环显式列边；校验无等待的循环、缺失出口、不可达节点及每次推进预算，禁止坏数据让宿主死循环。`resume` 只恢复原等待节点的子阶段，不重新进入并重放整个 message；ACK 只能在其必需 text_ready 完成后接受，重复通知维持现有拒绝语义。

## 6. 对应真实实现的记录样例

以下为**拟议 schema**，不是已经实现的接口。重点是参数、条件、分支和编排都可修改；校验器仍限制每种节点的合法字段和可用能力。

### 6.1 stage 1 第三回合 action6

已只读核对 FDFIELD entry4（文件 offset 6129，长度 1171）slot0 原记录 `(turn=3, action=6, phase=1)`；旧笔误 action5 已在权威伪 C 勘误，正确 callback 为 `field_stage1_turn3_execute @0x34778`。`field_game.c::execute_known_turn_event`、`scene.c::fd2_scene_play_field_event` 与 `field_stage_transition_test.c::test_stage1` 可交叉定位。

```json
{
  "schema_version": 1,
  "id": "stage.001.action.006",
  "binding": {"stage_id": 1, "action_id": 6},
  "trigger_source": {"archive": "FDFIELD.DAT", "entry": 4, "table": "turn_events", "slot": 0},
  "source": {"callback_va": "0x34778", "movement_script_id": 13},
  "repeat_policy": "caller_driven",
  "execution": "commit_then_notice_replay",
  "steps": [
    {"id": "pan", "op": "camera_pan", "use": ["commit", "replay"], "x": 9, "y": 1},
    {"id": "reinforce", "op": "append_group", "use": ["commit", "replay"], "group_id": 3, "placement": "force_original"},
    {"id": "move", "op": "movement_script", "use": ["commit", "replay"], "script_id": 13},
    {"id": "dialog", "op": "text", "use": ["replay"], "archive": "FDTXT.DAT", "entry": 2, "fragment": 4},
    {"id": "villagers", "op": "set_ai_target", "use": ["commit"], "raw_first": 5, "raw_last": 10, "x": 26, "y": 15}
  ]
}
```

触发表仍从 FDFIELD 或活动 SAV 的 metadata 读取，上述 `trigger_source` 是来源引用，不再手写 turn／phase 真值。script13 原数据 VA `0x62ad1`；FDTXT entry2 是 stage1 的 `stage+1`，不复制文本；group3 引用该关模板／placement。raw actor 5..10 是已证实的本关槽，不可改用 unit ID。`commit_then_notice_replay` 保持现有「先提交逻辑终态再播放」合同，`use` 指定节点在提交视图／呈现快照视图中的用途；`replay` 不写 live units。完整引用预检还须检查脚本／原始槽、追加前后人数和资源。

原版在 PAN 后有 100 ms、MOVE 后有 200 ms 延时，并在对白后写 AI 目标；当前 SDL action6 的 Scene 分支没有这两个显式延时，Game 先提交逻辑终态，再由 Scene 从前态重放镜头／移动／对白。上例按当前 SDL 顺序列出迁移输入，不能当作已等价于原版的完整时间脚本。迁移首先保持现有提交／呈现边界，通过同一节点来源生成两种视图；不能顺便改成新的逐节点热事务或悄悄加入原版延时。原版写点／时序差异单列证据和后续修复范围。这里无新增 consumed bit，原 caller 决定何时进入；重复进入触发冲突应沿现有失败路径处理。

### 6.2 箱子 mode0：满包、换物与取消均为静态流程

依据 `field_game.c::begin_wait_cell`（约 5048 行）、`fd2_field_game_loot_text_ready`（约 5091 行）、`fd2_field_game_loot_reply`（约 5111 行）及 `field_loot_test.c::test_wait_cell`。以下完整覆盖 mode0 的入口、空位、满包和交换出口；其它 mode 另有明确流程，不能落入此例。

```json
{
  "schema_version": 1,
  "id": "cell.item",
  "parameters": ["actor", "cell_index", "cell_flags", "current_descriptor"],
  "entry": "ask_open",
  "nodes": [
    {"id": "ask_open", "op": "choice", "fragment_by_flags20": [421, 428], "open": true, "yes": "open_delay", "no": "decline_delay", "cancel": "decline_delay"},
    {"id": "decline_delay", "op": "delay", "ms": 100, "next": "declined"},
    {"id": "declined", "op": "message", "fragment": 412, "append": true, "close": true, "delay_ms": 200, "next": "leave_uncompleted"},
    {"id": "open_delay", "op": "delay", "ms": 300, "next": "capacity"},
    {"id": "capacity", "op": "branch", "condition": "inventory_can_add_current_item", "yes": "received", "no": "full"},
    {"id": "received", "op": "message", "fragment_by_flags20": [422, 429], "append": true, "close": true, "wait_key": true, "text_ready": "grant", "ack": "complete"},
    {"id": "grant", "op": "inventory_add_current_item", "mark_cell_completed": true, "resume": "received"},
    {"id": "full", "op": "message", "fragment_by_flags20": [422, 429], "append": true, "close": true, "wait_key": true, "text_ready": "assert_still_full", "ack": "full_delay"},
    {"id": "assert_still_full", "op": "assert_inventory_full", "resume": "full"},
    {"id": "full_delay", "op": "delay", "ms": 100, "next": "ask_swap"},
    {"id": "ask_swap", "op": "choice", "fragment": 423, "open": true, "yes": "pick", "no": "no_swap", "cancel": "no_swap"},
    {"id": "no_swap", "op": "message", "fragment": 424, "append": true, "close": true, "delay_ms": 200, "next": "leave_uncompleted"},
    {"id": "pick", "op": "inventory_select", "close_box": true, "slot": "remember_old", "cancel": "pick_cancel_delay"},
    {"id": "pick_cancel_delay", "op": "delay", "ms": 100, "next": "pick_cancelled"},
    {"id": "pick_cancelled", "op": "message", "fragment": 424, "open": true, "close": true, "delay_ms": 200, "next": "leave_uncompleted"},
    {"id": "remember_old", "op": "remember_selected_item", "next": "remove"},
    {"id": "remove", "op": "inventory_remove_selected", "next": "add"},
    {"id": "add", "op": "inventory_add_current_item", "next": "store_old"},
    {"id": "store_old", "op": "cell_param_set_remembered_item", "next": "swap_delay"},
    {"id": "swap_delay", "op": "delay", "ms": 100, "next": "swapped"},
    {"id": "swapped", "op": "message", "fragment": 425, "open": true, "close": true, "wait_key": true, "new_item_ref": "current_descriptor.item", "old_item_ref": "remembered_item", "ack": "leave_uncompleted"},
    {"id": "complete", "op": "finish_cell", "completed": true},
    {"id": "leave_uncompleted", "op": "finish_cell", "completed": false}
  ]
}
```

`fragment_by_flags20` 按「有 0x20／无 0x20」取第一／第二项，是有限资源选择，不是表达式语言；`current_descriptor` 绑定入口从热 metadata 复制到现有 `loot.descriptors` 的本次快照，不把物品 ID 固化进脚本。换物写回的是 metadata 当前 param，提示 425 则仍引用原箱内物品和 remembered_item 两个值，见 `field_play.c::loot_text_prepare`。mode0 入口的 mode 分派在上级特殊格流程中显式描述。提示的默认 false 字段与合法组合须在 schema 中固定，缺失的必需边必须拒绝。

`text_ready` 是现有写入停点，不能挪到打开提示之前或 ACK 之后。分支预览只读；到写点重验库存，变化不符时沿现有 fail_action 冻结。node／resume 表达等待子阶段，执行位置、是否已收到 text_ready、所选槽及 remembered_item 使用现有 loot 热字段；重复通知不能重复加物品。库存槽校验、压槽和追加沿用既有原语，派生重算只保留原本存在的调用点，不为箱内换物新增重算。所有原语错误均进入现有失败路径；选择非法 raw 槽按现有拒绝语义处理，不执行后续边。

成功空位拾取置完成位并刷新箱图；**交换后不置完成位、不改箱图，箱内 param 改成换出的物品，允许再触发**。取消同样不置完成位；「结束当前动作」不等于「永久完成该格」。静态分支和运行副作用必须分别测试。将这种完整分支图保存为冷数据，才满足事件驱动分离目标。

## 7. 可跨关复用的提取与接入流程

1. **冻结来源**：记录原资源／EXE 摘要和 LE manifest，复用严格 page/fixup 解析、`fd2_movement_oracle.py`、`fd2_stage1_oracle.py`、opening／transition oracle 的已确认读取。输入身份不符直接失败，不用历史固定 file offset 猜另一个版本。
2. **批量取结构**：遍历全部 33 组资源和各分发表的实际范围（stage entry 表当前按 30 项，不能外推为 33），输出每个原槽、引用、目标、字节跨度与摘要。保留重复、未知 byte、非空 turn255 和未识别 mode。严格检查目录边界、template／placement 与脚本完整消费；不以当前启发式模板匹配兜底。
3. **恢复流程并分类**：有界的原始表直接生成；控制流按 Domain API 读取函数／CFG／xref，复用已确认的条件和动作组合。每个 branch、return、共享尾、间接调用与状态写点都要有对应节点或未解码项。自动识别只能提出候选，不能自动提升为已确认。记录转入 flow 的源范围和未覆盖基本块／边。
4. **形成唯一输入**：已确认流程进入 `flows/`；与原资源已有的触发表、动作参数只建立引用；不可解释项登记来源与阻塞原因，继续保持不支持。正常已支持流程中不允许用大 native handler 绕过数据定义；临时保留旧实现仅用于替换差分，须有移除阶段。
5. **常规新关接入**：批量生成 stage 绑定／引用 → 已知 flow 组合与参数核对 → schema／源字节／引用／覆盖检查 → 同输入状态与有序轨迹差分 → 按新增语义选择少量原版代表场景。只改数据即可表达的关卡，不再新增 stage 分支、复制执行器或逐事件人工校准。
6. **遇到新语义**：未知节点／必需 handler 明确停止或禁止开放该场景。原始结构提取可成功，语义覆盖结果必须失败／列未支持，二者不能共用一个绿色结果。取证后新增最小条件／动作、回归及代表性实机证据，再复用到其它关卡；缺证据时保持「待原版对照」。

流程节点不需要映射每条 x86 指令，但所有影响业务、交互、RNG 或顺序的路径都必须有对应解释。优化目标是把重复提取、配置校准和语义验证拆开复用，不能声称以后所有关卡只需提表即可等价。

## 8. 验证策略与可执行验收清单

| 层次 | 自动验收内容 | 复用基础与限制 |
|---|---|---|
| 定义与生成 | schema 版本、整数范围、ID 唯一、引用有效、节点出口／可达性、无非法循环、稳定输出；未知 node／mode／未确认绑定明确失败；修改 JSON 分支应改变生成路径 | 新增 Python 单测与生成 check 模式；不能只测生成器自己输出能被自己读回 |
| 源数据全量 | 每张迁移表的原字节／投影、长度、摘要、所有 slot 和指针目标；批量报告结构总数、解码数、未解码数、支持数分别统计 | `field_stage_tables_test` 已独立比对 806 准备 byte 和 60 音乐 byte；movement oracle 完整 580 byte。独立 oracle 不能引用同一生成常量作 expected |
| 执行语义 | turn／cell／特殊格／掉落／开场／结果／结算／经验成长与 phase status 每种当前受支持节点与边；顺序、优先级、取消、错误、重复通知、一次性／重复、text_ready／ACK、隐藏 actor 和随机消费；覆盖提示 488→489→490..494→587 及属性／EX 的原写点 | `field_event`、`field_turn`、`field_loot`、`field_progress`、`field_progress_play`、`field_game_failure`、`field_player_controller`、`field_ai_commit`、`field_stage_transition`；新增分支图变异测试防止实现仍走硬编码 |
| 迁移差分 | 相同初态／资源／输入与注入 RNG 下，旧／新有序 trace、状态写入和错误位置一致，呈现不重复提交 | trace 至少有 stage、源slot／action、node、调用时点、输入结果、RNG 次数、写集、notice／ACK；利用已有 notice、oracle，按需补局部取证，不搭通用日志框架 |
| 存档兼容 | stage0／1 活动、stage2 入场、四槽准备、满包换物后重入／Load、旧完成态重建；未知尾、32 人 roster 与 metadata 热值保留；失败 Load 不换 owner | `field_save`、`save`、`field_manual_slot`、stage2 storage／stage1 complete oracle；完整 SAV 并非跨端 RNG checkpoint |
| 原版代表性场景 | 本阶段变化涉及的触发／动作组合与交互时点；新增语义定向实机，不逐关逐事件重复 | 首批包含回合增援、普通格 pending 覆盖、箱子空位／换物／取消、村民结果覆盖与结算分支。选用现有夹具／runner，重复数据差异用批量校验闭合 |

阶段定向检查只覆盖新增、改动和直接受影响的共享路径；大阶段末统一审查、全量回归当前支持历史功能、修复并精简后提交。全量范围以[图形规范](../tools/gui-testing.md)和阶段记录中实际已支持历史入口清单冻结，不是重新遍历所有原版关卡。修复后复验受影响范围，不因每次构建重复全部旧实机流程。

Windows 实机场景使用 WGC HWND 单窗口、当前拥有 PID／HWND 的后台输入；图形串行，成功／失败／超时均清理自己的窗口和子进程。保持用户混音器静音，不更改电源计划；显示需求、采集校验和退出按既有规范执行。Linux 保留独立 X11／Xvfb 说明，本轮未执行的平台如实注明。正式命令维护于[工具速查](../tools/README.md)及[跨平台约定](../tools/platforms.md)，本文件不新增一套平行命令。

实施验收报告必须分别回答：

- [ ] SDL 自动测试：哪些表、流程节点、边、错误／取消和存档路径实际通过，哪些未执行？
- [ ] 静态／局部机器码：源身份、全表范围与 CFG 覆盖是什么，是否真的执行了局部原指令？只读原字节不得记为机器码执行。
- [ ] 原版运行：哪些代表场景、判据、截图／trace／SAV 已取得，哪些仍待原版对照？
- [ ] 全部提取记录能否归入「已支持／已解码但未开放／未解码」且数量闭合？未知项是否会明确阻止相应场景，而非静默跳过？
- [ ] 数据层是否真正控制分支和顺序？主实现是否仍含 stage/action 特判、藏完整剧情的大原语或重复流程？

## 9. 三个完整实施阶段与提交单元

以下为工作包定义，实际完成范围以阶段记录为准。每个 SD 编号为一个完整交付与提交单元，不拆出微阶段提交；阶段内持续推进，不逐步骤停下请求「继续」。遇到证据／依赖卡点可以调整范围，但须保留阻塞和验收边界。

| 阶段 | 目标与文件范围 | 依赖 | 验收与提交门 |
|---|---|---|---|
| **SD1：建立静态来源与冷表构建链** | 新增 `data/static/`、`tools/static_data/`、`src/static_data.[ch]`；迁移 §3 的 P1 及已确认 P2 参数；接 `CMakeLists.txt`、独立测试目标、构建／离线包说明。冻结 P0 事件清单和有限节点 schema | 当前 R1 基线、合法只读原资源和现有严格 LE／IDA 证据 | 所迁表全部字段／未知 byte 与独立来源一致；同源投影不重复维护；getter／ID／越界及运行支持门不变；生成可复现且依赖有效；Windows 当前平台定向逻辑与必要受影响实机通过，另一平台注明未执行。提交完整 SD1 |
| **SD2：分离现有受支持的全部事件驱动** | `flows/`、`stages/`；`field_event`、`field_game`、`scene`、`field_play`、`app_flow`、`main` 相关调用及测试；覆盖回合／普通格／特殊格／掉落、满包换物、自然开场、现有 entry／胜败／奖励／结算／交接，及经验／升级／学习提示、毒伤／状态到期的 progress 编排。复用现有 loot／progress／notice 热字段，不扩展玩法 | SD1；每个现有流程的分支／写点／等待清单 | 上述流程所有已支持分支与顺序由冷定义决定；两个真实样例、progress ACK 写点及取消／重复／失败通过；旧／新同输入 trace 与热状态差分一致，存档兼容通过；当前平台代表原版场景完成。删除替换后的手写编排与测试专用旧执行路径，不把整段逻辑藏进 native action。提交完整 SD2 |
| **SD3：批量提取闭合与大阶段收口** | 完成全部资源组／实际分发表的结构提取与覆盖报告；已有语义跨关组合和未解码隔离；生成／校验测试及文档规范。更新总计划／阶段记录／系统边界及必要 AGENTS 索引 | SD1、SD2；同一来源版本、代表场景证据 | 33 组原表和各真实分发表范围数量闭合，原始／已解码／已支持分别报告；用重复 action／不同 stage 绑定的批量 fixture 证明不加 C 分支即可组合，未知项负例必须失败；不开放 stage2 新战斗。统一审查当前受支持历史功能，当前平台全量回归、修复精简、删除冗余后验证并提交完整 SD3 |

SD3 的全量回归是本优化大阶段的最终交付门，不能用批量结构读取代替。下一轮 stage 2 战斗、更多法术／商店／部署或新关卡工作，在 SD 大阶段完成后另定，不在 SD 大阶段完成后自动续跑。

## 10. 渐进替换、风险与长期规范

先固定独立基线与可重放初态，再让旧 getter 转接新数据；事件按完整可运行场景迁移，最终 SD2 必须覆盖全部当前支持编排。过渡期间旧路径只用于测试差分，不提供用户长期双后端开关；每条流程验收后移除原分支，避免双份真相。每阶段可通过普通 revert 回到上一个完整提交，不改变原 SAV 格式、不写迁移版本、不覆盖用户存档，因此回退无需转换存档。

| 风险／前置 | 处理与影响 |
|---|---|
| metadata 同时含初值与运行值 | 冷流程读取 session 当前有效 descriptor；SAV 的已知运行字段与未知尾保留。禁止直接 const 化整个 metadata 或新增平行热覆盖表 |
| 原函数不是简单列表，含共享尾／条件／回调 | 按 CFG 边和原调用点登记；未覆盖路径不支持。现有线性 CALL 扫描只作候选，不自动生成成功流程 |
| Game 与 Scene 顺序／提交时点不同 | 一个冷来源生成两个受限视图，呈现只用已选路径与快照；现有事务／错误／资源预检不变。与原版的已知中途时序差异另列，不能悄悄「修正」 |
| 分支图变成另一套编程语言 | 只加当前事件所需的固定类型节点；不加任意表达式／任意内存操作。既不能过度抽象，也不能以大 callback 规避所有事件驱动外置 |
| 新构建依赖或生成缓存失效 | Python 标准库、CMake 显式依赖、普通项目构建目录及干净离线重建；按平台规范核验 MSVC FileTracker，不使用 Temp／LocalAppData 迭代缓存冒充安全 |
| 批量提取原资源被混进提交／发布 | 只提交已有分发边界内的定义与自有流程／生成脚本；完整原表 dump、EXE／DAT 切片、截图和数据库留本地。发布白名单明确排除 |
| 未知后期事件拖住整个优化 | SD3 要求完整盘点与拒绝路径，不要求发明未确认语义；后续新增能力按已登记阻塞取证，不回退为逐关复制 C handler |

后续开发必须遵循：新增或修改冷定义／事件编排首先改 `data/static/` 的唯一输入及来源；原有资源字段只建立引用；纯查询不写热状态；代码只新增确需的领域原语；必需事件缺失明确失败；任何遗留逐关特判必须在审查中解释并登记移除范围。代码审查检查源清单、schema／引用、修改数据能否影响分支、独立差分和原版覆盖，不能以「数组已搬到新文件」验收。总计划持续引用本规范，不将规则埋在完成日志里。

**当前没有必须由用户决定才能写定的技术取舍。**推荐已选择构建期可修改流程数据、有限领域节点、现有热 owner 与存档兼容；未引入运行中编辑器／热重载、通用 VM 或全关卡玩法开放。若后续明确需要无需重建即可修改脚本，或支持目前 SAV 不承载的中途恢复，再单独决定加载／兼容方案，不作为本优化阶段的隐含需求。

## 11. 调研交付的历史验证边界

2026-11-08 调研交付只读核实代码／符号、权威 IDA 导出和现有测试判据，交叉核对共享原资源身份、FDFIELD 目录／metadata／placement 与真实 action6 记录，完成文档链接／格式和范围自审。未实施静态层／流程执行器，未运行 SDL 自动测试、局部原机器码或 DOSBox 原版运行，也未跑历史 GUI。上述实施验收均是未来各 SD 阶段的门，不能把本次调研计为这些测试已通过。
