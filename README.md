# 炎龙骑士团 2 SDL3 重写

使用 SDL3 重写《炎龙骑士团 2：黄金城传说》（Flame Dragon 2: Legend of Golden Castle，汉堂国际，1995）。重写版读取原版游戏数据，运行时不需要 `FD2.EXE`。仓库不包含原版资源。

## 构建

需要 CMake 3.21+、Python 3.9+ 和支持 C11／C++14 的编译器。Windows 可使用 MSVC 19.28（v142）及以上，或 MSYS2 的 GCC／Clang；Linux 可使用 GCC／Clang。Python 只用于构建期生成数据。

从仓库根目录运行。不同平台、生成器和编译器使用独立构建目录；旧的 `cmake -S src` 构建目录不能复用。

Windows（PowerShell，Visual Studio）：

```powershell
cmake -S . -B build/windows -A x64
cmake --build build/windows --config Release --parallel
```

Linux（Bash）：

```bash
cmake -S . -B build/linux -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux --parallel
```

CMake 优先查找本机 SDL3（CMake 配置包，其次 pkg-config），未找到时联网下载并构建。离线构建请预装 SDL3，必要时指定 `SDL3_DIR`。更多选项见[跨平台构建](docs/tools/platforms.md)，开发回归见[测试说明](docs/tools/README.md)。

## 使用

将合法取得的原版数据放入仓库根目录的 `original_game/`，然后以仓库根目录为工作目录启动：

- Windows：`./build/windows/Release/fd2sdl.exe`
- Linux：`./build/linux/fd2sdl`

默认从片头进入标题菜单。方向键移动或选择，Enter 确认，Esc 取消；F1 查看全场地图，F2 查看焦点单位详情。不支持鼠标。存档默认写入工作目录的 `FD2.SAV`，可通过环境变量 `FD2SDL_SAVE_PATH` 指定其他路径。测试旧档请使用副本。

完整键位、资源清单和兼容范围见[使用指南](docs/user-guide.md)。数据格式、逆向依据和发布说明见[文档索引](docs/README.md)。

修改单位、物品和事件流程可参考[静态数据与 Mod 制作](docs/formats/static-data.md)。

## Credits

- 原版游戏：汉堂国际（Dynasty International Information）。原版数据归原权利人所有，不随本项目分发。
- 窗口、图形、输入与音频：[SDL3](https://www.libsdl.org/)。
- 音乐播放：OPL AIL model 与 [Nuked OPL3](https://github.com/nukeykt/Nuked-OPL3)。组件来源及许可见[第三方声明](THIRD_PARTY_NOTICES.md)。
- 项目自有代码采用 [MIT License](LICENSE)。第三方组件继续遵循各自许可。
