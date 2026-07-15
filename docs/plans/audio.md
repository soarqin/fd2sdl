# 音频前置调研与实施计划

## 1. 当前结论

原版音频不是单一的「OPL 文件」或标准 MIDI 文件，而是 Miles AIL 3.02 管理的两条播放路径：

- **数字音效**：`sfx_play @0x4acaa` 从嵌套资源包取一段样本，向 AIL sample handle 设置地址、长度和循环次数后启动播放。
- **音乐**：`FDMUS.DAT` 保存 XMIDI 序列，由 `.MDI` 驱动选择 AdLib、OPL3、MPU-401、MT-32 等输出设备。Sound Blaster 配置并不代表音乐本身是 PCM。

因此不能直接从详情页 SFX 5/6 开始零散接音效。正确顺序是先确认资源与时序，再建立统一音频设备、混音和播放后端，最后接入音效与音乐调用点。

## 2. 已确认的原版资源

| 资源 | 当前结论 | 后续工作 |
|------|----------|----------|
| `FDMUS.DAT` | 外层 `.DAT` 共 20 项；其中 15 项以 `FORM/XDIR/XMID` 开始，5 项是 `20 0d 0a` 哨兵。有效内容为 Miles XMIDI | 解析 XMIDI 事件、循环与曲目映射 |
| `FDOTHER.DAT[31]` | 13 项嵌套 `LLLLLL` 数字样本；`DAT_00003eec` 固定加载该 entry | 继续登记 SFX 0..12 的用途和响度 |
| `FDOTHER.DAT[80]` | 16 项嵌套数字样本；stage/battle 路径将该 entry 载入 `DAT_00003b13` | 登记战场调用索引与用途 |
| `SAMPLE.BNK` | 头为 `01 00 ADLIB-`，含乐器名称和 AdLib 元数据 | 确认其与 `.AD/.OPL` 的角色分工 |
| `SAMPLE.AD` / `SAMPLE.OPL` | 两文件内容相同；开头是 `patch, bank, offset` 目录，结构与 libADLMIDI 的 Miles AIL bank loader 一致 | 验证转换后的音色与 DOSBox OPL 输出是否一致 |
| `*.MDI` | AIL 音乐驱动，包括 AdLib、OPL3、MPU-401、MT-32、Sound Blaster 等 | 不作为 SDL 运行时依赖，仅用于确认原版设备语义 |
| `*.DIG` | AIL 数字采样驱动 | 不应误当成游戏音效数据 |

`DAT_00003eec` 已确认来自 `FDOTHER.DAT[31]`，`DAT_00003b13` 来自 `FDOTHER.DAT[80]`。`DAT_0000414b` 由战斗动画 metadata 的 bank selector 动态加载，`DAT_00004153` 也由场景／动画表给出的 FDOTHER index 动态加载，因此二者不是固定 entry。`sfx_play(container,index,loop_count)` 会按嵌套 offset 表取得样本地址和字节长度，再依次调用 AIL sample 初始化、地址设置、循环设置和启动接口。

`ail_init_sample @0x5e735` 的 core 在 corrected code0 `0x566f4` 写入默认播放率 `0x2b11=11025 Hz`、loop count 1 和 pan 64；`sfx_play` 不覆盖播放率、格式、音量或 pan，只把第三参数传给 loop-count setter。每次播放前先调用 `ail_end_sample @0x5ea19`；index `-1` 在 stop 后直接返回，是明确的 stop sentinel。资源字节以 `0x80` 为中心，结合 AIL 默认路径确认当前 bank 为 unsigned 8-bit mono PCM。

FDOTHER[80] 的固定调用已由视觉逻辑与波形共同确认：

| SFX | 长度／11025 Hz | 调用位置与行为 |
|---:|---:|---|
| 1 | 4452 bytes／0.404 s | `field_actor_group_flash @0x414ee`：重绘 actor list，并在修改／原始 field buffer 间闪烁 5 次 |
| 11 | 11022 bytes／1.000 s | `field_stage_transition_effect @0x4982c`：9 帧资源转场，等待 500 ms 后渐暗；剧情调用方随后清屏并递增 stage |
| 13 | 15725 bytes／1.426 s | `field_earthquake_effect @0x4673b`：3 个缩放／偏移 field buffer 按 `0,1,2,1` 循环 60 帧，前 43 帧每 6 帧重启低频衰减样本 |

动态 bank `DAT_0000414b/4153` 的索引由 FIGANI／战斗动画 metadata 提供；index `-1` 同样用于停止当前全局 sample handle。

`music_track_play @0x4ab8b` 是高层音乐入口：它用 `DS:0x1a11` 缓存当前 track，从 FDMUS handle `DS:0x1a79` 加载对应 entry，固定初始化 XMIDI sequence 0，再启动 sequence。常规场景传 loop count 0，特殊段落传 1；AIL 语义需在 SDL 后端明确映射为无限循环／单次播放。track `-1` 将当前 sequence 在 4000 ms 内降到音量 0；普通曲目先设音量 0，再在 2000 ms 内升到 127，track 16/17 例外为立即设到 127。

## 3. 技术方案

### 3.1 SDL3 音频核心

第一阶段直接使用 SDL3 核心音频 API，不立即引入 SDL3_mixer：

- 打开一个 SDL3 logical playback device。
- 使用一个 `SDL_AudioStream` 回调生成最终交错双声道 PCM。
- 内部统一为 float32 stereo；设备采样率和格式转换交给 SDL3。
- 建立 master、music、sfx 三层增益和静音状态。
- 主线程只通过 64 项有界 SPSC 队列投递播放、停止和参数命令；音频回调内禁止文件 I/O、内存分配和锁等待。
- source 在自然结束、停止、替换、抢占、丢弃或销毁时收到 retire 原因；retire 只能发布可回收状态，资源释放留在主线程。
- 提供 null/headless 后端和离线 PCM/WAV capture sink，供 CTest、ASan 和时序对照使用。

SDL3 官方文档说明，SDL3 音频围绕 `SDL_AudioStream` 组织，可绑定多个 stream 并由设备混音，也可通过回调按需提供数据：[SDL3 Audio](https://wiki.libsdl.org/SDL3/CategoryAudio)。本项目先采用单 stream + 项目内混音，避免音乐合成器、PCM voice 和场景状态分别控制设备。

当前 CPM 回退版本是 SDL `3.2.12`。新版 SDL3_mixer 已完全重写，并要求 SDL `3.4.0` 或更高版本：[SDL_mixer](https://github.com/libsdl-org/SDL_mixer)。为接入音频而同时升级 SDL 和引入新 mixer 会扩大变更面，因此暂不作为前置依赖。

建议接口边界：

```c
typedef struct fd2_audio fd2_audio;

int  fd2_audio_open(fd2_audio *audio);
void fd2_audio_close(fd2_audio *audio);
int  fd2_audio_play_sfx(fd2_audio *audio, uint8_t bank, uint8_t index,
                        unsigned loop_count);
int  fd2_audio_play_music(fd2_audio *audio, uint8_t track, int loop);
void fd2_audio_stop_music(fd2_audio *audio);
void fd2_audio_set_bus_gain(fd2_audio *audio, int bus, float gain);
```

资源解析器、OPL/MIDI 合成器和 SDL device 不直接互相引用，统一通过「向混音器渲染 N 帧 PCM」的 source 接口连接。

### 3.2 PCM 音效路径

PCM 音效作为第一个可交付后端：

1. 解析 `FDOTHER[31]` 的 13 项嵌套资源。
2. 已确认 AIL 默认值为 11025 Hz、unsigned 8-bit mono、pan 64、loop count 1；继续确认全局默认音量。
3. DOSBox 0.74 mixer capture 已验证详情打开 SFX 5 和返回指令菜单 SFX 8：在 44100 Hz capture 中，按 11025 Hz 线性重采样的相关系数分别为 `0.950012` 和 `0.967969`；8000/16000/22050 Hz 候选均低于 `0.12`。
4. 将 unsigned 8-bit mono 转为内部 float32，交由统一混音器做显式 rate conversion。
5. `tools/analyze_fd2_audio.py --extract-sfx-dir` 导出离线 WAV，`tools/compare_fd2_sfx_capture.py` 比较 DOSBox capture。
6. 先接入菜单移动／确认和详情页 phase SFX 5/6，再扩展到战场其他 sample bank。

11025 Hz 来自 `AIL_init_sample` 的直接常量，不是按 DOS 常见值推测；SDL 后端仍应把 source rate 作为 bank metadata 传递，避免未来其他 bank 使用不同参数时写死在混音器中。

### 3.3 XMIDI 与 OPL 音乐路径

优先做 libADLMIDI 原型，但在正式依赖前设置许可证决策点。

libADLMIDI 的上游说明确认其具备以下能力：

- OPL3 模拟与 Nuked OPL3 后端。
- `WITH_XMI_SUPPORT`，直接支持 AIL XMI。
- 可使用来自 AIL、DMX、HMI 等 DOS 游戏格式的 FM patch。
- 可载入自定义 WOPL bank。

来源：[libADLMIDI](https://github.com/Wohlstand/libADLMIDI)。其 AIL bank 转换工具包含 Miles `patch, bank, offset` 格式 loader，可作为 `SAMPLE.AD` 解析依据：[load_ail.h](https://github.com/Wohlstand/libADLMIDI/blob/9760df4e0acfe27818dee38e786f3b1fea8c8e7f/utils/gen_adldata/file_formats/load_ail.h)。

原型流程：

1. `tools/analyze_fd2_audio.py --extract-music-dir` 已从 `FDMUS.DAT` 提取 15 个有效 XMIDI 条目。
2. 原型在 libADLMIDI commit `9760df4e` 的 `gen_adldata` 中直接加载 `SAMPLE.AD` AIL bank；正式接入仍需转换成 WOPL 或实现只读运行时 loader。
3. 已用 Nuked OPL3、AIL volume model 8、单芯片离线渲染 track 1：44100 Hz stereo、46.186 秒；15 项 XMIDI 也均通过 libADLMIDI 的 DOSBox emulator 完成离线渲染。`tools/render_fd2_opl_prototype.sh [track|all]` 可在 `/tmp` 重建外部原型，环境变量 `FD2_ADLMIDI_EMULATOR` 可切换 emulator。
4. 已从 `FD2.EXE` file `0x76e73`（corrected code0 `0x66073`）提取两组连续 30 字节 stage 曲目表。机器码 `0x35687` 从 `DS:0x1e63[stage]` 读取 primary，`0x3f78e` 从 `DS:0x1e81[stage]` 读取 alternate；两者地址恰好相差 30。stage 0 primary 已静态确认为 track 1，alternate 为 track 4。
5. `tools/compare_fd2_music_capture.py` 曾将一次来源不够严格的自动化 capture 的 track 19 指纹排在首位，但这与静态 stage 表冲突；该候选已否决，保留为「音频指纹不能替代调用链」的反例。
6. 另一份 capture 在截图确认的 `new_game_opening_play` 对话期间录制；机器码 `0x57629` 独立证明此处播放 track 11。15 项候选中 track 11 同时取得最高直接波形相关 `0.212089` 和最高短时谱相似度 `0.974435`。这确认 XMIDI、AIL bank、节奏／音色主路径可用，但直接波形相关仍明显低于 PCM 的 0.95，不能宣称逐样本保真。
7. 原型评审结论：技术路径可行，适合继续比较循环、打击乐和响度；许可证与逐样本差异使完整 libADLMIDI 暂不适合作为当前发布依赖。
5. 验证后再接入实时 source 接口。

libADLMIDI 上游同时包含 LGPL、GPL 和 MIT 组件：Nuked OPL3 为 LGPL 2.1+，MIDI sequencer／XMI converter 与 WOPL 模块为 MIT，但 README 将其余核心标为 GPLv3+。因此当前结论是：在仓库没有顶层 `LICENSE`、项目许可证尚未决定时，不把完整 libADLMIDI 直接链接为发布依赖；`tools/render_fd2_opl_prototype.sh` 只在 `/tmp` 构建外部评估工具。若最终不能接受 GPLv3，应评估以 MIT sequencer/XMI、MIT WOPL 与 LGPL Nuked OPL3 组成范围明确的自有后端，并逐文件复核许可证，而不是复制 libADLMIDI GPL core。

已确认的直接曲目调用上下文如下；这里只登记调用场景，不据此猜测曲名：

| FDMUS entry | 直接调用上下文 |
|---:|---|
| 10 | 战斗演出入口和演出结束恢复路径 |
| 11 | `new_game_opening_play @0x3231b` 在 `0x57629` 启动第一段可交互开场曲；另用于部分战斗结果及复活／转职演出结束后的恢复路径 |
| 13/14/15 | 按 `DAT_00003f4a` 分支选择的战斗结果演出 |
| 16 | 转职演出，调用方传 loop count 1 |
| 17 | 复活演出，调用方传 loop count 1 |

战场常驻曲目由两组 stage 索引表间接选择，不能仅凭上述直接调用推断。primary/alternate 的完整 30 项值由 `tools/analyze_fd2_audio.py` 输出；stage 0 分别为 track 1/4。`field_turn_cycle_run @0x3f51f` 在 `0x3f716..0x3f731` 比较两表并在不同曲目时淡出 primary，`0x3f78e..0x3f796` 在 phase 0 事件后启动 alternate，递增回合并完成 phase 切换后于 `0x3f824..0x3f82c` 恢复 primary。

FluidSynth 可作为兼容性备选：它是跨平台 SoundFont 2/3 软件合成器并支持标准 MIDI 文件：[FluidSynth](https://www.fluidsynth.org/)。该路径需要 XMIDI 转标准 MIDI 和额外 SoundFont，输出不会保持原版 AIL/OPL 音色，因此不作为首选保真方案。

## 4. 分阶段实施

### A0：格式与调用链确认

- [x] 确认 FDMUS 为 XMIDI。
- [x] 确认 `FDOTHER[31]` 是 `sfx_play` 使用的 13 项样本 bank。
- [x] 确认固定 bank：`DAT_00003eec→FDOTHER[31]`、`DAT_00003b13→FDOTHER[80]`；确认 `DAT_0000414b/4153` 为数据驱动的动态 bank。
- [x] 确认 PCM 默认 rate、格式、声道、loop count 和 pan；全局默认音量值仍待运行时确认。
- [x] 确认 `music_track_play @0x4ab8b` 的加载、开始、停止、循环和音量渐变调用链。

### A1：音频框架

- [x] 新增 `src/audio.[ch]`。
- [x] 建立 SDL3 device、stream callback、有界单生产者命令队列和 music/sfx bus。
- [x] 建立 null backend 与离线 float32 PCM render/capture 入口。
- [x] 添加 bus 混音、source retirement、music 替换、voice 抢占、NaN/削波、队列边界和 SDL dummy callback 测试。
- [x] 主程序已验证设备打开失败自动降级为立即 retire source 的 discard null backend，不占满命令／voice pool。

### A2：PCM 音效

- [x] 新增 `src/audio_pcm.[ch]`，实现嵌套 bank、U8 mono source 与显式线性 rate conversion。
- [x] 实现 24 项无分配 voice pool、停止、循环、增益和 retirement；pan 暂沿用原版默认中心值 64。
- [x] 主程序加载 FDOTHER[31]，详情打开 phase 11/5 接 SFX 5，关闭 phase 0/7 接 SFX 6，并按原版单 AIL handle 语义重启前一声。
- [x] 主程序加载 FDOTHER[80]，`src/field_audio.[ch]` 统一登记详情、actor flash、stage transition 与 earthquake cue。
- [x] 新增 `src/field_effect.[ch]`，按原版 frame 顺序调度 actor flash、earthquake、stage transition snapshot 与 cue；earthquake 为 `0,1,2,1` 共 60 帧，SFX 在 `0..42` 每 6 帧重启。
- [x] 实现三个效果的事务化 snapshot 生成器和原版入口 wrapper：actor flash 按 FDICON 非透明像素重绘为 palette `0xc0`；earthquake 按 `DS:0x2096..0x20b6` 的 `(x,y,step)` 三组参数重采样；stage transition 直接使用 FDOTHER[3] frame `9→1` 并复现 `field_transition_lut_mask @0x4725a`、500 ms 尾停顿和渐暗。
- [x] 新增 `--field-effect-play STAGE` 可达验收入口，使用真实 FDOTHER[80] cue 和定时 wrapper；SDL dummy 实测总时长约 1.84 s，与 550 ms flash、600 ms earthquake、45 ms transition frames、500 ms 尾停顿及 128 ms 渐暗之和一致。
- [ ] 继续映射并接入 `DAT_0000414b/4153` 动态战斗动画 bank。
- [x] 用 DOSBox mixer capture 对照 SFX 5/8 的时长与波形，确认 11025 Hz；SDL 线性插值相关系数为 0.95/0.968，但不宣称硬件逐样本等价。

### A3：XMIDI/OPL 原型

- [x] 提取 15 项 XMIDI；曲目场景用途仍待完整登记。
- [x] 原型通过 libADLMIDI `gen_adldata` 载入 `SAMPLE.AD` AIL bank。
- [x] 完成 track 1 的 libADLMIDI + Nuked OPL3 + AIL volume model 离线渲染。
- [x] 15 项 XMIDI 均通过 libADLMIDI DOSBox emulator 离线渲染。
- [x] 静态确认两组 30 项 stage 曲目表，并否决 stage 0 的 track 19 指纹候选；stage 0 primary/alternate 为 track 1/4。
- [ ] 结合 primary/alternate 切换条件完成其余场景／曲目语义映射。
- [x] 用调用链已知的 opening track 11 对照 DOSBox：候选排名第一，短时谱相似度 0.974，直接波形相关 0.212；判定主路径可行但非逐样本等价。
- [x] 完成许可证评审：完整 libADLMIDI core 含 GPLv3+，当前只保留外部原型；正式后端需项目许可证决策或 MIT/LGPL 组件化实现。
- [ ] 选定正式音乐后端。

### A4：音乐与场景接入

- [ ] 实现播放、停止、循环、切曲和 bus 音量。
- [ ] 接入标题、过场、战场和战斗演出。
- [ ] 处理窗口失焦、暂停、设备切换和存档恢复。
- [ ] 完成 Linux、Windows、macOS 与 SDL dummy 验证。

## 5. 验收标准

- 无音频设备时游戏仍可启动、测试和完成快速入口。
- 音频回调无文件 I/O、动态分配和非有界等待。
- PCM SFX 的索引、时长、循环次数及触发 phase 与 DOSBox 一致。
- XMIDI 的 tempo、loop、program change、控制器和打击乐通过离线测试。
- OPL 输出与 DOSBox 对照后才标记音乐后端完成。
- 原版音频资源继续从 `original_game/` 读取，不提交转换产物或原版文件。
