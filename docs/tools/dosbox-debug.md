# DOSBox Debugger 内存转储操作

本文记录本项目在 Ubuntu `dosbox-debug 0.74-3` 中已经验证过的调试器命令，供原版 `FD2.EXE` 可执行映像和运行时表转储使用。

Windows 使用带 debugger 的 DOSBox 构建。普通 DOSBox 发行版不一定包含 debugger；不能仅凭版本号认定下列命令可用。Windows 构建的内部命令、转储文件名与长度解析须在首次使用时确认，尚未取得相应捕获前保持「待原版对照」。宿主 PowerShell 与 Linux bash 的文件操作分别列在下文，共用规则见[跨平台命令](platforms.md)。原版和 SDL 的自动输入、捕获及比对统一见[图形自动化测试](gui-testing.md)。

## 使用边界

- 本文的 debugger 命令用于转储已加载的原版可执行对象、运行时内存和二进制表；玩法验证另按图形自动化流程驱动独立副本。
- 允许自动操作原版菜单、战场或剧情并捕获截图、声音、存档和 trace；输入步骤与判据须来自逆向或已确认原版证据，不能凭自动运行结束宣称行为一致。
- 指定关卡、施法、物品或剧情状态须有可复现 fixture 和状态检查；无法确认当前状态时保存失败证据并停止该场景，不继续盲发按键。
- 每次运行都重新确认 selector、descriptor base 和 relocation delta。不得复用历史运行中的绝对地址。
- `original_game/` 只读。转储文件先写到临时目录，确认用途后再决定是否保留；不得提交原版内存映像。

## 启动注意事项

历史 Linux 环境的 `/usr/bin/dosbox-debug` 是图形／终端交互式程序。以下调用不会显示调试器命令帮助，反而会直接启动界面；两平台均禁止用这种方式探测帮助：

```bash
# 禁止用作帮助探测
# dosbox-debug --help
```

Linux 的通用 DOSBox 命令行参数查本地 man page；Windows 查安装包随附的 README／手册，不能执行 Linux 的 `zcat` 命令：

```bash
zcat /usr/share/man/man1/dosbox-debug.1.gz | col -b
```

调试器内部命令以本文记录为准。不要为了查询帮助而反复启动界面。

先将所需原版文件复制到独立临时目录，再启动 debugger。可自动启动与取证，但下列占位路径须换成本次副本目录，不能直接挂载 `original_game/` 后假定普通 `MOUNT` 会强制只读：

```text
MOUNT C <本次原版副本目录>
C:
DEBUG FD2.EXE
```

副本供 DOS 写入缓存／存档；宿主转储也写独立工作目录，原始证据保持不变。

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

Windows PowerShell（在启动 debugger 前创建目录，再从该目录启动 debugger）：

```powershell
$captureDir = Join-Path ([IO.Path]::GetTempPath()) ('fd2-debug-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $captureDir | Out-Null
Set-Location $captureDir
# 在 debugger 内执行 MEMDUMPBIN 后退出或切回 PowerShell
Move-Item -LiteralPath memdump.bin -Destination rng-initial.bin
Get-FileHash -Algorithm SHA256 -LiteralPath rng-initial.bin
(Get-Item -LiteralPath rng-initial.bin).Length
```

Linux bash：

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

该命令曾在第一次 `field_rng_next` 执行前转储两个字节，得到 loader 初值。`MEMDUMPBIN` 的长度是十进制或十六进制数值时，以实际 debugger 解析结果和生成文件大小为准；完成后必须检查文件长度，Linux 使用 `stat -c '%s' rng-initial.bin`，PowerShell 使用 `(Get-Item -LiteralPath rng-initial.bin).Length`。

相关文本转储命令也已确认：

```text
MEMDUMP <selector>:<offset> <length>
```

它写入固定文件 `memdump.txt`，适合人工查看，不用于逐字节二进制 oracle。

## FD2 pre-entry 全对象 oracle

以下为历史 LE entry（L1）取证流程，保留原捕获身份；当前 L0 严格门见后文。

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

6. 将 `MEMDUMP.BIN` 移到本次独立临时目录并命名为 `fd2-entry.bin`，返回项目根目录后执行。以下仍是历史 object base 示例，新捕获必须替换三个 base：

Windows PowerShell（`$captureDir` 指向本次捕获目录）：

```powershell
python tools/verify_le_runtime_dump.py (Join-Path $captureDir 'fd2-entry.bin') --object1-base 0x16c000 --object2-base 0x1ac000 --object3-base 0x1b2000
```

Linux bash：

```bash
python3 tools/verify_le_runtime_dump.py /tmp/fd2-entry.bin \
  --object1-base 0x16c000 \
  --object2-base 0x1ac000 \
  --object3-base 0x1b2000
```

2026-07-15 的 pre-entry dump 长度为 `0x200000`，SHA-256 为 `3eea0a49f8eb6afd7bea29fde75cda5a55fa5e903a44be67265720ba6b5f56e1`。历史验证结果：7959 条 relocation source 全部等于 `runtime_object_base + target_offset`；三个 object 的全部 file-backed 非 relocation 字节逐字节一致。object2 的 file-backed 区为前 `0x4000` 字节；尾部 BSS/stack 在 EIP transfer 前已有 21 个非零字节，历史记录将其归为 loader 建栈。转储只保存在 `/tmp`，不进入版本库。

**验收范围修正**：旧验证器仅打印 BSS 非零计数，仍输出全对象 `PASS`，该结论超出了实际检查范围。当前验证器对 BSS 差异也返回失败，不再把 loader 建栈当作自动豁免。上述历史 L1 证据只证明 file-backed 区和 relocation source 一致，完整静态零模型比较仍失败。F1 新 L0 通过不改写该失败。新的运行时证据必须重新取得 object base 与完整转储，不能沿用历史绝对地址。IDA 静态 loader／fixup 独立复核结论不受此口径修正影响。

### F1 装载态 L0 与实际入口 L1

永久入口为 `tools/run_le_preentry_check.py`。它使用本轮 owned DOSBox 的只读 RPC 和 debugger 断点，不修改 guest 寄存器、数据或 EXE。每次记录真实 DPMI `0501` 调用及返回，按 LE 对象各自唯一的分配大小取得基址；再次用全对象原字节与每条 fixup 验证，不能以分配大小相符代替字节验证。当前 GDT 决定 flat selector 和 16 位 loader descriptor，不固定复用 selector／base。

L0 停在 loader 首次写对象内启动参数前。本轮 Domain 对实际 loader 字节的 16 位解码确认四条 WORD 写入；`3966` 等是该 loader 的 selector-relative 坐标，**不是 FD2 canonical VA**。工具在当次 GDT 代码 descriptor 中查找指令，命中后再次核 descriptor 与字节。dump 前后均要求 protected、非 v86、同一 flat 视图和寄存器未变，读取完整 16 MiB 实际 RAM，不补零。

L1 则继续运行到由本轮 object base 和 LE header 算出的真实入口，另取完整 RAM 并运行同一个未放宽的验证器。F1 已观察 L0 全部字节和 7,959 条 fixup PASS、L1 仍 21 个非零 BSS 字节。只直接定位了最先两组 DWORD 的四条 WORD 写，不能据此声称全部 21 字节写源已追完；L1 不能提升为静态零模型或玩法初态。

该入口是内存专用实验，不申请显示需求，不采 WGC，不签画面／声音／设备时序。它为 debugger 建立独立隐藏 console，使用本进程的配置／资源副本；所有进程仍在外层 Job 内，正常关闭、必要的 owned 清理、原资源及配置的前后守卫均须通过。10 ms 前台采样只支持该粒度的有限观察，不作绝对无瞬态断言。配置链、实际 argv 和源身份须与运行收据一致；字节通过不能代签配置声明。

Windows PowerShell（实际路径和全新输出目录）：

```powershell
$Driver = './build/windows-test/Debug/fd2_field_play_input_driver.exe'
$Dosbox = 'C:/Users/Soar/scoop/apps/dosbox-x/2026.10.01/dosbox-x.exe'
$Bootstrap = 'C:/Users/Soar/AppData/Local/DOSBox-X/dosbox-x-2026.10.01.conf'
& $Driver run-job 240000 python -B tools/run_le_preentry_check.py --original E:/Projects/fd2sdl/original_game --dosbox $Dosbox --driver $Driver --bootstrap-config $Bootstrap --artifacts build/f1/loaded-image-new
```

该 Windows runner 不提供 Linux owned 进程／console 适配。本轮 Linux 未执行；Linux 可对已有完整 dump 使用上一节 Bash 离线验证，但不计一次新原生取证。完整 RAM、loader 字节与原版配置留在本地 `build/`，不提交或分发。

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

转换前先按[地址映射](../reverse-engineering/address-mapping.md)确认 object、file/code0/VA 与 selector-relative 视图；`D 0:<linear>` 不等于 `D DS:<offset>`。LE target offset 已是 object 内偏移，不能再套用 header bias。

## 完整 fixup 验证要求

转储只承担最终独立 oracle，不替代 parser 的格式验证。完整解析、独立 decoder、source 写入边界和全对象／BSS 零差异等硬性条件统一见 [LE 严格验收](../reverse-engineering/le-validation.md#完整-fixup-的严格验收)；任一缺项或差异都不通过。

terrain／critical 表和 RNG 初值等历史捕获身份见[地址映射](../reverse-engineering/address-mapping.md#历史运行时取证注意事项)，不作为新运行默认值。

## L1 有源启动写账的可复跑入口

`run_le_preentry_check.py`追加`--startup-trace`时，仍复用同一owned初始化和L0严格门，只执行一次`LOGL 2000`（十六进制8,192条上限）至真实LE entry。未知停止、计数先耗尽、首末寄存器／PC不符均失败，不补RUN。无此flag时保留原12寄存器及旧流程。collector的`ok`只签L0／trace／owner；L1须另过Domain和有源分层门，不能仅取顶层绿值。

以下从Windows工作树根目录执行，所有输出均须不存在。原EXE／配置／资源只读；本轮不申请显示需求或改电源／混音器：

```powershell
$TaskRoot = (Get-Location).Path
$Driver = Join-Path $TaskRoot 'build/windows-test/Debug/fd2_field_play_input_driver.exe'
& $Driver run-job 240000 python -B tools/run_le_preentry_check.py --original "$TaskRoot/original_game" --dosbox 'C:/Users/Soar/scoop/apps/dosbox-x/2026.10.01/dosbox-x.exe' --driver $Driver --bootstrap-config 'C:/Users/Soar/AppData/Local/DOSBox-X/dosbox-x-2026.10.01.conf' --artifacts build/f1/startup-NEW --startup-trace
```

Domain脚本按[IDA流程](ida.md)用现行执行器调用。临时wrapper须写实际**绝对**工作树根，不能从`__file__`或cwd推算；执行器会把wrapper复制到临时目录并在自身目录执行。例如在本地`build/f1/export-startup-domain.py`保存：

```python
from pathlib import Path
import sys
root = Path('C:/path/to/actual/worktree')  # 替换为本次绝对工作树
sys.path.insert(0, str(root / 'tools/ida'))
from export_le_startup_domain import export
export(root / 'build/f1/startup-NEW/report.json',
       root / 'build/f1/startup-NEW-domain')
```

用本机实际安装路径替换`$IdaSkill`／`IDADIR`；不加`--save`：

```powershell
$IdaSkill = 'C:/path/to/ida-domain-scripting'
$env:IDADIR = 'C:/Program Files/IDA Professional 9.5'
& "$IdaSkill/.venv/Scripts/python.exe" "$IdaSkill/run.py" "$TaskRoot/build/f1/export-startup-domain.py" -f "$TaskRoot/original_game/FD2.EXE" --no-wrap --timeout 120
python -B tools/verify_le_startup_writes.py --capture build/f1/startup-NEW/report.json --exe original_game/FD2.EXE --domain build/f1/startup-NEW-domain/domain.json --artifacts build/f1/startup-NEW-layered
```

Domain以L0当刻GDT／真实selector取得两段六窗，只在独立产物内建立小型数据库，关闭不保存；原runtime代码／完整RAM／Domain原字节报告都留本地，不提交。分层expected由L0的62B源上下文、原变换／搬运和四条WORD写计算，保全部未写字节及fixup；不是把21个事后差异加入白名单。复跑不沿用旧selector、基址、绝对指针、trace行数或旧Domain收据。

有源启动写账的纯数据负例、Windows junction 检查及旧失败保留在[F1 历史报告](https://github.com/soarqin/fd2sdl/blob/373e5c0bd21454793a20fc62ecb8addb18055dba/docs/plans/f1-reference-domain.md)。该历史测试不属于当前 CTest；不能用旧收据代替新候选的原生资格。
