# F5：音乐、音效与 Windows 音频生命周期

2026-11-18 后继实现说明：F5交付后独立[DOS-CLEAN](dos-simulation-cleanup.md)已移除两服务外的通用调度、BGM资格／地址诊断、ACK及未用DSP／重复采样配置；F5以下来源和运行身份保留。音乐内容／节奏、PCM、暂停恢复及关闭行为继续按真实消费者验收。

本阶段实现、实质审查和直接检查已完成；实际提交／本地集成及当前程序身份以两树 `build/f5/subject-delivery-20261118/local-integration.json` 为准。范围为 A17—A19、A22；Windows 为最终平台，保留并披露 `EX-AUDIO-NUKED`。2026-11-18 用户明确以游戏表现一致为目标，纯 DOS 硬件／驱动形式等价不再是本阶段或 F6 的必需门。当前执行顺序仅由 [总计划](development.md)维护。

## 1. 保留的实际修复

| 项目 | 正常游戏影响 | 现有独立依据与边界 |
|---|---|---|
| 两处停曲 | `field_play.c::play_system_confirmation_text` 的 leave 文本完成后、200ms前停曲；`scene.opening.031/bgm_before_move_25` 在 text9 后、script98前停曲 | 原19df7@1a2e7、3231b@327bb及请求顺序回归；不是新PCM漏stop，也不以请求替身宣称录音通过 |
| 音乐调度与状态 | 单一sequence、常驻chip，曲目／事件、队列、控制、循环、tempo／beat、fade、开关与同步起停保持原含义 | 15首实际曲目／独立EVNT向量、原512B包／控制字段、正常accepted502的104组写序；内容证据与自然游戏录音分账 |
| PCM／双声部 | 去原先持续线性插值差异，保正常U8采样、速率／相位、gain／pan、双P混合、stop／replace、夹幅和尾音 | 原资源／30函数来源、独立公式、26bank／111绑定／78唯一payload；226历史／2013packet为有限参数证据，不是111个自然cue均已录音 |
| 传输与关闭 | 主线程同Audio时点提交，源单次退休、首错、null输出及有界队列，切景／取消／失败／终态不留活动source或晚Put | 正式consumer直接回归；真实WASAPI／同进程removed及正常SFX双端另有原收据；不冒物理拔插或用户扬声器验证 |
| 采集回译 | v3无损保存S16/32768载体，避免旧采集器制造一LSB误差 | 65536个S16完整回译及worker／Put顺序检查；不借此证明任意F32与OS格式转换等值 |

F4基线为 `3bdd02784e19dccc04154bdc94f5bebd64ce5efe`。裁减前最后App为 `27733078d4e3b2b785895f4cc413c9b868375f1b25baf6a2c7c59f4b71d60a35`；这是旧候选身份，不是裁减后完成身份。

## 2. 复用的运行证据

具体来源、日志、哈希和失败原件统一保留在 [历史证据记录](f5-source-research-history.md)，不重新制造同样窗口或将不同候选相加为一次全绿。

- 两处stop的源绑定请求回归：`build/f5/bgm-request-lifecycle-review02.json`，SHA256 `147ed6260368534a490e1351cf1a98cc236a056956ec9bd31a57a9a3c385c05e`。正常／错误／取消前缀范围见原收据。
- 正常原版PCM窗口：32768个双声道帧与同次原WAV逐字节对应；同输入的实际C后级SRC已核。它只覆盖该样本／窗口，不代表全游戏或整设备波形。
- 原／SDL正常SFX重新开启代表：正式START、重新开启、完整22987B Save、2652点／13候选的有限样本身份及清理。SDL当次37445成功Put、首错／backlog0，查询峰18432B；可听路由、其它cue、全曲或绝对硬件时序未由此证明。
- `F:/fd2sdl-evidence/f5-audio-public-owned-20261117-01/receipt.json`：真实WASAPI，296次Put、单次退休与清理通过。`f5-audio-local-disconnect-owned-20261117-01`：同进程SDL removed、队列归零、继续提交及清理通过；不是物理拔插／真实路由切换。
- 全曲库、PCM绑定、采样／混合／增益／量化等独立参数／源内容检查复用。只因新二进制或删除未调用研究路径，不重放全部历史GUI。

## 3. 当前裁减与不再要求的研究

裁减前386个修改／新增文件已完整复制到 `F:/fd2sdl-evidence/f5-player-scope-before-20261118-01/snapshot/`，manifest SHA256 `b461698a38acddabd91344a85441bad20de11b30e1a90978743f8c219f70f40d`。此归档用于保存已有候选，不是新的语义或运行通过证据。

| 原要求 | 当前处理 | 若存在真实表现影响 |
|---|---|---|
| SOURCE工厂／DOS文件与DPMI／堆／寄存器／原ABI运输 | 未被Main使用；移出默认产品和必做测试，原件归档 | 直接测资源读取、错误处理、音乐内容及owner |
| CPU逐头成本、IRQ／PIC／PIT、SS预算、驱动内部入口／返回形式 | 停止扩展；旧严格DRO时刻RED保持历史失败，标为不再要求 | 测音乐节奏、cue顺序／间隔、动画等待及音画同步 |
| 03h／13h硬件日历、物理逐扫描线采样及真实首phase | 移出产品门，不转交F6 | 测完整游戏画面、palette相位及动画呈现；SDL上传／输出首错仍保 |
| 全对象BSS／完整内存或设备输出字节相同 | 既有来源结果保留；不再作为表现层完成前置 | 正常被消费的状态、样本、存档和结果继续逐项核实 |
| 新建未编译的source_memory四文件 | 已保存，停止实现并从源码树移出 | 当前没有Main消费者，不新增替代产品功能 |

仍保最小实际音乐服务与音频前缀调度，不把有用的共享时钟、PCM整数核或错误传播一起删除。研究源／完整转储／媒体不分发；未完成研究不标通过。

## 4. 裁减后的实际检查

当前主线仍为 `3bdd027`，F5 尚未提交。新收据均在 `F:/fd2sdl-evidence/f5-player-scope-root-20261118-01/`，不覆盖旧结果：

| 本轮 | 实际结果 | 解释 |
|---|---|---|
| configure01 | 成功 | 默认CMake只保普通clock两C与PCM五C，不再生成逐头成本表／链接硬件研究 |
| audio-build01／audio-check01 | 14目标构建、14项CTest通过；来源／用户档守卫不变 | PCM、双声部、SRC／混合、传输／owner、开关及采集回译；没有原版／设备／GUI新运行 |
| clock-build01 | 10目标请求，前5目标成功；随后core旧BGM引用已移出的25个SOURCE符号而链接失败 | 原日志保留；不恢复研究库凑通过，待BGM同批裁减后继续真正受影响目标；没有执行未链接场景 |
| clock-check01 | 实际已链接的5项通过 | BIOS／数值AIL、C++链接、VGA等待及真实SDL输出失败传播；不算全部10项构建通过 |
| app-music-build01／check01 | 20目标构建成功；30项检查28过、2个Scene夹具失败 | 当前App为a25a599a；普通曲库／loop／fade／包／缓存与owner通过。失败原件未回签 |
| scene-cache-fix01及独立Scene修复 | 三项中cache／transition过；handoff再单项修复后通过1.77s | 保原低WORD／延迟期望；合成时间展开与漏替的movement wait纳入同一测试owner，未弱化产品clock/HOST/ERROR |
| native-candidate-build01 | App／driver／BGM目标构建及startup owner通过 | App `b2646d80caa4c1b10042b18a0c7171d65a8d667f4bc88438e949eefb14db5ced`；正常renderer先于writer、创建失败及无资源泄漏，不是原构造形式 |
| unavailable-current-app01 | 当前App无可用音频后端仍完成正式快速入口 | 显式绑定C目录新App；dummy video不算真实窗口，原件／用户档守卫不变 |

上述分轮不相加成一次全fresh。新完整App及对应音乐／场景路径已构建，所有初次失败保持原身份。独立复审与单项修复：`F:/fd2sdl-evidence/f5-scene-handoff-clock-fix-20261118-01/receipt.json`，SHA256 `ef443ce50ae96971eb819b349a99f0bbe3dd4d2a1d06b0ffae688c5780d5b879`。

裁减共移出141个无当前编译／include消费者的新增研究C/H／冷定义与77个纯研究工具／测试；四个未编source_memory文件另停止。均先与裁减前快照逐byte核同才从产品树移出。通用输出路径守卫因F6 QK工具真实依赖而保留，不按source命名机械删除。

旧Title18源内容／节奏复核仅读已有录制：3748映射写同序，3599 live写／455有写服务组；旧C−原DRO编码时刻范围约−2.750567..+1.272109ms，2209服务tick跨度原18409ms／旧C18410.272109ms。没有新增容差、当前候选PASS或把旧1ms端口失败称为听觉失败；当前音乐score／loop／fade及共享服务由独立普通回归核验。

## 5. 新正常 LEAVE 双端代表

`F:/fd2sdl-evidence/f5-audio-leave-native-20261118-01/comparison.json` SHA256 `5e63490c98e2c23f8f8781f4944f05ea6b3cf0b8f244947d1f3ae50d949412a3`。实际原版PID49016／SDL PID28776均完成 CONTINUE→SYSTEM→LEAVE415／YES→完整416→正常exit0，YES后没有再投游戏键，来源Save未变。SDL末条leave停曲请求、23428成功Put、首错0／丢块0，采样队列峰7864B；无晚Put／终态owner由独立回归分账。

两端完整录音均封包，SDL按v3在完整416可见后显式stop。因此只签本次有限音频窗、请求／操作顺序及正常退出，不签完整200ms／4秒fade或退出后声学静音。淡出原算式／事件序列由普通独立回归另核；没有更改capture协议求通过。

外层 `F:/fd2sdl-evidence/f5-audio-leave-outer-20261118-01/report.json` SHA256 `d45d8e4e97cae843f8e8444b83b82891ddfb4ffad3987b327275b9105c50b938`：起始OFF，按既有授权一次需求＋零位移恢复到ON，指针未变；内层strict ON／WGC通过，两层各同线程释放。前后电源方案一致、未修改计划／超时／混音器，来源和用户SAV守卫同。完整owned／Job原始清理结果保同轮日志，不以成功Put称扬声器已验证。

## 6. 空曲目重播与新证据

原 `G1→G2` 正常操作记录保留在 `F:/fd2sdl-evidence/f5-town-g-music-pair-20261118-01/report.json`，SHA256 `7147ab27c8774a97b142261ac8ea7e7639b012cec69d999ced68b89ade7f2e92`。原版七个业务键 `DOWN ENTER ENTER G LEFT G LEFT`、两次完整59904像素响应和30.261秒连续录音均取得；SDL当轮在未跳过开场的Title45秒门超时，未发送业务键／开始录音，整体FAIL不改签。它不是G2无声的运行证据，也不证明已发现启动逻辑错误；未跳过开场的正常时长／可达性留A13联合核对。

原WAV SHA256 `6f5f4d6b84e99808d9ebb3fc87a91341bded4cc92b2b0efbeb2a2f4bce8865f2` 中，G2重新展开与G1对应的多声部攻击、长音及后续乐句。原初始化失败保旧EVNT和start重置cursor的既有来源互证。旧SDL因内部owner资格门阻止后续音符，形成真实音乐差异。现对相同3B空资源族复用同player／整DAT中已解码score，从开头正常重播；不建第二sequence或曲谱副本，不复刻释放后堆内存。`COMPAT_REPLAY` 标表现兼容，历史内部owner缺口保留；首次无score仍不猜曲谱。

`g2-replay-build01` 实际构建App及六个音乐检查目标，7项直接CTest通过；新App SHA256 `01fea945df958532846520268a6b74701f2196429e2c9666e388fbae6937b723`。五个空条目、独立原score首服务17消息／47B指纹、stop／pedal顺序、同ID早返、首无score／坏资源／闭owner／后valid恢复均由普通回归核。五条目复用同原资源及共享机制，不称五次原版新实测。

只补SDL跟进，不重跑原端：`F:/fd2sdl-evidence/f5-town-g-music-sdl-native-20261118-01/report.json` SHA256 `a195b782f300c20fb5c61dab344b7013f5313a4e089cef78f65d68b5a2329c6b`。PID18572正常七键／两次完整LEFT及约12秒保持（实际11.9946264／12.0050498秒），连续1340304帧／48000Hz录音封包；42986成功Put、首错0／丢块0、采样队列峰8448B。标准有界开场Esc单列4次，不混入七个业务键，也不回签旧未跳过开场的失败。SDL录音只在成功Put后的应用混音点，送设备为静音，不冒扬声器或物理路由验收。

外层 `f5-town-g-music-sdl-outer-20261118-01/report.json` SHA256 `6fc2f0aebbc164d3d44f6b6d948b13f65556f031c5e5efc6f3e5249bb35c10bd`：活动桌面／strict ON和WGC通过，显示本来ON、没有唤醒或电源改动，两层同线程释放；所有owned正常清理，无强杀／残余，原件及用户SAV守卫不变。录音采集通过与下列乐曲表现离线结论分账，不重签旧pair。

命名发布已用现有隔离IDA库完成真实元数据同步，未新增硬件分析：`F:/fd2sdl-evidence/f5-player-audio-publication-20261118-01/publication-receipt.json` SHA256 `dfd61f6e9eba8512a815be667b0e8eba349344263e74eb120aefbf38b169c9f1`。1312函数、573登记、21项更名／22注释；边界／签名／flags／chunks／字节不变，C token仅依登记名映射变化，11项manifest及14交叉检查／1001规范入口通过。初次发布缺口保留为失败，不靠手改生成文件或只刷新SHA签语义。

离线合证已完成：`F:/fd2sdl-evidence/f5-g2-music-compare-20261118-01/report01.md` SHA256 `2d2d53f3fb3a5705c78a3745e9ee68987d126cc5f902150f3e65d1a06a4ddb01`，seal SHA256 `c8e843a9ee6282f2b7c7d25d108679553cb61c60825c9e924d5c0ed289ab3c3d`。原／SDL各G1→G2在实际输入区间内分别平移14.97／13.96秒重现，未拉伸时间或拟合gain；四段多声部开头／后续乐句与独立score长音变化对应。原短节奏83.333..91.667ms网格的两端局部分析峰均87.5ms（分析格2.5ms），只签有限粗节奏，不拿相似率作新PASS容差。

本次给定Music1 Town及已有合法score可签G2重播／多乐句／粗节奏；不签样本级起音、逐事件全WAV、整loop、物理扬声器或该受控档的自然生产。完整loop／fade／事件及其它有效score由既有独立资源／源参数与实际C分账，不把本段录音夸为全部曲库重录。旧未跳过开场的Title45秒FAIL继续留A13／F6正常启动核对，不归入Nuked例外。

显示失败后缀已按下节完成审查修复及实际定向检查；最终提交／本地集成身份以本页首列收据为准。

本阶段按共享语义登记分母，不按录音次数计算覆盖率：FDMUS20目录=15份有效score＋5份相同3B空条目；前者由全score事件／loop／fade／包／缓存回归和实际正常代表互证，后者由共享重播来源／七键录音及五参数回归组合。PCM为26bank／111绑定／78唯一payload，绑定参数与实际cue调用顺序分账；平台设备正常、无设备、removed及关闭各保实际候选。所有未观测的自然链不会因表存在自动变成通过，跨系统当前候选／正常时点联合仍由F6逐项说明。

## 7. 失败传播审查与阶段交付

实质审查另发现显示上传已失败后，有限copy／零tick帧仍可能继续发音效、读键、改嘴型／画面或推进回合。已在Scene／cin_present、Play、Prepare及service、Battle／Effect、Game两个march点、Main两个标题提交点接住同VGA／clock首错，先返ERROR再分类HOST。临时页／box沿原free出口，保已经发生的绘制／输入泵前缀；正常路径不新增等待、RNG、音效或状态owner。音频设备失败仍按原降级处理，不伪装成游戏时钟错误。

窄审查 `F:/fd2sdl-evidence/f5-player-output-consumer-review-20261118-01/report.md` SHA256 `938c168d73c56986329de039757b738828aa60a4132fde56ec4d9c5c9acbdcb1`；Root Main两门的独立peer SHA256 `ca475590b8b8be4886a3a8fbac78d2357a0aa5004e91e21f1db66801ce0f83dc`。它们都是静态／修改收据，实际C另列。后继夹具修正保Root新的文件身份，不修改原seal。

本轮检查仍在 `F:/fd2sdl-evidence/f5-player-scope-root-20261118-01/`：

| 收据 | 实际结果及修复 |
|---|---|
| output-prefix-build01 | App和前7个检查目标已构建；Effect测试局部struct误写成指针访问导致构建停止，无CTest执行。原失败保留 |
| output-prefix-build02 | 只补剩余6目标成功；23项实际注册检查20过3失败。Main新fixture多排早键、Scene新资源读取缺Root cwd、Prepare合成时间与共享clock不一致；不改生产判断求绿 |
| output-prefix-fixture-fix03 | 3个受影响测试目标重构；6项定向5过，Prepare另一个同scope排队后回拨到110ms的夹具失败保留。Main／Scene及三Prepare错误族已过 |
| output-prefix-prepare-fix04 | 仅Prepare健康目标重构／单项通过。排队先设110ms再正常递增，两个显式新故障scope与合成时间一起复位共享clock；原相位／倒计时／RNG和所有功能断言不变 |

生产语义在build01后未再变化；提交时只移除SB缓冲provider文件末尾多余空行，App SHA256 `f0d79404a8c09d12aec0c71ea146c2dbed397fd7c90de30fe35b47f839c63eca`。修复后必要路径分轮通过，不称23项一次全fresh；原G／LEAVE录音与该候选的关系只按依赖继承。音乐、PCM、Audio／BGM、共享clock及两处stop没有被这批错误后缀修改；新增pure getter只截断已失败返回，健康邻接与故障／同时HOST／队列／cue／回合哨兵核实其边界。Game新错误模式已注册 `clock_output_owner_game`，不是遗漏的CLI检查。

本阶段按现行表现目标交付共享音频机制、绑定参数、两处stop及G2实际差异、正常代表与失败／降级／清理；不是整个A01—A22或所有跨系统自然链的最终合签。A17—A19的当前候选／真实玩法分支、cue与输入RNG／FX联合留F6；未跳过开场的Title45秒差异单列A13。Nuked例外、物理扬声器／真实拔插未验及原U后的安全适配继续披露。纯硬件形式不再要求，不以这些未验形式制造新的产品阻塞。

## 8. F5 必需验证与完成门

1. 裁减后完整App及直接音频／场景consumer可构建。正常曲库、tempo／loop／fade／起停／开关、PCM采样／pitch／duration／volume／pan、双声部叠加与replace有独立源内容／原版表现依据及实际回归。新移除路径不改变上述业务写集。
2. 两处stop正常顺序与取消／失败前缀通过；设备失败、关闭及单owner／单次退休／无晚Put保持。null或失败不改变游戏输入、RNG及存档结果。
3. 依现有来源和正常代表核音乐／音效内容、节奏、触发次数／顺序、叠加／循环／淡变、切景及音画同步。新差异用正常游戏操作及对应画面／音频采集核实，只有真正新时点／反馈才追加代表；不以端口、IRQ或整设备逐字节相同替代它。
4. 比较条件与判据来自原内容、原版正常输出或重复观测；采集误差单列，不随意放宽容差。Nuked例外仅覆盖其音色／响度及可归因混合影响，不豁免PCM、节奏、事件或清理。
5. 阶段实质审查／修复精简后，同一候选直接检查、来源／原件守卫及本地集成闭合；矩阵、完成归档、stage-log同步。尚缺必需的游戏表现证据时仍不能签完整F5。

F5提交、集成并汇报后，先执行用户新增的独立DOS-CLEAN，清理已落地模拟及冗余并独立提交／集成／汇报，再进入F6。F6只接回跨系统的正常游戏联合门，见 [总计划](development.md#4-f6联合审查修复精简与最终验收)。纯内部硬件形式项已退出必做范围，不得在收尾重新作为阻塞。
