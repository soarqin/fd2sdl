# DOS-CLEAN：保留游戏行为，删除冗余模拟

2026-11-18，基线为已集成本地 master 的 F5 `b6462edf`。本阶段按用户要求独立交付，位于 F5 与 F6 之间。当前验收目标是 Windows 正常游戏的操作、结果、存档和可见可听表现；`EX-AUDIO-NUKED` 继续披露。纯硬件／驱动内部形式不再要求，也不转到 F6。

本轮修改 35 个代码／构建／测试文件，新增397行、删除962行，净减565行；其中14个产品C／H文件净减358行。这只是实际删改规模，不是功能覆盖率。阶段提交、两树物理来源、主线构建／执行及用户档守卫只认两树 `build/dos-clean/subject-delivery-20261118/local-integration.json`。当前下一动作只查[统一计划](development.md)。

## 1. 已落地机制与实际消费者

| 机制／文件 | 实际消费者及游戏影响 | 删除／最小化 | 保留及验证依据 |
|---|---|---|---|
| `ail_timer.c/h`、`bios_clock.c/h` | Main先安装BGM，再安装DIG；等待、RNG和Audio共用时基 | 16槽缩为MDI／DIG两服务及必要游戏tick累计；删4字节句柄形式、任意频率／状态接口、pending／busy／defer、EOI／program hooks及未使用换算API | 保共同5000us基步、8333／5000／54925累计、安装清累计／重定、MDI→DIG→游戏tick、分数音频前缀。保原数值节奏，不改为独立精确120／200Hz |
| `bgm.cpp/h` | Main／Scene／Game的音乐请求；score、包、loop／fade和缓存决定内容／节奏 | 删除qualification、PC请求轨迹、first-gap／barrier、重复owner和未读TIMB／RBRN／VLQ计费投影 | `evnt && size`作为唯一已解码score事实。3B空资源保旧score并重开；首次无score安全空闲，后valid恢复。保512B消息、duration／voice／TIMB及chip借用／错误清理 |
| `sb_dma_provider.c/h`、`audio_sb.c/h` | 两个P声部共用ring及唯一Audio；块交接决定暂停、恢复和尾音 | 删除ACK／require_ack、CPU迟交付恢复、无正式消费者的便利／reset／DSP rate／filter控制；完成状态改称block_ready，块完成入口直接消费 | 保4096B ring／2048B half、原暂停请求／恢复、短尾单次任务及串号取消、已达前缀与错误锁定；不建立PIC或第二时钟 |
| `sb_channel.c/h`、`audio_original.h`、`audio.c` | 正式48k混音、采样／滤波／DC、Audio提交／关闭 | 删除一直为真的channel enabled、可变rate／filter及重复常量、无消费者的live SB gain、重复service_index／servicing_event；固定22050／48000及alpha38711 | 保Q13左右系数、预取／低通历史、1ms／48帧窗口、同窗最多两次pull、完整PCM、两P叠加和quiesce。游戏音效开关仍由DIG.enabled控制 |
| 普通测试与三个Python数值模型 | 独立内容、数学输出、owner／取消／失败回归 | 删除纯槽／EOI／嵌套／ACK／live硬件gain等形式族及原始地址资格断言；删除手写“未读却已块完成”的迟交付族 | 留12采样数学族、两buffer事件族、正常110ms双声部全PCM；给定短尾→真实quiesce→越旧due无读／无Put及单退休。真实pause／resume另走块消费链 |
| CMake外围 | 默认产品／测试注册 | 删除从未创建target的scanout条件块；更新过期SOURCE注释；新expected目录为`generated/native_audio_game` | 保旧expected不覆盖；真实输出首错／wait／owner检查继续注册。没有新增配置开关或通用框架 |

正式Main没有调用删去的通用槽／寄存器／DSP配置。所有产品时钟消费者只通过现有owner／服务接口；同刻旧前缀先提交，再执行音乐／音效，最后交游戏tick。纯timer重复安装失败在写前，高层安装仍先完成`<=now`已达前缀，不能把其失败说成整体回滚。

## 2. 按真实行为保留的相似机制

输入的15有效槽FIFO、BIOS键值／repeat及修饰键由Main和Prepare真实读取，继续保留。工作页、线性写入、palette、临时页／snapshot保护游戏像素；SAV原布局／保留字节／校验／取消失败保护存档兼容；共享RNG与冷事件图预算保护结果及终止性。它们不是因名称包含BIOS／VGA／source就应删除的模拟。

没有发现三条清理范围外仍链接到默认产品的CPU预算、DPMI、PM／RM、原SS／SP／IF、IRQ控制器、3DA或扫描线模拟。保留行为对照工具、IDA发布物、原媒体／失败与checkpoint，不重新生成旧证据。`f1_source_contract`中的旧loader／selector工具测试保留为取证工具安全合同；通过它们不构成产品硬件等价门，不删除整组真正保护玩法的F1测试。

外围只读盘点：`F:/fd2sdl-evidence/f5-dos-clean-perimeter-peer-20261118-01/report.md`，SHA256 `01a4f2fd7c6956d006d0eb0c5186953186a10bb682036eeb70256f2ca85bfbb4`。冷表／事件分支未修改，没有新VM、第二热owner或新增原版逆向。

## 3. 实际检查与独立依据

| 层次／收据 | 本轮实际结果 | 资格边界 |
|---|---|---|
| 静态消费者与交叉复审 | clock／BGM／Audio三个共享机制及外围调用闭包；差分与严格反向patch、末次来源守卫 | 只读复审不是实际运行；报告分别保各自候选与Root后续修正 |
| 独立数学模型 | 清理前后110ms的5280双声道帧完全相同，SHA256 `2b2c2fca163a049d6f06eacf04f17c3154e2ff6b8871a1c080e94f85ed0016fb` | Python整数／Fraction及既有源算式，不读C输出生成expected，不声称新原版录音 |
| `dos-clean-checks-20261118-01` | 21目标构建成功；21项CTest中20过，audio_transport残留DIG服务`==0`旧断言失败；源与用户档守卫不变 | 初次FAIL保原件，不改生产接口回旧编号 |
| `dos-clean-checks-20261118-02` | 修旧断言、测试替身ID及固定采样率上界；App及四目标重构，audio_transport／sb_channel／audio_sb_bridge／sb_dma_provider四项通过 | 没重复其余已通过检查；采样内容期望未改变 |
| `dos-clean-checks-20261118-03..05` | 新quiesce短尾族先误期望关闭后sync成功，03失败；04局部补丁误匹配相邻正例再失败；05正确限定后仅audio_original整项通过 | 关闭后sync应拒绝；同owner clock实际越旧due确认无尾任务／无Put。生产终态合同未放宽 |
| 原版／正常窗口 | 复用F5原G1／G2、后继SDL-only及正常LEAVE的录音／WGC／清理 | 本轮未开窗口／录音。删改不动score、包、原stop点、cue、数据／SAV／输入；数值及真实consumer回归支持有限依赖继承，不冒称新PE原生通过 |

以上本地收据均在 `F:/fd2sdl-evidence/` 对应目录。clock实现前后及说明在`dos-clean-clock-20261118-01`，BGM在`f5-bgm-dos-clean-20261118-01`，Audio在`dos-clean-audio-20261118-01`。clock peer在`f5-clock-dos-clean-peer-20261118-01`；Audio peer `dos-clean-audio-peer-20261118-01/report.md` SHA256 `9f25165288bc8542fb4dc6d803c4137a811c878286657245c4ecfa94f26c937f`与Root最后修改按其独立候选分别记录。

独立110ms保的是普通采样／双声部数学路径。给定PAUSED剩1／2字节、部分较晚buffer交接等callee回归保护短尾消费／取消，不宣称Main自然产生这些前态，也不要求CPU交付时间一致。真实音乐／动作cue及音画同步仍由F6正常联合说明，不用内部state字段数签完整游戏。

## 4. 矩阵与F6边界

本阶段关联原A10、A13、A17—A19和A21；22个稳定ID不变。A10保真实输入合同；A13保等待／节奏／RNG读点；A17—A19删除冗余但保音频内容、采样、暂停恢复与关闭行为。纯CPU／IRQ／PIT／PIC／驱动ABI／原栈／全内存形式已退出产品门，不进入F6待办。

F6仍须核QK真实掉落／经验、AI继续和结果去向，F3后期状态／败北重开／存取后续，F4正常目标／MP／禁Save／END→Save及未解释像素，以及正常输入／共享RNG／动画音乐节奏和cue联合。无skip开场Title45秒未知保A13；不以加Esc的后继成功回签。物理扬声器／真实拔插未验及Nuked例外继续披露。

完成清理阶段不等于A01—A22全部通过。最终声明只在必要游戏表现、有效域和独立证据闭合后给出；硬件形式研究退出不能掩盖真实功能未知。清理整阶段提交／本地集成／汇报后直接继续F6，不等待支持申诉，不自动开新功能阶段。
