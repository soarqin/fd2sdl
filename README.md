# 炎龙骑士团 2 SDL3 重写

通过反编译 `original_game/FD2.EXE`，用 SDL3 重写《炎龙骑士团 2：黄金城传说》
（Flame Dragon 2: Legend of Golden Castle，汉堂国际 1995）。

## 目标

- 读取原版游戏的数据文件（11 个 .DAT）
- 兼容原版存档（`FD2.SAV` / `FD2.TMP`）
- 不依赖原版 `FD2.EXE`

## 文档

文档分类和完整索引见 [`docs/README.md`](docs/README.md)。主要入口：

- 总体开发计划：`docs/plans/development.md`
- 战场路线：`docs/plans/battlefield.md`
- 数据格式：`docs/formats/data-formats.md`
- 地址映射：`docs/reverse-engineering/address-mapping.md`
- 函数命名：`docs/reverse-engineering/function-names.md`
- IDA 独立复核：`docs/reverse-engineering/ida-crosscheck.md`
- 工具速查：`docs/tools/README.md`
- 生成产物：`docs/generated/`（当前反编译参考：`docs/generated/ida-canonical/fd2-ida-decomp-all.c`）

## 构建

CMake 会优先使用系统已安装的 SDL3（CMake config，其次 pkg-config）；若未找到，自动通过 CPM.cmake 下载并构建 SDL3，无需手动安装 SDL3。

```bash
cmake -S src -B src/build
cmake --build src/build
./src/fd2sdl      # 从项目根目录运行，以找到 original_game/
```

调试入口：

```bash
./src/fd2sdl --map-preview 0          # 地图预览
./src/fd2sdl --field-preview 0        # stage 0 正式单位预览
./src/fd2sdl --field-play 0           # stage 0 移动、指令菜单与普通攻击
./src/fd2sdl --field-effect-play 0    # 实时播放 actor flash、震动和转场验收序列
./src/fd2sdl --new-game-play          # 完整过场结束后进入正式战场
./src/fd2sdl --prologue-preview       # 完整新游戏初始过场
```

战场入口使用方向键或数字小键盘方向键移动光标，`Enter`、`Space` 或数字小键盘 `0` 确认；`F2` 或 `Home` 可打开当前焦点单位详情。确认移动后会展开原版 FDOTHER[2] 四向图形指令菜单：上／左／右／下依次为 attack、magic、item、wait。attack 仅在存在合法目标时可选；确认后会显示真实武器范围，移动光标选择目标，再次确认结算，或按取消键返回并重新展开菜单。wait 直接完成行动。magic 和 item 的用途及原版入口已确认，但玩家执行状态机尚未实现，当前显示禁用帧。菜单中按 `Escape` 或数字小键盘 `.`／`Delete` 会收起菜单，并恢复移动前坐标、方向和镜头。

普通攻击已接入原版武器范围、RNG 初值 `0x7a18`、profile 暴击基础表、武器 effect、地形攻防修正、双击、邻接反击、HP 扣减和 HP 0 隐藏提交。attack 与 wait 均通过正式指令状态机设置 acted；同一单位在本回合不能再次进入移动状态。全部玩家完成后，phase 按 `2 → 1 → 0 → 下一回合 2` 推进，side 1/0 使用已接入的 M7 AI 行动，不再以自动待机代替。菜单选择、待机和取消不推进 RNG。

M7 已接入物理计划／提交、可确认的 AI 法术与物品效果、自动 phase 和 stage 0 胜负门。玩家 magic/item、特殊 handler、FIGANI 演出、关卡结束回调与下一关初始化仍未完成。Linux/X11 环境下的 `field_play_input` CTest 会通过 Xvfb 与 XTest 向正式 `--field-play 0` SDL 循环发送按键，覆盖非零移动后 wait、acted 拒绝、完整 phase 推进、真实属性扣血和双击击杀；`--field-play-once` 只保留资源与逻辑集成检查，不作为正式输入状态机的验收依据。

可用 `-DFD2SDL_USE_SYSTEM_SDL3=OFF` 强制走 CPM.cmake 回退路径；离线环境可预先安装 SDL3，或通过 `SDL3_DIR` 指向已有 SDL3 CMake 包。

## 工具

`tools/dat_extract.py` —— .DAT 归档解包：

```bash
python tools/dat_extract.py list original_game/TITLE.DAT        # 列出条目
python tools/dat_extract.py dump original_game/TITLE.DAT 0      # 查看条目 0
python tools/dat_extract.py extract original_game/TITLE.DAT /tmp/out   # 解包
```

FDTXT 自定义字形索引的最终 Unicode 映射见 `docs/formats/font-glyph-map.tsv`，格式与控制码说明见 `docs/formats/font-text-mapping.md`。

音频资源清单：

```bash
python3 tools/analyze_fd2_audio.py                       # XMIDI 曲目和固定 PCM bank 摘要
python3 tools/analyze_fd2_audio.py --json                # 机器可读输出
python3 tools/analyze_fd2_audio.py --extract-sfx-dir out   # 按已确认参数导出 WAV
python3 tools/analyze_fd2_audio.py --extract-music-dir out # 导出 15 个 XMIDI
# DOSBox OPL capture 与外部原型渲染结果生成候选排名：
python3 tools/compare_fd2_music_capture.py capture.wav rendered/
```

## 当前进度

| 模块 | 状态 | 已完成内容 |
|------|------|------------|
| 基础设施与资源 | 已完成 | SDL3/CMake 骨架、`.DAT` 嵌套归档、调色板、图像、字体、文本、地图、地形、精灵和存档读取 |
| 启动流程 | 已完成 | 片头、标题、新游戏 stage 32 → 31 → 0 过场、ANI.DAT/AFM 播放器和正式战场 handoff |
| 统一单位模型 | 已完成 | 原版 `0x50` 字节记录、角色／敌军基础表、等级成长、装备派生、group 增援和存档逐字恢复 |
| 战场交互 | 进行中 | 键盘光标、原版选择框、移动／攻击范围 LUT、参数化寻路、ZOC、六相位移动、取消回退、acted 门和完整 phase 推进 |
| 战场 UI | 已完成当前范围 | 常驻格子信息面板；DATO 全屏角色详情页；215 项物品表；四名初始玩家实机装备；原版头像边框和 12 帧三段开合视觉 |
| 事件与战斗 | 进行中 | FDFIELD 回合／格子事件、stage 0 action 0..3、正式物理攻击、双击／反击、敌方 phase AI 和 stage 0 胜负门；玩家法术／物品、特殊 handler、FIGANI 与跨关卡推进尚未完成 |
| 音频 | PCM 已接入，OPL 原型已评审 | FDOTHER[31]/[80] 已加载；详情音效及 actor flash、stage transition、earthquake 的 snapshot、原版帧序和 cue wrapper 已实现；15 项 XMIDI 与 stage 曲目表已确认。正式 OPL 后端仍待许可证决策或 MIT/LGPL 组件化实现 |

当前快速入口会验证 `cursor/info/detail/move/move-exec/player-round/attack/turn/event/effect`。CTest 还包含逻辑、资源和存档测试；依赖齐备的 Linux/X11 环境会额外注册正式 SDL 战场输入回归。原版不支持鼠标，因此不计划增加鼠标映射。

## 音频实施方向

原版同时使用两类音频：

- `FDOTHER.DAT[31]` 等嵌套资源保存数字 PCM 音效，`sfx_play @0x4acaa` 通过 Miles AIL sample handle 播放。
- `FDMUS.DAT` 保存 XMIDI 序列；`SAMPLE.BNK`、`SAMPLE.AD` 和 `SAMPLE.OPL` 提供 AdLib/OPL 音色数据，`.MDI/.DIG` 是原版 AIL 硬件驱动。

音效和音乐不能继续以单个调用点零散接入。计划顺序如下：

1. 继续确认 PCM 采样率、循环语义、全部 sample bank 和音乐控制调用链。
2. 使用 SDL3 `SDL_AudioStream` 建立统一 device、music/sfx bus、命令队列、null backend 和离线 capture sink。
3. 先实现原始 PCM 音效，接入菜单与详情页 SFX，并对照 DOSBox 波形和时序。
4. 以 libADLMIDI 的 XMI + AIL bank + Nuked OPL3 能力制作保真原型，同时先完成 GPL/LGPL 组件的许可证评审。
5. 选定正式音乐后端后，再接入标题、过场、战场和战斗音乐。

不立即引入 SDL3_mixer：当前项目回退版本是 SDL `3.2.12`，新版 SDL3_mixer 要求 SDL `3.4.0` 或更高版本；同时升级 SDL 和 mixer 会扩大音频前置工作的变更面。完整证据、方案比较和验收标准见 [`docs/plans/audio.md`](docs/plans/audio.md)。

## 逆向工具

| 工具 | 用途 |
|------|------|
| Ghidra 11.3.2 | 32 位 x86 反编译为 C 伪代码 |
| radare2 5.5 | LE 加载、汇编、交叉引用 |
| capstone | 程序化反汇编 |
| Python | 自定义二进制解析 |

详见 `docs/reverse-engineering/decompilation-report.md` 末尾的工具命令备忘。
