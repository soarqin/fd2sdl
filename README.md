# 炎龙骑士团 2 SDL3 重写

使用原版数据，以 SDL3 重写《炎龙骑士团 2：黄金城传说》（Flame Dragon 2: Legend of Golden Castle，汉堂国际，1995）。运行重写版不需要原版 `FD2.EXE`。

**项目仍在开发中**：已有片头、标题、新游戏过场和第一关部分战斗；玩家法术／物品、经验升级、战斗演出和跨关卡流程尚未完成。完整范围和后续安排见[开发计划](docs/plans/development.md)。

## 准备与构建

需要 CMake 3.16+、C11／C++14 编译器，以及合法取得的原版游戏数据。将数据文件放入 `original_game/`；本仓库不分发原版资源。

从项目根目录执行：

```bash
cmake -S src -B src/build
cmake --build src/build --parallel
./src/fd2sdl
```

CMake 优先查找系统 SDL3（CMake package，其次 pkg-config）；找不到时通过 CPM.cmake 下载并构建，回退路径需要网络。离线构建应预装 SDL3，必要时设置 `SDL3_DIR`；`-DFD2SDL_USE_SYSTEM_SDL3=OFF` 可强制使用下载路径。

以上为 Linux 命令示例；Windows 输出为 `fd2sdl.exe`，路径随生成器和构建配置变化。启动时保持工作目录为项目根目录，以便找到 `original_game/`。

## 使用

默认从片头进入标题菜单。也可直接进入已有场景：

```bash
./src/fd2sdl --new-game-play  # 播放新游戏过场后进入战场
./src/fd2sdl --field-play 0   # 直接进入第一关；正式战场目前仅支持 stage 0
```

游戏使用键盘，不支持鼠标。标题用上／下选择并确认；战场常用键位如下：

| 操作 | 按键 |
|---|---|
| 移动光标／选择菜单项 | 方向键；数字小键盘 8／2／4／6 |
| 确认 | Enter、Space、数字小键盘 0／Enter |
| 菜单取消／返回 | Esc、Delete、数字小键盘小数点 |
| 查看焦点单位详情 | F2／Home |
| 关闭程序 | 关闭窗口，或在标题菜单选择退出 |

移动确认后，上／左／右／下对应攻击／法术／物品／待机；法术和物品当前不可用。攻击后选择目标并再次确认；取消目标选择回到指令菜单，取消指令菜单会恢复移动前状态。在玩家阶段的空格上确认可打开系统菜单，部分菜单功能仍未完成。

存档默认使用工作目录中的 `FD2.SAV`，可通过环境变量 `FD2SDL_SAVE_PATH` 指定其他路径。当前只支持第一关活动快照，四槽手工入口尚未开放。调试或兼容性验证请使用存档副本，不覆盖 `original_game/` 中的原始证据。

## 测试与开发文档

```bash
ctest --test-dir src/build --output-on-failure
```

部分测试需要本地原版资源；真实键盘回归还依赖 Linux/X11、Xvfb 和 XTest。无窗口验证、Python 测试及调试入口见[工具速查](docs/tools/README.md)。SDL 测试不替代 DOSBox 原版人工对照。

- [文档索引](docs/README.md)：计划、格式、系统设计和逆向依据。
- [AGENTS.md](AGENTS.md)：Agent 开发规则与修改前必读事项。

## License

项目尚未指定顶层 `LICENSE`，不能据第三方组件的许可证推定整个项目的授权方式。音乐组件分别适用 MIT／LGPL 等许可证，详情与原文见[第三方组件清单](src/third_party/README.md)；对外发布前仍需落实项目许可和相应义务。

原版游戏、图像、音乐及其他数据归原权利人所有，不随本项目分发。

## Credits

- 原版游戏：汉堂国际（Dynasty International Information）。
- 窗口、图形、输入与音频：[SDL3](https://www.libsdl.org/)。
- 音乐播放：BW MIDI Sequencer、XMIDI converter、OPL AIL model 与 Nuked OPL3；上游版本及许可见[第三方组件清单](src/third_party/README.md)。
