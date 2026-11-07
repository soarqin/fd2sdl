# 许可与第三方声明

项目自有代码采用根目录的 [MIT License](LICENSE)。2026-11-07 用户明确选择 MIT；本许可不替换第三方文件的原许可，也不授予原版游戏、素材、机器码或反编译证据所涉原作品的权利。原版数据不随发布包分发。

## 直接使用的组件

| 组件 | 来源与版权 | 许可及保留材料 |
|---|---|---|
| SDL3 | libsdl-org/SDL；本轮固定源码 commit `2813f2630904ea29806fcfaeeb3287ed555e42e2`，版权见实际 SDK | zlib；[原文](src/third_party/LICENSE.SDL3)及其包含组件的声明一并保留。发布清单记录实际版本、静态／动态目标和摘要 |
| Nuked OPL3 1.8 | Copyright (C) 2013-2020 Nuke.YKT；经 libADLMIDI commit `9760df4e0acfe27818dee38e786f3b1fea8c8e7f` 引入 | `LGPL-2.1-or-later`；[原文](src/third_party/LICENSE.nukedopl3)，原始源文件头保留 |
| OPL AIL model | Copyright (c) 2025-2026 Vitaly Novichkov | MIT；[原文](src/third_party/LICENSE.opl-models-mit)与源文件头保留 |

历史保留的 BW MIDI Sequencer 适用 MIT。未进入当前运行路径的 XMIDI converter 原头允许 `LGPL-2.0-or-later`；本项目保留原头，分发时选择其中允许的 LGPL 2.1，并附 [LGPL 2.1 原文](src/third_party/midiseq/LICENSE.xmi-lgpl-2.1)。详细来源见[第三方音乐组件](src/third_party/README.md)。不把历史组件重新声明为本项目 MIT 代码。

## SDL 包含组件

实际 SDK 的公共头、原版权头和配置文件均随重建材料保留。SDL 的部分包含组件还有独立条件：

| 组件 | 本项目保留的声明 |
|---|---|
| HIDAPI | 选择上游允许的原始宽松许可；[选择说明](src/third_party/LICENSE.SDL3-HIDAPI)、[原文](src/third_party/LICENSE.SDL3-HIDAPI-original) |
| yuv2rgb | BSD-3-Clause；[版权、条款与免责原文](src/third_party/LICENSE.SDL3-yuv2rgb)，含二进制分发声明 |
| Sun libm | [原版权及许可声明](src/third_party/LICENSE.SDL3-libm)，不移除实际 SDK／源码头中的声明 |
| Khronos 平台头 | [MIT 式原声明](src/third_party/LICENSE.SDL3-Khronos)；带 Apache-2.0 标记的 EGL／GLES／Vulkan 头另附 [Apache License 2.0](src/third_party/LICENSE.Apache-2.0) |
| OpenVR 头 | [BSD-3-Clause 原声明](src/third_party/LICENSE.SDL3-OpenVR)；实际启用状态以 SDK 构建配置为准 |

这些组件的版权、许可及免责条款继续生效。完整 SDK 中的文件头保持原样；上游没有提供独立 `NOTICE` 的组件，不编造上游 `NOTICE`。

## 静态链接与可重建材料

MIT 不限制用户修改自有代码，也不禁止为调试修改而逆向。Nuked OPL3 仍适用 LGPL 2.1 或更新版本；选择 MIT 不消除 LGPL 对包含该库的可执行文件的分发要求。

Windows 静态发布采用 LGPL 2.1 第 6(a) 节的源码交付方式：随程序提供实际使用的完整应用构建源码、Nuked 源码、所需的匹配 SDL 开发 SDK、原许可和重建脚本，使接收者可以修改 Nuked 并生成包含该修改的程序。不以一份许可文本、源码链接或未验证的重建承诺代替这些材料。源码构建不需要原版游戏素材；运行游戏时由用户自行提供合法取得的数据。

发布清单和重建验证应覆盖实际交付文件；不得把 Debug SDK、另一份 SDL 库或开发工作区路径当成 Release 包的匹配重建材料。系统编译器与 Windows SDK 由接收者自行安装，所需工具链和 VC++ 运行库前置写入发布说明。

依据为 [MIT 原文](https://opensource.org/license/mit)、[LGPL 2.1 原文第 5、6 节](https://spdx.org/licenses/LGPL-2.1-or-later.html)、[SDL 官方许可说明](https://www.libsdl.org/license.php)及仓库保留的各上游原文。GNU 原站本次读取超时，许可条文通过 SPDX 与本地原文核对；会话未提供 `ketch`／Pi Web Search，外部检索使用 Cindy Web Search 和官方页面读取。
