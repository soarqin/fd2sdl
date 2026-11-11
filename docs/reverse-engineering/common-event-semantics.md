# G1 通用事件来源分类与执行能力缺口

2026-11-10。本文对应[通用审计](../plans/common-logic-audit.md) C02／C05／C13—C17，登记原来源的语义族、通用能力缺口与关卡实例边界。开发优先级只见[总计划](../plans/development.md)。**来源分类完成不等于事件图全部提取、G2—G4实现完成或原版运行验收通过。**

> 版本口径：§1—6 保留 G1 `5b9619f` 分类时的来源、统计与当时缺口，不作为当前实现状态；其中「仍需 G2／G4」或「本轮未运行」均指该历史分类轮次。G2—G4 的阶段历史见总计划；G5 代码冻结时点的消费者、旧新映射和直接验证范围见 §7；本轮最终统一回归和逐项签核见[审计 §13](../plans/common-logic-audit.md)，通用完成门已按所列来源域通过。§7 的待验描述保留冻结时点身份。

## 1. 方法、来源与输出

基于原 `FD2.EXE`（SHA256 `bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd`）、33组FDFIELD资源、已发布canonical函数／chunk和现有冷定义，复用 `audit_stage_data.py` 的资源身份、LE页／fixup、IDA发布清单、全表范围与引用校验。新分类器 [`audit_common_scope.py`](../../tools/static_data/audit_common_scope.py) 只在其上增加显式审查映射，不替代或改写SD3结构报告。

[`common-scope-review.json`](common-scope-review.json) 是本轮人工审查的输入：按表域＋target VA明确列目标和语义族，保留原EXE／资源／分发表及原函数chunk摘要；没有原始机器码、原始剧本或资源切片。它不会根据stage大小、是否已有flow、函数名、调用次数或未支持状态自动判定「仅缺场景」。新增target、错源、漏审或未确认operand仍输出 `unresolved`，`--require-classified` 失败。命名文字变化不使相同原字节失效；已核共享尾可新增owner，但未经审查的新字节／chunk变化必须重新核实。

本轮除读取权威canonical、主任务对180个stage域槽的根／共享尾与内联写点审查外，还通过本机IDA9.5 Domain／idalib只读独立preparation库，补读全部187个表目标chunk owner及8个共享helper，共195函数、27,165 byte。读集 `build/g1/ida/preparation/scope-root-readset.json` SHA256 `7f5beff4f8c8eb55c95b142608de8c82d1cccda6702d6fbe5957b3066fafd62a`，全部chunk与严格relocated镜像一致。该补读不是自动CFG语义恢复；其中原 `0x34f38` 与 `0x35a0d` 的反编译失败仍保留，不以空伪C作为no-op。`0x34f38`另有完整共享尾依据，见§4。

原字段采用canonical VA定位；运行时selector／object base须在真正DOSBox取证时重新取得，不复用历史线性地址。本轮未运行原机器码或任何DOSBox窗口，未把图形测试未注册／跳过视为通过。

### 1.1 执行和检查

Windows PowerShell：

```powershell
python -B tools/static_data/audit_common_scope.py --original E:/Projects/fd2sdl/original_game --output build/g1/common-scope-ledger.json --require-classified
python -B -m unittest discover -s tools/tests -p test_audit_common_scope.py -v
```

Linux Bash：

```bash
python3 -B tools/static_data/audit_common_scope.py --original original_game --output build/g1/common-scope-ledger.json --require-classified
python3 -B -m unittest discover -s tools/tests -p test_audit_common_scope.py -v
```

本轮Windows分类与12项保护回归通过；Linux未执行。输出留ignored目录，不能覆盖原资源、冷定义或发布文档。分类账保留每个源的稳定ID、canonical source／资源entry+offset、target／关联dispatch源、历史支持状态和当前通用依赖；没有把结果写回原资源或开放任何关卡。

## 2. 分类结果与统计口径

本轮分类账共1,497行：208个分发表槽、1,171条保守潜在引用、106个移动脚本指针槽和12个原模板尾位置问题。逐项ID分别为：

- `dispatch:<table>:<slot>`：result30／callback90／entry30／victory30及独立magic28。208是槽数，不是独立机制数。
- `stage:<id>:turn|lookup|cell|drop:<slot>` 与 `stage:<id>:entry|result|victory`：严格保留SD3的1,171个保守引用，含重复和不可达候选；不能当有效调用总数。
- `movement:<id>`：106个原指针槽，含36现runtime支持和70未开放来源。
- `stage:<id>:template:<index>`：12个原资源长度／有效模板前缀边界。

| 分类 | 行数 | 含义和后续门 |
|---|---:|---|
| `binding_only` | 622 | 在所列语义族范围内可由已确认规则表达，具体编成／触发／文字／顺序／参数留S；不等于场景已解码、已生成flow或所有前置都已验 |
| `common_gap` | 388 | 已识别需要补齐的跨关能力／有效域或实际消费者的共享呈现环境，按族进入G2或G4；一个能力可被许多源引用，不能解读成388个独立功能 |
| `original_out_of_table` | 450 | 原turn255候选将action255当表索引，无合法callback；G4需保持原源并明确安全失败，不发明callback255 |
| `original_default_noop` | 1 | stage14/template33的descriptor mode203在原default分支无业务操作，仍有逐项清键；G2收敛SDL原严格门与有效无操作语义 |
| `existing_supported` | 36 | 已有runtime严格支持移动脚本；不扩大原支持范围或重写历史证据 |

当前 `classification_complete=true` 只说明上述来源全部有经审查的机制归属；报告固定保持 `all_game_semantic_complete=false`、`original_runtime_executed=false`、`new_gameplay_enabled=false`。任何未决源应恢复 `unresolved` 并阻止G1分类门，不能为保住数字改成绑定。G1整体还须核实服务／控制语义、必要代表证据及取证阻塞；不能只凭这张绿色分类账进入G2或宣称通用机制已经完成。

## 3. 已识别的共享规则与通用缺口

| 语义族／原依据 | 当前已有范围 | 应补能力及阶段 | 必须保留的实例边界 |
|---|---|---|---|
| 初始化 `0x205da→0x1088d→0x10b4e/0x10c50` | 原group、模板和持久队伍构造已有当前支持夹具 | G2：metadata+2有效模板数／同索引placement、96 actor域、stage32原真实构造别名，见准备§9／战斗§4 | 地图、group、招募与部署参数留S；不重新造base表 |
| typed单位变更 `0x344f2`、callback内联写、`0x35f10` | 已有set_side、set_ai_target、actor_hidden、普通状态／死亡规则 | G2：有限字段枚举的range／masked AI34低4位、高位保留、AI35/36、flags、状态、HP、身份及重算／败亡顺序 | 不提供任意地址／任意offset写原语；具体raw范围和值留S |
| result与事件谓词 `0x205b4/0x206c5`及其它result根 | 已有side-present、leader-dead、all-hidden和当前结果flow | G4：turn阈值、hot-state、持久text成员、inventory/HP条件和带条件反馈的组合 | 原stage胜败条件仍是S；不能统一用全灭替代 |
| hot-state `0x53ad5`及callback `0x348ea/0x34c7a/0x35123/0x36214`等 | 现session保留完整32byte原cell state；已有低位格完成状态 | G4：原owner内指定字节的有界读写／递增、分支和重复门，不另建平行事件状态 | 16..21等具体字段的剧情用途保持原源，不猜新保存格式 |
| 相对回合调度 `0x34dd0/0x35bee/0x35cf1/0x362c5`等 | 已有原turn/phase查询 | G4：DWORD当前回合+N，按原写点截断到metadata turn BYTE；同槽覆盖和phase顺序 | 哪个slot、N和触发绑定留S；不搭通用调度器／事件总线 |
| metadata／map与完成格 `0x24754/0x35854/0x35a0d` | 已有当前地图资源和箱子地形更新 | G4：原地图／metadata变更和跨资源owner交接、条件格变更、失败边界 | 69/46/47等具体资源索引是S参数，不能把整段本关脚本包成native操作 |
| inventory／roster `0x35123/0x35a0d/0x240fa`等 | 已有招募append、持久同步、已确认loot与Give | G4：事件指定actor的grant/remove、队伍查物／消耗条件、必要离队／同步合同 | 具体ID／奖励／招募顺序留S；准备交易的通用服务归G3 |
| 嵌套loot／callback `0x352ca/0x35346/0x355f0/0x35730` | loot／progress冷图及ACK／取消／action.finish已实现当前范围 | G4：事件内部调用descriptor／物理效果／回调的续态和一次提交；相关战斗尾必需能力随G2交付 | 具体descriptor／对白／战斗目标仍属实例，不能全部改名通用后提前交付整关 |
| 共享场景操作 `0x233c6/0x24618/0x24d22/0x361b0/0x33f78`等 | 已有pan/move/text/fade/brightness/arrival/开场 | G4：有限批量场景单位位置／镜头、转场／滚动／flash／瞬移及原等待与资源生命周期 | 批量坐标、镜头、循环数、顺序属于冷数据；仅补执行能力，固定剧情留S |
| 实际text／pan／move消费者的共享环境 | 原主流程已完成已列运行场景，但嘴型跨框／等待RNG及透明工作页未闭合 | G4：C23／C25共享运行合同。显式补到真正调用这些消费者的已审根，包含3231b已有13像素底图原写入下界；不能因根只含文字／坐标就漏掉通用前置 | 文本、镜头、顺序仍是S实例。`-4/-5/-6`等动态token是否实际使用须按各资源流核对，不从有text调用推断每根都用 |
| 地图法术 `0x51d01`的28槽 | 已有共享effect、部分玩家与AI真实提交 | G2：按[战斗语义](common-battle-semantics.md)的候选、费用、模式／marker、范围、状态和复合族闭合 | 不把邻接magic槽当callback90..117；具体获得／敌人配置留S |

一项源可以同时依赖G2的单位语义和G4的条件／等待；分类账保留全部family和阶段集合。G2当前动作必经的事件尾、范围／notice和呈现能力必须随G2实现，不等到G4再补；同理G3所需交易等待／存档合同随服务闭合。G4收剩余共享流程，不构成隐含依赖环。

`field_actor_range_status_set @0x344f2` 的名称不能读成任意状态：原循环只写 `unit+0x34 = arg | (old & 0xf0)`，其字段是AI behavior raw；其它状态字段必须由各自已确认规则提供。`sub_35F10 @0x35f10` 从给定raw至actor count写HP0再统一败亡整理；不能仅隐藏或跳过死亡／掉落尾。`sub_3623C @0x3623c` 只取一次RNG，以同一值的`%3`和`(value+1)%3`标记两个acted，再按原回合槽写点继续；不能调用两次RNG或选三次。

## 4. 表外、default和共享尾的具体结论

### 4.1 turn255不等于合法callback255

`field_turn_event_check @0x1a813` 在 `0x1a840..0x1a85a` 比较原turn BYTE（零扩展）与DWORD当前回合，phase相符便以action BYTE索引90槽表；没有action255或turn255哨兵门。原整轮路径 `0x1a30b @0x1a5b9` 递增DWORD回合，不截断为BYTE。因此450条 `(255,255,0)`在turn255／phase0有潜在匹配，不能以通常战斗较短忽略。

action255索引读取 `0x51f8d`，原字节解释为 `u32=0x02030500`，无internal offset fixup，也不在canonical代码对象中。它是**原版表外间接调用行为**，不是未解码的合法剧情函数。SDL应保留所有原槽、在相应边界明确拒绝，并给出stage/turn/slot/action诊断；不模拟跳入任意地址、不静默忽略候选、不添加自造handler。该安全边界仍需定向负例和原版局部／运行证据按分层登记，不能反过来把所有未知都判成表外。

### 4.2 descriptor mode203是default无业务操作

唯一原候选为stage14/template33/unit102的descriptor `(203,0)`。`field_loot_descriptors_execute @0x1aa1d` 的switch仅对0／1／2／3处理物品／钱／回调／对白，4..254走default：推进下一descriptor，无对应业务效果，仍在 `0x1ac5c` 清BIOS键。`field_defeated_loot_collect @0x1b6b7` 排除255哨兵；AI collector `0x1b653`只收mode3。不能把203当callback203、未知道具或坏数据任意报错；也不能把255混入同一默认合同。

### 4.3 共享尾与无操作必须看原指令

`0x3642e`为`push4→0x36433 stackcheck→ret`；`0x36439/0x36440/0x36447/0x3644e`先push4再跳同一尾，完整指令确认无业务操作。其具体表槽可按已知no-op组合绑定，不由函数大小推断。

相反，原IDA把 `0x34f38` 主块只登记为5B `push40`，其自然fallthrough `0x34f3d..0x34f74`已经归属 `0x350be` 的共享tail。完整60B读窗SHA256为 `cf7734a4bca09f70340a01c1b01bfc9ae3a80002b7f58c8f1aca7329dffc5ab0`：stackcheck→`append_group(1)`→`text(fragment1)`→ret，与`0x350be`的push40／跳转调用同一尾对应。真实callback槽为32，source VA `0x51c11`；IDA的table-base OFFSET xref不能当作slot0证明。补读证据为 `build/g1/ida/preparation/34f38-boundary.json`。主执行者按共享tail补归属并重新导出，保留350be owner和原字节；不能只在私有报告备注而让发布函数继续缺主体。

独立复审又逐指令核实共享尾的消费者：348bb／349ec／34b07经34885到3488b调用text；35298／353b5／35321及34c52／34fc2进入34f65的text尾；353e7／353fa／3540f／36416经3528d／3528f执行344f2的masked AI写；34b9a在34b6f共享chunk内同时有masked AI、turn谓词及text／pan／move；35833经3571a调用pan。这些源已补相应G2／G4通用依赖。不能仅以catalog的直接callee清单决定共有尾不存在业务写点或必要呈现。

## 5. 移动和12条位置问题的归属

106脚本的语法／mode、原96actor容量、script24的mode80零步／direction5以及script84候选余尾，见[战斗语义](common-battle-semantics.md)。分类器消费逐ID审查记录，不只读取36条runtime支持集。

- 现有36条保持原支持状态，不重做。
- 70条未开放记录中，actor64以上的脚本46／47／48／49／60／66／74／75依赖G2容量域；script24依赖原零步姿态／方向帧能力。其余已确认规则只缺新实例绑定和相应S验证。
- 脚本的原group计数决定消费长度；相邻指针跨度只是候选分配边界。script24消费13B，可跨下一候选指针；script84消费5B，余4B原样保留，不能补造另一group。
- 12条原模板尾的位置问题按[准备语义 §9](common-preparation-semantics.md#9-追加发现原模板计数和-placement-索引合同)分类：stage0/31 index30与stage32 index30..39在原有效模板前缀30之外，但原raw仍保留。它们同时揭示当前SDL按ID/ordinal匹配与原同索引初始化的通用差异，必须G2修复，不能只记12条unused后宣布全域通过。

## 6. 仍需实施和运行验收的门

这张分类账冻结的是「哪个源需要哪个能力」，不预先声称已恢复每个回调的全部冷图。G2—G4按语义族实现时必须继续把原branch／shared tail／return／写点／等待映射到有限节点，逐源断言参数与引用；发现新语义先恢复未决／阻塞，不能在native handler中藏整段流程。

每族至少有成功、无条件满足／无候选、取消、重复触发、资源错误、owner和原字段保留的SDL回归；会影响RNG、费用、死亡／奖励或保存边界的写点需局部原码或代表性原版运行依据。现有stage0/1可作夹具；后期来源只做必要最小取证，不交付整关。

### 6.1 正常来源的法术位图写入域

为核对事件是否引入额外技能，本轮对195根／helper的83个外部callee继续作有界直接调用闭包，补读376个非FLIRT函数，37个FLIRT库入口作为边界停止；间接callback与magic表另由208源表分类覆盖，没有无限扩张到Watcom／AIL驱动内部猜测。全部补读chunk与严格镜像一致，本地 `scope-callee-closure.json` SHA256 `67ecf2532928a0359c63e5770eeace5d5feecf56098d2fe4d72b2dc1b5fc2c75`。这只是候选范围；结论还依据下列owner和实际复制源，不能以搜索不到某个offset代替语义审查。

| 原写入／复制路径 | 位图来源或保留合同 |
|---|---|
| `sub_112A5 @0x112a5` 持久新建 | base row `+8..11`四字节复制到unit `+0x1a..0x1d`，`+0x1e=0`；来源是已核32条原基础记录 |
| `field_unit_stage_template_append @0x10c50` | template `+13..16`四字节复制到unit `+0x1a..0x1d`，`+0x1e=0`；包含stage32 ID48／66时仍从各自原template取法术，而不是从构造别名窗额外取技能 |
| `field_unit_magic_bit_set @0x1d79c` | Domain直接caller唯一为奖励／升级 `0x1e292`，按已核学习表OR技能；实际学习ID最大35。不是事件任意赋位入口 |
| `0x1088d` 部署／`0x11506`持久同步 | 完整80B记录复制后只改部署／状态等明确字段；保留既有5-byte法术，不引入额外技能来源 |
| `0x2aa00→0x2ac7d` 转职 | 消费特殊物品／改职业／增量成长，保留既有位图；不调用技能OR，后续升级才按新职业学习 |
| `0x10010` SAV恢复 | 从原文件恢复整活动／持久记录，是独立兼容输入边界；不能把任意外部SAV置位当正常资源的新技能来源，也不能承诺所有畸形位图均可操作 |

180个stage域源根及相关共享helper的单位内联写归入位置／朝向、flags／side／身份、AI34..36、状态、掉落与HP；未发现其另行赋写法术位图。完整record复制追到上述构造／保存owner；`0x233c6`只做单位位置／朝向及acted／镜头，`0x25089`只清flags和恢复HP／MP。外部函数中同样出现的`+0x1c`写经owner核对属于AIL voice／sequence等结构，不是单位。库存压槽只在`+0x0a..0x19`，descriptor复制是3B到独立loot容器，不能当位图写入。

因此，结合[战斗语义](common-battle-semantics.md)对全部初始、template、学习和合法转职的保守并集计算，可以在**正常原资源／START／升级／合法转职及保存这些状态**的来源域内闭合：法术列表最多12项；ID31和36..39没有正常赋值源。当前`>12`及无record位的拒绝门在此域是容量／无效输入保护，不要求发明分页或开放空名31。该结论不涵盖任意手工修改／外部畸形SAV、未声明的运行时注入或未来新增数据；新来源必须重新核对，不把位图容量40bit误当40种合法法术。

G5再按完整机制清单统一审查和全面回归当前平台已支持历史功能。最终剩余S表只允许具体地图／编成／触发／剧情／确有证据的特例，以及已明确不在领域完整结论内的精度与平台测量；不能留下仍未归属的通用谓词、字段写入、等待／取消或服务。

## 7. G5：既有事件按实际等待续行（代码冻结，最终门待验）

G5 复审核实了旧 `execute_known_turn_event` 的真实顺序缺口：先整段 LOGIC 写 live Game，再由 Scene 重放呈现。action0 在 story11 等待前已追加后半组；action6 在 story4 返回前已写 AI 锚点。正常窗口关闭也会留下尚未到达的后缀，失败时整段 rollback 又会撤销已发生的前缀。

当前冻结代码已把原六个正式绑定迁入既有 `common_event`／`event_resume`，删除旧 walker、Game 资源 clone／整段 rollback、运动 snapshot 和 Scene 整事件重放。`waiting_legacy`、只为旧重放续行的公共 resume、无 caller 恒拒资源壳以及 `FIELD_EVENT` notice 也已删除。notice 只保来源诊断；人工注入的坏 deferred 状态明确拒绝，不等待不存在的 producer。业务、RNG、SAV和资源 owner 不变；没有第二事件热游标、业务 Game 或通用 VM，复用既有 Scene／runtime 有限呈现器。

### 7.1 原绑定与冷定义映射

| 原正式身份 | 原 Scene flow | 当前 common flow | 原 caller |
|---|---|---|---|
| `stage.000.action.000` | `event.action.000` | `common.legacy.action.000` | `34531` |
| `stage.000.action.001` | `event.action.001` | `common.legacy.action.001` | `3460b` |
| `stage.000.action.002` | `event.action.002` | `common.legacy.action.002` | `34673`，含共享尾 `34663` |
| `stage.000.action.003` | `event.action.003` | `common.legacy.action.003` | `346cd` |
| `stage.000.action.004` | `event.action.004` | `common.legacy.action.004` | `34738` |
| `stage.001.action.006` | `event.action.006` | `common.legacy.action.006` | `34778` |

没有新增 stage／callback 身份或开放 S。原 G4 的15个规则／片段夹具仍保各自来源有效域；21个 common flow 不代表21个完整关卡事件。`common39` 原生夹具仍是非 shipping 显式 alias，不计作自然触发绑定。

### 7.2 预检保证与限制

`common_event_preflight_identity` 只识别已列有限操作组成的线性 NEXT／ACK 图。它始终借局部 units 和最多32条 roster 投影身份；即使未注册视觉 `event_preflight`，也消费真实 counted MOVE 语法、检查未来 raw、SIDE 写域及 AI anchor 范围。真实 caller 的 pending 和进入 side1 的 turn 都调用同一预检，后者按原 `1a813` 传 raw0。上述线性身份／raw的坏初始引用在扣 MP、耗库存和取 RNG 前拒绝；合法 known callback 不受诊断日志已满限制。

投影不复制整 Game、不执行装备派生、不取 RNG，也不把未来位置／身份交给 live 业务。原 DWORD group 与模板 BYTE 零扩展比较，高值是无匹配，不截成 group0。视觉 callback 可借只读 view 检查该线性身份前缀对应的文字、portrait 和 movement 资源；实际执行到叶时仍重验当前资源，晚错保已提交前缀。

这个保证**不覆盖任意分支组合的精确未来身份**。未被线性白名单识别的图仍使用既有有限谓词／业务后两候选边资源检查；例如 `common.key.consume` 的条件消耗、追加和对白组合，不因六绑定迁移就获得全部分支身份精确投影证明。当前原资源、正式入口、有限机制测试和晚重验分别保各自有效域；不能承诺所有损坏未来资源都在业务首写前被发现，也不能把未知组合自动列为 S 或宣称已验。

### 7.3 唯一 PC、忙态与一次提交

Game 只在自己的原写点提交；Scene 的 PAN／MOVE 在实际发生的格／帧把 motion 交回同一 Game。每个呈现叶成功返回后才按 NEXT 或 ACK 推进父 PC。原16槽 turn 表逐槽重读 live metadata，callback 返回后从下一槽继续；玩家、AI、March 和 loot 保真实 caller，不保存独立 pending 副本。

子 loot 只占既有 loot owner 的深度1槽。子返回后才推进父 ACK 和 descriptor，不能借父 kind5 重跑刷新／奖励。HOST 保原叶 PC 和已写前缀，并由现 `runtime_host_quit` 阻止后续回复／续行；真实 ERROR 才走错误边。短寿命视觉资源在 finally 释放，清理不补写未达到的 INFO、acted 或控制器尾。已结束流程的重复回复、错误端口和二次 child 压入明确拒绝；连续不同等待仍可消费各自独立的已排键。

event、loot、progress、detail、controller 和 host 忙态仍禁止活动／手工 Save。成功 Load 清本来不入 SAV 的瞬时续态，保原 App input／视觉与借用 RNG 合同；没有新增中途存档格式。孤立 `child.active && !parent.active && !failure && !host` 无正常公开写入路径，人工破坏该不变量不属于新的中途 Save 能力。

### 7.4 独立原 caller 与等待来源

[`verify_fd2_legacy_events.py`](../../tools/ida/verify_fd2_legacy_events.py) 实际执行六个完整原 caller 和 `134e4` 本体，固定 strict relocated 镜像／完整函数摘要、原90分发表的六个指针及 FDFIELD 来源。构造、group、arrival、motion、text、BIOS、delay 是显式替身；原直接 SIDE、AI 成对字节和方向写点由 CPU 真正执行。它不证明完整派生、文字 RNG、声音、像素或原生窗口。

| 原函数 | 原 CPU 确认的关键分界 |
|---|---|
| `34531` | roster1／group3→pan／refresh／delay100／move7／clear→story11：count13；story11 返回后 display0／group7／refresh／delay100／move8／clear→story3：count14；末方向 reset→20 ms |
| `3460b／34673` | pan→32999 group4／5→outer clear／refresh→move3／4→reset／20 ms→story4／5；34673 的共享尾实际调用 story5 |
| `346cd` | pan→临时 force group6→refresh／move6→reset／20 ms→clear→story6 |
| `34738` | raw13 SIDE byte 先写1，再 story7；不新增持久玩家 |
| `34778` | pan／delay100→force group3→move13／delay200→story4：count27、AI仍为旧值；对白返回后，raw5..10 各先写 `+0x35=26` 再 `+0x36=15` |

`134e4` 完整46 B 仅逐 raw 写 direction(+3)=0，再于 `13507` push20／`13509` 调 `3790a`，不改 frame_phase、walk_frames、flags，不取 RNG、推进水色或清键。六个迁移 flow 中 action0—3 各有一次，action4／6不添加该调用。当前冷图保真实 caller 次数：裸 `35730→2e2b0` 无 reset；玩家／AI FULL 外围一次，SHORT 两次；`15055` 内部仅 reset→20 ms，外层 `13a9f` 是另一次 acted 后 reset→20 ms→`11cac(0)`，不能把附近的刷新误归内部尾。FULL／SHORT 的一次／两次只指 `18d8c／1548e` 外围；AI 随后 `13a9f` 的 common 尾另计，不是整个 AI 动作的总次数。

`18890` 仅非零 path 后 reset→20 ms；March `16f55` 在事件返回后、acted 前等待。stage1 入场两处、stage2 入场一处是 present-only 20 ms，逻辑资源准备不 sleep；自然开场已有20 ms不重复补。方向来源工具 [`verify_fd2_direction_callers.py`](../../tools/ida/verify_fd2_direction_callers.py) 核20个 parent、23个静态 callsite、23个完整函数与4个共享尾窗口；这些数不是23次实际运行。原12项方向负例加11项 runtime 测试已通过，不能代替 C adapter 或原生证明。

arrival `32999` 使用同一短寿命视觉 scope：prepare／snapshot→Game append→`runtime.arrival` 53叶→close。每帧 restore＋LMI→project，6／7／8帧再按源 postbase，随后 clear→wait1；空组也有12轮。不新建持久 PC／owner，不在尾部追加无源刷新。该函数独立的正常／空组原调用与冷序比较见 [`verify_fd2_arrival_owner.py`](../../tools/ida/verify_fd2_arrival_owner.py) 及 `build/g5/arrival-cold-original.json`；六 caller CPU 中的 `32999` 是声明的替身，不将两份证据混为完整像素／声音验证。

### 7.5 已执行验证与剩余门

本轮 Windows 证据分层如下，Linux 未执行：

| 层次 | 实际结果与有效域 |
|---|---|
| 原 CPU | `build/g5/legacy-original-cpu.json` 六 caller 通过；替身和真实写点范围见 §7.4。13项来源／契约测试通过 |
| 静态来源／冷合同 | `direction-callers-source.json` 的20 parent／23 callsite 与冷图一致；`direction-runtime-python.log` 的12新方向＋11原 runtime 测试通过。不是 CPU／像素／PCM／原生执行 |
| 同源旧红／新绿 | G4 `4ada76b` 只读头／已建库上，合法前置后旧实值 active0／side1／count14／failure0，目标断言失败；新 `legacy-green-ctest-13.log` 通过。同一测试 SHA256 `eb283f9dc1f8ab098dce48cf18c2d508341bcaf61b249fa408c81844c4281f36`，旧误工厂／构建失败保留且不计目标红 |
| IDA 证据发布 | `build/g5/ida/legacy-publication/receipt.json` 实际372项、0重命名／2注释增量，21 strict／14发布测试／最后11项SHA校验通过；原反编译失败保留。发布校验不增加行为或原生证明 |
| 当前无窗口 CTest | `legacy-all-ctest-16.log` 实际112项中109通过／3失败；同生产候选的 `remaining-three-ctest-17.log` 闭合 failure／Item 两项，`ai-tail-ctest-18.log` 闭合 AI。属于分段109＋2＋1，不称一次112全绿 |
| Core＋Play 前缀 | `field_progress_play` 真 shipping0／6、资源预检与正式 Scene PAN／MOVE；仅 STORY 叶注入 HOST／ERROR，核 count13／27、AI旧锚、完整 units／camera、忙态拒导档和重复回复无写。fast、未播放文字不作文字 RNG 或原生帧证明 |
| 最终原生门 | 最终候选的自然0／stage1-turn3／B7、准备服务／跨场景和完整历史回归仍由主线程登记；当前不预填通过 |

真实坏初始 raw／部署由纯 Core 预检拒绝；晚错回归改为首 RNG 后才破坏后续绑定，继续核成本、伤害、acted、EX、库存与 RNG 前缀。旧回调4测试已改为真实 STORY7／父 ACK，dispatcher 夹具在真实 hook 注册成功后才投递有限叶，不以放松生产 busy 使旧夹具通过。

当前只关闭已列实现与直接验证范围。GUI 全历史、最终 Python／来源检查、28项矩阵逐项审查及阶段集成仍是 G5 完成门；不能把上述分段结果提升成通用阶段已完成，也不自动启动 S。

## 8. G5 统一回归与当前结论（2026-11-11）

§7 保留实现冻结时的直接证据。随后完整候选已完成当前 Windows 所列历史回归：自然0／stage1及B7交接、准备服务和跨场景、共享条件／等待／取消及 child39 代表均按各自正式／夹具身份验收，最终来源和失败见[审计 §13](../plans/common-logic-audit.md)与[阶段记录](../plans/stage-log.md)。六个原正式绑定继续由同一 event_resume 逐叶推进，旧 whole LOGIC 预提交、回滚和 Scene 重放不再驱动已知事件。

当前来源域未发现尚未实现的已证通用事件规则。15 份机制图不等于15个自然绑定；未来非线性身份／资源组合仍须按真实使用补取证与预检。新关实例或真正私有剧情留 S，发现新通用语义则先补机制；S 未启动。
