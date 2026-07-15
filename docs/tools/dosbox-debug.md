# DOSBox Debugger 内存转储操作

本文记录本项目在 Ubuntu `dosbox-debug 0.74-3` 中已经验证过的调试器命令，供原版 `FD2.EXE` 可执行映像和运行时表转储使用。

## 使用边界

- `dosbox-debug` 仅用于转储已经加载的原版游戏可执行对象、运行时内存和二进制表。
- 不使用调试器自动操作菜单、战场或剧情。
- 需要指定关卡、施法、物品或剧情状态时，停止自动化工作，请开发者手工操作并提供截图、存档或转储结果。
- 每次运行都重新确认 selector、descriptor base 和 relocation delta。不得复用历史运行中的绝对地址。
- `original_game/` 只读。转储文件先写到临时目录，确认用途后再决定是否保留；不得提交原版内存映像。

## 启动注意事项

本机的 `/usr/bin/dosbox-debug` 是图形／终端交互式程序。以下调用不会显示调试器命令帮助，反而会直接启动界面：

```bash
# 禁止用作帮助探测
# dosbox-debug --help
```

通用 DOSBox 命令行参数查本地 man page：

```bash
zcat /usr/share/man/man1/dosbox-debug.1.gz | col -b
```

调试器内部命令以本文记录为准。不要为了查询帮助而反复启动界面。

启动原版程序时，在 DOSBox shell 中挂载只读证据目录并使用 debugger 的 `DEBUG` 命令：

```text
MOUNT C /home/soar/src/fd2sdl/original_game
C:
DEBUG FD2.EXE
```

实际挂载方式以当次环境为准。不要向 `original_game/` 写入调试输出。

## 已验证的调试命令

### 设置执行断点

```text
BP <selector>:<offset>
```

历史实例：

```text
BP 170:1AABE3
```

该实例只属于当次运行。后续运行必须先重新定位 selector 和 runtime offset。

### 查看 selector descriptor

```text
SELINFO <selector-name-or-value>
```

记录 descriptor base、limit 和属性。`CS`、`DS` 即使都是 flat selector，也可能使用不同 selector 值。禁止假设 selector base 永远为 0。

断点和检查点管理：

```text
BPLIST
BPDEL <number>
BPDEL *
```

普通执行断点：

```text
BP <selector>:<offset>
```

如需在内存改写时停止，使用已验证的三类命令：

```text
BPM <segment>:<offset>
BPPM <selector>:<offset>
BPLM <linear-address>
```

### 查看内存

```text
D <selector>:<offset>
```

`D 0:<linear>` 使用 debugger 的线性地址视图，不等于任意 selector 下的 offset。若 selector descriptor base 非 0：

```text
linear = descriptor_base + selector_offset
```

历史捕获曾发现 `D 0:<linear>` 与 `D DS:<offset>` 相差 descriptor base；两种写法不能混用。

### 转储二进制内存

```text
MEMDUMPBIN <selector>:<offset> <length>
```

调试器将内容写入当前目录的固定文件 `memdump.bin`，不是以地址自动命名。每次转储前应先准备独立临时工作目录；转储后立即改名并记录来源，避免下一次命令覆盖：

```bash
mkdir -p /tmp/fd2-debug-capture-YYYYMMDD
cd /tmp/fd2-debug-capture-YYYYMMDD
# 在 debugger 内执行 MEMDUMPBIN 后退出或切回 shell
mv memdump.bin rng-initial.bin
sha256sum rng-initial.bin
```

历史实例：

```text
MEMDUMPBIN 178:1AE7B8 2
```

该命令曾在第一次 `field_rng_next` 执行前转储两个字节，得到 loader 初值。`MEMDUMPBIN` 的长度是十进制或十六进制数值时，以实际 debugger 解析结果和生成文件大小为准；完成后必须用 `stat -c '%s'` 检查长度。

相关文本转储命令也已确认：

```text
MEMDUMP <selector>:<offset> <length>
```

它写入固定文件 `memdump.txt`，适合人工查看，不用于逐字节二进制 oracle。

## FD2 pre-entry 全对象 oracle

当前已验证的无游戏交互流程：

1. `DEBUG FD2.EXE` 在 loader stub 首指令暂停。
2. 设置 object1 EIP transfer 断点：

```text
BP 170:198CB4
```

3. 按 `F5` 运行；命中时 `CS=0170`、`DS=0178`。该地址和 selector 只属于本次捕获；后续必须重新确认。
4. 本次运行由完整内存签名确认 object base：

```text
object1 base = 0x16c000
object2 base = 0x1ac000
object3 base = 0x1b2000
```

5. 在 flat DS 下转储至少 2 MiB：

```text
MEMDUMPBIN DS:0 200000
```

6. 将 `MEMDUMP.BIN` 移到 `/tmp`，执行：

```bash
python3 tools/verify_le_runtime_dump.py /tmp/fd2-entry.bin \
  --object1-base 0x16c000 \
  --object2-base 0x1ac000 \
  --object3-base 0x1b2000
```

2026-07-15 的 pre-entry dump 长度为 `0x200000`，SHA-256 为 `3eea0a49f8eb6afd7bea29fde75cda5a55fa5e903a44be67265720ba6b5f56e1`。验证结果：7959 条 relocation source 全部等于 `runtime_object_base + target_offset`；三个 object 的全部 file-backed 非 relocation 字节逐字节一致。object2 的 file-backed 区为前 `0x4000` 字节；尾部 BSS/stack 在 EIP transfer 前已有 21 个非零字节，属于 loader 建栈，不作为 file-backed oracle 失败。转储只保存在 `/tmp`，不进入版本库。

## 推荐的可执行映像转储流程

1. 从 clean `FD2.EXE` 静态提取一段至少 24 字节、在目标对象中唯一的机器码签名。
2. 启动 `dosbox-debug`，用 `DEBUG FD2.EXE` 让 DOS/4GW loader 装入程序。
3. 在进入游戏逻辑或 overlay 改写前暂停。
4. 记录 `CS`、`DS`、`ES`，并对每个相关 selector 执行 `SELINFO`。
5. 在 runtime 内存中定位唯一签名，计算本次运行的 object base 或 relocation delta。
6. 用第二段不相邻签名独立复核 base；只匹配一段签名不算通过。
7. 对每个 LE object 的完整 `virtual_size` 执行 `MEMDUMPBIN`，包括已提交 file-backed page 和 BSS 零填充区。
8. 记录每个转储的 selector、offset、linear address、长度和 SHA-256。
9. 退出 debugger 后，用离线脚本逐字节比较静态 loader 输出和运行时转储。
10. 若任何字节不同，先分类为 page extraction、fixup、BSS、运行期初始化或 overlay 改写；未分类前不得修改静态镜像来迎合结果。

## 地址换算纪律

项目文档函数地址、clean code0、LE object offset 和 debugger runtime offset 是不同地址空间：

```text
object offset
relbase linear = object.relbase + object offset
code0          = object1 offset
object1 file   = embedded_mz_file_offset + LE.data_pages_off + code0
runtime linear = descriptor base + selector offset
```

转换前先在 `docs/reverse-engineering/address-mapping.md` 中确认当前对象和视图。LE internal fixup target offset 直接是目标 object 内偏移；`0x28b8` 仅是 embedded MZ 的 `e_lfanew`，不得再作为 target bias。

## 完整 fixup 验证要求

`dosbox-debug` 转储只承担最终独立 oracle，不替代 LE parser 的格式验证。完整 fixup 必须同时满足：

- fixup record table 全字节解析，没有未知记录或尾部残留；
- 两个独立 decoder 逐条产生相同结果；
- 静态 loader 只修改 relocation source 字节；
- 全部 object、全部 relocation source、全部非 relocation 字节和 BSS 与 loader 初始运行时转储逐字节相同；
- 任一差异都使验证失败。

详细硬性条件见项目根目录 `AGENTS.md` 的「完整 fixup 的严格验收」。

## 历史已验证实例

2026-07-13 的一次 battlefield overlay 捕获中：

- `CS=0x0170`、`DS=0x0178`；
- 由 runtime 唯一机器码签名反推出 code0 relocation delta `0x146dec`；
- 成功转储 terrain attack、terrain defense 和 profile critical 表；
- 另一次在 `field_rng_next` 首次执行前转储 loader RNG 初值 `0x7a18`。

这些值只用于说明流程，不是下一次运行的默认地址。完整记录见 `docs/reverse-engineering/address-mapping.md`。
