# 项目工具速查

本文集中记录 FD2 SDL 项目中常用工具的已验证命令。执行任务前先查本文和对应专题文档，不要仅为回忆语法而反复调用 `--help`。若工具版本或命令行为发生变化，应先更新本文，再继续依赖该命令。工具文档统一位于 `docs/tools/`，不与开发计划的编号体系混用。

## 1. 构建和测试

### CMake

首次配置：

```bash
cmake -S src -B src/build
```

构建：

```bash
cmake --build src/build -j2
```

完整 CTest：

```bash
ctest --test-dir src/build --output-on-failure
```

无窗口集成验证：

```bash
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
  ./src/fd2sdl --field-play-once
```

最终文本检查：

```bash
git diff --check
```

不要提交 `src/build/` 或 `src/fd2sdl`。

## 2. LE 分析镜像

重建命令：

```bash
python3 tools/rebuild_fd2_analysis.py
python3 tools/validate_le_fixups.py
python3 tools/verify_le_runtime_dump.py /tmp/fd2-entry.bin \
  --object1-base 0x16c000 --object2-base 0x1ac000 --object3-base 0x1b2000
```

`validate_le_fixups.py` 严格检查 fixup page table、独立 decoder、source prevalue、重复记录和非 source 字节不变。`verify_le_runtime_dump.py` 再验证 DOSBox pre-entry 全对象转储。两者都通过才满足 `AGENTS.md` 的完整 fixup 验收；当前 FD2.EXE 已通过该组合验证。

常用输出：

```text
tools/fd2_le_raw.bin
tools/fd2_le_code0.bin
tools/fd2_le_dual_clean.bin
tools/fd2_le_fixups.txt
```

这些二进制是可再生成文件，不提交。

转储 relocation-backed 表：

```bash
python3 tools/dump_fd2_fixup_table.py --ds 0x1d71 --count 40
python3 tools/dump_fd2_fixup_table.py --ds 0x1b91 --count 32
python3 tools/dump_fd2_fixup_table.py --object 3 --ds 0x27d8 --count 96
```

工具测试：

```bash
python3 -m unittest discover -s tools/tests -p 'test_*.py'
```

## 3. radare2

本项目原始程序必须按 32 位 x86 分析。`rabin2` 默认主要识别 DOS MZ stub，不能把其输出当作完整 LE 应用映像。

查看 clean code0 某段反汇编：

```bash
r2 -q -a x86 -b 32 -m 0 \
  -c 'e scr.color=false; s 0x28cb3; pd 40' \
  tools/fd2_le_code0.bin
```

查看原始文件指定 file offset 时，先切出有界窗口，再按 32 位反汇编：

```bash
dd if=original_game/FD2.EXE of=/tmp/fd2-window.bin \
  bs=1 skip=$((0x39ab3)) count=256 status=none
r2 -q -a x86 -b 32 -m 0 \
  -c 'e scr.color=false; pd 60' /tmp/fd2-window.bin
```

地址换算必须先查 `docs/reverse-engineering/address-mapping.md`。不要把 raw file offset、code0、relbase linear 或 runtime linear 混用。

## 4. 二进制检查

十六进制查看：

```bash
xxd -g1 -s $((0x36014)) -l 128 original_game/FD2.EXE
```

精确切片：

```bash
dd if=original_game/FD2.EXE of=/tmp/fd2-slice.bin \
  bs=1 skip=$((0x36014)) count=$((0x1000)) status=none
```

哈希：

```bash
sha256sum original_game/FD2.EXE /tmp/fd2-slice.bin
```

逐字节比较：

```bash
cmp -l expected.bin actual.bin
```

只判断是否一致：

```bash
cmp -s expected.bin actual.bin
```

`objdump` 不识别本项目的 bound LE 格式；不要用它作为 LE loader 或 fixup oracle。`rabin2 -I/-S/-R` 主要展示 MZ stub，也不能替代项目 LE parser。

## 5. Ghidra headless

权威重建命令模板：

```bash
/tmp/ghidra_11.3.2_PUBLIC/support/analyzeHeadless \
  /tmp/ghidra_fd2_clean_proj FD2Clean \
  -import tools/fd2_le_dual_clean.bin \
  -processor x86:LE:32:default -cspec gcc \
  -scriptPath tools/ghidra-scripts \
  -postscript decompile_clean.py \
  -overwrite -deleteProject
```

执行前必须确认输入镜像已通过当前 LE/fixup 验证。脚本输出到 `docs/generated/ghidra-decomp-all.c`。反编译结果仍须与原始机器码和独立反汇编交叉核对。

## 6. DOSBox debugger

完整说明见：

```text
docs/tools/dosbox-debug.md
```

重要限制：

- 禁止运行 `dosbox-debug --help`；本机版本会直接启动界面。
- 仅用于转储已加载的原版可执行映像或运行时二进制证据。
- 不自动操作菜单、剧情或战场。
- 每次运行重新确认 selector、descriptor base 和 relocation delta。

已验证的 debugger 内部命令：

```text
DEBUG FD2.EXE
BP <selector>:<offset>
SELINFO <selector>
D <selector>:<offset>
MEMDUMPBIN <selector>:<offset> <length>
```

## 7. Python 分析工具

音频摘要和导出：

```bash
python3 tools/analyze_fd2_audio.py
python3 tools/analyze_fd2_audio.py --json
python3 tools/analyze_fd2_audio.py --extract-sfx-dir /tmp/fd2-sfx
python3 tools/analyze_fd2_audio.py --extract-music-dir /tmp/fd2-music
```

关卡代码线索：

```bash
python3 tools/analyze_fd2_stage_code.py
```

FDFIELD event：

```bash
python3 tools/analyze_fdfield_events.py
```

`.DAT` 提取：

```bash
python3 tools/dat_extract.py
```

各脚本若需要参数，应优先阅读文件头 docstring 和项目文档；普通 Python CLI 才可在确认不会启动交互界面的前提下使用 `--help`。

## 8. Git 检查

查看工作树：

```bash
git status --short
git diff --stat
git diff --check
```

查看未暂存差异：

```bash
git diff -- path/to/file
```

除非任务明确要求，不自动暂存或提交。`original_game/`、`tools/*.bin`、`.pi-subagents/`、`.codegraph/` 和构建产物均不提交。

## 9. 外部资料检索

外部格式规范优先使用 `ketch`。若 `ketch` 因配置、限流或上游失败不可用，应降级到 Pi Web Search。当前会话未提供 Web Search 时，改用可审计的本地／公开工具源码，并明确记录来源；不得把搜索失败当作格式结论。

本次 LE 审计使用的独立公开实现之一为 HT Editor 的 LE loader 源码。第三方实现只能作为交叉解码依据，最终仍须与原版运行时完整内存转储逐字节一致。

## 10. 文档索引

- 地址空间和 r2/Ghidra：`docs/reverse-engineering/address-mapping.md`
- 反编译修正：`docs/reverse-engineering/decompilation-corrections.md`
- DOSBox debugger：`docs/tools/dosbox-debug.md`
- 数据格式：`docs/formats/data-formats.md`
- 函数命名：`docs/reverse-engineering/function-names.md`
- 项目强制规则：`AGENTS.md`
