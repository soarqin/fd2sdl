# 原版完全对齐审查与差异矩阵

推进策略更新（2026-11-15）：用户授权按真实依赖后置QK未完成验证。F1主体范围交付，F2—F5独立机制继续；A04／A05／A08相关资格及受影响输入／RNG／音画轨迹仍由[QK专项](qk-behavior-followup.md)在F6前闭合。下文事故时点的“F1阻塞”不再冻结全部开发，延期项不改签通过，最终完全对齐门不变。

实施状态补记（2026-11-14）：审查计划39422ae交付后，用户已授权本会话／现工作树连续实施F1—F6；当前F1实施中。本报告的“本轮只审查”及原测试结果保留审查时点身份，新增实现／证据由[总计划](development.md)和后续阶段记录登记，不将候选或局部修正签作整阶段完成。

审查日期：2026-11-14。代码、冷定义和发布证据基线：本地 master `495e7d9`。本会话的 `cindy/fierce-newton` 从 `88b47d7` 快进到该提交后开始审查。当前待办顺序只由[总计划](development.md)维护；本文记录事实、差异 ID 和验收依据，不建立另一套优先级。本轮只审查、取证和写计划，不实施补完功能。

结论：S5 已完成原定交付，原 13 隔离槽／14 引用不再是待办，stage20—29、26 坏结局、29 终章已接入。当前尚不能声明完全对齐：存在固定启动随机状态、宿主输入／时间近似，以及完整正常有效域、全部反馈和独立动态证据的缺口。新目标必须在这些已完成范围之上扩签，不能重做旧阶段，也不能把旧的有限验收自动提升为全游戏等价。

## 1. 终态、参照与有效域

### 1.1 本轮用户已选择的终态

- 最终验收平台为 **Windows**。Linux 继续维护构建／操作兼容说明；本轮和后续 Windows 结果不代签 Linux／macOS。
- **保留 Nuked 的声音，并明确记录为差异**。这不是原版声音一致。登记为 `EX-AUDIO-NUKED`，仅涉及该后端的响度与音色；不豁免曲目、事件、声部、PCM、cue、循环、淡变、开关、Load、时序或清理。
- 因此最终合格声明必须写成「Windows 正常有效域对齐，保留 `EX-AUDIO-NUKED` 声音差异」，不能写成「原版所有声音／波形完全一致」。除这一明确例外外，不默认沿用旧阶段的精度或支持域豁免。

### 1.2 固定参照身份

原资源来源是主项目 `E:/Projects/fd2sdl/original_game/` 中既有的用户本地文件。它们可识别为本项目一直使用的资源集合；**发行版标签、取得渠道、原盘身份尚无独立材料证明**，不能凭哈希猜称某正式版／补丁版。参照以字节身份为准，来源说明在 F1 补齐；不要求用户重新提供本地已有数据。

| 文件 | SHA-256 |
|---|---|
| FD2.EXE | `bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd` |
| FDFIELD.DAT | `81310211c6e96ebd89e79648650696691c740229fa4095ece37e7226bc27b74d` |
| FDSHAP.DAT | `901b70ea82d5d977192759fad510921ffe16a0ab6af6ab7c32757de03e30aa3c` |
| FDTXT.DAT | `b47e39ee727482528da061a8705f5cdee8a9cf759447aeda298afe17d9dd8bcc` |
| ANI.DAT | `be909c71d0f1121b6632ae931d978e990f6d54c830f4e0509cd6862187c4d963` |
| BG.DAT | `b9fc21d019d6256a4bb7e6da1cefcb0bfe331d8ff74a52a8201570afc98b56de` |
| DATO.DAT | `dff859783606ee95d9811e1e564bcb230ef53c4df9130e8b6d42ad37bc8c27bd` |
| FDICON.B24 | `7efb4448d05f19c1e17ebd53f3e3afead235f5c008d5167548d834c3686b1e44` |
| FDMUS.DAT | `4105ebde543fe1c497e852728f6bc333bda80edeb7fb3671e487504bee74e998` |
| FDOTHER.DAT | `882580dfa31bd22e069eb151b7a86a2b8dbcd8194ec755b00036c96a4a0e0667` |
| FIGANI.DAT | `c0cef326c5f8bc61252f03394140dbeed2280ddba0da99bfacdee1bf568846f8` |
| TAI.DAT | `d56fea9c43f8bb59aad89ad76698885d7e07f380d12a4547888a0b60ea5e0410` |
| TITLE.DAT | `06120a4605e054f2ff1e08f882661b9b345b39978aabf5126ec137e1207815b3` |

以上13文件为本轮实际只读核验的EXE与12份资源；11DAT共994个顶层entry，B24独立格式。其余运行文件、字体、驱动、DOSBox-X二进制和配置须列入同一小型身份清单；本地资源摘要与51个S5守卫核验见本轮证据索引。存档不是固定资源初值；每个用例分别登记只读输入与隔离写入副本。发布的IDA规范视图为canonical relbase VA，object1的OFF为VA−0x10000；raw／relocated／runtime继续分离。静态fixup复核保持有效，不重新制造loader疑虑。

推荐继续以已有 DOSBox-X／原驱动配置作为可复跑行为基准。F1 必须冻结实际二进制、配置、BIOS tick／CPU cycles、显示和 SBPRO2 驱动身份，而不是依赖未登记的默认设置。真实 DOS 声卡、CRT、键盘控制器的全部组合不在当前 Windows 验收声明中；若未来改为真实硬件参照，应另立参照身份，不能把两种输出混作一个 oracle。

### 1.3 “完全”包括什么

正常有效域是从原版启动／正式 START／原版有效 Save、Load、Continue 经正常输入可达、且原程序每次读取和分派均有确定来源的状态和轨迹。包括所有普通关卡、隐藏条件、招募与互斥、隐藏商店、全部剧情和分支结局、败北、等待、取消、重复操作、长回合、动态地图／身份／人数、配置、存档及场景切换。不是「旧测试走到过的状态」，也不以 stage0 的值或历史受控注入范围界定上限。

比较合同包括：整条业务状态和写序、整数宽度／回绕、共享 RNG 初始来源及每次调用、输入 FIFO 和各 caller 的读／清键、等待与跨框相位、完整索引像素／调色／RGB、文本和嘴型、全动作反馈、音乐音效及生命周期。时间以冻结的原时钟和输入时间轴为参照；未证明的像素、时间、音量容差一律不默许。逻辑必须等值；确属宿主调度的测量误差，先量化来源、范围和观测方法，再明确审批其比较方式。

资源 0..32 共 33 组不等于 33 个战场。正常战场分派是 0..29；30 是内部尾声，31／32 是开场资源链。正确终态是证明所有正式消费者和可达分支，而不是为了数量好看给 30..32 增造战场入口。

### 1.4 未定义行为与宿主适配

分三种域登记，不能用「安全」一词统包：

| 域 | 合同 | 关闭条件 |
|---|---|---|
| V：正常且有确定来源 | 必须复现，包括原版不合理但确定的读写与结果 | 源控制流／读写集和真实调用上下文闭合，独立 oracle 与正式路径通过 |
| U：原未初始化、越界、无合法 code target 等 | 不承诺模拟任意内存破坏；在确定的第一处 U 边界停止等价声明 | 证明到达条件及第一处失去定义的位置；此前有效前缀仍须一致；若正常玩法可到达，必须在完成声明中逐项披露 |
| H：宿主窗口、OS 音频、退出、设备缺失及坏外部输入 | 保留有界安全处理、原档不变和资源清理 | 不改变 V 域；与原 DOS 行为不同的部分有明确标识及反例，不能冒称原版失败语义 |

不能先把拒绝项放进 U 域再免验。默认 turn255 的触发、实际读取目标和当次内存来源仍需调查；也不能以历史「不支持无限回合」排除正常长时间游玩。外来 32 人档、修改资源、随意重复 callback 不自动属于 V，但必须通过来源分析证明，不能靠旧门名称判定。S5 已证明的特殊战斗自然 HP 来源不再列为 U。

F1补充对象来源准则：初始化按具体语义字段和当前owner有效期核查。新分配槽位中，尚无构造、合法active记录回读、明确字段写入或独立来源合同的值，不能仅因底层字节能追到已释放PCM而升级为人物属性。判U必须证明从分配／恢复到首次语义消费的相关writer闭包；`raw>=count`本身不够。合法Save的opaque字节复制、原逻辑明确保留的未知字节、显式xy-only写，以及已独立证明的live-owner／完整自然caller来源合同仍按各自V合同保留。S5自然HP的完整栈写序合同继续成立。此准则落实本次已定义的“原未初始化”域，不扩张成任意heap／错误内存的机器模拟；具体闭包未证的项仍未决，不能只改分类过门。

## 2. 本次实际审查与证据边界

39422ae原审查从 `main.c → run_field_chain → Play／Game／Scene → preparation／title`、当前冷定义及正式 bindings、支持／预检拒绝、原发布 IDA、来源账和实际收据交叉审查。重点覆盖历史边界和共享机制，未逐行重新证明所有 C／C++、第三方合成器或 1,312 个原函数。未进行新的全游戏自然通关、GUI／原版实机、录音或运行时全对象 dump。下文的「未审计」是显式剩余工作，不是隐藏在审查通过中的空白。

| 层 | 本轮实际执行 | 不能据此声称 |
|---|---|---|
| Git／基线 | 核三工作树和 master；本树 FF 到495e7d9；只读核 S5 集成收据及最终来源索引 | 不把旧88b47d7的stage20禁用状态套用当前代码 |
| 原数据／静态 | 用当前 `stage_source_ledger.py` 重读原 EXE／DAT，生成 72,470 行账；生成器校验196表、8 flow域；IDA发布物14项测试通过，927个源码地址token均canonical | 结构成功不是语义／自然运行成功 |
| Domain／原码静态 | 对 S5 已有数据库隔离副本用 idalib、Domain API 只读查询；核 main@25bf4和RNG@4ebe3的完整指令／chunk摘要，save=False，数据库前后不变 | 没运行原CPU或原生；未量出启动rand实际迭代数 |
| SDL已有二进制 | 主项目现有Debug无窗口6项：input、dialog_flow、field_rng、field_visual、bgm_native_sequence、audio_pcm，6/6通过 | 没重建本树；不是本树新构建或全历史复测 |
| 审计器单测 | 本树首次因无original_game路径，setUp错误／0项运行；改在主项目同提交运行17项，16过、1失败 | 失败仍保留；不能报告审计工具全绿 |
| 原版运行 | 继承S4／S5各候选的有限资格，只读核收据和来源 | 不合成“一次最新候选全绿”；原生失败、SDL-only和original-only原身份不变 |

新本地证据目录为 `build/alignment-audit-20261114/`；集成时复制到主项目 `build/full-alignment-audit-20261114/`，以 `review-receipt.json` 为入口。完整原资源／媒体／转储不提交、不分发。只读引用 S5 的 `build/s5/local-integration.json`、`source-final-C22-index02.json` 与 `F:/fd2sdl-evidence/s5-20261114/`；源摘要相同只支持相应有限继承，不代表重新执行。

收据核验的具体边界：S5的51个原资源／用户档守卫仍全部匹配；IDA清单11/11文件摘要匹配；8份F盘原生报告原件摘要匹配，stage29原C20整报告仍false、后续SDL-only另账。5个旧elegant-pasteur路径已不存在，其中C22构建／CTest／IDA发布／清单的主项目保留副本摘要匹配；旧App原件缺失，当前主App是S5集成重建的另一二进制，不称原件仍在。此次没有据路径漂移重开S5功能。

## 3. 全量盘点：分母、消费者与状态

### 3.1 结构和来源盘点

| 对象 | 当前分母与分账 | 当前资格／差异归属 |
|---|---|---|
| 顶层DAT | 11包：TITLE7、TAI56、FDTXT34、BG56、FDMUS20、FDFIELD99、FDOTHER103、FDSHAP66、DATO136、ANI9、FIGANI408 | 目录数不是全解码／像素通过数；另有FDICON.B24、字形／驱动文件。A16、A18 |
| 地图 | 33组、43,734格、622带事件标记格，实际引用189个action槽 | 后两数继承既有结构统计；本轮33组重新读取。动态换图后另核。A07 |
| metadata | turn／lookup／action各528槽；默认turn450、非默认78；stage29的ff5200单列 | 450为拒绝，非默认集合含callback82，不等于所有事件动态覆盖。A03、A05 |
| 模板／位置 | 1,899模板＝1,887有效＋12未消费尾；2,322 placement | 0..29有效模板1,826条；全部有效模板中stage32两条特殊ID48／66已有alias，不是缺32..67全部人物。A04 |
| shape／文本 | 66目录＝33帧包＋33属性表；8,256帧、9,366属性；34文本entry／1,016片段＝共享661＋其余355 | 目录／已解引用闭合；全部RLE、token、glyph、portrait正式反馈未由此证明。A14、A16 |
| 五分派表 | result30／18目标，callback90／90，entry30／28，victory30／30，magic28／27；合208槽／193目标／187 owner | 同目标不同参数和caller仍另验；magic是独立表，不能扩callback到120。A05、A15 |
| 移动 | 106指针／106已解码流，350 group、1,045 pair；消费长度和2,896B／字节并集2,894B | 跨目标复用及4尾字节保留，不能把两个长度差当丢失脚本。A04 |
| 审计语义归属 | 1,497来源行＝208dispatch＋1,171引用＋106movement＋12尾；20 reviewed语义族、19结构source family | 归属完备不等于每族全部有效域已验。A01、A05 |
| 正式入口 | 0..29共30 battle capability均enabled；准备1..29，0／30无正式准备入口；31／32开场、30内部 | stage2..29的163来源绑定／149目标、112 callback来源引用沿S5账；这些不是关卡通过率 |

本轮来源账恰好闭合：72,470＝69,901 confirmed＋450 rejected＋2,077 excluded＋42 unresolved。1,171个保守需求＝712 supported_definition＋456 unmapped_semantics＋3 decoded_unopened。**这些标签不能当新增功能缺陷数。**

39422ae审查时的42项拆分为：8个资源记录（stage1未被地图引用的action12；stage31四turn、未引用action12及两drop）＋6个callback分派槽58／85..89＋28个独立magic槽。后34项的分类器未识别现有非stage alias消费者／source-only；callback58已有完整来源图，85..89有已解no-op共享定义。456＝450默认255＋stage31四turn／两drop；3个decoded_unopened均属stage31。stage31是开场专用资源，不能据此宣布“缺第31关战斗”。F1后来已用独立原caller／writer proof和正式consumer join闭合这42项，来源09当前视图为69,935 confirmed／450 rejected／2,085 excluded／0 unresolved；原历史分类不改，去向闭合也不等于功能／全语义完成。

### 3.2 冷定义与绑定

本轮实际生成：196张表、8个flow域、241个定义图、22,128节点；计入alias为420个runtime flow identity、179条binding。alias／节点／重复引用都不是功能覆盖率。必须结合生产 consumer、源 CFG 和输入域，而不是用生成器输出自证。

| 域 | 定义图 | 含alias identity | 节点 | binding |
|---|---:|---:|---:|---:|
| common_event | 95 | 185 | 2,320 | 90 |
| loot | 2 | 2 | 215 | 0 |
| magic | 6 | 6 | 48 | 0 |
| preparation | 3 | 9 | 338 | 6 |
| progress | 6 | 6 | 101 | 0 |
| runtime | 7 | 7 | 381 | 0 |
| scene | 8 | 8 | 104 | 0 |
| stage | 114 | 197 | 18,621 | 83 |

操作是否有正式consumer、参数域是否完整、bind别名是否来源相同、分支是否动态覆盖，分别建账。下阶段以现有ledger的source_id、flow ID、dispatch VA为主键扩充，不再建立平行关卡清单或另一份生产数据。

### 3.3 已闭合项，不再列作功能待办

| 旧线索 | 当前结论与可复核入口 |
|---|---|
| S5 13隔离槽／14引用、stage20以后禁用 | 已闭合；当前stage2..29绑定及`fd2_field_stage_is_supported`／battle capability、S5最终source／scope／disposition相符 |
| callback51别名、source等待1000ms、text owner、NoTown白DAC | 已修；`common.callback.051`、source等待0..65535、借原text view、`notown.normal`及C22回归／原生资格见[S5报告](s5-batch-expansion.md) |
| 特殊战斗HP不确定／固定一轮方案 | 已撤回；自然caller末写DWORD0xAF8C0、marker低WORD63680已证，仅限对应20组caller；不推广为通用初值 |
| 原结果仅看HP、Load改写result | S0已修；205b4／206c5等按hidden／side，10010恢复不读写53ECC；剩余是完整caller组合证据A08 |
| FIFO不保队／跨页面全清、水色LUT／嘴型／fade仍旧近似 | 已有G／S4修正和对应测试；现有有限读写集保持。剩余是typematic／时钟、全部caller相位、全帧组合证据A10、A13、A14 |
| stage32 ID48／66、31人credits、30内部尾声 | 当前已实现，不增造普通battle30；外来32人credits需独立判域，不标正常招募未实现 |
| 缺5首音乐 | 20entry实际为15首XMIDI＋5哨兵（0／2／5／7／9），现15首golden与ID闭合；不补造哨兵曲目 |
| LE fixup模型仍不可信 | 静态独立复核已通过；A20仅针对未闭合的全对象运行时／BSS，不能混为loader语义失败 |

## 4. 差异与证据矩阵

稳定ID为A01—A22。分类为「差异」（当前可确认实现或表达不同）、「证据」（已有实现但所需证据不足）、「未审计」（全域尚未封闭）和「例外」。一项可含多类，不能把缺证推成错误。严重度衡量对新终态的影响；实施顺序和依赖只查总计划F1—F6。下文每项包括来源、当前域、影响和关闭标准。

| ID | 主题 | 分类 | 严重度 | 归属 |
|---|---|---|---|---|
| A01 | 审计器当前消费者／历史分类失配 | 来源分账已修，待F1整阶段签核 | 高 | F1 |
| A02 | 参照全集及运行配置身份 | 证据 | 高 | F1 |
| A03 | 默认turn255及全部有源拒绝 | 未审计／拒绝差异 | 高 | F1 |
| A04 | raw／人物／移动／容量与构造写集 | 差异／未审计 | 高 | F1、F3 |
| A05 | 完整CFG／ABI／page／mask／绑定 | 证据／未审计 | 高 | F1 |
| A06 | 全剧情、隐藏、结局与自然前提 | 证据 | 高 | F3 |
| A07 | 动态地图／身份／人数组合 | 证据／未审计 | 高 | F3 |
| A08 | HP／hidden／结果及调用续点 | 证据 | 高 | F3 |
| A09 | SAV兼容及错误／取消／重载owner | 写集差异／证据；回合门已判正常域 | 高 | F3 |
| A10 | FIFO／typematic／确认／等待相位 | 已知近似／证据 | 高 | F2 |
| A11 | 启动RNG来源和预推进 | 初始化／调用序差异 | 高 | F2 |
| A12 | 全共享RNG调用及真实写序 | 证据 | 高 | F2 |
| A13 | BIOS／PIC／帧与绝对时间 | 差异／证据 | 高 | F2 |
| A14 | 全像素、底图／palette／水色／嘴型 | 证据 | 高 | F4 |
| A15 | 各动作FULL／SHORT、全部目标FX | 有限域差异／证据／未审计 | 高 | F4 |
| A16 | 全资源解码／文本／字形消费者 | 证据／未审计 | 中 | F4 |
| A17 | 全曲／事件／寄存器及音频时序 | 证据／已有时序不符 | 高 | F5 |
| A18 | 全PCM／cue／混音／波形 | 证据 | 高 | F5 |
| A19 | 设备退化／路由／停止／清理 | 证据／宿主适配 | 中 | F5 |
| A20 | 全对象运行时BSS／未知读者 | L0严格门通过；L1／未知读者另账 | 高 | F1 |
| A21 | Windows全历史／完整完成声明 | 证据 | 高 | F6 |
| A22 | Nuked响度／音色 | 用户选择的例外 | 中 | F5 |

### A01：让审计表达跟上真实消费者

定位：`tools/static_data/stage_source_ledger.py::normalize`、`audit_stage_data.py`、`tools/tests/test_stage_source_ledger.py:79`、`data/static/flows/stage.json`及`common_event.json`的source.evidence。原来源为五分派表、已发布函数登记及S5 overlay。

39422ae原审查复现：stage23 result已有绑定，单测仍要求unresolved，17项中该项失败；42条unresolved中34条不能作为未实现。stage顶层来源说明仍混有candidate03“undefined HP／no shipping binding”等旧措辞，而最终S5已闭合。影响是误开旧阶段、错判功能缺口。依赖A02；验收以原槽／完整来源、正式consumer join和反例测试为准。旧标签保历史身份，新增当前视图；所有剩余行有可达／排除／拒绝／未决去向，分类数闭合，不能仅修改expected求绿。

F1更新：上述为39422ae审查时复现，现已修正。历史72,470行及69901／450／2077／42保留，当前consumer视图69935／450／2085／0需显式独立proof；另8条由原开场caller与完整map writer闭包排除，不因零未决宣称全语义对齐。修正S3测试夹具误混S5及callback51完整caller门，缺proof／协同回退负例仍拒绝。当前收据见[F1报告](f1-reference-domain.md#2-来源和本地证据身份)。

### A02：固定可重复的参照全集

定位：`data/static/manifest.json::sources`、`tools/static_data/audit_stage_data.py::SOURCE_HASHES`、S5 local-integration与源守卫。现有四核心摘要固定，多数历史证据各自冻结；完整发行来源／统一设备配置仍需整理。

影响：不同DOSBox构建／cycles、声卡驱动或字体可能使时序／音色比较无同一对象。无前置依赖；用全运行文件白名单摘要、DOSBox EXE／配置／驱动摘要和证据来源清单作为夹具。通过要求缺文件／换身份必拒绝，用户数据与运行资源分账；不以本地证据目录仍存在替代逐引用核验。

F1更新：[参照身份](../formats/original-reference.md)已冻结20文件＋字库，并查明即使传`-conf`仍先读版本化userconf的配置链。原发行来源未知，不能从哈希推定零售标签；完整配置派生值、实际启动argv与动态driver／硬件测量仍分层登记。

### A03：默认turn255和其它有源拒绝

定位：`field_event.c` turn/callback查询、`field_game.c` phase预检、`audit_stage_data.py`默认项、原`field_turn_event_check @0x1a813`和callback表`0x51b91`。当前450条ff ff 00有源拒绝（战场0..29内406条；stage2..29内379条），而原turn为DWORD，不能先假设永不到255。

原审查尚未确认这些正常长回合命中后的实际call target／第一处U边界；不新增callback255，也不把已知ff5200 callback82算进450。影响是正常长局可能被安全门截断，或原程序本身进入U域。依赖A02、A20；夹具为turn254→255→256、各phase／重复触发、32位边界及合法callback反例，先局部原码和真实内存目标，再最小原版代表。每条拒绝须有V实现或U证明；确认U后有效前缀仍要等值，最终声明须披露正常可达U出口。

F1更新：完整原1a813的12个turn／phase向量已核默认255读取`51f8d`的`00050302`后尝试取指`02030500`；无fixup、无FD2对象代码target。另以全部turn writer、phase0支配增量点的完整原CFG、30战场不可改默认slot15及Load BYTE来源作结构归纳，证明正常V前缀不能返回并增长至256，故Save>255门不是正常V差异；这项证明独立于旧支持门，随完整F1门统一签核。续审一度将stage27 callback66误拼到35677，经原分派表独立复核，66实为35d1e、53才为35677；该假线索已剔除，不作为回合判域新阻塞。整局自然到255尚未运行，先前合法callback必须保有效前缀；stage8／29已有原码和实际C前缀回归，不能新增callback255 no-op或提前吞掉早slot。

### A04：从正常来源证明人物和容量边界

定位：`field_unit_base.c`、`field_unit.c`、`field_event.c`移动、`field_game.c`能力／资源预检、`scene_cinematic.c`credits；原`0x627d8`106移动指针、模板构造／roster读者，来源详见[来源账](../formats/stage-source-ledger.md)。当前active容量96、SAV／persistent32、已证招募上界31，不等于所有人数／身份组合已验。

需要逐消费者核raw0..95、移动raw83／direction5、模板有效前缀、人物alias、技能学习／转职、库存／资源引用；明确哪些是V、哪些仅外来档可造。不能把ID32..67整段泛称未实现。依赖A01、A05；夹具从原模板／招募／转职／学习闭包生成，边界与无写入反例用实际C和局部原码比较。成功标准是每个拒绝能解释到源读者，正常可达组合不被保守预检误拒，外来32人credits不伪造raw31。

另有已确认的构造写集差异：`field_unit_base.c:199::fd2_field_unit_stage_template_apply`先全80B清零；原`field_unit_stage_template_append @0x10c50`以原active槽为底，仅写指定字段。fresh未写9B、reused槽和heap尾的全游戏读者未封闭，**玩法可观察影响尚未证明**。F1建原逐字段写掩码／来源／读者，F3依据真实fresh与reused preload修正，不凭stage0捕获常量补未知。

F1更新：全部1,887有效模板×两种preload及32个持久构造已取原写集。第29关两个drop81同phase合并使state16=2，原82只append group2、count27→29，再传送raw22／29／30；完整22253对inactive29／30零目标字段读取、仅写xy，仍在原96槽allocation内，属于有效写域。当前窄修复仅放宽该显式五参写址，保其余78B、count及普通active读取门；原码／C、fresh03分层门及正式pair04均通过。37／66的提前拒绝也已修，source writer闭包在首次无producer身份读前保有效前缀；75当前实现已等值，不重复修复。自然早触发、构造未知区及全部玩法闭包仍须分账，不能由受控向量升级成自然已跑。

原1db65另有实际frame补证：31个合成屏内死者可完整返回，第31个地址槽位于124B局部区的offset120，随后被临时页指针覆盖；12次FX原读取实际消费该临时页base。后续真实1a866／205b4状态caller在32..35仍可返回，不能以savedEBP覆盖普判U；36例在写返回槽后、RET进入工作页前停止。当前C截30锚与这些有限原写集不同；正常同时死者来源、完整renderer和其余caller域仍须分开证明。完整八caller内部复审当时因子模型轮次以`cyber_policy`结束而中止，最后工具成功，不能误称已定位的工具内容拒绝；父会话现以[正常调用合同](f1-defeat-caller-contracts.md)及独立C写集继续复核，不待申诉、不重发失败子任务。八caller来源身份新核通过，C的69个人数／192个筛选组合定向通过，但未直接签正常32+可达或全部ABI。正常参数域仍归F1；帧／临时页写集补完归F4，不将参数反例冒称自然玩法反例。

### A05：全控制流和正式分派的独立闭合

定位：8个冷flow域、`static_flow.h`、`field_common_event_*`、`field_page.*`、`scene_transition.*`、`scene_teleport.*`、Game／Scene walker；原208分派槽及其callee／xref，`24618`、`22253`、`362e8`等已列发布依据。

已有S5完整13来源与有限参数门、82真实Play桥；不是尚缺实现。新目标仍需把全部正常caller的条件、共享尾、宽度、page/mask和前缀失败参数域封闭。依赖A01、A02、A04；用独立原CFG／指令字节生成边清单，再与冷节点／consumer逐边join，ABI向量从真实caller生成。入口相同但参数、时点或写序不同必须另分；成功为已达边／源排除边／U边全账，未知边为零，生成器自检不充当oracle。

F1参数更新：五个22253原caller的focus／camera不变量、first_row及工作页读窗已由独立原源和正式consumer连接，未发现正常参数被现门截断；250cc坏臂单独核布局／移动／对白超集。此项关闭参数包含关系，不签像素／时序。预算结构上界与当前图分母另有独立审查；移动仍须把原固定出阵槽、必经追加和当次初始化字段分开，不能拿可能总人数作读取下界。具体资格与剩余读者查[F1报告](f1-reference-domain.md)。

### A06：全部故事和分支前提

定位：`scene.c`、`scene_cinematic.*`、`field_prepare*`、stage／scene flows；原entry／victory表、26的`250cc`、29的`25757→31529`及31／32开场链。已实现好坏结局、终章和非返回终端；已有自然首关、受控后期正式代表，未证自然全游戏所有分支。

缺口是招募互斥、物品前缀、隐藏条件、重复／一次触发、全部正常卡身份／outer-index组合及20组montage的真实时点覆盖。31是单次正常roster上界；正常招募人物的text_id身份并集为0..31；热unit_id另含转职／原视觉变换域，18／19互斥分支须分别验，不能缩成总共31种卡。依赖A04、A05、A12；以原CFG条件／输入状态做分支夹具，正式App路径验证分支可达、文本owner和完整SAV，不按每关另开开发轮。可复用相同语义证据，仅对新时点／反馈追加代表；未跑全链不得声称全自然通关。

### A07：动态地图、身份和人数

定位：`field_game.c` map replacement／append／rotate、`field_unit.c`、`field_common_event_projection.h`、NoTown与handoff；原`1088d/233c6/24e80`及对应源码头来源。已有换图、增援、真实34active入场和entry29最终27active代表，静态初始图引用校验不覆盖后续组合。

影响可能是动态之后raw、drop、mask或资源域改变；目前是缺证，不宣称已有错误。依赖A04、A05、A12；独立轨迹登记每次地图资源、actor数／身份映射和逐叶写点，边界组合用原码与实际C，代表走正式事件→下一动作→Save／Load。成功要求换图后所有读者和失败前缀有确定来源，不借stage0常量补未知。

### A08：原结果合同与调用续点

定位：`field_round.c`、`field_game.c` result／progress、`field_save.c`、stage result flows；原30槽／18个result目标（含205b4／206c5）、117e7和10010。S0已恢复hidden／side而非HP的判定、result latch和Load不改写；S5特殊HP也已闭合。

剩余是全部阶段和动态身份／主角HP0、hidden、已有result、输入返回时点组合，不能用一个result API通过代正式调用序。依赖A05、A07、A12；原caller返回前后状态／读写轨迹与正式Play测试配对。成功要求每种结果族及不同调用时点的状态、写序、终端完全一致；重复执行和HOST／ERROR不多推进。

F1续审已逐原入口核清18个result目标对incoming保存寄存器的使用，关闭该接口性质，不升级为全result玩法通过。[死亡调用合同](f1-defeat-caller-contracts.md)另给出明确的条件性行为：在前置collector未破坏所需现场时，D32..35的HP尾写可使`1aa1d`提前结束当前掉落队列；victory28的D33+可把非零EDI传到`main@25e8f`标题／退出分支。当前`field_loot_flow.h`／`field_defeat_flow.h`／`app_flow.c`依显式owner推进，并未一般复现这些栈别名传值；正常触发与可观察差异仍待F3核证。collector的K条写入又是独立维度，可能在D较小时就改变保存值，完整Q／K写集仍由F1补证；不能只用死亡人数门或“逻辑条数后即U”消除此问题。

### A09：存档兼容和失败／取消owner

定位：`field_save.c`、`field_prepare.c`、`field_play.c` context、`main.c::run_field_chain`、`app_flow.c`；原存取流程与[session合同](../systems/battle-session.md)。现有格式、未知字节保留、活动／手工槽／酒店Load和失败前缀有多层证据，不重做格式。

剩余是所有有效档状态和跨Scene／Town／战场的失败、取消、反复重载；fresh未写9B、heap尾与原读者未封闭。坏档预检无副作用、缺设备仍运行等属于H扩展，不能称原DOS坏档等价。依赖A04、A07、A08、A20；原→SDL→原／SDL→原→SDL完整容器及未知区逐byte比对，owner故障注入和取消／HOST确认无悬挂。失败前后保已提交前缀，不能整体回滚原已写结果。

F1另定位Load的raw2写集：原10010逐raw调用11019，按当前unit_id重建cache并写record+2；当前`field_unit.c::fd2_field_units_sprite_cache_restore`只重建旁路class，刻意保持80B原记录，`field_save.c`随后导出仍取这些记录。旧视觉映射正确不能代签这个保存写集。F3须以正式可保存的热身份变化／cache重排夹具核原Load→Save并按原写点修正；当前是静态写集差异，不宣称已运行自然原版反例，也不任意重算其它未知字节。

原审查定位的拒绝门是`field_save.c:86::fd2_field_game_export_battle_snapshot`在`turn_number>0xff`时失败；原运行回合是DWORD，保存字段是BYTE。F1已核完整turn写者、phase0支配增回合点、不可改slot15和metadata／Save来源：正常V前缀不能返回并增至256，因此该拒绝不再列正常V差异。不是仅据局部255试验作排除；注入／外来状态另记H。F3仍核正常254／255的完整文件与Load续态，并以256／DWORD边界验证H拒绝无副作用；自然长局到255的输入证据未执行。

### A10：输入重复率与跨框相位

定位：`input.h:19`的500ms／92ms、`input.c`队列与repeat、Play modal／dialog caller；原BIOS FIFO、16c57／clear读者和[输入依据](../systems/input.md)。15有效槽、满队丢新、读一保余键、caller清键已修；本轮input／dialog组件通过。

已确认采用PC/AT固定宿主近似，尚未量出与冻结参照的数值／仲裁是否相同，不能写成已测得500／92ms必然错误；全部确认／等待／跨框相位尚无整轨迹。旧首次END默认NO症状原因未证，后YES成功不抹掉原记录。依赖A02、A13；固定DOSBox配置量测首按／长按／多键／满队，记录原入队、取键、clear及caller时点，SDL正式pump同时间轴复跑。成功是FIFO内容和业务消费逐事件相同；任意按键关闭、非法确认、取消和HOST均有反例。

全键值域还需枚举Ctrl／Alt方向、NumLock／Shift小键盘、非游戏普通键及F1..F12修饰组合，以原INT16的AH／AL返回值和实际caller读取方式为oracle。双held的repeat仲裁及长阻塞恢复时点未被单测自证；Ctrl+C／Pause进程退出、失焦清held属于明确H适配，不混作原键盘语义。

### A11：启动初始化和预推进缺口

定位：`field_play.c:86..103`的`field_rng_initial_seed／context_init`、`field_rng.c`；原main@25bf4、`field_rng_next @0x4ebe3`。SDL未设环境变量固定0x7a18，只seed；原main在25d83读取BIOS46Ch，25d90调用Watcom rand，取模256后25da7循环调用共享RNG，再进入标题。

39422ae审查时Domain只读核完整main711B摘要`be0ce16a0b644231f4c39655270e9a3b95e15bff58df91897cdf7199e78e7e2e`；4ebe3完整28B确认word627B8加9014再ROL3，算法与C一致。**已确认缺原启动推进；未量出rand初态／循环次数与全部启动读者**，不把某次捕获值当通用初值。可改变后续随机轨迹与玩法结果，未声称每次运行的初态值必然不同。保原有符号取余／循环比较；BIOS读与rand来源分别核，不先假定BIOS直接seed rand。依赖A02、A20、A13；冻结冷初态、rand来源、启动tick，原CPU和原版状态捕获对照，再接正式START／标题／Load。保留显式测试seed为调试控制，默认正式启动要还原来源及调用序。

F1前置取证更新：文件初态为Watcom rand DWORD=1、共享RNG WORD=0；完整CRT后及25d83的原版两次实际RAM捕获均保该初态，25db1观察rand `41c67ea6`／RNG `5885`／ESI=EDI=198。原CPU五个BIOS向量独立复现198次推进；BIOS低WORD只写视觉clock，不seed rand。该证据不等于已实现F2，也未证明标题／所有caller共享流与绝对时序；相应默认初始化差异仍待F2修复。

### A12：完整共享RNG和逐写序

定位：`field_rng.c`、App context、`dialog_flow.c`／portrait、Game effect、AI、battle动画及各flow随机节点；原4ebe3全部xref和原调用链。已有局部计数、80个montage RNG、动作与取消测试；不是完整共享流证明。

缺口是等待、眨眼、嘴型、FULL／SHORT、跳过和重载相互交织的调用数、取值用途及写序。依赖A05、A11、A13；原trace按caller／PC／时钟／前后state标记，SDL通过实际owner记录，同一输入和时间基准逐次比较。SAV无需新增不属于原格式的RNG真值；不同端自然实战结果不能直接归因随机数，也不能用强制同seed掩盖漏调用。

### A13：共享时间域与硬件时序

定位：`field_visual.c`、`dialog_flow.h`、`scene.c`、`vga.c`、`field_game.c`、`field_play.c`。55ms换算BIOS tick、110ms等待、菜单16ms和自动行动150ms均有宿主取舍；原BIOS低16位、timer／PIC和无delay菜单循环不等于60Hz宿主帧。

这些是已知近似，不因S4精确DAC修复而消失。影响反馈相位、早键、RNG和音画同步；原A7严格PIC比较仍RED。依赖A02；先还原实际clock读点和wrap／不追赶规则，再用一个原虚拟时钟驱动现有owner，而非第二套热状态或全局休眠补丁。夹具含相邻tick、长停留、低WORD回绕、帧丢失和暂停／失焦；原时间轨迹与SDL正式入口相同，宿主采集延迟单独测量，不能用模糊时间容差通过。

阶段边界：F2关闭共享时间／输入／RNG合同和BGM时间基准接口；SBPRO2／PIC音乐驱动调度及非例外音频端时刻的修正、严格RED关闭归A17／F5。F2不先要求F5完成，F5依赖F2时间基准，避免循环依赖。

### A14：全像素与组合反馈

定位：`vga.c`、`tile.c`、`field_background.c`、`field_visual.c`、`portrait.c`、`dialog_flow.c`、Scene转场和`field_page.*`；原palette／water／actor／text renderer，完整VA见各源头和[共享运行语义](../reverse-engineering/common-runtime-semantics.md)。透明skip与opaque0、逐通道减暗、有限水色和嘴型已有证据。

剩余为完整画面／全部palette相位、透明底图恢复、前后景遮挡、嘴型／回合显示、page／mask组合和所有过渡帧。S5的2,779×64,000B索引帧严格比较只覆盖有限原PC驱动叶，不是cold walker全部RGB；正式WGC只观测有限首末页。依赖A05、A12、A13、A16；全帧索引／DAC及分层原读写集差分，再WGC校验实际显示。相位先对齐，不裁ROI或忽略透明像素求绿；宿主整数缩放必须与320×200逻辑像素分账。

### A15：全部动作和目标FX

定位：`battle_animation.c`、`figani.c`、`magic_fx.c`、`field_effect.c`、attack／item／magic消费链；原物理及magic28槽、FIGANI408项与FULL／SHORT各caller。现有通用动作、派生／复合、dynamic bank及skip提交合同已实现，不重新列为缺功能。

缺证是所有有效caster／目标、marker次数、FULL／SHORT、命中／miss／免疫／死亡、镜像／多目标的FX与调用组合。10个magic bank行含82重复，不能算10种不同bank；AI30组业务不能签目标FX。依赖A04、A05、A12—A14；从原资源和marker建立语义等价类、全参数验证，差异时点挑最小原生代表；MP逐marker、重复轮相位和受门SFX仍按原顺序。成功为全绑定引用均落入有原证的类，新增视觉／调用差异无遗漏。

F1已确认一个有限参数差异：当前`field_defeat.c`截30锚，原1db65第31锚会读取临时页指针；32..35在已取状态caller向量中也可返回。写集、callee替身和剩余八caller／正常同屏参数资格见[F1报告](f1-reference-domain.md)，F4须补相应呈现／有效前缀；不将该参数反例改写为自然同时死亡已证，也不以伪C30槽普判U。

### A16：全资源、文字和字形

定位：`archive.c`、`image.c`、`animation.c`、`text.c`、`font.c`、`portrait.c`、map_sprite／shape／tile，各自原res_load／RLE／text来源见源头。当前目录／格式、许多真实解码和已用引用通过，不是全资源consumer覆盖。

依赖A02、A04、A05；把11DAT、FDICON、字体所有entry按实际读取器分类为正式可达／原未引用／未审计，批量解码并核尺寸、调色、RLE尾和完整token展开；34entry／1,016片段逐consumer验证动态数字／名字／portrait。成功为分母闭合，无靠未引用测试资源扩造功能；原未初始化／坏token第一处U另账，不能统一缺字替换后称一致。

### A17：音乐事件和严格时间

定位：`bgm.cpp`原SBPRO2模型／EVNT调度、`field_audio.c`及原SBPRO2.MDI／AIL调用链，依据[音频来源](../reverse-engineering/audio.md)。15首实际曲目一／两循环原码向量和标题3,748寄存器写序已过，不缺另外5哨兵曲目；严格PIC仍有时序不符。

剩余为全场景调用、voice／controller／loop／fade／stop跨段顺序和绝对时间，不能因Nuked例外免除。依赖A02、A12、A13；离线原driver／PIC事件轨迹和SDL寄存器时间轴逐条比，代表场景使用有共同输入时标的双端采集。成功为非例外的事件、时长、循环和淡变一致，失败报告RED保留到实质关闭。

### A18：完整PCM、cue和混音

定位：`audio_pcm.c`、`audio.c`、`field_audio.c`、`battle_animation.c`／title／animation cue。原40240名义音量表已证101/128，primary／secondary独立owner与replace已实现。FDOTHER的29个嵌套容器共176子entry，其中含图片／商店，不能称176音效。

剩余为实际PCM bank／sample全集、重复实例、相对声道／混合、循环边界及最终preOS输出。依赖A02、A13、A15、A17；按生产PCM读取器分类，原sample字节与原音量／pan／rate语义独立解码。孤立PCM、前混音、增益与夹幅算法须有独立等值依据，再以同时间轴比较原WAV／AppMix。保留Nuked会改变BGM叠加幅度及最终clip点，完整混音不能直接要求逐byte相同；其差异必须可归因`EX-AUDIO-NUKED`，不能包庇PCM或事件误差。不调整用户混音器，也不以送零设备probe证明可听输出。

当前PCM线性插值及float混音／夹幅（`audio_pcm.c:57..78`、`audio.c:190..218`）是现实现实，尚未取得完整原采样／混合oracle判定其误差；不擅自登记为用户已接受差异。固定bank31／77／78／80／95、物理48..53、teleport81和法术82／82／83..90只是已知绑定分母的入口，不代签全部样本与重复cue。

### A19：Windows设备和清理

定位：`audio.c`、`audio_pcm.c`、`bgm.cpp`、A8 probe及App销毁路径；这是H宿主域，原DOS不提供WASAPI拔插参照。当前null／discard-null、Zombie继续回调、source退休和首错已有实现／局部设备证据。

缺证为真实路由变化／物理断连、前台到失焦事件、关闭／取消与后台并发；不自动增加设备选择UI／自动重开等无关功能。依赖A09、A17、A18；先用可控错误测试owner／队列有界性，再在明确不改全局设置的条件下取进程局部真实流证据。成功是业务／RNG不因设备可用性变化，source只退役一次、无残窗／子进程；设备可听与preOS分层报告。

### A20：严格运行时BSS与原未知读者

定位：`tools/verify_le_runtime_dump.py`、[LE严格门](../reverse-engineering/le-validation.md)、原对象／627B8等数据源。静态7,959记录／7,948唯一source／11相同重复已复核；旧runtime实际LE入口有21个非零BSS／stack字节，旧严格失败保留。

不得以“loader栈”解释直接豁免，也不得把不匹配镜像升级为权威。依赖A02；每次新取selector／object base／relocation delta，在进入逻辑前获取全部virtual size、BSS及每条relocation source，按规范逐byte比较，失败保原件。fresh9B／heap尾如被正常读者使用须追真实写源；字节门通过也不等于玩法一致。无法闭合则相关原初态／严格运行时声明继续阻塞。

F1更新：正式04的L0全对象严格通过，L1对旧静态零模型仍为21B RED，原件保留。后续把有界LOGL折回现有入口，新正式startup01／Domain六窗／layered三份同轮收据闭合：从L0独立预测66B启动写入，全部293,547B／7,959记录／7,948唯一fixup source相同、零mask／豁免。111来源守卫、采样及正常owned清理通过；没有拿L1差异值作expected或将整个栈域排除。该层只签固定Windows参照及本次LE入口，不代全RAM、玩法／malloc初值、其它unknown读者或整个F1；具体身份见[F1报告](f1-reference-domain.md#a20当前正式工具的分层资格)。

### A21：全量复审及Windows完成门

定位：CMake／CTest注册、全部正式App入口、[GUI规范](../tools/gui-testing.md)、历史G／S资格矩阵。现S4大阶段回归和S5定向回归都保持原范围；本轮6组件不是完整回归。

依赖F1—F5全部条目；建立source ID→实现／cold consumer→原oracle→当前候选→正式代表的逐项索引。当前Windows适用逻辑、输入、原生、原版、音画、失败／取消／清理均实际注册，缺依赖或skip不计通过。历史只能精确源影响继承；大阶段全面历史复审、修复消冗后再签最终门。无未知语义、必需证据缺失或未决终态；不能按测试数、节点数或少数通关样本宣布完全一致。

### A22：保留的Nuked声音差异

定位：`bgm.cpp`的Nuked后端及[A7既有测量](audio.md)。标题曾观察约2.1倍响度，只有该素材／窗口资格，不是所有曲目通用倍率；原声与合成库音色不同。2026-11-14用户重新选择保留，故不列后端更换／响度拟合实施。

依赖A17、A18，用`EX-AUDIO-NUKED`登记后端版本、例外性质和实际影响层；不得将其扩为PCM、事件或时序豁免。成功条件为可追溯披露而非“原版一致”，最终声音比较将BGM合成输出与其它仍需等值的部分分离；无需新增被用户排除的音色相似度测量。

## 5. 独立基准与复跑设计

### 5.1 五层互补，禁止循环自证

| 层 | 独立expected来源 | actual及作用 | 不可替代项 |
|---|---|---|---|
| 原字节／资源 | EXE、DAT、完整fixup／CFG、真实caller ABI／读写集 | ledger／参数／资源引用，覆盖全量静态分母 | 生成图或自己的输出不能作expected |
| 原局部执行 | 未补业务逻辑的原机器码、真实caller栈／内存／时钟 | 实际C叶／walker的输出、前缀／错误／RNG调用 | CPU替身不签设备、完整App或自然可达 |
| 可控状态轨迹 | 原版可复现输入、冷初态／Save副本、时标和RNG调用轨迹 | SDL正式owner／正式pump，不另建热状态 | 同seed只控一个变量，不能掩盖漏调用 |
| 原版／SDL实机 | 原版DOSBox画面／音频／状态时标 | SDL正式START／CONTINUE／LOAD及场景链 | WGC有限帧不签全部像素；受控注入不称自然全通 |
| 全量覆盖账 | 原资源／分派／语义类／正常分支的独立分母 | 绑定、参数、语义证据与代表的可追溯join | 不把重复引用、测试总数或API成功当功能率 |

全像素必须含索引与DAC；声波必须先隔离不同合成器的已选择例外；延时须区分游戏逻辑时间、原虚拟硬件时间、音频采样时间和WGC／OS采集时间。不同层的差异要解释原因，不能用整体容差抹平。

### 5.2 可复跑的最小组合

全量来源／consumer／参数校验在每个受影响域批量执行。语义族用原CFG分支、整数边界、合法资源和取消／失败向量；同一完整目标、相同caller合同与反馈的多绑定共用一次语义证明。不同ABI、执行时点、反馈、owner或写序才追加最小实机代表。

代表包括：正式启动到首输入／新游戏开场；长按跨确认与END；动作FULL／SHORT＋取消／跳过；动态增援或换图后Save／Load；普通Town／隐藏服务与NoTown；好／坏结局及非返回终端；曲目切换／PCM同时声部／设备失败。每类再从全量分支账挑**实际不同语义**，不按每关重复建runner，也不承诺自动全游戏等价。

本轮已执行命令（Windows PowerShell，项目根目录；首条显式只读主项目资源）：

```powershell
python tools/static_data/stage_source_ledger.py --original E:/Projects/fd2sdl/original_game --output-dir build/alignment-audit-20261114/source-ledger
python tools/static_data/generate.py --output build/alignment-audit-20261114/generated
python -B -m unittest discover -s tools/tests -p test_ida_crosscheck.py
python tools/ida/check_src_addresses.py
ctest --test-dir E:/Projects/fd2sdl/src/build-win -C Debug -R '^(input|dialog_flow|field_rng|field_visual|bgm_native_sequence|audio_pcm)$' --output-on-failure
```

来源单测改从主项目根目录执行，仍有A01的一项真实失败，不在本轮修改实现或断言。未来复跑使用新的输出目录，不能覆盖本轮证据。

本轮文档自检脚本随本地证据封存为`check-review.py`，实际核22个矩阵ID与表／正文对应、6阶段、图／节点／来源计数、13个参照摘要、51个原档守卫及相对基线无新增坏链接／锚点。只检查文档／身份，不证明玩法；结果为`document-check.json`。来源／Domain／组件各自收据由`review-receipt.json`统一引用，不提交完整来源账或媒体。

Linux只保留兼容形式，**本轮未执行**；原资源应置于本机合法只读路径，输出目录必须是新的build子目录：

```bash
python3 tools/static_data/stage_source_ledger.py --original /path/to/original_game --output-dir build/alignment-audit-new/source-ledger
python3 tools/static_data/generate.py --output build/alignment-audit-new/generated
python3 -B -m unittest discover -s tools/tests -p test_ida_crosscheck.py
python3 tools/ida/check_src_addresses.py
```

局部原码／Domain复跑沿[IDA流程](../tools/ida.md)、既有S5脚本与本轮封存查询脚本，不改命名或手改发布物。新的函数语义确认再按命名／发布规范执行，最后刷新SHA256SUMS；本轮仅核当前证据，不刷新清单伪装重验。

## 6. 执行边界与仍需决定的事项

Windows和Nuked选择已解决，不再等待确认。本轮没有需要用户先回答才能落盘的事项。F1冻结现有DOSBox／驱动作为首要可复跑参照；若后续证实同一原版在合法硬件配置下存在多个确定结果而没有明显更优策略，再将具体原证、差异和可行选项交用户选择，不以泛泛“真实性”问题阻塞工作。

本轮及后续定向原生实验须使用WGC HWND、owned后台输入、串行执行、正常关闭后必要的owned终止；不捕全桌面裁切、不抢前台、不动用户混音器。**本新审查不扩张旧G阶段的电源授权**，不修改电源计划／全局设置；显示或采集条件不满足时保留失败和缺证，安排可运行窗口。原资源／用户Save只读，写入隔离副本。

新阶段只测新增、修改和共享代码直接影响路径及对应原版代表；F6统一全面历史审查、复测、修复、精简后最终验收。无未经测量的工期估算；未知范围在F1展开到source ID与读者，不承诺全自动游戏等价。39422ae只交付审查／计划；用户随后已明确授权本会话连续实施F1—F6，当前[F1实施中](f1-reference-domain.md)，每完整阶段提交／本地集成后汇报并继续，不推送远程。
