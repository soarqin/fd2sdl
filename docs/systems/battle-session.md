# 战场 session、动作提交与场景交接设计

本文约束现有模块的后续扩展，补齐状态、失败和验收边界，不要求重写引擎。设计始于 `e9a2d81`；2026-11-11 续记 G5 输入、逐叶事件及共享返场合同，2026-11-14 补充 S3 结果调用与准备交接，保留各历史阶段原有验收范围。G4 `4ada76b` 及 G5 的来源域内完成结论保持原候选身份；阶段状态、支持清单、提交和本地集成统一查[总计划](https://github.com/soarqin/fd2sdl/blob/373e5c0bd21454793a20fc62ecb8addb18055dba/docs/plans/development.md)与[阶段记录](https://github.com/soarqin/fd2sdl/blob/373e5c0bd21454793a20fc62ecb8addb18055dba/docs/plans/stage-log.md)。源码接线、静态／局部原码验证与原版运行验收分别登记，工作包见 [`../plans/battlefield.md`](https://github.com/soarqin/fd2sdl/blob/373e5c0bd21454793a20fc62ecb8addb18055dba/docs/plans/battlefield.md)。

## 1. 现有模块与状态所有权

| owner | 持有内容 | 不应承担的职责 |
|---|---|---|
| `main.c`／`app_flow.[ch]` | 应用生命周期、标题分派、BGM、当前场景去向 | 不在菜单返回值转换时抹掉胜／败或错误 |
| `fd2_vga`／`field_page` | App 唯一工作页、共享视觉时钟、菜单高亮、嘴型与信息板临时门／侧状态 | 不持有第二份单位、资金、库存或战斗结果，不写 SAV |
| `field_play.c` | 正式输入循环、时间调度、系统动作编排、演出调用与退出原因 | 不复制伤害公式、候选评分或存档 codec |
| `field_game.[ch]` | 单位、回合／phase、坐标、事件、结果及交互临时状态；所属 stage 资源 | 不直接控制文件格式、SDL 按键事件或音频设备 |
| `field_attack`／`field_magic`／`field_ai`／`field_unit_stats` | 范围、计划、效果与属性计算 | 查询不消费 RNG，不偷偷提交 UI 或全局场景状态 |
| `field_system_menu`／`field_manual_slot` | 各自页、选择、确认及动画状态；共享高亮只投影 App owner | 不把活动快照与四槽混成一个 dispatcher，不另建计时真值 |
| `field_save.c`／`save.c` | session 与原版记录转换、整文件保留式更新和安全写回 | 不把临时交互、指针或额外 RNG 状态写进未知字节 |
| `scene`／`field_effect`／绘制模块 | 对已确定状态的画面、文本、动画与 cue | 不重复扣 HP/MP、移除物品或设置 acted |
| `input`／`audio`／`audio_pcm`／`bgm` | App 普通键队列、held／repeat、持久 quit；播放命令、设备和 source 生命周期 | 不推进战场 RNG、phase 或剧情 |

`field_play.c` 的主循环把一次运行的借用对象、事件报告计数和退出原因集中在本地 `field_play_loop` 中；每类已读输入（取消、详情、F1、焦点轮换、确认、系统菜单确认、活动存取、方向）各有一个分派函数，原 `break` 处的提前返回语义逐一保留。Scene 结果到 Play 返回码的映射、终局／宿主退出门、提示失败处理、CONTINUE 与活动 Load 共用的 10010 Round 和本关 primary 曲请求，以及胜利结算交接都只写一份。

保留现有分层与调用点。只有某项功能确实需要共享代码时才抽取局部 helper，不先拆分 `field_game.c`、建立通用 command bus、事务框架或剧情 VM。单位和存档的字节布局以 [`../formats/data-formats.md`](../formats/data-formats.md) 为准，本文件不维护第二份格式表。

概要板`18c6d`的板宽149不是HP条或目的owner边界。正式`18b84`直接在同一456×336 App页绘制，战斗板借320×200 owner；二者共用原signed条长及板／条／数字／姓名顺序。合法线性写可越板或逻辑行，真正越owner时保已写前缀并停止本叶。资源／姓名预检错误仍在首写前拒绝，不把条截成149宽或先画VGA再回交局部矩形。`field_info`只在调用期间借App像素，Game不保留内部指针，也不复制第二页。

### 持久与临时状态

- 原版单位始终是 `fd2_field_unit` 的 `0x50` 字节记录。玩法字段写入唯一记录，不能再保存第二份 HP、坐标或装备真值。
- 模板新建、开场构造、完整存档恢复、转职和派生重算是不同层，不存在可代替全部路径的通用构造器。基础表覆盖角色 ID `0..31`、敌军 ID `68..135`；G2 另支持原实际调用的 ID48／66 相邻读取窗，不能将其命名为新基础表或推广成任意 ID32..67 新建。成长／转职具有自己的有效域；stage 0 捕获值只作夹具，不是通用默认值。模板有效前缀与部署位置读取原 metadata byte2／byte1，不用资源总条数或同 ID 次序代替同索引 placement。
- FDICON 负责地图帧，DATO 当前只确认是立绘包，FIGANI 负责战斗演出；不能拿立绘索引猜属性或复用 TMP cache class 当职业。
- phase cursor、范围／路径、菜单、演出快照、计时与sprite cache映射数组是临时或派生状态，不直接序列化；单位raw+2本身是原80B记录中的字节，按caller合同保留或重写。
- 地图sprite使用原cache映射；热身份改写不自动换图。活动Load按当前ID及全部raw顺序重建cache（含hidden），并由`10010@10391`写回raw+2，保其余79B；手工`2986f`只重建cache，不改persistent raw+2，不增加SAV字段。方向5等超出普通12帧组的读取只接受有来源的cache映射与资源窗，不能取模成方向0..3或猜ID+1。
- 模板`10c50`以目标槽已有80B为底，保`28h..30h`九B，weapon=ff另保`0dh`；原明确清零字段仍清零。正式append与Scene也必须传实际owner底，不能用零局部抹掉这些字节。这不等于已证fresh原堆为零或任何固定资源值。
- `53BF7`的完整A00在App建立时只分配一次。复用`context.preparation.unit_area`为Game外的既有backing；`has_preparation`只标记准备入口可用，START只清count。Scene在四个原112a5节点原位构造，Game窄入口消费同一来源，不重构或从磁盘代读；真实Game关闭前交还完整roster/count，失败Load不交还空表。成功结算先更新Game，再交新准备状态，关闭不得用旧表覆盖它。
- START保留camera／effect／music／SFX四个raw byte，只在对应消费点转bool；新局资源值在原32969清0。不能随旧准备metadata盲继承stage／gold，也不能以创建Game为由重置原App选项。
- `option_values` 与 `resource_value` 已有 session owner。配置是逐次 toggle 立即提交：未修改时退出不能改状态，已确认 toggle 后退出也不能擅自回滚。
- `fd2_field_game` 含有拥有所有权的指针；结构体浅复制不等于可独立关闭的 session，也不等于外部 RNG 的快照。不能关闭两个共享资源的副本。

### App 视觉 owner（G4 接入，G5 续记）

`fd2_vga.field_page` 对应原 `53A49`，是 App 唯一的 456×336／153,216 byte 工作页，跨 Scene、Game 和活动 Load 保留；Game 只借用，不在绑定、换框或读档时清空。RLE 的 SKIP 保留页中旧值，opaque 0 则实际写 0。资源预检和隔离预览可以临时深拷贝视觉页，结束即释放，不能把副本交回为第二个持久 owner。普通 UI 不得把显示页任意回灌为地图底图；`1f1cc` 的 raw 64,000 byte 前缀、`19953` 的 `(68,68)` 整帧／`(73,180)` 底部固定复制，以及缩放页桥均走有证的窄接口。

`field_visual_state.h` 的七个 DWORD 与页面同属 App，不进入 SAV：

| 字段 | 原全局 | 推进来源 |
|---|---|---|
| `actor_idle_phase`／`actor_idle_tick` | `53C0B`／`53C0F` | `1297d` 的普通角色相位门 |
| `cursor_phase` | `53C07` | 每次 `1297d` 的光标相位 |
| `terrain_phase`／`terrain_tick` | `53A40`／`53A00` | `11eee`，仅 XOR phase 的低 byte |
| `range_phase`／`range_tick` | `53C1F`／`539F4` | `11eee` 的范围相位门；20 步冷表为 `0..10..1` |

水色、raw26 抖动、对话嘴型和菜单高亮有各自的 App 锚，不并入这七个字段，也不另取 UI 随机流。各 caller 按原顺序显式调用 `1297d`、`11eee`、`127e0` 或水色；普通 render／present 不偷偷推进这些状态。`fd2_vga_upload_current_frame` 只上传同帧像素，不采样输入、不增加等待或时钟步。正式 `runtime_refresh` 的 0／3 分别对应完整 `11cac(0)`／`11cac(1)`：actor／cursor → 仅模式0推进水色 → terrain／range → raw26 jitter → render；中途错误保留已到达的写点，不提前推进后项。1 为 phase 关闭的 tiles＋jitter，2 为 choice 的 actor-clock＋tiles＋jitter，后二者均不画信息板、不投影到 VGA。`51A83` 的显示模式与水色参数分别处理。

信息板有用户选项 3 和原 `51AAC` 两道门，须在绘图和更新 `51A0C` 侧状态前同时成立。`info_panel_suppressed` 是 `51AAC` 的取反投影，零初始化对应原门初值 1。原 `51A0C` 保存横偏移 1／242，初值 1 表示左侧；SDL `info_panel_right` 保存布尔投影，0 为左、1 为右，不能把原 DWORD 初值直接赋给它。START／手动 Load／胜利新入口／败北，以及 END／March／自动回合的临时跨度由 App／Play caller 管理，活动 Load 继承当前门，不借创建新 Game 重置。共享菜单高亮使用同一 phase／tick：低 WORD 符号扩展后判门，确认框以 raw phase 除 2 选帧，离开只清 phase，保留 tick。

回合、阵营横幅、整组恢复和通用确认的分支／顺序／等待／取消来自 `runtime.json`，有限 C 操作只提供现有提交点及呈现。整组恢复的等待属于整组，不能对每个 actor 套用单 WAIT 的三次等待。`runtime_host_quit` 只锁存一次宿主退出信号，在旧 void 调用链返回前停止同栈 controller／pending 推进；它不是业务副本，不进入 SAV，不能把真实资源错误改判为宿主退出。来源与当前有效域见[共享运行语义 §8](../reverse-engineering/common-runtime-semantics.md#8-g4-共享菜单与-phaseround-实施依据)。

## 2. 动作生命周期

统一约束，不强制所有动作使用同一个新 API：

```text
只读查询／列出可选项
  → 临时选择和预览
  → 确认后重验当前单位、资源、范围、目标及 handler
  → 预检所有会失败的分配／数据／目标
  → 按原版顺序执行 RNG、效果与资源写入
  → 按玩家／AI 各自 common tail 收尾
  → 必需事件与 stage 结果处理
  → 呈现已确定结果／返回下一交互边界
```

### 查询与取消

- 列表查询、范围、目标预览和菜单绘制不修改单位、HP/MP、物品、acted、phase 或 RNG；只允许更改对应的临时 UI／派生缓存。真实输入等待与纯查询分开：原版法术列表的 `field_player_magic_list_select_step @0x1d51d` 调用 `sub_16C57 @0x16c57`，进入等待及头像闪烁重置会消费共享RNG，不能省略或另建UI随机流。
- 提交时必须按当前状态重验，不接受过期候选。对多目标动作，要在第一个目标消费 RNG 前完成全部目标的数据与支持性检查。
- 普通攻击targeting取消先回COMMAND，保留当前临时移动位置；COMMAND取消才恢复本次移动前坐标、方向和镜头。不能把两级取消合并。
- 法术列表取消回COMMAND并重新选择首个可用命令；法术目标取消先聚焦施法者，再重开列表第0项。两者均保留临时移动及pending，不扣资源、不标acted、不触发cell handler；真实等待的RNG消费保留。物品有独立的ITEM子菜单层，返回合同见下文，不能套用法术列表取消。
- Options toggle 属于已经提交的配置操作，不能套用未提交战斗动作的取消规则。

### 提交边界与 RNG

- 预检拒绝应保证业务状态、原文件和 RNG 均不变。不能先执行半个效果，再发现后续目标或 handler 不支持。
- 进入效果提交后，RNG 消费次数、目标遍历顺序、整数截断和资源消耗以原版为准，不能为「结果更合理」而修改。
- 外部 RNG callback 的 userdata 不会随 `*game = snapshot` 回滚。需要区别提交前拒绝与提交后失败的接口，应在该局部调用链明确返回提交状态；不能让调用者无条件重试。
- 已有 behavior 11 的法术 → 物理具有分段提交语义：后段失败不能撤销已消费 RNG 的前段。新玩家动作也不能默认沿用 AI tail。
- acted、cell event、死亡清理、stage handler 与 phase 推进各执行一次；顺序按相应原版路径确认，不能笼统合成一个所有命令共用的结束函数。

地图法术按原 wrapper 在 CAST 前或后扣一次 MP，均先于逐目标 effect；顺序及费用记录来自冷表，ID17读记录18。费用不放入逐目标纯 effect，miss／免疫／状态无变化仍消费，后置失败不退款。FULL／派生／复合法术按真实 caster FIGANI intro 中 part marker 精确等于1的次数扣费，允许零次、多次与 WORD 回绕，不将 unit9 的一次扣费推广为全体默认。完整合同见[通用战斗语义](../reverse-engineering/common-battle-semantics.md)。

单位详情沿用独立 `detail_unit`／portrait owner，页索引只在装备与未选中法术详情间切换，不复用行动列表或selected_unit。打开前预检两页必需资源，失败保留现有详情与portrait；成功Load释放详情并重置临时页，失败Load保持原session。G4 候选的正式 Enter／F2 同步消费 `runtime.detail`：装备页等待一次，非空法术页再等待一次，均为原 `16c57(0)`，最后完成 12 相位关闭。frame 3 由本次 Play modal 预检并局部释放，frame 0 仍归 Game；两次眨眼等待借同一 RNG 和 App 水色，绘制原点 `(8,10)`，不改共享逐字嘴型计数。F2 详情不推进 controller，Enter 路径只在最终关闭后推进一次；宿主退出或资源错误均不继续 controller。实机范围与固定原 DATO 批量有效域见[共享运行语义 §8.4](../reverse-engineering/common-runtime-semantics.md#84-详情的两次等待与原资源有效域)。

### 玩家法术与物品（B2／B3 基础及 G2 扩展）

在现有交互状态中区分列表选择、施法中心／目标确认和返回路径，不将攻击专用 `TARGETING` 未加区分地用于全部命令。列表可用性取自原版玩家入口，已确认范围和 effect helper 可复用。

MAGIC_LIST 保存临时索引，step 不负责关闭，取消关闭后才回 COMMAND。原版18890对非零路径禁法，只有 unit ID18／19／34例外；此门与空法术、record+0x27禁止状态共同检查。低 MP 只阻止当前项确认。正式入口沿当前stage battle capability（S5后为stage0..29的已证范围），要求共享RNG，并支持正常有来源的技能ID0..30、32..35；每名caster仍必须实际持有该位，资源在PREPARE独立预检。ID31为空名且无正常赋值源，36..39无记录；G1已核初始／模板／学习／转职闭包最多12项，不为外部任意SAV发明分页或未知技能。关卡绑定状态见[总计划](https://github.com/soarqin/fd2sdl/blob/373e5c0bd21454793a20fc62ecb8addb18055dba/docs/plans/development.md)，全正常域及全目标FX证据不由通用能力门自动扩签。

| 正常调用路径 | 模式合同 |
|---|---|
| 玩家0..8 | 始终 FULL，不随 SHORT 选项转地图版 |
| 玩家9..23、25..27 | 原地图 wrapper；玩家9只提交首目标，FX仍收到完整原目标组 |
| 玩家24、28..30 | 派生 FULL；固定 caster FIGANI +2条目，signed AP／DP伤害及逐 part HP写点，不走普通命中／profile倍率 |
| 玩家32..35 | 固定 caster FIGANI +1及专用演出后回地图，按冷图执行复合组；33先清全部目标状态，34／35按原组间清浮字 |
| AI0..9 | `option_values[2]==0` 才 FULL，否则地图版；地图9仍只提交首目标 |
| AI其它正常可选项 | 继续按原 candidate 评分与地图 wrapper；23／24／25／28..35没有正分正常候选，不造可执行计划 |

系统菜单已开放效果选项2，并按原 byte XOR1提交，保留非规范原值；配置与 Load 决定 AI 动作模式。玩家物理始终 FULL，算法与呈现均不读取该选项。G4 候选已把信息板选项3接到上述双门，原生验收仍按候选收据登记。FULL9 的多目标继续执行原共享计数、效果与 RNG；只有 beam 请求超出真实31项目录的 DRAW 被省略。非 side0 的 FULL9 缺合法 selector，保留独立域限制；玩家9走 MAP。不能用相邻资源或伪造图像填补原未定义读取。

物品B3的已确认返回合同独立于MAGIC：Use列表取消使1bbdc返回0，18d8c重开ITEM子菜单；子菜单取消使1bbdc返回-1、18d8c返回0，再由18890重开COMMAND。Use目标取消先聚焦caster，再重开1b932列表并归0选项，保留临时移动及pending。53C53仅由Give完成分支置1，不能把它当作所有库存改变标志；装备／丢弃已提交变化也不能因为该值0而回滚。

ITEM_MENU／ITEM_LIST 共用正式输入与四向 command 呈现，语义为 Use／Give／Equip／Drop，各项独立重验能力。列表索引是临时 raw 槽，step 只报告可确认；列表完整关闭后才进入目标或提交 Drop／Give，Equip 成功确认继续原页。列表取消回 ITEM 首可用项，子菜单取消回 COMMAND 首可用项，两层均先于 COMMAND 原移动回滚分派。成功 Load 清列表、目标、收方及 Give 已提交标志并回浏览，失败保留所有旧 owner；这些临时字段不序列化。

ITEM_TARGETING 每次按当前 raw 槽重新读取物品的玩家 cast／area／filter，复用 move_range 与互斥 target 候选 buffer，不缓存第二份 slot／物品真值。候选按 raw 顺序和原版 filter 收集，不额外过滤 HP0 或 acted；121 占轮询槽但不移动焦点。范围 LUT 与确认时的 Manhattan 判据分别保留，area 光标沿用 20 帧资源。目标取消先聚焦使用者、清目标 owner、返回 ITEM_LIST0，保留移动路径／origin、pending 和 RNG。法术与物品共享范围建立和轮换，保留各自 record／列表／能力门。

Use 按 `field_item_use_effect_execute @0x20c6f` 分派实际 code5..24。HP／MP恢复、清状态、状态附加和法术伤害复用局部 effect；增长物品只写首目标的原始 WORD，浮字后仅重算使用者并压槽，MOV增长跨 EX 的临时高 byte 要等 helper 返回后才恢复。code6／7仅在对应状态存在时先按 power10恢复（含 spread RNG）再清状态；仍消费一次物品，状态不存在时不消费效果 RNG。MPmax0的 code11只显示 MISS，不恢复、不取 RNG。AP／DP状态的直接增量与装备重算是两条不同公式，不合并舍入行为。

Item 与 Magic 共用有界同步呈现接口，但保留 `action_kind`／use code／power和实际效果目标数：code5／13只有 effect13地图 FX及闪烁，不套完整 Magic13的大 mask；20／24固定 ramp、21固定双色，不按 value的 Magic ID猜样式。永久 stat 的 FX消费完整原目标组，业务与红色原 power浮字只用首项。`item.effect` 冷图控制 CAST→逐目标结果→POPUP_GROUP→条件重算／耗槽／恢复 EX→清贡献；code23调用既有传送冷图。TARGET只排结果，POPUP_GROUP播放后才继续写库存或EX，FINISH不重复播放或提交。

Use 在首个 RNG／业务写入前预检全部目标、可能败亡的掉落描述符、最终格和必经尾链；清状态可能救活的 HP0 目标须纳入预测。只在原版规定的 code5／6／7／8／9／10／11／17／18／19 移槽一次，其余不消耗。原版物品尾清全局经验贡献，不清单位 EX；之后执行败亡、acted、最终 lookup1 和 controller，不做 WAIT 恢复或物理方向复位。提交后错误保留效果／压槽／已到达的 acted 与外部 RNG，首错冻结，不退款或重试。

AI物品的掉落入口返回非零不等于仍有待续态：`field_loot_descriptors_execute @0x1aa1d` 的 mode0／1 可以同步终止。只有实际 `progress_pending` 才延后 common tail；同步、直接提交、scheduler和异步恢复各自恰执行一次，不遗漏 acted／贡献清零，也不补跑已完成的尾。

code23使用独立 ITEM_TELEPORTING 第二层：首目标 cast排除中心整格，门为使用者 text_id24且最大MP至少20；第二层选择目标可站立且无其他可见单位的目的格。任一层取消均返回使用者的Use列表0。成功按原WORD减MP并移动首raw目标，不另加当前MP门。传送按 OUT前画→坐标255→OUT_COMMIT→IN前画→写目的坐标→IN_COMMIT同步；临时255属于Game原写点，只交专用投影，不能交给常规可见actor绘制。自身移动时预检最终目的格事件。

正式 ITEM 查询无写入／RNG，接受当前 battle capability内的可行动玩家和连续已知库存；正式等待入口另要求共享 RNG。code0装备可查看，不能在Use确认。Drop／Give不套Use handler门，包含隐藏flag40的全8槽须满足各自派生检查。Equip许可来自原版29×7字节表，只比较每行前6字节，未知职业／物品拒绝。能力查询不借用MAGIC的移动、MP或unit+0x27门，能力失效也不能阻止原有取消。

Drop 用同一 ITEM_LIST／mode0，code0 可确认；确认先在80字节 actor 副本移槽，确认或取消均重算四属性。余项回 ITEM，删空回 COMMAND；不结束行动，之后取消移动保留丢弃结果。Equip 在80字节副本按 ID `<128`／`>=128` 清同组整个 flag、选中写40并重算；重选不是脱装。不兼容不重算，取消不重算，成功留原页／原 raw 索引。二者均在全部数据检查通过后交接，不改 HP／MP、EX、acted、pending 或移动 owner。

Give 用独立目标与收方列表层。相邻友军使该格可选，确认后按该格首个可见 raw 记录处理，不擅自增加 HP／acted 过滤。收方有空槽则压缩来源并追加；满8槽时打开 mode0 收方列表，双方各移除选中槽、以 flag0 追加交换 ID。只重算送方，保留收方原派生值；源列表、目标和交换列表取消均按原调用链重算送方并返回，不清已成功交接标志。53C53 对应的临时 `item_transfer_committed` 只在新的移动选择或成功 Load 时重置；成功 Give 后 COMMAND 取消保留移动及库存，并走不回血的物品行动尾。

私有 modal 每次读取一个列表确认键；退出、资源错误和完整关页在不可逆提交前检查，已消费事件不再交给新控制器，余键按原 caller 清键点处理。G5 修复逐帧丢弃后，法术列表开合不额外清队；物品／详情的17e0b打开末保原clear，不能统一吞掉层级切换期间早到键。Equip 留页复用同一 portrait／page owner，成功重画后继续原索引，不兼容保留当前眨眼画面。提交后重绘失败保留业务结果并冻结，不重试。实现与逻辑测试不等于原版运行验收；各冻结候选的双端 WGC、SAV 和精确随机范围见[阶段记录](https://github.com/soarqin/fd2sdl/blob/373e5c0bd21454793a20fc62ecb8addb18055dba/docs/plans/stage-log.md)。

普通物理目标同样复用14818／115b6的互斥范围和raw候选owner：按武器min／max与filter0建立范围，首次聚焦首raw，Z／小键盘5先递增再取槽；121占槽而不移动焦点。取消先回actor，再清候选／范围并重开COMMAND，成功尾同样清cache。候选不额外滤HP0／acted，提交仍沿独立物理有效门重验。移动预算0可保留原点，不得在game wrapper整体拒绝而令AI错过原地攻击。

MAGIC_TARGETING复用cast范围及raw候选owner，确认时从当前坐标重建几何和effect目标，不相信旧缓存。普通area支持多目标；cast0围绕caster收组，保留mode4／5的非空／空确认、原点及不轮换候选合同；30按原轴线逐格取首可见side0，空格或其它side不终止，同中心按向下扫描。MAGIC_TELEPORTING保留23第二层目的地选择，使用frame1；Item23同层用普通frame0，取消均回各自列表0，不能合并UI合同。复合34成功保留选择中心焦点，不自动回caster。

效果／费用／必要呈现、贡献资源分支、可能败亡描述符和必经事件均先预检。预测phase要按本次最终隐藏／恢复／清状态／acted结果判断；20／21仅把原对应状态非0的目标当作会恢复，不能将所有HP0友军当作被救活，也不能给13565本体添加HP门。收尾独立于WAIT恢复及物理方向复位，按效果／败亡、acted、归一、最终lookup1、奖励续态、controller执行。正常费用按对应wrapper或intro marker消费，不能统一成「每个多目标动作恰一次」。

玩家与AI仍分别以 `field_player_magic_command_execute @0x1cff0`、`field_player_item_command_execute @0x1bbdc`、`field_ai_magic_action_execute @0x15311` 和物品dispatcher为依据，不能仅由一个caller推广所有费用／失败合同。逐族写集及有效来源见[通用战斗语义](../reverse-engineering/common-battle-semantics.md)，函数登记见[规范命名表](../reverse-engineering/function-names.md)。

### 奖励与状态续态（B4）

`experience_value` 对应原全局53EC8，与单位EX、消息动态数字53AE1分别处理。物理每击按原门覆盖该值；法术／物品共享效果在HP／MP写入后立即累加，负状态的伤害贡献必须先于后续duration RNG。恢复贡献以signed实际差计算，old超过max再cap产生的负差按DWORD回绕，不能夹成0或拒绝所有超上限HP。玩家按signed除法归一，controller仅将signed大于99者封顶；AI物理尾没有该cap，AI法术和物品在原尾清零，不能移到首效果前。活动Load不清贡献，也不将其写入SAV。

0／13的纯奖励上下界查询复用原公式枚举100个spread，不取真实RNG；超上限HP时，伤害贡献在kill门可能从raw比例跳回full，不能只查spread99。多目标按DWORD回绕和signed断点作保守区间预检，负上界不转成正99；该连续包络及独立负值资源分支不是精确的整组RNG可达集合。来源和保守范围见[通用战斗语义 §8](../reverse-engineering/common-battle-semantics.md#8-g2-追加signed-派生伤害负贡献与动态数字边界)。

`field_unit_experience_reward_execute @0x1e292` 无side门。贡献0、hidden或精确等级cap（ID30／31等于99、其他等于40）早退并保留全局。成长支持已有ID0..67记录，不套创建器0..31门；未知成长数据或不安全的signed运算在提交前拒绝。普通40仅为入口门，循环只在新等级30或特殊ID新99清余值，不能另加每级>=40门。

game保存一个有界续态，play只持有背景／空底板／四帧立绘。LEVEL前先写level；每个非零成长量先提示／ACK，返回后才写WORD；学习先OR位，再显示587。每级末重算，最终自动等待和关闭后才写EX、清全局。最大HP／MP增长不改当前值。资源预检可用80字节纯副本，不能提前交接最终单位或消费RNG。

非零负gain继续显示原signed数字，按实际 `EX byte + signed gain` 判断升级；通常旧EX<100时不升级，最终EX低byte仍可回绕到>99，下一次不能套「负gain永不升级」。原负号会读到字体owner外；G2保留业务、RNG与EX写集，以已证FDOTHER[5] frame132安全投影负号，正数字形不变。负号属于明确的安全呈现扩展，不声明原版像素等价，也不因其原版缺陷禁用合法负power法术。

中毒按raw actor先扣 `maxHP/10`、聚焦、显示487；全部毒伤后才死亡清理。六状态byte逐项递减，恰归零就聚焦→481..486→等待／关闭→重算，再处理下一byte。失败或宿主退出不ACK后续写点；保留已提交HP、计时、成长及外部RNG，不假回滚。入口quit先于core resume，scroll／开合中途quit停止并释放缓冲。

成长固定取pair第二byte，无成长RNG。489／490..494的-3以任意键ACK，488／587和481..487无-3，487的-2只换行。`dialog_wait_key @0x16c57` 每次入口与眨眼复位耗共享RNG；`field_notice_wait_ticks @0x1e5c0` 自身不耗该RNG。宿主55／110ms门是BIOS时钟适配，跨框嘴型相位、完整音画／随机流仍未声明一致。

B4当时六组实际WGC来源为C4四组＋C5两组，消息资源、详情和完整Save／WAIT／Load／Save均通过；不是G2或最新程序重新跑六组。SDL回合编号横幅未图形对齐，使用有序phase日志与后续真实WGC／完整SAV核验，不把日志当横幅像素；当时未声明完整FIGANI、跨框嘴型相位或共享RNG等价。最终C6仅修AI结果误用玩家HP前值的printf下溢诊断，业务、RNG和呈现不变，无新GUI。

B4 当时另保留了非 ITEM COMMAND 收框返回值被忽略、历史首次 END 落到 NO 两项风险，B4 的定向运行没有关闭它们。G5 当前非 ITEM 分支先检查 `play_command_transition` 返回值和持久 quit，成功后才进入 `confirm_cursor`；`closing_frame_barrier_test` 的十个末帧向量及编号47的无窗口回归验证 HOST／真错不先提交 WAIT，错误优先级和原状态前缀保留。这是私有 Play／Core 故障回归，不是原生窗口退出注入。

历史 NO 的原因仍未归因。当前 END 每次按原 `19953` 重置 selector0，随后由唯一 input owner 读一项；Suite37 的两端首开连续原件分别观察到完整 normal 按钮及 YES 高亮，独立 END 场景在 default YES 后只投一次 Enter，实际推进 Round002 并正式 Save，Suite50 按严格来源链继承。normal 帧48／51本身不证明选择，YES 须匹配49／51；这些证据关闭当前已识别的首开／消费风险，不补签未采到的首个 present，也不声称定位了历史根因。实际原件和候选身份见阶段记录。

AP／DP重算使用binary64 1.15乘积先53位舍入再向0截断，推翻旧nearest-even说明，不能换成整数115/100。直接增益 `old + floor((15*old+100)/100)` 是另一条已证u16公式；AC／EV状态只各加15，不乘1.15。局部原码复现见[IDA工具说明](../tools/ida.md)。

## 3. 事件执行与停止边界（B0）

已知正式事件由 Game 的 `event_resume` 按冷图逐叶执行：逻辑写点、呈现、ACK 和后继 PC 交错推进，不再先提交整个事件，再由 Scene 重放。G5 将原六个正式身份从 scene 迁入 common_event，当时未新增关卡支持；来源映射和保证边界见[共同事件语义 §7](../reverse-engineering/common-event-semantics.md#7-g5既有事件按实际等待续行代码冻结最终门待验)。S3 的正式绑定继续使用同一 owner，新增合同见本文末节，实际开放清单仍只查总计划。

`event_log` 仍最多保存64个 notice，用于未绑定来源诊断及停止边界。fallback 的 `reserve_event_notice` 容量检查保留；未知必需事件不能记录后按成功继续。已知 common 绑定不借日志驱动，也不因旧演出日志满而拒绝合法执行。残留的外部 `presentation_deferred` 状态视为损坏，由 common 预检和 Play 的诊断门明确拒绝，不再交给 Scene 重放；不能把 `progress_resume` 的防御性 pending 返回当作仍有合法演出 producer。

设计决定：

1. 需要改变玩法或推进剧情的 handler 是必需逻辑，未知、失败或容量不足不能算作成功。
2. 必需呈现由当前事件叶和 ACK 驱动，不能以普通诊断日志的溢出策略跳过，也不新增持久事件队列或第二事件热游标。
3. 只有确认不影响玩法、呈现和后续控制流的诊断信息才可丢弃，且保留丢弃计数。不能仅因事件名称未知就归入这一类。
4. 原版要求的 turn event → phase 状态处理 → actor/cell/stage handler 顺序不变。原始 actor 遍历中的 skipped actor 仍可能需要 stage handler。
5. 若未知 cell/stage handler 只能在动作提交后发现，停止并报告 `stage/actor/action/phase` 与已提交边界；不能改成待机，也不能假称回滚了整个动作。

回归至少覆盖玩家和 AI 两条路径、notice 满边界、同一事件不重复提交、演出失败、提交后不再调度下一 actor，以及失败不会被应用层报告为正常完成。

session 的 `failure` 与 `failure_notice` 只记录首个不可恢复错误及其 turn、phase、actor、slot、action 和坐标，不写入原版单位或存档。记录首错后清除自动调度时间；玩家确认、移动、攻击、AI 提交和 phase 调度拒绝继续执行。正式循环先检查错误，再判断胜败；不能让同一动作导致的胜负门掩盖事件失败。

原版共享路径每步以`match_arg=0`更新单个pending，未匹配保留之前命中；移动循环不分派handler。AI normal common tail查询最终格`match_arg=1`，命中才覆盖，再acted、全方向reset；outer随后分派pending。未知handler在实际分派时冻结，保留已提交效果、坐标和acted；读取损坏事件数据仍明确失败。pending是session旁路状态，不写原版存档，成功活动Load清除，恢复失败保留原状态。

玩家完整路径后进入COMMAND，成功动作先acted再最终lookup；普通command取消回滚并重入移动选择、清pending，下级目标取消或无效确认保留pending。selection取消不acted／不lookup1，仍经过controller后置。零步WAIT恢复HP，非零路径WAIT不恢复。普通特殊格 mode0／1 不附加 stage0／1 限制；mode≥2 保完整 WORD callback，在已证90项表内按真实 caller 分派已知绑定。未知／不支持的 cell-action 在恢复／acted前明确拒绝，不能当普通pending处理或报告成功；仅接通通用分派不开放未知关卡绑定。

当前controller先完成B4奖励续态，再恢复已绑定结果门、精确13565门、必要整轮phase后的pending分派与清空。13565只看flags&0x81、side2和raw26，不加HP或bit04门。异步phase期间仅保存原actor与等待标志，不保存pending副本；AI仍可清空／覆盖同一slot，raw尾必须扫描完才换phase。side1尾先全表清acted，再进入phase0事件／状态，删除旧延迟清除补偿。Enter的普通不可操作actor详情关闭后走同一后置，F2直接返回。

完整玩家controller在119a6先清共享pending，再经1187a明确清键；Game只调用现有有限呈现叶，真实input仍归App，保held／repeat与quit。18d8c另仅在物理动作acted写后清键，不把任意MAGIC／ITEM／WAIT完成当清队依据。F1／F2／G的原直接return不套此尾。等待phase时拒绝导出／保存，成功活动Load清除续行与pending，失败恢复保留完整原状态。当前未解的关卡绑定仍按来源保留，不以此输入修复解除S支持门。

批量行军按 `field_empty_focus_menu_execute @0x16f55` 的独立顺序提交：确认后捕获固定绝对焦点，按 raw actor 顺序仅处理 `(flags & 0x85)==0 && side==2`，不附加 HP 门。每人先 focus、清共享 pending、执行完整路径的 match0 收集，再分派当前 pending；成功后才重置全表方向、标记当前 acted。无路仍走尾部，不查询最终 match1。当前 handler 失败保留已走路径和先前 actor 的提交，立即停止，不伪造整批回滚或继续 phase。

整批后只启动一次 side1→side0→新玩家回合，不自动 Save。March 期间由既有私有路径呈现参数关闭选择／信息层；play 局部续行把相同参数传给异步 AI visual step，普通 END 默认使用原呈现。同一 Game 的 `turn_post_caller` 区分无调用者／March／END，不另存业务副本；caller_return 冷入口按原成功返回点恢复信息板，仅 March 恢复 `51A83`，END 继承实际显示模式。清键叶不附带信息板写入，错误／HOST 不提前恢复成功尾。新玩家ready的原清键由现turncycle冷图决定，不在March／END返回后再加无源全清；保留 held／repeat，不把显示策略或 pending 副本写入 SAV。长路径中途的即时宿主退出仍沿现有路径协议，未声明逐动画相位响应通过。

攻击、法术或物品进入效果提交后发生的错误保留当前 HP、库存和外部 RNG 进度，锁住 session；behavior 11 已施法而后续物理动作失败时也停止，不再按成功执行 common tail。

成功呈现后才推进当前叶的 NEXT／ACK；子 loot 返回后才推进父 PC，不重复 descriptor、刷新或奖励。HOST 保当前 PC 与已提交前缀，并由现 `runtime_host_quit` 阻止后续 reply／resume；只有真实 ERROR 沿错误边停止，不把 HOST 当 ERROR 推进。短寿命资源释放独立于成功尾，不能借清理补写未到达的 acted／INFO。停止或事件／loot／progress 忙态禁止保存；显式加载有效存档可以恢复并清除错误，加载校验失败则保留完整停止状态和原文件。

## 4. 战场结果与应用去向

`battle_result` 通过 `fd2_field_play_result` 与 `app_flow` 传给应用层；stage 0／1 的既有结果门保留 S0 适用域，S3 增量按本文末节的真实调用点执行。正式循环先处理 session 错误和宿主退出，再推进下一 actor；快速检查不触发真实胜败结算。

`COMPLETE` 仅表示无交互快速检查结束。B5 候选的正式 App 流程在 stage 0 胜利后播放本关结算，同步持久表并进入 stage 1 准备页；败北完成 Game Over 后返回标题。独立 `--field-play` 不持有 App 准备状态，仍只报告胜／败并退出 `2`，不能把该调试结果当作跨关完成。游戏内离场、宿主关闭／快速检查退出 `0`，错误退出 `1`。离场使用独立 `LEAVE_GAME` 来源，不能改作重新进入标题。B5受支持范围及其后的统一历史回归已完成，具体来源见阶段记录。

| 结果 | 应用职责 |
|---|---|
| 仍在战斗 | 继续当前 session，不离场 |
| 胜利 | 完成已绑定结算后交接下一准备状态；既有 stage 0／1 沿原合同，S3 来源绑定沿末节单次提交／只读导出合同；未知后续关卡仍拒绝 |
| 失败 | 完成已确认 Game Over 流程后回标题；不能按胜利结算、递增 stage 或吞掉失败 |
| 游戏内离开战场 | FDTXT[416] 后独立返回 `LEAVE_GAME` 并退出应用；资源失败为错误，提示期间宿主关闭仍为 `HOST_QUIT` |
| 宿主关闭 | 结束应用，不重新进入标题 |
| 不可恢复错误／必需逻辑不支持 | 停止当前流程并失败，不返回正常完成 |
| 快速检查结束 | 返回检查结果，不执行胜利结算或自动保存 |

`DS:0x3ecc` 等剧情退出标志不是所有 stage 通用的胜负枚举；由本关 handler 和调用者解释。未确认关卡的通用全灭计算不构成开放该关卡的依据。

stage 1 的结算按 `field_stage1_victory_play @0x22f37` 顺序处理：先检查 raw5..10 的隐藏位，播放 fragment6 或7；六名村民全活时，`field_party_item_append @0x1c220` 按 raw 顺序给第一个有空槽的 side2 角色追加198，允许隐藏玩家，全部背包满则静默不写。随后 group4、脚本14..16和 fragment8..10完成后，追加持久角色8，再按 `0x11506` 同步完整 roster 并进入 stage 2 准备状态。结算投影不改 Game、RNG或输出；必经第三回合增援、资源、剧情或最终提交失败时，不交接 App 准备 owner。

原 `51A83` 的普通光标模式0／1由 Game 的 `ordinary_display_mode` 记录，供结算 Scene 读取，不进入 SAV。`0x13565` 的自动整轮控制返回时恢复1；显式 END 继承 AI 的最后实际写点。只有 fragment6／7 的入口继承该值，不能按最后动作种类猜测。Scene 按 `0x165ac` 在说话人聚焦前置0、聚焦后置1；后一次写入不会立即重画地图，文字与闭框仍使用无光标的缓存背景。随后 `0x135dd` 的 PAN 无条件置0，group4和脚本14均不改该值，因此 fragment8入口为0；其内部仍按说话人更新，fragment8／9结束后另有明确置0。目标覆盖层的模式2..6不由此字段推断。

stage 2 使用独立入口 `0x32e8c`，按六个部署槽、脚本18、group1九人、脚本17／19及最终方向重置构造15人活动表。准备页读取 `0x623ac` 的 type2，背景为 FDOTHER62，标记坐标也取 type2原表。B7 当时仅开放入场、浏览、详情、总览和存读档；第三回合 action9 `0x34818`、玩家提交与 END 是当时保留的支持边界。S3 的增量接入不改签 B7 证据，当前绑定与验收状态由总计划和阶段记录登记。

B5 的交接顺序应先确认原版 callback、持续单位字段和剧情，再准备下一关资源及 roster；成功后切换 owner，最后释放旧 session。不用 `stage++`、重放新游戏模板或复制旧地图代替关卡初始化。失败时不破坏原档；下一关尚未准备完成时，不释放唯一可诊断的旧状态。

## 5. 存档与恢复

### 两种原版路径独立

| 项 | 活动快照 | 四槽手工存档 |
|---|---|---|
| 原始单位区 | `0x1e00` 字节，格式容量 96 条 | 每槽 `0xa00` 字节，格式容量 32 条 |
| 入口 | 标题 CONTINUE／secondary Save/Load | 标题 LOAD／酒店 `field_hotel_menu_execute @0x29300`；NoTown 的独立保存入口 |
| 保存数据 | 本关 metadata、独立持久 roster、活动单位、cell state、stage/turn、镜头／焦点、配置及保留区 | 持久 roster、已确认 meta、配置及未知尾部 |
| 当前可恢复关卡 | stage 0／1 及 stage 2 入场的15人活动快照，跨 stage 先准备资源再交换 | App 恢复原准备行／NoTown 定义及资源就绪的 stage 1—29 准备状态；酒店拒绝 NoTown 目标；旧 Game 手工 Load API 仍拒绝导入活动 session |

runtime `FD2_FIELD_MAX_UNITS` 已按原 malloc `0x1e00`恢复为96条，与活动快照容量一致；持久 roster仍独立为32条，不能保存时截断或用当前side2人数替代。64是旧SDL实现上限，不能继续作为原版容量。容量恢复不自动开放尚无场景／事件支持的关卡。

活动 meta[1] 与 meta[9] 分别属于当前 actor 和持久 roster，不能互相约束或由当前 side 2 人数重算。持久记录按 text ID 表达跨关身份，新游戏按原版构造顺序建立；活动部署再设置位置、运行期 cache 和临时状态。未知字节在加载／保存往返中保留，不能从活动记录盲拷来构造新持久角色。

session 持有完整 `0xa00` raw roster，包括未用尾；活动与手工保存均写出这整张表。加载 A 后保存到 B 或新文件时，roster 尾来自 A，metadata 未用尾仍由目标容器保留。这两个保留规则不能混为一条。

G3 准备服务直接消费 App 的完整持久表、资金和共享 RNG；31 个同伴选择位按原 16／20 名额稳定重排，未出阵记录不截断。冷定义掌握服务分支／取消／等待和写点，UI 缓存不成为第二业务 owner。准备状态可恢复不等于相应战场已开放；晚关资源、事件和剧情支持门保留。

四槽保存 `2968d` 在确认后先完成一次真实写盘，再进入可中断收框；酒店成功提示 660 的资源须在提交前预检。收框失败或宿主退出保留已写结果，不重试、不伪造回滚。NoTown 原 caller 直接读键，跳过酒店等待 RNG／头像 129及 660 ACK，保存后仍在同一选槽位置；Load 保持先收框、完整校验／资源准备成功后才交换 owner。原 SAV 布局和未知尾合同均不变。

水色 DAC 状态属于应用的 VGA 视觉 owner：`field_water_last_tick/field_water_phase` 不在 session 或 SAV 中，活动 Load／新 session 不清零。只有已确认的等待和步态 caller 显式推进；render／present 不隐式更新时间。原版使用 BIOS 低16位无符号差，差至少2才前进一步，16回绕且不追赶；宿主55ms tick是近似。G4 已续接 Scene、共享菜单、确认、progress 与有限通用事件的已证等待调用点，保留各自是否推进水色的区别；接线不证明所有原版caller均已运行对照，更不承诺 BIOS 与宿主墙钟同相。

F1/Page Up 由 play owner 同步呈现全场缩放图，借用已解码 terrain，不替换 session cache。开7帧、关6帧；中间只更新阵营／焦点标记，不推进 game tick、RNG 或水色。正常关闭后复用完整 `runtime_refresh(0)`，按 actor／cursor → water → terrain／range → jitter → render 返回地图，长停留不追赶；错误优先于 HOST，失败不补后缀。原 `field_auxiliary_map_execute @0x2000a` 在开动画后 clear，稳态10620检查后通过 INT16 AH10 读一键才关闭；117e7的F1分支直接return，没有119a6尾，所以保留其余独立键，不把退出键复用给下一控制器。

G5 已去除只支持stage0／shape0／24×24的非原义门，资源有效性仍逐图检查；地图高度>40时缩放3，否则4，标记边长为scale−1。缩放底图经 App 工作页 raw-prefix64000 的320stride窄桥，开关帧从该页复制到 VGA，稳态标记不回写 App。stage1／stage2正式来源与高图alias均已完成各自当前候选原生门，按审计 §13 的分段来源记账，不外推为晚关战斗或一次全新实机；held／repeat与宿主退出合同保持。

总览条件按原 `1b41d→15f84` 复用 UI0 token 展开，`-2` 以原 origin 和19px行高换行；两段完整预检及坐标验证后才写 VGA。普通来源为 stage0..29、排除尚未绑定私有败条件选择的stage16，不再仅限首三关；657／658的动态 `-4` 属于已有准备页 consumer，不给总览新增参数或 owner。此纯绘制范围不开放相应 battle。当前原生 baseline／换行资源alias的两组配对及全容器结果见阶段记录，不扩为stage16或全关卡自然运行。

继续保留已有入口门：系统菜单只在玩家 phase、browse 且焦点无可见 actor 时打开；secondary Save 在任一未隐藏 actor 带 acted `0x80` 时禁用，hidden actor 的 stale acted 不触发该门。Load 要求完整有效且当前支持的快照，不因磁盘上有同名文件就启用。不能把活动快照的限制直接套到四槽 dispatcher。

宿主默认从工作目录 `FD2.SAV` 读写，可由 `FD2SDL_SAVE_PATH` 覆盖。测试／人工对照应使用可写副本，不把该变量指向 `original_game/` 中的原始证据。长度、checksum、四槽边界及 slot 3 尾部与 checksum 重叠的规则继续由 `save.c` 和格式文档维护。

### 恢复约束

- 先验证完整容器、checksum、stage 支持性、单位数、坐标、镜头及记录语义；失败时当前 session 和原文件不变。
- 活动快照的关卡 metadata 有独立 owner：保存本关有效前缀，保留未使用尾；加载先检查 shape 与当前资源相符，并按本关已知模板数准备临时 metadata，全部预检成功后交接。不能仅更新 stage 数字却沿用旧关卡数据头。
- 同关卡恢复复用现有事务导入。跨关卡恢复先准备独立候选 session，验证成功后再切换资源、调色板、音频及 owner；不能因资源准备失败留下半个新关卡。
- 活动快照没有独立的 active-side 或 RNG checkpoint；原版恢复回玩家控制器边界，SDL 应清理 Game 的 phase cursor、路径、菜单、演出和局部交互计时，按已确认入口处理随机流。App 页、七个共享 DWORD、菜单／嘴型／水色锚继续保留，不因笼统「清计时」重置。不新增未知字段来追求读档后 RNG 序列相同。
- 成功活动Load把焦点轮询起点重置为0，失败保持旧owner；该值不从SAV读取。普通可见actor的映射cache必须在当前FDICON bank容纳完整12帧组，原方向5另验实际跨cache读取窗，不能只检查当前idle帧或套用F1的unit121／race10过滤。hidden未知ID继续保留；这不等于原版畸形hidden记录也安全。
- 成功恢复清奖励／状态续态，但保留不属于SAV的全局经验贡献；失败保持原owner。奖励、状态、phase或详情忙态拒绝保存，不能把中间写点导出成正常边界。
- 恢复不能把存档单位再次经过新建构造器重置 HP/MP、装备或等级，也不能再次播放已完成的增援事件。未知 meta、未使用单位尾部及未拥有的 cell-state 字节继续保留。
- `FD2.TMP` cache class不是职业。热身份写入保留旧cache图形，Load则按原 `0x10010→0x11019` 用当前ID／raw顺序重建旁路cache；保留原80B及存档raw2，不保存旧TMP历史或增添SAV字段。磁盘TMP文件互操作不因旁路重建而自动完成。
- 验证工具区分 fresh 构造与已 Load owner。fresh 跨端只按独立来源比较已定义字段；构造未写的9B（unit+0x28..0x30）和初始堆尾不据此判为无语义。Loaded 的未知保留以本端 preload 实际容器为基准，仅允许有来源的 cache 重建与独立 view 写集，不能套 fresh 排除区。原活动导入只复制 count×80 的热前缀，SAV unused 区仍由容器保留，不用它给新 append 构造播种。
- 写回继续使用同目录独占临时文件与受检替换；失败不破坏原档。临时文件唯一不等于多进程更新锁，目前不宣称多个游戏进程并发写档安全。

恢复成功后应用音频 bus 配置，并由 A5 已接入的场景编排请求对应曲目。活动 Load 不重播开场；准备状态 Load 回到酒店／城镇，出阵完成后才请求战场 primary。曲目请求、sequence 生命周期和声音保真分别验收，不能把音量恢复或曲目请求成功当作原版音色／时序一致。

## 6. 演出与音频边界

已确认逻辑结果只提交一次。FIGANI 接入时复用现有确定性结算，演出用提交前快照和已确定的逐击结果展示过程；从原版动画内分离出的 effect 调用必须检查，不能又结算一次。跳过动画、完整播放、静音和 null device 不得改变最终单位、phase 或 RNG 次数。

B6／G2 的 Game 仍是唯一战斗业务提交 owner；正式随机流由 App context 持有，Game 和已证 UI 等待借用同一 callback／userdata，不另建演出 RNG。物理动作分资源 PREPARE、冷图中的地图／面板步骤、逐击 STRIKE 和一次 FINISH；预检先于费用／随机数，呈现失败后不重复已提交的攻击。玩家物理始终 FULL；AI 才按选项选 FULL 或 SHORT。FULL 按两项独立条件最多三击，SHORT 保留原 OR 条件最多两击；跳过当前呈现不改变所选算法或随机数调用。

`progress.json` 的 `physical.full`、`physical.short` 和 `stationary.recover` 定义顺序及分支。SHORT 先地图刷新、反击朝向、39..48面板，再复位方向和攻击；存活且仍可反击才重画并反击。`1e611` 使用实际传入的三个参数：descriptor、actor、target。真实disp及动态SP证明两处HP读取依次为target和actor，旧“缺第四值”的解释已撤回。写序为target背景→固定55→target比例条→actor背景→actor比例条，最后从同一App投影；不在两个target条之间重画背景。HP0跳过比例，正HP/max0或首owner错误保已写前缀，详见[F4来源及回归](https://github.com/soarqin/fd2sdl/blob/373e5c0bd21454793a20fc62ecb8addb18055dba/docs/plans/f4-presentation.md)。SHORT 的武器序列、FDOTHER64样本、solid64与逐列HP条由有限呈现器完成。counter只推进11eee的地形两字段及App jitter，不推进idle/walk或水DAC。

单单位静止恢复在 HP==max 或 raw+0x25／0x26 非零时立即返回，不改显示／焦点／HP。成功顺序是隐藏光标、聚焦、wait1、FDOTHER31声4、solid200、wait1、正常sprite、wait1，最后提交夹到 max 的 HP 并恢复显示；无浮字、无 RNG。恢复与 SHORT 共用现有同步物理呈现接口及 owner，恢复不借用 Magic13。首背景按已证caller及option3区分玩家WAIT信息板与AI纯背景，不另建SAV标志。AI11可能先施法再物理／恢复，所需资源以只读 probe 在首个 RNG 前保守预检；它不预测第二行动，缺资源时可能早于实际分支拒绝。

资源 callback／resource-only probe 自身不写 Game、VGA 或 PCM。正式 AI 法术提交在 PREPARE 前已沿原序聚焦 caster／中心、设置显示上下文并清理 pending；资源拒绝保证单位、MP、贡献与 RNG 尚未提交，不保证整个公开提交调用的 Game 字节不变。不能把局部 PREPARE 快照断言扩大为 cursor／camera／失败 owner 的纯回滚，也不为此引入第二套视图状态。

FULL法术0..9由Game按原phase0..8推进同一个 `magic_fx` 状态，并记录每段FX随机数。效果、贡献、HP插值与抖动随机数按原先后交错；呈现端只读重放记录，不能再调用业务RNG或effect。HP插值到分母上限后，后续有效impact仍耗抖动随机数。effect HP与最终可见HP分别记录：原8及9的后续目标可有贡献却无impact，不能补写HP。FULL9 的 BYTE 计数跨目标及完整九tick过渡保留，回绕后仍可产生impact；省略不存在的part绘制不会跳过计数、cue或业务。真实原目标循环／过渡／RNG与实际 C 的差分范围见[通用战斗语义 §9](../reverse-engineering/common-battle-semantics.md#9-g2-short恢复与-full9-跨目标补充)。

G2沿同一同步 `magic_present` 接口提供FULL／MAP／DERIVED／COMPOSITE四模式。PREPARE先完整检查资源，禁止推进Game/RNG或借可写信息板owner更新布局；CAST、TARGET、STEP及传送四阶段借用当前结果／before／after投影。派生效果由Game先算core贡献、恢复旧HP，再按原caster每part marker写HP，duration0也保留写点；renderer只绘对应片段。32..35回地图后，各组CAST与组间POPUP_CLEAR遵循冷图。Item以action_kind区分来源，POPUP_GROUP显式控制浮字结束位置；这些同步栈结果不形成另一套持久事件队列。

普通 MAP Magic10..12 仅在 `MAP_WRAPPER_PRELUDE`（11/12）、CAST、TARGET 三个同步输出点接受 `0=继续/skip、+1=HOST、-1=ERROR`。Battle 只收宿主 quit 并返回，Game 唯一锁存 `runtime_host_quit`：prelude HOST 不扣费，CAST HOST 保已经扣除的费用，TARGET HOST 保本目标已写 HP／贡献／RNG，并停止下一目标。HOST 仍调用一次 FINISH 正常释放 exchange，不伪置 `action_failed` 或执行 loot／acted 成功尾；FINISH 自身的真实资源错误保 ERROR 且不清除 HOST 锁。玩家和 AI direct／behavior11 的当前调用栈都在后继业务前停止；skip 仍沿既定完整提交。这个限定不改变其它法术族／phase或未定义正返回值的旧错误合同。

Game持有FIGANI原始归档；演出服务借用该字段地址，活动动作期间禁止Load／close。物理／FULL／派生／复合专用演出才按需读取BG／TAI，纯地图或Item恢复不要求无关背景。主、次PCM播放器均由App持有，动态样本借用App的FDOTHER，直到voice retire后才释放。音效开关只抑制cue／stop，不跳过必需资源检查；Game不依赖音频设备是否存在。

Play 结束按“解绑两个呈现回调、关闭服务、关闭 Game”释放；错误锁存后走真实 ERROR，宿主退出走 HOST_QUIT。正常 MAP 保留最后浮字／mask 的 App 工作页与 VGA；SHORT／恢复服务的 FINISH 只释放临时资源，不额外重画。FULL／派生／复合内部保留各自原 `11cac(1)` 返回点，主动跳过另按既有合同恢复当前地图；外层 caller 的 `11cac(0)` 顺序由 loot 冷图决定。物理返回页保留普通光标；法术 caller 按 `1cff0`／`15311` 的 `51a83=0` 隐藏光标。`11cac` 参数控制水色，不能当作光标开关。地图阶段按已确认调用推进 App 视觉时钟，不调用 Game tick、AI 或额外 RNG，纯 present 不暗中更新水色。

| 情况 | 处理原则 |
|---|---|
| 查询或确认前预检失败 | 拒绝本次动作，保留来源页和原状态 |
| AI已进入原提交序列、资源PREPARE失败 | 单位／MP／贡献／RNG尚未提交；原序已执行的聚焦、显示／pending及失败owner不承诺整Game字节回滚 |
| Save/Load 文件无效或写入失败 | 报告本次存储失败，保留原文件和可用 session；不冒充成功 |
| 必需玩法资源／handler 损坏或不支持 | 明确失败；不静默跳帧、改成待机或伪造成功 |
| 设备不可用 | 使用已有 null/discard 音频降级，保留逻辑；不把设备失败等同于资源合法 |
| 开发版本主动关闭未实现的纯演出 | 只允许在明确支持的无演出模式下执行已确认逻辑；不能将坏数据当作该模式 |
| 逻辑已提交后呈现失败／宿主关闭 | 返回真实错误／退出结果，不自动重试或伪造 RNG 回滚 |

BGM 仍由应用层持有。A5 已连接 stage 0／1 的 primary/alternate、城镇／酒店、Load 和离场请求；原版 phase 边界决定切曲，渲染与音频回调不参与规则。配置静音切换不应推进 RNG 或重新初始化单位。B5／A5编排验收及其后的统一历史回归已完成，实际来源见阶段记录；音色、混音、精确时序和完整演出不由这些请求接入结果证明。

音频 source 借用的 bank／数据必须活到 retire；换关卡、关闭和 Load 不得提前释放。沿用现有有界命令队列、source retirement 和销毁顺序，不再建立第二个临时播放器。具体 A5/A6 见 [`../plans/audio.md`](https://github.com/soarqin/fd2sdl/blob/373e5c0bd21454793a20fc62ecb8addb18055dba/docs/plans/audio.md)。

## 7. 验证要求

事件驱动的冷定义位于 `data/static/flows/`，由同一份生成节点供逻辑、呈现和预检消费。条件、顺序、等待及取消路径由数据决定；有限领域原语保留现有单位／资源 owner、事务及写点。`loot.step`、`progress.flow_step` 和 `event_resume` 是现有 session 的热续态；事件父 PC／真实 actor／item、深度1 child loot 与宿主退出锁存均不写入 SAV，没有 Scene 整事件重放。线性身份预检只作局部 units／roster 投影，即使无视觉 callback 也检查 counted MOVE／future raw；不复制整 Game、不取 RNG，也不承诺任意分支组合的完整未来身份预测。六个旧正式绑定已迁移，未绑定规则夹具和 common39 私有alias仍不自动成为自然 stage 触发或开放 S。事件流的结构与来源检查见[冷定义工具](../tools/static-data.md)，不能用其通过代替原版运行或声音验收。

G2新增验证分三类，不能由其中一类推出另两类通过：

| 证据 | 已覆盖的合同与限制 |
|---|---|
| SDL无窗口逻辑／输入适配测试 | `field_magic`／`field_magic_reward`／`field_magic_commit`覆盖正常技能族、费用、raw顺序、范围0／轴线30／传送、复合组、signed贡献与预检失败；`field_item_commit`覆盖20种use code、临时WORD／EX、浮字前后耗物顺序与失败冻结；`field_ai_commit`覆盖原candidate门与MAP9完整FX组／首效果目标。真实FIGANI／FDOTHER作为只读资源不等于真实窗口输入或原版逐帧对照 |
| SDL无窗口呈现／资源测试 | `battle_animation`／`figani`／`field_visual`及列表／progress play测试验证借用结果、marker、模式／Item来源、四段传送、零duration、显式浮字组及资源错误；`field_game_system_menu`验证效果配置正式选项与原byte XOR1。它们不证明宿主WGC、全音画时序或任意caster／场景均已实机覆盖 |
| 独立局部原机器码对照 | 原 `0x1c81f`与生产派生API独立C harness的23,040向量比对完整80B、贡献、raw伤害、RNG数及HP→贡献写序；只替换明确的栈检查／RNG夹具。不覆盖FIGANI wrapper、marker插值、SDL窗口或DOSBox运行；具体来源和工具见G2阶段记录及取证脚本 |

本次合同文档更新不增加GUI通过结论。新增正式SDL输入、DOSBox代表场景、WGC与双端SAV按各候选的实际收据另列；B2／B3／B4／B6的历史结果保留原程序与场景身份，不因G2源域扩大而自动覆盖所有新路径。Windows是本轮执行平台，Linux兼容说明与未执行项继续独立登记。

- 同一状态与注入 RNG 序列下，纯查询／提交前拒绝逐字段不变；取消检查业务状态及各级返回路径，真实UI等待按已确认调用点断言RNG消费，不能要求退款。成功动作只提交一次，并断言RNG消费次数。
- 故障注入区分提交前与提交后，验证没有自动重放、额外 phase、双重释放或原档损坏。
- 每种正式命令、存取与退出原因均通过真实 SDL 输入到达；快速入口与直接 session API 测试分别报告，不互相替代。
- 新显示场景保留 DOSBox 截图／操作／时序证据，可按[图形自动化测试](../tools/gui-testing.md)在独立副本上自动获取；没有确认判据的自动运行不记为通过。Windows 原生窗口输入与 Linux Xvfb/XTest 的执行结果分别登记。现有菜单机器码差分只验证坐标与帧选择，RLE 差分只验证指定解码分支，范围见 [`../reverse-engineering/ida-bytecheck-9.5.md`](../reverse-engineering/ida-bytecheck-9.5.md)。
- 静态 LE／IDA 已通过；严格运行时 BSS 仍需新转储，不把它重新包装成一般玩法开发的前置，也不宣称该验收已通过。

## S0 结果查询与恢复边界

S0 的 `fd2_field_game_battle_result` 是已证 stage0／1 有效域的只读查询；`game.battle_result` 对应原 `53ECC` 锁存，由既有 controller／phase 后置写点更新。stage0／1 结果图不以 HP 代替 hidden；正常源域及安全防御域分别核验。活动 snapshot 导入保留 HP／flags 与旧锁存，同 stage 和跨 stage owner 替换一致；Continue 新 Game 初值 ongoing。不因 Load、方向／Z／F1／F2、空格菜单或呈现帧提前刷新结果，actor Enter 动作／详情返回后才走原后置查询。HP0 可见记录仍能进入已证选择／详情，攻击和 AI 的独立有效域不据此改变。S3 正式输入使用下文的 checked 查询处理新增条件有效域，不把旧枚举接口用于坏输入。

恢复成功／失败／取消不额外改写SAV／RNG；锁存不是第二状态或存档字段，不新增“待结果”标志，也不靠整体Game回滚复原外部owner。完整原义、时点与局部执行限制见[运行语义§12](../reverse-engineering/common-runtime-semantics.md#12-s0-结果锁存与活动恢复合同2026-11-13)，本轮实际验证与未验范围见[阶段记录](https://github.com/soarqin/fd2sdl/blob/373e5c0bd21454793a20fc62ecb8addb18055dba/docs/plans/stage-log.md)。

## S2 显式来源机制与只读投影

S2 交付的 `stage.source.*`、`stage.result.source.*` 和新增 `common.callback.*` 是有来源身份的机制定义，当时未改普通 stage 入口、controller 和存档支持门。定义本身仍不构成正式关卡支持清单。来源机制夹具显式选择定义并声明受控状态，不能据此声称该关自然可达；S3 的实际绑定须另过依赖闭包与正式流程门。

来源 Scene 只借用空闲的现有 Game／Scene owner；pending progress（包括没有 notice 的 resume）、loot／child／completion、后置等待或交互均在写入前拒绝。资源准备只建立资源和原 roster，active 表在冷 `stage_setup_source` 叶到达时才建立。每个业务叶直接写原 owner，运动叶同步到该 owner；不先执行整个 Game 图再回放演出。HOST／错误保留已到达前缀，不自动从 entry 重放。`next_stage` 只改原 stage 值，旧资源仍归原 owner；调用者须关闭并重新准备资源后才能进入下一场景，不能继续拿旧地图运行新 controller。

事件预检使用局部、只读的单位／roster／状态／地图投影，最多 64 条路径、合计 65,536 步。新增单位只确认已证身份字段，HP／库存等未知条件检查两边；复杂战斗／奖励不在预检中执行或消费 RNG。这个投影不保存、不拥有资源、不回写 Game，也不替代逐叶晚验。缺源、容量和错误路径按现有失败前缀合同停止，不提供整图事务或回滚。

公共冷流 entry 扩为 WORD，以容纳来源 callback76 的固定动作序列；common_event 节点上限 32,767，原 BYTE 热游标域仍限制 254。Scene 仅共享固定 init／fadein／fadeout 三类冷子图，使用已有 Scene 栈和预算，不新增通用调用栈或热 PC。精确来源、原码替身域及代表验收见[S2 报告](https://github.com/soarqin/fd2sdl/blob/373e5c0bd21454793a20fc62ecb8addb18055dba/docs/plans/s2-shared-semantics.md)，当前顺序只查[总计划](https://github.com/soarqin/fd2sdl/blob/373e5c0bd21454793a20fc62ecb8addb18055dba/docs/plans/development.md)。

## S3 结果调用与准备交接的增量合同

### 结果查询、错误与原调用点

正式路径使用 `fd2_field_game_battle_result_query`，输入为 `const fd2_field_game *`，输出为 `fd2_field_battle_result *out`：成功返回 `0` 并写 `out`；缺少实际分支所需的 raw、锁存值有误或定义不可消费时返回 `-1`，`out` 与 Game 均不变。纯结果绑定沿现有 const 图查询；带故事／写锁存的绑定只读当前锁存，不在 getter 中执行故事。原枚举接口仅保留给已证有效域的兼容调用，正式控制器、自动入口、动作收尾、返场和结算投影均使用 checked 路径。

可写调用点只在查询成功后更新既有 `battle_result`；失败进入首个 `failure`，保留已提交的单位、资金、库存、RNG 和 owner 前缀。const 结算投影仅返回失败，不修改 Game 或输出。不能将失败改判为 ongoing／胜利／败北，也不能终止进程代替 session 错误。活动 Load 接受的 64 人 stage18 在晚期回合缺 raw64 时，允许完成原快照恢复，再在需要该 raw 的查询边界拒绝；不抬高人数下界以挡住合法早期恢复。

原结果表有五个已证 CALLsite，分别属于玩家后置、phase 状态和 AI 两类 outer；完整地址、参数与条件 raw 域见[共享运行语义 §14](../reverse-engineering/common-runtime-semantics.md#14-s3-结果调用者与条件有效域2026-11-14)。side1 按每个 raw 查询，side0 两个 pass 都按每个 raw 查询，包含未进入 AI 本体的 raw；不能仅凭 `1197b` 的来源记录省略 phase／AI 查询。正式 App scheduler 拥有 AI outer；同步 `commit_ai_*` 机制 API 仍是动作 helper，不因此升级为完整 outer。

### 结果等待沿现有 owner 返回

`event_resume.completion` 只标记当前结果图的有限返回点，不增加业务副本、平行 PC 或 SAV 字段：

| 完成标记 | 等待结束后的唯一续点 |
|---|---|
| `FD2_EVENT_COMPLETE_PLAYER_RESULT` | 原玩家结果调用已返回，再执行 `13565`／必要 phase／pending 尾，不重新查询结果 |
| `FD2_EVENT_COMPLETE_AI_RESULT` | 当前 raw 的 outer 结果调用已返回，再判锁存与下一 raw／pass |
| `FD2_EVENT_COMPLETE_STATUS_RESULT` | phase 的结果调用已返回，再从原 progress PC 继续逐 raw／byte 的六状态计时循环 |
| `FD2_EVENT_COMPLETE_LOOT_RESULT` | 返回 loot 图 `finish.result` 的后继边，不重启 descriptor、奖励或查询 |

奖励 CLOSE 提交 EX 并清原全局后，才恢复 AI tail／pending／结果调用。phase 先完成全部中毒提示和死亡整理，再查询结果；结果故事未 ACK 时不能递减六状态。成功交给结果等待时清除已消费的 progress notice，保留 resume／side／cursor／PC；旧 notice 的重复 ACK 不得改变 Game 或 RNG。失败前不清尚未交接的 owner。

已写锁存不等于结果函数已经返回。Play 必须等 `event_resume`、loot／progress 和原后置续行结束，才交接胜败；HOST 与真实 ERROR 保留各自原返回合同及当前前缀。39／41 的 mode0 物品 child 对非 side2 的原 no-op 只结束 child，再回父故事；不能清空父 loot 缓存或执行父奖励尾。库存写点、满包 Cancel 和 HOST 不消费未到达的后缀。

### 来源 entry／victory 与只读准备导出

正式来源 entry 的准备只建立本 stage 资源和原 roster，active 表由 Scene 在 `stage_setup_source` 叶单次构造。正式 wrapper 保留原准备记录偏移 `0x238d + (stage - 1) * 31`；显式 S2 来源机制 API 的旧 offset0 域保持原身份。`prepare_stage2` 复用统一正式入口，不能再先执行整段 Game entry 后重放 Scene。

来源 victory 的持久同步、追加、等待和 `next_stage` 按同一冷图单次执行。结束后 `fd2_field_game_export_preparation` 只复制当前 stage、完整 32 条 roster 区（含未用尾）、人数、资金及选项到准备状态，成功返回 `0`；busy、无有效准备域或不具备下一入口能力时返回 `-1`，不改 Game 或输出。准备域先查原 NoTown flags，再查普通31byte行；27—29没有普通行不等于没有准备域，30内部资源则不能导出为下一战场。它不再同步 active、不重算、不新建角色、不消费 RNG、不写 SAV，也不关闭资源。

部署槽数继续读取原 metadata byte1；来源 capability 不手抄一份槽数。`next_stage` 只改变原 stage 值，旧地图／资源仍归旧 Game。App／Play 在取得只读导出后按正式交接路径关闭旧资源并准备新场景，不拿旧资源运行新 controller，也不再调用一次 legacy settlement 提交。导出成功不代表已写盘或完成下一关入口；实际失败、原生门和阶段完成只在阶段记录登记。

## S4 旧调色调用与错误前缀

旧 Scene、App 和准备页的 fade 共用 `stage.source.fade.in/out`，不再维护比例色阶或副本。公共 `fd2_scene_palette_fade` 只接当前单 App／VGA 的固定 helper：HOST 和 Scene 真错误保留终态并向上返回，不能复位失败后继续提交业务。fast 保终态且不等待，旧入口不增加中间上传；source 机制原有 fast 上传合同不变。

转场 Game API 保留前段布局与 FX，仅把 `24618` 的 500 ms／32 级加白尾交给唯一 `stage.palette.transition.finish`。三个固定调色 helper 只允许 palette_add／palette_subtract／delay／end，沿现 Scene walker、栈和预算，不借假 Game 或增加热执行状态。独立插值原语仍保留；普通 void／clamp 暗度接口委托原减法。

每叶错误／HOST 保留已到达 DAC，不改 base palette、Game、SAV、RNG，也不执行后继业务或伪造整体回滚。纯 DAC 上传不推进动作／水色或读取普通键；原 delay 处理宿主事件，普通输入仍归原交互读取点。全部原函数、合法 6 bit 域、等待替身和实际原生边界见 [S4 报告](https://github.com/soarqin/fd2sdl/blob/373e5c0bd21454793a20fc62ecb8addb18055dba/docs/plans/s4-unified-review.md)，不承诺每个 2 ms 请求均成为独立物理显示帧。

## S5 源入口、局部资源与终态合同

S5适用验收已闭合，已证域为stage0／1原范围及stage2..29的战斗／END／事件／胜败交接与结局；默认255仍有源拒绝，resource30只供内部尾声。提交／本地集成及各候选实际身份见[S5工作包](https://github.com/soarqin/fd2sdl/blob/373e5c0bd21454793a20fc62ecb8addb18055dba/docs/plans/s5-batch-expansion.md)，本文维护消费者合同，不扩大为自然全游戏／全随机或任意外部存档等价。

`24754`在改变热stage后仍使用已打开的FDTXT23；`25757`先改热stage为30，再显式打开FDTXT31。Scene借用当前文本入口向正式Play资源预检传递`text_override`，和只读units／roster／map视图同寿命。预检与实际对白消费同一资源，不在Game／SAV记录第二入口，也不关闭借用的owner。

`fd2_field_play_bind_event_preflight`只向Game写入同一私有资源preflight callback及借用的fdother userdata两字段；正式Play和来源Scene使用相同binder。source `delay`是无资源引用的原纯等待，合法域为0..65535ms，不能转成common `delay_ms` 后受1000ms上限截断。等待不额外刷新地图、推进水色／actor或取得业务RNG；fast仍沿HOST检查。该合同不扩大common等待叶的参数域，也不为资源预检保留测试专用调用。

`24618`临时页、`22253`传送及终章绘制资源均由当前Scene scope拥有，所有出口释放局部分配。PCM bank只借App存活的原archive字节；播放器仍使用现有primary channel与禁用门。释放bank索引不销毁正在播放的原样本。条件、帧序、等待、skip、分支与返回／不返回合同只维护在冷图；固定callee白名单、深度8和局部预算不变，不接受任意目标跳转。

传送原写点先提交raw.xy，再执行mask与逐行wipe；后续资源错误不恢复旧坐标。callback82通过现有event PC→Play→唯一Scene helper→reply执行，每叶保留原等待、HOST及失败前缀，不新增业务执行游标或平行热状态。

C22的HOST前缀测试在真实`teleport_commit`完成后、下一`teleport_top`节点进入wipe前交付quit，xy由真实module写入。display0／1各自的成功、ERROR、HOST六场及缺cursor门由同Play目标链接的`field_s5_callback82_scope_checks.c`私有Scene TU执行，不借已移除的delay资源钩子，不写假坐标。旧C21钩子未触发的失败保原身份；删除临时诊断fprintf没有放宽精确PASS／空stderr门，也不把失败前缀测试称作原版宿主退出等价。

credits的`1088d(30)`只替换原资源、构造active及其缓存，不强制把坏结局热stage26改成30，不重置turn／camera／focus／event／result／roster／RNG。29 caller本身已先改热stage30。内部metadata部署字节31不套普通准备UI20槽；resource30不是新增战场能力。资源打开、active构造、帧缓存检查和group0追加都先在局部resource30上完成，全部成功后才整体换入Game；任一步失败返回错误，Game的资源与active表保持调用前状态。

S5 `cin_animation`仅消费完整原caller确认的ANI0／2。原20421的52766是临时调色区，未写永久53A65或DAC；复用现decoder时每帧present前恢复进入该叶的base／DAC。像素、输入、时钟和失败前缀照常保留，旧ANI调用路径不因此重新签证。


原`31529`的montage位于`319d3 call31c49`返回之后，活跃表已由credits的resource30构造。`31a9c..31add`直接写同一`53A45`的raw0／1 side与ID，`2ef9e`写target+0x40的HP WORD63680；不是另建两条业务记录。冷图沿既有`unit_field_write`提交这些实际Game写点，图像桥仅借两记录的绘制快照，不能让副本成为HP owner，也不重算cache／textID或同步roster。HOST／失败保留已到前缀，credits之前不得凭新蒙太奇提前改这些字段。

`stage.cinematic.montage_round`是第6个固定、只准ending调用的冷callee，最大22part／10tick仅为已证40clip的有限支持界；part数、duration、marker仍从FIGANI逐叶读取。两侧、3%最多二轮、marker HP、半场最后marker早返与每tick等待在冷图，沿既有Game RNG；原override将25a96挡在bank解引用／AIL之前，不伪造新clipbank声音。真原码／C索引帧相等不提升为整段RGB、声音或绝对时间相等。

Scene source 业务叶失败时先将 Game 已到达前缀装回当前 Scene view，再传播错误；不能让旧 view 在收尾时覆盖刚提交的身份、HP、坐标或状态。呈现 scope 只释放本次临时页／sheet／索引，未到达的冷后继不执行。宿主关闭同样沿现 HOST 结果返回，不把已提交业务或外部 RNG 伪装成可回滚事务。

NoTown 准备沿原 flags 独立于普通31 B准备行。`26152` 的入口顺序为清 VGA64000字节→`11d40(0,255,0)`→notice75／text410；恢复的是当前 base palette 对应 DAC，不改 base。这样 `250cc` 的白 DAC 不会泄入 stage27 准备。原退出 darkness255 仍在自己的写点，不能用入口正常色省略它，也不能因上一场景恰为 normal 跳过入口叶。

`terminal_wait` 是无正常返回边的末端，只有既有 HOST 出口；普通键不使26坏结局或29终章继续到下一战场。终章 helper 内部返回与 caller 的不返回分别表达。其执行期间仍持有当前 Game／资源 owner，不能提前导出准备、关闭或复用为 stage30 battle。活动／四槽存取继续只允许既有空闲边界；中途等待、terminal或失败状态不增加可保存的平行字段。

正常招募的已证人数上界为31；外来32人credits超出原构造／raw引用有效域时明确拒绝，不伪造第32个active或截断持久表。完整32条roster及96条active的SAV格式保持，资源构造、动态raw／头像和每个实际到达叶仍需自己的前缀检查，不能据容量声称所有人数组合等价。正式entry29的Title LOAD／NoTown／七次共享22253与27active存档已由C22完整pair另验，独立于82分层组件；turn255／phase0的82绑定未被伪称入场turn1触发。

该人数界来自原caller而非缩小格式：`3231b`初始四ID为0／9／4／30，完整已恢复招募literal与初始集覆盖0..31，但18／19互斥。`23a0a`招18；`24754`先经`24bde(18)`查整张roster，仅无18且signed turn<15才招19。`11506`按原textID同步，不删除人数或改身份；正常单次触发域不能推广到外来重复callback。`1088d(30)`虽malloc96容量，原metadata.byte1=31只初始化raw0..30；credits从`count-1`开始且无源clamp。外来32人不能将未初始化raw31或另一持久记录补成已证active。
