# 炎龙骑士团 2 SDL3 重写

使用原版数据，以 SDL3 重写《炎龙骑士团 2：黄金城传说》（Flame Dragon 2: Legend of Golden Castle，汉堂国际，1995）。运行重写版不需要原版 `FD2.EXE`。

**项目仍在开发中**：当前支持stage0／1原范围、stage2—29已证战斗绑定／END／事件与胜利准备交接，以及26坏结局和29终章的credits／特殊战斗／终端。S0—S5已交付；resource30仅内部尾声资源，不开放普通战场。支持域、450条默认拒绝与唯一开发入口见[统一计划](docs/plans/development.md)，新增批次和分层证据见[S5报告](docs/plans/s5-batch-expansion.md)，历史统一回归见[S4报告](docs/plans/s4-unified-review.md)。不承诺全部自然整关、任意外来存档、无限回合、全像素／共享RNG／声音或其它平台一致。

原版对齐补完正在实施，当前进度与未验边界统一见：[差异与证据矩阵](docs/plans/full-alignment-audit.md)、[F1—F6统一计划](docs/plans/development.md)。最终验收平台为Windows；按用户选择保留Nuked声音并明确记录为差异，不能称原版全部声音完全一致。

## 准备与构建

需要 CMake 3.16+、Python 3.9+（仅标准库，用于构建期生成冷定义）、C11／C++14 编译器，以及合法取得的原版游戏数据。Windows 使用带 C++ 桌面开发组件和 Windows SDK 的 Visual Studio Build Tools；图形测试驱动另需 C++17 与 C++/WinRT SDK。Linux 使用 GCC 或 Clang。将数据文件放入 `original_game/`；本仓库不分发原版资源。

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

`--new-game-play` 播放完整新游戏引导后进入应用层战场流程。`--field-play 0` 是跳过引导的独立部署态调试入口，角色还没有执行开场移动，也不执行应用层跨关交接。正式应用流程按已证绑定支持stage0—29的相应战斗／END、准备交接、活动恢复与四槽；标题CONTINUE与LOAD按同一能力门恢复。26坏结局与29终章保持原非返回出口，resource30不作为普通战场。默认turn255拒绝及外来32人credits边界保留，具体见统一计划。不能把调试入口截图当作自然新游戏的第一关初始画面。

游戏使用键盘，不支持鼠标。标题用上／下选择并确认；战场常用键位如下：

| 操作 | 按键 |
|---|---|
| 移动光标／选择菜单项 | 方向键；数字小键盘 8／2／4／6 |
| 确认 | Enter、Space、数字小键盘 0／Enter |
| 菜单取消／返回 | Esc、Delete、数字小键盘小数点 |
| 查看焦点单位详情 | F2／Home |
| 全场缩放图 | F1／Page Up；任意普通键关闭 |
| 关闭程序 | 关闭窗口 |

移动确认后，上／左／右／下对应攻击／法术／物品／待机。法术按角色实际拥有的正常技能位开放，覆盖伤害、恢复、状态、传送和派生／复合效果，当前范围见[战场计划](docs/plans/battlefield.md#1-当前可用范围与剩余缺口)。普通施法者移动后当回合不能施法，原版少数例外按已确认规则处理。列表方向键选择，MP不足时确认仍停在列表；确认后选择中心或目标，传送另选目的格。目标任一层取消重开列表首项，列表取消才回指令菜单；普通攻击目标取消直接回指令菜单。取消指令菜单会恢复移动前状态。资源、目标和必需尾链在动作前重验；提交后异常保留已写结果并停止，不自动重试或退款。

物品子菜单为 Use／Give／Equip／Drop，各项独立检查支持范围。Use 接入原版已确认的恢复、清状态、属性增长、状态附加、伤害和传送分支；普通武器／防具可查看，不能在 Use 列表确认。目标取消回列表首项，列表取消回物品子菜单，子菜单取消才回指令菜单。草药满 HP 也消耗一次并结束行动；其他物品是否消耗按原版分支处理。物品不增加单位经验，传送按原版条件扣 MP；未知或稀疏库存不开放。

Equip 按原版职业许可换装，成功后留在原列表及选中项；不兼容物品仍留页，Esc 返回。Drop 可删除普通或已装备物品，取消及成功删除均重算装备派生值；它不结束行动。余项返回物品子菜单，删空返回指令菜单；之后取消移动保留已装备或丢弃结果。

Give选择相邻目标，有空槽时直接转交，满槽时选择对方物品交换。各层取消回物品子菜单；成功交接后取消外层指令菜单会保留当前位置和库存并结束行动。原版无相邻友军时禁用Give。未知资源、掉落callback或必需格事件仍有明确支持门，不会用近似结果继续提交。

在玩家阶段的空格上确认可打开系统菜单，END可结束回合。次级菜单默认选中战况总览，任意普通键可关闭；Save需明确向左选择。系统确认框默认YES，左选YES、右选NO，上下不切换，Esc取消。march会将合格玩家向当前焦点批量行军并推进整轮；F1／Page Up可查看全场缩放图，关闭后焦点不变。效果（FULL／SHORT）设置按原版各动作调用规则接入；信息板设置控制上下两块单位信息板，并随存档保存／恢复。

单位详情可按任意普通键翻页或关闭；有法术时依次显示装备、法术，再关闭，无魔法时从装备页直接关闭。Esc 同样按此顺序处理，不跳过第二页。

存档默认使用工作目录中的 `FD2.SAV`，可通过环境变量 `FD2SDL_SAVE_PATH` 指定其他路径。标题 CONTINUE 按统一能力及资源门恢复 stage 0—29 的已证活动快照。标题 LOAD 和酒店 Load 可恢复具有原准备记录或NoTown定义、且资源就绪的 stage 1—29 准备状态；酒店拒绝NoTown目标，标题LOAD可进入其准备选择。具体原版对照范围见[审计§10](docs/plans/common-logic-audit.md#10-g3-实现与定向验收)与[S5报告](docs/plans/s5-batch-expansion.md)。准备页保留最多32人的持久队伍，出阵按原条件选择16或20人，未出阵成员继续保留。容量不扩大任意组合或外来32人credits的支持域；后期战场按当前已证绑定开放。原版与SDL实际写出的五人存档已双向经过正式LOAD、成员／库存查看及酒店新槽Save验证；21人NoTown亦已双端完成正式Save及返回选择页。调试或兼容性验证请使用副本，不覆盖 `original_game/` 中的原始证据。

商店支持按原货单购买、出售、装备与准备态转交；购入装备可选择收件人及是否立即装备，转交不进行战场的满包交换。教会支持符合条件成员的复活与转职，特殊转职按所持道具判断。取消逐层返回；已确认的业务结果不会因后续返回而撤销。独立装备重选、不同成员转交、一个隐藏商店入口及NoTown存档已有原版代表；未逐分支实机运行的部分按C回归、局部原码和代表语义类登记，见[审计§10](docs/plans/common-logic-audit.md#10-g3-实现与定向验收)。本轮未执行Linux验收。

正式应用流程已连接第一关胜利结算、准备页和下一关，败北流程返回标题；独立 `--field-play` 仍只报告胜／败并退出 `2`。A7 已核当前可达的事件、voice、曲目循环／fade及选项／Load行为，A8 已完成 Windows 本地降级和资源生命周期。2026-11-07 用户选择保留 Nuked 原始响度与音色，不以音色相似度作阶段门；这不等于完整波形或严格绝对时序相同，也不影响按原版模型修正 PCM 默认增益。后续范围与 R1 发布前置见[开发计划](docs/plans/development.md)和[音频计划](docs/plans/audio.md)。

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

项目自有代码采用 [MIT License](LICENSE)。第三方组件继续遵循各自的 zlib、MIT、LGPL 等许可，版权、许可原文和静态链接的可重建材料要求见[许可与第三方声明](THIRD_PARTY_NOTICES.md)。整个发布包不能仅标为 MIT 后省略第三方条款。

原版游戏、图像、音乐及其他数据归原权利人所有，不随本项目分发。

R1 提供 Windows x64 本地 Release 包及可替换 Nuked 后重建的源码／匹配 SDL SDK 材料，不自动上传或对外发布。启动、资源位置、实际依赖记录与离线重建步骤见 [Windows Release 说明](docs/tools/windows-release.md)；本地复制成功不等于发布验收或重链接验证通过。SDL 直接读取 `FDICON.B24`，不需要原版的 `FD2.TMP` 缓存；导出可交给 DOS 原版使用的 TMP 不在本轮支持范围。Linux／macOS 发布未验收，不据此阻塞本轮 Windows 定向验收，也不声明其他平台通过。

## Credits

- 原版游戏：汉堂国际（Dynasty International Information）。
- 窗口、图形、输入与音频：[SDL3](https://www.libsdl.org/)。
- 音乐播放：原 EVNT／32 槽时序，`SAMPLE.AD` 音色表、OPL AIL model 与 Nuked OPL3；历史 BW／XMIDI converter 的来源及全部许可仍见[第三方组件清单](src/third_party/README.md)。
