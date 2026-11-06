# 战场 session、动作提交与场景交接设计

本文约束现有模块的后续扩展，补齐审查发现的状态、失败和验收边界，不要求重写引擎。基线为 `e9a2d81`；标注「待实施」的内容是设计决定，不是现有能力声明。优先级见 [`../plans/development.md`](../plans/development.md)，工作包见 [`../plans/battlefield.md`](../plans/battlefield.md)。

## 1. 现有模块与状态所有权

| owner | 持有内容 | 不应承担的职责 |
|---|---|---|
| `main.c`／`app_flow.[ch]` | 应用生命周期、标题分派、BGM、当前场景去向 | 不在菜单返回值转换时抹掉胜／败或错误 |
| `field_play.c` | 正式输入循环、时间调度、系统动作编排、演出调用与退出原因 | 不复制伤害公式、候选评分或存档 codec |
| `field_game.[ch]` | 单位、回合／phase、坐标、事件、结果及交互临时状态；所属 stage 资源 | 不直接控制文件格式、SDL 按键事件或音频设备 |
| `field_attack`／`field_magic`／`field_ai`／`field_unit_stats` | 范围、计划、效果与属性计算 | 查询不消费 RNG，不偷偷提交 UI 或全局场景状态 |
| `field_system_menu`／`field_manual_slot` | 各自页、选择、确认及高亮／动画状态 | 不把活动快照与四槽混成一个 dispatcher |
| `field_save.c`／`save.c` | session 与原版记录转换、整文件保留式更新和安全写回 | 不把临时交互、指针或额外 RNG 状态写进未知字节 |
| `scene`／`field_effect`／绘制模块 | 对已确定状态的画面、文本、动画与 cue | 不重复扣 HP/MP、移除物品或设置 acted |
| `input`／`audio`／`audio_pcm`／`bgm` | 宿主输入帧、播放命令、设备和 source 生命周期 | 不推进战场 RNG、phase 或剧情 |

保留现有分层与调用点。只有某项功能确实需要共享代码时才抽取局部 helper，不先拆分 `field_game.c`、建立通用 command bus、事务框架或剧情 VM。单位和存档的字节布局以 [`../formats/data-formats.md`](../formats/data-formats.md) 为准，本文件不维护第二份格式表。

### 持久与临时状态

- 原版单位始终是 `fd2_field_unit` 的 `0x50` 字节记录。玩法字段写入唯一记录，不能再保存第二份 HP、坐标或装备真值。
- 模板新建、开场构造、完整存档恢复和派生重算是不同层，不存在可代替全部路径的通用构造器。当前基础表覆盖角色 ID `0..31`、敌军 ID `68..135`；无对应 record 的 ID 明确拒绝，stage 0 捕获值只作夹具，不是通用默认值。
- FDICON 负责地图帧，DATO 当前只确认是立绘包，FIGANI 负责战斗演出；不能拿立绘索引猜属性或复用 TMP cache class 当职业。
- phase cursor、范围／路径、菜单、演出快照、计时与 sprite cache 是 SDL 临时或派生状态，不直接序列化。
- `option_values` 与 `resource_value` 已有 session owner。配置是逐次 toggle 立即提交：未修改时退出不能改状态，已确认 toggle 后退出也不能擅自回滚。
- `fd2_field_game` 含有拥有所有权的指针；结构体浅复制不等于可独立关闭的 session，也不等于外部 RNG 的快照。不能关闭两个共享资源的副本。

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

当前AI无演出法术在完整预检后、首target effect前扣一次MP；ID17按原wrapper使用费用记录18。费用不放入逐目标纯effect，多目标不重复扣，miss／免疫／状态无变化仍消费，后置失败不退款。原版FIGANI分支按caster part标记触发，不能由此宣称所有动画演员恰扣一次；动画与玩家指令仍按B2／B6各自证据开放。

单位详情沿用独立 `detail_unit`／portrait owner，页索引只在装备与未选中法术详情间切换，不复用行动列表或selected_unit。打开前预检两页必需资源，失败保留现有详情与portrait；成功Load释放详情并重置临时页，失败Load保持原session。F2详情不推进controller，Enter路径只在最终关闭后推进。

### 玩家法术／物品扩展（B2/B3）

在现有交互状态中区分列表选择、施法中心／目标确认和返回路径，不将攻击专用 `TARGETING` 未加区分地用于全部命令。列表可用性取自原版玩家入口，已确认范围和 effect helper 可复用。

列表状态已接入：MAGIC_LIST保存临时索引，step不负责关闭，取消关闭后才回COMMAND。原版18890对非零路径禁法，只有unit ID18／19／34例外；此门与空法术、record+0x27禁止状态共同检查。低MP只阻止当前项确认。正式入口限 stage 0／1 并要求共享RNG、至少一个可提交的0（caster unit9已有费用标记证据）／13；其他已知ID可显示／导航，但逐项确认仍拒绝未实现效果，不能因新学17就禁用原有0／13。未知位36..39及超出现有12项可见布局仍拒绝，不从AI支持表推断玩家全能力。

物品B3的已确认返回合同独立于MAGIC：Use列表取消使1bbdc返回0，18d8c重开ITEM子菜单；子菜单取消使1bbdc返回-1、18d8c返回0，再由18890重开COMMAND。Use目标取消先聚焦caster，再重开1b932列表并归0选项，保留临时移动及pending。53C53仅由Give完成分支置1，不能把它当作所有库存改变标志；装备／丢弃已提交变化也不能因为该值0而回滚。

ITEM_MENU／ITEM_LIST 共用正式输入与四向 command 呈现，语义为 Use／Give／Equip／Drop，各项独立重验能力。列表索引是临时 raw 槽，step 只报告可确认；列表完整关闭后才进入目标或提交 Drop／Give，Equip 成功确认继续原页。列表取消回 ITEM 首可用项，子菜单取消回 COMMAND 首可用项，两层均先于 COMMAND 原移动回滚分派。成功 Load 清列表、目标、收方及 Give 已提交标志并回浏览，失败保留所有旧 owner；这些临时字段不序列化。

ITEM_TARGETING 每次按当前 raw 槽重新读取物品的玩家 cast／area／filter，复用 move_range 与互斥 target 候选 buffer，不缓存第二份 slot／物品真值。候选按 raw 顺序和原版 filter 收集，不额外过滤 HP0 或 acted；121 占轮询槽但不移动焦点。范围 LUT 与确认时的 Manhattan 判据分别保留，area 光标沿用 20 帧资源。目标取消先聚焦使用者、清目标 owner、返回 ITEM_LIST0，保留移动路径／origin、pending 和 RNG。法术与物品共享范围建立和轮换，保留各自 record／列表／能力门。

Use 按 `field_item_use_effect_execute @0x20c6f` 分派实际 code5..24。HP／MP 恢复、清状态、状态附加和法术伤害复用已确认的局部 effect；增长物品只写首目标的原始 WORD，并重算使用者，MOV 增长保留跨 WORD 的 EX 字节。code6／7 仅在对应状态存在时先恢复 9HP 再清状态，仍按原版消费一次物品；状态不存在时不消费效果 RNG。AP／DP 状态的直接增量与装备重算是两条不同公式，不合并舍入行为。

Use 在首个 RNG／业务写入前预检全部目标、可能败亡的掉落描述符、最终格和必经尾链；清状态可能救活的 HP0 目标须纳入预测。只在原版规定的 code5／6／7／8／9／10／11／17／18／19 移槽一次，其余不消耗。原版物品尾清全局经验贡献，不清单位 EX；之后执行败亡、acted、最终 lookup1 和 controller，不做 WAIT 恢复或物理方向复位。提交后错误保留效果／压槽／已到达的 acted 与外部 RNG，首错冻结，不退款或重试。

AI物品的掉落入口返回非零不等于仍有待续态：`field_loot_descriptors_execute @0x1aa1d` 的 mode0／1 可以同步终止。只有实际 `progress_pending` 才延后 common tail；同步、直接提交、scheduler和异步恢复各自恰执行一次，不遗漏 acted／贡献清零，也不补跑已完成的尾。

code23 使用独立 ITEM_TELEPORTING 第二层：首目标 cast 排除中心整格，门为使用者 text_id24 且最大 MP 至少20；第二层选择目标可站立且无其他可见单位的目的格。任一层取消均返回使用者的 Use 列表0。成功时按原版 WORD 减 MP 并移动首 raw 目标，不另加当前 MP 门，不把中间演出坐标255写给常规 renderer；自身移动时预检最终目的格的事件。

正式 ITEM 查询无写入／RNG，仅接受 stage0 可行动玩家和连续已知库存；正式等待入口另要求共享 RNG。code0 装备可查看，不能在 Use 确认。Drop／Give 不套 Use handler 门，包含隐藏 flag40 的全8槽须满足各自派生检查。Equip 许可来自原版29×7字节表，只比较每行前6字节，未知职业／物品拒绝。能力查询不借用 MAGIC 的移动、MP 或 unit+0x27 门，能力失效也不能阻止原有取消。

Drop 用同一 ITEM_LIST／mode0，code0 可确认；确认先在80字节 actor 副本移槽，确认或取消均重算四属性。余项回 ITEM，删空回 COMMAND；不结束行动，之后取消移动保留丢弃结果。Equip 在80字节副本按 ID `<128`／`>=128` 清同组整个 flag、选中写40并重算；重选不是脱装。不兼容不重算，取消不重算，成功留原页／原 raw 索引。二者均在全部数据检查通过后交接，不改 HP／MP、EX、acted、pending 或移动 owner。

Give 用独立目标与收方列表层。相邻友军使该格可选，确认后按该格首个可见 raw 记录处理，不擅自增加 HP／acted 过滤。收方有空槽则压缩来源并追加；满8槽时打开 mode0 收方列表，双方各移除选中槽、以 flag0 追加交换 ID。只重算送方，保留收方原派生值；源列表、目标和交换列表取消均按原调用链重算送方并返回，不清已成功交接标志。53C53 对应的临时 `item_transfer_committed` 只在新的移动选择或成功 Load 时重置；成功 Give 后 COMMAND 取消保留移动及库存，并走不回血的物品行动尾。

私有 modal 独占列表确认键；退出、资源错误和完整关页在不可逆提交前检查，caller 耗尽当前帧普通输入以免重复确认。Equip 留页复用同一 portrait／page owner，成功重画后继续原索引，不兼容保留当前眨眼画面。提交后重绘失败保留业务结果并冻结，不重试。实现与逻辑测试不等于原版运行验收；各冻结候选的双端 WGC、SAV 和精确随机范围见[阶段记录](../plans/stage-log.md)。

普通物理目标同样复用14818／115b6的互斥范围和raw候选owner：按武器min／max与filter0建立范围，首次聚焦首raw，Z／小键盘5先递增再取槽；121占槽而不移动焦点。取消先回actor，再清候选／范围并重开COMMAND，成功尾同样清cache。候选不额外滤HP0／acted，提交仍沿独立物理有效门重验。移动预算0可保留原点，不得在game wrapper整体拒绝而令AI错过原地攻击。

MAGIC_TARGETING复用cast范围而独立持有法术及raw候选索引；0／13仅area0，方向可离开范围，确认时重建当前范围与全部同格filter目标。完整效果／贡献上界、无掉落支持门、B4成长及必需消息资源和必经事件预检先于费用／RNG；miss仍扣费并行动，多目标只扣一次。收尾独立于WAIT恢复及物理方向复位，按效果／败亡、acted、归一、最终lookup1、奖励续态、controller顺序执行。预测phase必须考虑本次死亡清理后仍可行动的玩家，不能给13565本体添加HP门。FIGANI与完整附加呈现随机流仍归B6，B4消息的本阶段运行对照另登记。

必须先明确：玩家 MP 扣除时点、施法失败／无目标的处理、目标阵营、物品使用与库存移除的关系。AI `field_ai_magic_action_execute @0x15311` 已有的 MP 可用性门不证明玩家扣 MP 规则；已确认 AI item code `13/20/21/24` 不消耗，也不证明玩家同名物品一律不消耗。玩家证据入口为 `field_player_magic_command_execute @0x1cff0` 与 `field_player_item_command_execute @0x1bbdc`，登记见 [`../reverse-engineering/function-names.md`](../reverse-engineering/function-names.md)。

### 奖励与状态续态（B4）

`experience_value` 对应原全局53EC8，与单位EX、消息动态数字53AE1分别处理。物理每击按原门覆盖该值；法术／物品共享效果在HP／MP写入后立即累加，负状态的伤害贡献必须先于后续duration RNG。恢复贡献以signed实际差计算，old超过max再cap产生的负差按DWORD回绕，不能夹成0。玩家controller先cap99，AI物理尾没有该cap；AI法术和物品在原尾清零，不能移到首效果前。活动Load不清贡献，也不将其写入SAV。

`field_unit_experience_reward_execute @0x1e292` 无side门。贡献0、hidden或精确等级cap（ID30／31等于99、其他等于40）早退并保留全局。成长支持已有ID0..67记录，不套创建器0..31门；未知成长数据或不安全的signed运算在提交前拒绝。普通40仅为入口门，循环只在新等级30或特殊ID新99清余值，不能另加每级>=40门。

game保存一个有界续态，play只持有背景／空底板／四帧立绘。LEVEL前先写level；每个非零成长量先提示／ACK，返回后才写WORD；学习先OR位，再显示587。每级末重算，最终自动等待和关闭后才写EX、清全局。最大HP／MP增长不改当前值。资源预检可用80字节纯副本，不能提前交接最终单位或消费RNG。

中毒按raw actor先扣 `maxHP/10`、聚焦、显示487；全部毒伤后才死亡清理。六状态byte逐项递减，恰归零就聚焦→481..486→等待／关闭→重算，再处理下一byte。失败或宿主退出不ACK后续写点；保留已提交HP、计时、成长及外部RNG，不假回滚。入口quit先于core resume，scroll／开合中途quit停止并释放缓冲。

成长固定取pair第二byte，无成长RNG。489／490..494的-3以任意键ACK，488／587和481..487无-3，487的-2只换行。`dialog_wait_key @0x16c57` 每次入口与眨眼复位耗共享RNG；`field_notice_wait_ticks @0x1e5c0` 自身不耗该RNG。宿主55／110ms门是BIOS时钟适配，跨框嘴型相位、完整音画／随机流仍未声明一致。

本阶段六组实际WGC来源为C4四组＋C5两组，消息资源、详情和完整Save／WAIT／Load／Save均通过；不是最新程序重新跑六组。SDL回合编号横幅未图形对齐，使用有序phase日志与后续真实WGC／完整SAV核验，不把日志当横幅像素；完整FIGANI、跨框嘴型相位和共享RNG等价仍不声明。最终C6仅修AI结果误用玩家HP前值的printf下溢诊断，业务、RNG和呈现不变，无新GUI。

大阶段输入复审另保留两项既存风险：非ITEM的COMMAND关闭动画返回值仍有被忽略的调用，动画期间宿主quit可能先执行WAIT再退出；历史首次END确认落到NO的原因仍未定位。B4新提示的入口／scroll quit已单独处理，不把本阶段定向运行当作这两项已修或全历史输入回归。

AP／DP重算使用binary64 1.15乘积先53位舍入再向0截断，推翻旧nearest-even说明，不能换成整数115/100。直接增益 `old + floor((15*old+100)/100)` 是另一条已证u16公式；AC／EV状态只各加15，不乘1.15。局部原码复现见[IDA工具说明](../tools/ida.md)。

## 3. 事件执行与停止边界（B0）

`event_log` 最多保存 64 个 notice，兼有诊断与 `presentation_deferred` 驱动用途。当前实现保留这个有界容器，但必需事件在 handler 执行前检查容量，不再丢弃必需 notice 或把未知事件记为 `unhandled, skipped` 后继续。

设计决定：

1. 需要改变玩法或推进剧情的 handler 是必需逻辑，未知、失败或容量不足不能算作成功。
2. 逻辑已提交而必须呈现的 notice 也不能被普通日志溢出策略丢弃。最小实现可在追加前检查容量并显式停止，不要求引入持久事件队列。
3. 只有确认不影响玩法、呈现和后续控制流的诊断信息才可丢弃，且保留丢弃计数。不能仅因事件名称未知就归入这一类。
4. 原版要求的 turn event → phase 状态处理 → actor/cell/stage handler 顺序不变。原始 actor 遍历中的 skipped actor 仍可能需要 stage handler。
5. 若未知 cell/stage handler 只能在动作提交后发现，停止并报告 `stage/actor/action/phase` 与已提交边界；不能改成待机，也不能假称回滚了整个动作。

回归至少覆盖玩家和 AI 两条路径、notice 满边界、同一事件不重复提交、演出失败、提交后不再调度下一 actor，以及失败不会被应用层报告为正常完成。

session 的 `failure` 与 `failure_notice` 只记录首个不可恢复错误及其 turn、phase、actor、slot、action 和坐标，不写入原版单位或存档。记录首错后清除自动调度时间；玩家确认、移动、攻击、AI 提交和 phase 调度拒绝继续执行。正式循环先检查错误，再判断胜败；不能让同一动作导致的胜负门掩盖事件失败。

原版共享路径每步以`match_arg=0`更新单个pending，未匹配保留之前命中；移动循环不分派handler。AI normal common tail查询最终格`match_arg=1`，命中才覆盖，再acted、全方向reset；outer随后分派pending。未知handler在实际分派时冻结，保留已提交效果、坐标和acted；读取损坏事件数据仍明确失败。pending是session旁路状态，不写原版存档，成功活动Load清除，恢复失败保留原状态。

玩家完整路径后进入COMMAND，成功动作先acted再最终lookup；普通command取消回滚并重入移动选择、清pending，下级目标取消或无效确认保留pending。selection取消不acted／不lookup1，仍经过controller后置。零步WAIT恢复HP，非零路径WAIT不恢复；特殊cell-action尚未支持时在WAIT恢复／acted前明确冻结，不能当普通pending处理或报告成功。

当前controller先完成B4奖励续态，再恢复stage0结果门、精确13565门、必要整轮phase后的pending分派与清空。13565只看flags&0x81、side2和raw26，不加HP或bit04门。异步phase期间仅保存原actor与等待标志，不保存pending副本；AI仍可清空／覆盖同一slot，raw尾必须扫描完才换phase。side1尾先全表清acted，再进入phase0事件／状态，删除旧延迟清除补偿。Enter的普通不可操作actor详情关闭后走同一后置，F2直接返回。

controller完成时由play owner耗尽当前帧普通键，保留held／repeat；core不接input。等待phase时拒绝导出／保存，成功活动Load清除续行与pending，失败恢复保留完整原状态。此处恢复的是已确认控制顺序；未实现的cell handler、特殊格、物品已交接取消例外及跨地图镜头仍单独登记。

批量行军按 `field_empty_focus_menu_execute @0x16f55` 的独立顺序提交：确认后捕获固定绝对焦点，按 raw actor 顺序仅处理 `(flags & 0x85)==0 && side==2`，不附加 HP 门。每人先 focus、清共享 pending、执行完整路径的 match0 收集，再分派当前 pending；成功后才重置全表方向、标记当前 acted。无路仍走尾部，不查询最终 match1。当前 handler 失败保留已走路径和先前 actor 的提交，立即停止，不伪造整批回滚或继续 phase。

整批后只启动一次 side1→side0→新玩家回合，不自动 Save。March 期间由既有私有路径呈现参数关闭选择／信息层；play 局部续行把相同参数传给异步 AI visual step，普通 END 默认使用原呈现。恢复玩家阶段或终态后清当前帧普通键，保留 held／repeat，不把显示策略或 pending 副本存入 session。长路径中途的即时宿主退出仍沿现有路径协议，未声明逐动画相位响应通过。

攻击、法术或物品进入效果提交后发生的错误保留当前 HP、库存和外部 RNG 进度，锁住 session；behavior 11 已施法而后续物理动作失败时也停止，不再按成功执行 common tail。

成功演出后由呈现 owner 清除 `presentation_deferred`，同一 notice 不在下一帧重放。停止状态禁止保存，以免把部分提交的状态写成正常可恢复快照；显式加载有效存档可以恢复并清除错误，加载校验失败则保留完整停止状态和原文件。

## 4. 战场结果与应用去向（B5 已接入，整体验收中）

`battle_result` 的 stage 0 胜／败通过 `fd2_field_play_result` 与 `app_flow` 分别传给应用层。正式循环先处理 session 错误和宿主退出，再推进下一 actor；快速检查不触发真实胜败结算。

`COMPLETE` 仅表示无交互快速检查结束。B5 候选的正式 App 流程在 stage 0 胜利后播放本关结算，同步持久表并进入 stage 1 准备页；败北完成 Game Over 后返回标题。独立 `--field-play` 不持有 App 准备状态，仍只报告胜／败并退出 `2`，不能把该调试结果当作跨关完成。游戏内离场、宿主关闭／快速检查退出 `0`，错误退出 `1`。离场使用独立 `LEAVE_GAME` 来源，不能改作重新进入标题。B5受支持范围及其后的统一历史回归已完成，具体来源见阶段记录。

| 结果 | 应用职责 |
|---|---|
| 仍在战斗 | 继续当前 session，不离场 |
| 胜利 | stage 0 完整结算后交接准备状态，再按 stage 1 专用入口出阵；stage 1 → 2 仍拒绝，不伪造下一关 |
| 失败 | 完成已确认 Game Over 流程后回标题；不能按胜利结算、递增 stage 或吞掉失败 |
| 游戏内离开战场 | FDTXT[416] 后独立返回 `LEAVE_GAME` 并退出应用；资源失败为错误，提示期间宿主关闭仍为 `HOST_QUIT` |
| 宿主关闭 | 结束应用，不重新进入标题 |
| 不可恢复错误／必需逻辑不支持 | 停止当前流程并失败，不返回正常完成 |
| 快速检查结束 | 返回检查结果，不执行胜利结算或自动保存 |

`DS:0x3ecc` 等剧情退出标志不是所有 stage 通用的胜负枚举；由本关 handler 和调用者解释。现有非 stage 0 通用全灭计算不构成开放该关卡的依据。

B5 的交接顺序应先确认原版 callback、持续单位字段和剧情，再准备下一关资源及 roster；成功后切换 owner，最后释放旧 session。不用 `stage++`、重放新游戏模板或复制旧地图代替关卡初始化。失败时不破坏原档；下一关尚未准备完成时，不释放唯一可诊断的旧状态。

## 5. 存档与恢复

### 两种原版路径独立

| 项 | 活动快照 | 四槽手工存档 |
|---|---|---|
| 原始单位区 | `0x1e00` 字节，格式容量 96 条 | 每槽 `0xa00` 字节，格式容量 32 条 |
| 入口 | 标题 CONTINUE／secondary Save/Load | 标题 LOAD／酒店 `field_hotel_menu_execute @0x29300`；B5 候选已接入 |
| 保存数据 | 本关 metadata、独立持久 roster、活动单位、cell state、stage/turn、镜头／焦点、配置及保留区 | 持久 roster、已确认 meta、配置及未知尾部 |
| 当前可恢复关卡 | stage 0／1，跨 stage 先准备资源再交换 | App 恢复 stage 1 准备状态；旧 Game 手工 Load API 仍拒绝导入活动 session |

runtime `FD2_FIELD_MAX_UNITS` 当前为 64。**64／96／32 是三种不同边界，不能统一为一个猜测值，更不能保存时截断。** 跨关卡前必须确认实际 roster 与对应原版存档路径的关系。

活动 meta[1] 与 meta[9] 分别属于当前 actor 和持久 roster，不能互相约束或由当前 side 2 人数重算。持久记录按 text ID 表达跨关身份，新游戏按原版构造顺序建立；活动部署再设置位置、运行期 cache 和临时状态。未知字节在加载／保存往返中保留，不能从活动记录盲拷来构造新持久角色。

session 持有完整 `0xa00` raw roster，包括未用尾；活动与手工保存均写出这整张表。加载 A 后保存到 B 或新文件时，roster 尾来自 A，metadata 未用尾仍由目标容器保留。这两个保留规则不能混为一条。

水色 DAC 状态属于应用的 VGA 视觉 owner：`field_water_last_tick/field_water_phase` 不在 session 或 SAV 中，活动 Load／新 session 不清零。只有已确认的等待和步态 caller 显式推进；render／present 不隐式更新时间。原版使用 BIOS 低16位无符号差，差至少2才前进一步，16回绕且不追赶；宿主55ms tick是近似。当前接普通战场等待、总览稳定等待、共享路径步态和B4奖励／状态的按键及自动等待；剧情／其余通用对话尚未全接，不能称全部caller已完成。

F1/Page Up 由 play owner 同步呈现全场缩放图，借用已解码 terrain，不替换 session cache。开7帧、关6帧；中间只更新阵营／焦点标记，不推进 game tick、RNG 或水色。关闭后正常重绘前显式恢复一次水色更新，长停留不追赶。普通键只关闭页面，不改焦点或继续处理当前帧动作；保留 held／repeat，宿主退出优先。这是 SDL 当前帧适配，不宣称复制 BIOS FIFO 或原版末尾额外清队。

继续保留已有入口门：系统菜单只在玩家 phase、browse 且焦点无可见 actor 时打开；secondary Save 在任一未隐藏 actor 带 acted `0x80` 时禁用，hidden actor 的 stale acted 不触发该门。Load 要求完整有效且当前支持的快照，不因磁盘上有同名文件就启用。不能把活动快照的限制直接套到四槽 dispatcher。

宿主默认从工作目录 `FD2.SAV` 读写，可由 `FD2SDL_SAVE_PATH` 覆盖。测试／人工对照应使用可写副本，不把该变量指向 `original_game/` 中的原始证据。长度、checksum、四槽边界及 slot 3 尾部与 checksum 重叠的规则继续由 `save.c` 和格式文档维护。

### 恢复约束

- 先验证完整容器、checksum、stage 支持性、单位数、坐标、镜头及记录语义；失败时当前 session 和原文件不变。
- 活动快照的关卡 metadata 有独立 owner：保存本关有效前缀，保留未使用尾；加载先检查 shape 与当前资源相符，并按本关已知模板数准备临时 metadata，全部预检成功后交接。不能仅更新 stage 数字却沿用旧关卡数据头。
- 同关卡恢复复用现有事务导入。跨关卡恢复先准备独立候选 session，验证成功后再切换资源、调色板、音频及 owner；不能因资源准备失败留下半个新关卡。
- 活动快照没有独立的 active-side 或 RNG checkpoint；原版恢复回玩家控制器边界，SDL 应清理 phase cursor、路径、菜单、演出和计时，按已确认入口处理随机流。不新增未知字段来追求读档后 RNG 序列相同。
- 成功活动 Load 把焦点轮询起点重置为0，失败保持旧 owner；该值不从 SAV 读取。普通可见 actor 必须在当前 FDICON bank 中容纳完整12帧组，不能只验证当前idle帧，也不能套用F1的unit121／race10参与者过滤。hidden未知ID继续保留；这不等于原版畸形hidden记录也安全。
- 成功恢复清奖励／状态续态，但保留不属于SAV的全局经验贡献；失败保持原owner。奖励、状态、phase或详情忙态拒绝保存，不能把中间写点导出成正常边界。
- 恢复不能把存档单位再次经过新建构造器重置 HP/MP、装备或等级，也不能再次播放已完成的增援事件。未知 meta、未使用单位尾部及未拥有的 cell-state 字节继续保留。
- `FD2.TMP` cache class 不是职业。SDL 用稳定 unit ID／FDICON 重建运行期缓存；磁盘 TMP 互操作是否属于发布交付另行确认。
- 写回继续使用同目录独占临时文件与受检替换；失败不破坏原档。临时文件唯一不等于多进程更新锁，目前不宣称多个游戏进程并发写档安全。

恢复成功后应用音频 bus 配置，并由 A5 已接入的场景编排请求对应曲目。活动 Load 不重播开场；准备状态 Load 回到酒店／城镇，出阵完成后才请求战场 primary。曲目请求、sequence 生命周期和声音保真分别验收，不能把音量恢复或曲目请求成功当作原版音色／时序一致。

## 6. 演出与音频边界

已确认逻辑结果只提交一次。FIGANI 接入时复用现有确定性结算，演出用提交前快照和已确定的逐击结果展示过程；从原版动画内分离出的 effect 调用必须检查，不能又结算一次。跳过动画、完整播放、静音和 null device 不得改变最终单位、phase 或 RNG 次数。

B6 的 Game 仍是唯一业务与随机数 owner。物理动作分 PREPARE、逐击 STRIKE 和一次 FINISH；预检先于费用／随机数，呈现失败后不重复已提交的攻击。FULL 按两项独立条件最多三击，SHORT 保留原 OR 条件最多两击；配置切换模式与“跳过当前 FULL 呈现”不同。后者不改变 Game 使用的 FULL 规则或随机数调用。

FULL 法术0..9由 Game 按原 phase0..8 推进同一个 `magic_fx` 状态，并记录每段 FX 随机数。效果、贡献、HP 插值与抖动随机数按原先后交错；呈现端只读重放该段 FX 记录，不能再调用业务 RNG 或 effect。HP 插值达到分母上限之后，后续有效 impact 仍消耗抖动随机数。效果计算得到的 HP 与最终可见 HP 分别记录：原8的后续目标可有贡献却没有 impact，不能强行补写效果 HP。原9多目标会越过现有 FX44 的合法 part，预检拒绝；非 side0 的原9 selector 未确认，不猜相邻值。玩家开放门仍限既有0／13及其已确认演员，通用演出能力不自动开放其余玩家法术。

Game 持有 FIGANI 原始归档；演出服务借用该字段地址，活动动作期间禁止 Load／close。物理／FULL 才按需读取 BG／TAI，地图13不要求无关背景。主、次 PCM 播放器均由 App 持有，动态样本借用 App 的 FDOTHER，直到 voice retire 后才释放。音效开关只抑制 cue／stop，不跳过资源检查；Game 不依赖音频设备是否存在。

Play 结束按“解绑两个呈现回调、关闭服务、关闭 Game”释放；错误锁存后走真实 ERROR，宿主退出走 HOST_QUIT。普通结束用只读地图返回绘制重建当前场景，退出／错误恢复动作前 VGA 快照。物理返回页保留普通光标；法术地图特效及正常返回页按1cff0／15311的51a83=0隐藏光标，仍绘当前角色、前景与信息板，不带范围或菜单。11cac的参数控制水面更新，不能当作光标开关。地图阶段投影读取同一时钟锚点，不调用 Game tick、AI 或 RNG；水色仍只在已确认 caller 更新，不能用每个 present 暗中推进。

| 情况 | 处理原则 |
|---|---|
| 动作尚未确认或预检失败 | 拒绝本次动作，保留来源页和原状态 |
| Save/Load 文件无效或写入失败 | 报告本次存储失败，保留原文件和可用 session；不冒充成功 |
| 必需玩法资源／handler 损坏或不支持 | 明确失败；不静默跳帧、改成待机或伪造成功 |
| 设备不可用 | 使用已有 null/discard 音频降级，保留逻辑；不把设备失败等同于资源合法 |
| 开发版本主动关闭未实现的纯演出 | 只允许在明确支持的无演出模式下执行已确认逻辑；不能将坏数据当作该模式 |
| 逻辑已提交后呈现失败／宿主关闭 | 返回真实错误／退出结果，不自动重试或伪造 RNG 回滚 |

BGM 仍由应用层持有。A5 已连接 stage 0／1 的 primary/alternate、城镇／酒店、Load 和离场请求；原版 phase 边界决定切曲，渲染与音频回调不参与规则。配置静音切换不应推进 RNG 或重新初始化单位。B5／A5编排验收及其后的统一历史回归已完成，实际来源见阶段记录；音色、混音、精确时序和完整演出不由这些请求接入结果证明。

音频 source 借用的 bank／数据必须活到 retire；换关卡、关闭和 Load 不得提前释放。沿用现有有界命令队列、source retirement 和销毁顺序，不再建立第二个临时播放器。具体 A5/A6 见 [`../plans/audio.md`](../plans/audio.md)。

## 7. 验证要求

- 同一状态与注入 RNG 序列下，纯查询／提交前拒绝逐字段不变；取消检查业务状态及各级返回路径，真实UI等待按已确认调用点断言RNG消费，不能要求退款。成功动作只提交一次，并断言RNG消费次数。
- 故障注入区分提交前与提交后，验证没有自动重放、额外 phase、双重释放或原档损坏。
- 每种正式命令、存取与退出原因均通过真实 SDL 输入到达；快速入口与直接 session API 测试分别报告，不互相替代。
- 新显示场景保留 DOSBox 截图／操作／时序证据，可按[图形自动化测试](../tools/gui-testing.md)在独立副本上自动获取；没有确认判据的自动运行不记为通过。Windows 原生窗口输入与 Linux Xvfb/XTest 的执行结果分别登记。现有菜单机器码差分只验证坐标与帧选择，RLE 差分只验证指定解码分支，范围见 [`../reverse-engineering/ida-bytecheck-9.5.md`](../reverse-engineering/ida-bytecheck-9.5.md)。
- 静态 LE／IDA 已通过；严格运行时 BSS 仍需新转储，不把它重新包装成一般玩法开发的前置，也不宣称该验收已通过。
