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

列表状态已接入：MAGIC_LIST保存临时索引，step不负责关闭，取消关闭后才回COMMAND。原版18890对非零路径禁法，只有unit ID18／19／34例外；此门与空法术、record+0x27禁止状态共同检查。低MP只阻止当前项确认。正式入口限stage0、全列表为0（caster unit9已有费用标记证据）／13，并要求共享RNG可用；未知列表项不开放，不从AI支持表推断玩家全能力。

物品B3的已确认返回合同独立于MAGIC：Use列表取消使1bbdc返回0，18d8c重开ITEM子菜单；子菜单取消使1bbdc返回-1、18d8c返回0，再由18890重开COMMAND。Use目标取消先聚焦caster，再重开1b932列表并归0选项，保留临时移动及pending。53C53仅由Give完成分支置1，不能把它当作所有库存改变标志；装备／丢弃已提交变化也不能因为该值0而回滚。

ITEM_MENU／ITEM_LIST已接正式输入。子菜单复用四向command呈现状态，但语义为Use／Give／Equip／Drop；当前SDL仅Use可选，其他三项是实现能力禁用，区别于原版真实START只禁用Give。列表索引是独立临时raw槽，step只报告可确认，不自行执行效果；列表完整关闭后才进入目标层。列表取消回ITEM_MENU0，子菜单取消回COMMAND首可用项，两层均先于COMMAND原移动回滚分派。成功Load清索引并回浏览，失败保留所有旧owner，不序列化新UI字段。

ITEM_TARGETING仅支持草药192的player cast1／area0／filter1，复用现move_range和互斥target候选buffer，不缓存第二份slot／物品真值。每次按当前raw槽重新查192，候选按raw顺序只过滤hidden和side0，不额外过滤HP0或acted；121占轮询槽但不移动焦点。法术与物品共享范围建立和轮换，保留各自record／列表／能力门。目标取消先完整聚焦使用者，再清目标owner、返回ITEM_LIST0，保留移动路径／origin、单位、pending和RNG。方向可以移出cast范围，合法性另查；目标层不自行提交动作，正式confirm_cursor仅在能力门与合法目标都通过后调用提交API。

草药提交按当前raw槽／record、重建范围、同格raw目标、完整可见坐标／HP数据及必经尾链预检后，才逐目标调用power40 HP helper并消费一次RNG，再移除所选槽一次、处理败亡、acted、最终lookup1及controller。不扣MP、不写unit EX、不做WAIT恢复或物理方向复位。可能恢复的同格HP0目标不会误因drop描述符拒绝；预测仍HP0的非目标未知loot先拒绝，phase预测保留controller原无HP门。提交后失败保留HP／压槽／已到达的acted与外部RNG，首错冻结，不退款、不重试。

正式ITEM能力查询无写入／RNG，仅接受stage0可行动玩家及连续已知库存，要求非空共享RNG；非零Use code只允许192，code0普通装备只读。COMMAND、ITEM_MENU及目标确认都重新检查全库存；LIST由私有modal独占确认键，不能通过confirm_cursor跳过关闭过程。查询不借用MAGIC的MP、移动、unit+0x27或升级门。宿主退出与逐级取消先于正式Use能力检查处理；能力失效本身不阻止原有取消，也不清除尚未提交的移动。损坏的actor／record仍可明确报错并退出。

MAGIC_TARGETING复用cast范围而独立持有法术及raw候选索引；0／13仅area0，方向可离开范围，确认时重建当前范围与全部同格filter目标。完整效果／贡献上界、无掉落支持门、非升级EX与必经事件预检先于费用／RNG；miss仍扣费并行动，多目标只扣一次。收尾独立于WAIT恢复及物理方向复位，按效果／败亡、acted、归一、最终lookup1、EX、controller顺序执行。预测phase必须考虑本次死亡清理后仍可行动的玩家，不能给13565本体添加HP门。FIGANI、488奖励提示与附加呈现RNG仍按B4/B6验收。

必须先明确：玩家 MP 扣除时点、施法失败／无目标的处理、目标阵营、物品使用与库存移除的关系。AI `field_ai_magic_action_execute @0x15311` 已有的 MP 可用性门不证明玩家扣 MP 规则；已确认 AI item code `13/20/21/24` 不消耗，也不证明玩家同名物品一律不消耗。玩家证据入口为 `field_player_magic_command_execute @0x1cff0` 与 `field_player_item_command_execute @0x1bbdc`，登记见 [`../reverse-engineering/function-names.md`](../reverse-engineering/function-names.md)。

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

当前controller恢复stage0结果门、精确13565门、必要整轮phase后的pending分派与清空；B4经验尚未扩展。13565只看flags&0x81、side2和raw26，不加HP或bit04门。异步phase期间仅保存原actor与等待标志，不保存pending副本；AI仍可清空／覆盖同一slot，raw尾必须扫描完才换phase。side1尾先全表清acted，再进入phase0事件／状态，删除旧延迟清除补偿。Enter的普通不可操作actor详情关闭后走同一后置，F2直接返回。

controller完成时由play owner耗尽当前帧普通键，保留held／repeat；core不接input。等待phase时拒绝导出／保存，成功活动Load清除续行与pending，失败恢复保留完整原状态。此处恢复的是已确认控制顺序；未实现的cell handler、特殊格、物品已交接取消例外及跨地图镜头仍单独登记。

批量行军按 `field_empty_focus_menu_execute @0x16f55` 的独立顺序提交：确认后捕获固定绝对焦点，按 raw actor 顺序仅处理 `(flags & 0x85)==0 && side==2`，不附加 HP 门。每人先 focus、清共享 pending、执行完整路径的 match0 收集，再分派当前 pending；成功后才重置全表方向、标记当前 acted。无路仍走尾部，不查询最终 match1。当前 handler 失败保留已走路径和先前 actor 的提交，立即停止，不伪造整批回滚或继续 phase。

整批后只启动一次 side1→side0→新玩家回合，不自动 Save。March 期间由既有私有路径呈现参数关闭选择／信息层；play 局部续行把相同参数传给异步 AI visual step，普通 END 默认使用原呈现。恢复玩家阶段或终态后清当前帧普通键，保留 held／repeat，不把显示策略或 pending 副本存入 session。长路径中途的即时宿主退出仍沿现有路径协议，未声明逐动画相位响应通过。

攻击、法术或物品进入效果提交后发生的错误保留当前 HP、库存和外部 RNG 进度，锁住 session；behavior 11 已施法而后续物理动作失败时也停止，不再按成功执行 common tail。

成功演出后由呈现 owner 清除 `presentation_deferred`，同一 notice 不在下一帧重放。停止状态禁止保存，以免把部分提交的状态写成正常可恢复快照；显式加载有效存档可以恢复并清除错误，加载校验失败则保留完整停止状态和原文件。

## 4. 战场结果与应用去向（B1 已接入，B5 待实施）

`battle_result` 的 stage 0 胜／败通过 `fd2_field_play_result` 与 `app_flow` 分别传给应用层。正式循环先处理 session 错误和宿主退出，再推进下一 actor；快速检查不触发真实胜败结算。

`COMPLETE` 仅表示无交互快速检查结束。当前胜利结算和失败后续尚未实现，`main.c` 分别报告原因并退出 `2`；游戏内离场、宿主关闭／快速检查退出 `0`，错误退出 `1`。离场使用独立 `LEAVE_GAME` 来源，不能改作重新进入标题。

| 结果 | 应用职责 |
|---|---|
| 仍在战斗 | 继续当前 session，不离场 |
| 胜利 | 只进入本关已确认的结算／剧情出口；未实现下一步时明确报告支持边界，不伪造下一关 |
| 失败 | 进入已确认失败流程；不能按胜利结算、递增 stage 或吞掉失败 |
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
| 入口 | 标题 CONTINUE／secondary Save/Load | 原版标题 LOAD／`field_manual_slot_dispatch @0x29300`；SDL 可见手工入口未接 |
| 保存数据 | 本关 metadata、独立持久 roster、活动单位、cell state、stage/turn、镜头／焦点、配置及保留区 | 持久 roster、已确认 meta、配置及未知尾部 |
| 当前可恢复关卡 | stage 0 | codec/picker 与保存路径已有；session Load 在真实关卡重建契约完成前明确拒绝 |

runtime `FD2_FIELD_MAX_UNITS` 当前为 64。**64／96／32 是三种不同边界，不能统一为一个猜测值，更不能保存时截断。** 跨关卡前必须确认实际 roster 与对应原版存档路径的关系。

活动 meta[1] 与 meta[9] 分别属于当前 actor 和持久 roster，不能互相约束或由当前 side 2 人数重算。持久记录按 text ID 表达跨关身份，新游戏按原版构造顺序建立；活动部署再设置位置、运行期 cache 和临时状态。未知字节在加载／保存往返中保留，不能从活动记录盲拷来构造新持久角色。

session 持有完整 `0xa00` raw roster，包括未用尾；活动与手工保存均写出这整张表。加载 A 后保存到 B 或新文件时，roster 尾来自 A，metadata 未用尾仍由目标容器保留。这两个保留规则不能混为一条。

水色 DAC 状态属于应用的 VGA 视觉 owner：`field_water_last_tick/field_water_phase` 不在 session 或 SAV 中，活动 Load／新 session 不清零。只有已确认的等待和步态 caller 显式推进；render／present 不隐式更新时间。原版使用 BIOS 低16位无符号差，差至少2才前进一步，16回绕且不追赶；宿主55ms tick是近似。当前接普通战场等待、总览稳定等待和共享路径步态，剧情／通用对话／独立 timed wait 尚未全接，不能称全部 caller 已完成。

F1/Page Up 由 play owner 同步呈现全场缩放图，借用已解码 terrain，不替换 session cache。开7帧、关6帧；中间只更新阵营／焦点标记，不推进 game tick、RNG 或水色。关闭后正常重绘前显式恢复一次水色更新，长停留不追赶。普通键只关闭页面，不改焦点或继续处理当前帧动作；保留 held／repeat，宿主退出优先。这是 SDL 当前帧适配，不宣称复制 BIOS FIFO 或原版末尾额外清队。

继续保留已有入口门：系统菜单只在玩家 phase、browse 且焦点无可见 actor 时打开；secondary Save 在任一未隐藏 actor 带 acted `0x80` 时禁用，hidden actor 的 stale acted 不触发该门。Load 要求完整有效且当前支持的快照，不因磁盘上有同名文件就启用。不能把活动快照的限制直接套到四槽 dispatcher。

宿主默认从工作目录 `FD2.SAV` 读写，可由 `FD2SDL_SAVE_PATH` 覆盖。测试／人工对照应使用可写副本，不把该变量指向 `original_game/` 中的原始证据。长度、checksum、四槽边界及 slot 3 尾部与 checksum 重叠的规则继续由 `save.c` 和格式文档维护。

### 恢复约束

- 先验证完整容器、checksum、stage 支持性、单位数、坐标、镜头及记录语义；失败时当前 session 和原文件不变。
- 活动快照的关卡 metadata 有独立 owner：保存本关有效前缀，保留未使用尾；加载先检查 shape 与当前资源相符，并按本关已知模板数准备临时 metadata，全部预检成功后交接。不能仅更新 stage 数字却沿用旧关卡数据头。
- 同关卡恢复复用现有事务导入。跨关卡恢复先准备独立候选 session，验证成功后再切换资源、调色板、音频及 owner；不能因资源准备失败留下半个新关卡。
- 活动快照没有独立的 active-side 或 RNG checkpoint；原版恢复回玩家控制器边界，SDL 应清理 phase cursor、路径、菜单、演出和计时，按已确认入口处理随机流。不新增未知字段来追求读档后 RNG 序列相同。
- 成功活动 Load 把焦点轮询起点重置为0，失败保持旧 owner；该值不从 SAV 读取。普通可见 actor 必须在当前 FDICON bank 中容纳完整12帧组，不能只验证当前idle帧，也不能套用F1的unit121／race10参与者过滤。hidden未知ID继续保留；这不等于原版畸形hidden记录也安全。
- 恢复不能把存档单位再次经过新建构造器重置 HP/MP、装备或等级，也不能再次播放已完成的增援事件。未知 meta、未使用单位尾部及未拥有的 cell-state 字节继续保留。
- `FD2.TMP` cache class 不是职业。SDL 用稳定 unit ID／FDICON 重建运行期缓存；磁盘 TMP 互操作是否属于发布交付另行确认。
- 写回继续使用同目录独占临时文件与受检替换；失败不破坏原档。临时文件唯一不等于多进程更新锁，目前不宣称多个游戏进程并发写档安全。

恢复成功后音频 bus 配置已可应用；具体场景曲目与 sequence 生命周期由 A5 补齐，不能把音量恢复等同于曲目恢复。

## 6. 演出与音频边界

已确认逻辑结果只提交一次。FIGANI 接入时复用现有确定性结算，演出用提交前快照和已确定的逐击结果展示过程；从原版动画内分离出的 effect 调用必须检查，不能又结算一次。跳过动画、完整播放、静音和 null device 不得改变最终单位、phase 或 RNG 次数。

| 情况 | 处理原则 |
|---|---|
| 动作尚未确认或预检失败 | 拒绝本次动作，保留来源页和原状态 |
| Save/Load 文件无效或写入失败 | 报告本次存储失败，保留原文件和可用 session；不冒充成功 |
| 必需玩法资源／handler 损坏或不支持 | 明确失败；不静默跳帧、改成待机或伪造成功 |
| 设备不可用 | 使用已有 null/discard 音频降级，保留逻辑；不把设备失败等同于资源合法 |
| 开发版本主动关闭未实现的纯演出 | 只允许在明确支持的无演出模式下执行已确认逻辑；不能将坏数据当作该模式 |
| 逻辑已提交后呈现失败／宿主关闭 | 返回真实错误／退出结果，不自动重试或伪造 RNG 回滚 |

BGM 仍由应用层持有，只把必要的曲目请求或现有播放器引用连接到场景编排。原版 phase 边界决定 primary/alternate，渲染与音频回调不参与规则。配置静音切换不应推进 RNG 或重新初始化单位。

音频 source 借用的 bank／数据必须活到 retire；换关卡、关闭和 Load 不得提前释放。沿用现有有界命令队列、source retirement 和销毁顺序，不再建立第二个临时播放器。具体 A5/A6 见 [`../plans/audio.md`](../plans/audio.md)。

## 7. 验证要求

- 同一状态与注入 RNG 序列下，纯查询／提交前拒绝逐字段不变；取消检查业务状态及各级返回路径，真实UI等待按已确认调用点断言RNG消费，不能要求退款。成功动作只提交一次，并断言RNG消费次数。
- 故障注入区分提交前与提交后，验证没有自动重放、额外 phase、双重释放或原档损坏。
- 每种正式命令、存取与退出原因均通过真实 SDL 输入到达；快速入口与直接 session API 测试分别报告，不互相替代。
- 新显示场景保留 DOSBox 截图／操作／时序证据，可按[图形自动化测试](../tools/gui-testing.md)在独立副本上自动获取；没有确认判据的自动运行不记为通过。Windows 原生窗口输入与 Linux Xvfb/XTest 的执行结果分别登记。现有菜单机器码差分只验证坐标与帧选择，RLE 差分只验证指定解码分支，范围见 [`../reverse-engineering/ida-bytecheck-9.5.md`](../reverse-engineering/ida-bytecheck-9.5.md)。
- 静态 LE／IDA 已通过；严格运行时 BSS 仍需新转储，不把它重新包装成一般玩法开发的前置，也不宣称该验收已通过。
