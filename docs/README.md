# 文档索引

- [项目首页](../README.md)：构建和启动。
- [使用指南](user-guide.md)：资源文件、键位、存档和兼容范围。
- [测试与工具](tools/README.md)：跨平台回归、原版对照和数据工具。
- [历史证据索引](history.md)：已完成阶段、原版差异和历史验证记录。
- [Agent 开发规则](../AGENTS.md)：实施、逆向和验证约束。

## 仓库布局

| 路径 | 内容 |
|---|---|
| `CMakeLists.txt`、`CMake/` | 根构建入口；SDL3 依赖、平台选项、冷数据生成、程序目标和测试注册 |
| `src/` | SDL 程序与运行时，第三方源码和对应许可位于 `src/third_party/` |
| `data/static/` | 运行时冷定义的 JSON 输入 |
| `tests/` | SDL 回归、平台输入驱动和最小夹具 |
| `tools/` | 数据生成、逆向、原版对照与发布工具；工具回归位于 `tools/tests/` |
| `build/` | 各平台的本地构建输出，不入库 |

## 系统设计

- [战场 session](systems/battle-session.md)：状态／资源owner、动作与RNG提交、存取／交接，以及S5 source纯等待、薄binder、真实HP／xy及HOST前缀。
- [输入](systems/input.md)：原版键值语义、逐呈现帧采样和 SDL 宿主边界。

## 数据格式

- [静态数据与 Mod 制作](formats/static-data.md) · JSON 表字段、索引、事件图和构建步骤。
- [数据格式规范](formats/data-formats.md)：归档、RLE、战场、存档、动画、音频及地图帧缓存。
- [历史关卡来源账格式](formats/stage-source-ledger.md)：规范化来源、caller 和批量夹具合同；不是运行时数据。
- [原版参照身份](formats/original-reference.md)：20个原文件及字库摘要、DOSBox配置与运行资格分账、只读生成／校验和负例；不替代原版运行证据。
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
- [Windows Release](tools/windows-release.md)：本地包、实际依赖清单与离线重建。
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

当前接口和实现边界放在 `systems/`，原版数据布局放在 `formats/`，调用和地址依据放在 `reverse-engineering/`，操作命令放在 `tools/`。历史候选、命令和结果保持原身份，在 [Git 历史](history.md)中查阅，不改写为当前平台或构建已验证。
