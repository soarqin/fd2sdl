# 关卡来源账与批量夹具合同

S1 使用 `tools/static_data/stage_source_ledger.py` 读取原资源和既有独立审计，生成可复跑的转换输入。本文冻结 `fd2-stage-source-ledger-v1`／`fd2-stage-source-fixtures-v1` 合同；当前优先级只见[总计划](../plans/development.md)。产物包含原始字节，只留在本地 `build/`，不提交或分发，也不作为 App 的数据入口。

## 生成与复核

Windows PowerShell，从项目根目录执行，输出目录必须尚不存在：

```powershell
python -B tools/static_data/stage_source_ledger.py --original original_game --output-dir build/s1/sources-NEW
python -B tools/static_data/stage_source_ledger.py --original original_game --verify build/s1/sources-NEW
python -B -m unittest tools.tests.test_stage_source_ledger tools.tests.test_static_stage_audit tools.tests.test_audit_common_scope tools.tests.test_research_stage_batch
```

Linux Bash 对应命令；本阶段未在 Linux 执行：

```bash
python3 -B tools/static_data/stage_source_ledger.py --original original_game --output-dir build/s1/sources-NEW
python3 -B tools/static_data/stage_source_ledger.py --original original_game --verify build/s1/sources-NEW
python3 -B -m unittest tools.tests.test_stage_source_ledger tools.tests.test_static_stage_audit tools.tests.test_audit_common_scope tools.tests.test_research_stage_batch
```

生成器直接读取固定身份的 `FD2.EXE`、`FDFIELD.DAT`、`FDSHAP.DAT` 和 `FDTXT.DAT`；复用严格 archive、LE 页映射、fixup、IDA 发布及冷定义校验，不接受任意 dump 代替原来源。四个文件、审查输入、工具与冷定义在生成期间发生变化即拒绝。输出先解析真实路径，拒绝现有目录、`build/` 以外路径和原资源子树，包含 `build/` 内的隔离原版副本及 Junction 指向；失败不覆盖旧报告。`manifest.json` 最后写入，缺失表示该批不完整。

| 产物 | 用途 |
|---|---|
| `inventory.json` | 原读取器的全量结构、fixup、函数 chunk、原参数和审计分类 |
| `common-scope.json` | G1 的 1,497 行历史归属投影与 S1 caller 同步结果，分别保存旧／当前语义 |
| `ledger.json` | 全量规范化记录、来源／target／语义族／去向与异常索引 |
| `fixtures.json` | 每条规范化来源一例，按共享族聚合的字段／引用／拒绝夹具 |
| `manifest.json` | 固定 schema、四份产物摘要、原来源身份与本批计数 |

`--verify` 校验 manifest、全部摘要和 JSON 重复键，再从原来源重新生成并逐份比较，包括数值类型和工具输入身份；只读原批。它证明产物未漂移，不是新的语义 oracle。修改工具或输入后必须生成新目录，不能给旧报告补签新身份。

## 规范化记录

| 字段 | 合同 |
|---|---|
| `source_id`／`ordinal`／`parent_id` | 稳定来源身份、账内顺序和父来源；去重不删除原槽。固定槽、模板、drop、位置及移动操作的 ID 与真实坐标交叉校验，同字节也不允许错槽 |
| `source` | 文件 SHA、跨度、原字节及读集 SHA。DAT 使用 entry／entry SHA／entry offset／绝对 file offset；EXE 使用 `raw-unrelocated`、object／object offset／canonical VA，绝不将 VA 当作 file offset |
| `caller`／`decoded`／`references` | 真实消费合同、保留位宽的已解码字段与有序来源引用。actor raw 下标不换成角色 ID；坐标 WORD 原值与 consumer 的低 BYTE 投影并列保存 |
| `family`／`semantic_families` | 前者是 19 个来源结构族，后者是已有审查的 20 个语义族；两者不作为实现完成数或玩法覆盖率 |
| `definition` | 引用唯一冷定义的 ID；不复制第二份可编辑图。已有 20 条定义引用不是 20 个新转换或新绑定 |
| `evidence`／`evidence_level` | 原字节、caller 规则或既有定义引用的证据等级。结构确认不证明动态可达、完整 CFG 或运行一致 |
| `disposition`／`reason` | `confirmed`：已核结构或 caller 规则；`excluded`：在明确消费域内排除但保留原字节；`rejected`：已知拒绝域；`unresolved`：确实未闭合的 CFG／参数或具体绑定。正式 runtime 分类仍单列在 `decoded`，不能由 `confirmed` 开关卡 |
| `control_flow` | 未恢复 CFG 的 `ordered_successors` 必须为空；已知图只引用既有 definition。不得由附近 CALL、表邻接或同 target 伪造边 |
| `blocker` | 每条拒绝／未决都带 S2 归属、受影响绑定／来源、语义族、原证据 ID 与下一取证动作。同 target 已有确认来源时复用该证据，只核新 caller／参数／资源，不要求重复恢复原函数 |
| `new_semantic_conversion`／`new_runtime_binding` | S1 恒为 false；本账不执行事件，不改热状态或支持门 |

`target_index` 保留 208 个分派槽至 193 个不同目标的映射；187 个 IDA owner／208 个 chunk 是另一口径，shared tail 不造新函数。`family_index`、`reviewed_semantic_family_index`、`destination_index` 和 `exception_index` 均保留来源 ID；`common_source_join` 将全部 1,497 个历史投影 ID 连接到本账。S2 按语义族和 target 一次恢复规则，再用批量参数／来源夹具覆盖各 caller；异常按族处理，不改为逐关实现。

## 源域与计数

固定原来源版本共 72,470 条规范化记录，每条一份来源夹具：

| 源域 | 记录口径 |
|---|---|
| DAT 容器与地图 | 199 entries、3 个完整目录；33 张地图／43,734 格，33 个 metadata 头，turn／lookup／action 各 528 槽 |
| 单位与位置 | 1,899 个模板，其中有效 1,887／尾 12；1,899 个重叠 drop 子记录；2,322 个原 WORD 位置、33 个 count 头 |
| 地形与文本 | 33 个帧目录／8,256 帧、9,366 个属性；34 个文本目录／1,016 片段 |
| EXE 分派与代码 | 208 分派槽、208 个带 owner 的函数 chunk、1 段相邻非指针字节 |
| 移动 | 106 指针、106 计数流、350 groups、1,045 actor／direction pairs、1 个完整候选 payload、1 段未消费字节 |

结构记录有父子重叠和共享 owner，不能将所有长度相加当唯一字节覆盖。移动计数流长度和为 2,896 B，原跨度并集为 2,894 B；候选 payload 为 2,898 B，`0x63221..0x63225` 的 4 B 单独排除并保留，ID24 跨下一目标的 2 B 重叠也保留。地图索引为特殊格 0-based／普通 lookup 1-based；帧引用只做已证有限资源索引检查，不模拟实际时钟轨迹。

位置分区是有效模板前缀 1,887 加 metadata byte1 声明的部署容量 435。`deployment_ordinal` 仅表示声明容量内顺序；原 `1088d` 还受 stage、roster 和分支影响，不声称 435 条都实际消费。依据见[准备语义 §9](../reverse-engineering/common-preparation-semantics.md#9-追加发现原模板计数和-placement-索引合同)。模板末尾未知字节、未消费模板、整个帧包／文本 payload 均由父来源保留；角色身份与未来资源投影仍归 S2。

## caller 分类与实际样例

descriptor 的独立依据为原 `1aa1d`、`1b6b7`、`1b653`、`190ac` 四个完整函数共 2,148 B 及两个共享尾 14 B，原 EXE 身份与固定 raw 读集摘要由 `descriptor_source_evidence()` 复核。复用已有 G 原码／实机证据，不以生成图或当前 C 的 expected 作原义。`117e7` 内部 `1197b`／`1199c` 是 callsite；不能按表符号尾名造函数。

- drop 直接执行：mode 0／1／2／3 分别为 item／money／callback／story；4..255 为已证 default，仍清输入。普通 collector 在执行前排除 255，AI story collector 只收 mode3；这两种过滤不等于直接 drop 的 default。
- 特殊格 `190ac`：mode0／1 走 item／money，其余 mode（含 3、203、255）按完整 WORD callback。0..89 只查精确 caller 绑定；90／`0x0100` 明确拒绝，不截低 BYTE，不模拟表外调用。
- 450 条默认 turn255／phase0 记录保留潜在匹配及 callback255 拒绝域，不静默忽略。未知 CFG 没有合成空事件或默认胜利。

真实样例 `stage:14:drop:00033` 来自 `FDFIELD.DAT` entry43、相对 offset1011、绝对 offset90953，3 B 为 `cb0000`（mode203／WORD0）。规范化保留 raw 与 caller `defeated_collector -> drop_execute`，得到 `semantic_kind=default_noop`、`disposition=confirmed`；旧 `unmapped_semantics` 与当前 `decoded_unopened` 并列。它只确认 caller 规则；S1 生成时 stage14 未开放，当前正式绑定另见下文 S3 合同。`stage:02:drop:00000` 的 `030900` 指向 `text:003:00009`；story 的文本引用与 callback 的分派引用分开。

`dispatch:stage_runtime_result:002` 与 stage0 同指 `0x205b4`，S1 原账保留当时待 S2 闭合的绑定阻塞并指向已确认 stage0 证据，不重复生成结果图或自动开放 stage2。既有 callback0..4／6 登记其精确六个 caller 定义，其他来源仍单独检查。

本版本处置为 confirmed 69,628、excluded 2,077、rejected 450、unresolved 315；这是不同粒度的来源去向，不能算运行覆盖率。1,171 条保守需求的旧 51／468／652 与当前 51／469／651（supported／decoded unopened／unmapped）另账，只有两个固定 descriptor 分类表达变化。G1 的 `common_gap=388` 只属历史投影，不重开 G5。

## S2 转换叠加合同

S1 是不可回写的来源基线。S2 的完整函数读集、候选、原码／C 比较和去向叠加另建新目录；不把 S1 的 `unresolved` 换标签后冒充已实现。当前阶段状态和完成收据只见[总计划](../plans/development.md)与[阶段记录](../plans/stage-log.md)。

| 输入／产物 | 合同与消费者 |
|---|---|
| Domain 完整读集 | 每个原槽保留 target、真实 owner 与显式入口。逐 chunk 读取全部指令，含共享尾；所有字节核 strict relocated 镜像。表槽 raw-unrelocated 指针及 fixup 另核，不靠函数名称或附近 CALL 推流程 |
| callback 候选 | `convert_stage_callbacks.py` 输出固定 CFG 的有限冷图、逐槽决策及独立原指令轨迹。相同整图才去重；已有图用精确内容摘要引用。同 target 不自动提升 caller／WORD 参数域 |
| entry／victory 候选 | `convert_stage_transitions.py` 保留中途入口、共享初始化／淡入淡出及条件写序。固定子图引用沿现有 Scene 的深度和总预算，不支持任意函数调用、变量或新热执行栈 |
| 结果来源 | `s2_result_source.py` 登记 30 槽／18 目标的完整来源与原 raw 前缀。13 个无等待目标由 const 结果查询消费；5 个含文本目标用现有 common-event 续态，保留文本前后的锁存写点 |
| 全来源叠加 | `s2_source_disposition.py` 连接全部 208 槽，以及 S1 的 315 条未决、450 条拒绝。每条保留原 ID／locator／摘要、caller、候选定义或阻塞依赖。源图存在与可发布绑定分开，所有新 `new_runtime_binding` 仍为 false |

源读集、原指令执行与 C consumer 比较是三个独立证据。转换器模板和生成图不能给自己作 expected；C probe 必须运行正式 Game／Scene／Play 领域实现，明确列出资源、构造、绘制或输入替身。没有 `--c-probe` 的产物不能签 C 集成通过。输出保护继续遵守 S1 的真实路径、全新目录和输入晚守卫；callback／去向叠加最后写 manifest，缺失表示该批不完整。transition 使用单份 `conversion.json`，来源、工具、正式定义和实际 C 身份分别登记，不能因文件存在而忽略其中未验的门。

### 去向字段与计数

可复跑的 Windows／Linux 命令统一见[冷定义工具的 S2 入口](../tools/static-data.md#s2-批量转换与去向复现)。S2 使用的 S1 `ledger.json` SHA256 为 `1a47357ba1f179db74bd0d99bf2096748596e6fc46c1ab1aabf30950eb3e6202`；原 72,470 条记录及其 72,470 份来源夹具不重新编号。S2 校验原 S1 manifest 和 raw／跨度／引用身份，不用当前工具给旧批补签新身份。

| 源域 | 原槽／不同 target | S2 来源去向 |
|---|---|---|
| result | 30／18 | 12 槽引用既有 2 图；18 槽引用新增 16 图。共 18 个目标定义，其中 13 个 const 查询、5 个带等待的 common-event 图 |
| callback | 90／90 | 17 槽引用既有图；72 槽转换为 66 份新图；1 槽因 teleport actor／ABI 边界阻塞。90 个目标落在 88 个原 IDA owner，不把中途入口拆成函数 |
| entry／victory | 60／58 | 3 槽既有引用、45 个转换来源、12 个未决；转换来源共享 41 份 caller 图与 3 份固定 helper，另保留 4 个 source-only alias |
| adjacent magic | 28／27 | 继承 G2 已审有限 primitive 与 caller 合同，逐槽核原 target／别名／费用；不称新增 28 份图。player24 先走 FULL，不能因 map24 与 map16 同 target 把 caller 合并 |

208 槽合计 193 个不同 target。`converted_candidate` 统计来源槽，`existing_definition_reference` 统计既有图引用，`existing_primitive_caller_domain` 只用于已审地图法术 caller 域；三者均不批准新的正式绑定。`blocked`／`unresolved` 保留实际语义族、受影响来源和下一取证动作，未知不会变成空图、默认胜利或静默略过。

去向叠加保留 S1 的 315 条 `unresolved`：301 条已有来源图或既有 primitive 可供后续闭合，14 条仍有阻塞依赖；450 条 `rejected` 全部保留为有源拒绝。208 槽与 315 条未决有重叠，三组按 `source_id` 去重的并集为 778，不能相加为 973 个独立机制，也不能换算玩法覆盖率。运行支持状态继续取唯一冷支持门和精确 caller 绑定。

transition 行的 `flow_id` 保留逻辑来源 ID，`canonical_definition` 指向实际唯一图，`source_only_alias` 明确是否去重。当前 `stage.source.entry.015`／`.018`／`.019`／`.020` 与 `.012` 的完整图同义，只在 stage 域内引用 `.012`；原表槽、target、owner、参数和来源记录仍分别保存。`source_bindings` 必须与唯一 stage 输入中 `stage.source.*` 的别名子集完全相等，不允许别名链、循环或悬空目标。这些 source-only alias 不属于 `stage.NNN.action.NNN` 正式绑定。

`definition_present_in_static_input` 只说明规范终点存在，不能单独作为成功门。叠加工具还核 callback 的 `definition_sha256`、候选完整图内容、manifest 中原码／C 收据摘要，以及 transition 唯一图和 alias 合同。同 ID 换图、缺图或错终点必须重新生成并复核，不能继续借用旧候选的通过记录。

### 实际源到消费的例子

`dispatch:stage_runtime_result:029` 的原 target 为 `0x20bf5`。完整原函数不初始化 `53ECC`：raw20 hidden 时写原码 2（胜利），随后 raw0 hidden 时写原码 1（败北）；raw1 hidden 则先调用 `15f84` 播放当前关 fragment7，返回后才写败北。冷图唯一输入为 `common.result.source_020bf5`：

```text
count >= 21，否则拒绝
raw20 hidden? → result_set(SDL victory=1)
raw0 hidden?  → result_set(SDL defeat=2)
raw1 hidden?  → story(7) → ACK → result_set(SDL defeat=2)
end；无命中时保留现有锁存
```

`fd2_field_game_common_event_begin/peek/reply` 复用既有 Game 续态；Play／Scene 播放成功、检查 HOST 后才回复 ACK。文本未完成或退出时不提前写结果，不整体回滚已到达写点。S2 冻结时它仅为来源定义，没有新增 stage29 正式绑定；S5 的正式绑定另按下文核验。原结果码 `0/1/2` 对应 SDL `ongoing/defeat/victory`，必须显式映射。旧 stage0／1 的 S0 合同继续有效，不能用结果图的安全缺 raw 拒绝去模拟原越界行为。

`dispatch:event_callback:075` 对 caller 的 `record+8` 与 9 比较，是 `branch_actor(field=TEXT_ID)`，不是 `unit_id`；`030` 的两个身份字段终值虽同，原 `+7` 再 `+8` 的写序仍保留。S2 的早期 Python 对照曾与候选共用错误偏移表，正式 C 前缀比较将其拒绝；失败候选留本地，不计入最终通过。`offsetof`／枚举／WORD 断言与逐叶原码比较分别守卫 ABI 和写序。

### 身份、地图与等待边界

未来身份预检只借 Game，局部投影单位／roster、完成位和地图选择器；不复制资源 owner、不取 RNG、不回写或回滚 live 状态。原 group、placement、脚本和模板字段仍从既有 DAT 读取器引用。追加单位只确认原来源可证的身份，HP、派生值和库存未知时枚举受影响分支；已知 caller、回合、完成位等不因无关库存未知而虚构分支。路径数和步数有上界，超界明确拒绝。真实叶执行时仍校当前资源，错误或 HOST 保留已提交前缀。

换图先在独立资源视图核 map／shape 和地形帧引用，随后叶与说话人使用投影身份和地图；不把未来 map 写进 Game，不提前改变 App 水色／idle 时钟。正式提交继续由原 Game／Scene owner 完成。

新源图的 `palette_add` 是基色加值、上限 63；`palette_subtract` 是减暗度、下限 0，两者只写 DAC，保留基色和索引帧。旧比例 `brightness` 不作为这两种原义的替代。淡出 `1f882` 为 0..63 共 64 级，淡入 `1f525` 为 64..0 共 65 级，各级尾均等待 2 ms；级数和等待置于唯一冷子图。`17aa9` 保留 signed WORD 时钟差和 signed DWORD goal，正值大于 65,535 明确拒绝；名义 SDL／55 映射不声明硬件时钟相位一致。普通键不跳过固定等待，HOST／错误沿原 owner 停在当前前缀。

`common_event` 的原 G4 校验器仍只签既有 21 图；新来源图的 ID 另列为未由该工具证明，须通过 S2 专属完整来源／原码／C 门。S2 未因生成新图而扩大旧六个 shipping alias 或支持门；S3 的新增正式绑定另按下文核验，SAV 布局和共享 RNG 合同继续沿用。

夹具保留原 ID、顺序、参数、读集摘要、引用和去向，支持批量来源／规则验证；夹具由本账派生，不是独立原义 oracle，不替代原码轨迹、取消／失败状态测试或原版代表性运行。S1 未新增 CFG 转换、运行绑定或新关卡，未证明全部文本 token／字形／头像、图像像素一致、动态地图／身份路径及全部玩法。

## S3 发布域与活动恢复合同

S3 读取固定 S1 包、S2 去向与完整 Domain 来源，另外核当前生产定义和正式绑定；S1 的 72,470 条记录、315 条未决和 450 条拒绝不回写、不重编号。S2 历史报告保留原工具／图／probe 身份。当前命令、实际 Windows 产物与计数见[冷定义工具的 S3 入口](../tools/static-data.md#s3-正式绑定与发布域校验)。

### 来源勘误与独立消费门

`dispatch:event_callback:041` 的真实入口为 `0x35346`。完整原入口在 `0x353ae` 压入 4，再从 `0x353b0` 跳至共享尾 `0x3530d`，不会执行原 owner `0x352ca` 中 `0x3530b` 的 `push 11`。正确来源组合为 `story(3) → loot_child(1,-1) → story(4)`，对应唯一新图 `common.callback.041` 与正式 `stage.016.action.041`。旧 `common.loot.child.1` 的 story11 组合及 G 阶段已证域不修改。

这是对 S2 `existing_definition_reference` 分类的来源勘误，不给旧 S2 候选或原生报告改签。`s3_callback_binding_source.py` 另存 `S2_refinement`、完整 owner chunks、显式 non-owner 指令、条件两边、逐例输入、原检查点与 `actual_C` 状态。callback8／12／26／39／41 的新 caller 参数门共 180 例原指令及 192 例实际 C；非玩家 child no-op、满包 Cancel、wrong ACK、HOST 和错误输入仍分别校验，失败原件保留。原呈现／helper 替身范围随报告登记，不把 child 的原返回值喂成伪造 C expected。

S3冻结时，`convert_stage_callbacks.py` 因而得到 73 个转换槽／67 图／2,045 节点、16 个既有引用和 callback82 一项阻塞；S2 历史计数仍为 72／66／2,040／17。`verify_stage_release_traces.py` 读取唯一正式 `bindings`，逐绑定核 stage、原 caller 引用、定义全文摘要，再用该 stage 实际 DAT 资源执行原指令与实际 C。冻结 09 产物覆盖 38 个新模板绑定／36 个定义；3 个既有模板绑定由独立 caller 门签核。两项计数合为 41 个正式 callback 绑定，不是 41 个独立机制。

### 发布闭包产物

`stage_release_scope.py` 输出 `fd2-stage-release-scope-v1` 的 `scope.json`。它只读既有真值，既不生成正式绑定，也不成为第二份可编辑关卡定义：

| 字段／检查 | 合同 |
|---|---|
| `input_sha256`／`S2_history_retained` | 原 S1／S2 身份、当前定义、工具及 caller 勘误证据分别登记；原资源和输入晚变即拒绝 |
| `stages[].bindings` | 每条正式 runtime ID 保留原 source ID、target、定义域／规范终点；source-only alias 精确核 `stage.source.*` 子集，新增 shipping alias 单独核，不用来源图存在代替授权 |
| `map`／`metadata`／`placements` | 从实际 DAT entry 复核位置、长度、摘要及已解码引用；原模板／group／部署容量仍由 DAT 读取器提供，不手抄第二套资源 |
| `entry_count`／`capability` | 从完整冷 entry 成功路径和实际 group 计算人数集合，与支持门下界相联；不取当次 SDL 构造输出作为原义 expected，不用初始 group0 数冒充完整 entry |
| `callback_source_references` | 保完整 WORD action、原 caller、父模板 group 和原跨度；`required_for_battle_scope=false` 只保留非战斗来源，不表示已有活动绑定 |
| `resource_references` | 已解码文本与移动引用保留来源／参数；未来人数、身份、地图和换图后资源仍需实际有限 C 投影及到达叶预检，结构检查不替代这些消费者 |
| `victory_handoffs` | 每个成功 next_stage 必须有已绑定 entry、capability 和独立核原字节的准备记录；S3 冻结域为2..19→3..20的31 B普通准备行，S5另核NoTown。单次 source Scene 完成后只读导出准备，App 关闭旧资源再打开下关，不预执行下关 controller |
| `default255_refusals`／`strict_supported` | 默认槽原 raw `ffff00` 和拒绝 source ID 原样保留；结构发布闭合不提升为严格全状态支持 |

S3冻结新增正式域包含 battle2..19 与 entry-only20，共 96 个绑定／86 个不同原 target：19 entry、18 victory、18 result、41 callback。`stage.json` 新增 52 个 shipping alias，`common_event.json` 新增 44 个；既有 stage0／1 不计入这个增量。result 的 15 个无等待绑定由 stage 域消费，3 个带副作用绑定用 `stage.NNN.result` 的 common-event 续态；同关同时存在两种正式结果绑定应拒绝，不能用不存在的 pure 图冒充闭合。

S3 冻结的55条 callback 来源引用中，battle2..19 必需50条，entry-only20 保留5条。stage20 当时仅入场、浏览、已证准备及存取，battle／END 关闭。13个隔离源槽为 entry22／24／26／27／29、victory20／21／22／23／26／28／29，以及 callback82；后者还关联 `stage:29:turn:00000`，故共14条受影响来源。S3 当时未为这些隔离源新增正式绑定；S5 用完整来源和消费者门逐族闭合，不能给旧报告改签。

### 默认槽、人数与 owner 边界

本发布域2..20 保留 265 条默认 turn255 拒绝来源，battle2..19 为 254，entry-only20 为 11。全 S1 的 450 条是33组总数，其中0..29 为 406，30..32 为 44；不能把450当本批关卡的拒绝数。默认槽可以存在于合法 Load 的 metadata 中，到实际 turn255／phase0 分派才明确拒绝 callback255；不静默删除，也不以全 metadata 预检阻止恢复。

source-bound capability 的 `preparation_slots=0` 表示只读原 DAT 的部署容量，不能解释为零部署人数。snapshot 人数在原 96 容量内使用已证 entry 下界、`exact=0`，允许实际增援和 hidden 状态。stage12 的初始59与 stage18 的初始64不应为未来 raw59／raw64 读取补假单位；原条件和已证增援时序决定何时需要额外前缀。stage16 的 entry 成功人数可为52或53，按持久 roster 分支取合法下界，后续结果读取仍守实际分支。人数下界不宣称任意外部 SAV 全状态等价。

活动／手动 Load 完整恢复已存 metadata、schedule_turn、原单位及未知字节，不要求 metadata 与原资源恒等，不重放 entry、不额外自动查询结果；到真实 controller／END／结果／交接调用点再按当前有限语义和资源核验。失败、HOST、错误 ACK 或取消继续沿 Game／Loot／Scene owner，保留已到达前缀；不能借候选图新建热状态、修改 SAV 格式、复制资源 owner 或回滚共享 RNG。

`structural_binding_scope_closed=true` 只签当前来源／定义／精确正式绑定闭包；原码模板、实际 C、正式 controller／END／恢复链和少量原版原生代表分别留证。该工具的 `runtime_paths_validated`、`new_native_runtime_executed` 和 `all_text_glyph_portrait_semantics_validated` 仍为 false，不以夹具自验或静态读取替代运行证据。

## S5 来源扩展与候选资格

S5适用实现与验收已闭合，已证域为stage0／1原范围及stage2..29战斗／END／事件／胜败交接和结局；resource30不是普通战场。当前优先级、提交／集成状态只查[总计划](../plans/development.md)，依赖与实际候选见[S5工作包](../plans/s5-batch-expansion.md)。本节扩展S1原ID和S2／S3去向合同，不删除历史隔离标签或改签为当时已完成。复跑命令见[冷定义工具的S5入口](../tools/static-data.md#s5-扩展正式参数与独立-82-门)。

`convert_stage_transitions.py` 的扩展先执行原 S2 完整80-owner门；`s5_stage_transitions.py` 再从新增完整 owner／chunk／CFG、strict relocated 字节和独立 cinematic 合同生成 `fd2-s5-transition-candidates-v1`。13个原来源槽分别保留 caller、原目标和原跨度；callback82 另有 stage29 turn00000 引用，仍按13槽／14引用计数。中途 source08 的128-owner／19图／15,059节点及两个 ending 阻塞保持原身份。source10 严格合并141个完整 owner，13个来源均有完整候选，共20图／17,002节点；这些结构计数不表示141个独立机制或全状态运行通过。

扩展的`input_sha256`区分原字节／生产者代码和生成前静态JSON快照，后者保留生成时身份，不要求安装后整个JSON仍等旧摘要。`validate_candidate`重读独立原字节及当前生产者；`cold_flow`逐图与当前唯一冷定义精确比对，当前操作ABI由生成器与manifest另核。安装新绑定不为旧快照补签，其复现自检也不充当独立原义oracle。来源scope以原fixup／S1来源及明确S5扩展join正式binding，同target不能自动提升其它caller。未知、显式fail及未资格化绑定继续阻塞依赖批次。

新增普通交接沿原31 B准备行；NoTown 核 `0x523e7` 原30 B flags，22／23／24及27／28／29不要求不存在的普通行，26仍是普通 Town。`victory_handoffs` 只登记能到普通 `end` 的 next-stage 路径；26正常臂交接27，坏结局保热 stage26、借 resource30，29先改热 stage30再构造 resource30。两种终章均到无正常返回端口的 `terminal_wait`，不产生 stage30 普通战场或准备能力。

本轮全绑定预检另发现callback51旧alias误归属。完整`355f0`及`35631 push3→35633 JMP3530d`要求descriptor索引2的mode0／item101 child后story3（`52748=006500`），不能将callback分派mode2误作物品模式；新`common.callback.051`承接此独立原caller，旧`common.loot.child.2`和其story11历史图保持。它是既有已分类来源的细化，不增加或伪造新的S1来源ID，也不计入原13隔离槽；新定义与真实C／原CPU对照单独资格化。

### 完整来源、正式绑定与 82 分层门

`s2_source_disposition.py --s5-extension` 保持208槽／193个不同 target、S1 的315条未决原记录和450条拒绝不变。source10 叠加后，315条均有来源图或已审 primitive 可供消费，源去向不再有未决项；原记录的历史标签不回写，来源可用也不表示已经运行。按 source ID 去重的并集仍为778，不能把引用或重叠集合相加为独立机制数。

当前2..29正式绑定结构域包含163绑定／149个不同原target：28 entry、28 victory、28 result、79 callback；其中5个副作用result走common-event，其余23个走stage域。结构闭合与实际C、正式输入和原版门分别签核，未将163绑定称为163个独立运行机制。112条callback来源引用与79条正式callback绑定分列。`scope.json`保留379条域内默认turn255拒绝；全33资源组仍为450条，战场0..29的406与资源30..32的44另列。默认槽可合法Load，实际到turn255／phase0的callback255分派仍拒绝，不能删除或在恢复前静默归零。

26..29的 snapshot 下界分别为63／34／76／27，`exact=0` 允许原增援与 hidden 状态，容量仍为96；`slots=0` 只表示部署容量取原 DAT。人数下界和恢复合同不替代每个实际叶对未来 raw、身份、地图和资源的重验，也不宣称任意外部 SAV 等价。旧0..25绑定及 entry26 的原 alias 不因 B 补丁重写。

通用 callback converter 保留90槽、74转换来源、15既有引用和82一项明确阻塞；source82由独立 S5 分层门覆盖，不从通用 ACK 模拟实际 teleport。`verify_stage_release_traces.py --callback82-proof` 核原槽 target `0x362e8`、S1 caller `stage:29:turn:00000`、正式 `stage.029.action.082`、当前20-node图及唯一147-node共享图，并重核所有输入、二级原码／实际C及 EXE 摘要。同 ID 换图，即使同时修改收据摘要也须被来源模板门拒绝。

82 原 caller 轨迹覆盖 state16=0..4及33f78；实际 teleport module 覆盖8状态／408有序前缀；实际 Play→Scene 代表为state0／4及 ACK／HOST／错误、display／cursor边界；projection 另覆盖五状态。这些范围不能合称五状态完整 Play 像素运行。正式批量报告将82单列 `separately_verified`，其通用 ACK 向量贡献为0；70个普通模板绑定／67定义的1,890原码／C轨迹和8个既有模板引用另账。

C22分层协议仍为`fd2-s5-callback82-layered-C18-v1`，名称是schema版本，不是C18运行身份。当前Play层必须声明`build_target=fd2_field_s5_callback82_play_test`；输入SHA必须包含`src/CMakeLists.txt`、原Play main和新增`src/tests/field_s5_callback82_scope_checks.c`。严格门核CMake唯一同名目标实际列入双TU，缺项、错摘要、同改收据但删／移接TU、假注释recipe或重复目标均拒绝。新TU在真实commit后、wipe前注入HOST；旧delay preflight钩子不再承担该测试，未回签原版或旧候选的等待／像素范围。

### 自然栈来源与冷编排边界

原 `2ebe1` 不在自身入口初始化 HP 栈 cell，不能据此推断自然值任意。完整 `31529 → 2e2b0 → 2e9a8 → memmove` 原栈链已证明末写 DWORD 为 `0xAF8C0`，marker 按 WORD 写入 `0xF8C0`（63680）；资格仅用于31529的20组原参数和非split资源，不扩大到其他战斗 caller。旧240个受控 poison 向量仍属原 callee依赖域，不改签为自然状态。

marker、typed HP写、两侧 RNG、首侧3%最多两轮、第二侧最后marker之后返回以及等待都在唯一冷图。duration0仍经过HP／hit／cue，但没有帧等待。六个固定 cinematic callee 和有界22 part／10 tick来自已证族，不能变成任意脚本 VM、整pair native handler或新热状态／SAV字段。

发布 scope、默认255拒绝、独立原码、实际C、SDL正式输入和原版代表分别留证。当前来源账不签自然全流程、所有时钟相位／音色／像素、未用资源尾或原越界行为；错误、HOST和取消继续保留原owner已到达前缀。

### 输入变化与定向继承

C18来源索引 `build/s5/source-full-c18-index02.json` 的七份主报告共记录233种输入路径；解析相对／绝对路径和原资源junction后为223个实际文件。source10生成前的三个静态JSON快照独立保留，不加入安装后全文件相等门；完整来源和当前冷图仍由scope／disposition重核。路径条数、文件数、来源数和机制数不得混计。

C19当时仅在这些输入中的`preparation.json`增加`notown.normal`的darkness0及来源说明。完整G1 `26152`读集独立给出`2628b`清VGA→`2629c`调用`11d40(0,255,0)`→`262a6`notice75→`262d0`text410；三个暗度参数均占原32bit栈槽。旧出口`26370`的darkness255保留。当时scope／disposition和G4／G5来源join在新目录重跑，去掉输入manifest后与C18逐JSON相等；回调、正式参数、82分层和封存探针按其未变输入继承。该结论不跨过后续runtime注释、source等待／binder或HOST测试变化。

C19历史资格索引为`F:/fd2sdl-evidence/s5-20261114/source-C19-qualification03/index.json`；完整原字节与受影响读集另见同目录`notown-source.json`。当前严格闭包使用`build/s5/source-final-C22-index02.json`：七份主报告235种路径／225实际文件，加入fresh资源门后229个声明文件逐SHA全等；三份C22 Scene／Play／projection探针、C22正式参数Game探针及继承C21 Game探针另核身份。回调Game三矩阵保持C21原身份，82／module／正式参数／scope／disposition／资源及G4／G5由C22 fresh门连接，不能统一改称C22重新执行了全部旧门。

source10的三份生成前JSON快照保历史SHA，当前wholefile相等门不包含它们；其原来源／逐图／ABI由fresh工具重验。当前资源扫描仍为1,873引用／154 archive entry；正常招募已证人数上界31，外来32人credits超出原构造／raw引用域时明确拒绝，不改变SAV32条持久容器，也不代表所有未来身份组合已证。以后即使只有注释改动，也需记录精确旧输入重建、相关定义／可执行文件和参数域的资格；真实业务或冷序变化必须补受影响门。旧失败和旧报告始终保持原身份。

人数来源为初始4 ID＋已恢复招募literal的有限caller，18／19由`23a0a`与`24754→24bde(18)`及signed turn条件互斥。原`1088d(30)`只初始化31条active；`31c49`按`count-1`读取，malloc96不构成raw31已初始化的证明。该异常边界保原来源并显式拒绝，不回写S1账、不改32条roster容器，也不以重复callback或外来32人快照扩签正常路径。
