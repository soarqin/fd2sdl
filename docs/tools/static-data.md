# 冷定义的构建与来源校验

当前内置的已确认冷表位于 `data/static/tables/*.json`，有限事件图位于 `data/static/flows/*.json`；清单与消费契约在 `data/static/manifest.json`。现有 item、magic、unit、movement、stage 等 getter 保留。构建期生成私有 `const` 表；流程节点在 `static_data.c` 中只编译一份，由 Game、Scene、预检和成长预览共用。各 SD 阶段是否通过验收，以[阶段记录](../plans/stage-log.md)为准，不能以生成成功代替。

SD 大阶段当前落地 79 张冷表、四域 30 份流程及 518 个节点、6 个共享绑定。正式 START 的 Game 初始化与 Scene 前态使用同一个 `scene.init.000` 的视图，不能另在 C 中固定初始成员、group 或派生数值。数据修改仍须重新构建；现有已支持组合与尚未解码的后续事件分别登记，不因结构提取成功扩大玩法支持门。

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
python -B -m unittest discover -s tools/tests -p 'test_static*.py'
```

Linux Bash（当前未执行）：

```bash
python3 -B tools/static_data/generate.py --output src/build/generated/static_data --check
python3 -B tools/static_data/verify_unit_tables.py original_game/FD2.EXE
python3 -B tools/static_data/verify_stage_tables.py original_game/FD2.EXE --self-test
python3 -B tools/static_data/verify_movement_scripts.py original_game/FD2.EXE
python3 -B -m unittest discover -s tools/tests -p 'test_static*.py'
```

当前独立读集覆盖单位／成长／学习／物品／装备许可／法术／移动共 8,528 byte，准备／音乐／地形／暴击／脚步共 973 byte，36 条移动脚本共 794 byte，另核 11 项 cue 映射。FX 与战斗演出参数的迁移有原 C blob 身份和既有业务回归，不混入这个原字节统计。

## 事件图的编辑边界

脚本本身是冷数据。触发条件、分支、等待、取消、动作顺序与提示参数均由图定义；库存、选择、完成位、执行游标及单位状态仍由现有 Game／loot／progress／session 持有。SAV 不增加流程版本或执行位置，不序列化中途提示。活动 Load 仍重建原有边界状态。

四个领域共用节点结构，但各自只接受当前已确认的固定操作：

| 文件 | 冷定义范围 | 代码保留的职责 |
|---|---|---|
| `scene.json` | 自然开场、初始化、回合／普通格事件、game over 的呈现顺序 | 资源所有权、逐帧绘制／移动算法与单次副作用 |
| `stage.json` | 支持门、入场、结果查询、胜利、结算、交接及音乐／预检顺序 | 原 session 的建立、替换与错误返回 |
| `loot.json` | 掉落、特殊格、满包换物、取消、text_ready／ACK、奖励／controller 续接 | 库存槽操作、原动作提交与 owner 边界 |
| `progress.json` | 奖励门、成长／学习提示与写点、毒伤／到期处理顺序 | 已确认成长计算、装备重算、RNG 与原单位写入 |

每个 `operations` 条目定义参数名和范围；`flows` 使用稳定字符串 ID，节点用 `op`、有序 `args` 与命名出口 `on` 表达。`entry` 是默认入口，`entries` 仅声明既有 API 的其它入口。`views` 可限定 `logic` 或 `present`；省略则供两端投影。不能把两端各维护一份流程，也不能用一个 native 操作封装完整事件状态机。

同域 `bindings` 把运行入口映射到已有真实流程，例如 `"stage.000.action.000": "event.action.000"`。别名共享生成节点数组，不复制流程；禁止别名链、循环、悬空目标及跨域重名。以后已知组合可增加绑定，仍须经过本关资源／前置校验和既有支持门；同 action 的其它关卡只作为待核实的复用候选，不能自动开放。

例如当前满包换物使用下列真实节点（完整上下文见 `loot.json`）：

```json
{"id":"cell.ask_swap","op":"emit","args":[2,423,1,0,0,0],"on":{"error":"failure","yes":"cell.pick","no":"cell.declined","cancel":"cell.declined"}}
{"id":"cell.pick","op":"emit","args":[3,0,4,0,0,0],"on":{"error":"failure","slot":"cell.valid_slot","cancel":"cell.cancel.delay"}}
```

`emit` 的参数依次为提示种类、fragment、呈现 flags、等待毫秒数、fragment 是否取 descriptor、value 是否取当前 message_number；其参数契约在同文件的 `operations.emit`。YES／NO／取消和选槽后的去向由边决定，槽有效性由下一领域节点检查。原有 ACK、text_ready 和槽输入的类型校验仍由 API 负责；未到写点不得授予物品，重复通知不得再次提交。交换成功仍不置完成位，箱内参数保存换出的物品；取消同样不完成。

Game 先提交逻辑、Scene 再呈现的既有合同保持。notice 只增加呈现所需的旧位置／方向／步态快照，避免重放移动两次；单位与资源 owner 不变。原动作确认、事务校验、存档拒绝中途状态及宿主输入分派属于稳定引擎边界；事件图调用这些边界，不能复制它们，也不能借它们隐藏事件分支。

正式 START 的初始成员、部署槽引用和模板选择也由 `scene.init.000` 共用定义。Game 消费 logic 视图，继续调用原 persistent 创建、active 拷贝和模板构造算法；Scene 的开场前隐藏由单独的 present-only 节点表达。生产代码不再保留旧四人成员、group 顺序、装备或派生属性的固定值断言。默认数值兼容性由独立来源与回归核对，运行时仍拒绝无效模板、精灵、profile 和越界引用。初始化节点缺少 logic 视图或带有 Game 未支持的逻辑操作时，构建校验直接拒绝。

生成器拒绝未知操作、非法参数／出口、悬空引用、不可达节点、没有退出的闭环、缺失的必需流程以及消费 ABI 变化；有意的有界扫描循环在运行时另受步数预算保护。结果查询必须是有显式结果终点的无环只读图。动作执行出错或遇未知操作沿现有失败路径停止，不能把未解码项当空事件成功。

修改数值或边不需要改 C；新增领域语义才需要更新最小原语、消费契约及独立回归。`manifest.json` 的 ABI 摘要固定操作参数合同，出口集合固定执行器接受的端口；它们不是第二份事件流程。原 FDFIELD 触发槽、metadata 与当前库存等引用仍读取现有资源／热值，不再抄成另一张静态触发表。

`required_labels` 还固定 C API 实际查找的外部节点入口。当前 loot 的 preflight.drop、preflight.cell、cell.begin、drop.begin、finish.common 不可仅同步图内引用后随意改名；缺失即构建校验失败。普通内部节点仍可重命名／重排。`entries` 的可达性声明不能代替真实 node.id 的消费合同。

## 批量来源与覆盖审计

`audit_stage_data.py` 只读提取 FDFIELD 的全部组、原始槽／模板／placement、地图实际格引用，以及严格 LE fixup 分发表目标。它校验原文件身份、已发布 IDA 的清单／EXE 身份／函数 chunk 与历史 Git 源 blob；报告保留源跨度、摘要、未知字节和未覆盖项。schema、固定操作 ABI、出口和引用复用生产校验，原始 EXE／DAT 预期从独立读集取得，不取生成 C 作为 expected。

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

第二、三条对当前原资源均应返回非零，并保留阻塞报告。默认 `(255,255,0)` 在当前 C 查询的 turn255／phase0 会命中；尚未证实原版可达性或可忽略性，故逐槽保留并纳入严格阻塞。`bound_definitions_closed` 仅说明排除这些未证默认项后，已列非默认定义／引用集合是否闭合；当前 0／1 为 true，不代表所有可达语义已支持。`--require-supported` 还拒绝默认 turn255 候选、其它未知／未开放语义和引用错误，当前 0..32 均不通过。结构提取始终带 `semantic_complete=false`。stage 2 的 `entry_supported=true` 与 `battle_enabled=false` 分别登记，不否定既有入场、准备和浏览存取。

这里的 `entry_supported` 对应原战场 entry 分发表；stage31／32 没有该表的槽，不用它判定 `scene.new_game` 中已支持的开场段落。开场链及两端实际运行证据另按对应冷图和阶段记录核对。

本来源版本有 99 条有效资源项／33 组，turn、lookup、action 各保留 528 槽，1,899 个模板和 2,322 个 placement。12 条未配对模板保留为 `unresolved_no_fallback`：stage 0／31 的 group10 各一条、stage32 的 group255 十条；不猜最近 placement，已绑定 append_group 真正需要时才构成活动引用错误。

实际事件 callback 表是 `0x51b91` 起 90 项；其后 `0x51cf9/0x51cfd` 为两个值 18 的无 fixup DWORD，`0x51d01` 起是独立的 28 项 magic 表。另三张 result／entry／victory 各 30 项，主要分派共 180 槽；加 magic 共 208 个分派槽，目标可能重复，均有已发布 IDA chunk owner。函数边界／原字节覆盖不证明未知 handler 或 CFG 的语义，不将邻接区域合算为 120 个事件。

移动指针表另按完整106槽／424 byte盘点（`0x627d8..0x62980`），保留 `0x62980..0x634d2` 的2,898 byte原始payload。报告 `movement_pointer_table` 逐槽保存fixup、原DWORD、目标、下一不同目标形成的候选跨度及计数观察；候选跨度不等于已证分配边界。ID24的计数读取跨下一目标、ID84保留4 byte尾、其它项的原始actor字段可到83、direction可为5；均原样保留，不清洗、不判原资源坏。

其中只有原36个ID／794 byte继续通过现有严格runtime验证并接入；另70项只标 `source_structure_only`、`runtime_supported=false`，不自动扩大当前64单位容量或开放玩法。完整指针／payload覆盖不等于106条脚本语义已解码；当前getter支持门、冷表与C执行器保持原范围。

v3 报告保守潜在需求集合共 1,171 条记录引用，含重复：51 条有当前已支持定义，468 条属于已知类型／定义但未开放，652 条未明确绑定或未解码；后者含 450 条默认 turn255 潜在匹配。它们不是 1,171 个不同事件，也不是执行轨迹覆盖率。当前仅 0／1 的 `bound_definitions_closed` 为 true；严格支持集合为空。v2 的 721 条计数保留为排除默认填充的历史范围，不能当作严格全范围通过。已解码引用无错误不等于所有未知 raw 引用均有效。

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

输出分别保留 208 个分发表槽、1,171 条保守引用、106 条脚本和 12 条模板尾。缺少审查、实际 owner／chunk 不符或语义族为未决均不得记为完成；结构数、分类数与运行支持数分别统计。`classification_complete=true` 只表示当前来源全部有归属，`all_game_semantic_complete`、`original_runtime_executed`、`new_gameplay_enabled` 仍为 false；它不能解除任何游戏支持门。新增 byte／CFG／数据语义先更新证据与审查记录，实施时再把真实条件、分支、等待、取消和写点写进唯一冷定义。
