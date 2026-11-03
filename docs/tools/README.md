# 项目工具速查

以下命令从项目根目录执行。先查本文及专题，不为回忆语法反复调用 `--help`；工具版本／行为改变后再修订对应命令。开发约束见 [AGENTS.md](../../AGENTS.md)，完整分类见[文档索引](../README.md)。

## 构建与回归

```bash
cmake -S src -B src/build
cmake --build src/build --parallel
ctest --test-dir src/build --output-on-failure
python3 -m unittest discover -s tools/tests -p 'test_*.py'

# 无窗口资源／逻辑集成，不是正式输入或原版行为验收
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./src/fd2sdl --field-play-once

python3 tools/ida/check_src_addresses.py
(cd docs/generated/ida-canonical && sha256sum -c SHA256SUMS)
git diff --check
```

- 部分 CTest／Python 测试依赖原版资源或本地 LE 产物。Python seed／Ghidra 工具测试还需 `capstone`；缺依赖、跳过或未注册不能记为完整通过。
- `resource` 检查 .DAT／RLE 错误与原版资源兼容；`field_play_result` 用链接器替身验证正式循环错误出口，不启动窗口。
- Linux/X11 的 `field_play_input` 由 CMake 在 X11、XTest 和 Xvfb 齐备时注册，驱动真实 SDL 键盘，不直接调用玩法 API。可先用 `ctest --test-dir src/build -N` 核实测试是否存在。
- `test_ida_crosscheck` 不需要 IDA，检查发布完整性、报告结构和源码规范地址；不等于重新跑完整 IDA 字节对照。

### Sanitizer（Linux／Clang）

```bash
san=$(mktemp -d /tmp/fd2-sanitize.XXXXXX)
cmake -S src -B "$san" \
  -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build "$san" --parallel
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir "$san" --output-on-failure -E '^field_play_input$'

# 两个构建目录共用 src/fd2sdl 输出，完成后恢复普通主程序链接
cmake --build src/build --target fd2sdl --clean-first
```

该 sanitizer 子集不覆盖窗口输入；窗口回归仍须由普通构建运行。构建目录和可执行文件不入库。

## 调试与快速入口

| 命令 | 用途 |
|---|---|
| `./src/fd2sdl --map-preview 0` | 地图预览 |
| `./src/fd2sdl --field-preview 0` | 正式单位预览 |
| `./src/fd2sdl --field-play 0` | 第一关正式输入循环，当前只支持 stage 0 |
| `./src/fd2sdl --field-effect-play 0` | flash、震动与转场的定时验收序列 |
| `./src/fd2sdl --prologue-preview` | 新游戏初始过场 |
| `./src/fd2sdl --new-game-play` | 过场后交接正式战场 |

除 `--field-effect-play` 外，上述入口各有 `-once` 版本，例如 `--field-preview-once 0`、`--new-game-play-once`。它们是有限的资源／逻辑检查，不证明对应正式入口可达、完整输入或视听一致。

存档验证用可写副本，通过 `FD2SDL_SAVE_PATH` 指向副本；不要指向 `original_game/`。`FD2SDL_FIELD_RNG_SEED` 仅供正式输入回归固定随机流，不证明原版各次进入战场的实际初值相同。

## 逆向、镜像与运行时证据

| 任务 | 操作手册 |
|---|---|
| 日常函数／指令／xref 查询 | [IDA Domain API／idalib](ida.md)：skills、环境、只读查询及授权发布 |
| LE 重建、fixup 和完整字节检查 | [LE 镜像与验收](../reverse-engineering/le-validation.md)：产物、命令、独立性与失败条件 |
| 从原版重导出，避免覆盖数据库 | [IDA 9.5 复现](../reverse-engineering/ida-bytecheck-9.5.md#复现与本地证据) |
| 已加载映像／内存取证 | [DOSBox debugger](dosbox-debug.md)：当次 selector/base、转储和离线比较 |
| 必要的独立结构复核 | [Ghidra 结构验收](../reverse-engineering/ghidra-reconstruction.md)：不是日常语义基线 |

`dosbox-debug --help` 会启动界面，禁止用作帮助探测。不得自动操作原版玩法，也不得将旧捕获中的 object base 复制到新运行。r2／objdump／Ghidra 仅按需使用，所有地址先查[映射](../reverse-engineering/address-mapping.md)。

### relocation-backed 指针表

```bash
python3 tools/dump_fd2_fixup_table.py --ds 0x1d71 --count 30
python3 tools/dump_fd2_fixup_table.py --ds 0x1b91 --count 32
python3 tools/dump_fd2_fixup_table.py --object 3 --ds 0x27d8 --count 96
```

不要把未重定位镜像的 dword 直接当运行时指针；明确 object，再使用输出的 code0／relbase 坐标。

## 原版资源分析

```bash
python3 tools/dat_extract.py list original_game/FDOTHER.DAT
python3 tools/dat_extract.py dump original_game/FDOTHER.DAT 7
python3 tools/analyze_fdfield_events.py --stage 0
python3 tools/analyze_fd2_stage_code.py --stage 0
python3 tools/analyze_fd2_audio.py --json

# 只写临时目录，不提交原版或转换产物
work=$(mktemp -d /tmp/fd2-assets.XXXXXX)
python3 tools/dat_extract.py extract original_game/FDOTHER.DAT "$work/other"
python3 tools/analyze_fd2_audio.py --extract-sfx-dir "$work/sfx"
python3 tools/analyze_fd2_audio.py --extract-music-dir "$work/music"
```

这些提取工具面向本地原版样本，不代替生产资源解析器的损坏输入校验。`analyze_fd2_audio.py` 的历史 `code0_offset` 仍按旧 `0x10e00` 基址计算，不作 IDA 地址；仅使用其文件偏移／数据结果，规范换算以地址映射为准。

### 离线音频对照

```bash
python3 tools/compare_fd2_sfx_capture.py /tmp/capture.wav --sample 5
bash tools/render_fd2_opl_prototype.sh 1   # 也可传 all；外部评估，不是运行依赖
python3 tools/compare_fd2_music_capture.py /tmp/capture.wav /tmp/fd2-adlmidi-prototype/xmi
```

capture 必须附人工确认的场景与调用依据；比较脚本另需 NumPy，只处理已有音频，不自动操作 DOSBox。原型需要上游源码、网络和本地构建工具，默认在 `/tmp/fd2-adlmidi-prototype` 工作，可通过 `FD2_ADLMIDI_WORK`／`FD2_ADLMIDI_EMULATOR` 调整；每次会重建其 `xmi/`，不要在该目录存放人工证据。比较全部候选前先渲染 `all`。结果范围与已否决候选见[音频依据](../reverse-engineering/audio.md)。

## 二进制与工作区检查

```bash
xxd -g1 -s $((0x36014)) -l 128 original_game/FD2.EXE
sha256sum original_game/FD2.EXE
cmp -s expected.bin actual.bin  # 返回 0 才相同

git status --short
git diff --stat
git diff -- path/to/file
git diff --check
```

生成／本地产物和提交边界遵守 AGENTS。外部规范检索遵循其中的 `ketch → Pi Web Search → 可复核本地源码` 降级规则，搜索失败不构成格式结论。
