# 音频状态与后续实施计划

## 1. 当前基线

2026-11-04 的代码基线 `e9a2d81` 已有统一音频核心、PCM、现用 XMIDI/AIL/Nuked 后端，以及标题和新游戏过场接入。**A0～A4 不再是待重做的前置工作。** 跨系统顺序以[总计划](development.md#4-新优先级与依赖)为准；下一步是 P1 的 A5，随后为 P2 的 A6～A8。

| 工作包 | 已有能力 | 尚未覆盖 |
|---|---|---|
| A0：格式与调用链 | PCM 默认参数、固定／动态 bank；XMIDI、stage 曲目表及播放／淡变入口 | 其余场景映射、全局默认音量 |
| A1：音频框架 | SDL3 device／stream、music/sfx bus、有界队列、retirement、null/offline 与错误降级 | 宿主和设备生命周期扩展归 A8 |
| A2：PCM | 24 voice、U8 mono、rate conversion、replace／stop；标题、对白、脚步、菜单与已确认战场效果 cue | FIGANI 动态 bank 归 A6 |
| A3：XMIDI/OPL | `bgm.cpp` 直接读取 `SAMPLE.AD`，现用组件化后端；播放、循环、替换和销毁测试 | 全场景与逐样本保真归 A7，不重做选型 |
| A4：场景接入 | 标题 track 18、新游戏 track 11 及原版淡变；Load 应用 music/sfx bus 配置 | 战场切曲与 sequence 恢复归 A5，bus 恢复不等于完整音频恢复 |

历史记录包含 Linux 原生、SDL dummy 和 MinGW 交叉构建；Windows 实机音频、macOS 和后续改动的平台回归仍待对应环境，不能由交叉构建替代。

### 依据与实现边界

- [音频调用与对照依据](../reverse-engineering/audio.md)：bank 用途、触发帧、曲目调用、PCM 捕获与外部原型结果；不把局部相关系数当全场景保真。
- [数据格式](../formats/data-formats.md)：XMIDI、PCM bank、AIL 音色表和 stage 曲目表布局。
- [session 设计](../systems/battle-session.md)：BGM owner、Load、离场、演出与资源生命周期。
- 实际 API：`src/audio.h`、`audio_pcm.h`、`bgm.h`、`field_audio.h`；不另维护建议接口，不引入 SDL3_mixer 或临时播放器。
- [第三方清单](../../src/third_party/README.md)及文件头决定组件许可；完整 libADLMIDI core 只用于外部原型，不是运行依赖。顶层 LICENSE 与 LGPL 发布义务仍是 R1 硬门。

## 2. 后续工作包

### A5：战场曲目与恢复边界（P1）

依赖 B0 和现有 A4，可与玩家动作独立推进；不是 FIGANI 的后置任务。

- [ ] 在正式战场入口播放对应 stage primary；按 `field_turn_cycle_run @0x1a30b` 已确认的边界淡出、进入 alternate，并在新回合恢复 primary。stage 0 的 track 1/4 有静态依据，不按听感另选曲。
- [ ] 在现有应用／场景编排处连接 BGM 引用或最小曲目请求；播放器仍由应用层持有，不让 `field_game`、绘制和音频回调共同维护当前曲目。
- [ ] 明确标题 → 新游戏、Load → 战场、离场 → 标题、胜败 → 结果／下一关的替换与淡出；同曲不重启。恢复配置不应重置单位、phase 或 RNG。
- [ ] 以 null/offline 后端断言请求顺序、loop count、淡变和 bus；真实输入检查 Load 与返回标题。人工对照对应场景时序，不能仅测试 PCM 有能量。

### A6：FIGANI 动态音效 bank（P2）

依赖 B6 的 metadata／帧序证据，不按全局变量名猜固定资源号。

- [ ] 确认动态 selector、样本索引、handle replace／stop、循环和触发帧；复用现有 PCM 后端。
- [ ] 明确演出跳过、换 bank、Load 和离场时的 source retirement；bank 字节活到最后一个引用 source retire，音频回调不做文件 I/O 或释放。
- [ ] 验证同一动作无论完整播放、跳过、静音或无设备，逻辑和 RNG 均一致；失效 selector／坏样本不能伪装成成功解析。

### A7：保真与场景对照（P2）

- [ ] 优先复核已可达的标题、过场、stage 0 primary/alternate，再随 B6 扩展战斗；不一次扩充所有曲目语义。
- [ ] PCM 分别比较索引、时长、循环、replace 和触发帧；已有 SFX 5/8 波形结果只覆盖相应样本。继续确认全局默认音量。
- [ ] XMIDI/OPL 分别比较 tempo、loop、program/control change、打击乐、voice 分配和响度；保留原型指纹与现用后端结果的区别，不把谱相似度等同逐样本一致。
- [ ] 由开发者手工操作 DOSBox 并提供已确认场景的 capture／操作记录；所有原版捕获和转换产物只留本地。

### A8：宿主与设备生命周期（P2，配合 R1）

- [ ] 固化失焦、关闭窗口、设备打开失败和设备丢失时的降级／销毁行为；不为该工作新增暂停或设备选择 UI。
- [ ] 保持音频不可用时逻辑可运行，source 只 retire 一次，不悬空、不占满队列；主线程与 callback 的资源边界不变。
- [ ] 在实际 Windows/macOS 环境验证打开、播放、切曲、Load、关闭与错误路径；交叉构建和 SDL dummy 不替代设备实测。

## 3. 验收标准

- 无音频设备时游戏仍可启动、测试和完成快速入口；回调无文件 I/O、动态分配或非有界等待。
- PCM 的索引、时长、循环次数及触发 phase 与 DOSBox 一致；XMIDI 的 tempo、loop、program/control change 和打击乐通过离线测试。
- 后端实现、场景接入和原版保真分别登记。未取得对应 DOSBox 对照，不能标记该场景保真通过，也不把已实现的播放器重新列为待选型。
- 原版资源继续只读加载，不提交 capture、渲染、转换产物或原版文件。
