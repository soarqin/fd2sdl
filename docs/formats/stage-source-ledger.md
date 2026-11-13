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

真实样例 `stage:14:drop:00033` 来自 `FDFIELD.DAT` entry43、相对 offset1011、绝对 offset90953，3 B 为 `cb0000`（mode203／WORD0）。规范化保留 raw 与 caller `defeated_collector -> drop_execute`，得到 `semantic_kind=default_noop`、`disposition=confirmed`；旧 `unmapped_semantics` 与当前 `decoded_unopened` 并列。它只确认 caller 规则，stage14 仍未开放。`stage:02:drop:00000` 的 `030900` 指向 `text:003:00009`；story 的文本引用与 callback 的分派引用分开。

`dispatch:stage_runtime_result:002` 与 stage0 同指 `0x205b4`，账仍保留 S2 绑定阻塞并指向已确认 stage0 证据，不重复生成结果图或自动开放 stage2。既有 callback0..4／6 登记其精确六个 caller 定义，其他来源仍单独检查。

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

transition 行的 `flow_id` 保留逻辑来源 ID，`canonical_definition` 指向实际唯一图，`source_only_alias` 明确是否去重。当前 `stage.source.entry.015`／`.018`／`.019`／`.020` 与 `.012` 的完整图同义，只在 stage 域内引用 `.012`；原表槽、target、owner、参数和来源记录仍分别保存。`source_bindings` 必须与唯一 stage 输入的 `bindings` 完全相等，不允许别名链、循环或悬空目标。这些 source-only alias 不属于 `stage.NNN.action.NNN` 正式绑定。

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

`fd2_field_game_common_event_begin/peek/reply` 复用既有 Game 续态；Play／Scene 播放成功、检查 HOST 后才回复 ACK。文本未完成或退出时不提前写结果，不整体回滚已到达写点。它是来源定义，没有新增 stage29 正式绑定。原结果码 `0/1/2` 对应 SDL `ongoing/defeat/victory`，必须显式映射。旧 stage0／1 的 S0 合同继续有效，不能用结果图的安全缺 raw 拒绝去模拟原越界行为。

`dispatch:event_callback:075` 对 caller 的 `record+8` 与 9 比较，是 `branch_actor(field=TEXT_ID)`，不是 `unit_id`；`030` 的两个身份字段终值虽同，原 `+7` 再 `+8` 的写序仍保留。S2 的早期 Python 对照曾与候选共用错误偏移表，正式 C 前缀比较将其拒绝；失败候选留本地，不计入最终通过。`offsetof`／枚举／WORD 断言与逐叶原码比较分别守卫 ABI 和写序。

### 身份、地图与等待边界

未来身份预检只借 Game，局部投影单位／roster、完成位和地图选择器；不复制资源 owner、不取 RNG、不回写或回滚 live 状态。原 group、placement、脚本和模板字段仍从既有 DAT 读取器引用。追加单位只确认原来源可证的身份，HP、派生值和库存未知时枚举受影响分支；已知 caller、回合、完成位等不因无关库存未知而虚构分支。路径数和步数有上界，超界明确拒绝。真实叶执行时仍校当前资源，错误或 HOST 保留已提交前缀。

换图先在独立资源视图核 map／shape 和地形帧引用，随后叶与说话人使用投影身份和地图；不把未来 map 写进 Game，不提前改变 App 水色／idle 时钟。正式提交继续由原 Game／Scene owner 完成。

新源图的 `palette_add` 是基色加值、上限 63；`palette_subtract` 是减暗度、下限 0，两者只写 DAC，保留基色和索引帧。旧比例 `brightness` 不作为这两种原义的替代。淡出 `1f882` 为 0..63 共 64 级，淡入 `1f525` 为 64..0 共 65 级，各级尾均等待 2 ms；级数和等待置于唯一冷子图。`17aa9` 保留 signed WORD 时钟差和 signed DWORD goal，正值大于 65,535 明确拒绝；名义 SDL／55 映射不声明硬件时钟相位一致。普通键不跳过固定等待，HOST／错误沿原 owner 停在当前前缀。

`common_event` 的原 G4 校验器仍只签既有 21 图；新来源图的 ID 另列为未由该工具证明，须通过 S2 专属完整来源／原码／C 门。旧六个 shipping alias、支持门、SAV 和共享 RNG 合同均不因生成新图而扩大。

夹具保留原 ID、顺序、参数、读集摘要、引用和去向，支持批量来源／规则验证；夹具由本账派生，不是独立原义 oracle，不替代原码轨迹、取消／失败状态测试或原版代表性运行。S1 未新增 CFG 转换、运行绑定或新关卡，未证明全部文本 token／字形／头像、图像像素一致、动态地图／身份路径及全部玩法。
