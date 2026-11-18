# 原生窗口测试与原版对照

## 判据和进程边界

- 图形用例串行执行。只使用本轮拥有的窗口和子进程，先请求正常关闭并等待，必要时才强制结束。成功、失败、超时都须清理，不按进程名关闭其他窗口。
- 原版资源和已有存档只读。DOSBox 挂载完整隔离副本；写入产物前解析实际 `original_game/` 目录（含符号链接或 junction），拒绝写进该目录及其子目录。
- 分别记录输入投递、实际游戏响应、尺寸和内容、检查点、保存结果与进程清理。输入发送成功、空白截图、缺少检查点或仅退出码为 0 都不能代替行为判定。不要跨候选拼接端点并称为一轮新通过。

## Linux：X11 和 Xvfb

安装 X11 和 XTest 开发库以及 `xvfb-run`、`xauth`。例如 Debian／Ubuntu 使用 `libx11-dev libxtst-dev xvfb xauth`。配置、构建和运行：

```bash
cmake -S . -B build/linux-test -DBUILD_TESTING=ON -DFD2SDL_REQUIRE_GUI_TESTS=ON
cmake --build build/linux-test --parallel
ctest --test-dir build/linux-test -R '^native_window$' --output-on-failure --no-tests=error
```

`native_window` 在独立 Xvfb 中创建 SDL 窗口，检查实际 X11 窗口的渲染像素，用 XTest 投递 Enter 和 Pause，验证正式输入层的 BIOS 键码与退出请求，然后销毁窗口。它不运行战场，也不是完整画面对照。

提供[原版资源](../user-guide.md#游戏资源和启动模式)及 `START.SAV` 后，配置时加 `-DFD2SDL_TEST_ORIGINAL=ON`，用 `ctest --test-dir build/linux-test -R '^field_play_input$' --output-on-failure --no-tests=error` 验证正式 SDL 战场的待机、攻击及收尾。该入口不运行 DOSBox，也不执行 WGC。

## Windows：WGC 和后台输入

使用 MSVC、Windows SDK、C++/WinRT 和可交互桌面。启用 `BUILD_TESTING` 后可构建 `fd2_field_play_input_driver`。`graphics_job` 检查 Job 对本轮进程树的正常、失败和超时清理。`field_play_input` 还要求 `FD2SDL_TEST_ORIGINAL=ON` 及本地资源。用与构建一致的 `-C Debug` 或 `-C Release` 执行 CTest。

- SDL 和 DOSBox-X 都使用 Windows Graphics Capture 的目标 HWND 接口。验证客户区尺寸、可用内容和对应业务资源；禁止截取桌面 DC 后裁切，失败不换捕获方式。
- 使用按本轮 PID／HWND 校验过的后台窗口消息。不得激活目标窗口、点击标题栏或退回全局 `SendInput`。后台投递与业务响应分别留证，不把消息收据当作原版键态。
- 保持当前显示状态，核实活动输入桌面和 WGC 实际可用。已在 console 和 session 显示全程 OFF 时通过 SDL 的 `field_play_input`、双端标题对照及双端 FULL 物理攻击的连续帧、音效和存档检查。声明熄屏取证时，从启动前到进程清理完成持续记录两路显示状态通知。
- 静态页用首尾离散帧判断。动态过渡须先确认采集就绪，再发唯一触发键。未采到必要阶段就记录缺证，不放宽像素门或补发按键。

### 显示需求与恢复

仅在需要恢复显示或临时保持显示状态时使用本节流程。部分早期测试入口会自行申请显示需求，复测熄屏条件前须核对实际入口。2026-11-09 的授权允许在需要时主动唤醒一次并记录结果。用 `SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED)` 申请临时需求，并实际确认 WGC 可用。不得周期轮询或循环唤醒，不修改用户混音器。

若临时需求不足，先记录活动方案 GUID、供电方式和拟改项的原 AC／DC 值。优先复制原方案，只修改当前供电方式下必要的超时，记录临时 GUID 及实际查询值。清理本轮窗口、进程并核验来源后，恢复原方案、删除本轮副本并重新查询。用户已切换到其他方案时保留并报告冲突，不覆盖用户选择。请求线程须在成功、失败或取消后恢复原需求。异常中断后在下轮窗口启动前核实并恢复本任务的遗留设置。

### 标题与自然开场对照

以下是当前目录结构的示例，不是新执行记录。需要匹配的 DOSBox-X、原版资源、MSVC 测试构建和 Python 的 Pillow。确认实际 DOSBox-X EXE 路径；使用新产物目录。以 owned Job 包裹 Python 和子进程树：

```powershell
$driver = './build/windows-test/Debug/fd2_field_play_input_driver.exe'
$game = './build/windows-test/Debug/fd2sdl.exe'
$dosbox = 'C:/Tools/DOSBox-X/dosbox-x.exe'
& $driver run-job 180000 python -B tools/run_original_comparison.py --root . --game $game --driver $driver --dosbox $dosbox --artifacts build/graphics/title-new
```

`run_original_comparison.py` 核标题菜单方向键和取消，不覆盖新游戏。`run_original_opening.py` 以相同参数执行自然开场，需单独的目录和足够的 Job 时限。`run_original_end_turn.py`、`run_original_load.py` 分别检查回合和活动存取档，使用有正确前置状态的原版 `START.SAV` 或显式只读夹具。先阅读对应脚本的场景与判据，不能把受控夹具换成任意外来存档。历史原版截图和来源资格见[历史证据索引](../history.md)。
