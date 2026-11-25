# 项目工具速查

以下命令从仓库根目录执行。首次配置使用新的构建目录；不要复用以 `src/` 为源码目录的旧缓存。Windows 实际构建配置必须与 CTest 的 `-C` 一致。[跨平台约定](platforms.md)列出工具链、路径和环境变量的差异。

## 常规回归

默认不开启测试。下列测试构建不需要原版资源，覆盖启动与资源边界、存档、输入、战场提交与拒绝路径、准备流程、音频和冷定义。Python 工具回归也通过 CTest 注册。

Windows（PowerShell，MSVC）：

```powershell
cmake -S . -B build/windows-test -A x64 -DBUILD_TESTING=ON
cmake --build build/windows-test --config Debug --parallel
ctest --test-dir build/windows-test -C Debug --output-on-failure --no-tests=error
```

Linux（Bash）：

```bash
cmake -S . -B build/linux-test -DBUILD_TESTING=ON
cmake --build build/linux-test --parallel
ctest --test-dir build/linux-test --output-on-failure --no-tests=error
```

配置时加 `-DFD2SDL_TEST_ORIGINAL=ON` 可注册依赖原版数据的代表用例：标题和动画、回合与战斗、存取档、准备交接、过场及原版资源读取。需要[运行资源](../user-guide.md#游戏资源和启动模式)和 `original_game/START.SAV`；缺文件时 CTest 报未运行，不记为通过。用 `-L original-resources` 只选这组，用 `-LE original-resources` 排除这组。它们运行的是 SDL，不是 DOSBox 对照。

Linux 的独立窗口烟测需要 X11 和 XTest 开发库及 `xvfb-run`／`xauth`；无需游戏资源。Windows WGC 工具需要 MSVC、Windows SDK 和 C++/WinRT。用 `ctest --test-dir build/linux-test -N` 或 `ctest --test-dir build/windows-test -C Debug -N` 核对清单。需要将图形依赖缺失视为配置错误时，加 `-DFD2SDL_REQUIRE_GUI_TESTS=ON`。原生测试的范围和原版对照命令见[图形测试](gui-testing.md)。

### Web 资源与存档回归

在 [Web 构建](platforms.md#web-构建与本地预览)的配置命令中加 `-DBUILD_TESTING=ON`。启用测试时需要 Node.js 22 或更新版本。`web_game_data` 用合成 ZIP 检查资源选择、解压和 CRC 校验。`web_game_save` 模拟两个标签页争用存档和存储不可用的情况。实际浏览器启动、画布交互和存档恢复可在本地预览页验证。

```text
ctest --test-dir build/wasm -N
ctest --test-dir build/wasm --output-on-failure --no-tests=error
```

## 配置与引导回归

CTest 为所有用例设置 `FD2SDL_CONFIG=none`。正常运行和手工测试可用 `--no-config` 隔离本机配置。原版对照工具在 SDL 候选的实际命令行加入该参数，在进程收据的 `settings_identity` 记录 `none`。此时采用默认规则，含第 23 章招募和 30 级经验归零修正。涉及这两条分支的对照需要记录所用规则和对应原版字节。专测启动设置时，收据记录所选文件的解析路径及启动前 SHA-256，首次创建的文件其摘要为 `null`。

从已配置的 Windows 测试构建运行以下命令。`settings` 覆盖配置读写、优先级和指向原资源的目录别名。`input` 检查 BIOS 键值和映射共用的按住状态。`gamepad` 用两只 SDL 虚拟手柄验证热插拔、死区、长按和队列。`pacing` 检查默认及自定义等待和新游戏选项。`launcher_controls` 使用 SDL 虚拟设备检查操作与试听。`launcher_window` 使用原生单窗口截图和后台定向输入，检查六页文字、保存和放弃，以及从引导进入游戏。同一时间只运行一个原生图形用例。实体手柄的按键布局与手感须单独记录。

```powershell
ctest --test-dir build/windows-test -C Debug -N
ctest --test-dir build/windows-test -C Debug -R '^(settings|input|gamepad|pacing|launcher_controls|launcher_window)$' --output-on-failure --no-tests=error
```

## 定向命令

| 任务 | 入口 |
|---|---|
| 合成存档：校验和、读回、覆写、坏文件拒绝 | `ctest` 加 `-R '^save_synthetic$'` |
| Python 工具回归 | `python -B -m unittest discover -s tools/tests`（Linux 用 `python3`） |
| 检查源码规范地址 | `python tools/ida/check_src_addresses.py`（Linux 用 `python3`） |
| 从 FD2.EXE 导出冷表、生成 C 定义及核对原字节 | [冷定义工具](static-data.md) |
| 确认原版资源和模拟器配置身份 | [`original_reference.py`](../../tools/original_reference.py)，见[原版参照规范](../formats/original-reference.md) |
| 导出／调试原版 | [IDA 流程](ida.md)、[DOSBox debugger](dosbox-debug.md) |
| Windows x64 本地打包与离线重建 | [Windows Release](windows-release.md) |

已完成的阶段夹具与原验收记录见[历史证据索引](../history.md)。SDL 自测、局部原码、实际 DOSBox 对照和历史继承分别登记。
