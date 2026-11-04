# LE 镜像与严格验收

本文集中维护 LE 产物边界和验收条件。地址换算见 [address-mapping.md](address-mapping.md)，数据库操作见 [IDA 手册](../tools/ida.md)，运行时取证见 [DOSBox 转储手册](../tools/dosbox-debug.md)。普通玩法开发不必重建分析镜像。

## 结论与适用范围

- **静态模型已通过独立复核**：项目 parser／独立 decoder 与 IDA loader 的 object、全部 fixup 和字节一致；LE/LX fixup 可靠性疑虑已关闭。历史依据为 [IDA 9.4 报告](ida-crosscheck.md)，[9.5 重导出](ida-bytecheck-9.5.md)再次通过 21 项静态检查。
- **严格运行时全对象验收尚未通过**：历史 DOSBox 捕获的 file-backed 区和 relocation source 匹配，但 object2 BSS/stack 有 21 个非零字节，需要新转储。不能用「loader 建栈」或静态 BSS 规范化自动豁免差异。
- **玩法／视听另行验收**：静态、运行时字节检查和局部机器码执行都不替代 DOSBox 场景对照。允许在独立副本上自动操作与捕获，仍须有状态、截图／存档／trace 和已确认判据。

## 产物与写入边界

| 产物 | 用途 |
|---|---|
| `tools/fd2_le_raw.bin` | 按 LE relbase 摆放的 raw-unrelocated object 字节 |
| `tools/fd2_le_code0.bin` | object1 从 offset 0 开始的真实代码视图 |
| `tools/fd2_le_relocated_relbase.bin` | 独立 loader-relocated 输出，仅写已验证的 relocation source |
| `tools/fd2_le_relocation_manifest.tsv` | 完整记录、source/target、前值与重复关系 |
| `tools/fd2_le_fixups.txt` | internal fixup 索引，只用于解释引用，不是 patch 指令 |
| `docs/generated/le-fixups.txt` | 可提交的生成摘要 |
| `tools/fd2_le_ghidra_chkstk.bin` | 兼容文件名，与 raw 相同，未 patch `__chkstk` |
| `tools/fd2_le_dual_clean.bin` | 低 64 KiB 镜像的历史兼容产物，不得用作新分析输入 |

**不得用 relocated 输出覆盖 raw 输入，也不得无 manifest 直接 patch。** 合法 loader relocation 可以落在 object1 代码中的绝对地址 source；禁止的是错误／未经验证的写入，而不是笼统禁止所有代码页 relocation。所有非 source 字节必须保持原样。

`tools/fd2_dual_final.bin`、`/tmp/fd2_le_correct.bin` 及其他无清单 patched 镜像不是权威输入。镜像、数据库、完整内存和本地导出不入库。

## 重建与静态复核

以下为 Linux bash 命令。Windows PowerShell 使用 `python` 替换 `python3`，参数和相对路径保持相同；Python 环境、文件哈希和临时目录操作见[跨平台命令](../tools/platforms.md)。两平台的实际执行结果分别登记，命令可改写不等于已重新通过验收。

从项目根目录执行：

```bash
python3 tools/rebuild_fd2_analysis.py
python3 tools/validate_le_fixups.py
python3 tools/ida/verify_ida_vs_project.py
```

前两步从原版构建并独立验证 page/fixup。第三步是纯数据比较，但需要已有完整 `tools/ida-out/` 导出，包括 segment 字节、`ida-program.json` 和 `ida-fixups.tsv`；只有版本库的精简报告不能重跑完整 21 项检查。缺导出时按 [9.5 复现流程](ida-bytecheck-9.5.md#复现与本地证据)使用新临时目录，不覆盖已有数据库。

当前原版有 7,959 条 fixup 记录、7,948 个唯一 source、11 条相同 source/target 的重复记录。相同重复可以显式验证后去重写入；冲突或部分重叠不能按后写覆盖。

`objdump` 不是 bound LE loader／fixup oracle；`rabin2` 的 MZ stub 输出也不能代表完整应用映像。Ghidra 仅按需作[独立结构验收](ghidra-reconstruction.md)，不替代 IDA 语义基线。

## 完整 fixup 的严格验收

修改 parser、构建新的 loader-relocated 镜像或宣称严格运行时通过时，必须同时满足：

1. **完整解析**：消费 fixup record table 全部字节；每条记录的长度、source type、target flags、source list、additive、16/32 位字段都有 LE 规范依据。未知类型、尾部残留、截断和越界一律失败。
2. **独立解码**：两个独立实现逐条比较 page、source、target、addend、写入宽度和记录边界。共享解析函数或复制同一分支逻辑不算独立验证。
3. **写入受限**：raw 与 relocated 分开输出；只在 manifest 覆盖的 source 范围写入，非 relocation 字节逐字节不变。重复、重叠和冲突必须显式验证，不能以后写覆盖处理。
4. **运行身份可靠**：只用 `dosbox-debug` 转储已经加载的原版对象，在进入游戏逻辑或 overlay 改写前获取全部已提交 page、BSS 和 relocation source；selector、object base 与 relocation delta 必须来自该次运行。
5. **全对象一致**：静态输出与运行时转储在全部 object 的完整 virtual size 内逐字节相同，包括 BSS；每个 relocation source 的运行时值也逐条验证。只匹配入口、几个函数、少数表或 hash 抽样不构成通过。
6. **缺项即失败**：任一字节不同、记录未覆盖或步骤无法执行，只能报告未通过；不得声称「完全正确」，也不得把未通过的镜像提升为权威基线或用于 Ghidra 权威重建。

运行时转储的具体命令、历史输入身份及地址重取步骤只在 [DOSBox 手册](../tools/dosbox-debug.md)维护。新的转储不能沿用其中的历史绝对地址。
