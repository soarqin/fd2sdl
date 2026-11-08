# 冷定义的构建与来源校验

当前内置的已确认冷表位于 `data/static/tables/*.json`，有限事件图位于 `data/static/flows/*.json`；清单与消费契约在 `data/static/manifest.json`。现有 item、magic、unit、movement、stage 等 getter 保留。构建期生成私有 `const` 表；流程节点在 `static_data.c` 中只编译一份，由 Game、Scene、预检和成长预览共用。各 SD 阶段是否通过验收，以[阶段记录](../plans/stage-log.md)为准，不能以生成成功代替。

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

生成器拒绝未知操作、非法参数／出口、悬空引用、不可达节点、没有退出的闭环、缺失的必需流程以及消费 ABI 变化；有意的有界扫描循环在运行时另受步数预算保护。结果查询必须是有显式结果终点的无环只读图。动作执行出错或遇未知操作沿现有失败路径停止，不能把未解码项当空事件成功。

修改数值或边不需要改 C；新增领域语义才需要更新最小原语、消费契约及独立回归。`manifest.json` 的 ABI 摘要固定操作参数合同，出口集合固定执行器接受的端口；它们不是第二份事件流程。原 FDFIELD 触发槽、metadata 与当前库存等引用仍读取现有资源／热值，不再抄成另一张静态触发表。
