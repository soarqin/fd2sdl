# 关卡来源账与批量夹具合同

S1 使用 `tools/static_data/stage_source_ledger.py` 读取原资源和既有独立审计，生成可复跑的转换输入。本文冻结 `fd2-stage-source-ledger-v1`／`fd2-stage-source-fixtures-v1` 合同；当前优先级只见[总计划](https://github.com/soarqin/fd2sdl/blob/373e5c0bd21454793a20fc62ecb8addb18055dba/docs/plans/development.md)。产物包含原始字节，只留在本地 `build/`，不提交或分发，也不作为 App 的数据入口。

> 本文记录历史数据合同，不是当前构建输入。旧生成器、批量覆盖工具与复跑命令保留在[同一提交的完整规范](https://github.com/soarqin/fd2sdl/blob/373e5c0bd21454793a20fc62ecb8addb18055dba/docs/formats/stage-source-ledger.md)。

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
