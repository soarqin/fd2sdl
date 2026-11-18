# F6 Windows 回归清单

本轮记录 F1–F6 大阶段结束后的 SDL 全量回归。各小阶段只执行本阶段及直接受影响的测试。全量回归、问题修复及复测统一在大阶段收尾执行。本轮使用同一当前候选，无需切回历史提交。历史原版对照与研究记录仍可查询，完整原版对照及 DOS 硬件等价退出本次验收。

| 检查 | 执行与记录方式 |
|---|---|
| Windows Debug 构建 | 从独立检出构建全部默认目标，使用已安装的 SDL3，保存配置和构建日志 |
| CTest 注册清单 | 用 `--show-only=json-v1` 保存全部命令及属性；JUnit 区分通过、失败、未运行与跳过 |
| 无窗口 CTest | 在隔离检出运行所有无窗口测试；存档和其他写入仅在本次检出或临时目录生成 |
| SDL 原生窗口 | `field_play_input` 单独串行运行，保存 WGC、输入和 Job 清理记录 |
| Python 既有单测 | 用 `unittest discover` 扫描 `tools/tests/test_*.py`，逐项保存结果；缺本地夹具或解释器依赖时列出原因 |
| 资源及存档保护 | 原目录和用户存档只取摘要、大小和修改时间。已有资源及夹具只复制到本轮目录 |
| 通用回归修复 | 保存首次失败，维持现有断言和用例，再复测直接受影响的项目 |

Windows 收尾命令（PowerShell，`<build>` 为本次独立 Debug 构建目录）：

```powershell
ctest --test-dir <build> -C Debug --show-only=json-v1
ctest --test-dir <build> -C Debug -E '^field_play_input$' --no-tests=error --output-on-failure
python -B -m unittest discover -v -s tools/tests -p 'test_*.py'
```

`field_play_input` 通过同一构建的 Job 工具和显示需求入口单独运行，见[图形测试约定](../tools/gui-testing.md)。Linux 使用各自构建目录，单配置构建省略 `-C Debug`，Python 使用 `python3`。报告只登记实际运行的平台。

## Windows 执行结果

基线 `3fdf087098680cae4c0aa14a64a7ae0d34995f97`。独立构建使用 Visual Studio 18 的 Debug 配置、SDL 3.2.12 和已启用的图形测试注册。

| 检查 | 首轮 | 修复后与交付结果 |
|---|---|---|
| 全目标构建 | 7 个测试目标编译或链接失败 | 重建全部默认目标通过。恢复游戏 tick 边界计算的声明和链接符号，将 3 个画面测试接到现有绘制实现 |
| 无窗口 CTest | 171 项，166 通过，5 失败 | 重跑全部 171 项通过。修正阶段支持、存档缓存、单调时钟和 S5 临时目录夹具 |
| 原生 SDL CTest | 1 项通过，包含 wait、attack、lethal 各 2 个 WGC 捕获 | 复用同一应用二进制的窗口结果。显示恢复至 console／session ON，测试后释放显示需求，Job 无残留、无超时、无需强制结束 |
| Python `unittest discover` | 2335 次单测调用，日志有 2325 个通过、3 个断言失败、7 个错误记录和 3 个跳过记录。类初始化和导入错误按各自记录计数 | 定向复测恢复了失败、错误和一个缺夹具的跳过项，展开两个首轮无法加载的模块。按用例名合计 2371 个通过、无未处理的失败或错误 |
| 历史原码对照 | `LegacySourceTest`、`PreparationMachineTest` 在类初始化跳过 | 本轮验收范围不含两组原码执行及与 C 的对照。共 10 个方法未运行，保留测试及跳过记录 |
| 原资源及用户存档 | 记录 51 个受保护文件 | 大小、修改时间和 SHA-256 均一致。未复制项目根目录的用户档，未修改混音器或电源计划 |

首轮构建和测试原始日志位于 `F:/fd2sdl-evidence/f6-sdl-regression-20261118-01/`。CTest 清单 `ctest-inventory01.json` 的 172 个注册项与最终 `ctest-headless02.xml` 和 `ctest-native01.xml` 的用例名完全一致。后续变更仅涉及测试和文档，窗口测试使用的应用 SHA-256 始终为 `50d8f04f01455196b5bcd314d968df556d13116a1629e9a0f821cf34dbf235a4`。

Python 复测保存在 `python-targeted02.log`、`python-targeted03.log` 和 `python-unicorn-retest01.log`。定向复测使用已有证据及数据夹具的副本和本地缓存的 Unicorn 2.1.4。保留首轮历史夹具依赖失败、临时目录错误和断言失败记录；未重复执行全量 Python 测试。既有的断言用当前交付的路由顺序和绑定更新，仍检查取消、拒绝、无副作用和来源身份。应用原逻辑保持原有实现。

最终本地提交、快进、构建与保护结果见两树 `build/f6/sdl-regression-20261118/local-integration.json`。Linux、原版行为对照、DOS 硬件等价未执行。本轮 Windows 结果不用于认定原版完整一致。
