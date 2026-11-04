# 音频调用与对照依据

本文记录已确认的原版播放语义和局部捕获，不维护开发排期。资源布局见[数据格式](../formats/data-formats.md#4-音频相关文件)，后续工作见[音频计划](../plans/audio.md)，实际组件与许可证见[第三方清单](../../src/third_party/README.md)。函数地址均采用[规范命名登记](function-names.md)中的 canonical VA。

## 播放入口

### PCM

`sfx_play @0x25a96` 从嵌套 bank 的 offset 表取得样本地址和长度，先结束原 sample，再设置新样本及 loop count。index `-1` 仅停止。primary／secondary wrapper 对应不同 AIL handle，不代表固定资源 bank；换样本应复用各自 handle 的 replace 语义。

`ail_init_sample @0x39521` 直接初始化为 `11025 Hz`、loop count 1、pan 64；`sfx_play` 不覆盖 rate、format、volume 或 pan。现有样本路径为 unsigned 8-bit mono，字节中心为 `0x80`。循环参数 0 为无限，正数为总播放次数。全局默认音量仍待运行时确认。

| bank | 已确认调用 |
|---|---|
| FDOTHER[31] | SFX 2 对话逐字；5/6 详情页开合；8 四向菜单开合；9/10/11 移动脚步 |
| FDOTHER[77] | SFX 0 片头滚动；1 确认；2 选择移动；3 标题 action 入场。0/1/2 用 primary，3 用 secondary |
| FDOTHER[78] | ANI[1] frame 0 解码后、首个 delay 前播放 SFX 0，结束或跳过时停止 |
| FDOTHER[80] | SFX 1 actor flash；11 stage transition；13 earthquake |
| FDOTHER[95] | 两次 stage 0 group arrival 均在 frame 1 播放 SFX 0 |

FIGANI／动画 metadata 给出的 selector 动态决定 `DS:0x414b/0x4153` 对应 bank，不能登记为固定 FDOTHER entry。index `-1` 仍停止对应 sample handle。

已确认的战场效果：

| 原版函数 | FDOTHER[80] 样本 | 原版调度 |
|---|---|---|
| `field_actor_group_flash @0x1c2da` | 1，4,452 字节，约 0.404 s | 修改／原始 field buffer 交替 5 次 |
| `field_stage_transition_effect @0x24618` | 11，11,022 字节，约 1.000 s | LUT frame `9→1`，尾等 500 ms 后渐暗；后续场景去向由调用者决定 |
| `field_earthquake_effect @0x21527` | 13，15,725 字节，约 1.426 s | `0,1,2,1` 循环 60 帧，前 43 帧每 6 帧重启样本 |

标题滚动的 14 个阈值、闪电调色板和动画顺序见[启动流程](boot-flow.md)；脚步类别表、全局步态计数和逐字 BIOS tick 见[数据格式](../formats/data-formats.md)。不能为补音效另建播放器或在音频线程推进场景。

### XMIDI／OPL

`music_track_play @0x25977` 缓存当前 track，读取 `FDMUS.DAT` 对应 entry，初始化 sequence 0 后开始播放。同曲请求不重启；常规场景 loop count 0，特殊演出传 1。`DS:0x1a79` 是文件名字符串，不是文件 handle。

- 普通曲先设音量 0，在 2000 ms 内升至 127。
- track 16/17 立即设为 127。
- track `-1` 在 4000 ms 内淡出当前 sequence。

以下只登记调用场景，不猜测曲名：

| track | 已确认上下文 |
|---|---|
| 10 | 战斗演出入口和演出结束恢复路径 |
| 11 | `new_game_opening_play @0x3231b` 的开场曲；另有部分战斗结果、复活／转职后的恢复路径 |
| 13/14/15 | 按结果分支选择的演出 |
| 16 | 转职演出，loop count 1 |
| 17 | 复活演出，loop count 1 |
| 18 | `main @0x25bf4` 在调用标题 dispatcher 前启动；片头与标题共用，标题入口不重启 |

标题 track 18 继续覆盖新游戏最初两段对白；原版在 movement script `0x64` 前请求淡出，渐暗和镜头就位后才启动 track 11，不能在标题到新游戏的宿主边界提前硬停。

两张 30 项 stage 表决定 primary/alternate，完整字节见[音乐格式](../formats/data-formats.md#22-音乐格式fdmusdat)。stage 0 为 track 1/4。`field_turn_cycle_run @0x1a30b` 在两曲不同时淡出 primary，phase 0 事件后播放 alternate，新回合 phase 2 前恢复 primary；不能按听感或音频指纹替换静态调用依据。

## SDL 实现边界

- `audio.[ch]` 管理一个 SDL3 device／stream、float32 stereo 混音、master/music/sfx 增益、有界单生产者命令队列和 null/offline 输出。
- `audio_pcm.[ch]` 管理借用型 bank、24 项无分配 voice pool、rate conversion、replace／stop 和 retirement。
- `bgm.cpp` 直接读取 `SAMPLE.AD`，驱动 BW sequencer、XMIDI converter、OPL AIL model 和 Nuked OPL；默认对应原版 9 个 OPL2 voice，不链接完整 libADLMIDI core。
- 主线程准备资源；render/retire 回调禁止文件 I/O、动态分配、锁等待和阻塞。retire 只发布可回收状态，bank/source 数据必须活到最后一次引用退休。
- 无设备时允许 null/discard 降级，但不能因此把坏资源解释为合法。音频不可用、静音或跳过演出不能改变单位、phase 或 RNG。

播放器 API 以 `src/audio.h`、`audio_pcm.h`、`bgm.h` 为准，不另维护伪接口。失焦、Load、切曲与释放工作边界见[session 设计](../systems/battle-session.md)。

## 已有对照及限制

### PCM capture

DOSBox 0.74 的 44100 Hz mixer capture 已对照 FDOTHER[31] SFX 5/8。按 11025 Hz 线性重采样，相关系数分别为 `0.950012`、`0.967969`；8000/16000/22050 Hz 候选均低于 `0.12`。采样率另有 AIL 初始化直接常量支持，不能只按波形拟合猜测。

结果仅覆盖对应样本、时长和重采样路径，不证明所有 bank、硬件输出或逐样本等价。

### 外部音乐原型

以下是外部 libADLMIDI 原型结果，不是当前 `bgm.cpp` 的全场景保真验收：

- 原型固定 libADLMIDI commit `9760df4e0acfe27818dee38e786f3b1fea8c8e7f`，通过 AIL loader 读取 `SAMPLE.AD`；相关格式依据为上游 [load_ail.h](https://github.com/Wohlstand/libADLMIDI/blob/9760df4e0acfe27818dee38e786f3b1fea8c8e7f/utils/gen_adldata/file_formats/load_ail.h)。
- track 1 使用 Nuked OPL3、AIL volume model 8、单芯片，离线输出 44100 Hz stereo、46.186 s；15 项 XMIDI 也通过其 DOSBox emulator 完成离线渲染。
- 一次来源不够严格的自动 capture 将 track 19 排在首位，与 stage 0 静态表冲突，该候选已否决。
- 另一份 capture 有截图确认的开场对白上下文和独立 track 11 调用证据。track 11 在 15 项候选中直接波形相关最高，为 `0.212089`；短时谱相似度最高，为 `0.974435`。这证明主路径可行，不是逐样本保真。

`tools/render_fd2_opl_prototype.sh` 是 Linux bash 外部评估脚本，在临时目录重建评估工具；Windows 原生游戏和测试不依赖它，需复跑历史原型时使用单独的 Linux／WSL 环境并记录环境。`tools/compare_fd2_music_capture.py` 可在两平台比较已有 capture（Windows `python`／Linux `python3`），操作见[工具速查](../tools/README.md)。原版捕获、bank、渲染和转换产物只留本地。完整 libADLMIDI core 未被选作运行依赖，现用组件的逐文件许可证以第三方清单和源文件为准。
