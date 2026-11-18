# 静态数据与 Mod 制作

在 `data/static/` 中可以调整物品、单位、法术、关卡参数和事件流程。CMake 在构建时把这些 JSON 编译进游戏，修改后用新构建的可执行文件测试。地图网格、单位模板、放置、台词和媒体资源通过原有 DAT 文件加载，存档通过 SAV 文件读取。相关布局见[游戏资源与存档格式](data-formats.md)。

## 修改和构建

1. 在准备发布 Mod 的版本上建分支，按下表找到记录或流程。使用 UTF-8 JSON，数值写成十进制整数。
2. 按 ID 找到记录并修改具名字段，或修改现有流程节点。保留必填字段、固定表的 ID 集合和有序数组的长度。修改引用时核对目标 ID、资源条目或节点标签。
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

下表路径相对于 `data/static/`。`tables/` 的记录按 ID 或数组索引读取，`flows/` 提供可分支的流程。同名文件各有独立用途。

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
| [`tables/field_game_effects.json`](../../data/static/tables/field_game_effects.json) | 三组地震定点参数 `x_offset_fp`、`y_offset_fp`、`step_fp` |
| [`tables/field_info.json`](../../data/static/tables/field_info.json) | 战场总览显示的胜负条件文本 |
| [`tables/runtime.json`](../../data/static/tables/runtime.json) | 战场范围动画帧序列、辅助提示的阵营色基值 |
| [`flows/`](../../data/static/flows) | 开场、关卡、奖励、掉落、施法、准备和交互流程。见[流程结构](#流程结构) |

## 普通冷表

`tables/` 下的文件使用 `schema_version: 3`，根对象直接保存具名集合或参数。按 ID 查找的集合用十进制字符串作键，例如 `items["52"]`、`spells["17"]`、`stages["1"]`。键的书写顺序不影响 ID。相同含义的成本、帧序列、库存槽等按顺序写成数组，数组索引参与运行时查找。

生成器按 [`table_schema.py`](../../tools/static_data/table_schema.py) 中的字段映射读取 JSON，按 [`manifest.json`](../../data/static/manifest.json) 的 `table_contracts` 检查冷表投影的类型和尺寸。带 `unknown_` 前缀的字段保留原值，后缀是十六进制字节偏移。需要核对或重新导出默认值时，见[从原版 FD2.EXE 导出冷表](../tools/static-data.md#从原版-fd2exe-导出冷表)。

### 物品和装备

`items.json` 的 `items` 按物品 ID `0..214` 索引。常用字段如下。原版消费布局见[物品和范围数据](data-formats.md#战场格子与单位信息面板)。

| 字段 | 含义 |
|---|---|
| `category` | 装备许可使用的物品分类 |
| `attack_bonus`、`hit_bonus`、`defense_bonus`、`evasion_bonus` | 有符号 16 位装备加成 |
| `weapon_effect`、`weapon_effect_parameter` | 武器附加效果的类别和参数 |
| `attack_min_range`、`attack_max_range` | 普通攻击范围。最大值大于或等于 16 时使用十字范围规则 |
| `use_effect`、`use_parameter` | 使用效果码和原 16 位参数。效果码为 0 时，物品列表禁用使用确认。参数的解释由效果决定，填写原字的无符号数值 |
| `use_range`、`effect_range`、`target_filter`、`ai_target_filter` | 施用范围、效果范围、玩家阵营筛选和 AI 筛选 |
| `buy_price` | 无符号 16 位购买价。出售价为购买价的 `3/4`，向下取整 |
| `unknown_16` | 原记录偏移 `0x16` 的字节 |

例如要把关卡 1 道具店中物品 ID `192` 的购买价改为 500，将 `items["192"].buy_price` 设为 `500`。重建后检查购买价 500 和出售价 375。各商店的货单见 `stage.json`。

`equipment_profiles` 按职业 profile `0..28` 索引，每条记录含六项 `allowed_categories` 与保留字段 `unknown_06`。装备分组按物品 ID 小于 128 或大于等于 128 区分。

### 单位、成长和移动

| 集合 | ID 或索引 | 字段 |
|---|---|---|
| `unit_base.json` 的 `players` | 单位 ID `0..31` | `race`、`movement_profile`、`initial_level`、`base_hp`、`base_mp`、`movement_points`、`initial_magic_mask`（32 位法术位图）、两件 `equipment`、四个 `inventory` 槽、`base_attack`、`base_defense`、`base_accuracy`。空携带槽为 255 |
| `enemies` | 单位 ID `68..135` | `race`、`movement_profile`、`movement_points`、`experience_coefficient`，以及 HP、MP、攻击、防御和命中的 `*_per_level` 系数 |
| `growth` | 单位 ID `0..67` | 攻击、防御、命中、最大 HP 和最大 MP 各有 `*_per_level` 与 `*_on_level_up` 两项。前者用于按等级构造，后者用于运行中升级。`learning_id` 为 255 时表示无学习表 |
| `learning` | 学习表索引 `0..19` | 每行六条 `{level, magic_id}`，法术 ID 范围为 `0..35`。空配对保留原值 |
| `movement.json` 的 `cost_profiles` | profile `0..28` | 每行 20 项，列号来自 FDSHAP 的地形成本类别 `0..19`。第 0 行用于普通攻击和法术等范围传播。单位使用 profile `1..28` |
| `attack.json` 的 `terrain`、`critical_thresholds` | 地形战斗修正类别 `0..5`、暴击职业索引 `profile - 1` | `terrain` 的每行含 `attack_percent` 和 `defense_percent`。`critical_thresholds` 是基础暴击阈值，武器效果可另加阈值 |

`constructor_aliases` 登记两个原版构造窗口。单位 48 的 24 字节来自 `growth` 编码后偏移 384，单位 66 来自 `stages[*].preparation` 编码后偏移 68。涉及这两个窗口的改动需要一起验证构造场景。初始成员和部署顺序由 `scene.init.000` 指定，战场模板和放置读取自 `FDFIELD.DAT`。见[单位数据布局](data-formats.md#运行中成长经验与状态字节)。

### 法术和特效

`magic.json` 的 `spells` 以法术 ID `0..35` 为键，每个键下有一条完整记录。法术的基础字段是 `effect_value`（16 位效果数值，解释取决于效果族）、`accuracy`（伤害族命中率）、`cast_range`、`effect_range`、`mp_cost` 和 `target_filter`。`damage_scale_by_profile` 以 `profile - 1` 为索引，值 10 表示 1 倍。

地图法术 ID `0..27` 的同一条记录下还有 `map`，包含 `effect_id`、`cost_record_id`、`target_limit` 和 `cost_before_cast`。`target_limit` 为 0 时没有额外上限，`cost_before_cast` 为 1 时在 CAST 前扣费。地图法术 17 默认读取 `spells["18"].mp_cost`。地图法术 24 的效果和费用引用都是 16，玩家法术 24 走独立的 FULL 路径。复合效果和传送顺序另见 `flows/magic.json`。见[通用战斗语义](../reverse-engineering/common-battle-semantics.md)。

演出资源的 ID、帧号和音效样本需要与实际的 FIGANI、FDOTHER 条目配套。

| 集合 | 索引和字段 |
|---|---|
| `battle_animation.json` 的 `cast_colors`、`map_cast_preamble` | 前者按法术 ID 取 `red`、`green`、`blue`。后者是闪色步骤，`use_magic_rgb` 为 0 时写黑色，当前原语要求 `wait_ticks` 为 1 |
| `map_fx`、`map_extra_cues` | `map_fx` 按效果核心 ID 取 `first_frame`、`frame_count`、`initial_sample`。核心 `0..29` 另有 `damage_tint`。附加音效按 `core_id`、`frame` 和 `sample` 配置 |
| `short_fx_sequence_by_category`、`short_fx_sequences` | 武器分类指向六条共享演出序列之一。每条序列内按顺序写 `frame` 和 `sample`，样本 255 表示无音效。六条序列共 39 个帧／样本对 |
| `map_action` | 短演出和恢复演出的音效 bank、原精灵 stride、面板首帧和帧数、三处毫秒延时、反击上方血条宽度 |
| `composite_fx`、`heal_masks` | 分别以法术 ID `32..35` 和 `13..16` 为键，配置颜色和音效 bank，或初始半径和每帧步长 |
| `item_presentations` | 以物品效果码 `5..24` 为键，配置 `actor_style`、`popup_kind`、`numeric_zero_on_success`。着色方式 `0..3` 为无、单色、渐变、双色。飘字 `0..2` 为无、红、绿 |
| `magic_fx.json` 的 `effects` | 按 FULL 特效 ID `0..9` 取 `hostile_clip`、`sfx_bank`、`hp_denominator`、最小样本和片段数量、命中帧及返回帧基值。ID `0..8` 有 `friendly_clip` |
| `rain0`、`rain1`、`shower`、`orbit` | 依对应特效的粒子序号或相位索引原坐标和演出参数。`dust_x`、`surge_x` 和 `burst_offsets` 是单列序列 |

其余演出参数和资源预检见 [`battle_animation.c`](../../src/battle_animation.c) 和 [`magic_fx.c`](../../src/magic_fx.c)。

### 准备和战场表现

| 集合 | ID、字段或单位 |
|---|---|
| `stage.json` 的 `stages` | 关卡 ID `0..29` 各有 `music.primary` 和 `music.alternate`。关卡 `1..26` 另含 `preparation`，内有 `town_type`、`hidden_shop_focus`、`hidden_shop_scan_code` 和三份货单 `weapon_stock`（12 项）、`item_stock`（8 项）、`hidden_stock`（8 项）。每份货单在 255 处停止 |
| `preparation.json` 的 `no_town_by_stage` 和 `deployment_roster_rules` | 前者按关卡查无城镇开关。后者按 `first_stage`、`last_stage`、`active_slot` 和 `required_text_id` 限定部署成员 |
| `revive_coefficient_by_profile`、`promotions`、`promotion_item_by_unit` | 复活费用为系数乘等级，索引为 `profile - 1`。转职记录以目标单位 ID `32..67` 为键，含 `movement_profile` 和 `movement_bonus`。转职物品按原单位索引 |
| `towns`、`services` | `towns` 按城镇类型取 `background_entry` 和六个 `service_positions`。`services` 按服务索引存头像、FDOTHER 条目及购买、出售和返回提示的文本片段。服务 2 是出阵，`fdother_entry` 固定为 255。第六个服务的 `return_fragment` 与原复活表索引 0 共址 |
| `menu_offsets`、`notice_origins_by_portrait` | 前者按菜单项取 `slide` 和 `cursor`，后者按特殊肖像 ID 减 128 取得图原点 |
| `field_audio.json` 的 `cues`、`footstep_classes` | cue 按 `fd2_field_sfx` 枚举取 `bank` 和 `sample`，支持 FDOTHER bank 31、80、95。脚步类别按 `profile - 1` 索引 |
| `field_background.json` 的 `stages` | 以关卡 ID `0..32` 为键，含 `kind`、`fdother_entry`、`width`、`height`。模式 0 无底图，1 使用 320 × 200 原始图做波纹，2 拼接两个连续 RLE 半高图，3 使用 312 × 192 滚动图。替换资源时保持对应模式的像素格式和尺寸 |
| `field_info.json` 的 `conditions`、`roster_rules` | `fragment_base`、`stage_stride` 和 `defeat_offset` 计算胜负条件的文本片段。附加规则以 `stage`、`text_id`、`absent_delta` 和 `present_delta` 调整败北条件的显示文本 |
| `runtime.json` 的 `range_frames`、`auxiliary_color_by_side` | 20 个相位各引用一个 `0..22` 的帧。辅助色按阵营 `0..2` 查色基值，再加闪烁相位 |
| `common_event.json` 的 `nested_drops`、`cell_items` | 嵌套掉落记录含 `mode` 和 16 位 `parameter`，现有记录使用模式 0，参数为物品 ID。格子物品按原地图格第三字节完整值 `0..4` 索引 |

城镇服务、转职条件和参考资源见[准备服务语义](../reverse-engineering/common-preparation-semantics.md)。战场音效枚举见 [`field_audio.h`](../../src/field_audio.h)。

## 过场移动脚本

`movement_scripts.json` 的 `scripts` 以十进制 ID 字符串作键，范围为 `0..255`，省略前导零。每条脚本包含一个有序的 `groups` 数组，每组包含 `actors`，并填写 `move_cells` 或 `wait_frames` 中的一项。生成器根据数组长度写入计数。每条脚本最多 255 组，每组最多 32 个参与者。

- `move_cells` 和 `wait_frames` 的范围均为 `0..127`。`wait_frames: 0` 使用特殊重绘。
- 每个参与者的 `slot` 为当时战场单位表的槽号 `0..95`，执行前须已建立该槽。`direction` 的 `0..3` 依次为下、左、上、右。特殊重绘还允许方向值 5。
- `move` 节点的 `args` 通常为 `[script_id, fade]`。可用 ID 由该领域的 `operations.move` 和实际存在的脚本共同决定。

下面是 `scripts` 中的一条记录，槽 0 的单位向右走两格。引用 ID 150 前，确认对应的流程允许该 ID，并核对单位的出发位置与后续对话锚点。

```json
"150": {
  "groups": [
    {"move_cells": 2, "actors": [{"slot": 0, "direction": 3}]}
  ]
}
```

原版字节格式见[过场移动脚本](data-formats.md#32-战场过场移动脚本exe-指针表-解释器格式)。

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

## 迁移旧 Mod

以新版的默认 JSON 为底稿，按原物品、法术、单位或关卡 ID 迁入已修改的值。旧版低字节在前的 16 位字用 `低字节 + 256 × 高字节` 合成。对于 `attack_bonus` 等有符号字段，合成值大于或等于 32768 时再减 65536。旧版法术分派的四组值按地图法术 ID 合并到对应记录的 `map`。

旧移动脚本保留原 ID 和组顺序，把计数字段展开为 `groups` 和 `actors`。原 `mode` 小于 128 时写入 `move_cells`，达到 128 时将其低七位写为 `wait_frames`。事件图沿用版本 2，可直接迁移已有节点参数、连线和绑定。来自版本 1 的 `source`、`sources` 和单独文件名清单已在版本 2 移出构建输入。迁移后重新生成，并运行改动影响的关卡或界面。
