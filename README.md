# 炎龙骑士团 2 SDL3 重写

使用原版数据，以 SDL3 重写《炎龙骑士团 2：黄金城传说》（Flame Dragon 2: Legend of Golden Castle，汉堂国际，1995）。运行重写版不需要原版 `FD2.EXE`。

**项目仍在开发中**：已有片头、标题、新游戏过场、第一关部分战斗及基础玩家法术；玩家物品、升级、战斗演出和跨关卡流程尚未完成。完整范围和后续安排见[开发计划](docs/plans/development.md)。

## 准备与构建

需要 CMake 3.16+、C11／C++14 编译器，以及合法取得的原版游戏数据。Windows 使用带 C++ 桌面开发组件和 Windows SDK 的 Visual Studio Build Tools；图形测试驱动另需 C++17 与 C++/WinRT SDK。Linux 使用 GCC 或 Clang。将数据文件放入 `original_game/`；本仓库不分发原版资源。

从项目根目录执行。两平台使用独立构建目录，不复用另一平台的 `CMakeCache.txt`。

Windows（PowerShell，Visual Studio 生成器）：

```powershell
cmake -S src -B src/build-win
cmake --build src/build-win --config Debug --parallel
./src/Debug/fd2sdl.exe
```

Linux（Bash，单配置生成器）：

```bash
cmake -S src -B src/build
cmake --build src/build --parallel
./src/fd2sdl
```

CMake 优先查找系统 SDL3（CMake package，其次 pkg-config）；找不到时通过 CPM.cmake 下载并构建，回退路径需要网络。离线构建应预装 SDL3，必要时设置 `SDL3_DIR`；`-DFD2SDL_USE_SYSTEM_SDL3=OFF` 可强制使用下载路径。

启动时保持工作目录为项目根目录，以便找到 `original_game/`。Visual Studio 的 Release 输出为 `src/Release/fd2sdl.exe`；Windows Ninja 单配置生成器的输出为 `src/fd2sdl.exe`。生成器、Python、环境变量与临时目录的对应方式见[跨平台命令约定](docs/tools/platforms.md)。

## 使用

默认从片头进入标题菜单。也可直接进入已有场景：

Windows（PowerShell）：

```powershell
./src/Debug/fd2sdl.exe --new-game-play
./src/Debug/fd2sdl.exe --field-play 0
```

Linux（Bash）：

```bash
./src/fd2sdl --new-game-play
./src/fd2sdl --field-play 0
```

`--new-game-play` 播放完整新游戏引导后进入战场。`--field-play 0` 是跳过引导的部署态调试入口，角色还没有执行开场移动，不能把它的截图当作自然新游戏的第一关初始画面。正式战场目前仅支持 stage 0。

游戏使用键盘，不支持鼠标。标题用上／下选择并确认；战场常用键位如下：

| 操作 | 按键 |
|---|---|
| 移动光标／选择菜单项 | 方向键；数字小键盘 8／2／4／6 |
| 确认 | Enter、Space、数字小键盘 0／Enter |
| 菜单取消／返回 | Esc、Delete、数字小键盘小数点 |
| 查看焦点单位详情 | F2／Home |
| 全场缩放图 | F1／Page Up；任意普通键关闭 |
| 关闭程序 | 关闭窗口 |

移动确认后，上／左／右／下对应攻击／法术／物品／待机；物品当前不可用。法术仅开放已确认的火炎术与恢复术分支，支持范围见[战场计划](docs/plans/battlefield.md#4-b2玩家法术p1)。普通施法者移动后当回合不能施法，原版少数例外按已确认规则处理。列表方向键选择，MP不足时确认仍停在列表；确认后选目标并再次确认。法术目标取消重开列表首项，列表取消才回指令菜单；普通攻击目标取消直接回指令菜单。取消指令菜单会恢复移动前状态。升级、掉落等未接入的结算分支不提交动作、不扣MP。

在玩家阶段的空格上确认可打开系统菜单，END 可结束回合。次级菜单默认选中战况总览，任意普通键可关闭；Save 需明确向左选择。系统确认框默认 YES，左选 YES、右选 NO，上下不切换，Esc 取消。march 会将合格玩家向当前焦点批量行军并推进整轮；F1／Page Up 可查看全场缩放图，关闭后焦点不变。后两项设置尚未开放。

单位详情可按任意普通键翻页或关闭；有法术时依次显示装备、法术，再关闭，无魔法时从装备页直接关闭。Esc 同样按此顺序处理，不跳过第二页。

存档默认使用工作目录中的 `FD2.SAV`，可通过环境变量 `FD2SDL_SAVE_PATH` 指定其他路径。标题 CONTINUE 恢复第一关活动快照；LOAD 的四槽手工入口尚未开放，会提示并留在标题。调试或兼容性验证请使用存档副本，不覆盖 `original_game/` 中的原始证据。当前到达胜／败后会分别说明尚未实现的后续流程并退出 `2`，不会自动进入下一关。

## 测试与开发文档

Windows（PowerShell）：

```powershell
ctest --test-dir src/build-win -C Debug --output-on-failure
```

Linux（Bash）：

```bash
ctest --test-dir src/build --output-on-failure
```

部分测试需要本地原版资源。窗口自动化的环境要求、运行命令、覆盖范围和截图证据见[图形自动化测试](docs/tools/gui-testing.md)；Windows 使用可交互桌面，Linux 使用 X11／Xvfb 和 XTest。无窗口验证、Python 测试及调试入口见[工具速查](docs/tools/README.md)。DOSBox 原版可在独立副本中自动操作和取证；SDL 自测通过仍需对应原版场景和判据才能证明一致。

- [文档索引](docs/README.md)：计划、格式、系统设计和逆向依据。
- [AGENTS.md](AGENTS.md)：Agent 开发规则与修改前必读事项。

## License

项目尚未指定顶层 `LICENSE`，不能据第三方组件的许可证推定整个项目的授权方式。音乐组件分别适用 MIT／LGPL 等许可证，详情与原文见[第三方组件清单](src/third_party/README.md)；对外发布前仍需落实项目许可和相应义务。

原版游戏、图像、音乐及其他数据归原权利人所有，不随本项目分发。

## Credits

- 原版游戏：汉堂国际（Dynasty International Information）。
- 窗口、图形、输入与音频：[SDL3](https://www.libsdl.org/)。
- 音乐播放：BW MIDI Sequencer、XMIDI converter、OPL AIL model 与 Nuked OPL3；上游版本及许可见[第三方组件清单](src/third_party/README.md)。
