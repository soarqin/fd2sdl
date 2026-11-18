# 冷定义的构建与来源校验

普通冷表和移动脚本位于 `data/static/tables/`，事件图位于 `data/static/flows/`，消费契约在 `data/static/manifest.json`。构建期生成私有 `const` 表。流程节点在 `static_data.c` 中只编译一份，由 Game、Scene、预检和成长预览共用。JSON 字段、索引和改动示例见[静态数据与 Mod 制作](../formats/static-data.md)。

## 构建与修改

正常 CMake 构建自动使用 Python 3.9+ 标准库，在构建目录的 `generated/static_data/` 下生成 `.inc` 文件。运行时使用已编译的数据和所需 DAT 资源。构建需要 JSON 和生成器，离线源码包包含这些材料。命令见[工具速查](README.md)。

修改定义时编辑 JSON。版本 2 精简了构建输入，原版地址、读集与来源身份由源码注释、独立校验工具和逆向文档记录。旧 JSON 中的逐表、逐域 `source` 可在 Git 提交 `7ddd365` 的 `data/static/` 中追溯。原版文件身份见[原版参照身份](../formats/original-reference.md)，历史候选的验收范围见[历史证据索引](../history.md)。

生成器检查重复键与符号、字段、整数范围、普通表尺寸和已解码引用。普通表的 ABI 尺寸变化需要同时审查调用方。移动脚本只检查 ID、字节流结构和流程引用，按实际字节数生成数组。所有输入验证成功后才替换输出。若报多余旧 `.inc`，可使用新的构建目录重新生成。

生成校验接受符合类型和引用合同的有意修改。是否与原版一致由独立校验器按原始读集判断。保留用途未确认的字节。

## 独立校验

从项目根目录执行。原字节和调用常量校验工具从指定 EXE 的严格 LE 页读取独立登记的范围，或比对已审查的调用投影。工具回归可在没有原版文件的环境运行。

Windows PowerShell：

```powershell
python -B tools/static_data/generate.py --output build/windows-test/generated/static_data --check
python -B tools/static_data/verify_unit_tables.py original_game/FD2.EXE
python -B tools/static_data/verify_stage_tables.py original_game/FD2.EXE --self-test
python -B tools/static_data/verify_movement_scripts.py original_game/FD2.EXE
python -B tools/static_data/verify_magic_contracts.py original_game/FD2.EXE --self-test
python -B tools/static_data/verify_runtime_tables.py original_game/FD2.EXE --self-test
python -B tools/static_data/verify_preparation_tables.py original_game/FD2.EXE --self-test
python -B tools/static_data/verify_runtime_flows.py
python -B -m unittest discover -s tools/tests -p 'test_static*.py'
```

Linux Bash：

```bash
python3 -B tools/static_data/generate.py --output build/linux-test/generated/static_data --check
python3 -B tools/static_data/verify_unit_tables.py original_game/FD2.EXE
python3 -B tools/static_data/verify_stage_tables.py original_game/FD2.EXE --self-test
python3 -B tools/static_data/verify_movement_scripts.py original_game/FD2.EXE
python3 -B tools/static_data/verify_magic_contracts.py original_game/FD2.EXE --self-test
python3 -B tools/static_data/verify_runtime_tables.py original_game/FD2.EXE --self-test
python3 -B tools/static_data/verify_preparation_tables.py original_game/FD2.EXE --self-test
python3 -B tools/static_data/verify_runtime_flows.py
python3 -B -m unittest discover -s tools/tests -p 'test_static*.py'
```

当前单位／成长／学习／物品／装备许可／法术／移动独立读集仍为9张原表、8,528 byte；另验ID48／66两份实际构造alias窗，共48 byte，不能混称新增基础表。准备／音乐／地形／暴击／脚步原973 byte及11项cue映射保留原身份。移动脚本已扩大为106条、计数消费总和2,896 byte，独立保留的完整候选payload为2,898 byte；跨流重叠和未消费尾分别保留，不能用两个总数之差猜尾部归属。

地图法术的 4 张 28 槽合同投影分别表达效果别名、费用记录、效果目标上限和费用相对 CAST 的顺序。`verify_magic_contracts.py` 核对 28 个 fixup、原 call／jump 和标量锚点，以及已审查的 wrapper 合同。动作与物品呈现由 `verify_runtime_tables.py` 核对原字节与已审调用常量。`verify_field_background.py` 可用严格重定位镜像与 Unicorn 验证背景分派的局部机器码和调用参数。

## 事件图的编辑边界

脚本本身是冷数据。触发条件、分支、等待、取消、动作顺序与提示参数均由图定义；库存、选择、完成位、执行游标及单位状态仍由现有 Game／loot／progress／session 持有。SAV 不增加流程版本或执行位置，不序列化中途提示。活动 Load 仍重建原有边界状态。

运行期有两处解释同名逻辑叶：Game 内 source 节点与入场／胜利／结算共用的 stage 逻辑。二者对 `branch_any_hidden`（区间下界非负且上界在表内）、`grant_party_item`（0..255）、`next_stage`（1..30）和 `roster_append`（0..31）执行同一参数域检查。越界参数明确失败，不截断成 BYTE 后继续写状态。

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

同域 `bindings` 把入口 ID 映射到已有真实流程，例如 common_event 的 `"stage.000.action.000": "common.legacy.action.000"`。别名共享生成节点数组，不复制流程；禁止别名链、循环、悬空目标及跨域重名。实际运行支持由 caller 和支持门决定；S2 的 `stage.source.*` 同义别名仅供显式来源入口。以后已知组合可增加正式绑定，仍须经过本关资源／前置校验和既有支持门；同 action 的其它关卡只作为待核实的复用候选，不能自动开放。

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

### 执行预算的独立结构审查

`tools/audit_static_flow_budget.py` 直接读取八域唯一 JSON 源及已人工审阅的 C consumer 合同，不导入生成器，也不以生成 C 为 expected。Windows 用 `python -B tools/audit_static_flow_budget.py --output build/f1/static-flow-budget-NEW.json`；Linux 对应 `python3 -B`。输出须为不存在的本地文件，原资源目录拒绝写入。

工具分别核 Scene 的共享调用、cinematic 子调用独立计数并恢复父计数、credits 节点计数后重开 epoch，以及各 Game／准备页 API 的本地 advance、真实读键 epoch 和有界游标。common_event 的身份预检另算全部 pending 分支的累计访问数及 64 槽工作栈，汇合尾部按每条投影重复计费。common_event 的 teleport 叶经 Play 创建新的零初始化 Scene context，因此另有 root 预算／深度；`action_call_full` 也有独立的固定 caller 和本地预算，不能混为 Scene 的深度 8。

对当前源，Scene／stage 全部 122 份真实图（83 绑定、233 个显式／隐式调用点）的每 epoch 上界为 9,293 步、深度为 5；credits 的 32 个真实 epoch 边界间上界为 2,045。八域共 241 图、22,128 节点、179 绑定，仅是结构分母。准备服务／部署的真实输入 epoch 上界为 55／33；loot、reward、phase.status 单次 advance 的保守上界为 5,018／60／10,766。reward／status 资源预检分别最多调用 advance 3,075／673 次；choice 采用没有该步数门的正常输入等待。身份预检累计访问上界为 1,542，pending 上界为 7。这些数值只适用于同一收据的源身份，不是原版语义或功能覆盖率。

`consumer_C_change_requires_manual_requalification=true` 是资格限制：片段存在检查和整文件摘要不会解释任意 C 变更。consumer、循环变量／重置、调用、分支或参数域变化后，须重审实际代码并重新生成收据；未知回边、绕过比较、假 epoch、深度或预算超限均失败。证明只保证已核当前图在正常计数域内不触发这些损坏定义停止门，不签分支正常可达性、领域叶内部等待、时序、输入、资源或原版等价；`whole_F1_complete` 始终为 `false`。

## 共享流程的独立来源

`verify_runtime_flows.py` 读取已固定身份的原 EXE 与严格 LE 页，核对 Round、阵营横幅、恢复、确认和死亡等 caller 的调用及标量锚点。已确认的读集、限制和原码替身见[共享运行语义](../reverse-engineering/common-runtime-semantics.md)。

历史批量转换、去向与冻结候选见[旧工具手册](https://github.com/soarqin/fd2sdl/blob/373e5c0bd21454793a20fc62ecb8addb18055dba/docs/tools/static-data.md)及[历史证据索引](../history.md)。历史探针、收据或生成成功不代表当前候选的原版运行结果。
