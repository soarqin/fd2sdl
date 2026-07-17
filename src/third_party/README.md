# 第三方音乐组件

本目录只保留 BGM 运行时需要的组件，不包含完整 libADLMIDI core。

| 组件 | 上游版本 | 许可证 | 用途 |
|------|----------|--------|------|
| BW MIDI Sequencer | libADLMIDI commit `9760df4e0acfe27818dee38e786f3b1fea8c8e7f` | MIT；见 `midiseq/LICENSE.txt` | MIDI／XMIDI 事件解析与时序调度 |
| XMIDI converter | 同上，`midiseq/impl/cvt_xmi2mid.hpp` | LGPL-2.1-or-later；见文件头和 `midiseq/LICENSE.xmi-lgpl-2.1` | 将 Miles XMIDI 事件转换为 sequencer 内部 MIDI 数据 |
| Nuked OPL3 | 同上，Nuked OPL3 1.8 | LGPL-2.1；见源文件头和 `LICENSE.nukedopl3` | 生成 OPL PCM |
| OPL AIL model | 同上，`model_ail.c` | MIT；见源文件头和 `LICENSE.opl-models-mit` | 复现 AIL 频率与音量量化公式 |

`src/bgm.cpp` 自行读取原版 `SAMPLE.AD` Miles AIL 音色表，并驱动以上组件。
原版资源仍从 `original_game/` 读取，不复制到本目录。
