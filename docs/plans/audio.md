# 音频状态与后续实施计划

## 1. 当前基线

2026-11-04 的代码基线 `e9a2d81` 已有统一音频核心、PCM、XMIDI/AIL/Nuked 后端，以及标题和新游戏过场接入。**A0～A4 不再是待重做的前置工作。** A5 随 B5、A6 随 B6 完成各自支持范围，A8 已提交 `ec8e699`。本轮 A7 完成当前可达场景的受支持事件与声音行为验收，按 2026-11-07 用户选择保留后端原始响度，不把 MIDI/OPL 音色差异列为阶段门。跨系统顺序和后续 R1 见[总计划](development.md#4-新优先级与依赖)。

| 工作包 | 已有能力 | 尚未覆盖 |
|---|---|---|
| A0：格式与调用链 | PCM 默认参数与名义 sample 增益、固定／动态 bank；XMIDI、stage 曲目表及播放／淡变入口 | 未开放场景；最终 DAC／OS 混音不由名义增益推定 |
| A1：音频框架 | SDL3 device／stream、music/sfx bus、有界队列、retirement、null/offline；A8 本地降级与生命周期完成 | 真拔插、WASAPI 恢复与默认路由迁移未单独实测 |
| A2：PCM | 24 voice、U8 mono、rate conversion、replace／stop；标题、对白、脚步、菜单与已确认战场效果 cue | FIGANI 动态 bank 归 A6 |
| A3：XMIDI/OPL | 原 EVNT／32 duration 槽、SBPRO2.MDI 20 逻辑槽／18 硬件 channel、整数音量／fade，读取 `SAMPLE.AD` 并由 Nuked 合成 | 保留原始响度与音色；完整波形、严格 PIC／绝对时序和后续场景未验 |
| A4：场景接入 | 标题18、开场11、stage 0 的19／12／19及 Music／SFX、Load 接线 | 配置与曲目请求恢复不等于 sequence 播放位置恢复；证据按实际单端／联合来源登记 |

历史记录包含 Linux 原生、SDL dummy 和 MinGW 交叉构建。当前开发环境已迁移到 Windows，可在本机自动运行窗口输入、设备打开与关闭回归；执行范围与结果见[图形自动化测试](../tools/gui-testing.md)。Windows 实际声音保真、macOS 与未覆盖的设备生命周期仍须分别登记，不能由交叉构建或 dummy 通过替代。

### 依据与实现边界

- [音频调用与对照依据](../reverse-engineering/audio.md)：bank 用途、触发帧、曲目调用、PCM 捕获与外部原型结果；不把局部相关系数当全场景保真。
- [数据格式](../formats/data-formats.md)：XMIDI、PCM bank、AIL 音色表和 stage 曲目表布局。
- [session 设计](../systems/battle-session.md)：BGM owner、Load、离场、演出与资源生命周期。
- 实际 API：`src/audio.h`、`audio_pcm.h`、`bgm.h`、`field_audio.h`；不另维护建议接口，不引入 SDL3_mixer 或临时播放器。
- [第三方清单](../../src/third_party/README.md)及文件头决定组件许可；完整 libADLMIDI core 只用于外部原型，不是运行依赖。顶层 LICENSE 与 LGPL 发布义务仍是 R1 硬门。

## 2. 后续工作包

### A5：战场曲目与恢复边界（P1）

依赖 B0 和现有 A4，可与玩家动作独立推进；不是 FIGANI 的后置任务。

B5 已接入应用持有的 BGM 请求：城镇 10、酒店 13、出城淡出、stage 0／1 入口与活动 Load 的 primary，以及 phase alternate／primary。沿用现有同曲不重启和音频 render 执行的 4 秒 stop 淡出；null/offline 逻辑与真实设备声音分别验证，不在 Game 或 SAV 新增 sequence 真值。

Windows原版 controlled C10录音支持城镇10／酒店13／入场后19；最终自然项采用C25完整原版＋C27独立SDL全链联合证据，C25首phase的闭合录音30.704秒按六参考、五个固定窗口得到19／19／12／12／19首候选，支持静态19→12→19调用顺序。旧C2等失败流程的音频前缀仅保留当时范围，不能改写其业务结果。来源与限制见[音频依据](../reverse-engineering/audio.md#b5-windows-场景与-phase-录音)，实际冻结和独立Job见[阶段日志](stage-log.md)。这些证据未覆盖SDL实时声音、完整runtime曲目指针、绝对时序或A7保真。

- [x] 在正式战场入口请求对应stage primary；按 `field_turn_cycle_run @0x1a30b` 边界淡出、进入alternate，再于新回合恢复primary。stage 0／1的19／12依据canonical表与调用者，不按听感另选曲。
- [x] 在应用／场景编排连接BGM引用及同步曲目请求，播放器仍由应用持有；Game、绘制与音频回调不共同维护当前曲目真值。
- [x] 标题／新游戏、Load、离场、胜败／下一关的替换与淡出已连接；同曲不重启，配置恢复不重置单位、phase或RNG。
- [x] null／offline覆盖请求顺序、loop、淡变与bus；真实输入覆盖Load和离场，原生场景／phase录音提供已列曲目身份与切换证据。此项只完成A5编排验收，精确时序和声音保真继续归A7。

### A6：FIGANI 动态音效 bank（P2）

依赖 B6 的 metadata／帧序证据，不按全局变量名猜固定资源号。当前受支持实现及既定最低验收**已完成**：已接物理 `47+selector`、FULL0..9的82／82／83..90及地图恢复术13的bank80，primary与`sfx_play_secondary @0x25b45`各有原handle owner；动态bank与样本在播放前预检，字节引用随source退休。具体语义见[音频依据](../reverse-engineering/audio.md)，最低实机范围见[阶段记录](stage-log.md#b6a6full-战斗演出与动态音效)。

- [x] 确认动态 selector、样本索引、handle replace／stop、循环和触发帧；复用现有 PCM 后端。
- [x] 明确演出跳过、换 bank、Load 和离场时的 source retirement；bank 字节活到最后一个引用 source retire，音频回调不做文件 I/O 或释放。
- [x] 验证同一 FULL 动作在完整播放、主动跳过、静音或无设备时，业务和 RNG 均一致；SHORT 与 FULL 保留原本不同的攻击次数／FX随机流。失效 selector／坏样本不能伪装成成功解析。

核心对照仅验证预先声明的有限 VOICE 身份及录音内有序存在，物理与Magic0、恢复术13分别登记实际／离线来源；相同样本字节只证明声音等价类，Magic0的七次重复实例未逐次验收。原版DOSBox WAV无首packet QPC，不声明精确输入时延；完整时序、全部声音、混音与A7仍待验。AI业务／SAV及取证报告的`collected_only=true`不等于其全部FX／声音已配对；新增ITEM评分场景也只证明该动作和完整终态。A6完成的是本阶段动态bank实现与核心最低声音门；AI的30组casting配对不扩为其目标FX或全部SFX通过，精确时序及A7继续单列。

### A7：保真与场景对照（P2）

**完成口径：当前受支持行为范围完成，不是全游戏／全波形保真。** 2026-11-07 用户选择保留当前 Nuked 原始响度，不作 BGM 补偿；另明确 MIDI/OPL 音色受驱动与解码库影响，不再作为本阶段验收门，也不新增该类测量。已有标题固定素材约 2.1 倍响度差异保留为后端边界；它不是所有曲目的通用倍率，不据此拟合增益。最终交付树测试记录见[阶段日志](stage-log.md#a7当前受支持音频行为与后端边界)。

- [x] 对标题18、自然 START 最初两段对白／script64 淡出／开场11、stage 0 一次 END 的19→12→19及 Round002完成实际资源／场景与有限声音观察。
- [x] PCM 以原 `0x40240` 的256值名义表确认默认 sample volume100、pan64时的应用层 gain 为 `101/128`；通用 PCM 显式 gain 语义不变。代表性 once／replace／stop和启用后的31/8身份有单列证据，不能扩为全部实例或最终设备幅度。
- [x] 按原 EVNT／32 duration 槽和 SBPRO2.MDI 分支修正事件、voice、controller、loop 与整数 fade；15首实际曲目一次／两次循环机器码向量通过。标题预声明窗口3748/3748映射寄存器写序通过，严格 PIC 时序比较仍为 RED；两者分别保存。
- [x] Music 逐字节非零→0／零→1，使用原1秒 sequence-volume切换；SFX 只改变后续 cue／stop 门。成功活动 Load／CONTINUE 的 stop→primary、手工槽场景恢复及跨标题配置交接分别验证；未触及 raw 配置字节保留，坏档保持 SDL 的安全无副作用扩展。

四 episodes 的六个实际目标是四 SDL／两原版，不是六 SDL。SFX重开C3采用旧失败运行的闭合原版声音、新原版业务／Save和新SDL窄场联合，不是 fresh 音频 pair；原失败报告不改记通过。所有 capture／输入／SAV 和工具事实按来源封存，纯解析、采集成功或谱排名不能互相替代。完整资料见[音频依据](../reverse-engineering/audio.md#a7-当前证据与限制)。

本次选择只限定 BGM 的后端响度与音色，不禁止依据原版模型修复 PCM 默认值。完整 PCM／混音波形、严格 PIC／绝对输入时延、其它原版场景与全部重复 cue仍未验证；这些事实继续存档，不以新增音色测量无限延后当前 A7 交付，也不改写成已通过。后续进入 R1 发布准备及其必需的阶段审查；新玩法／场景随对应工作包补证。

### A8：宿主与设备生命周期（P2，配合 R1）

当前 Windows 受支持的本地降级与资源生命周期范围已提交 `ec8e699`。A8 从 `8fdfb2f` 独立提取，不包含本次 A7 的 XMIDI 调度、原版驱动模型、默认 PCM 增益或命名发布。候选与定向测试结果见[阶段记录](stage-log.md#a8宿主与设备生命周期)。

- [x] 固化后台运行、关闭窗口、设备打开失败和 SDL 本地断连通知后的降级／销毁行为；失焦不新增暂停策略，也不新增设备选择 UI。
- [x] 保持音频不可用时逻辑可运行，source 只 retire 一次；两个 bus 的最新配置不再占用 PLAY／STOP／master 的 SPSC 队列。Open／Resume／Put 的生命周期首错可只读查询，Put 失败同时使当前 AppMix 取证失败，不关闭、重放或自动重开 source。
- [x] 在 Windows 实际 WASAPI 流上验证打开、source 替换、两 bus 配置恢复、stop／retire／close，以及本进程原 SDL removed→Zombie 路径。正式切曲、活动 Load、后台与关窗沿用已列共同 owner 的实际来源，不把 API probe 的 source 替换称为正式 Game Load。

Open／Resume 失败时，允许 null 的对象保持本轮 discard-null 到销毁，不承诺自动恢复或播放位置恢复。成功打开后的默认设备管理交给 SDL；本地断连通知后的 Zombie 继续消费 callback，避免把正常 SDL 断连等同于队列永远停滞。callback 不做文件 I/O、分配、关闭设备或无界等待。

已有声音／资源记录和新交付候选的验证分开登记。设备 probe 为 `SDL_INIT_AUDIO`、48 kHz 双声道 F32、实际 render 后向设备提交等长零；本地断连一轮有 151 次 callback／Put／成功提交、三个 source 各 retire 一次，owner／ref 与正常 Job 清理均闭合。它证明 pre-OS 处理和进程内 SDL 生命周期，不证明扬声器输出。可复现入口见[工具速查](../tools/README.md#a8-音频生命周期验证)。

真实前台→失焦事件、物理拔插、WASAPI `AUDCLNT_E_DEVICE_INVALIDATED` 恢复成功、实际默认路由迁移均未单独实测；稳定后台已有实际证据。Linux／macOS 本轮未执行，私有 SDL 内部 ABI probe 只支持已固定的 Windows x64 Debug。后端响度／音色按 A7 的用户选择保留；完整波形和绝对时序仍未验，不能由 A8 的本地生命周期结果替代。

## 3. 验收标准

- 无音频设备时游戏仍可启动、测试和完成快速入口；回调无文件 I/O、动态分配或非有界等待。
- PCM 的索引、时长、循环次数及触发 phase 与 DOSBox 一致；XMIDI 的 tempo、loop、program/control change 和打击乐通过离线测试。
- 后端实现、场景接入和原版保真分别登记。未取得对应 DOSBox 对照，不能标记该场景保真通过，也不把已实现的播放器重新列为待选型。
- 原版资源继续只读加载，不提交 capture、渲染、转换产物或原版文件。
