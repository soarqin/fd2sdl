# 文档索引

项目文档按用途分类。新增文件应放入对应目录，不再使用开发步骤编号作为文件名前缀。

## `plans/`：计划与里程碑

- [`development.md`](plans/development.md)：项目总体开发计划。
- [`battlefield.md`](plans/battlefield.md)：战场系统 M1～M9 路线和验收标准。
- [`audio.md`](plans/audio.md)：音频逆向、后端选择和实施计划。

## `reverse-engineering/`：逆向分析

- [`decompilation-report.md`](reverse-engineering/decompilation-report.md)：原始程序总体反编译报告。
- [`address-mapping.md`](reverse-engineering/address-mapping.md)：LE、file offset、code0、dual、relbase 和 runtime 地址换算。
- [`decompilation-corrections.md`](reverse-engineering/decompilation-corrections.md)：历史反编译污染和修正记录。
- [`ghidra-reconstruction.md`](reverse-engineering/ghidra-reconstruction.md)：Ghidra 11.3.2 规范 relbase 重建、relocation、函数边界和验收流程。
- [`boot-flow.md`](reverse-engineering/boot-flow.md)：启动、标题和 ANI/AFM 调用流程。
- [`field-unit-constructor.md`](reverse-engineering/field-unit-constructor.md)：战场单位构造链。
- [`function-names.md`](reverse-engineering/function-names.md)：已确认函数命名登记表。

## `formats/`：数据格式

- [`data-formats.md`](formats/data-formats.md)：原版 `.DAT`、战场、动画、音频等二进制格式。
- [`font-text-mapping.md`](formats/font-text-mapping.md)：FDTXT token 与 Unicode 映射规则。
- [`font-glyph-map.tsv`](formats/font-glyph-map.tsv)：完整字形映射数据。

## `systems/`：SDL 系统设计

- [`input.md`](systems/input.md)：原版键盘语义与 SDL 输入层边界。

后续独立系统设计文档放入此目录。逆向事实仍应链接到 `reverse-engineering/` 或 `formats/` 中的证据。

## `tools/`：工具操作

- [`README.md`](tools/README.md)：项目工具命令速查。
- [`dosbox-debug.md`](tools/dosbox-debug.md)：DOSBox debugger 内存转储流程。

工具文档不参与开发阶段编号。使用工具前先查本目录，避免为查询语法而启动交互式程序。

## `generated/`：生成或机器辅助产物

- `ghidra-canonical/`：规范 relbase structural inventory、函数边界账本和候选 C 导出。
- `fd2-function-seeds.json`：证据分层函数 seed、`__chkstk` wrapper 和延后候选清单。
- `fd2-name-migration.json`：`function-names.md` 全部 209 行的迁移 disposition。
- `fd2-confirmed-function-names.json`：已重新确认的 canonical name/VA registry。
- `ghidra-decomp-all.c`：旧 dual 流程的历史 Ghidra 输出，不再作为规范基线。
- `r2_funcs.txt`：旧 radare2 函数清单，只作候选线索。
- `authoritative_funcs.txt`：确认入口清单。
- `le-fixups.txt`：LE fixup 生成摘要与验证结果。
- `opcode-functions.c`：辅助 opcode 分析输出。

生成文件不得手工改写业务语义。函数语义命名应先登记到 `reverse-engineering/function-names.md`，再由生成流程更新对应产物。

## 分类规则

- 跨系统的开发顺序和验收：`plans/`。
- 原版机器码、调用链和地址证据：`reverse-engineering/`。
- 原版文件／记录布局：`formats/`。
- SDL 重写层的系统边界：`systems/`。
- 命令、工具限制和操作流程：`tools/`。
- 可再生成的大型文本或机器分析结果：`generated/`。

若单个文档同时包含计划与逆向证据，应按主要用途归类，并用链接引用另一分类中的权威依据，避免复制两份结论。
