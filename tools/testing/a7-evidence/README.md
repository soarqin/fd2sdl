# A7 历史实机采集源码

本目录保留 2026-11-07 已执行的两套 Windows 本机采集源码，供复核原证据的自动化来源。共 11 个 Python 文件、1720 行、100871 字节。源码按原字节保存；同名辅助模块保留两份，不合并或重构。

本目录的属性规则保留原有 CRLF 和文件末尾空行，防止换行规范化改变已执行脚本的摘要；生产代码的空白规则不变。

**这是依赖历史 sealed 输入的档案，不是当前程序的可配置运行入口，也未注册到永久 Suite。不能把脚本中的程序或源码路径换成当前二进制后沿用旧通过结论。** R1 另补永久可配置入口。

## 来源与主脚本

| 归档目录 | 真实来源目录 | 主脚本 |
|---|---|---|
| `episodes-c3/` | `C:/Users/Soar/source/repos/fd2sdl-work/a7-three-scenes-gui-prep-20261107-c3` | `run_audio_scenes.py` |
| `sfx-reenable-c3/` | `C:/Users/Soar/source/repos/fd2sdl-work/a7-sfx-reenable-gui-prep-20261107-c3` | `run_sfx_reenable.py` |

`episodes-c3` 依次执行 SDL options-only、SDL load-only，以及两端的 opening 18→11、一次 END 19→12→19。原版 options／Load 引用先前已封证据，不在该轮重跑。`native_reference.py` 保留旧报告及独立 QA 的路径和 SHA。

`sfx-reenable-c3` 保留新的 00→01 业务与 Save 检查，以及 SDL 窄音频窗口。`prior_original_audio.py` 引用旧 C1 已闭合原版录音；新原版端只补业务和 Save。它不是同轮原版／SDL 音频配对。

两套脚本中的 C3／C4 manifest、程序、driver、fixture、旧报告、WAV 和 QA 等身份检查均保留。执行仍需要原本的本机 sealed 输入；这些输入不随本目录提供。本目录不含测试、临时启动器、commands、SEAL、原版资源、存档、截图、录音或报告。

## 原证据定位

以下是历史定位信息，不是仓库随附文件或当前重跑命令。

- 四 episode 原报告：`C:/Users/Soar/source/repos/fd2sdl-work/a7-three-scenes-gui-prep-20261107-c3/actual-four-episodes-c3-c3/comparison.json`；SHA-256 `0aff66469d97226b7801bffa6a3a624e3e3dfac92a9a06f2d866ba1abf5b7f18`。
- 四 episode 独立 QA：`C:/Users/Soar/source/repos/fd2sdl-work/a7-four-episodes-actual-readonly-qa-20261107-c1/receipt.json`；SHA-256 `1c1a50000b8d5518269db022a636e19965b13d0c05fa23abd305f4c73348ea59`。
- SFX 窄场原报告：`C:/Users/Soar/source/repos/fd2sdl-work/a7-sfx-reenable-gui-prep-20261107-c3/actual-sfx-reenable-c4-c3/comparison.json`；SHA-256 `f8aa6d4a4f83e5ab7467c9666ba10d7724d6048082c07b8b53b2d20e08c504e6`。
- SFX 窄场独立 QA：`C:/Users/Soar/source/repos/fd2sdl-work/a7-sfx-reenable-actual-readonly-20261107-c3/receipt.json`；SHA-256 `380edbb5bfd2d57b791e6caca1d72fedeff89d96f84e0c9d8041c01378353695`。

旧 FAILED、跨 Job 复用及有限声音判据继续按原报告登记。采集成功不扩为同一播放相位、完整波形、严格 PIC 时序或扬声器输出相等。

## 源码字节清单

`SHA256SUMS` 只覆盖下面 11 个运行源码。部分原件使用 CRLF，目录内 `.gitattributes` 对 Python 关闭文本转换，以保留原字节；清单自身固定 LF。

| 归档文件 | 字节 | SHA-256 |
|---|---:|---|
| `episodes-c3/run_audio_scenes.py` | 5712 | `ed46b14dffc6ce64f0ee33a203ab0feff437f4984995c607fedcc87b0ef4161c` |
| `episodes-c3/options_load.py` | 24434 | `e63e06bad1e158ccd873f5813ce508795a1200fda505e1b9293c3fa6d2c53f1d` |
| `episodes-c3/passive_observer.py` | 2587 | `cadb32dcee64c0df9b39d52ac093921287cabd72788c24239f1879cbc52cc7aa` |
| `episodes-c3/short_scenes.py` | 6141 | `ea98e93c59d3652936d67b368d9c06b5cb52d571b1886c62bf4d9845c6e15e84` |
| `episodes-c3/native_reference.py` | 8173 | `eb73efdf3298b24e6672c94836eb60e59b36f1e2cc2683824e4dc50ccf5bdf2d` |
| `sfx-reenable-c3/run_sfx_reenable.py` | 3945 | `ec1c7f0106321a6af13b1e2dc3994a9cb24054302e20f35e71a316d896eb3dc4` |
| `sfx-reenable-c3/options_load.py` | 17922 | `8f0d87a8cb62ebbbc743ca4d3ba0c6267fc21adf43a0b4be48f5ec69982f78dc` |
| `sfx-reenable-c3/passive_observer.py` | 2587 | `cadb32dcee64c0df9b39d52ac093921287cabd72788c24239f1879cbc52cc7aa` |
| `sfx-reenable-c3/reenable_case.py` | 15347 | `0e2407698955366b9e56a47587ecf9e17ba7befa2a7e04aa7a2a860defa4d7c2` |
| `sfx-reenable-c3/save_confirmation.py` | 10251 | `7c2103ac29b0c3432a8d3bf52e0bb3c0aa7d553f369ebe7356fb7c468f1ec2fd` |
| `sfx-reenable-c3/prior_original_audio.py` | 3772 | `7d95c4b8e9375c9a5a816744a3eedf97256db9ffc9356f8412fab571d7134a6a` |

归档时只解析 AST、比较字节和校验 Git clean-filter 后的 blob；未执行脚本、dry、图形、录音、设备或显示 API。
