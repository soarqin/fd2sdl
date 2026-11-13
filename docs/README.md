# 文档索引

构建和使用见[项目 README](../README.md)，Agent 约束见 [AGENTS.md](../AGENTS.md)。本目录按用途组织技术资料；计划、原版证据、实现设计和工具操作各自维护，避免复制结论。

## 开发计划

- [总计划](plans/development.md)：下一执行阶段、**唯一当前待办顺序**、补充阶段、状态依据及测试门。
- [已完成阶段归档](plans/completed-stages.md)：B／A／R／SD／G／S完成摘要、提交、长历史和收据索引；不维护当前优先级。
- [通用逻辑完整性审计](plans/common-logic-audit.md)：源码／证据矩阵、通用与关卡边界、缺口分类、新旧工作项映射及进入关卡阶段的完成门；顺序仍以总计划为准。
- [静态数据层与事件流程](plans/static-data.md)：已完成的 SD1—SD3 调研与工作包；覆盖冷定义、可修改的事件分支与编排、提取／验证和存档边界。
- [关卡批量读取与转换调研](plans/stage-batch.md)：全量来源盘点、真实转换样例、能力上限、可复跑研究；研究候选不等于当前阶段实施，状态只查总计划。
- [S2共享语义报告](plans/s2-shared-semantics.md)：冻结来源图、有限能力和隔离依赖，保留当时机制证据。
- [S3批量正式接入报告](plans/s3-batch-integration.md)：实际批次／绑定、共享修正、五层验收与未开放原因；当前状态只查总计划。
- [S4统一审查报告](plans/s4-unified-review.md)：旧调色调用修正、冻结支持域、全历史门的实际运行／严格继承和残余边界；当前状态只查总计划。
- [战场工作包](plans/battlefield.md)：B0～B7 的范围和验收。
- [音频工作包](plans/audio.md)：A0～A8 已有支持范围和证据边界；后续随通用机制与场景补证。

## 系统设计

- [战场 session](systems/battle-session.md)：状态／资源 owner、动作与 RNG 提交、必需事件、结果、存档和跨关交接。
- [输入](systems/input.md)：原版键值语义、逐呈现帧采样和 SDL 宿主边界。

## 数据格式

- [数据格式规范](formats/data-formats.md)：归档、RLE、战场、存档、动画、音频及地图帧缓存。
- [关卡来源账与批量夹具](formats/stage-source-ledger.md)：S1 固定 schema、原字节／caller／引用、全量去向、异常队列与复跑入口；不是 App 数据或玩法脚本。
- [文字映射](formats/font-text-mapping.md)：FDTXT token、Unicode 和缺字规则。
- [字形映射表](formats/font-glyph-map.tsv)：完整映射数据。

## 逆向依据

- [函数命名表](reverse-engineering/function-names.md)：canonical VA 与语义名，**生成文件**。
- [命名输入](reverse-engineering/function-names-legacy.md)：历史登记与追加入口；旧结论修正按 [IDA 流程](tools/ida.md)处理，不反向导入生成表。
- [地址映射](reverse-engineering/address-mapping.md)：LE object、file、code0、legacy 和 runtime 坐标。
- [LE 镜像与验收](reverse-engineering/le-validation.md)：产物边界、静态复核和完整 fixup／运行时硬性条件。
- [IDA 9.4 独立复核](reverse-engineering/ida-crosscheck.md)：发布基线、ABI、边界和历史模型修正。
- [IDA 9.5 字节与局部实现复核](reverse-engineering/ida-bytecheck-9.5.md)：从原版重导出、RLE／菜单机器码差分、修复及限制。
- [启动流程](reverse-engineering/boot-flow.md)：片头、ANI、调色板和标题调用链。
- [音频调用与对照](reverse-engineering/audio.md)：bank、曲目、cue 与局部捕获依据。
- [通用战斗语义](reverse-engineering/common-battle-semantics.md)、[准备服务](reverse-engineering/common-preparation-semantics.md)、[共享控制与呈现](reverse-engineering/common-runtime-semantics.md)：G1 的完整原指令／调用／数据读集结论及实施合同；静态取证与原版运行分别登记。
- [通用事件来源分类](reverse-engineering/common-event-semantics.md)：分发表、保守引用、移动脚本和模板尾逐源归属；审查输入为 `common-scope-review.json`，不代替流程翻译或通用实现验收。
- [Ghidra 结构验收](reverse-engineering/ghidra-reconstruction.md)：仅按需复核 block、权限、relocation 和确定性，不是日常语义基线。

## 工具操作

- [工具速查](tools/README.md)：构建、测试、调试入口和常用数据工具。
- [冷定义工具](tools/static-data.md)：JSON 唯一输入、构建期生成、消费契约与独立原字节校验。
- [跨平台命令约定](tools/platforms.md)：Windows PowerShell／Linux Bash、构建输出、Python、环境变量和只读二进制检查。
- [图形自动化测试](tools/gui-testing.md)：SDL 原生窗口、真实按键、截图、退出与隔离存档的验证流程。
- [IDA Domain API／idalib](tools/ida.md)：环境、只读查询、授权发布流程与陷阱。
- [DOSBox debugger](tools/dosbox-debug.md)：二进制取证、当次运行地址确认，以及独立工作副本中的原版自动操作约束。

## 生成产物

| 路径 | 用途与限制 |
|---|---|
| [`generated/ida-canonical/`](generated/ida-canonical/) | 当前已标注伪 C、规范登记和发布证据；输入身份以文件头和复核报告为准 |
| [`generated/ghidra-canonical/`](generated/ghidra-canonical/) | 结构 inventory、边界账本和候选 C；不代替 IDA 基线 |
| `generated/fd2-*.json`、`generated/le-fixups.txt` | seed、迁移和 fixup 的机器账本；随对应生成器维护 |
| `generated/ghidra-decomp-all.c`、旧 r2／辅助输出 | 历史证据及旧工具输入；patch 与 wrapper/body 伪拆分使旧 Ghidra 伪 C **不得作为开发参考** |

生成文件不手改业务语义。命名先改输入，再按 IDA 流程发布，所有产物完成后最后刷新并校验 `SHA256SUMS`。大型镜像、数据库和原版捕获只留本地。

## 维护约定

新文档按上述用途放置；实现进度只更新计划，原版布局放 `formats/`，调用／地址证据放 `reverse-engineering/`，SDL 边界放 `systems/`，命令放 `tools/`。操作命令注明 PowerShell／Bash；通用转换引用跨平台约定。历史 Linux 路径和机器码证据保留原记录，不因宿主迁移改写为 Windows 已验收。已被现行资料覆盖且含错误模型的早期叙述不再另存一份；需要追溯时查 Git 历史，正式证据与命名输入仍保留。
