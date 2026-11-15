# 已完成阶段归档与证据索引

本页只登记**已完成阶段原范围**和历史证据，不是当前待办表。当前优先级和未开放域只查[当前计划](development.md)。S0—S5 均已完成各自授权范围；G5 `4d7ed15` 的原完成域保留，结果恢复、审计表达和旧调色等补充各由独立阶段闭合，不撤销旧阶段。F1—F6已获连续实施授权，未完成阶段不提前计入本页。

## 1. 完成摘要

表内“完成”只指所列交付及其当时有效域，不指未开放关卡或全游戏所有状态。早期阶段存在多提交／多候选，表中如实列关键提交，不补造单一完成commit。A5随B5、A6随B6，不另造重复记录。

| 阶段 | 已交付范围 | 完成／关键提交 | 证据入口 | 适用边界 |
|---|---|---|---|---|
| B0 | 必需事件、提交与首错停止 | `d282503` | [早期阶段表](stage-log.md) | 有限支持域，不等于整关原版验收 |
| B1 | 正式控制、菜单／END／存取／结果；自然开场、镜头补验 | `542aeb6`起；`2035e02`、`0842f05`补验 | [补充复验终审](stage-log.md#补充复验终审与恢复后续计划) | 多提交原身份；旧END症状、绝对输入精度不扩签；结果新补充见S0 |
| B2 | 玩家基础法术0／13的正式输入、提交和取消 | `b20dc53`；`6603241`登记 | [B2正式提交](stage-log.md#b2正式提交与默认图形套件登记) | 当时基础法术域；其余正常来源后来由G2补齐 |
| B3 | 已确认Use／Give／Equip／Drop | `09818fa` | [完整B3](stage-log.md#2026-11-05-完整b3usegiveequipdrop) | 多候选配对和组合原码域，不是所有组合一次fresh原生通过 |
| B4 | 经验、成长、学习及状态提示 | `47744f3` | [完整B4](stage-log.md#2026-11-05-完整b4经验成长学习与状态提示) | WGC／局部原码各有有效域 |
| B5／A5 | 首关结算、准备／四槽、stage1入场与场景曲目 | `557155d`；统一审查`9206cc1` | [B5／A5](stage-log.md#b5a5结算准备与下一关受支持范围完成)、[统一审查](stage-log.md#2026-11-06b0b5a5-统一审查与历史回归) | 自然流程联合来源，不声明全部随机战果／音画一致 |
| B6／A6 | FULL演出与动态音效bank | `852ecbc` | [B6／A6](stage-log.md#b6a6full-战斗演出与动态音效) | 核心最低门／AI casting／业务采集分账；SHORT与FULL不合并语义 |
| B7 | stage1自然实战、结算至stage2入场／浏览／存取 | `a72c54a` | [B7](stage-log.md#b7stage-1-整关与-stage-2-交接) | stage2战斗／END及后续私有绑定未开放 |
| B0—B7总收口 | 实质审查、历史回归与精简 | `8fdfb2f` | [最终闭合](stage-log.md#最终闭合当前-windows-受支持范围) | 分候选完成，不回写为单次全部fresh |
| A0—A4 | 格式、框架、PCM、XMIDI／OPL及基础场景 | `e9a2d81`前既有多提交 | [音频基线](audio.md#1-当前基线)、[原义依据](../reverse-engineering/audio.md) | 未登记独立阶段单commit；后续A7／A8有修订 |
| A7 | 当前事件／voice／loop／fade、选项／Load行为 | `15d262a` | [A7](stage-log.md#a7当前受支持音频行为与后端边界) | 保留Nuked原始响度／音色；不扩签全波形或严格PIC时序 |
| A8 | Windows本地降级及device／source生命周期 | `ec8e699` | [A8](stage-log.md#a8宿主与设备生命周期) | 未签真实拔插／路由迁移／扬声器输出 |
| R1 | MIT Windows本地包、第三方材料及离线替换库重建 | `678ee13` | [R1](stage-log.md#r1windows-本地交付许可与常规音频回归)、[发布规范](../tools/windows-release.md) | 本地交付，非对外发布；Linux／macOS发布未验 |
| SD1 | 冷表来源、严格生成与构建链 | `6b24b33` | [SD1](stage-log.md#sd1静态冷表来源严格生成与构建链2026-11-09) | 原资源不分发、生成输入唯一 |
| SD2 | 受支持事件编排进入冷图 | `ecb154a` | [SD2](stage-log.md#sd2当前受支持事件驱动进入冷定义2026-11-09) | 当时提交／重放模型后来由G5逐叶续行修订，不能照旧例开发 |
| SD3 | 批量审计与静态化统一收口 | `c1b6598` | [SD3最终门](stage-log.md#sd3-最终验收与提交门2026-11-09) | 分类是当时读集；不以结构成功证明玩法；审计同步另列S1 |
| G1 | 通用原义取证与来源分类 | `5b9619f` | [G1](stage-log.md#g1-通用语义取证与来源分类2026-11-10) | 归属不等于全部CFG已转换；早期common_gap标签非当前缺口 |
| G2 | 正常来源通用战斗动作及必要呈现 | `61d5856` | [G2最终门](stage-log.md#g2-最终定向验收复审与提交门) | 原语／参数域，不是任意异常外部状态 |
| G3 | 持久roster、准备服务、16／20部署及写序 | `08f61f6` | [G3收口](stage-log.md#g3-阶段收口2026-11-10) | 32人为保留容量，不是所有自然组合已验 |
| G4 | 共享流程／菜单／运行及视觉owner | `4ada76b` | [G4](stage-log.md#g4-共享流程菜单与运行2026-11-10) | 当时跨帧输入等阻塞由G5后续闭合 |
| G5 | 通用合同修复、逐项实审与Windows历史回归 | `4d7ed15` | [G5最终门](stage-log.md#g5-通用机制统一审查与历史回归收口2026-11-11)、[审计§13](common-logic-audit.md#13-g5-通用完成门与最终验收2026-11-11) | 当时已审计域；新结果恢复差异另列S0，不把旧证据改签为补充通过 |
| S前研究 | 全量来源盘点、最小规范化原型与候选计划 | `bf714f3` | [研究报告](stage-batch.md)、[研究日志](stage-log.md#s前批量读取转换调研与计划修订2026-11-12) | 新语义转换／正式绑定0；不是S阶段已实施 |
| S0 | stage0／1 hidden结果原义、Load／controller结果锁存时点、HP0可见输入及恢复owner | `02bf7d8` | [S0记录](stage-log.md#s0-结果判定与活动恢复合同修正2026-11-13)、本地`build/s0/local-integration.json` | 受控接受域6类双端，局部原码／SDL／原版分账；不扩签自然产生、全Load CPU或S1—S4 |
| S1 | 全量来源／规范化与批量夹具、四caller审计同步、target／语义族／去向及异常索引 | `35e3f26` | [S1 记录](stage-log.md#s1-全量来源账审计同步与批量夹具2026-11-13)、[来源账合同](../formats/stage-source-ledger.md)、本地`build/s1/local-integration.json` | 72,470 来源／夹具；315 未决与450拒绝域保留；新语义转换／绑定0，不是运行通过 |
| S2 | 126个新冷定义、共享语义转换与实际有限消费者、有界投影、结果／调色／等待及源域冻结 | `54ebc72`；`build/s2/local-integration.json` | [S2报告](s2-shared-semantics.md)、[S2记录](stage-log.md#s2-共享语义转换与能力补充2026-11-13) | 四组双端原生、分层原码／C域；新正式绑定0。13来源槽／14依赖仍隔离、450拒绝保留，旧比例fade唯一归S4；不承诺全游戏／全RNG／逐帧或其它平台 |
| S3 | stage2—10／11—19战斗批次、stage20仅入场，96来源绑定；结果调用续点、恢复、只读准备交接与必要总览修正 | `ad5a63e`；`build/s3/local-integration.json` | [S3报告](s3-batch-integration.md)、[S3记录](stage-log.md#s3-按依赖闭包批量接入正式流程2026-11-14) | C12的24项CTest／当前探针；3组双端代表中turn3／victory按明确源码影响继承，stage12为C12新实做。受控而非自然整关；13隔离／14引用与450拒绝保留，旧fade当时留S4 |
| S4 | 旧 Scene／App／准备调色和调试转场尾修复、审计信任边界、全历史有限资格及统一回归／精简 | `0cca15f`；`build/s4/local-integration.json` | [S4报告](s4-unified-review.md)、[S4记录](stage-log.md#s4-统一审查全历史回归与精简2026-11-14) | 同C01的120项CTest、1661工具项／0skip，当前原码／C及6组fresh pair；历史30组／146登记有限资格，不称全fresh。13／14隔离和450拒绝不解除，原精度／平台边界保留 |
| S5 | 原13槽／14依赖闭合；stage20—25与26—29两批战斗／交接／结局、有限Scene能力及正式恢复 | 本阶段完整提交；`build/s5/local-integration.json` | [S5报告](s5-batch-expansion.md)、[S5记录](stage-log.md#s5-隔离语义闭合与后续批量接入2026-11-14)、[研究历史](s5-investigation-history.md) | C22 18项CTest含124工具项；source／参数／资源全账、有限原码／C、分候选原生及严格资格分账。450拒绝、外来32 credits及原精度／平台边界保留；不称全游戏所有状态或一次全fresh |

## 2. 长历史与实际收据

F1主体在2026-11-15用户调整范围后作为一个阶段提交交付：固定参照／来源分母、已证有效域、37／66前缀和82容量修复、A20正式工具与装载资格。提交身份为本次F1主体提交；本地集成收据为`build/f1/rescope-20261115/local-integration.json`。[F1报告](f1-reference-domain.md)保各候选实际测试／原码／原生和继承边界。**QK专项未通过**，不计入已完成范围，见[专项](qk-behavior-followup.md)；不是原F1完整域无条件全绿。

中途候选、旧顺序、数量、失败和诊断由[stage-log](stage-log.md)及其链接的历史报告维护，S5调查另归[研究过程归档](s5-investigation-history.md)。这些记录均不维护当前优先级；细节回到以下原始入口，不另复制一份流水账：

- 第一关自然117页开场、镜头及边缘焦点：[B1补验](stage-log.md#补充复验终审与恢复后续计划)；完整首关／结算／四槽的多来源联合证据：[B5／A5](stage-log.md#b5a5结算准备与下一关受支持范围完成)。
- B0—B7各候选和复审：[B大阶段记录](stage-log.md#最终闭合当前-windows-受支持范围)；原版SHORT与FULL、音画有效域分别见[战场历史工作包](battlefield.md)和[音频证据](audio.md)。
- SD历史模型、来源账及补验：[静态数据历史方案](static-data.md)、[SD3最终记录](stage-log.md#sd3-最终验收与提交门2026-11-09)。旧SD集成收据属于原任务证据目录，当前主项目 `build/sd3-local-integration.json` 不存在；不伪称已从该路径重新核验。
- G5中间失败、原候选／继承／离线与新实做之别：[G5最终节](stage-log.md#g5-通用机制统一审查与历史回归收口2026-11-11)和[审计§13](common-logic-audit.md#13-g5-通用完成门与最终验收2026-11-11)。旧报告失败保留；最终签核不将其改成passed。

### 旧优先级归档

B／A／R历史路线为B0—B7及所需A→A7／A8→R1，之后SD1→SD2→SD3→G1→G2→G3→G4→G5；各阶段实际范围以上表为准。研究提交`bf714f3`当时只规划S1—S4，没有实施；`fdad3b6`在其前独立列S0；当时S0闭合后待办回到S1，顺序只查统一入口。这些只是历史，不用旧P0／P1／P2、B小步骤或G表确定下一动作。2026-11-12版本的完整旧入口可由 `git show bf714f3:docs/plans/development.md` 复查，当前只保留摘要和原日志链接。

2026-11-13只读核实的本地收据如下；它们是历史证据，不是本次重新运行结果。路径相对主项目，完整收据／dump留本地不提交：

| 收据 | 实际身份与核验范围 |
|---|---|
| `build/g1/integration.json`；`build/g2..g4/local-integration.json` | 文件均存在，对应上表G提交；G1收据的后续G未完成字段保持当时身份 |
| `build/g5/local-integration.json` | `4d7ed15`，本地FF与Debug重建成功；其hash与最终门引用一致 |
| `build/g5/final-gate-review.json` | `source_domain_general_gate_passed=true`、`full_G1_G5_delivery_complete=true`、`S_started=false`；34份引用收据及N69最终原件存在且摘要匹配 |
| `build/research-stage-batch-20261112/local-integration.json` | `bf714f3`由`4d7ed15`快进；31项证据摘要匹配；SDL／原码动态／原版运行均为false，新玩法未开放 |

最终G5的CTest／Python、默认历史登记及补充登记各有独立候选、skip、继承和离线范围，数量只查原账，不相加成“最终候选一次全过”。本次只核收据及来源身份，不重跑历史游戏测试，不触碰原档。

## 3. 保留的未验边界

| 边界 | 当前结论／后续处理 |
|---|---|
| 严格运行时LE全对象／BSS | 旧严格门未通过，需新selector／object base／完整dump；历史21个非零BSS／stack字节不得自动豁免。只阻塞严格运行时等价声明；见[LE规范](../reverse-engineering/le-validation.md)与[审计§13.4](common-logic-audit.md#134-共用限制与精简结论) |
| 完整音画／共享随机同轨／绝对时序 | 全targetFX／SFX、全部像素、严格PIC／绝对时钟及两端全随机轨迹没有被现有有限门证明。Natural各端实战不同不自动归因RNG；见[审计§13.4](common-logic-audit.md#134-共用限制与精简结论) |
| 音频用户选择与物理设备 | 保留Nuked原始响度／音色，不加BGM补偿；不等于设备可听、路由／拔插、绝对静音、唯一播放实例或全波形通过。PCM原义修正不受该选择禁止；见[音频边界](audio.md) |
| 输入／END旧症状 | 有限FIFO／caller读清键已修复，宿主repeat仍近似。旧首次END默认NO原因未定位，后来的YES样本不抹掉旧失败；再次复现按原接收链取证，不无源猜改 |
| 未知字节／资源有效域 | fresh未写9B、heap尾与全游戏读者未完全确认；Loaded已有字节按本端preload保留。未确认资源／未来CFG拒绝；不以SAV unused补热初值或以节点数作完整率 |
| 平台／发布 | 历史Linux证据保原身份，后续Windows验收不反写为Linux／macOS已验；R1仅Windows本地包和已列离线重建，不是其它平台发布或自动分发 |

这些边界忠实保留旧交付身份，不代表原版一致。2026-11-14用户要求新的完全对齐审查，剩余差异／未证域已统一进入[新矩阵A01—A22](full-alignment-audit.md)及[当前计划F1—F6](development.md#4-新优先级与依赖)，不重开G／SD／S历史。用户重新选定Windows最终验收及保留Nuked声音并明记差异；其余旧有限域不作为永久豁免。`39422ae`仅交付审查计划；用户随后授权连续实施，当前F1尚未通过完整阶段门，证据与残余查[F1报告](f1-reference-domain.md)。
