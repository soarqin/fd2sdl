# 图形自动化测试

当前开发按 Windows 原生交互桌面验收，Linux 保留兼容说明和既有 Xvfb/XTest 入口；本阶段未执行的平台不声明通过。原版资源只读，所有写入和产物使用独立副本或构建目录。

## Windows 运行

```powershell
cmake -S src -B src/build-win -DBUILD_TESTING=ON -DFD2SDL_REQUIRE_GUI_TESTS=ON
cmake --build src/build-win --config Debug --parallel
ctest --test-dir src/build-win -C Debug --output-on-failure
```

多配置生成器的构建与 CTest 配置必须一致。`FD2SDL_REQUIRE_GUI_TESTS=ON` 在依赖不足时使配置失败；先以 `ctest --test-dir src/build-win -C Debug -N` 核实 `field_play_input`、`field_play_result`、`graphics_job` 实际注册。Windows SDK 必须提供 C++/WinRT；测试捕获工具为 C++17，音乐生产后端仍用 C++14。

正式输入回归使用真实宿主键盘，覆盖移动后待机、完整 phase、普通攻击及固定 seed 致死双击。Windows harness 先等 `field input ready`，单发 Z 并等 `field focus` 确认起始 actor 后才继续；ACK 缺失或焦点错误立即失败，不盲发 Enter，也不改变逐呈现帧输入语义。输入结果保存在 `test-artifacts/field-play-input/`。

## 捕获与原版证据

Windows 的 SDL 与 DOSBox 截图统一使用 Windows.Graphics.Capture 的 HWND 单窗口接口；禁止读取全屏／桌面 DC 后裁切，失败不得换用其他捕获方式。捕获须验证目标 PID、客户区几何、整数倍率、内容和检查点，黑帧或尺寸错误不是通过证据。WGC 工具不以目标窗口保持前台作为捕获条件；发送输入前仍须校验焦点和窗口归属。

原版允许在独立工作副本中自动操作。先依据 IDA 控制流、资源与数据确定前置状态和通过判据，再逐检查点输入、保留截图／存档／trace，并与 SDL 对应场景比较；只运行不崩溃不能算行为或视觉对照通过。静态机器码、SDL 自测和原版运行分别报告范围。

## 窗口及进程树清理

图形测试串行。记录本轮 PID，成功、失败、捕获失败与超时都执行正常关闭并等待，必要时再强制结束自身进程树。禁止按进程名关闭用户窗口；强制清理只能证明清理完成，不能证明程序正常退出。

Windows driver 的 `run-job` 在恢复工作进程主线程前将其挂入非继承句柄的 Job，以 `KILL_ON_JOB_CLOSE` 回收整棵本轮进程树。超时先向所属窗口发 WM_CLOSE，等待后才必要时终止 Job；worker 已退出但仍有子孙时同样失败。`graphics_job` 不开游戏或窗口，实测参数转义、非零退出码、超时及孤儿子孙回收。

## Linux 入口

使用 [跨平台约定](platforms.md) 的独立目录与 Bash 命令。Linux/X11 的正式输入依赖 X11、XTest 和 Xvfb；依赖缺失或未注册不得写成图形测试通过。Windows WGC 专用检查不作为 Linux 已通过的证据。
