# 第三方音乐组件

本目录保留 BGM 运行组件及历史事件转换组件，不包含完整 libADLMIDI core。当前运行路径只引用 OPL AIL model 和 Nuked OPL3；历史源码停止引用后仍保留来源与许可证。

| 组件 | 上游版本 | 许可证 | 用途 |
|------|----------|--------|------|
| BW MIDI Sequencer | libADLMIDI commit `9760df4e0acfe27818dee38e786f3b1fea8c8e7f` | MIT；见 `midiseq/LICENSE.txt` | 历史事件解析与时序路径；当前 BGM 不再引用 |
| XMIDI converter | 同上，`midiseq/impl/cvt_xmi2mid.hpp` | LGPL-2.1-or-later；见文件头和 `midiseq/LICENSE.xmi-lgpl-2.1` | 历史 XMIDI→MIDI 转换路径；当前 BGM 不再引用 |
| Nuked OPL3 | 同上，Nuked OPL3 1.8 | LGPL-2.1；见源文件头和 `LICENSE.nukedopl3` | 生成 OPL PCM |
| OPL AIL model | 同上，`model_ail.c` | MIT；见源文件头和 `LICENSE.opl-models-mit` | 复现 AIL 频率与音量量化公式 |

`src/bgm.cpp` 直接按原 EVNT 和32个 duration槽推进，读取 `SAMPLE.AD`，按已确认的 SBPRO2.MDI 双算子分支分配20个逻辑槽／18个硬件channel，再驱动 OPL AIL model 与 Nuked OPL3。转换后的 MIDI note-off 不能完整保留原槽顺序、满槽和同 tick EOT／fade，因此局部替换事件调度，不重选 OPL 合成器。Windows／Linux 沿用项目 CMake 及相同许可；本轮仅在 Windows 执行，Linux 未验。原版资源仍只读加载，不复制到本目录。

2026-11-07 用户选择保留 Nuked 原始响度与音色，驱动／解码库造成的音色差异不作本阶段门，不增加补偿增益或新的音色测量。该选择不改变第三方许可义务，也不等于完整波形或严格硬件时序已经相同；具体证据范围见[音频计划](../../docs/plans/audio.md#a7保真与场景对照p2)。

外部音色评估脚本 `tools/render_fd2_opl_prototype.sh` 使用 Linux／Bash 和上游完整源码，独立于运行时；其平台条件见[工具速查](../../docs/tools/README.md#离线音频对照)。构建与打包在两平台都必须保留以上许可证，不能因静态链接而省略许可义务。
