# 图形自动化与原版对照

本流程在 Windows 桌面串行运行 SDL 和 DOSBox-X，自动输入、捕获、判定并清理窗口。Linux 保留 Xvfb/XTest 的真实输入回归。原版资源只读，DOSBox 挂载测试目录中的完整副本；运行失败、捕获失败、缺少依赖或缺少判据都不能记为通过。

开发验收只要求当前执行平台的适用检查通过。当前平台为 Windows，WGC 图形流程属于必测项；Linux 等其他平台未执行时如实登记，不阻塞当前开发任务，也不记为已通过。跨平台命令仍维护，当前平台的逻辑、输入、图形与原版对照范围不能因此缩减。

2026-11-04 的开发约束已调整：允许自动操作原版菜单、剧情、布阵、移动、攻击、法术和物品。过去因 Linux 桌面条件不足而留下的「待人工对照」改为待原版对照，可由本流程逐项关闭。允许操作不表示这些功能均已实现或已经对照通过。

## 截图规范

当前 Windows 截图统一使用 Windows Graphics Capture（WGC）的 HWND 单窗口接口，SDL 与 DOSBox-X 均执行同一规范。禁止使用全屏截图再裁切，也禁止通过 `GetDC(NULL)`／`BitBlt` 读取桌面作为单窗口证据；遮挡、其他窗口和桌面缩放会使结果不完整。捕获失败时保留错误，不切换截图方式绕过失败。

| 对象 | 捕获方式 | 检查 |
|---|---|---|
| Windows SDL 窗口 | `Windows.Graphics.Capture`，通过 `IGraphicsCaptureItemInterop::CreateForWindow(HWND)` 指定窗口 | 核实 HWND 所属 PID；客户区尺寸与裁切边界有效；有实际帧且不是黑屏；保留原始 BMP |
| Windows DOSBox-X 窗口 | `Windows.Graphics.Capture`，通过 HWND 指定本轮进程的窗口 | 验证客户区是 `320×200` 的整数倍率，不得混用历史截图 |
| Linux SDL | 现有 Xvfb/XTest 输入回归 | 当前不生成原版像素对照证据；不得把日志回归登记为图像一致 |

微软文档规定，Win32 的 [`CreateForWindow`](https://learn.microsoft.com/en-us/windows/win32/api/windows.graphics.capture.interop/nf-windows-graphics-capture-interop-igraphicscaptureiteminterop-createforwindow) 至少需要 Windows 10 1903。捕获实现使用窗口图像，不依赖窗口在桌面上未被遮挡；最小化、零尺寸、超时、设备或 API 不可用时明确失败。操作按键前仍必须确认窗口焦点，不能把按键发给其他应用。

历史试验曾按 [DOSBox-X 2026.10.01 的官方截图绑定](https://github.com/joncampbell123/dosbox-x/blob/dosbox-x-v2026.10.01/src/hardware/hardware.cpp#L2313)尝试原生热键和独立 mapper，但未生成 PNG，未记为通过。随后用户确定 WGC 为截图规范；原生 PNG 不属于当前流程，也不是待完成的验收前置。报告将实际捕获方式记录为 WGC，不能登记为模拟器原生 PNG。

图像只在确认整数倍缩放且客户区比例正确时以 nearest-neighbor 还原到 `320×200`。比较对象是同一资源、存档身份和游戏检查点，不比较任意时间的两张图片。VGA 的六位颜色扩展差异必须单列：只能容忍有依据的每通道量化差，不能通过模糊、扩大容差或隐藏差异像素让错误画面通过。菜单必须证明选择发生变化和回绕恢复，黑屏相等不算通过。

F1 动态标记使用完整帧判据识别相位；等待指定相位时，未命中按 17／43／79／131 ms 轮换错开观察，保留原 15 秒期限。逐次记录观察相位、目标或排除相位、是否命中及额外延时；水色漂移等硬错误立即失败。错开的是自动化观察时点，不调整游戏时钟、不补键、不放宽像素门，也不能仅凭首尾两个合法相位就断言中途一定出现过目标相位。

## Windows 运行

需要可交互、未锁定的 Windows SDR 桌面、CMake、MSVC 与含 C++/WinRT 的 Windows SDK、Python 3、Pillow，以及 DOSBox-X。当前截图使用 BGRA8 SDR，HDR 色彩保真未验证。游戏逻辑和 BGM 后端仍分别按 C11/C++14 构建，截图工具单独使用 C++17。命令从项目根目录运行：

```powershell
python -m pip install Pillow capstone
cmake -S src -B src/build-win -DBUILD_TESTING=ON -DFD2SDL_REQUIRE_GUI_TESTS=ON
cmake --build src/build-win --config Debug --parallel
ctest --test-dir src/build-win -C Debug --output-on-failure
python -m unittest discover -s tools/tests -p 'test_*.py'
python tools/run_graphics_tests.py --game src/Debug/fd2sdl.exe --build-dir src/build-win --config Debug
```

DOSBox-X 不在 `PATH` 时，最后一条追加 `--dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe'`。系统 SDL3 的使用与首次下载见 [README](../../README.md)，其他 shell 差异见 [跨平台命令](platforms.md)。不要把另一个生成器的缓存目录复用为 Windows 构建目录。

`FD2SDL_REQUIRE_GUI_TESTS=ON` 在依赖不足时使配置失败；普通无桌面逻辑构建可关闭它，但结果不代表图形验收。先用 `ctest --test-dir src/build-win -C Debug -N` 核实 `field_play_input`、`field_play_result` 与 `graphics_job` 存在。Debug 能暴露 MSVC 的迭代器／运行库检查，不能只运行 Release 后宣布 Windows 通过。

正式输入与原版对照可单独复跑：

```powershell
& src/build-win/Debug/fd2_field_play_input_driver.exe run-job 1500000 python tools/run_original_opening.py --root . --game src/Debug/fd2sdl.exe --driver src/build-win/Debug/fd2_field_play_input_driver.exe --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --artifacts src/build-win/original-opening
& src/build-win/Debug/fd2_field_play_input_driver.exe run-job 600000 python tools/run_original_focus.py --root . --game src/Debug/fd2sdl.exe --driver src/build-win/Debug/fd2_field_play_input_driver.exe --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --artifacts src/build-win/original-focus
ctest --test-dir src/build-win -C Debug -R '^field_play_input$' --output-on-failure
python tools/run_original_comparison.py --root . --game src/Debug/fd2sdl.exe --driver src/build-win/Debug/fd2_field_play_input_driver.exe --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --save-fixture 'path/to/stage0-FD2.SAV' --artifacts src/build-win/original-comparison
python tools/run_original_battle_menu.py --root . --game src/Debug/fd2sdl.exe --driver src/build-win/Debug/fd2_field_play_input_driver.exe --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --save-fixture 'path/to/stage0-FD2.SAV' --artifacts src/build-win/original-battle-menu
& src/build-win/Debug/fd2_field_play_input_driver.exe run-job 300000 python tools/run_original_end_turn.py --root . --game src/Debug/fd2sdl.exe --driver src/build-win/Debug/fd2_field_play_input_driver.exe --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --artifacts src/build-win/original-end-turn
& src/build-win/Debug/fd2_field_play_input_driver.exe run-job 360000 python tools/run_original_load.py --root . --game src/Debug/fd2sdl.exe --driver src/build-win/Debug/fd2_field_play_input_driver.exe --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --artifacts src/build-win/original-load
& src/build-win/Debug/fd2_field_play_input_driver.exe run-job 300000 python tools/run_original_leave.py --root . --game src/Debug/fd2sdl.exe --driver src/build-win/Debug/fd2_field_play_input_driver.exe --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --artifacts src/build-win/original-leave
& src/build-win/Debug/fd2_field_play_input_driver.exe run-job 600000 python tools/run_original_overview.py --root . --game src/Debug/fd2sdl.exe --driver src/build-win/Debug/fd2_field_play_input_driver.exe --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --artifacts src/build-win/original-overview
& src/build-win/Debug/fd2_field_play_input_driver.exe run-job 360000 python tools/run_original_player.py --root . --game src/Debug/fd2sdl.exe --driver src/build-win/Debug/fd2_field_play_input_driver.exe --dosbox 'C:/Tools/DOSBox-X/dosbox-x.exe' --artifacts src/build-win/original-player
```

CTest 输入回归保存到构建目录的 `test-artifacts/field-play-input/`；总流程为每轮创建 `graphics-artifacts/<时间>/`，其中包括输入 trace、日志、原始截图、差异图、原版资源 SHA-256、子进程退出与清理记录及 `summary.json`。当前驱动的文件接口使用 Windows 当前代码页，测试产物目录先使用 ASCII 路径。总流程自动把 SDL 正式 UI 生成的 `storage-cancel/FD2.SAV` 传给原版配对。单独复跑时将示例的 fixture 路径换成该文件，并使用新的产物目录；不要拿任意高关卡存档要求仅支持 stage 0 的 SDL 呈现相同菜单。原版与转换产物不提交；只提交测试脚本、配置生成逻辑与文字结论。

Windows 总流程先执行自然开场和边缘镜头配对，再执行快速战场入口、菜单和动作测试。自然开场必须从标题 START 经 stage 32／31／0，到首次可操作状态；`--field-play 0` 跳过引导，不能作为自然站位基线。117 个等待点各自通过文字、头像、框边和必要箭头的资源门后才发送一次确认键。首次焦点先截图确认，再向左一格打开正式 Save 测量；原始焦点与测量焦点分别报告。当前不宣称 RNG、完整场景像素、音频或精确时序相等。

自然流程保留同一来源 `FD2.SAV`，但从 START 进入，不加载该来源。来源为其他关卡时，原版可读取活动档，SDL 只可恢复当前支持的 stage 0，因此 Save 测量前的 Load 图标可不同；必须按各端前置完整验证普通或禁用图，不排除该图标。原版次级 Load 只看文件能否打开，不能套用标题 CONTINUE 的 stage `0xff` 门。该 runner 对 stage 0 来源暂只接受已验证的真实 START 完整快照，其余来源明确报工具范围限制；这不代表游戏拒绝其他合法 stage 0 存档。

边缘场景从 START 副本只改 actor0 坐标及 checksum，保留原焦点／镜头，使用 Z 聚焦到左上和右下目标。以干燥静态地块唯一识别相机，再检查选择范围、COMMAND 四个完整图标及角色遮挡，最后逐级取消。合成坐标不冒称自然开场；原版可用命令与 SDL 开放能力分别登记。地图边界和 same-target 行为遵循原版，不要求所有焦点始终居中。总流程约需半小时，外部超时应至少预留 45 分钟，并用 `run-job` 管理本轮进程树。

## Linux 运行

```bash
# 安装发行版对应的 X11/XTest 开发包及 Xvfb 后运行。
cmake -S src -B src/build -DBUILD_TESTING=ON -DFD2SDL_REQUIRE_GUI_TESTS=ON
cmake --build src/build --parallel
ctest --test-dir src/build --output-on-failure
python3 -m unittest discover -s tools/tests -p 'test_*.py'
```

现有 XTest 序列覆盖移动后待机、完整回合、普通攻击与固定 seed 的致死双击。WGC 与 DOSBox-X 配对 runner 当前不能直接在 Linux 执行；当前 Windows 任务不要求补跑 Linux。以后在 Linux 开发时按当时适用的检查验收，保留图形捕获范围和未覆盖项，不能把未执行的平台写为已通过。

## 场景与判据

| 层 | 场景 | 通过依据 |
|---|---|---|
| 基础回归 | 资源、状态机、存档、声音、错误出口 | CTest 与 Python 的实际注册、执行及退出码；Release 也不得禁用测试断言 |
| 原版共同前置：自然开场 | 标题 START → 32／31／0 的 117 个等待点 → 首次浏览 → 显式 UI Save | 逐页资源 ACK；12 名角色身份、阵营、坐标、方向、隐藏／行动标记及阶段／计数／资源／相机／焦点核对；不使用已操作 START.SAV 的坐标推导自然终态 |
| 原版共同前置：边缘镜头 | 坐标派生副本 → Z → selection → COMMAND → 两级取消 | 左上／右下合计覆盖四向卷动，唯一地块识别镜头、完整范围格与四图标；内容／mtime 不变；地图边界和同目标例外另由坐标核心回归覆盖 |
| SDL 正式输入 | 移动后待机、acted gate、完整 phase、普通攻击和致死双击 | 真实宿主键盘到 SDL 循环；固定 seed 仅用于复现；生产日志断言与非空窗口帧 |
| SDL 系统菜单 | 移动取消、存档确认取消、Save → 改变状态／配置 → Load → 再次 Save → 继续行动 | 取消不提交行动／文件；校验 checksum、stage、turn 和配置；重写后快照与加载源一致，恢复后可待机 |
| SDL 首次保存 | 无活动文件时从正式菜单 Save | 肯定前没有文件，肯定后 checksum、metadata、active／roster 独立数量有效，手工槽为空；不能只测已有文件覆盖 |
| SDL END | Esc、NO、上下不切换、默认 YES 提交 | 取消后 WGC 稳定区域恢复浏览；只提交一次，side 1 → side 0 → turn 2 玩家阶段顺序正确，不自动再结束下一回合 |
| SDL 应用分派 | 手工 LOAD 拒绝、CONTINUE、leave、合成 victory／defeat | 未实现的手工入口仍留标题；肯定 leave 自然退出 `0`；胜／败各自说明后续未实现并退出 `2`，不改存档。合成隐藏位夹具只证明 SDL 分派 |
| SDL 窗口生命周期 | 地图、单位预览、对话、新游戏交接期间关闭窗口 | 正式窗口产生有效帧，关闭被处理，进程正常退出 |
| 原版配对 | 标题默认选择、上下移动、回绕、Esc 无副作用 | 原版与 SDL 的 WGC 单窗口帧在同一检查点比较；菜单变化与恢复均有断言；另测宿主遮挡不影响捕获 |
| 原版 B1 菜单配对 | CONTINUE → 空焦点菜单 → END 默认 YES／Right 选 NO → Esc／NO 回浏览 → 重新开菜单 | 原版资源像素识别各检查点；YES/NO 必须采到对应选中高亮，取消后光标与静态面板区域恢复初始浏览，两份存档不变。菜单中March普通／高亮图也必须通过，肯定批次由独立场景验收 |
| 原版 B1.3a | 真实 START → 肯定 END 一次 → round 002 → actor 0 最终焦点 → 左格正式 Save | 原版回合 banner、角色与光标组合资源、有效存档及独立数量；两端确定性字段相等。源码日志仅补充 SDL phase 顺序 |
| 原版 B1.3b Load | round2 不保存 → Load 的 Esc／NO → 肯定 Load 来源 A → 显式 UI Save 测量 | Load 前后来源副本内容和 mtime 不变；测量存档恢复 turn1 与全部已确认字段，仅排除 active cache `+2` 和记录的测量焦点导航。取消只证明浏览恢复、文件不写及菜单可再次使用 |
| 原版 B1.3b leave | Esc／NO 回浏览 → 肯定一次后不再发键 | 自然退出 `0` 必须发生在清理前；未退出时先保存 WGC 并失败，后续清理退出不能翻转为通过 |
| 原版 B1.4b 总览 | 真实 START → 次级默认项 0 → 四种键分别关闭 → 显式 Save 测量 → 页内宿主关闭 | 原版资源独立合成四区，含数字与597／598条件字形，共25,289像素；SDL再与原版对应区域比较。关闭回同焦点，查看期间内容与mtime不变，测量容器只排除已声明cache／导航字段 |
| 原版 B1.4c1b 玩家取消 | 源快照定位actor0 → 一格合法移动 → COMMAND取消 → 恢复选择态 → Enter直接COMMAND → 取消回browse | 真实地图／profile／占用／ZOC选前置，范围LUT与资源证明状态；逐键ACK，bytes／mtime全程不变。COMMAND完整24×20图标及actor遮挡组合；另以满HP WAIT校验已行动映色／详情入口，未测全部runtime／RNG |
| 原版 B1.4c2b 行军 | 固定START焦点，取消／NO／一次肯定 → 四玩家终点与整轮 → 正式Save | 双端真实输入及资源门；完整容器仅排除声明字段，未知区保留；不比较未对齐的AI／伤害／随机流 |
| 原版 B1.4d F1 | 四场景开合、同标记相位、页内关闭窗口 | 每端完整64,000像素资源门、冻结合法水色；双端原始RGB水色周期未对齐，不宣称实际RGB相等 |
| 原版 B1.3c 结果门 | START仅改hidden位派生，原版选择取消到胜利首对白／败北两帧；SDL独立结果exit2 | 完整manifest、逐键ACK、GAME OVER无键等待、自然退出及文件守卫；不证明整关实战或B5后续 |
| 后续原版配对 | 完整phase／AI／随机流、胜败后续、法术、物品、战斗演出、声音 | 按下节逐项补充，不因当前控制场景通过而视为全部完成 |

`field_play_result` 用测试目标私有替身触发正式循环的自动动作失败、剧情失败、剧情宿主退出与移动失败；不向生产代码加测试钩子。此测试不是 DOSBox 行为对照。

`run_original_battle_menu.py` 要求 stage 0、焦点为空且菜单能完整容纳的有效活动快照。按本次快照的镜头／焦点计算屏幕位置，确认框使用 FDOTHER[2] 的 frame `48/49/51/52`，调色板取 FDOTHER[0]，沿用每通道差 `≤3`、不合格像素为零的门。取消时必须同时检查光标、菜单／按钮消失和 `(16,184,32,196)` 恢复，不能在 FDTXT[412] 自动提示期间提前发送下一个 Enter。该夹具的原版 NO 按钮下方有 actor／地形动画，不能用该区域作静态恢复判据。场景切换的短暂黑帧只在原有超时内重试同一 WGC 接口，记录错误与最后有效帧；其他捕获错误立即失败。

该 runner 不提交肯定 END，也不比较地图底图、完整对话头像／文字布局或伤害；原版确认框的眨眼消耗共享 RNG，不能把 SDL 固定 seed 当作原版相同初态。资源像素与菜单路径通过不表示活动快照的全部原版运行期缓存已兼容。

## 新增原版流程

1. 按 [IDA 流程](ida.md)复用已有数据库只读查询。先读 Domain API skill；写脚本时同时读 scripting skill。记录 canonical VA、调用者、分支和格式；伪 C 不能单独证明 ABI。
2. 为场景建立可复跑输入：确定原版资源与初始存档 SHA-256、游戏内状态、菜单选择和结束检查点。启动地址与运行时 selector/base 必须本次取得，不沿用历史绝对地址。
3. 先执行有界导航并识别画面／状态，再输入下一动作。等待必须有超时，超时保存最后证据并失败；不靠长串盲按 Enter 绕过剧情。
4. 非随机操作比较菜单、光标、单位位置、配置及存档字段。随机动作记录原版 RNG 初态和调用次数，再比较相同流的结果；不得用 stage 0 的历史 seed 冒充原版通用默认值。
5. 动画比较稳定检查点或明确帧序；声音使用模拟器原生音频捕获，分别比较样本、时长、触发顺序与响度。不用一张静止图证明时序或声音一致。
6. 保留成功、取消、非法目标、失败与退出分支。新确认的函数语义同时更新实现注释和命名输入，由生成器发布；不手改权威生成文件。
7. 每项分别登记自动测试、局部机器码对照和原版对照范围。只有建立独立 oracle 且执行通过的检查点才能关闭验收项；未覆盖功能保持未验收。

## 窗口清理

测试窗口串行运行。每个进程启动后立即记录 PID；成功、断言失败、捕获失败和超时都执行同一 `finally` 清理：正常关闭 → 等待退出 → 必要时 terminate → 等待 → 必要时 kill。正常用例还须断言正常退出码，强制关闭只能算清理成功，不能算游戏正常退出通过。

只清理本轮启动的进程。禁止按进程名批量关闭所有 DOSBox、SDL 或用户窗口。先清理前一用例再启动下一用例；报告必须能核实没有遗留测试进程，避免多个窗口累积耗尽系统资源。捕获、输入和整个用例都应有有界等待，运行器不能无限挂在锁屏或失焦窗口上。

Windows 的外层命令使用驱动 `run-job TIMEOUT_MS COMMAND...`：挂入带 `KILL_ON_JOB_CLOSE` 的原生 Job 后才恢复 worker，Job handle 不向子进程继承。超时先向本 Job 的窗口发送关闭并等待，必要时终止 Job 并等待；worker 正常退出却遗留子孙也判失败。不能仅用 `subprocess.run(timeout=...)` 杀掉 Python，让其 `finally` 来不及清理游戏窗口。正常退出后的短暂 Job 计数延迟须等待核实，不能误报遗留。

`graphics_job` 无窗口测试实际运行子孙进程，覆盖正常参数／退出码、超时整树和父进程退出后遗留子孙。超时返回 `124`、遗留子孙返回 `125`、guard 错误返回 `126`；需要强制终止的负向用例只验证清理能力，不计为游戏正常退出通过。所有正向图形场景仍要求正常关闭、没有强制终止和遗留。

## 本轮验证记录

2026-11-04 在 Windows 11 `10.0.26300`、MSVC `19.51`、Debug 构建上执行：

| 项目 | 结果与范围 |
|---|---|
| CTest | 29 项非窗口测试和 1 项正式窗口输入测试通过；输入项内含 wait／attack／lethal 三场景，不是未注册后的空跑 |
| Python | 60 项通过，无跳过；含原有 39 项和新图像比较／进程清理回归 |
| 完整图形流程 | 7 组全部通过：输入回归、地图、单位预览、对话关闭、新游戏交接关闭、取消／存取档、原版标题配对 |
| 原版配对 | 使用真实 SDL UI 生成的 stage 0 快照；原版和 SDL 的 DOWN／UP／UP 回绕／DOWN／Esc 五检查点均零差异像素、最大通道差 0；默认画面也匹配 |
| 遮挡验证 | 用本轮自建的可见 `240×180` 白窗覆盖 SDL，WGC 窗口图像仍零差异；遮挡窗口在 `finally` 销毁 |
| 隔离与清理 | 原版目录所有文件哈希不变；所有测试游戏进程正常退出并清理，无强制终止、无遗留窗口 |
| 源码／发布完整性 | 290 个源码函数地址 token 均为 canonical；IDA 发布清单校验通过，未重新发布或改变生成证据语义 |

完整流程的本地证据目录为 `src/build-win/graphics-artifacts/20261104-042158-435413/`，总报告为 `summary.json`，配对报告为 `original/comparison.json`。这些文件只在本地，不提交。单独原版配对的复跑证据为 `original-title-wgc-02/`。

本轮修复了三类迁移／输入问题：BGM 的 `std::search` 迭代器跨两个独立字符串字面量；存档测试硬编码 `/tmp`；移动范围选中后误用浏览输入上下文，导致真实 Esc／Delete 无法取消。另用 `.gitattributes` 保持发布证据的 LF 原始字节，避免 Windows 检出改变 SHA-256。

范围限制：原版配对仅覆盖上述标题检查点，未证明完整第一关、胜败、后续关卡、FIGANI、声音或时序一致。本地旧存档的活动 stage 为 29，原版显示三项标题而 SDL 当前仅支持 stage 0，直接使用该旧存档的差异已保留为失败证据，不靠扩大像素容差消除。Linux 本轮未重跑，不影响当前 Windows 验收；HDR 不在已验收的 SDR 范围内。DOSBox 原生 PNG 是未通过的历史试验，不是当前 WGC 流程的验收缺项；严格运行时 BSS 仍待新的独立转储。

历史 Linux 的 CTest／sanitizer 与局部机器码证据仍见 [9.5 复核报告](../reverse-engineering/ida-bytecheck-9.5.md)。文档命令迁移、LF 归一和 SHA-256 校验不构成新的逆向语义或全游戏一致性证据。

### B0 后续验证：2026-11-04

当前 Windows Debug 构建完成 B0 后，30 项非窗口 CTest 与 1 项正式输入 CTest 全部通过，Python 仍为 60 项通过、无跳过。新增 `field_game_failure` 内含 12 个场景：玩家待机／移动／攻击的未知事件、非法格数据、双击和反击的提交后失败、notice 的第 64 项与溢出、turn handler 前容量检查、未知或失败事件停止后续事件和状态计时、状态重算失败及 AI 提交后的禁止重试。AI、正式循环和存档测试另补分段施法／物品、错误优先于结果门、演出只确认一次，以及停止状态拒绝保存／有效加载恢复。

正式输入首次复跑发现 50 ms 按键在当前桌面有漏采现象，按键序列进入了错误菜单，最终断言失败并清理进程。Windows 驱动改为保持按键 150 ms，覆盖 Debug／WGC 后的短帧停顿，仍低于 SDL 的 500 ms repeat 门；这只是宿主测试参数，不登记为原版输入常量。wait／attack／lethal 三场景随后全部通过。

完整 7 组 WGC 图形流程在 `src/build-win/graphics-artifacts/20261104-111402-532745/` 再次通过，含存取档、原版标题五检查点与遮挡验证；配对仍为零差异像素。全部游戏进程正常退出并清理，原版文件哈希不变。`--field-play-once 0` 的资源、事件和效果快速检查也通过；此项只作补充，不代替正式输入。源码地址检查为 293 个 canonical token，未新增局部机器码执行或原版战场行为验收结论。

### B1 后续验证：2026-11-04

B1 结果分派、END 后端、能力门、存取档后继续行动及原版确认框已接入。Windows Debug 的 31 项非窗口 CTest、84 项工具 Python 通过；`field_play_input_harness` 内另含 4 个无窗口检查。新增回归覆盖未行动恢复的 flags／状态／HP 门、无副作用拒绝、phase 事件失败停止、独立胜／败返回、宿主退出优先级，以及 END 必需提示资源缺失时不推进 phase。

原版对照发现并修复三项业务错误：标题 CONTINUE／LOAD 映射；确认框默认与左右方向、可见按钮及关闭层级；活动快照沿用旧关卡 metadata 导致原版选错 FDSHAP。存档回归新增 metadata 已用前缀、未知尾和 profile 保留、shape 错误原子拒绝、80 模板边界，以及真实 FDFIELD[1] 前缀一致性。

正式输入复跑还发现首个 Z 未采到时，旧盲按序列会误开系统菜单并执行 END。Windows harness 现等待 `field input ready`，逐次发送 Z 并核对 `field focus`，确认起始 actor 后才执行余下序列；ACK 缺失或焦点错误立即失败，不补发 Enter。Linux 的 `wait/attack` 驱动接口不变。此修复不改变生产输入的逐呈现帧语义，也不改为 FIFO。

单独原版菜单对照已在 `src/build-win/graphics-artifacts/20261104-122030-825194/` 通过：两份进程的 8 个菜单／按钮像素检查点均零差异，Esc 与 NO 取消均恢复浏览并能再次开菜单；隔离存档与原版目录不变，进程正常退出并清理。该轮使用修复后真实 SDL Save 生成的 stage 0 快照，不使用旧的混合关卡头夹具。

最终完整流程在 `src/build-win/graphics-artifacts/20261104-122317-187897/` **12 组全部通过**：正式输入、4 个窗口生命周期场景、存取档、END、3 个应用结果路由、原版标题和原版 END 菜单。`summary.json` 与两个 `comparison.json` 均通过；标题五个配对检查点及战场菜单／按钮八个配对检查点均为零差异像素、最大通道差 0。31 项非窗口 CTest 加 1 项正式输入共 **32 项 CTest 通过**，84 项工具 Python 无跳过。13 个顶层记录进程均无强制终止且已清理，输入子场景也逐一等待正常退出；胜／败路由按预期退出 `2`。原版目录哈希、源夹具和两份取消测试存档不变；记录的游戏二进制哈希与当前构建一致。源码地址检查为 309 个 canonical token，IDA 发布自洽检查通过。

原版菜单 runner 的两次失败现场仍保留：一次是切场景短暂黑帧，另一次是把含原版动画的 NO 下方区域误作静态恢复判据。分别改为原 deadline 内重试同一 WGC，以及检查静态面板 ROI；未放宽像素容差、未切换截图接口。只读检查同时排除了「旧 FD2.TMP 直接导致底图错误」：Continue 会从 FDICON 重建该缓存，实际错误来自保存的 metadata 首字节。

本轮复用已发布伪 C、局部静态指令及原版资源取证，更新命名输入并重新生成登记／清单；未新增局部机器码执行、全量 IDA 字节复核或数据库基线发布。原版 END 肯定提交后的整回合 phase／AI／随机结果、完整第一关胜败、结算、地图全帧、FIGANI、声音与时序仍待对应原版证据；Linux 本轮未执行。

### 分阶段复核与存档纠正：2026-11-04

Windows 基础、B0、B1.1、B1.2a、B1.2b 已分别从独立暂存候选构建并提交，哈希及各轮测试数量见[阶段记录](../plans/stage-log.md)。B1.2b 候选为 33/33 CTest、39/39 当时已提交工具 Python，无跳过；额外工具测试属于下一批自动化脚本，不能混入该提交的测试数量。

真实 `START.SAV` 的活动数量／持久队伍数量是 12／4，本地后期档为 27／31，纠正此前 count-copy 假设。活动与手工 Save 现在写完整 session-owned `0xa00` raw roster，含从源档恢复的未用尾；metadata 未用尾仍来自目标容器。首次保存、坏档保护、跨文件保留和招募回滚已补回归。手工 Load 在真实 stage rebuild 完成前明确拒绝，不能把持久表当活动 actor 恢复。

`src/build-win/graphics-artifacts/20261104-133153-138281/` 的 **13 组 WGC 全部通过**，比上一轮增加无文件时首次 Save。原版标题五个配对点、战场菜单／按钮八个配对点均通过，遮挡检查通过；原版目录哈希不变，顶层进程记录均正常关闭且无强制清理，外层 Job 记录 `residual=0 forced=0 cleaned=1`。这是 SDL 正式输入、所列原版标题与取消菜单的验收，仍不覆盖原版肯定 END、Load 或 leave 的去向。

本批审查统一了标题／战场输入 trace，发送前就记录 PID、按键和时间，输入或截图失败也保留尝试；移除重复的捕获子类。总 runner 在创建产物前拒绝 `original_game/` 下的输出路径。错误、超时与遗留进程均判失败，不以最终清理成功代替正常场景通过。

### B1.3a 正向 END 与 Save：2026-11-04

`src/build-win/graphics-artifacts/original-end-20261104-06/` 在同一只读真实 `START.SAV` 副本上完成原版与 SDL 的一次肯定 END、第二回合、原始焦点及正式 Save。两端均为 stage 0、turn 2、active 12／roster 4；资源、配置、cell state、actor 身份、acted 全清、未知容器区和 roster 未用尾满足所列断言。存档 checksum 有效，移动到 leader 左格后的焦点同为 `(6,14)`；源档及原版目录不变，两个进程均正常退出，Job 无强制清理或遗留。

正向运行发现 SDL 新回合没有像原版 `field_turn_cycle_run @0x1a30b` 那样重置 Z 起点并聚焦 actor 0，镜头和光标留在自动行动末态。现按原版末尾顺序在 phase 2 事件／状态成功后沿 X、再 Y 移动焦点，复用既有卷屏阈值；phase 2 失败时不继续聚焦。边界、镜头、Z 起点与失败路径已有逻辑回归。

真实 START 初始光标底部会被下格 actor 覆盖，仅初始到达检查已确认可见的上 21 行（144 个不透明像素），随后每步导航到完整可见空格。新回合检查组合 FDOTHER[1] 光标与 FDICON actor 0 的 12 帧（角色上移 6 px），要求整网格只有一处匹配，拒绝聚焦动画途中的空格。SDL 总览的透明禁用帧单独排除，另验证 Up 不能选中，其他菜单资源仍严格校验。Save 自动提示关闭以固定对话边沿和完整浏览光标判断，不把会变化的地形缩略图当静态 ROI。

第 1～5 轮失败现场继续保留：包括初始遮挡、SDL 焦点缺失、动画中间态、透明能力帧及不适用的缩略图 ROI。均修复具体实现或判据后复跑，未扩大像素容差、改截图接口或补发盲键。`run_original_end_turn.py` 已纳入总流程；单独运行默认使用真实 START，不以 SDL 自产档作为唯一格式 oracle。

通过范围是上述确定性回合／保存合同。原版与 SDL 相机分别为 `(0,13)`／`(4,11)`，如实记录而不判相等；RNG 未对齐，伤害、命中、AI 目标／坐标、roster 有效记录内容和完整画面／声音仍排除。未新增局部机器码执行或 IDA 数据库发布；Load、leave 和 B1.4 页面另行验收。

### B1.3b 活动 Load：2026-11-04

`src/build-win/graphics-artifacts/original-load-20261104-02/` 完成两端 START→END→round2（不保存）→Load 的 Esc／NO→肯定 Load→正式 Save 测量。启动前记录隔离来源 A 的 `st_mtime_ns`，在 END、取消、Load 及测量 Save 肯定前同时检查内容与时间戳，全部保持原值；只有显式测量 Save 允许写盘。第 1 轮已通过内容比较，第 2 轮补齐同内容重写的检测，不把 hash 相等冒充完全未写盘。

测量文件恢复到 stage 0、turn 1、active 12／roster 4。逐字比较整个明文容器，只允许原版重新分配的 active `+2` cache class、明确记录的空格导航焦点四字节，以及重新计算的 checksum 不同；完整 raw roster、有效 actor 的 HP/MP／装备／法术／flags、cell、配置、资源、镜头、metadata 与其他未知字节均匹配来源。Load 本身与随后测量 Save 分别取证，不能混记为自动写档。

两端 Esc／NO 只验收浏览焦点恢复、菜单可再次打开及磁盘无写；未单独测量取消后的全部 runtime／RNG。完整画面、声音、随机流和坏档在原版中的安全恢复不在此项范围。两个进程均正常关闭，Job 无强制清理或遗留，源档及原版目录不变。该 runner 已纳入总流程；leave 和 B1.4 仍独立推进。

### B1.3b 离场：2026-11-04

`src/build-win/graphics-artifacts/original-leave-20261104-05/` 完成两端 Esc、NO 与一次肯定离场。原版和 SDL 均在清理前自然退出 `0`，肯定后没有额外按键；取消恢复完整浏览光标、对话框消失，菜单可以再次打开。源档和原版目录不变，所有本轮进程正常结束，Job 无强制终止或遗留。

第 1 轮确认原版退出游戏，SDL 错误返回标题；现删除该无真实业务来源的返回标题路由，使用独立 `LEAVE_GAME`，保留与宿主关闭的来源区别。肯定前置呈现已有 FDTXT[416]，缺文本／字体／fragment 返回错误，提示期间宿主关闭仍返回 `HOST_QUIT`，不推进战场玩法。最小呈现替身覆盖提示一次与这些失败路径；未新增生产测试钩子。

第 2～4 轮保留了驱动误报现场：SDL 正常关窗后，驱动在发送结束后的节奏等待末尾再次检查焦点，误判投递失败。Windows 驱动现于按下／松开配对完成时检查本轮进程句柄和焦点；后续等待期间不再发键，允许游戏完成提示后关窗。调用者仍独立检查正常退出码和业务检查点，目标仍在运行且失焦则失败。清理带来的退出不能计为自然退出。

该项只关闭所列离场输入和应用去向合同。当前共用提示 helper 为 250 ms，原版该处为 200 ms；完整对话布局、精确时序和声音仍待 V1／A5。此前历史轮次中的「leave 回标题」为旧 SDL 行为，以上述原版纠正与本轮通过范围为准。

### B1 控制流程收口与字体前置：2026-11-04

`c649883` 对应的完整流程在 `src/build-win/graphics-artifacts/20261104-144934-027836/` **16 组全部通过**：原 13 组，加真实 START 的 END、Load 和离场。五份原版 `comparison.json` 均通过；SDL 真实输入、首次保存、存取档、END、独立主程序出口及窗口关闭继续通过。原版目录哈希不变，所有正向进程正常清理，外层 Job 为 `residual=0 forced=0 cleaned=1`。这不是 B1 大阶段终审，剩余页面、批量动作、手工重建及 V1 视听范围仍分别保留。

B1.4a 按 `font_glyph_blit_16x16 @0x4ed7a` 的背景／阴影指令修正字体基础，新增纯内存 `font` 回归；同时把 FDTXT[412] 瞬态识别模板改为下左／正下阴影，并将模板原点移至 `(98,146)`。受影响原版菜单配对在 `src/build-win/graphics-artifacts/font-battle-menu-20261104-01/` 通过，所有进程正常清理，源档与原版目录不变。该菜单检查仍不覆盖完整文字／头像，字形静态 ROI 的原版像素验收随 B1.4b 总览接入；未新增机器码执行或 IDA 数据库发布。

### B1.4b 战况总览：2026-11-04

`src/build-win/graphics-artifacts/original-overview-20261104-01/` 完成原版先行、SDL 后行的真实 START 配对。独立资源 oracle 合成 FDOTHER[5] 的133..136四个完整矩形，逐 bit 合成597／598条件文字及其下左／正下阴影；每个检查点共25,289像素，沿用每通道最多3的调色板展开容差，错误像素必须为0。页面显示01／001、7／4／0和00000000，SDL还逐次与相应原版页面比较。这同时补齐 B1.4a 在这两段条件文字中的原版像素证据，不扩大为所有字形／页面均已验收。

每端用 Enter、Esc、Right、A 四次单键关闭，检查完整浏览光标、同一焦点及无残留对话；关闭方向键未额外移动战场焦点。查看前后内容与启动前mtime均保持不变，最后单独从正式菜单明确Left选择Save作全容器测量；回合、资源／配置、raw roster、active已确认字段、cell及其他未知字节与来源一致，只允许已声明cache class、导航焦点和checksum差异。再打开页面作宿主关闭，两端自然exit0、正常清理，外层Job residual=0、forced=0，源档与原版目录未变。

实现复用同步modal与两张像素缓冲，不添加持久interaction或另一套输入上下文；稳定页复用缓存内容，背景只绘地形、actor、遮挡与边框。开合恢复最初背景，当前帧键全部消耗但保留物理held／repeat状态。原版 `@0x4e381` 的15字节实际清BIOS队列，登记纠正原vsync_wait；资源phase8与人数phase5的signed负高度由局部原始指令确认，保持0行复制。无窗口回归另验证裁切、分配失败、quit优先、同帧输入隔离及owner／RNG不变。

完整流程在 `src/build-win/graphics-artifacts/20261104-154221-037970/` 17组全部通过，六份原版配对报告通过；所有本轮正向进程正常清理，Job residual=0、forced=0。END／Load／leave 的次级菜单检查已同步为总览默认0，Save必须显式Left，不再保留旧的SDL禁用总览实跑分支。

精确开合时长、瞬时动画画面、整幅地图／idle及声音未作原版一致性验收；Linux未执行。测试替身最初把恢复帧0误当作quit注入点，已修正夹具；未为此改变生产modal或扩大像素容差。B1整体仍部分完成，下一项为批量行军的路径事件契约和正式输入。

### B1.4c1a 路径事件修正后的控制回归：2026-11-04

独立暂存候选在完整重建全部TU后通过35项CTest和135项Python，无跳过；新session字段未借旧结构布局作正式验收。受影响的原版／SDL START→END→round2→显式Save在 `src/build-win/graphics-artifacts/ai-pending-end-20261104-01/` 复跑通过，使用的是独立候选程序，其路径与SHA-256由comparison.json记录。确定性字段相等，源档／原版目录不变，两个本轮进程正常清理，Job residual=0、forced=0。

本次修正沿途match0收集、末格match1覆盖及outer延后分派；新增SDL回归检查最后命中、无路径、RNG提交后未知handler冻结、path owner恢复与Load清理。局部原始指令支持lookup／tail结论，当前没有已实现的cell handler；上述END场景不证明特定沿途事件的原版运行等价。玩家动作顺序、批量行军和具体handler仍分别验收，完整17组记录仍对应此前400891a总览阶段。

### B1.4c1b 玩家controller与移动取消：2026-11-04

独立暂存候选完整重建后36项CTest、147项工具Python通过，无跳过。`src/build-win/graphics-artifacts/original-player-20261104-01/` 完成原版先行、SDL后行：由来源数据定位actor0，选一格合法无事件移动；COMMAND取消恢复原位置／焦点／选择态，再Enter直接进入COMMAND，最终selection取消回browse。所有检查点和清理后副本bytes／启动mtime不变，源fixture及原版目录未变，两个进程正常清理，Job residual=0、forced=0。

原版-only第1轮研究probe在one-step-command超时，未追加按键，现场保留于`src/build-win/research/player-move-cancel-original-20261104-01/`。原因是原版@0x179d5画图标后以@0x127e0重画focus actor，遮住上icon末6行。已用原始资源离线证明完整图标＋actor组合可匹配；第2轮研究probe和正式双端runner的控制presence仅检查上方14行，其余3项20行，沿用delta≤3、bad=0，并明确`full_icon_visual_equivalence=false`。它不证明SDL完整遮挡绘制正确；该已发现差异及acted映色将在下一独立视觉阶段修复。

受影响END／round2／Save在`src/build-win/graphics-artifacts/player-controller-end-20261104-01/` 用新候选程序复跑通过，确定性字段相等，进程正常清理。新runner已注册为总流程第18组，完整17组通过记录仍对应400891a；本阶段没有冒称完整18组已跑。特定pending handler、受伤WAIT、B4经验、完整runtime／RNG与跨地图镜头仍未做原版配对。当前未知handler和特殊cell-action明确失败，等待阶段不保存，成功Load清续行、失败保留。

### B1.4c1 角色绘制修正：2026-11-04

COMMAND 现按 `@0x179d5 → @0x127e0` 在图标后只重画当前 actor，再裁视区边框；不重画整队或地形遮挡层。已行动角色按 `@0x4e1a6` 将实际写入颜色映为 `(color & 7) + 24`，透明位置保留底图。`audit_fdicon_opaque_zero.py` 逐帧解析真实 FDICON 的 1,680 帧，确认 opaque color 0 写入为零，当前缓存的零可代表 skip；损坏或截断流不能作通过依据。

独立暂存候选 37/37 CTest、160/160 Python 通过，无跳过；后者包含本轮标题过渡捕获回归。`player-overlay-20261104-01/` 与 `player-wait-20261104-01/` 位于本地 `src/build-win/graphics-artifacts/`，两组均原版先行、SDL 后行。前者检查完整 24×20 图标及 actor 遮挡，分别识别图标资源状态；后者要求来源 HP 已满，验证原地 WAIT、acted 映色、返回浏览和 Enter 详情入口。所有检查点及清理后 bytes／mtime 不变，原版与来源未变，窗口正常清理，外层 Job 无遗留或强制终止。受伤恢复、raw26 抖动、全详情页及完整 runtime／RNG 不在此次配对范围。

完整回归第 1 轮在 `20261104-174310-530439/` 的 SDL 手工 LOAD 返回标题时遇到空白 WGC 帧，已失败并正常清理，原版目录未变。修正仅在这个已知页面切换点允许原 deadline 内重试同一 WGC，保留错误帧及 `capture-errors.jsonl`，不追加输入；默认空白错误和其他捕获错误仍立即失败。不能用启动跳过动画的 Esc 掩盖返回标题的检查。

第 2 轮 `src/build-win/graphics-artifacts/20261104-174657-913248/` 的完整 19 组及八份原版配对全部通过，含上述两组玩家场景。所有窗口正常关闭，外层 Job 为 `worker_exit=0 timed_out=0 residual=0 forced=0 cleaned=1`，原版目录哈希不变。此记录不覆盖尚未开放的行军、F1、四槽重建、整关结算或声音；Linux 本轮未执行。

### B1.4c2a 共享移动修正：2026-11-04

最终暂存候选重新构建并通过 38/38 CTest、169/169 Python，无跳过。修正移动成本表的 16 字节取址偏差、目标探测与完整长路径扫描、对称排序和起点控制区门；新增原始 START 四玩家共享几何及双方分组合成回归，公开路径接口拒绝越界起点和超界工作缓冲。删除第二条堆路径缓冲及逐步重复计算范围。依据为既有权威伪 C、getter fixup、严格 LE 页映射与局部原始指令，不声明整函数机器执行或新的运行时 BSS 验收。

受影响原版／SDL WGC 分别在 `src/build-win/graphics-artifacts/c2a-end-20261104-02/`、`c2a-load-20261104-02/`、`c2a-player-20261104-03/` 与 `c2a-wait-20261104-03/` 通过，维持各 runner 的原有状态、严格资源及文件守卫判据。原版目录和来源夹具未变，bytes／mtime 守卫通过；双方均正常退出，Job 无超时、遗留或强制终止。完整 19 组记录仍对应 `6b460e4`，本轮只重跑这四组及 CTest 中的正式输入。

`c2a-player-20261104-02/` 在原版 actor0 导航第二步失败：WGC 取得 `3440×1377` 客户区、游戏居中且带黑边，尺寸校验拒绝，未追加按键或裁切成整数倍率。失败画面、输入和清理记录均保留；SDL 当时未启动，来源及原版目录未变。新窗口的第 3 轮玩家场景及尚未执行的 WAIT 均通过。此项不证明尺寸变化的宿主原因已修复；特定近敌 ZOC 的原版运行、完整 runtime／RNG 和批量行军仍分别待验。

### B1.4c2b 批量行军：2026-11-04

独立暂存候选通过 39/39 CTest、189/189 Python，无跳过。March 覆盖固定 anchor、raw 顺序及精确 `0x85` 门、无 HP 门、无路尾部、只 match0、handler 在 acted 前分派、部分提交失败及整轮只推进一次。呈现只由私有模式和 play 局部续行参数区分，普通 END 的画面、伤害及 RNG 回归保持原契约。必需 418 提示失败或提示后宿主退出均不开始批次。代码保留原 call 的名义 200 ms 参数，不声称实际 DOS／宿主等待时间相等。

因 E: 空间耗尽，本阶段完整重建和后续图形产物改在 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b1-20261104/`。按用户指示，不带 Force 的循环仅移除旧候选中的可再生编译文件并清空目录，释放约 4.23 GiB；源码、日志、截图、原版和研究补丁均保留。早期 E: 构建失败不能当通过，以下结果来自 C: 完整候选。

`march-graphics-01/` 的新一轮原版先行、SDL 后行通过：取消和 NO 不改存档 bytes／mtime；只肯定一次；WGC 识别 round2、实际 leader、无滚屏导航和真正空格 parent，再由 UI Save 测量。本次四名玩家终点一致，metadata、完整 `0xa00` roster、未用 actor 尾、cell state、手工槽与其余未声明区域逐字保留。cache、伤害、AI 位置和未对齐随机流另列排除，不把 round2 或未崩溃单独当通过。两端显式 host close 均在清理前正常退出 0，原版及来源未变，Job 无遗留或强制终止。

`march/build/graphics-artifacts/20261104-184719-653282/` 的完整 20 组、九份原版配对全部通过，最外层 Job 为 `worker_exit=0 timed_out=0 residual=0 forced=0 cleaned=1`。上述路径均相对本段的 C: 产物根。未声明长路径每个动画相位的即时关闭响应、所有延后剧情 clean、特定 cell handler、整关视听或完整 runtime／RNG 一致；Linux 未执行。

### B1.4d0 水色DAC前置：2026-11-04

在同一 C: 产物根的 `water/` 完整导出、重建后，40/40 CTest、193/193 Python 通过，无跳过。首次完整构建暴露独立 `field_visual` 测试未链接新增的 input 依赖，补齐后重新完整构建和重跑全部 CTest；第一次结果未计为通过。严格 LE／fixup 审计验证93字节读集及全部16个滑窗，内存回归验证只有DAC224..239变化、低16位无符号门／回绕、长停留一步、Load／新session不重置和render／present不推进。

`water-overview-01/` 使用新候选程序完成受影响的原版／SDL 总览配对，原静态区、输入、全容器测量和只读守卫继续通过；双方正常退出，Job `residual=0 forced=0 cleaned=1`，原版与来源不变。此项仅证明既有总览场景未回归，不证明完整水面动态RGB已验；后者随F1下一阶段用完整64,000像素分别验证。宿主55ms tick和单次采样为明确近似，剧情／通用对话／独立timed wait等caller尚未全接，Linux未执行。

### B1.4d1 F1全场缩放图：2026-11-04

同一 C: 产物根的 `f1/` 最终候选通过41/41 CTest、214/214 Python，无跳过。旧 `field_play_result` 假设辅助页仍未实现；改为检查空地图替身被资源预检拒绝且不继续取键后，重新构建并重跑全部CTest。独立 `field_play_auxiliary` 检查完整64,000索引、负坐标floor、黑边／零色、全部8个标记相位、过滤与灰标覆盖、7开／6关、普通键耗尽／held保留／quit以及开稳关水色冻结、restore一步。尚未实际运行的精确时长和首次未初始化tick没有登记为通过。

`f1-graphics-01/` 完成原版先→SDL后，四场景为F1/Esc、PageUp/Right、F1/A、actor0焦点覆盖。每端每页严格资源RGB检查64,000像素、保留黑边／零色／水，每页至少两标记相位，SDL取得本次原版相同相位，共8对实际帧；另有页内宿主关闭。磁盘bytes／mtime和来源／原版hash守卫通过，双方在finally清理前正常退出0，Job无遗留、超时或强制终止。

每端从原版16种合法水色中识别并锁定首帧周期；锁定后观察到完整帧属于另一不相交合法周期会立即失败。复审修复了旧谓词只返回None、可能重试跳过漂移的问题；三帧漂移流程回归验证第二帧失败且不读取第三帧。两端各自全帧资源门均通过，但打开前周期不同（本次四页原版13/2/7/15，SDL14/5/12/6），实际RGB有水区差异：每帧分别287／1085／879／879像素不同，max delta4／8／12／12。报告保持 `full_raw_rgb_equivalence=false`、`within_vga_tolerance=false`，没有调色归一或扩大容差；不能把独立合法周期验收改写成两张实际图完全相等。

本段路径相对 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b1-20261104/`。总流程已注册第21组F1，尚未把上一20组记录冒称本阶段完整21组；统一全套将在B1审查修复后执行。原版本轮实际只取上述每页两相位，全部8相位来自SDL纯回归；声音、精确开合／repeat时序、完整runtime/RNG及其他stage均不在通过范围。Linux未执行。

### B1收口审查：Load资源与焦点轮询，2026-11-04

`load-fix/` 全新暂存候选通过41/41 CTest、219/219 Python，无跳过。成功Load清焦点轮询起点，失败完整保留session／RNG／源文件；可见actor要求完整12帧资源，真实bank最后ID139通过，坏ID140/255及race10／acted等非hidden记录不豁免，hidden坏ID原样保留。原版cache builder实际每ID读12帧，SDL拒绝坏资源是安全边界，不把坏记录送入原版。import末尾已用现有纯胜败查询替换约20行重复分支，恢复ongoing／胜／败均有回归。

`load-z-graphics-01/` 新跑原版先→SDL后，两端先以角色专属资源逐键证明Z选raw0／1／2／3／0，再沿原取消／NO／肯定Load流程恢复来源A；此时仅一次Z必须选raw0。由来源推导、逐步ACK回原焦点后进行原有显式Save测量，没有扩大容器allowlist。读档／取消bytes与mtime守卫、原版目录hash、测量字段与未知区均通过；双方正常关闭，Job无遗留或强制终止。所有路径相对前述C:产物根。完整21组仍待其余收口修复完成后统一执行。

### B1总审：必需提示错误边界（2026-11-04）

C盘独立暂存候选 `fd2sdl-b1-20261104/messages/` 通过42/42 CTest、219/219 Python，无跳过。普通字形、换行、结束符及字体大小在首个像素写入前完整检查；正式确认、Esc／NO取消及Save／Load结果提示错误均返回ERROR。真实存储测试只写Temp隔离副本：保存已成功而结果提示损坏时，文件仍可解码且含新值；读档成功后提示损坏也保留已恢复状态。错误报告说明已提交事实，不伪造回滚。成功Load另清空play局部的已报告事件计数，不增加session字段。

该项是SDL损坏资源与提交边界回归，没有把损坏资源运行到原版，也不声明原版具有相同安全性。正常菜单与存取的WGC配对将在最终整套21组中复跑；当前未新增原版运行通过声明。

### B1总审：标题必需资源错误（2026-11-04）

C盘独立暂存候选 `fd2sdl-b1-20261104/title/` 通过43/43 CTest、219/219 Python，无跳过。标题飞字ANI失败、调色板缺失或不足768字节、标题底图解码失败均返回ERROR；hostquit与主动动画skip保持原分派。私有TU只替换输入／资源I/O，所有后续替身成功，覆盖8条路径并检查资源释放；旧实现的4个错误反例不能通过。该回归不启动窗口，不声称已对原版执行损坏归档；正常标题会纳入最终21组WGC复跑。

### B1总审：移除March旧能力豁免（2026-11-04）

早期March未开放时，普通battle-menu oracle允许SDL禁用图并排除其opaque像素；该历史范围不能沿用到当前已开放版本。现删除两个豁免：按已知选择态匹配全部菜单图，并把March资源像素纳入双端比较。新增回归拒绝禁用March和单个March opaque像素错误，旧实现两者均会误报通过。独立暂存候选220/220 Python通过，无跳过；March脚本的源码根查找简化为固定父目录。当前这项没有新增GUI运行，最终全套会以新oracle验收，不修改历史通过记录的覆盖量。

### B1总审：详情资源与首次失焦记录（2026-11-04）

C盘独立候选 `fd2sdl-b1-20261104/details/` 的220项Python通过。CTest首轮42项通过，field_play_input在attack-focused阶段报告 `target window lost focus; target=57188 foreground=44648 title=AkagiMS` 后停止；没有继续发键或放宽前台检查。原始 `ctest.log` 和唯一场景目录保留，仅受影响项在 `ctest-input-02.log` 重跑通过，因此43项均有通过结果，但不将首轮记作无失败整套通过。

F2及敌方／已行动Enter进入详情时，头像晚解码或开合动画分配失败会返回ERROR。空焦点、隐藏记录、HP0等合法无目标仍不打开详情。现有私有TU增加六条错误及两条无目标反例；失败不会继续详情close或controller后置。正常详情资源配对留待最终总跑，未声称原版畸形资源行为等价。

### B1总审：默认片头必需资源错误（2026-11-04）

首次启动默认进入完整片头，ANI3及后续插播4／5／6／7／8／0的负结果、所请求调色板和必需图像错误都必须停止。helper现在首错返回，4／6失败不再接着播放5／7；外层释放滚动缓冲，保留hostquit专用分派。复用已验证的归档palette指针，删除两处重复读取，不改变主动skip和可选音频规则。

独立候选 `fd2sdl-b1-20261104/intro/` 单轮43/43 CTest、220/220 Python通过，无跳过。私有TU共29例，原8例标题加21例片头；旧片头的15个漏门均被期待结果断言拒绝，已有错误门和正常／skip／quit控制不靠后续故障伪造通过。这里只验证SDL错误边界，正常原版配对仍按最终WGC记录登记。

### B1／B2交界：AI无演出MP费用（2026-11-04）

新研究发现旧“15311本体没有扣MP，所以只有gate”结论漏掉了下层wrapper。1ca89原始指令把actor+0x44的u16减去费用记录+5；无演出分支在逐目标循环前一次调用，ID17实际使用record18。生产仅在完整预检后、首effect前加入费用，保留失败后已提交状态；费用不放入逐目标纯effect，也不新增session字段。

独立候选 `fd2sdl-b1-20261104/mp-cost/` 单轮43/43 CTest、220/220 Python通过，无跳过；25个真实AI可评分ID、多目标一次、首RNG观察费用时点、miss／免疫、预检拒绝及behavior11后段失败保留均有回归。尚未进行原版实机施法对照；不能把该逻辑修复列为WGC已覆盖。真实START的side0/1没有可施法单位，首轮END不触发15311，因此此前START1→2证据范围不变。

动画2ff01→30e9d按caster part标记扣费；实际FIGANI回退条目存在零／多标记。当前实现只恢复已确认无演出分支，不能据unit9／ID0单标记把全部动画演员概括为一次。玩家法术和动画的运行验收仍由B2／B6补齐。

### B1.3c：受控胜败结果门（2026-11-04）

`run_original_result.py` 注册为第22组。只从真实START派生：victory将side0 actor的flags bit0置1；defeat只将raw0 flags bit0置1，重算checksum/XOR，保留来源及派生hash、全部明文与密文字节差异。HP、roster及未知字段不改；这不是整关实战。原版10010读档本身不运行stage结果门，随后Z／Enter必须各自命中资源ACK，选择态Esc后才由controller后置到205b4；若提前出现对应结果立即停键。

`result-graphics-02/` 正式运行原版两场后再SDL两场：胜利FDTXT[1]fragment9首说话人4的两行共2794字形／阴影像素及620对话顶边像素；败北FDOTHER79 frame0及无键自动等待的frame1分别检查完整64,000像素，GAME OVER字样已覆盖。全部bad=0、最大通道差3。SDL从同来源Continue后不发额外按键，分别要求唯一victory／defeat日志、相应未实现后续说明和自然exit2；不强行把原版controller输入时点套给SDL。

四个进程均在finally清理前／正常hostclose后按预期退出（原版0，SDL2），没有强制终止；外层Job58096 `worker_exit=0 timed_out=0 residual=0 forced=0 cleaned=1`，来源／隔离SAV bytes和mtime、原版hash均未变。前一原版only校准为probe_only且ok=false，正式01只验败北frame0；本轮02补frame1，旧记录不追改。所有路径相对C:候选根。全新最终候选43/43 CTest、231/231 Python通过，无跳过；完整22组正在统一执行。

该项只证明已确认结果门和应用来源分派：胜利没有继续对白验证11506持久同步或stage1，败北没有继续验证回标题，B5和完整V1仍未验。它不是SDL结算画面与原版相同的声明。

### B1最终整套验收（2026-11-04）

冻结暂存候选单轮43/43 CTest、231/231 Python、22/22 WGC全部通过，无跳过。证据根为 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b1-20261104/final/`；整套 `build/graphics-artifacts/20261104-204240-393412/summary.json` 的22项status均passed，含11项原版配对任务：标题、确认框、END、Load／Z、leave、总览、玩家取消、WAIT映色、March、F1及受控胜败。原版任务的24个进程和其余SDL窗口均正常清理，外Job46160无超时／遗留／强制终止，原版hash未变。

该次使用已经撤销March像素豁免并委托共享菜单的oracle，覆盖新增的Load后Z、F1和胜败GAME OVER帧。审查修复与独立复审已完成，B1支持范围当时据此关闭。历史17／19／20组和各个21组待跑记录仍是当时的范围，不改成当前通过量；该次以这份22组报告为准。

限制保持：受控终局不是整关自然实战；胜利只验首对白，未推进结算／持久同步／准备页；败北只验两帧，不验后续回标题。F1每端资源RGB和冻结门通过，双端实际水色周期没有对齐，不能宣称整帧原始RGB相等。AI费用只做静态／SDL逻辑回归，实际施法、全部FIGANI、完整音画／时序、未知handler和Linux本轮未验。

### B2.1a共享查询验证（2026-11-04）

B1关闭后，B2.1a仅把现AI五字节／40bit枚举共享到field_magic，实际AI caller复用；没有玩家UI状态，正式MAGIC继续禁用。独立fresh全构建及43/43 CTest单轮通过（含真实输入），另10/10源码地址／发布检查通过。容量0／截断／NULL、全40位、未知36..39、真实AI状态／MP门与输出不变都有回归；函数没有RNG依赖。

本阶段没有新增原版列表或施法运行，不把B1前一冻结基线的231项Python／22组WGC冒称为本次共享查询后的整套复跑。下一阶段先按真实START caster raw1的资源门校准列表、确认选择和Esc返回COMMAND，再接提交；恢复术13并非原START能力，另列来源和验收。
