# 项目工具速查

以下命令从项目根目录执行。命令按 PowerShell／Bash 标注，Python、环境变量、临时目录、二进制比较和历史绝对路径的转换见[跨平台约定](platforms.md)。先查本文及专题，不为回忆语法反复调用 `--help`；工具版本／行为改变后再修订对应命令。开发约束见 [AGENTS.md](../../AGENTS.md)，完整分类见[文档索引](../README.md)。

## 构建与回归

开发验收执行当前平台的适用检查即可；下列另一平台命令用于维护兼容说明，不是当前任务必须补跑的前置。未执行的平台如实登记，当前平台必测项仍须实际注册并通过。

Windows（PowerShell，Visual Studio Debug）：

```powershell
cmake -S src -B src/build-win
cmake --build src/build-win --config Debug --parallel
ctest --test-dir src/build-win -C Debug --output-on-failure
python -m unittest discover -s tools/tests -p 'test_*.py'
python tools/ida/check_src_addresses.py
python -m unittest discover -s tools/tests -p test_ida_crosscheck.py
git diff --check
```

Linux（Bash，单配置生成器）：

```bash
cmake -S src -B src/build
cmake --build src/build --parallel
ctest --test-dir src/build --output-on-failure
python3 -m unittest discover -s tools/tests -p 'test_*.py'

# 无窗口资源／逻辑集成，正式窗口另测
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./src/fd2sdl --field-play-once

python3 tools/ida/check_src_addresses.py
(cd docs/generated/ida-canonical && sha256sum -c SHA256SUMS)
git diff --check
```

- 部分 CTest／Python 测试依赖原版资源或本地 LE 产物。Python seed／Ghidra 工具测试还需 `capstone`；缺依赖、跳过或未注册不能记为完整通过。
- `resource` 检查 .DAT／RLE 错误与原版资源兼容；`field_play_result` 单独编译正式循环并替换依赖，验证错误传播、胜／败、游戏内离场、宿主退出和移动取消；`field_turn` 检查 END 的恢复门、phase 顺序及提交后停止边界，均不启动窗口。
- `font` 使用合成字模逐字比较完整受保护 VGA 内存，检查下左／正下阴影、背景预清、字形 10、0 色写入及边缘裁切；不读取资源或启动窗口。
- `field_info` 同时覆盖 stage 0 总览的四块布局、可见人数门、数字边界及两段条件预检；缺资源或文本越界不修改画面。纯绘制测试不代替正式入口和原版 WGC 验收。
- `field_play_overview` 不启动窗口，验证总览的开合裁切、任意键关闭、held／repeat 生命周期、同帧输入清理、分配失败、宿主退出及玩法／RNG 不变；另读取真实资源检查干净背景。
- `field_player_controller` 检查玩家整路径收集、取消重入、原地WAIT恢复、acted后分派、精确13565门及共享slot跨phase覆盖；`field_play_result`另检查Enter详情与F2来源、controller尾按键耗尽。原版具体格事件与镜头仍按场景验收。
- `field_geometry` 检查原 anchor 优先、整条长路径扫描、局部 profile 覆盖与零预算，并用只读真实 START 逐角色核对已取得的原版终点。它不执行 March 批次、AI／phase 或保存，不能代替正式行军配对。`fd2_movement_oracle.py` 复用 LE loader，从 canonical `0x61646` 提取全部 580 字节并校验摘要；玩家图形 runner 共用此依据，禁止再硬切旧文件偏移。
- `field_water` 检查 DAC224..239 的精确写集、全部16色窗口、低16位／phase回绕、长停留只一步、Load／新session不重置、纯绘制不推进。`audit_fd2_water.py` 只读比对原版 LE 的93字节有效读集与C的48字节周期，并核对state fixup；它不输出镜像，不等于动态画面或硬件时序验收。
- `field_unit_render` 检查单actor帧／坐标、acted映色、透明、bank不变、边裁与深度顺序；`python tools/audit_fdicon_opaque_zero.py original_game/FDICON.B24`只读核对原始RLE的opaque0与skip，Linux使用`python3`。
- `field_play_input` 驱动真实 SDL 键盘，不直接调用玩法 API。Windows 使用原生交互桌面；Linux/X11 需要 X11、XTest 和 Xvfb。用 `ctest --test-dir src/build-win -C Debug -N` 或 `ctest --test-dir src/build -N` 核实测试清单；未注册不算已测。
- Windows `graphics_job` 实际检查 worker／子孙进程的退出和超时清理；图形 runner 通过驱动 `run-job` 约束子进程树，正常场景的强制清理仍判失败。
- `test_ida_crosscheck` 不需要 IDA，检查发布完整性、报告结构和源码规范地址；不等于重新跑完整 IDA 字节对照。

### 图形自动化

Windows（PowerShell）：

```powershell
python tools/run_graphics_tests.py --game src/Debug/fd2sdl.exe --build-dir src/build-win --config Debug
```

运行条件、Linux 对应入口和测试范围见[图形自动化测试](gui-testing.md)。当前 Windows 的 SDL 与 DOSBox 截图统一使用 WGC 单窗口捕获。报告和截图写入构建目录下的 `graphics-artifacts/`，不提交仓库；自动化仅操作本次启动并识别的测试进程。原版 DOSBox 自动对照使用隔离副本及独立判据，不把 SDL 截图自检当作原版一致性证据。

总流程先执行 `run_original_opening.py` 的自然新游戏引导交接，再执行 `run_original_focus.py` 的四向边缘聚焦与完整 COMMAND 显示，随后串行验证标题、菜单和动作。前两项分别使用自然 START 与精确坐标派生副本，不能互相替代。单独复跑命令、快照前置条件和每项证据范围统一见图形测试规范；END 确认框／取消不提交回合，不替代整回合的 phase／AI／随机结果验收。

另以 `run_original_end_turn.py` 的真实 START 副本提交一次 END，识别 actor 0 最终焦点并正式 Save，比较回合、计数、配置等确定性字段；已加入总流程。相机、伤害和 AI 随机结果仍不作相等声明。

`run_original_load.py` 在 round2 不保存，再用正式 Load 恢复真实 START，最后显式 Save 测量；Load 前后同时核对文件内容与修改时间，测量时只排除已确认 cache class 和导航焦点差异。该项也在总流程中串行执行。

`run_original_leave.py` 比较 Esc／NO 和肯定离场后的自然退出；清理前必须已观察 exit `0`，停在标题再由清理关窗不会通过。已纳入总流程。

`run_original_overview.py` 比较 START 总览的四个完整静态区域、数字和条件文字，分别用 Enter／Esc／Right／A 关闭，检查焦点和文件未变；再从正式 Save 测量完整容器，并测试页面内关闭窗口。已纳入总流程；瞬时动画像素、精确时长、地图和声音另行验收。

`run_original_player.py` 从真实 START 定位 actor0，验证一步移动取消恢复选择态、确认可直接进入 COMMAND、取消返回浏览及磁盘只读。COMMAND 检查完整 24×20 图标与 actor 遮挡组合，分别识别普通、禁用及高亮资源，不把 SDL 尚未开放的能力当作原版禁用规则。`--scenario wait-full-hp` 要求来源 HP 已满，另验原地 WAIT、已行动映色及 Enter 详情入口；受伤恢复、raw26 抖动和完整 runtime／RNG 仍待原版配对。两场景均已纳入总流程。

`run_original_march.py` 使用真实 START，先跑原版再跑 SDL，逐步识别 March 取消、NO、肯定一次和 round2 的实际资源画面，再由正式菜单 Save 测量。它比较本次原版四名玩家终点，并严格检查完整容器中未声明字段、整张 roster、metadata、手工槽和未知区保留；伤害、AI 位置、cache 与随机流不作相等声明。单独运行仍须使用 `driver run-job` 管理进程树，已纳入总流程第 20 组。`field_march` 覆盖精确 actor 门、pending／acted 顺序、部分提交失败、March 与普通 END 呈现隔离；纯逻辑不能代替该 WGC 配对。

`run_original_f1.py` 为总流程第 21 组。每端按原始 terrain、标记及合法水色表严格检查完整64,000像素；每页首匹配锁定周期候选，之后完整命中另一不相交合法周期即失败，不能重试跳过漂移。原版先取得至少两个标记相位，SDL再等相同相位。直接RGB差异、容差内相等和原始RGB完全相等分别报告，不做颜色归一，也不排除水／黑边／零色。`field_play_auxiliary` 另验全部8个标记相位、7开／6关、输入隔离、资源预检及水色恢复一步；实际WGC捕获范围以报告为准。

`run_original_load.py` 还逐键验证Z轮询raw0→1→2→3→0，再Load后仅按一次Z，必须回到raw0；之后按来源推导的无滚屏路径回原焦点，原完整存档测量门不增加allowlist。专属FDICON＋光标判据拒绝错误角色、裸角色／裸光标及歧义，缺ACK时不补按键。新增坏sprite编号和恢复胜败门属于SDL存档回归，畸形记录不送入DOSBox。

`run_original_result.py` 是第22组：从真实START只改已确认hidden位并重算checksum，保留完整派生manifest；原版按资源门逐键到达胜利首对白、败北frame0及自动GAME OVER frame1，SDL从同来源独立报告胜／败并自然exit2。测试不推进结算或回标题，不声称整关实战。共享进程helper只在本SDL场景显式接受exit2，其他窗口仍要求默认exit0，强制清理不得记为通过。单独运行使用与其他配对相同的 `driver run-job`、`--root/--game/--driver/--dosbox/--artifacts` 参数。

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

该 sanitizer 子集不覆盖窗口输入；窗口回归仍须由普通构建运行。当前 Windows 任务不要求补跑该 Linux 子集；Windows 普通 CTest 通过也不登记为 Linux ASan／UBSan 通过，Windows MSVC 不能直接使用上述 Clang 参数。构建目录和可执行文件不入库。

## 调试与快速入口

以下为 Linux 主程序路径；Windows Visual Studio Debug 将 `./src/fd2sdl` 替换为 `./src/Debug/fd2sdl.exe`，参数相同。Release／Ninja 路径见跨平台约定。

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

`dosbox-debug --help` 会启动界面，禁止用作帮助探测。原版玩法自动化必须运行独立副本，先确认逆向依据、场景步骤和通过判据，保留截图／存档／输入记录／必要 trace；不得将旧捕获中的 object base 复制到新运行。r2／objdump／Ghidra 仅按需使用，所有地址先查[映射](../reverse-engineering/address-mapping.md)。

### relocation-backed 指针表

以下为 Bash 命令；PowerShell 将 `python3` 换成 `python`，参数不变。

```bash
python3 tools/dump_fd2_fixup_table.py --ds 0x1d71 --count 30
python3 tools/dump_fd2_fixup_table.py --ds 0x1b91 --count 32
python3 tools/dump_fd2_fixup_table.py --object 3 --ds 0x27d8 --count 96
```

不要把未重定位镜像的 dword 直接当运行时指针；明确 object，再使用输出的 code0／relbase 坐标。

## 原版资源分析

Windows（PowerShell）：

```powershell
python tools/dat_extract.py list original_game/FDOTHER.DAT
python tools/dat_extract.py dump original_game/FDOTHER.DAT 7
python tools/analyze_fdfield_events.py --stage 0
python tools/analyze_fd2_stage_code.py --stage 0
python tools/analyze_fd2_audio.py --json

$work = Join-Path ([System.IO.Path]::GetTempPath()) ('fd2-assets-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $work | Out-Null
python tools/dat_extract.py extract original_game/FDOTHER.DAT "$work/other"
python tools/analyze_fd2_audio.py --extract-sfx-dir "$work/sfx"
python tools/analyze_fd2_audio.py --extract-music-dir "$work/music"
```

Linux（Bash）：

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

两平台的比较脚本都只读取已有音频，另需 NumPy。capture 必须附确认的场景和调用依据，可由有判据的 DOSBox 自动取证产生。

Windows（PowerShell；先将路径换成实际捕获和候选目录）：

```powershell
python tools/compare_fd2_sfx_capture.py "$work/capture.wav" --sample 5
python tools/compare_fd2_music_capture.py "$work/capture.wav" "$work/xmi"
```

Linux（Bash）：

```bash
python3 tools/compare_fd2_sfx_capture.py /tmp/capture.wav --sample 5
bash tools/render_fd2_opl_prototype.sh 1   # 也可传 all；外部评估，不是运行依赖
python3 tools/compare_fd2_music_capture.py /tmp/capture.wav /tmp/fd2-adlmidi-prototype/xmi
```

`render_fd2_opl_prototype.sh` 是 Linux／Bash 外部评估脚本，不是 Windows 原生构建步骤，也不是运行依赖。Windows 可直接比较已有候选 WAV；需要重新生成历史原型时，在具有 Linux 编译器、CMake、Python 3 和 Git 的 WSL 中执行 Bash 命令，证据登记为 WSL／Linux 结果。原型需要上游源码、网络和本地构建工具，默认在 `/tmp/fd2-adlmidi-prototype` 工作，可通过 `FD2_ADLMIDI_WORK`／`FD2_ADLMIDI_EMULATOR` 调整；每次会重建其 `xmi/`，不要在该目录存放捕获证据。比较全部候选前先渲染 `all`。结果范围与已否决候选见[音频依据](../reverse-engineering/audio.md)。

## 二进制与工作区检查

Windows 的 SHA-256、字节比较和十六进制读取见[跨平台约定](platforms.md#哈希二进制比较与发布清单)；以下二进制工具示例限定 Linux／Bash，Git 命令适用于两平台。

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
