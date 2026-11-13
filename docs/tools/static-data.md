# 冷定义的构建与来源校验

当前内置的已确认冷表位于 `data/static/tables/*.json`，有限事件图位于 `data/static/flows/*.json`；清单与消费契约在 `data/static/manifest.json`。现有 item、magic、unit、movement、stage 等 getter 保留。构建期生成私有 `const` 表；流程节点在 `static_data.c` 中只编译一份，由 Game、Scene、预检和成长预览共用。各 SD 阶段是否通过验收，以[阶段记录](../plans/stage-log.md)为准，不能以生成成功代替。

SD 历史基线为79张冷表、四域30份流程、518个节点及6个共享绑定；G2 历史生成范围为168张表、五域36份流程定义、565个节点和6个共享绑定。2026-11-10 的 G4 中间候选曾为193张表、八域60份流程定义、1,283个节点和12个共享绑定；当时新增 `runtime` 与 `common_event`，后者尚无 stage 绑定。G4 最终冻结为193张表、八域62份流程、1,390个节点和12个共享绑定，已以 `4ada76b` 提交并集成本地 master；中间候选及失败记录保留原身份，最终依据见阶段记录。

G5 当前冻结输入为194张表、八域65份流程定义、1,476个节点和12个共享绑定；其中6个既有正式事件身份由 scene 迁入 common_event，没有新增关卡支持。**G5 已按本轮来源域完成统一历史回归和逐项签核；表、节点、绑定数不是玩法覆盖率。** 正式 START 的 Game 初始化与 Scene 前态继续共用 `scene.init.000`；新增领域不构成通用 VM。数据修改仍须重建，结构／来源检查不自动开放后续关卡，实际阶段验收见[阶段记录](../plans/stage-log.md)。

## 构建与修改

正常 CMake 构建自动使用 Python 3.9+ 标准库，将数据生成到各平台独立构建目录的 `generated/static_data/`。Release 运行无需 Python、JSON 文件或 `FD2.EXE`；构建需要 JSON 和生成器，离线源码清单已包含这些材料。命令见[工具速查](README.md)。

修改定义时只改 JSON，不改生成 `.inc`；源身份、原始读集摘要、canonical VA／依据保留在 `source`。`file_sha256` 表示完整源文件身份，`sha256` 表示原始读集；从已有确认实现迁移的参数使用 `baseline_commit` 和 `baseline_source_sha256` 追溯。本轮没有把这些 C 参数的迁移提升为新的原版机器码／运行验收。

生成器检查重复键／符号、未知字段、整数类型／范围、矩形维度、稳定消费契约、来源身份引用和已解码跨表引用。ABI 尺寸变化需要同时审查调用方，不能通过缩短数组绕过 getter 的原 ID 门。完整验证成功后才替换输出；多余旧 `.inc` 会明确失败，须确认其不再被调用后仅移除该构建目录内的旧文件。生成失败不得继续用历史缓存宣称本轮通过。

修改数值不等于与原版一致。生成校验允许符合类型和引用的有意修改；独立来源验证器仍必须指出它与原始读集的差异，不自动更新预期摘要掩盖差异。未知 byte 保留，未确认语义不猜成引用。

## 独立校验

从项目根目录执行。下列工具只读指定 EXE，使用严格 LE 页映射；不导入生产生成器，也不使用 JSON 自报的地址／尺寸作为预期读集。没有合法原版文件时可运行无资源负例测试，但不能记为源字节对照通过。

Windows PowerShell：

```powershell
python -B tools/static_data/generate.py --output src/build-win/generated/static_data --check
python -B tools/static_data/verify_unit_tables.py original_game/FD2.EXE
python -B tools/static_data/verify_stage_tables.py original_game/FD2.EXE --self-test
python -B tools/static_data/verify_movement_scripts.py original_game/FD2.EXE
python -B tools/static_data/verify_magic_contracts.py original_game/FD2.EXE --self-test
python -B tools/static_data/verify_runtime_tables.py original_game/FD2.EXE --self-test
python -B -m unittest discover -s tools/tests -p 'test_static*.py'
```

Linux Bash（当前未执行）：

```bash
python3 -B tools/static_data/generate.py --output src/build/generated/static_data --check
python3 -B tools/static_data/verify_unit_tables.py original_game/FD2.EXE
python3 -B tools/static_data/verify_stage_tables.py original_game/FD2.EXE --self-test
python3 -B tools/static_data/verify_movement_scripts.py original_game/FD2.EXE
python3 -B tools/static_data/verify_magic_contracts.py original_game/FD2.EXE --self-test
python3 -B tools/static_data/verify_runtime_tables.py original_game/FD2.EXE --self-test
python3 -B -m unittest discover -s tools/tests -p 'test_static*.py'
```

当前单位／成长／学习／物品／装备许可／法术／移动独立读集仍为9张原表、8,528 byte；另验ID48／66两份实际构造alias窗，共48 byte，不能混称新增基础表。准备／音乐／地形／暴击／脚步原973 byte及11项cue映射保留原身份。移动脚本已扩大为106条、计数消费总和2,896 byte，独立保留的完整候选payload为2,898 byte；跨流重叠和未消费尾分别保留，不能用两个总数之差猜尾部归属。

地图法术4张28槽合同投影分别表达effect别名、费用记录、效果目标上限和费用相对CAST的顺序；`verify_magic_contracts.py`独立核28个fixup及原call／jump／标量锚点，并与审查过的wrapper合同相联。它们不是4张原连续字节表，也不声称自动证明全部wrapper控制流。动作／Item呈现投影由 `verify_runtime_tables.py`核原字节与已审调用常量；迁移的C参数仍保留原blob身份，不混入8,528 byte统计。

## 事件图的编辑边界

脚本本身是冷数据。触发条件、分支、等待、取消、动作顺序与提示参数均由图定义；库存、选择、完成位、执行游标及单位状态仍由现有 Game／loot／progress／session 持有。SAV 不增加流程版本或执行位置，不序列化中途提示。活动 Load 仍重建原有边界状态。

八个领域共用节点结构，但各自只接受当前已确认的固定操作：

| 文件 | 冷定义范围 | 代码保留的职责 |
|---|---|---|
| `scene.json` | 自然开场、初始化及 game over 的呈现顺序 | 资源所有权、逐帧绘制／移动算法与单次副作用；旧六事件不在此重放 |
| `stage.json` | 支持门、入场、结果查询、胜利、结算、交接及音乐／预检顺序 | 原 session 的建立、替换与错误返回 |
| `loot.json` | 掉落、特殊格、满包换物、取消、text_ready／ACK、奖励／controller 续接 | 库存槽操作、原动作提交与 owner 边界 |
| `progress.json` | 奖励门、成长／学习提示与写点、毒伤／到期处理顺序 | 已确认成长计算、装备重算、RNG 与原单位写入 |
| `magic.json` | 32..35复合组、23传送的焦点／费用／四段呈现／坐标写点，以及Item组效果／浮字／重算／耗槽／恢复EX顺序 | 既有有限effect core、同步结果投影、Game的HP／MP／贡献与库存owner；不重放业务提交 |
| `preparation.json` | 商店、教会、转职、准备库存、选阵与 town／NoTown 的服务分支、等待、取消和写点 | App 持久 roster／资金／共享 RNG 与现有准备状态；页面缓存不拥有业务副本 |
| `runtime.json` | Round、阵营横幅、turn caller恢复／交接／返回、共享确认、死亡、详情及到场的分支、等待、取消和页操作 | 原 Game 提交、App 页面／视觉锚和同步呈现；不新建持久业务游标 |
| `common_event.json` | 已解码通用规则／来源片段及原六个正式事件身份的有限组合 | 同一 Game `event_resume` 与原 loot／progress／单位 owner，逐叶写点及呈现 ACK；不自动增加关卡绑定 |

每个 `operations` 条目定义参数名和范围；`flows` 使用稳定字符串 ID，节点用 `op`、有序 `args` 与命名出口 `on` 表达。`entry` 是默认入口，`entries` 仅声明既有 API 的其它入口。`views` 可限定 `logic` 或 `present`；省略则供两端投影。不能把两端各维护一份流程，也不能用一个 native 操作封装完整事件状态机。

同域 `bindings` 把运行入口映射到已有真实流程，例如 common_event 的 `"stage.000.action.000": "common.legacy.action.000"`。别名共享生成节点数组，不复制流程；禁止别名链、循环、悬空目标及跨域重名。以后已知组合可增加绑定，仍须经过本关资源／前置校验和既有支持门；同 action 的其它关卡只作为待核实的复用候选，不能自动开放。

例如当前满包换物使用下列真实节点（完整上下文见 `loot.json`）：

```json
{"id":"cell.ask_swap","op":"emit","args":[2,423,1,0,0,0],"on":{"error":"failure","yes":"cell.pick","no":"cell.declined","cancel":"cell.declined"}}
{"id":"cell.pick","op":"emit","args":[3,0,4,0,0,0],"on":{"error":"failure","slot":"cell.valid_slot","cancel":"cell.cancel.delay"}}
```

`emit` 的参数依次为提示种类、fragment、呈现 flags、等待毫秒数、fragment 是否取 descriptor、value 是否取当前 message_number；其参数契约在同文件的 `operations.emit`。YES／NO／取消和选槽后的去向由边决定，槽有效性由下一领域节点检查。原有 ACK、text_ready 和槽输入的类型校验仍由 API 负责；未到写点不得授予物品，重复通知不得再次提交。交换成功仍不置完成位，箱内参数保存换出的物品；取消同样不完成。

事件按原写序交错提交有限逻辑叶和呈现叶，成功 NEXT／ACK 后才进入后继；不先整段修改 Game 再由 Scene 重放。父 PC、真实 actor／item 和深度1 child loot沿现有热 owner，晚错／HOST 保留已到达前缀，不复制 Game 或回滚外部 RNG。`event_log` 保留未绑定来源诊断和停止边界，fallback容量及坏external deferred防御仍在，不能据此推成全库没有容量检查。原动作确认、事务校验、存档忙态和宿主输入分派仍是引擎边界；冷图调用它们，不复制 owner，也不借整段 native handler 隐藏分支。

正式 START 的初始成员、部署槽引用和模板选择也由 `scene.init.000` 共用定义。Game 消费 logic 视图，继续调用原 persistent 创建、active 拷贝和模板构造算法；Scene 的开场前隐藏由单独的 present-only 节点表达。生产代码不再保留旧四人成员、group 顺序、装备或派生属性的固定值断言。默认数值兼容性由独立来源与回归核对，运行时仍拒绝无效模板、精灵、profile 和越界引用。初始化节点缺少 logic 视图或带有 Game 未支持的逻辑操作时，构建校验直接拒绝。

生成器拒绝未知操作、非法参数／出口、悬空引用、不可达节点、没有退出的闭环、缺失的必需流程以及消费 ABI 变化；有意的有界扫描循环在运行时另受步数预算保护。结果查询必须是有显式结果终点的无环只读图。动作执行出错或遇未知操作沿现有失败路径停止，不能把未解码项当空事件成功。

修改数值或边不需要改 C；新增领域语义才需要更新最小原语、消费契约及独立回归。`manifest.json` 的 ABI 摘要固定操作参数合同，出口集合固定执行器接受的端口；它们不是第二份事件流程。原 FDFIELD 触发槽、metadata 与当前库存等引用仍读取现有资源／热值，不再抄成另一张静态触发表。

消费契约的 `ports` 必须是「操作名 → 可接受的排序出口列表集合」，不能写成全域通用出口清单。例如 runtime 中 `choice_tick` 只接受 `["no","yes"]`，`choice_read_key` 只接受 `["ack","cancel","next"]`，终点 `choice_done` 只接受空列表。生成器逐节点精确比较 `sorted(node.on)` 与该操作的某个合法变体，不能为了某个新图而给所有操作开放任意端口。`operation_abi_sha256` 按 `json.dumps(operations, sort_keys=True)` 的 UTF-8 内容计算，只固定参数合同，不证明 CFG、写序或原版语义；新增语义仍须审查消费代码及独立来源。

`required_labels` 还固定 C API 实际查找的外部节点入口。当前 loot 的 preflight.drop、preflight.cell、cell.begin、drop.begin、finish.common 不可仅同步图内引用后随意改名；缺失即构建校验失败。普通内部节点仍可重命名／重排。`entries` 的可达性声明不能代替真实 node.id 的消费合同。

## 共享运行与通用事件的来源边界

G4 冻结时，`runtime.json` 有68个有限操作、6份流程和321个节点：Round 42、phase 132、turncycle 28、choice 25、defeat 86、detail 8。G5 当前为78个操作、7份流程和381个节点，追加到场图及有源的清键／信息板／caller返回和方向reset等待边界。`verify_runtime_flows.py` 独立读取固定身份原 EXE 与严格 LE 页，按原 Round／phase／恢复／确认／死亡／详情等 caller 的 call／jump／立即数推导预期；不把 JSON 自报来源或生成 C 当作期望。静态正反例、原码、真实 C consumer 和 WGC 各自记账；实际等待、写点、工作页和可见内容的有效域见[共享运行语义 §8](../reverse-engineering/common-runtime-semantics.md#8-g4-共享菜单与-phaseround-实施依据)与[共同事件语义 §7](../reverse-engineering/common-event-semantics.md#7-g5既有事件按实际等待续行代码冻结最终门待验)。

G4 冻结时，`common_event.json` 有47个有限操作、15份未绑定来源流程和115个节点。G5 当前为54个操作、21份流程和183个节点，六个既有正式身份为stage000 action0..4及stage001 action6；其自然入口证据与15份机制来源夹具分开。`verify_common_events.py` 独立核原完整函数／显式片段、17 byte 原表及冷控制；`verify_fd2_legacy_events.py` 另核六个原 caller 的实际调用／写序。线性身份投影检查 future raw／MOVE，但不证明任意分支组合的完整未来资源。common39 仍是非 shipping 显式alias，空槽／满包取消／死亡反馈配对不自动成为自然 stage 绑定，也不表示21图全部有原生证据。G4 的30项局部机器码差分与原生范围见[共享运行语义 §9](../reverse-engineering/common-runtime-semantics.md#9-g4-有限通用事件续态与有效域)；G5 实际候选和剩余门见统一审计及阶段记录。

Windows PowerShell：

```powershell
python -B tools/static_data/verify_runtime_flows.py --output build/g4/runtime-source.json
python -B tools/static_data/verify_common_events.py original_game/FD2.EXE --json build/g4/common-event-source.json
python -B -m unittest tools.tests.test_static_runtime_flows tools.tests.test_static_runtime_tables
```

Linux Bash（本轮未执行）：

```bash
python3 -B tools/static_data/verify_runtime_flows.py --output build/g4/runtime-source.json
python3 -B tools/static_data/verify_common_events.py original_game/FD2.EXE --json build/g4/common-event-source.json
python3 -B -m unittest tools.tests.test_static_runtime_flows tools.tests.test_static_runtime_tables
```

## 批量来源与覆盖审计

当前 S1 全量来源入口为 `tools/static_data/stage_source_ledger.py`；固定 schema、Windows／Linux 命令、原字节／caller 合同、批量夹具及拒绝边界见[来源账规范](../formats/stage-source-ledger.md)。它扩展既有严格 audit／common-scope／LE／RLE 读取器，生成本地来源与转换输入，不改正式冷定义、consumer 或支持门。新输出必须是 `build/` 下不存在的目录，且不在解析后的原资源子树内；`--verify` 从原来源重生成比较。

2026-11-12 的历史计数、候选与当时审计口径见[批量调研](../plans/stage-batch.md)。`research_stage_batch.py` 保留为历史最小原型，不是当前全量合同；新运行不再把已由 S0 闭合的旧 HP 门列为阻塞，旧报告仍保留原身份。它不写正式定义或提权支持状态，不是任意 EXE 控制流翻译器。

`audit_stage_data.py` 只读提取 FDFIELD 的全部组、原始槽／模板／placement、地图实际格引用，以及严格 LE fixup 分发表目标。它校验原文件身份、已发布 IDA 的清单／EXE 身份／函数 chunk 与历史 Git 源 blob；报告保留源跨度、摘要、未知字节和未覆盖项。schema、固定操作 ABI、出口和引用复用生产校验，原始 EXE／DAT 预期从独立读集取得，不取生成 C 作为 expected。

S1 另列 `descriptor_semantics()` 的 drop_execute／defeated_collector／ai_story_collector／cell_execute 四种真实 caller，保完整 WORD callback 与旧分类差异；mode203 default 不再记为未知机制，cell 非0／1模式仍是 callback，collector255 与直接 default 不合并。`audit_common_scope.py` 的 1,497 行及 G1 `semantic`／`common_gap` 仅为冻结历史投影，`current_semantic` 单列当前路由，不代表当前缺机制。两个 audit CLI 均拒覆盖已有输出；复跑需换新文件名。源资源中的450个默认turn255候选和严格支持失败保留。

Windows PowerShell：

```powershell
python -B tools/static_data/audit_stage_data.py --output build/sd3-audit/full.json
python -B tools/static_data/audit_stage_data.py --stage 0 --stage 1 --require-supported --output build/sd3-audit/supported.json
python -B tools/static_data/audit_stage_data.py --require-supported --output build/sd3-audit/required-all.json
```

Linux Bash（本轮未执行）：

```bash
python3 -B tools/static_data/audit_stage_data.py --output build/sd3-audit/full.json
python3 -B tools/static_data/audit_stage_data.py --stage 0 --stage 1 --require-supported --output build/sd3-audit/supported.json
python3 -B tools/static_data/audit_stage_data.py --require-supported --output build/sd3-audit/required-all.json
```

第二、三条对当前原资源均应返回非零，并保留阻塞报告。默认 `(255,255,0)` 在turn255／phase0命中；G1已确认原DWORD回合无255上限、分派无action255哨兵，其表外读值不是合法code fixup。SDL保留原槽并在该边界显式拒绝，不补callback255或假定永远不到255；原版运行后果未据此宣称已测。`bound_definitions_closed`仅说明排除默认候选后的已绑定集合闭合，当前0／1为true；`--require-supported`仍拒绝默认候选、其它未知／未开放语义及引用错误，当前0..32均不通过。报告保留 `semantic_complete=false`。stage2入场支持与战斗禁用分别登记。

这里的 `entry_supported` 对应原战场 entry 分发表；stage31／32 没有该表的槽，不用它判定 `scene.new_game` 中已支持的开场段落。开场链及两端实际运行证据另按对应冷图和阶段记录核对。

本来源版本有99条有效资源项／33组，turn、lookup、action各保留528槽，原始1,899个模板和2,322个placement。G1／G2按metadata有效模板前缀及同索引位置复核，实际消费1,887个模板，无有效模板缺位置；stage0／31各一条、stage32十条共12条是前缀外原始尾，继续保留。历史兼容字段中的12项 unresolved 不再表示有效运行模板未知，不猜最近位置或据尾部group强行追加。

实际事件 callback 表是 `0x51b91` 起 90 项；其后 `0x51cf9/0x51cfd` 为两个值 18 的无 fixup DWORD，`0x51d01` 起是独立的 28 项 magic 表。另三张 result／entry／victory 各 30 项，主要分派共 180 槽；加 magic 共 208 个分派槽，目标可能重复，均有已发布 IDA chunk owner。函数边界／原字节覆盖不证明未知 handler 或 CFG 的语义，不将邻接区域合算为 120 个事件。

移动指针表另按完整106槽／424 byte盘点（`0x627d8..0x62980`），保留 `0x62980..0x634d2` 的2,898 byte原始payload。报告 `movement_pointer_table` 逐槽保存fixup、原DWORD、目标、下一不同目标形成的候选跨度及计数观察；候选跨度不等于已证分配边界。ID24的计数读取跨下一目标、ID84保留4 byte尾、其它项的原始actor字段可到83、direction可为5；均原样保留，不清洗、不判原资源坏。

G2的106条计数流均通过严格runtime语法／原字节校验，合计2,896 byte，getter与有限执行器覆盖原三种mode；活动容量恢复96。direction5保留原cache跨帧读取语义与资源校验，不取模改值；脚本引用还须小于当次实际actor count。106条语法可执行不等于所有stage触发绑定、资源或原版运行已通过，不能据此开启S阶段关卡。

SD3／G1 的 v3 来源报告保守潜在需求集合共1,171条记录引用，含重复：当时51条有已支持定义，468条属于已知类型／定义但未开放，652条未明确绑定或未解码；后者含450条默认turn255潜在匹配。它们不是1,171个不同事件，也不是当前执行覆盖率。该报告仅0／1的 `bound_definitions_closed` 为true，严格支持集合为空；不拿G5定义计数替换这个历史来源分母。v2的721条保留为排除默认填充的历史范围，不能当作严格全范围通过。已解码引用无错误不等于所有未知raw引用均有效。

常规新关接入：运行批量审计 → 核实 `reuse_candidates` 与原 callback／条件 → 将确认的组合加入唯一 `bindings`／定义 → 运行生成与引用检查、同输入状态／轨迹差分 → 仅对新增语义选代表原版场景。复用候选不会自动转为支持。新增未知控制流先保留源函数／chunk 和阻塞原因，再补最小领域原语及测试；不静默跳过、不写逐关 C handler，也不复制已有 DAT 表。

报告含原始资源／代码字节，只放 ignored 的本地构建证据目录；不提交、不进入发布包。离线 App 构建不依赖该审计工具、Git 历史、IDA 产物或原版文件。

## G1 通用机制来源分类

`audit_common_scope.py` 复用上面的原资源／LE／fixup／发布证据核验，再按[经审查的来源账](../reverse-engineering/common-event-semantics.md)映射原 source ID 与通用机制。审查输入是 `docs/reverse-engineering/common-scope-review.json`，包含原文件／分发表／函数 chunk 摘要和显式语义族；它不含原字节切片，不参与 App 构建，不是一份可执行剧情定义或平行优先级。

Windows PowerShell：

```powershell
python -B tools/static_data/audit_common_scope.py --output build/g1/common-scope.json --require-classified
python -B -m unittest discover -s tools/tests -p test_audit_common_scope.py
```

Linux Bash（本轮未执行）：

```bash
python3 -B tools/static_data/audit_common_scope.py --output build/g1/common-scope.json --require-classified
python3 -B -m unittest discover -s tools/tests -p test_audit_common_scope.py
```

G1 原分类输出分别保留208个分发表槽、1,171条保守引用、106条脚本和12条模板尾。后续 source-kind 账按1,497行、20个语义族分类，其中15个历史 common_gap族的388条引用不是388项当前未实现功能。缺少审查、owner／chunk不符或语义族未决均不得记为分类完成；结构数、分类数、当前实现和运行支持数分别统计。`classification_complete=true` 只说明该来源有归属，`all_game_semantic_complete`、`original_runtime_executed`、`new_gameplay_enabled` 仍为false，不解除游戏支持门。新增byte／CFG／语义先更新来源账，再把分支、等待、取消和写点写入唯一冷定义；当前完成结论只按28项审计与真实阶段门。


## G2 正常 caster 与 FIGANI 源域

`audit_caster_domain.py` 将32个原base、68个growth、20行learning、合法转职保留路径及1,887个有效模板的技能位图，按玩家／AI真实分派与FIGANI选槽交叉。只读严格身份原资源，不以SDL冷表作期望；普通选槽可按原规则回退，派生／复合固定槽不能借普通回退吞掉empty。

Windows PowerShell：

```powershell
python -B tools/static_data/audit_caster_domain.py --output build/g2/caster-source-domain-final.json
python -B -m unittest tools.tests.test_audit_caster_domain
```

Linux Bash（本轮未执行）：

```bash
python3 -B tools/static_data/audit_caster_domain.py --output build/g2/caster-source-domain-final.json
python3 -B -m unittest tools.tests.test_audit_caster_domain
```

当前固定原资源结果为50个玩家FULL技能组合、238个模板FULL实例／30个独立组合，均选到非empty。stage13／unit103／技能30的固定槽确实empty，但原AI评分为0且该ID走MAP分派，必须在报告中保留排除依据，不能藏入成功统计。MAP不读取caster FIGANI；玩家9与AI9的分派不同。此门复用G1的位图写入owner闭包，未覆盖任意外部SAV、注入位图、未来新增技能来源、真实图像／声音或原版运行。修改来源或分派时需重审对应域，不能由旧报告推定新组合合法。
