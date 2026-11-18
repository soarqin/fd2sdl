# 静态数据与 Mod 制作

在 `data/static/` 中可以调整物品、单位、法术、关卡参数和事件流程。CMake 在构建时把这些 JSON 编译进游戏，修改后用新构建的可执行文件测试。地图网格、单位模板、放置、台词和媒体资源通过原有 DAT 文件加载，存档通过 SAV 文件读取。相关布局见[游戏资源与存档格式](data-formats.md)。

## 修改和构建

1. 在准备发布 Mod 的版本上建分支，按下表找到现有记录或流程。在 Windows 和 Linux 上均使用 UTF-8 JSON，数值使用十进制整数。十六进制偏移仅用于文档定位，例如 `0x13` 表示索引 `19`。
2. 保留表的名称、形状和记录顺序。优先修改一处 `values` 或一个已有流程节点的参数。修改引用时检查目标 ID、资源条目或节点标签。
3. 从源码根目录重新构建。首次配置需要 CMake、Python 3.9+ 和项目编译工具链。完整环境要求见[项目构建说明](../../README.md#构建)。

### Windows（PowerShell，Visual Studio）

```powershell
cmake -S . -B build/windows-mod -A x64
cmake --build build/windows-mod --config Release --parallel
```

### Linux（Bash）

```bash
cmake -S . -B build/linux-mod -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux-mod --parallel
```

构建后，从源码根目录运行构建目录里的 `fd2sdl`，用实际资源触发改动路径。测试已有存档时，通过 `FD2SDL_SAVE_PATH` 指向存档副本。要验证初始能力、装备或队伍构造，使用新游戏或会重新创建该单位的场景。已保存的单位记录会从 SAV 恢复。启动和资源准备见[使用指南](../user-guide.md)。

只检查 JSON 和生成头文件时，可在构建命令末尾加 `--target fd2_static_data`。生成器报告错误后，先修正报错的源 JSON，再重新构建。独立原版校验用于核对默认数据，有意修改过的值会被报告为差异。参见[冷定义校验工具](../tools/static-data.md)。

## 按用途找文件

下表路径相对于 `data/static/`。`tables/` 提供按索引读取的记录和参数，`flows/` 提供可分支的流程。同名文件各有独立用途。

| 文件 | 可修改的内容 |
|---|---|
| [`tables/items.json`](../../data/static/tables/items.json) | 物品和装备记录、职业装备许可 |
| [`tables/unit_base.json`](../../data/static/tables/unit_base.json) | 玩家与敌军基础、成长、法术学习 |
| [`tables/magic.json`](../../data/static/tables/magic.json) | 法术记录、职业伤害倍率、地图施法分派参数 |
| [`tables/movement.json`](../../data/static/tables/movement.json) | 移动与普通范围传播的地形成本 |
| [`tables/attack.json`](../../data/static/tables/attack.json) | 地形攻防百分比和各职业的基础暴击阈值 |
| [`tables/stage.json`](../../data/static/tables/stage.json) | 城镇类型、隐藏店热键及商品、关卡音乐 |
| [`tables/preparation.json`](../../data/static/tables/preparation.json) | NoTown、复活费、转职、准备界面布局、服务资源和提示 |
| [`tables/movement_scripts.json`](../../data/static/tables/movement_scripts.json) | 事件中的成组移动、转向和等待 |
| [`tables/common_event.json`](../../data/static/tables/common_event.json) | 共用事件的格子物品映射和嵌套掉落描述符 |
| [`tables/battle_animation.json`](../../data/static/tables/battle_animation.json) | 战斗演出用的颜色、抖动、资源索引、帧数、音效和时序 |
| [`tables/magic_fx.json`](../../data/static/tables/magic_fx.json) | 法术演出用的动画片段、音效 bank、位置偏移和生成参数 |
| [`tables/field_audio.json`](../../data/static/tables/field_audio.json) | 战场脚步类别和 cue 对应的声音 |
| [`tables/field_background.json`](../../data/static/tables/field_background.json) | 按关卡配置的底图、波纹偏移、滚动初值 |
| [`tables/field_game_effects.json`](../../data/static/tables/field_game_effects.json) | 三组地震定点参数，列顺序为 `[x_offset_fp, y_offset_fp, step_fp]` |
| [`tables/field_info.json`](../../data/static/tables/field_info.json) | 战场总览显示的胜负条件文本 |
| [`tables/runtime.json`](../../data/static/tables/runtime.json) | 战场范围动画帧序列、辅助提示的阵营色基值 |
| [`flows/`](../../data/static/flows) | 开场、关卡、奖励、掉落、施法、准备和交互流程。见[流程结构](#流程结构) |

## 普通冷表

文件根对象包含 `schema_version: 2` 和 `tables` 数组。每张表有以下四个必填字段。

| 字段 | 含义 |
|---|---|
| `name` | 引擎使用的 C 数组名，区分大小写 |
| `c_type` | 元素类型。整数类型按相应位宽和有无符号检查。`fd2_field_stage_music` 元素写成两个字节的数组 |
| `dimensions` | 每层数组的长度，例如 `[215, 23]` 表示 215 行、每行 23 字节 |
| `values` | 与 `dimensions` 对应的定长数组，从索引 0 开始。原始字节表中的两字节字段按低字节在前拆开 |

[`manifest.json`](../../data/static/manifest.json) 的 `table_contracts` 固定普通表的文件名、表名、类型和尺寸。数组索引经常就是单位、物品、法术或资源 ID，插入或删除一行会改变后续 ID。未确认用途的字节应保留原值。

### 物品和装备

`ITEM_RECORDS` 为 `215 × 23` 字节，行号为物品 ID `0..214`。以下为常用列。完整调用语义见[物品和范围数据](data-formats.md#战场格子与单位信息面板)。

| 字节索引 | 含义 |
|---|---|
| `0` | 分类，装备许可表用它匹配 |
| `1..2`、`3..4`、`5..6`、`7..8` | 分别为攻击、命中、防御、回避加成，均为有符号 16 位 |
| `9..10` | 武器附加效果类别及其参数 |
| `11..12` | 普通攻击的最小、最大范围。最大范围 `≥16` 时使用十字范围规则 |
| `13` | 使用效果码。`0` 表示物品列表的使用确认不可选 |
| `14..15` | 使用效果参数，具体解释取决于效果码 |
| `16`、`18`、`21` | 玩家使用时的施用范围、效果范围、阵营筛选。AI 路径另读 `17` 作为筛选 |
| `19..20` | 购买价，无符号 16 位。出售按购买价的 `3/4` 向下取整 |

给商店已有的商品定价 500 时，在 `ITEM_RECORDS` 的 `values` 中找到该商品 ID 对应的行，将索引 `19` 改成 `244`、索引 `20` 改成 `1`。重建后检查购买价 500 和出售价 375。商品在哪家店出现由 `k_stage_prepare` 决定。

同文件的 `profiles` 为 `29 × 7` 字节，按职业 profile 直接索引。每行前六项可匹配物品分类，第七项保留原值。两件装备的分组还受物品 ID `<128` 或 `≥128` 约束。

### 单位、成长和移动

| 表 | 行索引和字段 |
|---|---|
| `k_player_base_records` | `32 × 24`，行号为单位 ID `0..31`。索引 `0..2` 为种族、移动 profile、持久队伍创建的初始等级。`3..4` 和 `5..6` 为基础 HP、MP，`7` 为移动力，`8..11` 为初始法术位图。`12..13` 是初始装备 ID，`14..17` 是四个初始携带物品 ID，空携带槽用 `255`。`18..23` 为三组 16 位基础攻击、防御、命中 |
| `k_enemy_records` | `68 × 10`，用 `unit_id - 68` 索引，单位 ID `68..135`。`0..1` 为种族和移动 profile，`2..3` 为 HP 系数，`4..7` 为 MP、攻击、防御、命中系数，`8` 为移动力，`9` 为经验贡献系数 |
| `k_growth_records` | `68 × 11`，行号为单位 ID `0..67`。前十项依次为攻击、防御、命中、最大 HP、最大 MP 的五个数值对。每对第一项用于构造时按等级计算，第二项用于运行中升级。`10` 指向学习表行号，`255` 表示无学习表 |
| `k_learning_records` | `20 × 12`，每行六组 `[level, magic_id]` 依次铺平，法术 ID `0..35`。编辑已启用的配对时须保持合法 ID，空配对保留原值 |
| `k_profiles` | `29 × 20`，用移动 profile 直接索引，列号来自 FDSHAP 地形属性的移动成本类别 `0..19`。第 0 行用于普通攻击和法术等范围传播。单位行中的 profile 使用 `1..28` |
| `k_terrain_attack_percent`、`k_terrain_defense_percent` | 各六项，按地形战斗修正类别 `0..5` 索引，数值单位为百分比 |
| `k_critical_by_profile` | 按 `profile - 1` 索引的基础暴击阈值。武器效果可另加阈值 |

`k_player_base_alias_views` 为两个固定构造窗口。单位 `48` 的 24 字节来自 `k_growth_records` 展平后偏移 `384`，单位 `66` 来自 `k_stage_prepare` 展平后偏移 `68`。调整这两段源数据时需一起验证相关构造场景。初始队伍的成员和部署顺序在 `scene.init.000`，战场模板与放置读取自 `FDFIELD.DAT`。见[单位数据布局](data-formats.md#运行中成长经验与状态字节)。

### 法术和特效

`MAGIC_RECORDS` 为 `36 × 7` 字节，行号为法术 ID。`0..1` 是效果数值，可由不同效果族解释为威力、恢复量等，AI 也读取此值。`2` 是伤害族的命中率。`3..6` 依次为施法范围、效果范围、MP 费用、目标阵营筛选。`MAGIC_DAMAGE_PROFILE_SCALES` 按 `profile - 1` 索引，`10` 表示 1 倍。

地图施法 ID `0..27` 分别通过四张表取得效果 ID（`MAGIC_MAP_EFFECT_IDS`）、费用记录 ID（`MAGIC_MAP_COST_IDS`）、效果目标上限（`MAGIC_MAP_TARGET_LIMITS`，`0` 表示无额外上限）和费用相对 CAST 的时点（`MAGIC_MAP_COST_BEFORE_CAST`，`1` 表示在 CAST 前扣费）。修改费用前先查实际路径。例如默认地图法术 `17` 读取费用记录 `18`，地图法术 `24` 使用记录 `16`，玩家法术 `24` 走独立的 FULL 路径。部分复合效果和传送顺序另见 `flows/magic.json`。见[通用战斗语义](../reverse-engineering/common-battle-semantics.md)。

演出资源的索引跟随各自的原语。表中的资源 ID、帧号和音效索引应与实际的 FIGANI、FDOTHER 条目配套。

| 表 | 索引和字段 |
|---|---|
| `k_battle_cast_colors`、`k_battle_map_cast_preamble` | 前者按法术 ID 取 `[red, green, blue]`。后者按 `[use_magic_rgb, wait_ticks]` 声明地图法术的闪色，`use_magic_rgb` 为 `0` 时写黑色，当前原语要求 `wait_ticks` 为 `1` |
| `k_battle_map_fx_start`、`k_battle_map_fx_count`、`k_battle_map_fx_cue`、`k_battle_map_extra_cues` | 前三张按地图效果核心 ID 取起始帧、帧数、默认音效。附加音效每行为 `[core_id, frame, sample]` |
| `k_battle_short_fx_offsets`、`k_battle_short_fx_sequences` | 先按装备武器的物品分类查偏移，再进入 `[count, frame, cue, ...]` 字节流。cue `255` 表示无音效 |
| `k_battle_map_action_parameters` | 顺序为短演出 bank、恢复 bank、短演出精灵 stride、面板起始帧、面板帧数、面板延时、效果延时、血条延时、反击上方血条宽度。延时单位为毫秒，宽度单位为像素 |
| `k_battle_item_presentations` | 行号为物品效果码减 `5`，覆盖效果码 `5..24`。列顺序为角色着色方式、飘字类型、成功时是否显示数值 0。着色方式 `0..3` 依次为无、单色、渐变、双色。飘字 `0..2` 依次为无、红、绿 |
| `k_fx_friendly_clips`、`k_fx_hostile_clips`、`k_fx_banks` 等 | `magic_fx.json` 按对应特效 ID 选择片段、音效 bank、HP 演出分段和帧基值。`rain`、`shower`、`dust`、`orbit`、`surge`、`burst` 数组由相应特效的粒子序号或相位索引 |

其余演出参数及资源预检见 [`battle_animation.c`](../../src/battle_animation.c) 和 [`magic_fx.c`](../../src/magic_fx.c)。

### 准备和战场表现

| 表 | 行或列的含义 |
|---|---|
| `k_stage_prepare` | `26 × 31` 字节，关卡 `1..26` 以 `stage - 1` 取行。`0` 是城镇类型，`1..2` 是隐藏店的焦点与 BIOS 扫描码。`3..14`、`15..22`、`23..30` 分别是武器店、道具店、隐藏店货单。每段在 `255` 处停止 |
| `k_stage_music` | 关卡 `0..29` 按 `stage` 取 `[primary, alternate]`，值为背景音乐曲目索引 |
| `k_prepare_no_town`、`k_prepare_deployment_roster_gate` | 前者按关卡编号取值，非零时走无城镇准备流程。后者按 `[first_stage, last_stage, active_slot, required_text_id]` 限定一个部署槽的队伍成员 |
| `k_prepare_revive_coefficients`、`k_prepare_promotion_records`、`k_prepare_special_items` | 复活费用为有效职业的系数乘等级。转职记录按 `target_unit_id - 32` 取 `[新职业 profile, 移动力增量]`。特殊物品表按原单位读取转职物品引用 |
| `k_prepare_town_x`、`k_prepare_town_y`、`k_prepare_town_portraits`、`k_prepare_town_background_entries`、`k_prepare_town_service_entries` | 坐标按城镇类型和服务项读取。头像、背景和服务表索引到相应资源。服务选择项 `2` 为出阵，服务资源索引固定为 `255` |
| `k_prepare_buy_messages`、`k_prepare_sell_messages`、`k_prepare_service_return_prefix` | 购买、出售和返回提示的文本片段索引。返回提示的前五项来自 `k_prepare_service_return_prefix`，第六项来自 `k_prepare_revive_coefficients[0]`。复活费用的有效职业索引从 `1` 起 |
| `k_prepare_menu_slide_offsets`、`k_prepare_menu_cursor_offsets`、`k_prepare_notice_portrait_origins` | 菜单滑动、光标位置与特殊肖像的取图原点 |
| `k_sfx_cues`、`k_footstep_classes` | cue 行号采用 `fd2_field_sfx` 枚举顺序，各行为 `[bank, sample]`，支持 FDOTHER bank `31`、`80`、`95` 中的样本。脚步类别按 `profile - 1` 索引 |
| `k_field_background_config` | `33 × 4`，按关卡取 `[kind, fdother_entry, width, height]`。`0` 无底图，`1` 使用 320 × 200 原始图做波纹，`2` 拼接两个连续 RLE 半高图，`3` 使用 312 × 192 滚动图。替换资源时保持与该模式匹配的像素格式和尺寸 |
| `k_field_overview_condition_policy`、`k_field_overview_roster_rules` | 基础三项为 `[片段基址, 每关步长, 败北条件偏移]`。附加规则为 `[stage, text_id, absent_delta, present_delta]`，只调整总览中显示的败北条件文本 |
| `k_field_range_sequence`、`k_field_auxiliary_side_colors` | 20 个动画相位分别引用帧号 `0..22`。辅助色按阵营 `0..2` 查基值，再加闪烁相位 |
| `k_common_event_nested_descriptors`、`k_common_event_cell_items` | 前者存放共用掉落分支使用的三字节描述符。后者以地图格第三个字节的完整值 `0..4` 查物品 ID |

城镇服务、转职条件和参考资源见[准备服务语义](../reverse-engineering/common-preparation-semantics.md)。战场音效枚举见 [`field_audio.h`](../../src/field_audio.h)。

## 过场移动脚本

`movement_scripts.json` 的根对象包含 `schema_version: 2` 和 `scripts`。`scripts` 是以十进制 ID 字符串为键、字节数组为值的对象。ID 范围为 `0..255`，除 `"0"` 外省略前导零。数组依次写入组数和各组数据，每组是 `mode`、参与者数量、相同数量的 `actor, direction` 字节对。长度由这些计数决定，可随编辑改变。

- 每组最多 32 对。`actor` 是当时战场单位表的槽号 `0..95`，且该槽在执行时须已建立。
- 普通方向为 `0` 下、`1` 左、`2` 上、`3` 右。`mode` 为 `0..127` 时表示移动格数。`128..255` 用于转向和等待，低七位为等待帧数。`128` 使用特殊重绘，此模式还允许已确认的方向值 `5`。
- `move` 节点的 `args` 通常为 `[script_id, fade]`。可用 ID 范围取该域的 `operations.move` 定义与现有脚本 ID 的交集。脚本只有通过实际入口引用才会执行。

例如数组 `[1, 2, 1, 0, 3]` 描述一组移动，槽 `0` 的单位向右走两格。可把它写成 `scripts` 中的 `"150": [1, 2, 1, 0, 3]`，再在允许 ID `150` 的场景节点引用它。实测前确认出发槽位、坐标和后续对话锚点。原字节格式见[过场移动脚本](data-formats.md#32-战场过场移动脚本exe-指针表-解释器格式)。

## 流程结构

`flows/` 下每个文件包含 `schema_version: 2`、`domain`、`operations` 和 `flows`，可选 `bindings`。`domain` 与文件名去掉扩展名后的部分一致。

| 领域 | 流程入口和用途 |
|---|---|
| `scene` | `scene.new_game`、`scene.init.NNN`、`scene.opening.NNN`、Game Over。负责新游戏、活动单位准备、镜头和过场 |
| `stage` | `stage.capability.NNN`、`stage.entry.NNN`、`stage.result.NNN`、`stage.victory.NNN` 等。负责关卡支持声明、入场、结果、胜利与交接，包含共享场景子流程 |
| `common_event` | `stage.NNN.action.AAA` 等入口。已接入关卡动作、格子和掉落回调的组合，以及部分具有写入和呈现的关卡结果 |
| `loot` | 掉落、特殊格、确认、换槽、取消和收尾 |
| `progress` | 奖励、升级、学习、状态到期和毒伤 |
| `magic` | 复合法术、传送、物品效果与呈现顺序 |
| `preparation` | 城镇、商店、教会、队伍部署与未经过城镇的准备流程 |
| `runtime` | 回合、阵营、死亡、共享确认、单位详情和到场 |

`operations` 给出操作接受的参数顺序和整数范围。`flows` 中每个流程含 `id`、`entry` 和 `nodes`。`entry` 是默认入口，引用本流程的一个节点 ID。节点包含 `id`、`op`、`args` 和 `on`，可选 `views`。`on` 将原语产生的出口连到本流程的节点。常见出口有 `next`、`yes`、`no`、`cancel` 和 `ack`，终点通常使用 `{}`。允许的出口组合随操作变化，见 `manifest.json` 中该域的 `ports`。

只修改 Mod 数据时，沿用已有 `operations` 和消费契约。新增操作或改变参数契约需要对应的 C 执行器支持。

下面以 `scene.opening.000` 为例，将现有 `delay_02` 的等待时间改为 300 毫秒。`next` 保留原本的后继节点，`present` 指定在演出视图执行。

```json
{"id": "delay_02", "op": "delay", "args": [300], "on": {"next": "text_03"}, "views": ["present"]}
```

- `views` 省略时同时声明 `logic` 与 `present`。执行器按域选择视图和操作。修改 `views` 或调换写入、确认、取消节点前，需要核对同一流程的所有调用方。
- 可选 `entries` 为结构校验声明额外的起点。C 从实际节点 `id` 进入，`manifest.json` 中的 `required_labels` 登记它依赖的标签。重命名内部节点时，同步更新图内引用和入口。保留由调用方查找的流程 ID 与节点 ID。
- `bindings` 将同一域里的新入口名映射到一个真实流程，例如 `"stage.000.action.000": "common.legacy.action.000"`。别名共享节点，只能引用同文件的真实流程。要使事件触发，还要有实际调用方，例如 FDFIELD 的对应回合、格子或掉落动作，以及有效的关卡支持声明。`NNN` 是补足三位的十进制关卡号，`AAA` 是补足三位的动作号。
- 分支、等待、取消和状态写入按图上连线推进。循环必须有可达出口，并符合执行器的容量与步数限制。修改关卡结果时，保留现有只读查询或含写入的结果入口合同。引擎的操作定义、跨表检查与运行边界见[冷定义工具](../tools/static-data.md#事件图的编辑边界)。

## 从版本 1 迁移

以版本 2 的文件和 `manifest.json` 为底稿，迁入旧 Mod 改过的 `values`、节点参数、连线或绑定。版本 1 的 `source`、`sources` 和独立文件名清单已从构建输入移除。移动脚本用原 `source.script_id` 的十进制字符串作为 `scripts` 的键，用原 `values` 作为字节数组。迁移后重新生成并在对应关卡测试，尤其检查资源索引和跨表引用。
