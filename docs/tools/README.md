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
- `field_info` 覆盖stage 0总览及物品纯列表：raw槽选中、空槽压行、全部215项、frame41缺省属性图、raw零色复制，以及完整预检失败全VGA不变；原详情与MAGIC上页组合另有回归。纯绘制测试不代替正式入口和原版WGC验收。
- `field_item_select` 覆盖共享8槽计数、纯列表导航／Use确认／取消、稀疏槽和未知ID支持门，检查错误无写入及单位全记录不变。START三槽为已确认字节构造样例，不直接读取存档；没有物品UI、效果或原版运行验收。
- `field_item_flow` 覆盖ITEM_MENU／ITEM_LIST的纯session取消层级、重开归0、临时移动／pending保留、错误无写入及Load清理；list step本身不提交handler。另验Drop的mode0装备确认、确认／取消重算、末槽stale、删空回COMMAND、隐藏坏装备拒绝及RNG0。正式Use能力门／confirm_cursor与目标提交另有回归，原版单端校准与SDL图形配对分别登记。
- `field_item_target` 覆盖草药192的纯目标范围、raw候选／121轮询、HP0／acted、取消聚焦回LIST0、坏末项及Load owner；合成光标／地形锁定frame1与LUT，不执行物品提交或代替WGC。
- `field_item_commit` 覆盖草药逻辑提交的power40／逐目标RNG、压槽及stale ID、MP／EX保留、死亡／phase预测、重复拒绝、提交后错误冻结及正式能力查询／confirm_cursor分派；不借AI尾链，也不代替真实键盘可达性或原版raw字节量测。
- `field_magic_list_play` 现同时覆盖法术／物品私有列表的真实按键等待逻辑，替换宿主时钟与呈现，不开窗口；检查共享RNG、开合、退出、释放和首错。`field_item_menu_render` 用合成资源检查ITEM IDs8–11、禁用图、高亮、动画与角色最后覆盖；二者均不是SDL物品WGC。
- `field_play_overview` 不启动窗口，验证总览的开合裁切、任意键关闭、held／repeat 生命周期、同帧输入清理、分配失败、宿主退出及玩法／RNG 不变；另读取真实资源检查干净背景。
- `field_player_controller` 检查玩家整路径收集、取消重入、原地WAIT恢复、acted后分派、精确13565门及共享slot跨phase覆盖；`field_play_result`另检查Enter详情与F2来源、controller尾按键耗尽。原版具体格事件与镜头仍按场景验收。
- `field_geometry` 检查原 anchor 优先、整条长路径扫描、局部 profile 覆盖与零预算，并用只读真实 START 逐角色核对已取得的原版终点。它不执行 March 批次、AI／phase 或保存，不能代替正式行军配对。`fd2_movement_oracle.py` 复用 LE loader，从 canonical `0x61646` 提取全部 580 字节并校验摘要；玩家图形 runner 共用此依据，禁止再硬切旧文件偏移。
- `field_water` 检查 DAC224..239 的精确写集、全部16色窗口、低16位／phase回绕、长停留只一步、Load／新session不重置、纯绘制不推进。`audit_fd2_water.py` 只读比对原版 LE 的93字节有效读集与C的48字节周期，并核对state fixup；它不输出镜像，不等于动态画面或硬件时序验收。
- `field_unit_render` 检查单actor帧／坐标、acted映色、透明、bank不变、边裁与深度顺序；`python tools/audit_fdicon_opaque_zero.py original_game/FDICON.B24`只读核对原始RLE的opaque0与skip，Linux使用`python3`。
- `field_play_input` 驱动真实 SDL 键盘，不直接调用玩法 API。Windows 使用原生交互桌面；Linux/X11 需要 X11、XTest 和 Xvfb。用 `ctest --test-dir src/build-win -C Debug -N` 或 `ctest --test-dir src/build -N` 核实测试清单；未注册不算已测。
- Windows `graphics_job` 实际检查 worker／子孙进程的退出和超时清理；图形 runner 通过驱动 `run-job` 约束子进程树，正常场景的强制清理仍判失败。
- `test_ida_crosscheck` 不需要 IDA，检查发布完整性、报告结构和源码规范地址；不等于重新跑完整 IDA 字节对照。

### A8 音频生命周期验证

普通音频专项不创建窗口。`audio_lifecycle` 以单 TU 平台替换覆盖 Open／Resume／Put 故障和队列压力；`audio_device_probe_finalization` 覆盖实际 probe 的末次提交错误；`audio_pcm_capture_callback_order` 检查 Put 失败不能留下成功 AppMix summary。`audio_unavailable_field_quick_entry` 启动真正主程序，以 dummy video 和无效音频后端验证 discard-null 快速入口，不算原生窗口或设备播放通过。交付候选的实际结果见[阶段记录](../plans/stage-log.md#a8宿主与设备生命周期)，以下命令不自行构成通过记录。

Windows（PowerShell，已配置的 Visual Studio Debug 构建）：

```powershell
cmake --build src/build-win --config Debug --target fd2sdl fd2_audio_test fd2_audio_lifecycle_test fd2_audio_pcm_capture_test fd2_audio_pcm_capture_hook_test fd2_audio_device_probe_finalization_test fd2_audio_pcm_test fd2_bgm_test fd2_field_progress_play_test fd2_field_unit_render_test --parallel 4 -- /nodeReuse:false
ctest --test-dir src/build-win -C Debug -R '^(audio|audio_lifecycle|audio_pcm_capture_boundaries|audio_pcm_capture_callback_order|audio_device_probe_finalization|audio_pcm|bgm|audio_unavailable_field_quick_entry|field_progress_play|field_unit_render)$' --output-on-failure
```

Linux 使用前文的独立 `src/build` 配置和构建，再运行同一组无窗口测试；本轮 Linux 未执行：

```bash
ctest --test-dir src/build -R '^(audio|audio_lifecycle|audio_pcm_capture_boundaries|audio_pcm_capture_callback_order|audio_device_probe_finalization|audio_pcm|bgm|audio_unavailable_field_quick_entry|field_progress_play|field_unit_render)$' --output-on-failure
```

实际设备 probe 不注册为自动 CTest。它只初始化 SDL audio，实际 render 后向设备提交等长零，检查 source 替换、两个 bus 恢复、stop／retire／close；不改变系统音量、默认端点或用户 session mixer。无参数或 `--dry-run` 不初始化 SDL。显式 `--execute 100..10000_MS` 才打开本进程的设备，须串行置于 owned Job：

```powershell
cmake --build src/build-win --config Debug --target fd2_audio_device_probe fd2_field_play_input_driver
& ./src/build-win/Debug/fd2_field_play_input_driver.exe run-job 30000 ./src/build-win/Debug/fd2_audio_device_probe.exe --execute 1500
```

须核对 JSON `ok=true`、所有 Put 成功、非零设备提交为0、三个 source 各 retire 一次，以及进程／Job 正常关闭。source API 替换不等于正式 Game Load，quiet/pre-OS 不等于扬声器声音通过。

#### 固定 SDL 内部 ABI 的本地断连 probe（仅 Windows x64 Debug）

`tools/testing/sdl-local-disconnect` 独立链接现有 SDL 静态库，模拟该进程收到原 SDL removed 通知，不重编 SDL、不禁用系统端点。配置必须显式提供 `SDL3_DIR`、`SDL3_SOURCE_DIR` 和 `SDL3_BUILD_CONFIG_DIR`。目前只接受已验证 commit `2813f2630904ea29806fcfaeeb3287ed555e42e2` 的特定 Debug 静态库 SHA256 `5d920411374519962ce4dacbd04c86d210a4c71dea243e5d922779392dcbb090`，并校验 `SDL_sysaudio.h`、`SDL_internal.h` 和生成的 `SDL_build_config.h` 三个头文件摘要。即使 commit 相同，另一构建的库或配置头摘要不同也会拒绝；不回退到另一套 SDK 或 DLL。

从项目根目录执行。下面路径是本机已验证依赖位置；其它环境必须先取得同版本源码、配置头和相同静态库：

```powershell
$sdlPackage = 'E:/Projects/fd2sdl/src/build-win/_deps/sdl3-build'
$sdlSource = 'E:/Projects/fd2sdl/build/_deps/sdl3/2813f2630904ea29806fcfaeeb3287ed555e42e2'
$sdlBuildConfig = "$sdlPackage/include-config-debug/build_config"
cmake -S tools/testing/sdl-local-disconnect -B build-a8-sdk -G 'Visual Studio 18 2026' -A x64 "-DSDL3_DIR=$sdlPackage" "-DSDL3_SOURCE_DIR=$sdlSource" "-DSDL3_BUILD_CONFIG_DIR=$sdlBuildConfig"
cmake --build build-a8-sdk --config Debug --target fd2_sdl_local_disconnect_test fd2_audio_device_loss_probe -- /nodeReuse:false /m:1
ctest --test-dir build-a8-sdk -C Debug -R '^private_sdl_local_disconnect$' --output-on-failure
./build-a8-sdk/Debug/fd2_audio_device_loss_probe.exe --dry-run
```

上述 CTest 只有九组纯边界检查，不初始化 SDL 或打开设备。实际本地断连验证须另行串行执行，不能把 dry 或纯测算设备通过：

```powershell
& ./src/build-win/Debug/fd2_field_play_input_driver.exe run-job 30000 ./build-a8-sdk/Debug/fd2_audio_device_loss_probe.exe --execute 1500
```

最终两行 JSON 都须 `ok=true`，并核同一 logical／physical owner、ref 成对、zombie0→1、Wait／GetBuffer／Play 三函数切换、断连后 callback／Put 继续、新 source／配置恢复／retire 及正常 Job 清理。probe 只操作本进程 SDL 对象；不证明真实拔插、WASAPI 恢复、默认路由迁移、完整波形或绝对时序。该私有 ABI 入口无 Linux 对应命令，不能移用其它 SDL 版本或 Release 库。

### 图形自动化

Windows 已验证按本轮 PID／HWND 投递窗口消息的后台输入：DOSBox-X 与 SDL 在其他应用持有前台时完成标题导航、END 确认框及取消返回浏览，仍使用 HWND WGC 和完整资源门。驱动不抢焦点、不退回前台 `SendInput`；该证据不覆盖物理键盘或 RawInput。支持范围及监测限制见[后台窗口消息校准](gui-testing.md#windows-hwnd-窗口消息后台输入已验证)，旧前台执行证据保留原来源。永久入口 `run_background_input_check.py` 最初等价迁移后又新增 HWND／PID 所有权保护，已不与旧 GUI 候选 AST 等同；最初的 13 项纯回归与后来大阶段统一回归中的实际后台校准分别保留来源。

独立后台校准必须由 `run-job 600000` 管理进程树，产物目录须为新目录；内部 `--sentinel` 不单独运行。Windows PowerShell：

```powershell
& src/build-win/Debug/fd2_field_play_input_driver.exe run-job 600000 python -B tools/run_background_input_check.py --root . --game src/Debug/fd2sdl.exe --driver src/build-win/Debug/fd2_field_play_input_driver.exe --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --artifacts "$env:TEMP/fd2-background-input-check"
```

Windows（PowerShell）：

```powershell
python tools/run_graphics_tests.py --game src/Debug/fd2sdl.exe --build-dir src/build-win --config Debug
```

运行条件、Linux 对应入口和测试范围见[图形自动化测试](gui-testing.md)。当前 Windows 的 SDL 与 DOSBox 截图统一使用 WGC 单窗口捕获。报告和截图写入构建目录下的 `graphics-artifacts/`，不提交仓库；自动化仅操作本次启动并识别的测试进程。原版 DOSBox 自动对照使用隔离副本及独立判据，不把 SDL 截图自检当作原版一致性证据。

不验片头动画的场景允许仅在启动等待内发送 Esc 加速（首 1 秒、随后每 2 秒），完整标题资源门通过后立即停止；由场景决定是否启用，命令不增加参数。片头动画专项保持无 skip，并单列实际覆盖，不能把跳过片头当作动画通过。B5 标题 START 后的 117 页引导仍逐页资源 ACK 并核对自然站位，不使用启动 skip 绕过剧情；历史无 skip 证据保留原来源。开启skip本身不能替代该候选的实际验收。

B5受支持范围已完成15个唯一场景；自然首关由旧完整原版、新门离线导航复核、短程双端校准和独立新SDL全链联合登记，非单轮同源配对。其后的大阶段统一审查和历史全面回归已有独立完成记录，不反写为旧 B5 的单轮来源。逐候选及 Job 来源见[图形规范](gui-testing.md#b5-结算准备页与下一关)与[阶段记录](../plans/stage-log.md)。

默认完整 Windows 总流程先执行 `run_original_opening.py` 的自然新游戏引导交接，再执行 `run_original_focus.py` 的四向边缘聚焦与完整 COMMAND 显示，随后以 600 秒 Job 运行后台校准，再进入 input、标题、菜单和动作；B3／B4／B5-only 不调用后台历史回归。后台注册已有大阶段实际 GUI 回归记录，后续阶段仍只测自身及直接受影响路径。前两项分别使用自然 START 与精确坐标派生副本，不能互相替代。单独复跑命令、快照前置条件和每项证据范围统一见图形测试规范；END 确认框／取消不提交回合，不替代整回合的 phase／AI／随机结果验收。

另以 `run_original_end_turn.py` 的真实 START 副本提交一次 END，识别 actor 0 最终焦点并正式 Save，比较回合、计数、配置等确定性字段；已加入总流程。相机、伤害和 AI 随机结果仍不作相等声明。

`run_original_load.py` 在 round2 不保存，再用正式 Load 恢复真实 START，最后显式 Save 测量；Load 前后同时核对文件内容与修改时间，测量时只排除已确认 cache class 和导航焦点差异。该项也在总流程中串行执行。

`run_original_leave.py` 比较 Esc／NO 和肯定离场后的自然退出；清理前必须已观察 exit `0`，停在标题再由清理关窗不会通过。已纳入总流程。

`run_original_overview.py` 比较 START 总览的四个完整静态区域、数字和条件文字，分别用 Enter／Esc／Right／A 关闭，检查焦点和文件未变；再从正式 Save 测量完整容器，并测试页面内关闭窗口。已纳入总流程；瞬时动画像素、精确时长、地图和声音另行验收。

`run_original_player.py` 从真实 START 定位 actor0，验证一步移动取消恢复选择态、确认可直接进入 COMMAND、取消返回浏览及磁盘只读。COMMAND 检查完整 24×20 图标与 actor 遮挡组合，分别识别普通、禁用及高亮资源，不把 SDL 尚未开放的能力当作原版禁用规则。`--scenario wait-full-hp` 要求来源 HP 已满，另验原地 WAIT、已行动映色及 Enter 详情入口；受伤恢复、raw26 抖动和完整 runtime／RNG 仍待原版配对。两场景均已纳入总流程。

`run_original_march.py` 使用真实 START，先跑原版再跑 SDL，逐步识别 March 取消、NO、肯定一次和 round2 的实际资源画面，再由正式菜单 Save 测量。它比较本次原版四名玩家终点，并严格检查完整容器中未声明字段、整张 roster、metadata、手工槽和未知区保留；伤害、AI 位置、cache 与随机流不作相等声明。单独运行仍须使用 `driver run-job` 管理进程树，已纳入总流程。`field_march` 覆盖精确 actor 门、pending／acted 顺序、部分提交失败、March 与普通 END 呈现隔离；纯逻辑不能代替该 WGC 配对。

`run_original_f1.py` 已纳入总流程。每端按原始 terrain、标记及合法水色表严格检查完整64,000像素；每页首匹配锁定周期候选，之后完整命中另一不相交合法周期即失败，不能重试跳过漂移。原版先取得至少两个标记相位，SDL再等相同相位。直接RGB差异、容差内相等和原始RGB完全相等分别报告，不做颜色归一，也不排除水／黑边／零色。`field_play_auxiliary` 另验全部8个标记相位、7开／6关、输入隔离、资源预检及水色恢复一步；实际WGC捕获范围以报告为准。

`run_original_load.py` 还逐键验证Z轮询raw0→1→2→3→0，再Load后仅按一次Z，必须回到raw0；之后按来源推导的无滚屏路径回原焦点，原完整存档测量门不增加allowlist。专属FDICON＋光标判据拒绝错误角色、裸角色／裸光标及歧义，缺ACK时不补按键。新增坏sprite编号和恢复胜败门属于SDL存档回归，畸形记录不送入DOSBox。

`run_original_result.py --b5-defeat` 已纳入当前总流程：从真实START只改已确认hidden位并重算checksum，保留完整派生manifest；两端验证败北frame0、自动GAME OVER完整合成及返回标题，清理前必须已观察到标题。它不证明自然战败或胜利结算。省略该参数保留历史exit2能力门校准，不适用于已接入后续流程的正式App。单独运行使用与其他配对相同的 `driver run-job`、`--root/--game/--driver/--dosbox/--artifacts` 参数；强制清理不得记为通过。

`run_original_magic_list.py` 的列表、伤害和治疗双端场景也已纳入总流程。正式输入必须经过列表和目标选择，仅确认一次施法，分别校验完整静态资源矩形、效果与MP／LV／EX；原版单端校准和配对输出口径不同，参数及夹具限制见[图形测试规范](gui-testing.md)。失焦补跑保留失败及来源，累计结果不能冒称单轮通过。

`run_original_item_list.py` 支持B3的Use／Give／Equip／Drop原版单端校准及串行双端配对。默认controls检查真实START raw0的四向ITEM子菜单、三槽Use导航、code0拒绝与逐级取消；herb-full／herb-injured检查目标取消及单次Use详情。code5..24另有20项原表实例、真实item102双目标area2及code23零MP瞬移；它们验证实际效果、合法END与完整Save。受控位置、库存和身份差异在各自manifest明列，不冒充自然整关路线。物品表及期望取自严格LE／原码，不以SDL为oracle；不传`--sdl`仍是`ok=false/probe_only=true`单端校准，双端需各自资源门、实际PNG及终值／文件守卫全部通过。完整共享RNG／演出声音另验。Windows命令、逐场覆盖及无窗口回归见[物品菜单规范](gui-testing.md#b3物品菜单校准与双端配对)。

`--scenario drop-herb/drop-weapon/drop-armor/drop-last`各选一场，覆盖未装备草药、已装备武器／防具及最后一项。添加`--sdl`时先运行原版并清理，再运行SDL；四场均已登记实际双端配对及完整Save，核对raw16B、末ID、派生值和未知字节。取消、一次删除、删空回COMMAND、逐级取消与正式Save按同一规范执行；正式Drop已开放，早期仅原版校准的来源仍保留在阶段记录。

B5 的统一入口为 `--b5-only`，已登记准备／酒店四槽、正式败北、10类掉落／特殊格、stage 1 两场和 `natural-stage0`，共15项；完整套件默认也包含它们。用 `--b5-scenarios` 选择本阶段子集；未知、重复、空选择及与 B3／B4 的互斥组合在创建产物前拒绝。Windows 已核对的入口如下：

```powershell
cmake --build src/build-win --config Debug --target fd2sdl fd2_field_play_input_driver fd2_field_natural_plan --parallel
python tools/run_graphics_tests.py --game src/Debug/fd2sdl.exe --build-dir src/build-win --config Debug --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --b5-only
```

自然场景的规划器默认查 `build-dir/CONFIG/fd2_field_natural_plan.exe`，再查单配置构建目录；可用 `--planner` 覆盖，不存在时在窗口启动前拒绝。stage 1 默认使用本轮 `prepare-controlled` 生成的原版活动 Save；没有先运行准备场景时须提供独立只读 `--b5-save-fixture`。自然流程默认最多50轮，败北、重复语义状态或到达上限均保留失败，不重开／重抽；调度器只为原版首个实际阶段结束周期录一次音频，作为 A5 曲目身份／切换证据，不是 A7 保真。所有 WGC 场景串行运行，先清理原版再启 SDL，正常／错误／超时均清理本轮 Job，失败收据不覆盖。

B5受支持范围已完成15项的实际证据验收；自然首关为不同来源的独立运行联合验证，不是完整Suite单轮通过。后台基础校准、旧失败与各冻结来源分别保留；其后的大阶段统一审查及历史全面回归现有独立完成记录，不反写为旧B5来源。累计结果统一见[阶段记录](../plans/stage-log.md)。上述命令沿用已有参数；具体15项、Windows子集命令、存档前置、Linux无窗口／规划器兼容说明与未执行范围见 [B5 图形测试规范](gui-testing.md#b5-结算准备页与下一关)；本轮没有 Linux 图形或规划器验收结果。

### B7 第二关与 stage 2 交接

统一工具为 `run_original_stage1_complete.py`，不是 B5 的 `run_original_stage1.py`。当前接受 9 种场景：3 种受控胜利、`chest-129`／`chest-201`／`chest-193`、`villager-defeat`、`stage2-storage` 和 `natural-stage1`。B7既定9种场景已按实际候选完成双端验收，C14另复验两条结算分支的光标修复；来源与未覆盖范围见阶段记录。工具登记、纯测试和只读来源前检本身仍不等于场景通过。

- 必填参数为 `--root`、`--driver`、`--dosbox`、`--save-fixture`、`--source-report`、`--scenario`、`--artifacts`；双端另传 `--sdl`。所有场景由 Windows `driver run-job` 串行管理，使用 WGC HWND 和后台定向输入；成功、失败、超时均按 owned PID 清理。
- 3 种受控胜利与三箱使用已完成 script13／9 页 ACK 的真实 turn4／27 人原版来源。该旧受控源含主角 MOV40 加速值；三箱按确证原表与同身份自然记录恢复该主角 MOV4，单列 manifest，不把加速预算或捕获值作为通用默认。败北和自然整关使用已完成 25 页入场 ACK 的自然 stage 1 原版来源。`stage2-storage` 使用本次 B7 实际 stage 2 入场 Save、15 页 ACK 和同一原版端报告。不能混用顶层聚合报告或只复制一份 SAV。
- `natural-stage1` 另需 `--planner EXE`，`--max-turns` 默认 60、允许 `1..100`。败北、停滞或超限保留失败，不重开或重抽。可选 `--public-app PATH` 只守卫该实际文件，不锁旧 SHA，也不代替待测 `--sdl`。
- 每场 `--artifacts` 必须是独立新目录。Windows 的真实 DOSBox EXE、PowerShell 九场示例、来源准备及 Linux 对应纯测命令统一见 [B7 图形测试规范](gui-testing.md#b7-第二关续战村民结算与-stage-2-交接)。该配对工具仅支持 Windows WGC，本轮 Linux 未验。

首结算页的576像素光标格可用 `tools/verify_stage1_settlement_cursor.py --root ROOT --artifacts CASE/artifacts --output NEW.json` 离线复核；输出必须新建，既有图、来源及报告不改。PowerShell／Bash完整命令和严格适用前置见上述B7图形规范；静态复核不算新GUI或连续时序验收。

### 连续画面与进程音频取证（Windows）

B6 取证新增以下入口；采集成功不自行判定整段动画或声音一致。B6／A6受支持实现及既定最低验收已完成，场景总数19：3核心最低帧／SFX、15项AI最低casting与业务／保存／采集、1项`ai-item-score20`实际评分回归。AI由C6十四项＋C8仅08multi复验覆盖；完整生命周期与判据遵循[图形测试规范](gui-testing.md#b6-战斗演出采集与判据)，来源统一见[阶段记录](../plans/stage-log.md#b6a6full-战斗演出与动态音效)。进程音频使用 Windows process-loopback，要求系统 build **20348 或更新**；不回退到桌面录音，也不依赖前台窗口。

```powershell
cmake --build src/build-win --config Debug --target fd2_field_play_input_driver fd2_process_audio_capture
```

以下是父 runner 异步启动 owned 子进程的参数格式，不是顺序阻塞执行的示例：

| 程序 | 参数 | 上限与输出 |
|---|---|---|
| `fd2_field_play_input_driver` | `capture-series PID OUTDIR MAX_MS INTERVAL_MS` | 两个时长均为 `1..120000` ms，最多600帧；OUTDIR须不存在、父目录已存在 |
| `fd2_process_audio_capture` | `PID WAV MAX_MS READY_FILE STOP_FILE` | `MAX_MS=1..120000`；三个绝对输出路径互异、原先不存在，父目录已存在；PCM16／stereo／44100 Hz |

两者都在本轮 `driver run-job` 进程树内启动，父 runner 不激活游戏窗口。持续 WGC 复用同一 PID／HWND 会话，首张真实 PNG 与 `frames.jsonl` 落盘后才原子发布 `OUTDIR/ready.json`；父端等 ready 后才确认动作，创建 `OUTDIR/stop` 正常结束。帧按实际 WGC 时间戳去重和限制最小保存间距，不补旧图或承诺固定帧率；保留原始 RGB、渐黑／闪白和每帧尺寸／时间／owner信息，不以调色或裁掉随机区促成通过。

进程音频 ready 绑定 PID、创建时间和进程树，记录 QPC／格式；ready 不等于已观察到首个 PCM packet。父端写 STOP_FILE 后等待 helper 封好 WAV 和最终 JSON，核对退出码、HRESULT、包时间和实际帧数，再关闭目标并完成 Job 清理。目标提前退出是失败；超时／600帧截断、未正常 stop 或强制清理不能记为完整采集。原版使用独立副本的 DOSBox 原生 HOST_W 录音；该 WAV 无首 packet QPC，只能据录音内时点比较，不虚构输入绝对延迟。Linux 没有这些 Windows 采集入口，本轮也无 Linux 对应执行结果。

核心场景的`run_original_battle_animation.py --verify-core`仅接受`physical-full-hit`／`player-magic0`／`player-heal13`且必须有SDL端，完整清理与晚守卫后核对完整帧和两端最低SFX门，未满足退出1；不传该参数仍是`ok=false/collected_only=true`的采集报告。AI15场不因采集退出0升格为全FX／声音通过。Suite的`--b6-only`和核心子集参数复用此严格门，具体命令只在[GUI规范](gui-testing.md)维护。

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
| 日常函数／指令／xref 查询 | [IDA Domain API／idalib](ida.md)：skills、环境、隔离建库／查询及生成发布；项目数据库操作不再逐次授权 |
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

两平台的比较脚本都只读取已有音频；旧比较器需 NumPy，新的有限 SFX oracle 另需 SciPy。capture 必须附确认的场景和调用依据，可由有判据的 DOSBox 自动取证产生。

`fd2_bgm_capture_probe TRACK SECONDS NEW_OUTPUT.wav` 使用当前原 EVNT／AIL／Nuked 后端，20个逻辑槽、默认18个 OPL硬件channel，生成44100 Hz、双声道16位离线参考；曲目范围0..19，时长1..120秒，输出路径必须不存在。JSON明确输出 `logical_voices=20` 和 `opl_channels=18`，不能继续以旧九voice元数据登记新素材。从项目根目录执行：

```powershell
cmake --build src/build-win --config Debug --target fd2_bgm_capture_probe
& ./src/build-win/Debug/fd2_bgm_capture_probe.exe 10 20 "$env:TEMP/fd2-track10-new.wav"
```

```bash
cmake --build src/build --target fd2_bgm_capture_probe
./src/build/fd2_bgm_capture_probe 10 20 /tmp/fd2-track10-new.wav
```

该参考用于预先声明的曲目／切换观察，不保证原版混音或逐样本相同，也不等于实时设备通过。2026-11-07 用户选择保留 Nuked原始响度与音色，不新增补偿或音色相似度门；既有未匹配结果保留。Windows已有本地执行证据，Linux命令本轮未执行。

`fd2_bgm_opl_trace_probe` 固定离线执行 Title18，输出真实 Nuked寄存器写序JSONL和metadata，不写WAV、不打开设备。`--frames` 为 `1..sample-rate*120`，`--sample-rate` 默认44100，`--block-frames` 默认512；unhooked目标是无记录对照，不是另一音频后端。

两个离线probe共用 `audio_probe_output.h/.cpp` 的输出门：按canonical实际原资源目录和实际父目录检查，所有输出共同预检，拒绝原目录内路径、已有／重名目标及指向原目录的junction别名，再独占创建；不沿用先rb探测再wb覆盖的写法。Windows使用UTF-8 argv。只有该离线C ABI helper使用C++17，正式音乐runtime仍为C++14。输出须在已存在的独立目录，失败保留现场并返回非零，不覆盖原资源或旧证据。

Windows（PowerShell，以下新输出路径不可已存在）：

```powershell
cmake --build src/build-win --config Debug --target fd2_bgm_opl_trace_probe fd2_bgm_opl_trace_probe_unhooked fd2_bgm_capture_probe -- /nodeReuse:false
& ./src/build-win/Debug/fd2_bgm_opl_trace_probe.exe --original original_game --frames 44100 --trace "$work/title18-new.jsonl" --metadata "$work/title18-new.json" --sample-rate 44100 --block-frames 512
ctest --test-dir src/build-win -C Debug -R '^(bgm_opl_trace|bgm_opl_trace_unhooked|audio_probe_output_paths)$' --output-on-failure
```

Linux（本轮未执行）：

```bash
cmake --build src/build --target fd2_bgm_opl_trace_probe fd2_bgm_opl_trace_probe_unhooked fd2_bgm_capture_probe --parallel
./src/build/fd2_bgm_opl_trace_probe --original original_game --frames 44100 --trace "$work/title18-new.jsonl" --metadata "$work/title18-new.json" --sample-rate 44100 --block-frames 512
ctest --test-dir src/build -R '^(bgm_opl_trace|bgm_opl_trace_unhooked|audio_probe_output_paths)$' --output-on-failure
```

短离线命令只复现输出与记录接口；标题3748写序的比较仍绑定其原预声明窗口／映射，不由任意1秒trace推定。路径测试在owned临时原资源副本中验证UTF-8及已有／原目录／重名／原目录别名拒绝，不向本树 `original_game/` 写入。

#### A7 事件／voice与配置回归

下列无窗口目标读取现有原资源，覆盖 SBPRO2.MDI分配、整数 sequence volume、原32 duration槽、loop／fade、选项与配置恢复。原机器码向量与新交付候选测试分开登记，不能将纯回归写成真实设备或完整声音通过。Windows（PowerShell）：

```powershell
cmake --build src/build-win --config Debug --target fd2_bgm_test fd2_bgm_sbpro2_test fd2_bgm_sequence_volume_test fd2_bgm_native_sequence_test fd2_bgm_options_test fd2_audio_options_test fd2_audio_pcm_test --parallel 4 -- /nodeReuse:false
ctest --test-dir src/build-win -C Debug -R '^(bgm|bgm_sbpro2|bgm_sequence_volume|bgm_native_sequence|bgm_options|audio_options|audio_pcm)$' --output-on-failure
```

Linux 使用独立构建目录；本轮未执行：

```bash
cmake --build src/build --target fd2_bgm_test fd2_bgm_sbpro2_test fd2_bgm_sequence_volume_test fd2_bgm_native_sequence_test fd2_bgm_options_test fd2_audio_options_test fd2_audio_pcm_test --parallel
ctest --test-dir src/build -R '^(bgm|bgm_sbpro2|bgm_sequence_volume|bgm_native_sequence|bgm_options|audio_options|audio_pcm)$' --output-on-failure
```

`tools/inspect_dro_v2.py` 只读解析已关闭的 DOSBox-X `b6abbd5` writer的未压缩 DRO v2，核对头、转换表、delay、bank／寄存器记录和完整长度；不播放、不调用模拟器、不恢复 loaded MDI pointer，也不判 PCM或A7行为通过。输出必须是原资源目录之外的全新文件，父目录已存在：

```powershell
python tools/inspect_dro_v2.py "$work/closed.dro" --output "$work/dro-structure-new.json"
python -m unittest discover -s tools/tests -p test_dro_v2.py
```

Linux将 `python` 改为 `python3`，参数相同。writer的cache前缀、同值写过滤和尾静音未编码限制须保留，不能将结构有效或全部记录数量当完整端口史。实际标题、opening／END、options／Load和SFX重开使用[阶段记录](../plans/stage-log.md#a7当前受支持音频行为与后端边界)指向的封存 worker `commands.json` 与输入／程序事实；换候选后须重核来源，不盲重放旧fixture或把单端／联合来源称fresh pair。

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

`fd2_sfx_oracle.py` 从 FDOTHER 独立 VOICE 原字节建立候选，读取调用者预声明的查询 JSON；缺 `--wav` 只输出 pending。实际 WAV 须已关闭、至多120秒，格式为 PCM16 mono／stereo、44100／48000 Hz。依赖现有 `numpy`、`scipy`，不新增运行时游戏依赖。输出必须是新文件且位于原资源目录之外：

```powershell
python tools/fd2_sfx_oracle.py --fdother original_game/FDOTHER.DAT --request "$work/sfx-query.json" --wav "$work/closed.wav" --output "$work/sfx-observation.json"
python tools/fd2_core_sfx_gate.py --original original_game --scenario player-magic0 --wav "$work/closed.wav" --output "$work/core-sfx.json"
```

Linux 离线分析使用相同参数并将 `python` 改为 `python3`；这不表示 Linux 采集已执行。查询窗、固定前缀和身份阈值须先声明，不能看波形后裁到好点。核心三场声明由 `fd2_battle_animation_sfx.declare()` 提供；`fd2_core_sfx_gate.py` 预声明固定60 ms前缀，并按完整样本 SHA 合并相同字节候选，只核有限 VOICE 字节身份及录音内有序存在，不证明重复 cue 的全部次数、运行 bank 指针、输入精确时延、整段声音或 A7 保真；未提供录音仍为 pending。B6三核心已有上述最低身份／顺序的实际校准；整段声音和精确时序的A7校准仍未完成，不能把这些分析参数当作完整保真容差。

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
