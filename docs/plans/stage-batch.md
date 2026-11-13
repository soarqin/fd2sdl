# 关卡数据批量读取与流程转换调研

2026-11-12，代码基线为本地 master `4d7ed15`。本文给出 S 工作包的来源、转换边界与验收设计；**唯一优先级仍在[总计划 §4](development.md#4-新优先级与依赖)**。本轮只做只读研究、最小候选转换和计划修订，没有开放新关卡，也没有启动后续玩法实施。

2026-11-14 状态定位：本文保留上轮研究事实与复跑记录，不是当前阶段状态表。S0 结果／恢复补充和 S1 全量账／审计同步已分别交付；当前转换输入和复跑合同见[来源账规范](../formats/stage-source-ledger.md)。S2 转换和有限能力已交付，来源／冻结域及验收见[S2 报告](s2-shared-semantics.md)，S3正式批量接入已交付，实际范围与新证据见[S3报告](s3-batch-integration.md)；S4尚未开始。下一步只查[当前计划](development.md)，完成范围见[归档](completed-stages.md)。下文旧计数和 HP blocker 均保留研究时点身份，不作为当前缺陷。

## 1. 推荐与基线

推荐扩展现有 `audit_stage_data.py`、`audit_common_scope.py` 和冷定义生成器，以「全量来源账 → 按共享语义转换 → 按依赖闭包接入 → 统一验收」组织后续工作。地图和原资源表继续直接读取；EXE 表批量提取引用；已确认的控制流转换为现有有限 flow。开发和验证的主要单位是共享语义族，不是关卡编号或事件引用次数。

本轮新隔离工作树最初位于 `88b47d7`，核实主工作区 master 为 `4d7ed15` 后使用 fast-forward 更新。G1 `5b9619f`、G2 `61d5856`、G3 `08f61f6`、G4 `4ada76b`、G5 `4d7ed15` 及 SD1—SD3 的历史门保持原范围，不重开旧阶段。已完成的通用能力、正式消费者及未验边界见[审计 §13](common-logic-audit.md#13-g5-通用完成门与最终验收2026-11-11)。本轮复用已发布 IDA 证据和严格 LE 读取器，不改数据库、函数命名或生成伪 C。

能批量读取不等于能直接运行。现有审计器的 `semantic_complete=false`、`original_runtime_executed=false`、`new_gameplay_enabled=false` 均保留。stage 0／1 的 `bound_definitions_closed=true` 仅指审计器排除默认候选后的已绑定集合；严格 `--require-supported` 集合仍为空。stage 2 仍只支持既有入场／浏览／存取，不开放战斗与 END；31／32 的开场链由 `scene.new_game` 消费，不属于仅 30 槽的战场 entry 表。

## 2. 本轮全量来源盘点

原资源只读使用主项目 `E:/Projects/fd2sdl/original_game/`。固定身份由现有工具验证，完整原字节、全量 JSON 及本地报告不进入分发。

| 来源 | SHA-256 |
|---|---|
| `FD2.EXE` | `bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd` |
| `FDFIELD.DAT` | `81310211c6e96ebd89e79648650696691c740229fa4095ece37e7226bc27b74d` |
| `FDSHAP.DAT` | `901b70ea82d5d977192759fad510921ffe16a0ab6af6ab7c32757de03e30aa3c` |
| `FDTXT.DAT` | `b47e39ee727482528da061a8705f5cdee8a9cf759447aeda298afe17d9dd8bcc` |

### 2.1 原资源读取链

| 数据 | 真实来源与读取链 | 本轮可确认范围／后续工作 |
|---|---|---|
| 地图／地形 | `archive.c` → `field.c::fd2_field_map_open_stage` 读 `FDFIELD[3*stage]`；`tile.c`／`shape.c` 读 metadata 指定的 FDSHAP 属性和图帧 | 99 个有效资源项组成 33 组 `0..32`，均可读；地图格、shape 引用和当前解码的格事件可批量检查。图帧视觉、所有未知控制流未来改图不由结构审计证明 |
| metadata／触发 | `FDFIELD[3*stage+1]`；`field.c` 解 `0x03` turn、`0x33` lookup、`0x53` descriptor | turn／lookup／action 各 528 槽，连默认字节一起保留；不是 528 个不同机制。运行时仍读所属 Game 的 metadata，活动 Load 使用 SAV 有效运行值 |
| 单位模板／编成 | 同一 metadata `0x83` 后每条 26 B；`field_unit_base.c`／`field_unit.c` 构造，group 在模板 `+0x15` | 原始 1,899 条；metadata 有效前缀实际消费 1,887 条；前缀外 12 条原样保留，不追加为单位、不猜补位置 |
| placement／部署 | `FDFIELD[3*stage+2]` 的计数及 6 B 记录；模板按 `template_index` 取同索引位置；metadata byte2为有效模板数，byte1为玩家部署槽数，玩家第ordinal槽取 `placements[有效模板数+ordinal]` | 2,322 条位置记录，有效模板缺位置为 0；位置数不等于出场人数。模板 `+0x16` 是drop mode，不能误作位置索引。出场时机、持久队伍选择、slot 映射来自 entry／callback，而非从坐标猜测 |
| 格动作／掉落 | 地图 byte2、FDSHAP flags → lookup 或 action／completed；单位 raw `+0x31..33` descriptor → `loot` | 可全量读原槽和地图引用。普通 lookup 是低 5 位 1-based；特殊格用低 5 位 0-based，不能合并。mode／完整 WORD param 按真实消费者解释，未知 callback 保持阻塞 |
| 移动流 | EXE object3 `0x627d8` 的 106 指针槽 → `0x62980..0x634d2` payload → `movement_scripts.json` → `field_event.c` getter／执行器 | 106 条严格语法已解码，消费总长 2,896 B，候选 payload 2,898 B；ID24 跨下一目标、ID84 尾 4 B 保留。raw actor 最大可达83、direction5 不归一化；仍需按当次实际 actor 数校验 |
| 文本／资源引用 | `text.c` 读 FDTXT entry／fragment／动态 token；map sprite、FIGANI、FDOTHER、FDMUS 各走现有读取器 | 不复制对白、地图、图像或声音为另一套配置。当前 audit 校验已解码 flow 的片段／group／移动等引用；尚未恢复的 CFG 中的资源引用没有因此完成全量覆盖 |
| 音乐／准备数据 | `tables/stage.json` 引用 EXE `0x6238d` 的 26×31 B 准备记录、`0x51e63/0x51e81` 两张30 B曲目表；`field.c` getter | 已有表和正式消费，无需重新手抄。准备服务／队伍能力属已完成 G3，具体城市、商品、quota、强制成员和交接参数按新绑定核验 |

原生资源是唯一初值来源。规范化记录只带来源定位、引用和审计结果，不成为 App 读取地图／模板的第二入口。活动地图、库存、换箱后的 param、完成位、RNG 和原 SAV 布局继续属于现有热 owner。

本轮地图共43,734格，其中622格带事件标记；实际引用189个action槽（mode0为158、mode1为31）。FDSHAP完整目录66项，即33帧包／33属性表，共9,366属性记录；FDTXT目录34项、1,016片段，其中entry0共享661、各stage entry合355。本轮核的是目录／属性及已有解码引用，没有重跑全量图帧RLE，也未证明全部token／glyph／portrait合法。metadata turn非默认78条，另450默认槽；非255 lookup13条，turn＋lookup合91次callback引用、62个不同目标，不能与所有drop／格动作的保守需求引用相加冒充机制数。

### 2.2 EXE 分发表与事件读取上限

地址统一为 canonical relbase VA，指针经严格 LE fixup 解释；object1 offset 为 `VA-0x10000`。本轮重跑得到：

| 表 | VA | 槽数 | 不同目标数 |
|---|---|---:|---:|
| runtime result | `0x51b19` | 30 | 18 |
| event callback | `0x51b91` | 90 | 90 |
| stage entry | `0x51d71` | 30 | 28 |
| stage victory | `0x51de9` | 30 | 30 |
| 主要四表合计 | — | 180 | 166 |
| 独立 magic 表 | `0x51d01` | 28 | 27 |
| 五表合计 | — | 208 | 193 |

callback 后 `0x51cf9/0x51cfd` 是两个无 fixup、值为18的 DWORD，不能算 action90..119。表内 callback 目标各不相同，但不同关可引用同一 action，目标还可能调用共享尾。193 是表内不同入口目标数，不是193种语义，也不是193个完整待开发功能。IDA chunk owner／摘要可以批量确认来源和边界，不能单凭函数边界、附近 CALL 或伪 C 猜测 ABI 自动恢复控制流。

现有 `common_event.json` 有 54 个操作、21 图、183 节点：15 份未绑定来源机制图与6个旧正式绑定分账。后者是 stage000 action0..4、stage001 action6；G5 已按原等待点逐叶推进，不再整段先提交再 Scene 重放。stage／scene／loot／runtime 的已有图和消费契约继续复用。

### 2.3 统计口径与审计器滞后

`full.json` 的保守需求共 1,171 条记录引用：51 `supported_definition`、468 `decoded_unopened`、652 `unmapped_semantics`。652 中包含450条默认 `(255,255,0)` 潜在命中。它们是当前工具按其规则给出的**引用分类**，不是当前通用机制缺口、独立事件数或覆盖百分比。

`audit_common_scope.py` 的 1,497 来源行分为：208 dispatch、1,171引用、106 movement、12模板尾，归属20个语义族，未归属为0。历史标签622 `binding_only`、388 `common_gap`、450 `out_of_table`、1 `default_noop`、36 `existing_supported` 仍指 G1 分类时点，不能把388项重新列为 G5 未完成。

现有 `audit_stage_data.descriptor()` 仍保守地分类 mode0..3、mode2 低 byte 等 SD 读集；正式 G5 `190ac` 消费已按完整 WORD 和原 default 分派闭合。S1 必须同步审计表达和已证消费语义，同时保留旧字段／旧报告身份与差异账，不能用改标签提高“完成率”。默认255没有合法code fixup，DWORD turn无255上限，不能删槽或擅自增加 callback255。严格门失败是已知边界；0／1已开放范围的历史结论不变。

例如stage14有效template33的drop mode203、param0有合法同索引placement33，但现audit归为未映射。它应通过G1／G2已证default无业务动作的caller合同重新分类，不应猜成坏资源或新增通用机制。`read_dispatch.semantic_decoded=false` 同样只是该结构报告未判语义，不能反推G5已确认的函数全部未知。

本轮引用错误0只覆盖工具已解码并检查的引用，不包括未知函数内部的全部资源／写点。`classification_complete=true` 只证明来源有归属，不证明所有 CFG 已恢复、所有机制已实例化或全游戏可运行。

## 3. 可以批量转换什么

| 类别 | 转换方式与可信度 | 明确停止条件 |
|---|---|---|
| 资源记录／规则表 | 严格目录、布局、count、固定身份、源跨度校验后全量生成引用索引／夹具；可信度为原字节确定 | 未知字节保留，超界／缺源即失败；不推断玩法，不复制 DAT 为生产真值 |
| fixup 绑定／同目标别名 | 原槽 → canonical target／owner／摘要 → 经确认的同义 flow 候选；表引用可信，语义复用须有完整来源证明 | 相同 target 但 caller、参数域或读写上下文不同，仍需分路径校验；候选不自动变 shipping binding |
| 已确认 CFG／有界动作序列 | 在 Domain API 已有函数／chunk／xref 证据上识别有限语义模板，保留条件、宽度、出口、共享尾和写序，降为现有领域节点 | 只能消费已证参数域；相似 CALL 序列不足以匹配。切片不得冒充完整 callback，分支不得压平为线性数组 |
| 参数化共用段 | 已证有限 getter／原语和模板复用，如 turn BYTE／DWORD、state16、child loot；绑定层只提供来源参数 | 未知表达式、无界循环、未解间接跳转、动态身份／map替换后的引用未闭合、特殊全局写点／RNG时点未确认均进入异常队列 |
| 真正私有事件流程 | 完整恢复私有条件与编排后仍写入冷 flow，算法仅补最小领域操作；按语义族集中闭合 | 不用整段 `native_stage_handler` 藏控制流，不建通用脚本 VM，不以空事件、最近位置、默认全灭或 stage++ 代替原义 |

全自动化上限是严格记录和已确认语义模板的机械投影。当前没有可信的“EXE任意函数到可运行关卡”转换器。应拒绝自动处理无法证明的间接控制流和写点，而不是继续扩大模板猜测。`analyze_fdfield_events.py`、`analyze_fd2_stage_code.py` 的历史启发式 dump／附近CALL 仅作线索，不作转换真值。

## 4. 真实源样例与现有消费目标

### 4.1 跨关重复结果目标：11槽只恢复一次语义

result表中 stage `0,2,3,4,5,6,7,8,10,13,23` 的11槽均到 `0x205b4`。例如 stage2 槽 `0x51b21` 原 DWORD 为 `b4050100`，经 object1 fixup `+0x105b4` 到 `0x205b4`。完整 `0x205b4..0x205da` 先置原result2，可见side0使其归0，最后raw0 hidden覆盖为1；不查HP。原结果1／2需映射SDL败北／胜利枚举，不能直接复制整数或用常识上的“HP全灭”替换。

研究原型输出的规范化候选形态如下，完整摘要随本地输出保存：

```json
{"runtime_id":"stage.result.002","definition":"stage.result.000","status":"candidate","semantic_conversion_approved":false,"source":{"original_slot":2,"source_canonical_va":"0x51b21","target_va":"0x205b4"},"shipping_definition_changed":false}
```

这11个引用可共用一次原函数恢复：1个已有定义引用、10个新候选，涉及1个结果来源族。**目前仍不能把11个候选直接发布成原函数等价别名**：`stage.result.000` 的 `result_leader_dead [0,1]`／`result_side_present [0,1]` 含SDL既有HP防护，原函数只看hidden／side；缺raw0的防御域也不同。上轮将它列为S2前置；本轮发现既有snapshot导入接受HP0且visible并直接按HP判胜败，该旧stage0／1合同现已在S0修正，正式图禁用HP门并由真实controller后置锁存；原研究的`[0,1]`只保历史基线。S2复用S0已修正的合同，再证明新caller／别名有效域与结果码映射；在此之前提案保持 `semantic_conversion_approved=false`。同target去重的可行性确定，现图全状态等价未证明。

后续生产绑定经审核后由 `generate.py` 写入既有只读定义，`field_game` 的结果查询消费。当前 stage/result 查找合同仍需 S2/S3 扩展别名索引或生成对应现有节点形式；本原型不修改生产文件，不声称已经消费新alias。其它entry／victory／callback、资源及状态轨迹未闭合时，stage2乃至其它10个场景都不能只因结果函数相同而开放。

### 4.2 有分支、ACK后写一次位：callback12

callback12 槽 `0x51bc1` 原 DWORD `ea480200`，fixup到完整86 B函数 `field_event_state16_ai7_once @0x348ea`。现有 `common.state.once` 已表达以下真实转换：

```text
state[16] == 0 ?
  yes → unit_ai_write(24,27,7) → story(3) → ACK
      → event_byte_write(16,16,1,0) → end
  no  → end
各失败边 → fail；HOST/ERROR 由现有续态 owner 停止，不执行后缀。
```

实际节点是 `branch_event_byte [16,0,0]`，yes接 `unit_ai_write [24,27,7]`，再接 `story [3]` 的 `ack`，最后才 `event_byte_write [16,16,1,0]`。AI字段按原掩码改低4位，不覆盖其它位；once来自原state门，不能给所有事件加统一once。生成后由 `field_common_event_runtime.h`／`event_resume` 执行，story叶交给 Scene 呈现，父PC等待ACK，再提交标记。这是已解码机制来源图，没有新自然关卡绑定，不能算本轮新转换了一个可玩事件。

### 4.3 具体关卡到已有流程的映射

另一个可从具体关卡落到已解码分支图的候选是stage12：metadata entry37 `+0x33` 槽0原记录 `0801`，即lookup ID1、action8、match_arg1；callback8槽 `0x51bb1` 原DWORD `23510200`，fixup到 `0x35123`。现 `common.inventory.once` 保留 `caller==raw0 → inventory_count!=8 → state16==0 → append item89 → story11/ACK → state16=1`，任一谓词false直接end。它证明真实引用可以对应现有可处理图；原型本轮未生成／提交该候选绑定，stage12仍禁用。story前已提交的物品在HOST／晚错时不回滚，完成位不能提前写。

metadata 的第三回合 action6 经 callback6 `0x34778`，现唯一绑定为 `stage.001.action.006 → common.legacy.action.006`：`pan(9,1) → delay100 → append_group(3,1) → move(13,0) → delay200 → story4/ACK → ai_anchor(raw5..10,26,15)`。移动13来自 `0x62ad1` 的原计数流，文本仍从该关 FDTXT entry2 fragment4读取，group3仍从原模板／placement选择；不手抄初始单位或对白。

此样例说明转换目标已被正式系统消费，写点在等待后继续。G5已有原码和运行证据的范围沿原记录引用；本轮只复查来源与定义，不重新宣称完成一次 SDL／原版运行。旧 SD 设计样例中的先提交／再重放是历史模型，现以 G5 的逐叶续行为准。

## 5. 可执行流水线与产物边界

| 步骤 | 扩展现有位置与输出 | 必须保持的合同 |
|---|---|---|
| 全量原始读取 | 扩展 `audit_stage_data.py`，复用严格 archive／LE／fixup；产生 ignored 本地来源账 | 源file SHA、archive entry／相对offset／长度、canonical VA、chunk摘要、原ID与raw值齐全；来源变化阻止复用旧转换 |
| 规范化中间记录 | 本轮最小研究脚本先演示 source-id／target去重／候选别名；S1扩大为全量账 | `source_id` 保持 kind、stage、slot、caller，不因去重改ID；记录引用有序，不含第二份热状态或生产地图；这是可再生审计产物 |
| 去重与语义分类 | 复用 `common-scope-review.json` 来源族；增加按target及已证CFG模板的映射 | 同target可聚合调查，但每个caller绑定仍保留；语义类和引用数量分别报告；新语义标记 `needs_primitive`，未知为 `unresolved` |
| 转为唯一冷定义 | `data/static/flows/{common_event,stage,scene,loot}.json` 及必要表；`manifest.json` 合同不另复制 | 中间记录引用flow-id，不存另一份长期可编辑图。只将已证控制流落入这些唯一输入；已有资源字段只引用。已确认参数／边的修改不要求加逐关C分支 |
| 全量能力与引用门 | 扩展 audit并复用 `generate.py` 的ABI／端口／可达性／引用校验；输出闭合候选批次及异常队列 | 逐条原始来源必须对应已转换、已证排除或阻塞之一，禁止漏项。图可生成、绑定闭合、正式可达与运行验收分开；不能用节点数作完成率 |
| 运行接入 | 既有 `field.c` 读取器、Game/event_resume、Scene、app_flow和save codec | source→binding→flow可双向追踪；只开放依赖闭包已过门的批次；未知必需项明确拒绝，不能默认落到 `stage.result.default` 当成原版支持 |

中间记录至少包括 `source_id`、source locator/hash、caller与目标、原参数位宽、ordered successors及端口、read/write owner、引用集合、证据等级、转换状态和阻塞原因。CFG记录的边必须附源分支／写点证据；结构提取记录没有这些信息时显式为空／未决，不伪造边。原始字节和完整导出留本地；提交转换脚本、最小不含资源payload的样例、审核映射及可修改的确认冷定义。

顺序与状态要求：

- turn／phase 按原16槽顺序比较；turn255默认槽仍登记可能到达。重复调用和动态 `schedule_turn` 按原BYTE写点处理，不消费原表或自动去重触发。
- pending仍为共享单槽；移动match0、行动／AI尾match1的覆盖和清除时点不改变，不改成消息队列。回调caller raw actor与unit身份分开，不能把raw下标替换为角色ID。
- 条件保留有符号DWORD、BYTE截断、完整WORD等真实宽度；参数 getter 只选现有有限来源，不引入任意表达式语言。
- ACK、取消、错误和宿主关闭均以现有PC／parent-child owner续行。child深度1沿现有loot合同；晚错保留已提交前缀，不复制Game、不回滚外部RNG、不重放业务叶。
- `common_event_preflight_identity` 目前仅保证有限线性身份投影及两边检查，逐叶晚验仍存在。新图含身份改写、追加人数或map替换再分支时，先生成各可达路径的有界引用／状态夹具；现有能力不足则作为接入依赖补最小投影，不能宣称已有任意CFG未来资源全预检。
- once、可重复、取消后完成位、交换后param、已到达的story标记均按原写序证明。冷规则可以修改，热值／临时续态及SAV owner保持现状，不向未知存档字节塞入IR、PC或RNG检查点。

## 6. 如何减少开发与验收轮次

本节保留上轮批量S1—S4的设计依据；当前增加的旧结果恢复补充S0及完整执行顺序只见[总计划](development.md#4-新优先级与依赖)，不在本页另排。工作包按共享语义族展开，例外只进入同一异常队列：`source_id／target／family／missing_semantics／blocking_bindings／evidence／next_check`。同族一次取证、补机制、生成所有适用定义和夹具，再验证整族；不因发现一个新关重复开一个阶段。

S1先让每条来源都有去向；S2按结果条件、增援／招募、相对回合、物品／特殊格、剧情／换图和entry／victory交接等来源族集中恢复CFG。已证条件／动作直接复用；真正新通用语义先以最小原语和代表来源闭合。上述族是组织方式，不能预设每族语义已完整；发现跨族共享写点时合并取证，未解间接控制流保持阻塞。S3按依赖闭包生成少量兼容批次，包含能共用机制的多个场景，不预填关卡名单或固定几关一批。不能把每关拆成一个新S阶段，也不能把闭包尚未通过的全游戏一把开放。

能力门区分已证排除、可达但按原来源边界拒绝、真正未决三类。默认turn255的旧strict报告继续非零；新批次须公开有效运行域和边界拒绝测试，不能删255让strict变绿，也不能宣称turn255原版后果已运行验证。闭包“通过”表示所声明有效域内定义／能力／引用闭合，所有域外必需路径有明确拒绝；不等于全状态无限回合原版一致。

批量夹具由源账派生，至少覆盖：全部33组读入与每个有效模板／位置关联、每个分派槽与owner、已转换图每条可达条件边、每个绑定的原参数／引用、动态身份前后actor数与map资源、恢复所拥有的未知字节。相同图／相同参数类别的绑定共用测试执行器，仍逐绑定检查来源和资源，不重复手写剧情测试。

每个语义模板保留一组最小独立oracle：从原字节／完整CFG或有限原码执行推导条件、写集、RNG调用和有序事件轨迹；不从生成C或同一转换结果生成expected。代表组合按真实源选择：条件真／假、once首次／再次、相对turn边界／255、空组／上限人数、空槽／满包／换物取消、奖励后重复ACK、缺资源、提交前拒绝与等待后错误、HOST和重复输入。路径组合按共享读写交互补齐，不做无依据的全状态笛卡尔积。

静态结构／来源／引用全量覆盖不同绑定；有界状态与轨迹差分覆盖其参数和组合。**原版实机用于首次确认新语义族的时点、输入、取消、可见反馈和资源owner，以及少量跨机制的端到端抽验**：例如批次中有真实自然来源的一条入场→触发→结算→准备／下一入口链、一个跨等待／取消组合和恢复交接。由覆盖矩阵选择能够同时覆盖多种绑定的代表，不要求每关、每个重复引用再跑一场；新语义无法被既有代表证明时才追加最小代表。若静态／轨迹差分不能证明caller差异、动态资源／必要呈现，则该差异仍需实机取证，不能以“同模板”豁免。

阶段内只验证本阶段及共享代码直接受影响路径；S4统一审查、全面复测届时全部已支持历史功能、修复并精简。阶段编号是完整交付／提交单元，内部操作不逐步骤提交或停下来等“继续”。未验证场景保留支持门和原因，不换名为“已完成静态阶段”后悄悄开放。

## 7. 本轮可复跑验证与边界

本地输出放 `build/research-stage-batch/`，不提交完整dump。以下从本工作树根执行；Windows Python为3.12.10。Linux使用相同相对输出、将原资源路径换成本机合法副本，命令中的 `python` 换为 `python3`；本轮未执行Linux。

Windows PowerShell：

```powershell
python -B tools/static_data/audit_stage_data.py --original E:/Projects/fd2sdl/original_game --output build/research-stage-batch/full.json
python -B tools/static_data/research_stage_batch.py build/research-stage-batch/full.json --output build/research-stage-batch/normalized.json
python -B tools/static_data/audit_common_scope.py --original E:/Projects/fd2sdl/original_game --output build/research-stage-batch/common-scope.json --require-classified
python -B tools/static_data/generate.py --output build/research-stage-batch/generated
python -B tools/static_data/verify_stage_tables.py E:/Projects/fd2sdl/original_game/FD2.EXE --self-test
python -B tools/static_data/verify_movement_scripts.py E:/Projects/fd2sdl/original_game/FD2.EXE
python -B tools/static_data/verify_common_events.py E:/Projects/fd2sdl/original_game/FD2.EXE --json build/research-stage-batch/common-events-source.json
python -B -m unittest tools.tests.test_static_stage_audit tools.tests.test_audit_common_scope tools.tests.test_static_flows
python -B -m unittest tools.tests.test_research_stage_batch
```

Linux Bash（未执行；先将合法资源放入本树 `original_game/`，只读使用）：

```bash
python3 -B tools/static_data/audit_stage_data.py --original original_game --output build/research-stage-batch/full.json
python3 -B tools/static_data/research_stage_batch.py build/research-stage-batch/full.json --output build/research-stage-batch/normalized.json
python3 -B tools/static_data/audit_common_scope.py --original original_game --output build/research-stage-batch/common-scope.json --require-classified
python3 -B tools/static_data/generate.py --output build/research-stage-batch/generated
python3 -B tools/static_data/verify_stage_tables.py original_game/FD2.EXE --self-test
python3 -B tools/static_data/verify_movement_scripts.py original_game/FD2.EXE
python3 -B tools/static_data/verify_common_events.py original_game/FD2.EXE --json build/research-stage-batch/common-events-source.json
python3 -B -m unittest tools.tests.test_static_stage_audit tools.tests.test_audit_common_scope tools.tests.test_static_flows
python3 -B -m unittest tools.tests.test_research_stage_batch
```

| 检查 | 本轮结果 | 结论上限 |
|---|---|---|
| 全量来源audit／common归属 | 全33组结构成功；当前已解码引用错误0；1,497来源行归属完整 | 统计见§2；严格支持失败／未知CFG保留，不证明可运行 |
| 静态生成 | 194张表通过校验并生成当前flow | 当前输入／消费ABI可处理，不是新增场景实现 |
| stage表独立原字节 | 6张原始表、973 B、11项cue映射；11个负例拒绝 | 表来源与映射，不是entry／victory全流程 |
| 移动流独立原字节 | 106条、2,896 B通过 | 语法及原字节，不是所有actor绑定和实机运动 |
| common事件独立来源 | 21图、19来源控制路径、2张表17 B；0新绑定、6旧绑定迁移 | 静态读集，无本轮CPU／C差分或WGC |
| 现有审计／图负例回归 | 51项Python通过，无skip | 工具合同，不是SDL运行或原版验收 |

最小原型 `tools/static_data/research_stage_batch.py` 将全部1,584固定槽转换为稳定来源ID、完整源文件／entry摘要、相对／绝对offset、长度／读集摘要、原参数和分类／未决原因，输出 `normalized.json`。分类为35 supported、495 decoded_unopened、539 unmapped、515 unused_lookup；它含未被地图引用的action槽，分母与1,171保守需求不同。原型同时输出11条result提案（1既有／10候选、1来源族），新语义转换0、新正式绑定0；不把规范化槽数叫事件机制数或运行覆盖率。它沿用audit解码，不是独立语义oracle；候选只核来源／owner／现定义摘要，不自动证明HP防护有效域。复审后研究单测共5项通过，覆盖未知／默认255保留、坐标／字节不符拒绝、同target不提权、候选owner／阻塞保留，以及禁止输出覆盖输入审计／越出研究目录。与前述51项联合运行最终56项通过，无skip。

§2的shape／text完整目录统计可独立复跑下面的只读命令（PowerShell；Bash使用 `python3` 并替换最后资源路径）。它复用严格archive及文本目录解析，不重复写一套资源解码器：

```powershell
python -B -c "import sys; from pathlib import Path; sys.path.insert(0,'tools/static_data'); from audit_stage_data import archive_entries,text_fragment_count; p=Path(sys.argv[1]); s=archive_entries((p/'FDSHAP.DAT').read_bytes()); t=archive_entries((p/'FDTXT.DAT').read_bytes()); print({'shape_entries':len(s),'attribute_records':sum(len(raw)//4 for _,raw in s[1::2]),'text_entries':len(t),'text_fragments':sum(text_fragment_count(raw) for _,raw in t),'shared_text_fragments':text_fragment_count(t[0][1])})" E:/Projects/fd2sdl/original_game
```

严格门也已复跑：在第一条audit命令上分别加 `--stage 0 --stage 1 --require-supported` 和 `--require-supported`，输出改为 `required-01.json`／`required-all.json`；两者均按预期exit1，报告保留，不解释为结构读取失败。另在主项目相同 `4d7ed15` 基线只读执行 `python -B -m unittest tools.tests.test_common_event_sources tools.tests.test_common_event_contracts tools.tests.test_audit_common_scope`，22项通过。工作树首跑因未挂 `original_game` 有1项缺资源错误，另一次误写不存在模块名，均已纠正；不记为首次全绿，不将重复的common-scope测试累加作独立覆盖。

本轮 **SDL自动运行测试：未执行**；**局部机器码动态对照：未执行**（执行的是严格原字节／fixup及已发布来源检查）；**原版运行验收：未执行**。没有运行历史GUI、申请显示需求、改电源计划或混音器。原资源和主项目用户存档保持只读；Linux／macOS未执行。静态结果不能扩签G5未验的严格BSS、完整随机同轨、全部FX／声音、设备输出或绝对时序。

## 8. 收益、成本与防回退门

收益来自复用已建的原资源读取器、194张冷表、有限流程和G5真实owner，而非引入一套新运行架构。全量源账一次发现坏引用；相同result的11个槽共用一次语义恢复；同callback的多关触发共用一次控制流取证；例外按条件／动作族集中处理。成本主要是审计器与现行消费同步、CFG到有限节点的可追溯转换、分支身份／资源路径夹具，以及少量新语义实机代表。当前证据不足以给全部关卡自动转换比例或日历工期。

合并门要求：新关绑定必须能回到原source-id和确认flow；新增C分支必须说明哪项现有领域操作表达不了及独立来源；不得出现逐stage/action的剧情C链、DAT字段手抄、native整段handler或默认跳过未知。转换输出必须同时列资源组、原始记录、不同target、语义族、已转换定义／候选／未决和阻塞绑定，禁止把重复引用当独立机制数。来源／ABI／模板变化时只重验受影响闭包，批次封板后在S4统一全历史回归。

无需要用户重新选择的重大架构取舍。后续实施仍需单独启动，本轮交付止于调研、可复跑候选验证与计划修订。
