# F5 历史证据与硬件研究记录

2026-11-18 口径更新：本文件保留当时的来源、候选、失败与局部结果。用户已明确以游戏表现一致为目标，纯 DOS 硬件、驱动内部形式、寄存器／周期／IRQ／PIT、原栈及完整设备字节相同不再是产品完成门。下文旧“必须／仍须／最终门”仅为历史要求；现行范围和必需缺口以 [F5 当前计划](f5-audio-device.md)、[总计划](development.md)及 [矩阵](full-alignment-audit.md)为准。原失败没有改签通过；真正影响游戏行为、存档或可见可听输出的事项仍须关闭。

# F5 非例外音频与设备合同

本阶段正在实施，尚未完成或提交。范围沿用[总计划](development.md#f5补齐非例外音频与windows设备合同)的 A17—A19、A22；Windows 为最终平台，`EX-AUDIO-NUKED`只保留原始音色／响度及可归因的混合夹幅影响，不豁免 PCM、事件、时刻、owner 或清理。

F4 主体已以 `3bdd02784e19dccc04154bdc94f5bebd64ce5efe` 集成本地 master。两树实际收据为 `build/f4/subject-delivery-20261117/local-integration.json`，SHA256 `3ecafb0b122ce13dccbd59afc99465326bac965456dbf2cb7fef276d62ea077f`；主项目 Debug 五目标重建及九项定向 CTest 通过。首次命令误列不存在的独立 `scene_transition` target，原 MSB1009 日志保留，按共用 `scene_handoff --b5` 更正后通过。972 个 tracked 文件物理一致，12 个已证仅换行路径刷新 Git stat，索引内容不变；用户 SAV 未变。稳定证据归档 `F:/fd2sdl-evidence/f4-subject-delivery-20261117-01`，3215 文件／259271757 B，manifest SHA256 `a2d78bc23ddcc4b633248957de22ef20af58f948c1070a26d3a663f7d057bc8a`。F4 正常 before／C11 双端后缀及未解释非 Q 点仍按[原专项](f4-presentation.md#5-f6必须接回的完整域)留最终门，不随主体集成改签。

## 1. 已确认事实与实施边界

| 矩阵 | 实际结论 | 本阶段动作与未验范围 |
|---|---|---|
| A17 请求生命周期 | 原 `19df7@1a2e7` 在 text416 完成后、200ms／close 前请求 stop；原 `3231b@327bb` 在 text9 完成后、script98 前请求 stop。当前两处遗漏已修 | 复用现 App BGM owner 与唯一 `scene.json` 的 bgm 叶；未改后端、tone 或 fade 算法 |
| A17 严格时刻 | 旧Title18及本次共享IRQ v2均匹配3748项寄存器内容／顺序；新候选3599项时刻仍RED，首次empty write170。旧97／455 callback内跨1..4ms的见证保留 | 已有16槽整数叶及host端口源；实际相位／CPU预算／回调内耗时仍补。不能只凭新共享IRQ或内容通过签门 |
| A18 PCM | 原 `49690` 使用16.16商、655 容差及每服务片段半相位；U8正常叶仅读当前 source byte。审计时的线性插值已替换为共享DIG／SB／整数混合层；全部111真实绑定的reader／DIG有限状态已逐缓冲核验 | 正常续档参数为22050／kind0／flags0／mono8／2048帧半块；自然cue、后级SRC／共同DC联合、完整原波形／时刻仍补，不能把host48000当原rate |
| A18 绑定 | 26 bank／111 项：110 项有有限源参数绑定、51/3 一项已由完整参数域证明源未引用，绑定未知0；78 个唯一 payload、12 个真实 replace caller | 分母保111，不删样本或新增拒绝；不是110项自然播放通过，handle、重复、时点、同时声部另验 |
| A19 设备与 owner | 现两 PCM handle、资源借用及销毁顺序保既有资格；null、首错、Zombie／retire、真实设备清理仍须当前候选核实 | 不改用户混音器、不造设备选择 UI／自动恢复功能；preOS 与可听路由分账 |
| A17／A18 固定参照 | 固定 DOSBox-X、SB16.DIG、SBPRO2.MDI；正常成功安装的源条件为 DIG5000us、MDI8333us、BIOS54925us，base5000／raw divisor5966 | 条件源结论不是实际16槽表／IRQ相位；正常标题只读观察不修改原资源／存档／寄存器／内存 |
| A22 有名例外 | 已登记vendored Nuked1.8、本地文件SHA及实际CL默认宏，正式48000、无补偿 | 只关闭版本／披露子项，完整混合输出归因仍依赖A17／A18；不新增音色测量 |

审计初始的两PCM默认整数网格能由float32精确表达，不能只因使用float就报错；当前共享SB／DC正式层已按其更大中间范围保持Q13整数，末端S16/32768载体才转F32。需核采样、加和／夹幅、8bit丢位及宿主输出变换。旧采集 `lround(v*32767)` 不能无损回译该S16载体，已改v3，不能将旧采集误差反推为设备错误。

stop回归的`T`观察点是字形已可见；正常用例的调用顺序另外确保文本成功返回，上传时HOST例并未返回，不得按同一字母扩签。NO／Esc例当前在取消反馈200ms内注入HOST，只证明该有效前缀不发stop；完整普通取消沿既有正式分支，不能由本新替身代签。

正常 Town G 会请求 3B sentinel2。完整原来源证明先 free／新读资源、格式失败后保旧 EVNT 指针且重新置 playing；旧借用失效不等于已证所有运行统一首 U。新分配／stdio 可能复用已初始化 heap，实际 owner／writer 未证，首实际事件字节读为 `433b0`。阶段初始的 `track_valid` 原子拒绝已由本节末的已证请求前缀替换；后继未知读取及H延续仍独立保留，不擅自解释为静音、旧曲续播或开放3B decoder。

## 2. 实际运行与来源账

来源均是只读原资源／严格 image／完整 Domain 函数与固定官方宿主源码，局部模型不冒正式运行。完整转储和媒体只在本地 ignored／F 盘，不提交或分发。

- 两处 stop：`build/f5/bgm-request-lifecycle-review02.json` SHA256 `147ed6260368534a490e1351cf1a98cc236a056956ec9bd31a57a9a3c385c05e`；应用 `bgm-lifecycle-candidate01` 五文件。实际主程序、`field_play_result`、`scene_handoff` 构建通过；六项直接 CTest 通过21.52s，含两 source-bound 前缀／正常开场及生成、schema、flow 检查。记录 `build/f5/bgm-lifecycle-apply01/`。替身证明请求／提交顺序，不签实际声音／4秒耗时；NO／Esc负向例只覆盖取消反馈中的 HOST 前缀，不能称完整正常返回。
- PCM：三完整源包共30函数／2129指令，`pcm-contract-review01.md/json`；独立 Fraction 模型16项及真实31/8条件反例，不是 C 或正常 DMA／WAV 运行。
- AIL：`shared-irq-bda-model01` 的22检查；`ail-timer-candidate02` 独立 C11 core／test 的15组通过0.02s保原身份。后续源接线及本次正式目录检查单列如下，不能把 standalone 通过写成 PIC 通过。
- Driver：`driver-service-word-review01` 核 command300 真正写 header+2e=ffff，signed−1排除额外 service timer。`mdi-port-time-source-review01` 核实际 writer 每次2 OUT／14 IN，映射及初始化内容与当前 C 相连；同值／未映射写也消耗 I/O 时间，不能在 writer 过滤后才计时。
- Host：`host-io-time-contract01` 核固定官方 commit 与源码；普通条件 IN20／OUT15 cycles、OPL额外 `min(9,CPU_Cycles)`，边界剩余可负，不擅自下限钳0。实际 live CPU／last_callback／完整 IRQ 与安装相位未测。
- Sentinel：`sentinel-preview-source-review01` 完整35函数／2601指令、15守卫，纯静态，不是自然 G 实机或安全后继证明。

正常Title工具02已实际应用，23项本工具纯检查通过；真实prepare通过但未运行，收据`F:/fd2sdl-evidence/f5-title-state-prepare-20261117-01/report.json` SHA256 `49482c037cdaecf69123aeb0c0abf3bf0c6b95497c8d00011a155d3f095bf2eb`。源复审修正了RPC非原子标记、设备absent rate标记、清理晚错覆盖首错、实际root／13个import路径守卫；没有扩客体读范围或预算。

其唯一实际Title观察**未通过**：`F:/fd2sdl-evidence/f5-title-state-native-20261117-01/report.json` SHA256 `898c61b9f483b0923120bed6ab623837b24ce2a66098a70da39ccf34dc81b2fe`。16个六字段PC样本均不在普通protected context，未进行任何data读取，未取得D／cap／timer值；只有首Title资源WGC，末检查点未到达，不签PCM／PIC。未发送业务键、改读门或加预算。原进程3444正常exit0，Job32852的worker1、timeout／residual／forced均0、cleaned1，RPC关闭且原件／来源／SAV守卫通过。

该轮外层`f5-title-state-outer-20261117-01/report.json` SHA256 `c6a88c0d88180c73d41dbcfb58bc06032b28c777500d82989cca6a5a8726bcbb`：显示先OFF，一次有界需求／零位移恢复后内层strict ON；外层线程22836、内层36208均在owned清理及来源守卫后同线程释放。前后活动方案一致，未改电源／混音器。此失败不是模型／工具服务拒绝，也不是已发现游戏或SDL差异；不重跑同条件Title。下一独立代表研究现有合法`START.SAV`续档→战场正常业务入口，观察相同有限音频合同；不冒fresh NewGame、改档或保证一定采到protected样本。

单owner clock与主线程Audio transport已接入正式源；后续正式DIG／PCM接线见本节末，严格PIC仍未完成。同一epoch／AIL／BDA支持明确live与external输入；AIL模式read／deadline纯读，正式caller先observe提交同一输入。十个正式源码单元有63处observe，数量只作接线盘点，不是63项功能。IRQ前和最终now关闭音频前缀，输出错误不决定BDA；prefix保`whole_ns+fraction/1193182`，不能先ceil纳秒。独立44.1／48k反例的错误多一帧已由实际C回归覆盖。

`main`在driver前显式attach，终态现先DIG quiesce，再close clock、AudioDestroy／BGM退休／释放借用。首65536周期是有名H完成调用政策，**不签实际原安装相位**。BGM事件仅由共享MDI service推进，Nuked渲染不再推进sequence；begin／toggle／stop同步提交，删旧sample计时字段和无时标64命令队列。源安装次序为MDI→DIG，旧DIGfirst错误标签已撤回；当前Main已安装真实DIG服务。只含音乐的离线夹具仍显式提供idleDIG以固定8333／5000共同base，既不签DIG实现，也不冒正常波形。

实际`build/f5/clock-transport-check01/receipt.json`：六目标构建、`bios_clock`／`bios_clock_ail`／`bios_clock_cpp`／`bios_wait`／`audio_transport`／`bgm_native_sequence`六项CTest通过0.32s，含当前16组clock边界、11组transport及原独立音乐序列向量；没有设备／GUI。`build/f5/app-probe-connection-check01/receipt.json`另记录主程序与三音频工具构建、两OPL trace／输出路径三项CTest通过0.90s。后者只验新入口与离线组合，不重跑六项，也不组成原生全绿。前后source／用户SAV守卫通过。旧prefix-check01的13组和check02的14组各保原候选身份，不相加为覆盖率。

只读复审`build/f5/live-clock-parent-review02/report.md` SHA256 `e9410b822449fc60ec195a5742c79eb56cac0d19627c1e006c6ed6cce8d4f16e`确认等待初读／尾读、精确prefix及终态借用方向；所发现旧字段残引用和main缺attach均已修。非终态destroy不得继续dispatch旧pending，external离线不能混用liveSDL helper；这些生命周期／输入前提仍明确保留。`observe`不是错误成功凭证，真实错误／设备收尾须另外验。

新App mix用`fd2-app-mix-capture-v2`明确主线程成功Put后的取证点；离线OPL用`fd2-title18-offline-opl-v2`明确共享PIT、H首相位及instantaneous-no-cpu。DRO reader保严格v1历史分支，v2身份缺失或混入v1均拒绝；本轮19项协议检查通过。旧报告／波形不改签。live用SDL已有AudioStream队列，有名H上限500ms、积压只丢新chunk并保首错；null与输出故障仍推进逻辑。正式App正常cue的队列峰值／零丢弃及新v2原版联合仍未验；下节设备探针另有有界实测，不能用成功Put称可听通过。

上述v2保原候选身份，当前采集器已升级`fd2-app-mix-capture-v3`：成功Put观察点不变，明确`pcm_quantizer=s16-unit32768-round-half-away-saturate`／version1。旧`lround(x*32767)`不能无损回译原S16/32768载体，不能把这层采集误差归为PCM算法。新量化保持全部65536个S16值，非整数F32另按已声明half-away／饱和，不冒任意F32的SDL转换等值。v1／v2 reader严格保历史，current runtime只收v3，不补字段或改签旧WAV。

`build/f5/capture-quantizer-v3-check01/receipt.json`（SHA256 `3d0cfed2fe275c223fd0ae9d8578298784322ef56cf20279aecd1e5e89c34dd9`）记录实际root配置、三目标及三个直接CTest通过3.40s，含worker封包、主线程Put顺序和完整S16载体回译；源／SDK／SAV及执行前后PE同。另14项AppMix协议及33项audio-check工具纯测试实际通过，无设备／GUI；后者是整个测试文件的本次运行，不冒33个新增行为，也不补造它的pre/post守卫。没有为该采集格式变化重跑已过owned设备代表。

新`F:/fd2sdl-evidence/f5-shared-irq-title18-offline-20261117-01/receipt.json` SHA256 `f42337fab23a84705ff690da0108733490babe8872d5f3dfeb05baca5ea32ec9`记录实际当前源v2离线24秒，与原封存DRO比较：3748写内容／顺序相同，3599 live事件严格区间仍空。上下界为`121300/44100`、`−12000/44100`毫秒，有符号宽`−133300/44100`，首空write170。没有重录原版、改投影／容差、删点或搜索相位；原RED与新RED分候选保留。共享IRQ的修复不替代端口内部时间。

四个直接BGM测试已按同步begin／控制及close迁移，原SBPRO2静态前缀、volume golden和pure函数不变；400帧旧等间隔假设改为由两5966周期上界导出的481帧。`build/f5/bgm-clock-fixtures-check01/receipt.json`为当前root四目标／四项CTest通过1.87s；独立候选先3通过、options旧memcmp失败、按原449e0同步CC7重发合同修正后单项通过的原件保留，不能回写首次失败。

12文件consumer迁移已接当前root：v1历史／v2当前身份严格分开，控制／retire／首错／quiet／成功Put后tap及关闭无晚Put均保直接回归。`build/f5/audio-main-consumer-check01/receipt.json`：六目标及六项CTest通过3.59s，另12＋2工具检查通过；只有`audio`使用SDL dummy stream，其余为替身／observer协议或dry，没有真实设备。CTest现名`audio_pcm_capture_main_order`不再误称RT callback；旧登记保历史，不改签。

### 当前Windows真实流与私有SDK资格

两次owned静音设备代表串行执行，**有真实WASAPI流、无窗口，无混音器／路由／电源改动**。来源／SDK／用户SAV前后不变，Job均正常exit0、timeout／residual／forced为0、cleaned1：

| 本次代表 | 实际结果 | 收据与未验边界 |
|---|---|---|
| public，PID12180 | 48000Hz／2ch、296 Put全部成功，首错0，source owner违例0、三个source各退休一次；所采队列峰5224B | `F:/fd2sdl-evidence/f5-audio-public-owned-20261117-01/receipt.json`；峰值是GetQueued采样值，不冒连续最高值或物理延迟 |
| 同进程SDL内部removed，PID33480 | 298 Put全成功，首错0，峰18008B；ref1／1、Zombie0→1、三函数切换，确认断连后正队列976→0且期间无Put，之后sync与成功Put继续 | `F:/fd2sdl-evidence/f5-audio-local-disconnect-owned-20261117-01/receipt.json`；不证明真实拔插、WASAPI恢复、默认路由迁移或可听 |

当前SDK源码真实commit为`5ac37a8ffcf89da390404c1016833d56e2d67ae4`／`release-3.2.12`，目录`2813…`只是缓存名，CMake默认回退版本也不是本次实际依赖。旧私有库`5d9204…`与当前`89b0bb…`不同，原精确门正确拒绝，失败原件保留。只读COFF／C13六member的365条SHA256／264唯一路径均连到当前源；6条PCH无checksum标记单列，PDB同GUID不同age也没有冒同快照。来源报告为`build/f5/sdl-private-sdk-qualification-review01/`。

同一当前SDK的独立89b候选两目标构建、九组pure及dry通过后，父侧才执行上述一次owned新版代表。`build/f5/sdl-private-owned-guard01/receipt.json`的46输入及snapshot／root连接前后相同，资格仅限该固定库／ABI／owned探针；候选原`qualified=false`原件保留，不回签旧5d。现行工具固定身份更新与直接集成检查另记，不因换一个hash宣称任意SDK兼容。送零、成功Put及队列消费都不签原版波形／绝对时序／整个A19。

现行私有CMake已保留旧5d分支，并新增干净5ac源码／四个额外source-header摘要所限定的89b分支。`build/f5/sdl-sdk-identities-apply01/receipt.json`记录新独立目录配置成功，明确选中`SDL-3.2.12-5ac37a8-89b0bb04`；没有重跑设备／九组pure或重编SDK。第一次应用后的逐字节检查因root CRLF／候选LF而停止，三个文件规范化后全部同；patch已经应用一次，续接只作配置。第一次脚本未持久化其局部before守卫，收据明确该限制，不补造；配置前后另有实际来源／SAV守卫。

### 有界正常标题日志的实际结果

`title-mdi-trace-native-20261117-01`在LOGL派发前失败：工具误把七条`startup_geometry`只读WGC诊断当业务输入。实际只有PING；PID28912正常退出，Job44048无timeout／residual／forced，来源与SAV及显示释放通过。原报告不改；候选02只允许同PID、完整已知字段、真实WGC来源的该类诊断，key／delivery或其它记录仍拒绝，三项新守卫检查通过。

`F:/fd2sdl-evidence/f5-title-mdi-trace-native-20261117-02/report.json`为唯一实际LOGL：一次固定32768条日志＋一次末行EV6，没有BREAK／RUN／step／BP／客体写入或业务键。日志完整且末行未执行的PC／寄存器吻合，时间43987.000100..43988.646500ms，跨度1.6464ms；**完整writer为0，故overall FAIL，不能签端口时序**。按该入口合同不扩大预算或续跑。两次失败都是具体工具／观测结果，不是模型或服务拒绝。

实际第二轮PID2976正常exit0，Job36508的worker1、timeout／residual／forced0、cleaned1；原始与停止后Title均由WGC核内容，未观察到owned DOSBox前台。两层来源／SAV守卫及同线程显示释放通过，活动电源方案不变。日志／媒体留F，不提交。该instrumented前缀的日志完整性与“没有取得所需MDI时刻”分账；原严格DRO RED仍保留，后续先继续独立PCM／混音实现，不以这次局部观察签完整F5。

### 正常城镇 G1／G2 的独立业务返回

`F:/fd2sdl-evidence/f5-town-g-native-20261117-01/report.json`（SHA256 `8046c225ff2e9afff8ac374d12f3a0542d28b656dd4a72ee0b736b8244348429`）实际使用只读原`START.SAV`的完整22987B容器，仅copy2到隔离`FD2.SAV`；未改字段。manual0为stage6／9人／资金2601／options[1,1,0,0]，通过完整原row6连接type2、背景62和原坐标，未套stage1模板或把stage改2。准确七键为`DOWN,ENTER,ENTER,G,LEFT,G,LEFT`，两个G按原先加一的源码分别请求1、2，不是首次G0。

本次`ui_return_qualified=true`：两个LEFT均到对应新selector的完整312×192／59904像素源页，最大通道差3，沿既有资源颜色合同；不是停在同一帧或只凭未崩溃。没有WAV、RPC、日志、寄存器／内存观察或客体写入。原music/SFX为0，不能据此签音频、实际MDI安装状态或旧EVNT后继已定义；normal G2后的UI返回子性质成立。该观察时SDL的前置拒绝／上抛退出在后续单S候选中已修正，原报告不回签新候选；不能把未知音乐后继统称整个正常操作不可审。

PID10788正常exit0，Job7792 worker0／timeout0／residual0／forced0／cleaned1，未观察到owned前台；原件、资源、用户档及**隔离副本全部文件实际均未变**（包括TMP）。`scratch_full_value_qualified=false`仍是未给manual cache完整语义oracle，与实际copy未改分开。outer报告SHA256 `7cd448ca03fc0f966f612ad3926a11ccfcd09d78cd46382c4f8afb273bbd17e7`；两层显示同线程释放、一次授权唤醒后strict ON，电源方案不变。工具刻意令`ok/full_alignment=false`，只签独立UI合同，不误记为完整F5通过。

### 正常续档的独立设备字段资格

`loaded-field-native-20261117-01`实际走原START.SAV隔离copy→UP／CONTINUE资源ACK→ENTER→首浏览，前后field门通过。整体报告SHA256 `f825a164e1f1aa75af41a0d9da36e0557926760ff285c0434b67cc3170e3a7ac`仍为**FAIL**：全目录copy守卫误要求正常生成的`FD2.TMP`保持不变；只有该隔离scratch改写，其它文件及SAV字节／mtime全等，原件／用户档也未变。原11512与Job38828正常清理，timeout／residual／forced均0；显示同线程释放、外层守卫及活动方案不变。原guard在TMP处先抛，运行时末次reference.verify未到达，不能补造其已调用。

事后只读资格为`build/f5/loaded-field-scratch-review01/report.md` SHA256 `70cf5b27dd80b0b65d1121437647faa89ab7369f63075f020450cb77a9b87e6e`，`analysis03.json` SHA256 `0a0a8fda4b3d3efed1db0f196252f487cb8474eb57f122c3e6028c75ca83f197`。完整`10010@103a9..103d4`从新cache写0x32a00；独立按START唯一ID顺序0／9／4／30／96与FDICON原12帧各组重建，**24,158个已赋值byte全部相同**，范围[0,240)及[1920,25838)。其余183,202B没有本次值oracle，不猜0／旧cache／heap、不自动判U，也不称全207360B已证。末次reference在事后重新核验成功，与运行时调用分账。不改旧报告、不重跑窗口；将来守卫需上述源写集、size／路径及其它文件零差异，不能仅排文件名。

同轮18个原读取片共1742B的长度／hash、两锚／fixup／descriptor、D→AIL type0＋registry、两P→D及稳定D／cap／roots／preferences已独立复核。由完整`406b0/40cf0`重新计算的参数也一致：**22050Hz、kind0、flags0、单声道U8、half2048B／2048frames、int32 build8192B**；两P为type0／11025Hz／volume100／pan64／loop1，观察时status2。实际slot0 MDI8333us、slot1 DIG5000us、slot15 BIOS54925us，base5000／raw PIT5966；busy／defer及volatile样本仍只是各次读取，不拼同刻，也不反填Title或首IRQ。

新`sb16-normal-output-source01`的23完整direct-CFG body／630唯一head／1617B把D低WORD rate／kind，经真实两个401 producer和driver，接到`41 high low`及C6／mode00／half count2047，固定emulator不再按TC量化。该源资格SHA256 `888eacb7af227f48c7fbcb8732ada72421fa1f05172891b2249dc105ef9b3c15`；78个合法cap条件式不是78次运行。故实际DIG参数和SB输入合同可用于下一实现，完整DMA内容／IRQ时刻／最终波形仍未通过。

`build/f5/sb-host-profile-source02/receipt.json`另由已冻结bootstrap／conf重算declared配置，并核固定官方blob：内部48000、nosound／DC为true、sampleaccurate／swap为false、SB16／sbmixer为true；未覆盖filter／mindma按固定源为false／−1。给定已资格D，输出低通11025Hz一阶、alpha16.16=38711，minDMA=66；fresh空channel的48点需求是24个source byte（普通ceil23＋首点预取）。这是配置＋源码推导，不是新读runtime变量或PCM。**不能把`left<66`泛化为每半块都会安排短尾event**：完整caller续审表明CheckDMAEnd只在speaker／UNMASKED入口，SB16 speaker又早返；正常C6块末由mixer需求读到left0，短尾float event按start／resume的实际unmask条件另核。先前粗述撤回，不改旧来源原件。

三个有限核及共同DC叶已从已审独立候选取入`src`：共享DIG的两P／build／DMA控制、SB物理消费与IRQ／ACK provider、SB channel原2048×2整数窗／插值／低通。converter原`sb-channel-converter-candidate01/receipt.json`记录15族134 checkpoint、完整输出窗和状态，真实pre/post输入／PE守卫后的CTest通过0.02s；不是正常资产或整机波形。正常304自身先DSP_Reset，再发non-auto stereo16单word探测；固定host的残word规则不装一个完整stereo点，但等待期间的窗／相位仍须联合，不能因此假fresh。合流之前必须保持Q13整数：合法常值U8=255／gain8192可使filter达到266338303，先F32会舍入而使最终一位不同。共同DC在全部FM+SB求和之后，不能塞进每cue或以Nuked例外豁免。

`build/f5/native-math-leaves-apply01/receipt.json`记录21项新源码／合成夹具按封存字节取入，未替换旧PCM或改Audio/Main。CMake共用`fd2_audio_native`静态核；`native-math-join-check01/receipt.json`（SHA256 `acc2f0427295cbcfd62c6ef789da9d4c4e40c8b4a804c5cece7d407483e0fba7`）记录实际四目标构建、四个数值与两个输出守卫CTest通过0.42s，所列源／SAV及执行前后PE同。生成器不读原资源或C输出来造expected；这只是当前root的数值／组合接线资格，不签实际Main、clock安装、原WAV或完整F5。

`build/f5/sb-bootstrap-gain-review01/report.md`（SHA256 `3be4f51e86e3690b6fbc0d3814bc20849f11fda646e8602d4b69a00bc1b6631b`）进一步由固定启动命令、完整已解码MDI端口及DIG305实际DAC32／33写集推出正常源条件下左右gain8192，不再仅凭默认猜测；无实时读gain。304不装完整stereo frame可证此前signal／filter／fraction零，不能据此把ms游标或首次cue暖态也填零。正常401先消费静音128仍会使loaded／相位改变，正式owner须持续推进，不能每cue重置。该报告只有12输入只读资格，没有新增GUI／原CPU。

`build/f5/pcm51-3-binding-review01/report.md`（SHA256 `65c191d8185482b8b38704ee02ca03e053683f919ae9208819f3d9dc859674ea`）已关闭唯一绑定未知子门：全408 FIGANI元数据、三个完整314de producer、physical双向／AI-ray／DERIVED与MISS0公式中，selector4仅11项／209part、cue集合{0,1,2,4}，不产生3；其它bank来源也不通51。52去重owner与119伪C调用表达式分账，后者不是119机器CALL／声响。111资源分母仍为110 finite-bound＋1 source-unreferenced；无本绑定任务未知，不是全进程dead／全A18通过。原51/3样本仍有效，不删资源、不伪造cue；本次只读元数据及严格chunk join，无播放／GUI／原CPU。

正常销毁入口新静态补证为`build/f5/sb16-normal-shutdown-source02/source01.json`，SHA256 `925ea6df09ca542feb548b498d58f458df262f77b4d18364b2a51350ad16050a`。raw16 command306@09a5的七个完整direct-CFG／171head无unresolved，原件与旧来源守卫同；DSP_Reset→原vector／mask恢复→mixer32／33保存高五位恢复→0124=1。未执行原CPU／游戏／设备；不能用清数组或controller release冒完整DMA quiesce，实际关闭前的有源全局FillUp与借用结束仍须接回。

### 正式Main／PCM／整数混音接线及直接检查

`build/f5/original-audio-connection-apply01/receipt.json`记录正式接线与四项consumer夹具迁移。Main使用固定48000的original profile，一份D／P0／P1／build／DMA借用同Audio；不再使用旧24声部线性PCM路径。SB保Q13整数，和Nuked的精确S16载体合流后做唯一共同DC／record／S16夹幅，最后以S16/32768送AudioStream；v3采集可无损回译该载体。默认U8 scale202只定义一处。正常driver存在与OS stream分开，未就绪／disabled／坏index保各自已达前缀。

清理先完成原306 Reset投影的DeactivateIRQ→旧全局FillUp→mode NONE／清left与END，再close clock→AudioDestroy→释放driver、view及bank。normal init只适用于fresh D，同App一次；失败也可能已经建立借用，header明确要求D继续存活至终态，不由负返回推导可提前free。Load／START不重建D。Audio新1ms anchor／DC0、完成调用后交付SB IRQ、tick／END同刻顺序仍是显式H政策，不能据接线成功称原PIC已对齐。

实际`build/f5/original-audio-root-check01/receipt.json` SHA256 `2c9ff3e66439687b58d6bfc2cbe998728c85adca9788e912f992062e3e4f06e2`：20目标（含完整App）构建通过53.56s，20项直接CTest通过10.17s。原命令把实际`s5_cinematic`误写成`scene_cinematic`，未注册项不计通过；只补跑漏项，`cinematic-followup01.json` SHA256 `97741b50ec1229ff01626fd9b0d626f29ba977d58690f57ffb812950955659ca`记录1项通过0.10s，前20项未重跑。两份收据分别保源码／资源／SDK／SAV及所列PE前后守卫。该候选App SHA256 `6654f4384970a93e76e688de8ed35667930ec8e541ec652d66fb710b0fa603df`；后续候选另记身份。这些是实际C／完整链接／dummy quick-entry证据，没有新正式GUI、实际native-profile设备或原波形通过声明。

独立复审`build/f5/audio-final-candidate-review01/report.md` SHA256 `b9a8c5d14f50d9e0f53b4b928cfe4cfc2052487be3ec54d5d25df5d05d62f60f`未发现正常Main的内存／availability阻塞；明确留下正remaining-END组合未测及原MDI packet／IF时序差异。前者新增有限状态检查，后者按既有完整源继续实现；不能仅换测试名或调整容差签门。`mdi-finite-cost-review01/report.md` SHA256 `373339f6e562aa5799dcec17fa13d1e89acac360c59ab9bf89a7563bf7b23cd0`证明：512B消息packet在service尾flush、command502期间IF0、普通writer有15个全局FillUp点。复审时的直接dispatch及即时ACK尚非此合同；随后packet修复见下，真实IF／成本仍缺，共享PIT与数学核通过不足以关闭A17。

后续`build/f5/persistent-chip-check01/receipt.json` SHA256 `b3a54752159753a3ea7948274c315f80f1639142ef0bf58d91b6dc345b0bd672`记录8个实际App／BGM目标重建及7项BGM CTest通过2.296s。chip改由同AilDriver常驻source持有，直到AudioDestroy才退休；单曲交接只终止旧sequence，不替换chip。新增有限生命周期用例确认无活动sequence、pre-start窗口及结束后仍推进Nuked，不让未来带耗时的TIMB／flush把未started的新曲误退役。该候选App为`4424c8c901f5e3c3abc7a10016a1d0ef766dfa2753cbc4f9d31adabc019415f9`；没有新增原版／设备观察或PIC资格。

FA03的给定状态组合现已补：`build/f5/audio-positive-end-apply01/receipt.json` SHA256 `f7693d9ea75fc433e7202b020ac64722992e6df1f548d0401de1f42dcdba7c58`记录实际root测试目标重建、仅`--remaining-end`新3族／8例通过0.016s；未重跑原7组／22向量。独立Fraction／host算式在执行前给出left1／2的500us正END、980us下一Mix先取消旧END、真实第三空DIG service请求pause后给定pendingIRQ／D4重排，以及NaN／溢出首错前缀。检查全DMA／cursor／half／channel／共同DC与已发布PCM，不以零输出或未崩溃求绿。该门只关闭给定合法数学状态的whole Audio集成缺证；自然短尾可达性、真实IF／同刻tie／桥／安装相位仍未验。

正常SFX代表的工具已按当前固定48000／v3迁移，`build/f5/sfx-reenable-readiness-apply01/receipt.json` SHA256 `db6b71c83a01f485aa8eee5811a72301ac1ce2593bbb98d9ca114512ab6e7d44`记录39项Python检查通过3.015s。原版首窗前及退出后核bootstrap／reference及实际quiet argv，Save沿已证10010验证raw+2而非排除该字段。该准备收据早于下述唯一双端，不以dry签native。代表只补正式输入→共享PCM主路→成功Put→完整2652点／13候选的有限身份，不重放其它A7。TMP整文件mutable名单仅为隔离保护，不是全TMP语义oracle；完整波形／PIC与可听不由此扩签。

MDI消息协议现已取入：同一AilDriver只有原512B storage／count／cursor，严格`used+size>512`才先flush，正好512不提交；共同service尾包括无active／status2／EOT，stop先flush32槽note-off、再留下cached控制尾，正常非空TIMB前flush旧包。正常Create另恢复原16通道、逐通道15消息／44B初始化，含遗漏的program0，不在换曲重置。`build/f5/mdi-message-root-check01/receipt.json` SHA256 `1f3a17361e4028555af9c5809d92d39d5621d97f52cf071b45aa7fcc46b6a806`保实际8目标构建成功及首轮7项中6通过／1失败。失败是旧sbpro2私有夹具仍要求入队立刻改变hardware；按原42980／422c0的字面9B pending与hardware保持前态更正，仅重建并补测该1项。`sbpro2-followup01/receipt.json` SHA256 `f8eb201452af906d4e80bf731e5510afb23863dde80b9b397c8b9d1a59954b6d`记录16原组＋7新packet组通过0.26s；前6项未重跑，旧logical golden hash不变、物理提交另查。此候选App SHA256 `252575591ed8abef9c4fc6727089a1c6d3e8f748617ecd3b32b9d12dc5932880`；新两项source_live失败guard也在本候选实际执行。

消息包修复仍为时间擦除后的协议资格，不是严格时刻：普通stop在busy0分支的三次retrace、初始化四次三retrace、normal writer／DPMI／IF／PIT安装相位尚未接完。startup renderer须在OPL_Reset后、首writer FillUp前ready，当前完成调用H顺序尚不证明该前缀。原sentinel的未知event barrier现已接入下面的单S实现，不把到门当正常service完成或自动flush；原后继声音仍缺证。正式PCM结构变更直接影响的准备／进展／结算三个consumer另经`build/f5/pcm-formal-callers-check01/receipt.json` SHA256 `b1ee99d5adf38e6b737af6627b312c14421cc4bd606d499b18a98317223cbe33`实际重建／3项CTest通过12.766s；这些使用有限spy／逻辑，不能代正常GUI或波形。

渐变golden的观察层另作更正：旧R3／R4生成器及其stub精确位置尚未恢复，不能由测试顶部说明或未变hash推定它位于42980、422c0或physical502。当前逻辑观察点新增独立依据：`build/f5/source-fade-arithmetic-oracle01/result.json`由449e0／43270／43230／42980四个完整源及strict chunk身份推导，不读C或其输出，也不执行原码。给定active／普通／无EOT、busy／lock已允许、raw cache及map在切片内不变且不溢出的六域，共784状态和逻辑CC7序列精确匹配六对旧hash。独立实审`source-fade-arithmetic-review01.md`（SHA256 `1bdddc7e9609e3b1a4cc7f526ec193ab8b3a58af8cd13e2c7a244e2006cecaf3`）核signed IDIV、目标先写、相等／duration0、逐级acc余值及第8service到目标仍重发的原分支，无新增C／原CPU运行。

可复跑的纯整数投影为`tools/testing/sequence_fade_vectors.py`；实际`portable-check01.json`核六组完整状态／消息与上述源模型相同，并验证拒绝覆盖已有输出和原目录。它只为当前C的有限逻辑层提供新source oracle，不补造历史runner身份，也不证明真实包、端口、时刻、曲目全循环或wave。physical packet仍由独立字面来源检查，严格PIC仍RED。旧原码向量的历史范围保留，未因新模型重新执行或扩签。

### 当前正常SFX双端及全Audio传输观察

同一Audio现有纯读transport快照：有效GetQueued样本数／其Put前查询字节峰、成功Put数／backlog丢新chunk数，首错沿原owner保留。仅测试capture环境在quiesce→clock_close后、AudioDestroy前输出一次；getter不sync、查SDL或推进时钟。`build/f5/audio-transport-stats-root-check01/receipt.json`记录正式App及AudioOriginal构建通过、新`--transport-stats`及43项直接工具检查通过；旧10族／22向量／BGM不重跑。统计不改变500ms政策，不把查询峰称OS真实峰；quiet等长零Put也计成功，不能据此说可听。

唯一实际原版→SDL代表为`F:/fd2sdl-evidence/f5-sfx-reenable-native-20261117-01/comparison.json`，SHA256 `6f0178ee1362cf52168f7c748eae8cb23e0acf312d3979309fc9a9b53bec75e6`；外层`f5-sfx-reenable-outer-20261117-01/report.json` SHA256 `c707213f42793c3c4becb3198d72fc6540246e352189d1cc690f66bba86a72c9`。实际App为`1791e5511e894bc6c6a0da876eaeea3bc36a736a0a62cc66b441b7ca1b626604`。只读START派生focus6/14及Music/SFX00的完整22987B受控夹具，沿正常CONTINUE／SYSTEM重新开启SFX、退出菜单发声、UI Save01；没有END或其余A7。原／SDL均通过完整Save及原10010 raw+2门、48000闭合录音、13候选／完整2652点sample8身份，score分别0.9643299701／0.9559123747，和runner-up的margin为0.1528834168／0.1812268855，未改0.8／0.05阈值。两端重复实例onset仍ambiguous，不能据相关性签唯一ESC实例、绝对时延、默认幅度、全部样本逐byte波形或OFF绝对无声。

SDL这次同Audio生命周期首错0、37445次有效查询／成功Put、backlog丢块0；Put前查询字节峰18432，v3窗口error0。原PID41888、SDL36056各正常exit0，Job38424为worker0／timeout0／residual0／forced0／cleaned1。原reference pre/post、所有源码／PE／原资源／用户档及关闭证据守卫通过。显示初OFF，经一次零位移idle-reset且指针未移后console／session均ON，内层strict门通过；内外ES各由原线程释放，活动电源方案未改。取证只签该次正式共享PCM路径及有限身份／传输／清理；没有物理扬声器／路由、全cue或严格PIC资格。

`build/f5/sfx-reenable-native-join01/receipt.json`及脚本只读上述既有报告复核，不重跑窗口或分析音频来改变expected。后续owner／时序修正按真实影响做继承或最小新证据，不因新PE机械重放本对。F5仍未完成，下面的源时序／sentinel／全波形等门保持待办。

### 单一音乐状态与占位资源的已证请求前缀

原 Main 只持有一个 S。当前移除两套 host voice、RT atomics 与逐曲退休状态，改用同一个 S、唯一曲号 cache 及常驻 chip。合法不同请求先写 cache，再同步旧音频前缀、stop、init、start、volume 和 loop；host 错误保留已完成的前缀。只有完整三字节 `20 0d 0a` 资源按已知格式失败族继续菜单，其它坏参数／未知坏格式仍负返。`43160` 只写原已表示 reset 字段，不清旧 EVNT、volume 或 inactive note／duration。

`build/f5/bgm-single-sentinel-root-check01/receipt.json`，SHA256 `b8bc0be2ed041890941038e86af849318e49094861e645609f3240bf6948a0e9`，记录本树应用、8目标含正式App构建14.109s及7项BGM直接CTest全部通过2.313s；输入、原资源／SAV、SDK与运行前后PE守卫相同。新五族依据完整源指令、固定资源和原写序，检查唯一S、cache早返、G1→G2→G3、reset保留、closed输入、纯getter和sticky首缺口。没有新GUI、设备或原CPU执行，旧golden常数未重生成。

正常启动在MDI/S及资源18初始化成功的条件下，`25db5`固定请求18支配后续标题／游戏UI；不能把“API首次直接请求占位资源”说成正常UI首请求。正常有效曲切到G2后，原资源被替换，旧EVNT指向的内容／寿命仍未获资格。当前分别记录`447ae`字段producer缺口与后续`433b0`实际将读event的门；未知service不继续event、fade或共同tail-flush。H延续只渲染同chip已拥有的声音，不模拟原未知读取，也不证明原ISR返回、IF／PIC或后继波形。有效曲恢复当前事件资格，不清历史首缺口。正常Town原七键只证明旧原版UI返回；本候选不据此回签声音。

### 独立时序观察器的当前资格

为补原DPMI返回预算／PIC的独立数值证据，已在普通C盘隔离目录、固定官方`b6abbd5`完整源上构建默认关闭的被动观察器。`build/f5/host-observer-implementation01/report.md`记录新PE `cd1c4e113a1c443980e551a714635dc70e6146d1f0fa014de595450b01e8275e`、9文件补丁、3865实际构建输入守卫与15组宿主检查。该交付未启动新host／guest／GUI；父复审仍在进行，不能把“构建通过”写成实际返回／首IRQ已获证。

新诊断host与原参照`a82adcfa…ee0832`分账，不能加入原reference允许表，不能替代原DRO的3599严格RED。恢复候选、实际下一取指和最早返回是三个不同命题；现日志没有OPL端口／command502身份或PIT divisor，不据桥日志泛称这些已经实测。只有完成具体hook／上下文资格及新有限正常场景后，才为对应字段建立独立证据。

### 同一 S 的节拍字段与来源发布

当前已补同一 S 的 `+44..6c`：成功init的tempo100／target100与累计0、原reset的beat／measure写集、meta51的denominator、meta58的频率除法／CL-mask移位和meter相位顺序、每callback一次计数与tempo子步、单if beat、非EOT的volume→tempo fade，以及EOT回环只重置三个节拍字段。正常15曲的meta51不改变120Hz或tempo百分比；给定0／50／250及高位DWORD纯向量也不冒自然UI可达。

`build/f5/bgm-meta-beat-root-check01/receipt.json` SHA256 `3cf9fd52c9b4773ec344b81e07c3aa141293ec3f1a9d1102f86780d61fb4d8d0`记录正式App及三个直接目标构建6.25s、三项CTest通过2.593s。原72个hash不变，同一次15曲×两loop另核30个独立16字段状态指纹及六族边界。独立生成器`tools/testing/sequence_beat_vectors.py`只读固定FDMUS，输出短指纹，不输出曲目内容，也不读取C结果。当前App SHA256为`5c602321f970210b754c233489f85c9c70329373c8aa98d3790f65291bbfeb82`；它晚于已过SFX原生代表，未据新PE重放该对。一次父脚本准备命令在PowerShell引号解析处失败，尚未应用或测试；改文件脚本后才产生本次收据，不把该准备失败写成候选C失败。

原EOT在回环或结束后仍peek一BYTE，完整EOT flag门使它不消费新meta／MIDI／delay。当前C只投影相同内容状态，没有补未知尾读。`build/f5/mdi-service-eot-peek08.md`记录15条回环首BYTE均为资源内FF、十条terminal资源内尾BYTE为0；另五曲terminal位置恰等loaded长度，真实heap owner／值未闭。不能由目录长度判U或借whole-DAT邻字节代原malloc来源；高bit分支多两个ordinary head，严格成本仍须补，不能因内容无写而豁免。

此处的正常producer已再闭合，不能把五曲都当游戏必达或全移诊断。`build/f5/host-observer-revision02-peer/music-loop-addendum01.md`按13个原caller owner、32个物理site及9个唯一冷music节点盘点；31site有完整指令，`22e5c`的stop由已发布154B全文继承，未冒raw资格。正常正曲目loop1仅`{11,16,17}`；复活成功的`2a43e→2a681 music(11,1)→2a44d/2a4a2`可正常等待到11终止。故五个未知terminal尾中，**11仍是正常门**；3／8／12／19的正loop仅属本已闭producer之外的诊断参数，保API支持而不将它们伪造为自然声响。循环rewind的FF窥读成本仍须保留。

统一来源已通过隔离Domain／真实数据库发布：18项命名输入中17项更名、25个owner追加来源注释。`39805`改为`ail_stop_sample`，真正`398df`登记`ail_end_sample`；五个raw16 driver offset标签没有混入FD2 canonical VA。`build/f5/ida-audio-publication01/publication-receipt.json` SHA256 `6e7121fa798f1e581c8481148b5ccf8fd0a81dad16fee6d93648de7aaac212c9`核全部1312函数的原bytes／chunk／边界／flags／类型保持、只确认的17名变化，反编译后未保存推测类型，旧C仅按before单向更名映射后token相同；11个发布清单最后刷新并逐项校验。`direct-tests01.json`另记录四项当前checksum／入口／命名／VA检查通过0.203s。继承的LE／原型／地址迁移报告没有重跑或扩签；最终源码地址盘点留本阶段代码集稳定后刷新。

曲目11尾读的进一步只读复核保 `build/f5/track11-terminal-owner-review01/report.md`，SHA256 `61bbda3de31851096a6b5fb930d4a6c69380dec4b07eaa7bbc4a86dfb6f4de90`。原N=3152、Q=3156，实际payload extent A保守为3152..3163：若实际A=N，`433b0`在资源owner外；split虽在同址写下一free header，也不使它变成资源payload。若A>N，位置在slack，本次fread／锁N+1／TIMB scratch／S初始化链均未赋该尾，但仍未排除适用的历史／opaque／平台来源。故不能仅凭本allocation epoch未写就统一判U或猜0，旧unknown保持。19完整函数／20chunk／7271B逐byte核同strictimage，11输入晚守卫同；无新IDA／C／原CPU／窗口。此后仅在A=N的实际来源闭合，或A>N的合法保留／无来源闭包取得后，才扩签相应条件，不能靠改变U/V定义取消正常11终态门。

### 全部真实样本的reader／DIG有限核验

独立构建的一次实际C为`F:/fd2sdl-evidence/f5-pcm-real-assets-C-20261117-01/report.json`，SHA256 `101261d400a4e4124f1baae5b8b5dc1f1770a39f3b05738e6be9ed797a1a6497`。26库／111绑定／78唯一payload，各child分别走P0／P1，再加真实双P正负clip、不等长、stop8→replace及loop3四段，共226 histories、1556 services、2013完整packet，比较24920940B，其中完整build／两DMA为24735744B，全部相同。载入raw回传与独立工具同份冻结输入逐byte同；每次play的实际span也比较，不用相同payload hash掩盖别名绑定错误。PID46048正常exit0／ended，285个原资源／source／SDK／PE／实际编译依赖事实守卫相同；没有旧默认／22-vector、原CPU、GUI或设备运行。

期望独立来自原完整40240／404c0／49690等正常路径与每片段`2R−1`闭式；C走真实archive／bank／play adapter、同一DIG和两个view，逐服务比较整个build、两个DMA、P状态及provider调用。给定half是测试输入，不是实测IRQ。51/3是有效诊断样本，其固定正常绑定排除证据保持；其余110项也不因资源全过而变成自然cue或最终波形通过。

父侧应用`pcm-real-assets-candidate02`六个test／tool／注释文件，无新shipping API或CMake owner。`build/f5/pcm-real-assets-root-check01/receipt.json`记录本树目标构建1.734s及18项最终纯协议检查通过0.156s；此接入收据没有再次运行226段或旧22向量。原一次实际C属于其独立PE；两处工具修正仅加强真实resource目录保护和cleanup异常分账，模型／请求／packet的11定义AST及probe字节相同，继承边界见候选02。不能把新工具或新root PE写成同一次actual。随后补齐的portable原字节来源与实际producer资格见下；旧本地JSON来源包及v1收据不改签。

公开入口现为 [PCM真实资源复跑](../../tools/testing/PCM_ASSETS.md)：新增显式portable-bytes，从固定原EXE复用既有双LE验证／重建取得相同406738B镜像，核30来源chunk及111样本span；三份旧本地source包的语义身份仍明确继承，不再要求其机器路径／mtime。固定MSVC producer采集实际CL／LINK／Lib闭包并重新采集验证，拒空files、假计数、缺实际依赖或晚于PE的已编译输入，明确 `compiler_consumed_hash_claim=false`。Linux producer尚未适配／未验，不冒Windows资格。

`build/f5/pcm-portable-root-check01/receipt.json`（SHA256 `5984e41e79fe5f98c5ca3e05acdd761f9946297306bb15731f7efca4d86cbb96`）记录完整候选02只应用一次、21＋8＋18项纯检查通过，实际只读289输入／23库／四project target，fresh原字节／全部绑定和晚守卫相同。原junction路径门的真实缺口已修：同时resolve声明root与工具root的original_game，再比较resolved输出，保持独占创建；owned真实junction负例未触碰原件。没有新编译、PE执行、设备／GUI或旧226段重跑；数值证据继续绑定原独立PE，当前collector不补造编译时hash观察。

### 新观察器一次正常启动的失败与可用前缀

修订02只补两项证据完整性：全部观察入口的non-normal污染，以及raw-v2从Entry／真实离开／RestoreCandidate／NextFetch重建闭合并核header；Entry记录本来已有的return_eip实参，不增加guest读取。新host为`bd8a6844515171f96a1b6113cb47ae638c34404837317c4ce878da2bf7430d20`，10组新增宿主POD／24项decoder检查及3865实际构建输入守卫通过，旧15组未重跑。独立复审`host-observer-revision02-peer/report.md`未见旧两项遗留或新增运行阻塞；这些仍是host／静态资格，不冒guest通过。

唯一实际新host在`F:/fd2sdl-evidence/f5-host-observer-normal-title-20261117-01/report.json`，SHA256 `b0f83be8b5c641646bdf0ec72c4a8ce564c3ba265b3a42aa4f8685d1fb688306`，**overall FAIL**。未投业务键，45s的标题菜单资源ACK超时；实际WGC为FlameDragon2 logo。原首失败和expected门保留，不把logo当菜单通过，也不增加容量／循环重试。PID24848正常exit0、未强清；Job32516为worker1／timeout0／residual0／forced0／cleaned1。内外strict ON／Default桌面、原资源／真实用户SAV／源码／实际PE守卫及同线程ES释放均核实，活动电源方案未改。外层收据SHA256 `2b612ec71f38c732367a5d0d9935b264d8c3d17106e885e763efdd0118fd8017`。

atexit关闭的raw为17301648B，32768记录已满，26个入口中25个闭合、末26未闭，overflow1、VGA8355。父只读严格decoder另输出`F:/fd2sdl-evidence/f5-host-observer-normal-title-analysis-20261117-01/receipt.json`，SHA256 `02cda67d4385ca40bfa411812cfd0e3917c8c3a089153d8688c8c875d9ea3f45`；全部header／record计数一致，但transport／整场完整门仍false。没有裁掉末项或把25前缀改签整场通过。这25条只是该独立host的部分上下文恢复候选与预算事实，不证明最早返回、command502、OPL／PIT、canonical映射或旧a82a时刻。

只读`build/f5/host-observer-stutter-review01/report.md`进一步确认，连续同pair／kind／helper及两快照全字节相同的run为0，简单无损RLE节省0条。末pair占25869条／78.945923%，队列／callback／RunMachine交错中的预算与上下文字段确实变化；不能压掉不同kind来帮通过。4659对queue合并、3579对callback写集与固定host源码关系相符，但仍不赋予MDI命令身份。下一步须按真正需要的事务取源／观察，不能扩大旧32K日志或由这次失败推定SDL逻辑错误。

### 启动失败的画面身份与下一观察范围

只读复核 `build/f5/host-observer-title-postmortem01/report.md`（SHA256 `2b094f669748b310824062ada74d2567b727479d13a8526dcbd21f207880ea7b`）将 timeout WGC 与既有独立原 ANI1 第33帧逐像素连接：64000 RGB 像素差数为0，按原有序mask／page及palette8 DAC展开得到，未用截图反造expected。该ANI的首帧完整写满页，不借未知入场页。`original-receipt-join.json`另将全部51个已封资源叶逐字典连接到原full report，未重跑原资源叶。精确画面只确认合法菜单前缀，不证明当前PC、持续卡住或原执行时长，也不改变整场FAIL／overflow。

完整 `1f894` 的滚动和 `20421` 的51帧循环都有有限退出，普通键只提前跳过；菜单后的阻塞读不能解释动画第33帧。旧工具默认自动ESC跳过，本次明确 `startup_skip=False`，因此旧45s成功不能继承为本次无跳过的wall-clock上界。不据此增加旧预算或重试同一入口。

后续只选择正常固定安装后一个非空accepted502事务，方案为 `build/f5/mdi-transaction-observation-plan01/plan.md`（SHA256 `c7d1bee004bdeb3c8e3b0c316e7a5075c3582d37c11d0939f0663bef82874ecf`）。先以正常300接收／分派／返回链绑定当轮driver owner，再逐站确认502真正进入3ed8及同caller NextFetch，不能拿INT31的AX0300或低PC单点代替。OPL真实端口／FillUp和PIT requested／effective分层记录；保持32768记录／256事务上限，缺站、污染、open或overflow都不通过。该方案只继承固定正常文件及安装闭包，未证明任意加载代码身份、最早返回、a82a绝对时刻或完整F5。

### 首个真实写OPL的accepted502：一次新正常取证

revision03只扩大独立观察器的来源识别和记录：完整300接收／返回后才绑定当轮owner，重装／306失效；保此前真实零端口502，预先选择首个真正OPL data OUT所在的已接收502，直到同caller NextFetch闭合才停logger。OPL PortRead仍恰一次，PIT requested只记PIT0 handler完成状态，effective独立记实际set_active_counter。新raw-v3保持32768记录／256事务，未改原a82a允许表。`host-observer-revision03/receipt.json` SHA256 `c190e8d92394af41a60cb50e5e7fe62aa5ccbd7be768210e7e0a2cbb50cb8d32`记录build08及3864实际输入／64tlog，14组POD、2组失效和27项decoder检查分别保身份。独立 `host-observer-revision03-peer-review01/report.md`核原语句恢复、绑定／失效、OPL／PIT点和records重建，无运行阻断。父先前漏读PIT caller过滤的疑点已撤回，实际过滤已在sealed03内，没有因此改源／重建。

新入口14项pure及4004来源prepare通过后，父只运行一次 `F:/fd2sdl-evidence/f5-host-observer-normal-title-20261117-02/report.json`（SHA256 `c13f4564667729f61190dc5d1683ebadf62233e2b70a5d6ea04e9ee1082bc8de`），结果PASS。原START仅copy到隔离FD2.SAV；4个普通ESC只来自已授权的实际startup wait，同PID／HWND完整delivery核同。首菜单资源ACK后封口，第二ACK不再投键；完整Title图已复看为正常三项菜单。没有选择START／CONTINUE或写用户档。新host PE为 `1c929fa57fdd5fddbec09016c8bd754a0247e1d6d50c19896fa1a6e70c11bfc8`；a82a仅未启动的配置control，不能将新结果改签旧原DRO时间。

raw共7717记录／4074848B（SHA256 `64254eb381c995a484d2cd9c6fd5a39148a7ba5206d1a0d3ff76fd40834d722c`）：27入口全部闭合，1个完整binding、17个selected和9个excluded；nested／invalid／open／overflow／selection_error均0。第27包为预声明首个真实data OUT目标，含104个寄存器数据写、1456次读和1560对FillUp点，完整端口序与每段预算交由来源分析，不能只靠数量签完整writer。该包内另记录3次TickBefore；PIT0全局requested9／effective8保各自实际位置。decoder输出15979560B，SHA256 `465316e9a8d1ee487fee2248bded1105da0713073000ae2e490dbc54455a2c65`；无需再次运行decoder或重录窗口。

原进程PID1248正常exit0／未强清；Job12872为worker0、timeout0／residual0／forced0、cleaned1。外层开始OFF，依既有授权一次零位移idle reset后console／session ON、指针未动，再过内层strict ON／Default和实际WGC门；内外ES分别在43436／26948原线程释放，电源方案及用户混音器未改。所有原件／实际源码／host／SDK／真实用户SAV守卫同。外层收据SHA256 `3e5506dbe501dd66ec8c8722ad4e3a7d3b4170ef3f9d9c2b604612b9eeea5f41`，父只读join `build/f5/accepted502-native02-join01/receipt.json` SHA256 `d5434a056423282fe1a5ae58581a7f1197fde4b5f13d79fa6ac76e3ffd9c5432`。此前Title01失败／溢出原件不改。

这次只签固定正常安装源条件下，一个含真实OPL写的502完整观察，未签新运行完整driver／gate字节身份、MDI timer caller归属、earliest return、a82a绝对时刻、全部音乐／PCM波形或F5完成。实际PIT.output与PIC input／edge不能互相代替，当前日志缺后者，不从观察到的高字节即时pending实例生成通用首相位。

### 当前公共入口与新原版104写的内容联结

新原侧包的独立只读复核为`build/f5/accepted502-writer-review01/report.md`，SHA256 `600de0e41d3431b9ba82f8b2f0a4488c7b7d754a797371d2c2ea89ec69cab687`。104组全部为`address OUT→6 IN→data OUT→8 IN`，共208 OUT／1456 IN／1560 FillUp。1445个没有host记录的相邻IO seam满足原普通取指与一般IO扣量；另外115个保留实际Queue／Event／Tick／Slice次序，不套无切片公式。附加读扣量中77次为负，1次为0，27次为1..8；1560对FillUp只有已记录Snapshot相同，不能据此判未混音。18个无host记录区间的端点仅为address OUT结束到末IN结束；不是完整writer无slice，其中5组预算差也不是467。第37组虽无EventBefore，仍跨tick。原观察没有mixer工作页／DMA／样本计数或timer caller身份。

本轮另加`bgm_first_packet_native_content`，以原104项的`uint16LE register＋uint8 value`短指纹`0813266f04ed078a`作独立expected。实际当前公共`create/play(18)`、单S／512B包、共享MDI／真实DIG、Audio及Nuked全部参与；只生成首个有硬件写的有界前缀，没有重跑旧24秒或全曲。`build/f5/first-packet-native-content-check02/receipt.json` SHA256 `91a2464a5f75c95f702991ba0260146d6a522537210e43e4d492b74a5bb76e96`记录：一个新目标构建1.39s，新CTest通过0.31s／总0.33s，104写的值／顺序全同；当前C包32条消息／85B、sequence tick1，H离线推进11ms。421项实际CL／LINK／Lib／SDK等输入、24库及独立来源／用户SAV前后守卫相同；PE为`21dd192aba2d6d970983baebbd24a23a3fb6727b0df4a32b2de47f21306a47ff`。未开设备／窗口、未重建App，正式App仍`6cf0b69f…40afb`。实际读取身份不声称编译时观测全部input hash。

首轮只在测试TU的PCM C/C++声明链接失败，没有执行PE；`first-packet-native-content-check01`保原日志和修前test。随后仅在该TU正确声明C linkage并修复未引用helper告警，不改原expected、BGM或资源。`accepted502-vs-legacy-register01/result.json`另有旧`cc84c299`C trace的首104内容相同，只作历史辅助；不能用它替代本次当前C检查。上述新联结不证明原包就是Title18 timer callback、不签原／C同刻、首相位、完整消息producer或PCM波形，原严格DRO RED仍在。

### source成本与唯一clock的接线依据

独立 `build/f5/cpu-budget-candidate01/`已用固定官方normal／PIC／I/O源码验证借用budget的小算式：3782个新独立整数／Fraction输入和8参数检查实际C相同，含失败while后减、unsigned tick回绕、真实signed中间溢出前缀和负remaining的OPL额外credit。它没有接入root，不采时、不持有第二clock或执行器；有理坐标也不代原double的cast／除／加。该候选不能签原回调时长或3599严格时刻。

`build/f5/clock-source-integration-plan01/report.md`列同C栈／同clock的具体接点和63个observe读点：保原S／voice／512B包，source成本在原读写与端口边界同步到达；host设备事件与guest IRQ可交付性分开，不先算完整callback后排未来OPL。当前瞬时callback、固定tick／END tie和完成调用后送SB IRQ仍是H。IO扣负必须先完成该指令的handler／FillUp／OPL额外量，再到真实while退出／RunQueue；原PIC标量可以暂回退，Mixer只保已render前缀，不能用单调时间钳预算。真实bridge／初始phase、IF／pending、FP队列／设备源顺序仍须补齐；外部离线固定N输出与live调用的horizon合同也须分别闭合，不靠一个已过数学叶或新增拒门签完整F5。

真实writer消费者的独立候选在 `build/f5/mdi-writer-consumer-candidate01/`：从原37B／20head独立展开56分派，以同一借用budget执行2OUT／14IN／15个FillUp接点，在data OUT点实际写Nuked。24例／1160有序事件实际C相同，含未写与已写后的失败前缀；183实际编译输入前后守卫同。消费者／原expected不变，首轮actual01失败来自fault107被测试替身过宽条件误归首FillUp，原失败保留。该prototype未应用root；Audio端是逐点记录budget coordinate的替身，尚未投影真实original_point，不能叫timed PCM。无slice／event条件和NEEDS_OUTER_CONTINUATION是它的能力边界，不得变成正式V域拒门；真实外层／IF／bridge／phase仍待接。

之后的新`mdi-writer-source-continuation01`已完成**真实合流**，不再以无slice预检拒绝续行：唯一CPU／PIT／AIL，实际同C栈source writer、Audio／SB／channel／共同DC及Nuked逐点相接。`join-run01/receipt.json` SHA256 `d28a9860025115164f81150d004d1d6f656ec00723010f024bb48533c5537da2`记录10条显式funded／IF0／无外部事件历史、1030有序12字段全部通过，新CTest0.02s；391实际CL／LINK／Lib输入、22库及前后来源／用户SAV守卫相同，PE `a94a1ab4f8716873077a4e3c17c7b59ca9d2f85410bde47b387bd873f460749d`。零remaining的normal入口与while失败分开，跨毫秒仍在当前C栈继续，14次IN负credit可回退而已提交声音不回滚。原writer0的16个IO Begin tuple另独立相同；该例未观测entry／tail是显式合成输入，不被升级为原测。这里只写105／20等未key-on寄存器和零DIG输入，不能签有声PCM、完整502／自然calendar或Main成本。候选尚未应用正式App。

准备入口现强核两组件PASS、实际输入守卫及当前源码：clock qualification `aca1adfd3d612ee40df51170f756bb20efc9afc6863378951e90ccc98b866c59`和Audio final `e49c0467…96d31347`分别逐after字节／大小／mtime连接，并把一次pin的原bytes直接复制、核实际副本。首次准备因原`original_game`逻辑路径与既有junction解析路径不同而在建目录前停止；全四输入的字节／mtime未变，旧脚本／失败收据保留。后续只将声明路径解析为同一物理目标，原声明／解析身份都保留，不放宽内容门。没有C、原版或GUI失败被改签。

AIL IRQ的执行顺序另以同一个内部算法同时服务普通数值／source入口，避免另存timer：`ail-irq-source-candidate01/actual01/receipt.json` SHA256 `9b57573d85aaf21603ac9a3b0871ce913b3cf3263983543ddb5400771dbe5449`，10条新源路径／1660原PC访问及8条共享数值投影通过一个新CTest0.01s／总0.03s，163实际input／14库首尾同。EOI OUT前收一般IO、STI／最终IRET在原点恢复准入，source错误保已到busy／pending而不补假退出。该层要求外部旧BIOS链provider；本测试给定其3grant有限效果，**没有**据此资格原CPU IRQ入口、BIOS链本体／BDA真实写时刻或完整IRQ时长。尚未应用root，完整handler仍须接其合法源provider。

外部确定性工具另有 `clock-source-external-addendum01/report.md`：旧固定N缓冲不能在callback中途丢C栈，或隐式多产未来PCM。新严格入口须明确允许有限source调用完成越过request-target，并另给足够external horizon与同步sink，报告实际完成点、最大已到FillUp及extra frames；不借SDL补时，不建OPL FIFO。这里只确定接口和所需证据，未把方案标成已实现。

旧bound runtime的桥入口已有独立定位：`dpmi0300-module-location01/report.md` SHA256 `57186776c5bfc30315f5884af07f6311fb5074aefdc96af9ce543ba815605da9`由旧route14 capsule／descriptor及五份旧RAM各22B的保存表连接到selector70:448e，file映射为df6e，入口96B原字节相同。全view仍有109B未统一资格的runtime差异；这是应用LE canonical之外的模块，不能套VA−10000或仅按新raw同值宣称fresh加载身份。后续只补正常0300的必要静态运输／IF／返回来源，不以DOSBox内置同名服务替代。

随后正常0300窄源已从既有capsule以隔离Domain取得并连接：`dpmi0300-source-join01/report.md` SHA256 `4875036207f02f2fd055defbbff546546d9232c5c4d7dfe593300506c59cc66c`，181个唯一head；源中间形成INT66的原写法仅作参数／成本来源，没有修改capsule、原guest或可执行字节。对应`dpmi0300-cost-contract01/contract02.json` SHA256 `1921be6dd2381578f7bf9f09156e51252c7ce25877176de06bc11c53b4a0b15d`保存43片段／417行／10 string sites／8 wrapper IO sites，16个显式条件组合及192个真实CALL／RET连接检查。它们是source-given普通取指与元素分母，排除driver、IO附加扣量和调度，不是“153cycles桥”或测时。prefix、唯一driver体、suffix按原调用穿插；共享defer持续到最终PM IRET之后的3f1e0才减少，不能提前排空pending。WAIT在固定normal core只收一grant；两处裸STOSW／MOVSW仍各收元素费，IRETD强制出口不凭空补失败while。

`dpmi0300-integration-review01/report.md` SHA256 `02ad802c3c9a4455bc69cbf3dfd05604f7e509d76f949666b13e73f0b1bfe331`保13个当前源snapshot、12个另外输入的晚守卫及具体接点。源clock给raw PIC point，Audio只保已render高水位；SB的service_index须用srv_lag分量，不能将含整tick的CurrentEventTime再加一次tick。11f0／word35／A20关系、SS／PG／mode与当前加载身份仍分别资格，不能从source初值或一次运行补成通用参数。包外App同样消耗原预算；当前正常包的局部tuple可作独立给定输入，不能每IRQ重新塞同一捕获常量冒全App时序。

PIT0 mode3的隔离资格先保`pit0-mode3-candidate01`，随后已按下文共享合流取入当前工作树：24条给定历史／65 checkpoint／134有序effect，实际链接当前AIL的fresh raw0→MDI120 raw9943→DIG200 raw5966 consumer通过一个新CTest0.02s，169实际input／14库／25 SDK前后同。它不另持BDA、CPU预算、事件队列或wall clock。原43/36、40低／高三点分别作用；高字节可立即改变PIT输出／PIC pending，不能以当前`cursor+divisor`一次重锚代替。新原观察seq420→422→423正好给出output低→高且IRR bit0变1的实例；PIC input／edge与PIT stored output仍是不同状态，未观测部分不倒推。两个binder源视图／head数拒绝均在C前停止并保原件，actual首轮通过；该候选本身不签完整PIC／原相位。

早先源审确认独立实现差异：原版按请求顺序注册192个物理音色cache、4096B heap、drum lazy-cache、保护位及时间戳，当时实现按256个归一key预解全库，未执行该注册运输。既有104输出相同不证明lookup物理slot／扫描成本或全部warm写集相同。`mdi-driver-source-candidate01/inventory01.json` SHA256 `d91c5869d34ce1065d74ad397101b48684b29682e82107b2ff90d7a56d650154`只读实际资源确认162个唯一key、全14B合2268B，15个FORM曲TIMB请求全部在其中；169只是C可编码key上界。现已按下文共享合流在同instrument owner补cold bank／真实runtime cache，不按sorted或每曲fresh，也不增平行热选择。容量盘点、局部register匹配或Nuked例外仍不代签503完整成本／全暖态。

### 首writer前的常驻chip与失败借用撤销

按同clock／同C栈方案复审发现，原Create在全部初始化writer／control和timer安装后才注册chip。现已拆开driver／OPL Reset与492个初始化writer，在Reset后、首writer前由Audio持有同一chip；不创建第二chip或sequence。原无时间成本的夹具仍复用完整reset组合，不据此声称过去已听波形错误或原首相位现已通过。

新增 `fd2_audio_abandon_source`仅为安装失败撤销借用：以render／retire／userdata三元身份移除，DROPPED退休，不advance、不依赖失败的clock，也不清首错。普通stop／曲目fade合同不变。BGM在source投递失败、全槽DROPPED、同步错误或MDI timer无空槽时先撤销借用再释放player；不再为此调用可能在clock错误前就返回的整bus同步stop。Create失败路径没有有效新timer可释放，已删该不可达清理分支。

`build/f5/startup-renderer-apply01/receipt.json`（SHA256 `c36207e9dfb568779eeaa24e3a1191c6a1ea212a5fcb06a86bb5a4fb5c3f1b53`）记录App／两个直接目标重建及两项新CTest通过1.95s。随后补全timer池失败和render身份不匹配例、删上述死分支，仅复建这三个目标／复跑同两项：`startup-renderer-followup02/receipt.json` SHA256 `8ddad086ae15d0e49c128bade316ff958dfecafb190b06d52c22908e6abe7643`，两项通过1.14s，来源／原资源／SDK／真实用户SAV与测试PE前后守卫同。未重跑七项旧BGM、全部PCM、设备或GUI。显式离线96帧在首writer前确认实际Nuked已能推进，不能把该2ms测试输入当原耗时。新App为 `6cf0b69f36e33f7c638d04a4740f03af073702d8d17e6a14b8babb5355f40afb`；旧5c602 App／PDB已按字节与mtime保 `F:/fd2sdl-evidence/f5-before-startup-source-20261117-01`，旧运行不改签。

`build/f5/startup-renderer-peer01/report.md`（SHA256 `94e30244fb5cfce697756e0f8ad7ca5d2fc06f158b42aa5d27a854dbdbbf84c0`）未发现当前Main正常路径新增生命周期阻塞，20输入首尾同。随后 `startup-renderer-original-profile03/receipt.json` SHA256 `9c1f4522135e2b68aab19facfcd8d799883100e9731c75d482105d1a1b6f3a8f`补正式original_mixer=1的上述三个条件；仅BGM目标／一项CTest通过1.13s，未重跑Audio生命周期或设备，App仍为6cf0b69f。abandon可关闭已到partial并按当前H政策交付pending，不承诺完全零副作用；一般终态仍须close clock→AudioDestroy→BgmDestroy。显式96帧只证明该profile的借用／可渲染边界，不是原首phase、真实端口FillUp时刻或严格PIC。

### 保留Nuked的例外身份与归因

`EX-AUDIO-NUKED`沿用户2026-11-14的选择保留，不换后端、不补偿增益或新增音色测量。实际vendored头声明Nuked OPL3 1.8；来源README登记libADLMIDI commit `9760df4e0acfe27818dee38e786f3b1fea8c8e7f`，本轮只核本地文件，没有重新fetch上游或冒称本轮证明其逐byte同上游。固定文件身份为：

- `src/third_party/nukedopl3.c`：`add31fcc6138e3590fac0a33b8cf66720b8e89594a861b7f1c262e7fb7d372c2`。
- `src/third_party/nukedopl3.h`：`ab9fcea7a32b0e7b37150de5cf5ffec2e2c71de6d48e043362032d171c8ae00c`。
- `src/third_party/model_ail.c`：`8d7a27dc97eb39a9bfc8a65e532e8b069b59090c9f66d027be9c27e89c2d081d`。
- `src/third_party/opl_models.h`：`ba9035f421d6b85277fb4a6ce9ea33a496c1a84d4283254021473aaf52500b10`。

`build/f5/nuked-exception-identity02/receipt.json`（SHA256 `80771cbe48b0316cf7111757d1f9cd0ba62f25ee11a4e464564e6c1467f0b485`）只读连接实际core CL command、object、当前App和来源／许可：Nuked TU无相应宏或forced include覆盖，头默认 `OPL_ENABLE_STEREOEXT=0`／`OPL_FAST_WAVEGEN=1`；正式chip Reset使用48000，GenerateResampled给S16，按S16/32768原载体合流，不加原DOSBox公共scale1.5。旧标题约2.1倍测量仍只绑定其旧素材／窗口。最初记录脚本误用默认build输出路径，在读取不存在PE时停止，未执行程序；失败脚本保 `nuked-exception-identity01/attempt-script.py`，随后按实际release-target清单取路径才生成本收据，没有重建或改写旧证据。

例外限于所选合成器的原始响度／音色及仅由该FM输入差异引起的混合clip变化。事件内容／时间、DIG／SB转换和gain、共同DC公式与更新次序、量化和owner清理仍需等值；不得把整个混合WAV的任何误差直接归例外。共同DC保留输入历史，因此不能在BGM叠加后只靠最终PCM差值定位原因：验收须先比较相同FM／SB输入下的DSP各层，再将已选择的FM载体差异单列传播，不能借例外豁免算法或未知暖态。该身份登记关闭披露子项，不签A17／A18或整个F5。

### 自有观察器的精确符号元数据补证

在旧 PDB 全表枚举失败后，没有重放 `symbols.Next`／line 查询。另以封存自有 PE 的只读 Exception Directory 得到一个两锚候选，再用同一官方 DIA 的两个精确 `findSymbolByRVA` 查询成功读取元数据：`build/f5/observer-direct-symbols01/receipt.json` SHA256 `659457473f51aa2196ecf617e87519297a44b2b889ac2baf57f149315ab7dfbd`。PDB GUID／age 与既有 PE RSDS 匹配；RVA 9926512／9699104 分别为 `VGA_DrawSingleLine`／`DMA_DAC_Event`。没有执行新 host／guest／GUI，实际守卫前后同；两次元数据派发前的编译／日志编码准备失败均保留。

这关闭的是上述具体 PDB 元数据缺口。推导 base 只在完整 unwind entry 集合中唯一，非完整 function-entry 集合或独立 runtime 测量，因此两个 raw Event owner 仍有映射条件，不能无条件签全日历。候选若最终获证，`DMA_DAC_Event` 的逐样本 DMA／channel／IRQ 作用需要实际接入；不能以 Title 无显式 SFX 为由忽略 304 probe 或其它合法暖态。详见同目录 `report.md`；旧失败报告不改签。

### 同一时钟的失败前缀、IRQ 与离线输出

普通 writer 的第二次候选去掉了临时 `MdiSourceWriter` 中重复的 clock／首错字段，只借 `AilDriver::source_clock` 及显式 IO 参数。`normal_write_reg` 遇源错立即沿共同 `BgmSourceAbort` 展开 C++ 栈，C timer／bridge 边界再转为同 clock 负错误，不能跨 C 抛异常或继续上层字段写入。`build/f5/mdi-writer-source-continuation02/join-run01/receipt.json`（SHA256 `7c4c6081db1bb085ef3c1497d768102d49882963154026e7c945e2b840c926cd`）记录两个新实际失败族通过：非法寄存器在首取指前停止；已完成真实 OPL data 写后，horizon 不足仍保前缀、停止上层 suffix，之后不能再写。十条既有 writer 历史未重放。这仍是给定入口的真实 Audio／SB／DC／Nuked 组合，不是 Main 初相位或完整 502。

AIL IRQ 候选与原数值入口共用 `timer_irq_internal`，按原读写点收普通 head／EOI／STI／pending 与返回成本。`build/f5/ail-irq-source-candidate01/actual01/receipt.json`（SHA256 `9b57573d85aaf21603ac9a3b0871ce913b3cf3263983543ddb5400771dbe5449`）记录十条源路径、1660 个原 PC 访问与八项既有数值投影；另一个新 nested 检查 `nested01/receipt.json`（SHA256 `4f92dc4d1bdf5c6cc1dd32b07b3975f207d3a92c6c7cc365517412871180105f`）实际核对 STI 后一条 CMP、同 owner busy IRQ、EOI／IRET 后回到原分支，共 204 个 PC。给定 BIOS callee 的三 grant 不是原 BIOS／IRQ entry 运输资格。接 PIC hook 时必须把 timer 的正常 `1`／busy `0` 统一转 hook 成功 `0`，负错误原样传递，不能重复 EOI。

显式离线 source 流候选为同一次同步调用临时借用输出 sink。调用者先为唯一 clock 提供足够 horizon；源调用允许超过原请求点，返回真实完成坐标及已生成／已交付帧，不以固定 N 截断 C 栈，不缓存未来 OPL 或 PCM。sink 失败只记输出首错、停止后续交付，源仍完整返回或沿其独立首错展开；活动 borrow 期间不能 destroy 同 Audio，非法最终 point 不冒用旧值。`build/f5/audio-source-stream-candidate01/qualification.json` SHA256 `6cd65e6394be62482eb944ba6df0a9c059e6b0367236fc48c54be1f32c035f92`，实际 `join-run04/receipt.json` SHA256 `b8678a8c3dc4b6af9e645fda14cec1716d4ea91fec16466f74c47a1de0426504`：五个新组合通过一个 CTest 0.03s；无设备、GUI 或旧十条重放。源 tuple 给 PIC index `.9955`，独立推出首窗 47＋1 帧，而不是从新 C 生成 expected。第一轮把该入口误当 index0，以及第三轮误写已有 validator 的错误分类，两份失败及修订均保留；不改实现凑 oracle。

上述 source-clock／IRQ／流接口随后已合入当前工作树；实际 Main 仍采用完成调用 H 政策，没有完整 SOURCE 构造／控制／调度资格。未闭的 503 注册成本、DIG timed body、全日历和首相位继续待补，不改变下面的完整 F5 完成门。

### 当前工作树的共享合流与定向检查

`build/f5/coherent-source-integration01/apply-receipt01.json` SHA256 `92d6e297f11a9ee6078d288d073bfa5d466cbe969b52712aa50aac10905a4416` 记录 36 项封存源／冷输入的实际取入：同一个 AilDriver、S、512B 包和时钟借用；`actual_driver_batch` 只接唯一 raw 分派器，不再保留搬出的旧解析循环；FD2 原读取时点捕获 count／cursor，在原 PC 清尾。2aa4／2abe 与 writer02 的 2ad7 起点不重收费。公共 AIL wrapper、source IRQ 与 Audio source／流接点共用同核。物理音色 cache 保原 TIMB 次序、跨曲记录、drum／保护与时间戳；503 的功能性注册已实现，完整源成本仍缺，strict 入口的显名缺口不是原 V 域失败。

普通构建复用一个 `fd2_static_data` 依赖，以同一生成器的两个固定 schema 生成 raw driver 与 FD2 head 表；不依赖原资源或 ignored Domain JSON。两个 schema 的 18 项 root 纯检查通过，收据 `generator-check01/receipt.json` SHA256 `c8f6e091298cee0b923e915944e42097c4d72f0368873d43dcedd6c409b7c9e3`。源证明与生成器自检仍分账。

十个直接目标实际构建通过：`build01/receipt.json` SHA256 `cdc5bad8e75b9168d893843656a193aed39dbd637b1cfbf914c384eb506b832d`，该轮 App 为 `9bfa0cbf138e5f88837522be57d6890c9be4de2966d7503afd11270d063be84c`。旧 6cf App／PDB 已按完整字节及 mtime 保在 `F:/fd2sdl-evidence/f5-before-coherent-source-20261117-01/`。独立静态合流复审 `build/f5/coherent-source-integration-peer01/report.md` SHA256 `afbf81fb4dccbc3a89b970ae2f14c1e1b39cf18fe1e3df5eff1001a8394343b0` 未发现新增产品错误，具体投影／费用边界保留，不扩签 Main。

首轮九项 CTest 有八项通过，`bgm_native_sequence` 在旧私有 fixture 中空 S.driver 崩溃；`check01/receipt.json` SHA256 `cbe12efb07fd46623b574962a4f6776c6332ac877d8e851b3072c6fc9c138b71` 保存失败、735 个实际 CL／LINK／PE 输入及执行前后守卫。一次 owned CDB 只读异常／栈诊断定位到两个仅测试 S 字段的夹具；补其真实同 D 借用，所有原消息／状态／beat goldens不变，没有给产品增加 null fallback 或缩小原域。只重建／复测该失败目标后通过：`native-sequence-fixture-fix02/receipt.json` SHA256 `b6bc5ee42ef730ec2c282807dd3a003c632bcfff9c25a643889b6ea06d4e2f04`，先前八项未重复。失败 PE／PDB、旧 fixture 和 CDB 原件均保存；CDB Job 正常清理、来源／资源／用户档未变。此后没有新增原版／SDL 窗口、设备或音量设置改动。

随后 `build/f5/control-factory-root-integration02/apply-receipt.json` 实际取入 14 文件：同 timer 的 source 控制及公共 wrapper、Audio source 专用构造、真正全链消费者和四个测试目标。中断恢复已核收据及物理文件，未再次 apply。source 构造在同一 owner 发布前绑定显式 clock，不重置 BIOS／timer／PIT、不借旧 prefix 推进；重入 create、无效 point 和失败关闭均按自身生命周期拒绝／清理。timer 控制保持原三次 PIT 写入及失败前缀，低字节后出错不会伪执行高字节或清 pending。

八个直接目标实际构建通过，`build01/receipt.json` SHA256 `db9b958abb340a5daddc6988ef164921252978b2a856cb8fabfaeb0997623342`；该轮 App 为 `f11eeda053fd76360739c7149703f278bffb7a80de1c077a82c73b49e29db909`。此前 9bfa App／PDB 已按字节和 mtime 封存在 `F:/fd2sdl-evidence/f5-before-control-factory-20261117-01/`。新四项及直接受影响的 BIOS／Audio 旧三项共 7 CTest 一轮通过，0.17 秒；`check01/receipt.json` SHA256 `48f93f4468a3b16bc98989df866758eeb0591e6bc18da82cb1af2566569fe57b`，684 项实际 CL／LINK／PE 输入和源／原件／用户档前后同。未重跑已过旧大批次、原版窗口或设备实验。

新增合流测试从给定合法音乐 before，经同 timer IRQ、真实 AIL／0300、唯一 raw 分派及 writer，达到真实 Audio／Nuked／流接点；成功及末段故障分别核原 104 写、32 条／85 字节消息及真实 3 次 Mix。消息内容来自独立 FDMUS 原字节，104 写另来自已有原生观察，不由待验 C 反生 expected。给定 before 由旧完成调用路径建立，因此不签正常 SOURCE 启动、暖态／首相位或全 Main。source 错误冻结前缀也不等于成功执行原 remove。

以上只是当前 F5 工作树集成，尚未提交或集成本地主线。命名输入已修正 382e9 的 `AIL_call_driver` 误名并补七个公共 timer wrapper，生成发布待本阶段统一刷新；不能据旧清单仍自洽声称新命名已发布。完整 SOURCE owner／正式 caller 错误传播、503 成本、真实 PCM 层取证和下列完成门继续执行。

### PCM 暖态与新被动层观察的待验边界

固定 host 的 `DMA_DAC_Event` 每次实际 DMA Read 后先把一个 mono／stereo 点送入 channel，再饱和扣减 left；它与 `GenerateDMASound` 的分支不同。因此旧报告对后者的零帧结论不能扩为全部 304 probe 都没有暖态。是否走 DAC mode、首提交读取的完整性和实际 SRC／LP 初态仍须正常运行证据，不填 fresh0。既有两个 master WAV 含 FM，不能按其差异直接判 PCM gain，也不能用 HOST_W 投递 QPC 反推 sample0。

`build/f5/pcm-layer-observer-candidate01/` 已封新独立 host、普通 Release 构建及离线检查；PE 为 `ef539ff91348a1a6c91ad6a1eebefc96dc432c8657cc7da166b88db3a0bd57f0`。它在原有操作中被动保存 actual ConfigApplied、首个真实提交及固定首非128选择点、实际 SRC／LP／声道加数、DC／量化和同次 WAV offset。容量固定且不延长游戏等待，不更换旧 reference 身份。独立复审 `build/f5/pcm-layer-observer-peer01/report01.md` SHA256 `35b352f7005b7a29da578d34fd9ee0675889e824c06bdc838ac262ae0adc7e40` 重核四源移除插入后与官方字节相等，未见额外 guest／IO／PIC／FP／FillUp 操作；增加 host 开销仍不能称零墙钟扰动。

复审发现 runner 先 hash 后另读 JSON／原始输入，解析字节未与已核身份绑定。新 `runner-revision02` 只修同份 frozen bytes／stat／SHA／解析和实际 raw／WAV／log 报告绑定，旧 host／decoder／收据不变；八个新协议检查通过。首次 helper 缺 path 的失败及补丁 CRLF 物理字节失败均保留，不改 expected，也没有重跑旧 26 组或重建 host。

父随后只运行一个原版正常 `sfx-reenable`：`F:/fd2sdl-evidence/f5-pcm-layer-normal-20261117-01/report.json` SHA256 `b55a7f7e4990b70cf62977fa85f03ef74e657e4ff92fc03d4bc56bc154e7d3e7`，外层 `f5-pcm-layer-normal-outer-20261117-01/report.json` SHA256 `35234ec1b1ac4d78a5d8dac4cf16e022f375491627e9a6c66ecf82bdb75f6e40`。该轮覆盖原版正常输入／Save 和同次被动层对照，SDL 配对仍沿前述旧身份。原 PID1740 正常退出；Job root12936 的 worker／timeout／residual／forced／cleaned 为 0／0／0／0／1。外层由 OFF 经一次零位移唤醒恢复 console／session ON，strict ON 内层和实际 WGC 通过；两线程各自取得／释放 ES，电源方案和混音器未改，该轮原资源／用户档／root 守卫同。保存完整 22987B，未延长原输入和 tail。

实际封包 14073 条 control、791 Mix、32768 完整双声道输出帧，10019B 已初始化 DMA 数据，零 overflow／incomplete／open；同次 WAV 为 `e7fad0a510063db12433abd298502a3becbf2ea3cc0e28bf78e61203216426f8`。固定窗口各声道求和、DC、record gain／量化与该 WAV 的实际 offset312960 逐字节相等；不是按相关性寻找偏移。raw 为 `38ffab731e1d3908481558184d66f0cf59e84b64b06cdee1184dd3eec89a2ca7`，decoded 为 `34a300d2f98f213e9211be99ee8057614edc38130b39462c0a01ecf94af4d3a7`。

真实 304／B0 首提交确有一个 signed stereo 点：DMA Read 两个16位 unit，完整四字节 `80 80 80 80`，解码 LR 均 −32640；102716 次 DAC 入口计数不等于102716次提交，masked 重排单列。选中 C6 before 为 phase50100／50100、四个有效零窗、LR 低通 −1／−1、gain8192、alpha38711／order1；不能用 init0 代替。

随后正式 `src/sb_channel.c/h` 在同一真实 before、输入与操作序列下实际重放通过，生产实现无需修改。收据 `F:/fd2sdl-evidence/f5-pcm-actual-tape-C-20261117-01/actual01/report.json` SHA256 `ef0827c46a7d706487d9851a221ba97e7ffeeec16817d7fb83aa463e703a2c34`；资格 `build/f5/pcm-actual-tape-consumer01/qualification.json` SHA256 `16d8938aa06dbfe93c6c8deb4b84cab489693c6b9220c136ef19b439d8353ae4`。一个 C owner 原样导入暖 phase／LP／四点窗，没有 init／reset／gain 或相位拟合；476 AddSamples／10009B、1002 demand、791 Mix、683 EndFrame 共4902正常操作，逐步比14个数学字段，32768实录LR int32帧／262144B全等。原snapshot其余元数据未冒全字段实现；末Mix额外16帧只有状态而无frame实录，不算逐帧已测。另两个停止／非法49无写前缀case通过，它们是私有消费者和宿主H边界，不冒游戏UI取消。163实际编译／链接输入及原件／SDK守卫同，无新GUI或旧批次重跑。

31/8 的2652个原source bytes按独立49690关系形成5303 expanded bytes，首Read实际cursor231；tape首5072B与原变换 `[231,5303)` 逐字节等，含非零主体和完整尾。前231点虽为128，选择后input tape没有直接保留，不称全部5303B均已实采；其余4937B后继实际输入也交同C核，不裁成恰好单条voice。本结果关闭给定实际状态／输入下 SRC／LP；普通 Main 的304→C6暖态生成、完整 DAC 前史、绝对 PIC／首相位仍未签，不把捕获常量写作默认。旧证据及失败保持原身份。

### BIOS 唯一 SOURCE owner 的当前集成

`build/f5/bios-source-root-integration01/apply-receipt.json` SHA256 `f015fc6d10382957e34a3655489606a84eed2c61b044bd1046d34d9eb60562c5` 取入完整候选02及 CMake，共六文件；未先应用候选01。原 `clock_timer`、BDA 和 epoch 不复制，新 CPU／PIT／SourceClock 存储属于原 BIOS owner。prepare 后没有隐式 LEGACY 回退；SOURCE 不再进入旧 numeric IRQ／audio-prefix，安装使用同源公共控制。checked 观察成功才写 actual out，拒绝／首错／无进度均不写 out；历史前缀另由纯 getter 读取。

九目标 root 构建通过：`build01/receipt.json` SHA256 `538473c72e5dbdf9b038cd8a09cc027eb63a16ea51acf768a39b7e6e22768b2d`，App 为 `37883941d6bbc5af5ad2456e09dfe8b62702236ab8f1d6a80ad8095e6020e0b8`。此前 f11 App／PDB 已按字节及 mtime 封存在 `F:/fd2sdl-evidence/f5-before-bios-source-20261117-01/`。两个新 owner／C++ 目标及六个直接受影响目标共8 CTest 一轮通过，0.25秒；`check01/receipt.json` SHA256 `ab39b0949194eda30a626e539e0b2d7e3e62e7d0d7f1eea595647b3758ac89e7`，699实际输入及 source／原件／用户档守卫同。

正式 caller 的 checked 返回链已按下一节合入，不把 MAX 自动等同停止；已盘点63个原读点／27共享函数及95个生产 VGA wait／present 调用。真实 BIOS provider 仍未闭：FD2 保存的 PM INT8 向量不同于 RM packed向量，不能把 stock ROM 的13＋2个 head 直接连上；BDA原写点前后的 midnight／LED／motor 和原 BIOS 的独立 EOI 均须保留。当前 Main 仍为原完成调用 H 政策，SOURCE 构造／相位／全日历资格没有因上述接口通过而扩签。

### 正式错误传播、TIMB 与同一 SB 暖态的当前合流

`build/f5/clock-consumer-root-integration01/apply-receipt.json`，SHA256 `7fbbba4e8794d7b6dc0507fb853b1b7f4bd2c1638e3bb8d4b3544ae5a032fc4a`，记录40个已封文件、Main三个文件及Scene两文件修订的43个最终路径。五个VGA等待／呈现API显式返回失败；调用者保留已达的phase／页／DAC写入，立即停止后续输入、RNG或Save。未知SOURCE deadline是宿主提示缺失，不伪造source首错。Scene入口及walker返回后的SOURCE优先于HOST；Title普通取消／资源失败归还palette，SOURCE错误保已达前缀。初始checked→RNG及clock-close失败清理有静态控制流审查；八个新增Title场景并不代签这两个Main动态入口。旧Main SOURCE路由仍未启用。

`build/f5/timbre-warm-root-integration01/apply-receipt.json`，SHA256 `c100f008b2b7cd8fccd7e98f02f80657fcb57157954cd770d6d7085a9aa3e4df`，记录TIMB11项、SB暖态16项及旧检查分母修正共28个路径。仅CMake重叠，保留既有upload wrapper修正，合入唯一warm插入；新故障目标移到既有assert／生成依赖扫描之前。TIMB的14B runtime payload为唯一热音色，完整503、重复missing扫描、512B preflush／复制及成功返回尾按原写点执行；两份冷schema由同生成器扩到20函数／1890 head，另有raw194 head。旧18／1554检查只按两个完整函数新增的46＋290 head修正，拒绝测试未放宽。候选本来已有`<iterator>`，未重复增加。

Audio构造时建立唯一SB channel；DIG只绑定该owner，不能重置已有窗口。真实304/B0的四个80字节产生一个−32640的S16点；DSP计数与DMA两个WORD分别保存，早END与真正DAC不混同。新typed SB_DAC沿同SourceClock调度；ACK16 state1只增原counter，C6消费旧FillUp后保同声道phase／低通／窗口。独立复审在有限给定态机制中未发现确定阻断，Main构造epoch、完整304／3DA／IRQ及每次Mix仍未签。旧coherent测试的stopped-DIG路径没有probe，其adapter扩用暖态时必须同步ADD_DAC／REMOVE_DAC和实际Audio DAC receiver，不能把原104写用例当暖态组合已过。

当前实际构建、执行和保留边界如下：

| 产物 | 实际结果与身份 |
|---|---|
| Root合流构建 | 41个目标构建成功；`F:/fd2sdl-evidence/f5-ui-timbre-warm-root-20261117-01/build.json`保首次输出守卫失败，`build-qualified02.json` SHA256 `83d2a67907c090d1b6839f4f2b70459402214df4ab38d0796f6556515e20bd47`补足真实输出分类。新App在`C:/Projects/fd2sdl-builds/fierce-newton-f2-20261115/Debug/fd2sdl.exe`，SHA256 `bd03f40d042a3d520eccba9922662bcceab5d39c4ce49abc95dbc308120e9708`。 |
| PDB／空间 | E盘不足，本轮MSBuild临时OutDir指向现有C盘构建目录；CMake写死的PDB仍在E盘被链接器更新。构建前旧EXE／PDB已按字节与mtime归档；新PDB另存F盘及C盘，E盘旧EXE／PDB配对已恢复。唯一变化由实际link命令与vcxproj解释，原source／资源／用户档全部不变；未重跑构建。默认产品输出规则未改。 |
| 直接检查 | `check01/report.json` SHA256 `891d72a276a33defc9fe063118a394834718d672c89094f11b61d65aefaf7819`：43项CTest通过48.80秒，包含六个新故障族、完整Title及直接影响的旧调用者／SB／Audio／时钟；另18个实际TIMB组合与独立oracle完全相等，6＋12＋13项工具检查通过。1480个实际CL／LINK／PE／构建规则输入及原件／SAV守卫同；无GUI、设备或新原CPU。 |
| 可复跑聚合入口 | `bgm_timbre_source`使用正式私有TU的exact case argv，独立原资源／源模型给expected；`bgm_timbre_source_tools`核17个工具合同，新增4项覆盖进程失败、缺失结果及运行中PE变动。新入口实际两项通过39.85秒，未重建PE或重跑旧43项。`ctest-entry02/report.json`保严格mtime守卫失败；`qualification03.json` SHA256 `d268ef512d852bbb0214ada02d398d9e2111a4107eea2e95ca76ddab35ed4e26`只分账显式CMake注册改动和内容完全相同的configure-owned stamp时间更新，其余1478个实际输入严格相同。不是新全构建或Main／PIC资格。 |

上述输出均位于同一`F:/fd2sdl-evidence/f5-ui-timbre-warm-root-20261117-01/`。首次apply前置检查的物理／LF hash与CMake前缀假定错误都在各intent中记录，均未派发产品写入；更正后只实际应用一次。已有原版32768帧窗口、同输入C SRC、旧GUI失败与声音例外保原身份，不重复采集。正常DOS文件服务、完整304安装、真实BIOS链、首相位和非例外全音频仍是必需门，F5尚未提交或完成。

### 输出失败的正式共享链回归与可复跑入口收口

`F:/fd2sdl-evidence/f5-coherent-sink-root-20261117-01/report.json`，SHA256 `97a39cae25b6778996f76241d51d48800fcf63478bd6c16c98291446495f577d`，只重建coherent测试目标并运行新增`--sink-failure`。首次sink拒绝后不再投递，实际Audio仍完成同一source操作和104个原写序；SOURCE无首错，operation完成、消息／busy／defer／depth归零，OUTPUT首错与0交付另账。369个实际CL／LINK输入及原资源／用户档／两处App守卫同。已有两个coherent场景未重复；不是304暖态或Main的联合资格。该fixture同时补全typed DAC两effect及真实Audio receiver，其新增DAC分支本例未执行，不以编译代签暖态覆盖。

此前joined构建的项目内实际输入已先归档到该目录`prior-joined-inputs/`，manifest为`archive.json`。大部分为当时物理文件，旧coherent源来自编辑前副本；旧CMake以删除唯一新CTest注册段重建、逐字节摘要等于原执行前记录，明确标为重建，未冒历史物理副本。内容相同但mtime更新的configure stamp也单独登记。原失败收据不改写。

为避免再次改到E盘旧PDB，现有C盘构建仅在本地cache设置`CMAKE_PDB_OUTPUT_DIRECTORY`为该普通构建目录，重新生成后的vcxproj已核实际Debug PDB指向C盘；本轮没有重建App或改产品默认规则。此前新App／PDB的身份仍沿原构建收据。

此时还发现回归入口交付缺口：CPU预算、PIT3、source queue／IRQ、0300运输及source-stream的若干独立回归只在ignored候选中。只读盘点`build/f5/regression-entry-inventory01/report.json` SHA256 `de0034e905dd275e2db77c1a4b3c4327528aadde7ea2dd500171d84786c25720`固定五组源／生成器，未重跑旧3782等向量。以下已将唯一合同整理进正式入口；coherent／warm不替代全部core边界，新增入口也不代签完整Main。

### 普通 DOS 文件服务与正式回归入口

`build/f5/dos-file-root-integration01/apply-receipt.json` SHA256 `0be0b46e57c5c46a616aa8eaabf4e64bda38c21920c4fe59223ebe3fb84418d5`记录七路径：原DOS三文件、C++声明修订／链接测试、参数化独立oracle／四项输出协议检查及最小CMake接线。同一个`fd2_clock`归档消费正常CB_INT21／同FILE cursor／实际seek或read／A20及native IO／至少一次Idle；没有另一个CPU／文件系统／时钟。native callback期间普通IO费用按真实last_callback条件跳过，Idle内部恢复callback0。DOS失败与typed host IO边界分账，不补CF／count／IRET。

C++声明原先缺extern C，经独立实际C／C++编链发现后以单独revision修正，旧封包不变。新Root generator只改CLI和输出管道，数学模型不读取C；15族的36064B expected仍为原`468dbda5c7d9761f67c5e6fd7bbe429a8578bc5c1b31f92b1a470c6ad58ca4d7`。原资源只在CTest fixture运行时读取，普通App构建不依赖原资源或ignored source。公共化生成／四项输出协议实际通过，`F:/fd2sdl-evidence/f5-dos-file-public-oracle-20261117-01/report.json` SHA256 `896e3357652c6723624da869fc8fd7d802acf167c65efa7b36654f8ca20ed7bd`。

App／两个DOS目标构建通过，`F:/fd2sdl-evidence/f5-dos-file-root-20261117-01/build.json` SHA256 `c4485efcb53f80cd3e48da7b00871884ea8af94609d9a46d2d0424896a92e26e`。当前新C盘App为`6c6bcbcbf3852dcc93be8db15cf3fdf24659debab63f00ace2fcf987d8ca3617`；PDB确在C盘，E盘旧配对及原资源／用户档守卫同。`check01/receipt.json` SHA256 `5da7afbb55ae15a793de7fec7e6d5b00af5effa0dda55a0e41c23d5de0506be7`：四CTest通过0.20秒，含15个完整字段／scratch／有序trace例、同clock真实AIL busy-body interleave、真实C++链接及四项协议；451实际输入前后同。App与先前bd03候选的`.text`／`.data`等七个PE section物理相同，`.rdata`不同，`app-sections.json`只签这些分段字节关系，不称whole-image等价或新GUI／Main SOURCE已验。

5a0的正常全direct引用与非别名写集现已有来源：仅比较FFFF、原样传DOS或保存open返回，没有数值运算／序列化。因此有源open成功后可一对一映射同一个FILE／cursor，无需再采固定handle数字。正常打开／关闭本身仍在补；固定官方源码确认open按1024B、close按512B做真实delay，关闭返回AL为原pre-close refcount，唯一owner时是1，原driver关闭后不清5a0。依据在`build/f5/dos-file-lifecycle-source01/`；已有五个旧时点IVT21事实保旧身份，不代当前RM IDTR／hook闭包或Main初相位。

`build/f5/regression-entry-root-integration01/apply-receipt.json` SHA256 `61edb79e2e55c0edc0b557024931eab0e3106087885c7933b0687260b768823b`记录十二路径，把五组唯一边界及四个独立生成器移入仓库。只链接现有clock／native归档，前三份header逐byte等旧件，数学AST／测试正文关系另账；stream只保公开owner／重入／final-point两组，普通／source-fail／output-fail严格继承现coherent收据，不重编第二sequencer。五目标Root构建通过，`F:/fd2sdl-evidence/f5-regression-entries-root-20261117-01/build.json` SHA256 `a7ea917868d97ff3f7949073026a42898e588a36516dccdc8cf262143e3e9a52`。

首次Root执行的新stream两组与五项输出协议通过；新0300公共边界失败，收据`check01/receipt.json` SHA256 `1064b05498897fa8d43bd2d33743b15f30771a53fe5b1e6b8d179c6a5c3e082a`保426实际输入守卫。迁移夹具漏记最近grant后的预算，补observer后再定位到失败计数误漏进入driver前的两次IO；第二失败收据`F:/fd2sdl-evidence/f5-regression-bridge-fix-20261117-01/check01/receipt.json` SHA256 `8d982f88f3024e9e0ae4ea68fcfa89abe674ada22228cdb1151784f894fee436`保原件。

修正计数来自独立`contract02`的2864／286a→8cc源序列：第三次OUT失败，尚未到8d3，故IO3／removed900／CMOS data0。未改产品、正常16行独立成本或放宽断言。只重建／复测失败目标后，公共16参数／八个前缀／4506 pending边界通过：`F:/fd2sdl-evidence/f5-regression-bridge-prefix-fix-20261117-01/check01/receipt.json` SHA256 `9ef21e6fd30e43694ae01ac021f80042a8e67674c702f6dafad9952915d54c4d`，294实际输入守卫同。失败PE／PDB／源及项目内实际输入均先归档。旧CPU3782／PIT24／source-clock矩阵与旧0300完整逐PC trace未重跑，不把公开总数检查冒成逐PC新观察；它们在F6统一当前候选历史回归中实际执行。

### 同一文件 owner 的打开、关闭与失败释放

`build/f5/dos-file-lifecycle-candidate01` 在前述只读服务上抽出共用 CB21 入口／返回，未另建文件游标或时钟。固定原来源要求：open 先建立映射和 AX，后执行 1024B disk delay，最后清 CF；close 先释放唯一映射，AL 保关闭前 refcount1，后执行 512B delay，最后清 CF。原 driver 的 5a0 在 close 后仍保留，不能由宿主释放重写。新增 `owns_file` 仅标本 owner 创建的资源；借用 FILE 不被兜底关闭。host release 必须在源栈展开且同 clock 冻结后执行，不收原 306 费用、不清 SOURCE 首错。

原候选的独立源方程、15 段历史／41 次操作和 5100 条有序状态记录有实际 C 对照；这些记录数不是 5100 个独立功能。共享段改写直接影响的旧 DOS15 在候选中复测一次，36064B expected 保持 `468dbda5…58ca4d7`。C++ 链接和五项输出守卫另有收据。Root 以八路径一次取入，`build/f5/dos-file-lifecycle-root-integration01/apply-receipt.json` SHA256 `614a96d1b2fbe4dcb96a0b45a8b3b3c5816a85dcd6d799829ba5d1165d6715cc`；没有重跑旧原生窗口或旧 DOS15。

Root 只构建两个新目标并运行四项直接 CTest：`F:/fd2sdl-evidence/f5-dos-lifecycle-root-20261117-01/build.json` SHA256 `299be9dc30ca62807292cb219d1402bb48605214464f21f611a9cf960a55834d`，`check01/receipt.json` SHA256 `be6026e5d774679ee79358e19b681c92cad8420e3ea6680cc943656caeba1739`。316 个实际 CL／LINK 输入、原资源／用户档和旧 App 守卫前后相同；独立 1092628B expected 为 `4ff93b9d832e1eddd93f74ab0b15d33664d633abf2bf0793465d25857aa23c2f`。重建前的可变库／PDB 已存本目录 `before-app/`。App 未重建，仍为 C 目录的 `6c6bcbcb…ca3617`。

这些结果只签给定、具备来源的普通只读 SFN／空槽／唯一引用／无时间 writer 的有限 native 合同。当前 RM 路由、完整 raw501／306 与外层 AIL／0300、正常 filename 和 Main 活时钟 quiesce 仍未签；不得用已打开服务或宿主 fclose 代替正常源关闭。

### 正常 304 探测的有限源组合与共同 REP 参数

`dig-install304-candidate02` 是未取入 candidate01 的完整替代。40b40 的完整 80 heads 与 raw driver 的 16 个 owner／530 heads 来自独立来源，新增第三份固定冷定义；生成头继续由现有唯一冷生成规则产生。consumer 借同 AIL API／0300／Audio／SB，新增端口／配置／向量字段也只在该 SB 的 driver owner 保存。真实 ACK 读点只清 pending／LOWER，probe counter 到原 06bd 才加；未把旧整 IRQ 叶提前调用，也未建立第二硬件或时钟。

父复审发现并修正 REP 资格错误：同一个 CPU 的 `rep_max` 曾分成两值，安装端还错误拒绝合法 0。现仅借 `bridge.facts.rep_max`，0 为原不限，负数仅作为尚未完成宿主配置归一的 profile 在首源动作前拒绝。六个新实际组合覆盖共同 0／2 的完整探测、busy 桥、由原 string 长度独立推得的额外 20 grants、budget29 的首 DWORD 前缀及负 profile 无写。原 23 例的 9／0 给定态不回签为同一正常 CPU，旧件保持原身份。

Root 实际取入 13 文件及最小 CMake 接线，`build/f5/dig-install304-root-integration02/apply-receipt.json` SHA256 `ffc3529f22a0eb9f6dd9e4f48467c15fecafa6a3311f31d9c78cbd4a2a79ee67`。父另将两个 `mask.bit_count()` 换成 Python3.9 可用的等值表达式，完整独立 expected header 逐 byte 相同；原候选 seal 不改。首次 CMake 锚不唯一的断言在任何 Root 写入前停止，intent 保留此边界。公开 Python 工具的其余版本 API 仍按实际复核，不能由本机新版本通过代签 3.9 实跑。

只重建 App／新 304／旧 SB provider／暖态四个直接目标，`F:/fd2sdl-evidence/f5-dig-install-root-20261117-02/build.json` SHA256 `09064c1ce6a6d28109ef09f6af3df3d049dc9f443c82f4392e57e7bf17b5b0bb`；四项定向 CTest 通过，`check01/receipt.json` SHA256 `606d1e16ecdba98a60323a9986e74a397e4d6d772e6f4cb287a26119be394b78`。除新六组合／15 工具项外，仅因 ACK 提取复测直接受影响的 SB 八族与暖态合同；未重跑旧 23 或原生窗口。494 个实际 CL／LINK 输入及原资源／用户档／E 盘旧 App 守卫同，原两份 MDI 生成头逐 byte 相同。最新编译 App 位于 C 盘，SHA256 `2f69c7af9466d57327d4775aac06e864517d1a1167ee2a1a75baa31409cfa169`，不是已集成主线或新 GUI 候选。

该组合只签显名 provider 的 DMA16、probe IRQ 状态0／1及同参数源写集。3DA 仍为给定输入，Main 启动／真实外部日历、完整 IRQ0 运输、DIG 后 C6 的状态2／3源费用及全部非例外波形未签，F5 保持未完成。

独立复审 `build/f5/dig-install304-peer01/report.md` SHA256 `b455ac6a5ff90060bb742f79f63f42aa9cabd9b2ecdb705bfa66acaac2f6e55d` 核完上述 owner／写序、六组合真实编译身份和原字节；未见声明域内新增确定错误。首次 Root 跨应用时点的检查保 false 原件，后续改用封存 before 作 AST 比较，未改签为 Root 全时段快照。DMA8 fallback 是原合法分支，当前 consumer 的 H_SCOPE 仅标未实现，不能判原 U。

公开工具的 Python3.9 静态复核覆盖本次44个改动／新增脚本，完整名单／身份见 `build/f5/python39-api-review01/`。另修 `build_mdi_source_costs.py` 的 `Path.write_text(newline=)` 为 UTF8 字节写入；`F:/fd2sdl-evidence/f5-source-cost-python39-20261117-01/report.json` SHA256 `927fbba4da6ab64aed6943e952ee81be87c9794197aa4521cdcf44788e07d3d9` 实际用三个公开 CLI 生成到新目录，三份 header 与当前编译头全字节相同，原资源／SAV／旧 App 守卫同。没有重编 C 或重跑旧组；3.9 AST 与 API 静态检查不叫该解释器实际运行。命名发布工具三处同类 API 在 HEAD 已存在，按原范围单列，不冒称本次新增或已修。

### 旧 BIOS callee 的两份 flags 与正式回归

`bios-chain-source-candidate02` 修正原 candidate01 的同 flags 假设：80:08c8 留未掩码返回 frame，08d7 留清 TF／IF 的 target frame；70:0455 和045e再形成两字。正常硬件 IRQ 的 saved IF1 经这些 writer 得到 ROM entry IF0、ROM continuation IF1，不能把当前 IF0 或两份手填值冒源。SS 来自同 AIL 的 C8 32bit alias，并非任意 flat SS；AH 仍须借真实被中断操作。原01及16例只保旧给定态资格，未重跑或回签正常链。

Root 一次取入7文件、AH9测试增量及公开独立 PC oracle／最小 CMake，共9路径：`build/f5/bios-chain-root-integration03/apply-receipt.json` SHA256 `316056a0e8a0e105accc7d87c9f0aafc75251a408b2f631cddd3f0a0cbf82d52`。同一个 `fd2_clock` 编译 BIO，mode-out／mode-in 继续复用原0300实现，没有第二 BDA／CPU。公开 oracle 只固化原 Domain CFG 的 PC事实，不读取 C、冷 catalogue 或生成头；4例3408B与原 `0c0e020c…71a39` 全同，新增 AH9 的309头1252B与 `e9777d48…7614b` 全同。builder 的 Python3.9 字节写入修订另账，不改原封包。

App／BIO／受重构直接影响的公共0300三个目标构建通过，`F:/fd2sdl-evidence/f5-bios-chain-root-20261117-03/build.json` SHA256 `d5abd9858fcfdd0f50bab8fb00a439636a4c1563f35e593dda68c9c9b6d646ac`。三项直接 CTest 通过：新4例包含正常双flags／坏context／mode-in首错／AH getter首错，AH9另核比AH0多出的0956／095a两头，公共0300保16参数及八前缀合同。`check01/receipt.json` SHA256 `c79e001e2416df0d66dff5f3b18e17e318928832483a001aa40aec7c9b7bcf7e`，470实际输入及原资源／SAV／旧App守卫同。最新 C 盘 App 为 `e55b732d66f73393e7f0c79b8e0cea9ddc40fd01199ba6c5c11199bd3efa35cf`，未进行该 App 的新 GUI 或 Main SOURCE 验收。

仍缺的是精确的正常接入资格：PM 硬件入口、RM IVT8=0c5d:111c 的反射入口／返回、当前 loaded身份、实际 interrupted AH、native pre/post 和全日历／首相位。IRQ route 可在唯一 App context 内沿 mode 叶成功返回且 IF0 的窗口借换；不得在 CLI 前提前换 RM，不得把 RM IRQ 直接跳 PM AIL 当零费用。该最小方案及源边界见候选的 `irq-route-plan02.md`；SourceClock 不因此增加第二 PM／RM CPU模型或默认 provider。

### 曲11正常588等待的本次 owner 观察

静态复审已证正常复活最后一人后请求 `(11,1)` 并可无键留在588等待。`track11-terminal-owner-review02` 只给条件：实际 A=N 时末尾 reader 越资源 owner；A>N 时仍需一个 slack BYTE 的历史来源，不能由本次加载未写就统判 U。旧G3约6秒声音和画面没有块头，不代本项。

Root 新薄入口／16项纯协议保原候选身份，仅应用2个工具文件，收据 `build/f5/track11-owner-runner-root01/apply-receipt.json` SHA256 `8428fbfaa9a0d3701678a9069d95b0db0aa245cf1758dea1da7165889de42033`。默认prepare先核当前48k参照／actual quiet argv及合法受控完整SAV；随后只运行一次正常原版前缀，没有 SDL、录音、后继业务键或 Save。

`F:/fd2sdl-evidence/f5-track11-owner-native-20261117-01/report.json` SHA256 `0eeb5ec18e599fb72d22f3a6c97550856f110c6f2657b94b16ade596688fbba8`：当前B=2200396、S=2060312／D=2056520，S为8记录pool首项，playing4／loop1／零hook，原3152B曲11 payload全同。真实 tag 两轮3157，块大小3156、可用A=3152=N；末尾根／tag／两代码锚／descriptor均复核。共35个Probe命令、3716B合法读取，**未读 B+3152、原栈或整RAM，也未让terminal越界实际发生**。这关闭本实例的局部条件边界，仍以前缀V为条件；不推广所有堆布局、完整first-U／PIC／声音或slack来源。

本次正常588 phase3与真blink由同HWND闭合WGC序列取得；有限字形／portrait判据不是全帧资格。初始显示OFF，经现有ES需求及一次零MOVE恢复console／session ON，指针未移，活动桌面Default；需求由线程30164在owned清理／来源检查后恢复旧flags。未改电源计划或系统混音器。原PID32260正常exit0，Job45196 worker0／timeout0／residual0／forced0／cleaned1，RPC已核同PID并关闭；所有原件、用户SAV、稳定游戏副本及封存WGC守卫同。外层收据 `F:/fd2sdl-evidence/f5-track11-owner-native-20261117-01-job/outer.json` SHA256 `7be08edbfccb258ac1aeb329d7be2e6e074e7304f03966c60a4801f130149859`。本次窗口已结束，不为同一块头重复运行。

独立只读复审 `build/f5/track11-owner-native-review01/report.md` SHA256 `f1c6c8c6bfb763abdedc76fc89968a09e37d1c1e5c931db048c773b43383ab6a` 已核123必要输入、23个足额BIN、35命令加PING、fresh映射／8fixup、完整资源、18个实际键及46帧闭合WGC／53封存文件，无新增窗口或整RAM hash。150ms输入receipt的 `qpc_valid=false` 原样保留，独立QPC bracket仅作页面／输入来源，不补guest收键或严格声音时刻。已有13 caller owner／32 callsite及冷绑定盘点中，五个无尾BYTE的曲目与正式正loop producer交集只有曲11；其正常复活请求不能排成诊断。其它曲目的任意正loop公共测试不因此成为新正常producer，原未验域仍保独立身份。

### MDI 文件打开、503 与完整关闭的当前组合

`build/f5/mdi-file-lifecycle-candidate01/qualification.json`（`3a56e26f…de4f84`）与独立来源模型固定给定参数下 raw501→实际503→raw306 的完整源顺序。原 driver `5a0` 只在同一个 `AilDriver.raw_sample_handle` 保存，profile 只供首次 seed；503 此后只读 live 字段。501 先执行真实只读 FILE 打开／AX／原 disk delay，再按 CF 写5a0；306 先做492次寄存器初始化和一次disable的真正OPL writer，再关闭同一FILE。关闭保留原5a0值，失败不补源返回或清除首错。所有费用、API／0300／132、IF、budget／PIT／PIC和chip继续借现owner。

候选首轮的 oracle 将完整Create后的patch初值错当FF，probe另将data_port错当logicalreg；均已用独立原initializer／真正OPL writer修正，失败原件保留，未改产品C去适配expected。另一轮case3取消本来重复击中writer前缀，改为明确的慢disk与初始horizon后只补跑3／4／5：实际覆盖open mapping已建立但5a0尚FFFF、close mapping已释放、以及writer第10次后的source错误。原actual02九例、actual04三例与最终空行修正保持分身份；不虚称每次PE都跑全九。

父侧12文件一次应用收据：`build/f5/mdi-file-lifecycle-root-integration01/apply-receipt.json`，SHA256 `c4f5396bc7f49afe96c7766837075314b0f1d12a9dda1629d6f0e376159dfd1b`。`F:/fd2sdl-evidence/f5-mdi-file-root-20261117-01/build.json`（`d82bb706…62022`）记录App及三个直接consumer构建通过，当前C盘App为`af7a27b0a10adb4093389f7a56a443320306882ade1b45260a910ab74ad872f0`。`check01/receipt.json`（`78902cba…c7cbd5`）记录新增fixture／实际C九例／八项工具门共三项CTest通过1.68s，645份实际输入及原件／用户SAV／旧E盘App守卫相同；没有重播旧18或coherent整组。5,022,960B独立expected为`40425f9f…ad363`，与源侧封存逐byte相同。

该九例的 BGM 与 bridge REP 参数实际均为0；`fd2_bridge0300_facts` 的最后一个字段是 `cmos_restore`，初始化列表的末尾9属于该字段。父审查草稿曾误读为REP9，核结构定义后已纠正，不构成实现差异，也不因此撤回原九例或补跑。后续正式source接线仍须让各consumer借同一已资格CPU配置，不能从本次给定值扩签任意Main参数。

普通构建不读取原资源；新的公开回归仅在runtime读取只读SAMPLE.AD／FDMUS／MDI，源侧模型不读待验C、冷成本表或actual。Linux入口已登记，未执行。给定normal DOS条件、filename映射、全部raw关闭writer不等于正式Main构造／whole443d0／507已接线；任意DOS错误及重复close等原合法错误分支仍按具体来源保留，不能用源H签原结果。Main正常卸载须先在live clock执行306，冻结后host释放仅作宿主回收。

### SB 暂停、续播、三档 rate 与同一 channel 的当前组合

`build/f5/sb-raw-irq23-candidate01/qualification.json`（`7e06e87e…31de7f`）封18文件，正式补丁为`candidate-final.patch`（`1f9d85d0…1afad`），早期`candidate.patch`不作应用入口。独立复审`build/f5/sb-raw-irq23-peer01/report01.md`及`final-seal-join01.json`逐项连接原139个raw head、两组pause／resume表、原72B CAP、固定host源、实际CL／LINK输入与两个PE；未重跑C或原版。没有在该给定域发现阻断项，完整Main及其它格式仍未签。

raw state2在真正D0 OUT执行FillUp→PAUSE→RemoveEND→RemoveDAC后，才写续播token／halfFFFF／state0。state3先写programmed rate，再pause→41 high／low→resume，最后翻半；真实low OUT才执行DSP_ChangeRate。requested／programmed／kind各对应原字段，不与唯一硬件DSP rate混作同一状态。所有I/O、effect或源失败保已达写集，不补后缀。

同一channel支持原CAP的11025／22050／44100三档U8 mono；SetFreq在den1下保phase／sample／预取窗。DSP_ChangeRate不重配低通，新的DoTransfer／Reset才有独立filter writer，cutoff5512／11025／22050的系数分别27467／38711／48672。D4按实际DMA单位计算min33／66／132，短尾END继续按原float运算与同queue缩slice；特殊304的两WORD与double DAC另保。例外`EX-AUDIO-NUKED`没有参与这些PCM或时序判据。

父一次应用收据`build/f5/sb-irq23-root-integration01/apply-receipt.json`为`6f3c2754…30f9f`（18文件及CMake）。父只对两个公开oracle的CLI追加现有`source_fixture_paths`保护：固定文件名、原件／source拒绝、保留相同内容mtime、拒绝覆盖不同结果；数字`build()`不变。另补三项CLI协议检查；新sample-file目标同时补齐/W4及/fp:strict，仅编译，不因此重播原九例。`F:/fd2sdl-evidence/f5-sb-irq23-root-20261117-01/build.json`（`1e0594d9…fdcd81`）记录四直接目标与App构建通过，C盘App为`fed8957770eec62e30dc996e37abc9c15de4da49542eafa715691565faaf7321`。

`check01/receipt.json`（`c0ec0e99…32e9c0`）记录新增两fixture／三C组合／九方法工具门共六项CTest通过0.71s，556份实际输入和原件／用户SAV／旧E盘App守卫相同。74个channel事件逐完整2048×2窗／48×2输出／17字段对照，11个rawIRQ组合逐完整PC／I/O／窗／F32载体，13个短尾／首错组合逐bit校验delay／index；三expected分别`8b002a94…d9332`、`421ea0c6…30baa0`、`a119914b…42943`，与子侧封存相同。这是给定before的真实C，不是自然首phase或全波形。旧304／设备／PCM历史组未重跑，Linux未执行。

上述封包仍不签Main／PIC。kind1／2／3在原SDK的CAP中合法、当前renderer未实现；但“CAP有四项”本身不证明本固定Game可达四种，前版将这两层混列为待补能力，现由下面的完整producer复审更正。`H_FORMAT`仍是SDK条件域未实现，不是原U；它没有被新增为正常Game拒绝门。

`build/f5/sb-game-output-kind-review01/report.md`（`7b0c1e22…94290`）、`checks.json`（`d6f2271a…acc42a`）及seal（`30bf2efa…636f8`）核11完整函数／28关键opcode／整72B preference窗的33个literal及全部base xref、setter和间接目标目录。Main先经37d3e→3fca9，明确写pref7=0／pref8=0；setter链只从该初始化进入。固定DIG.INI不写格式，406b0据0选择顺序0／1／2／3，原SB16 CAP首项为1，故首项即写D.kind0；pref1=1另使输出rate为22050。普通sample rate11025不重写D输出rate，后续raw401／暂停恢复也不另造kind。13份输入末守卫同，无新C／原CPU／IDA／GUI；旧D观察只作旁证，原TMP失败不改签。

因此本固定未修改Game＋原DIG.INI／SB16.DIG的正常成功安装只需kind0／22050；不应用未编的`sb-cap-formats-candidate02/after`草案，也不为SDK扩展域增API或GUI。完整四格式及warm余component草案保原身份；未来若增加其它driver／外部preference／真实新producer须重开资格，不把此结论推广为任意SDK或任意内存别名定理。**304的B0/S16 stereo暖态仍真实可达**，独立于D.kind且已留同一owner，不能随其它播放格式一并排除。当前重点仍是raw401费用、完整构造、实际IRQ运输、3DA／VGA日历、Main／绝对phase和非例外联合输出。

### 正常 IRQ0／IRQ7 的 PM 与 RM 运输合同

`build/f5/irq-transport-contract01/report.md`（`7696faa3…c0f6d`）复用已保原来源，并补五次定向Domain，189次head访问／175个新增唯一head；未运行新GUI、guest或C。它区别四种入口：PM0走70／80保存与AIL；RM0先保原flags副本、清当前frame IF后反射到同AIL；PM7必须经历PM→RM普通反射才能到raw0682；RM7才直接到该raw。RM0的AIL／BIO continuation IF0与最终回游戏IF1来自不同writer，旧PM-only IF1验收不因此改签。

原源还证实普通mode-out与IRQ返回0931／283c前缀不同，不能互代计费；PM7的ISR选择OUT0b也不是EOI。真实操作的AH须从当前typed调用借用，未知不得默填0。合同及注册writer补充保持各自seal；当前新loaded根、完整运输C、实际日历／相位和Main SOURCE仍待验证。后续frame复审还发现managed调用的普通返回、BIO尾链返回与outer hardware frame是不同层，原表中将AIL返回IF直接等于outer IF的说法尚不足以签完整运输，须用完整调用形状继续收敛；不会改签旧given-callee结果或填默认值。下一步用同一clock和有限App route实现已证共享运输，并只追加直接影响的源／C检查，不把静态head数当时间验收。

### type3 中介的来源更正与有限运输候选

`build/f5/irq-transport-contract02/report.md`（SHA256 `1607830fd3a288d513bb540df5c0b0c61f0eb8ab05b934ac086dfd8fe72398e0`）取代 contract01 对 BA4 调用形状的解释。父侧与独立复审都确认：BDD 保存 user node 后，BDE／BE1 将 DI 改为 type3×8；BF2 只恢复 BX，BF4／BFA 仍从 DI=24 取中介。五份既有合法 RAM 的 C0 entry3 共40B均指向80:E6A。新增34／27个静态 head 与旧 selector 原字节逐项吻合；没有新 guest、原栈读取、GUI 或设备实验。旧 contract01 和给定 callee 测试保留原身份，不倒签。

E6A 从同一 workspace 保存 outer flags，只在局部清 IF／TF，建立 ED6 continuation，再恢复被中断操作的实际寄存器并进入 AIL。因此普通／busy AIL 返回 ED6 时 IF0；BIO pending 经 D09 时才用 workspace outer flags 改写该 continuation。ED6 再合并原 TF／IF，F1D 明确返回 EAX0，使 C45 停止 managed chain。默认旧 BIOS node 不在每个普通 callback 后自动执行；不能重复收其费用，也不能把 AIL continuation 与最外层硬件返回合成一个 `restored_if`。

另已证明 RM reflection 的同一 CS:8=8 会使70:5231走短退，不执行 PM 普通路径的 ISR OUT／IN；PM IRQ7 默认旧 node 入场 SS=A0，返回取0496，D09旧链 SS=C8则取0495。真实接口须借同一 kernel／workspace／有限 descriptor 活字段，不建立第二 CPU、GPR 或原栈模型。实际 interrupted AH、当前 loaded 身份、首相位及 Main 的 producer 资格仍独立保留。

为缺少旧日志样本而准备的被动 observer 已停止在离线 clone／来源固定步骤：`build/f5/hold_irq_return_observer01.json` 明确未写新 hook、未 build、未开窗。来源已足以解释此前冲突，不为同一性质新增原生窗口。后续有限运输 C 与 Root 结果见下文；来源更正本身不签完整 PIC。

### 完整音序初始化与当前 Root 冷缓存组合

`build/f5/mdi-sequence-init-candidate01` 的13文件以 `qualification.json`（`6fd1ca9a…42605d`）、seal（`07db90f6…186cd`）和补丁（`92ac43cf…5ae0a4`）固定。正式 `fd2_bgm_play` 的 SOURCE 分支实际调用完整443d0：按42520／4997e逐BYTE查FORM／CAT／XMID，沿原size+8遍历，逐原写点建立S字段并初始化；5个3B sentinel在第一次BYTE比较已失败，没有第四BYTE读。第一次512B复制写原全局54400 scratch，第二次才写唯一D包；失败／busy／preflush保真实前缀。这两个原owner不能合并，也不形成第二sequencer。

子侧24例包含15真实曲目、5 sentinel及busy／源错等，3冷缓存例另经正式play、真实503／FILE读与306关闭。公共27个expected对象逐一等于执行前冻结的源模型对象，没有用C更新golden。子侧C／头／库属于完整旧MDI12冻结epoch；其24／3结果不回签最新Root SB18布局。构建准备／首次oracle／收据标签失败和勘误均保原件。

父应用收据 `build/f5/mdi-sequence-root-integration01/apply-receipt.json`（`1c35f23c…acc180`）保留已合VGA的CMake，只追加新入口。父复审还将现代 `prepare_native_sequence` 预检限定到legacy：SOURCE仅消费原parser，固定27项expected不变。实际五目标构建 `F:/fd2sdl-evidence/f5-mdi-sequence-root-20261118-01/build.json`（`dccc2f25…cd6ffa`）通过；新三项CTest通过2.69s，`check01/receipt.json`（`370504b6…b4469`）核669实际输入及原件／SAV／旧E盘App守卫相同。新24+3实际完整对象与 `a07025dd…83d76` 独立fixture相符；旧9／18没有重跑，timbre／coherent只直接编译。候选README建议的upstream target在Root不存在，父没有新增冗余target；旧孤立TU只保局部指针适配，不冒新执行。

本轮闭合的是给定before之后的完整callee与冷FILE组合。25977／res_load／3adf5外层费用、首writer前的43ad0构造、真实callback／IRQ／完整日历与Main绝对phase仍未签。最新构造来源还发现原raw300生成扩展名 `.OPL`，正常应打开 `SAMPLE.OPL`；本固定参照的SAMPLE.OPL／SAMPLE.AD虽逐byte同为3242B（`87ddb683…ed5b`），也不能把旧给定SAMPLE.AD路径费用改签为正常filename producer。正常构造继续按原字符串与唯一FILE实现。

### 普通 03h／13h VGA 日历的当前合流

`build/f5/vga-calendar-candidate01` 以固定官方 `b6abbd5` 和已保正常配置证明S3普通两mode的实际时钟／retrace／hblank、3DA底位04及图像事件族。03h／13h的整数clock分别3146888／3146875；period差低于原0.0001ms阈值，切换时不能擅自重置旧fps／time_base／frames。原先8355 poll前缀的1024首末端点只作旧诊断旁证，不恢复未记中间时点或改变旧运行FAIL。

新增calendar借现唯一SourceClock，精确保留SETUP、VERTICAL、DISPLAY／PANNING latch、VINT和LINE的顺序／float延时／失败post-decrement及StartUpdate正常false。按handler精确REMOVE只筛EXTERNAL=4的相同token，原通用remove不变，SB_DAC=5不受影响。图像callback仍只是输出接点；真正VRAM／DAC行采样另补，不能把记录sink当完整像素。

七源文件加父CMake的应用收据为 `build/f5/vga-calendar-root-integration01/apply-receipt.json`（`ec2782e5…84b53`）。App及新target构建 `F:/fd2sdl-evidence/f5-vga-calendar-root-20261118-01/build.json`（`d5290257…260d5`）通过；三新增CTest通过0.98s，`check01/receipt.json`（`adbf9a17…3ebb7`）核451实际输入和保护原件相同。公共48边界、4族／3604完整状态与有序queue检查点及6纯方法通过；子侧私有1072端点没有为Root机械重播。最初独立oracle漏一次普通失败post-decrement，已从官方source修正，失败原件保留，未放宽容差或改产品计时。

当前没有Main初始calendar、INT10中途CRTC编程轨迹、真实scanout／SDL／WGC资格。稳定mode的给定日历与原App首phase是两项不同证据。

### IRQ 运输与 OPL timer 的当前 Root 检查

`F:/fd2sdl-evidence/f5-irq-transport-candidate-20261118-01/handoff02.json` 固定IRQ补丁 `a85431b4…1c250` 和最终actual06；README02另更正旧README的数值，不重写seal。21条完整源PC／写集覆盖PM／RM0／7、BIO有无、动态2b7／0931、same-PIC嵌套／busy及失败前缀，11545 visits不当功能分母。mode借用在32d／28a6实际成功且IF0的写点更新；后续descriptor失败保已达PM，不伪恢复RM。普通AIL不重复旧BIO，原PM-only给定flags接口保留。

父12路径应用 `build/f5/irq-route-root-integration01/apply-receipt.json`（`3883b9a6…1bc57`）保VGA／MDI源。第一次父脚本误将manifest的absent标记当旧文件fact，保护检查在任何intent／Root写入前停止；第二入口只修manifest读取，未重跑子侧21例。测试两TU借Root唯一clock／native，测试中的bios_clock及mode trace替身不再编第二份CPU／计时器。

OPL timer为新的有限数值叶，属于现常驻chip，既不切换Nuked后端，也不改变其原始声音。固定SB16 auto选OPL3，原配置／默认的polling hack=false。reg2／3先按旧interval更新再改counter，保当前trigger；reg4的80仅清overflow，mask／stop／start各保原写集。读状态先完成普通IN、偶数port的FillUp和所有PortRead的额外预算，只有status口才取当前PIC点更新timer。write参数必须是现chip地址模式选择后的full reg，返回1为timer已消费、0才交Nuked；不建第二mode或第二clock。

`F:/fd2sdl-evidence/f5-opl-timer-source-20261118-01/actual01-report.json`（`7ee0f3b5…0f0c`）记录MIT C与私有原官方Timer／Chip各自对17072操作通过，326运行守卫同。Python方程不读C，C核完整状态及double位；未改的官方方法核公共状态／后续status，未读取其private double。原GPL方法仅在隔离C／F参考程序保留，不复制进MIT产品或分发。首次autoexec的INI解析和tlog目录收集失败均保原件；第一成功build有一份cmath缺前置pin，故保旧PE，未执行其C；新build03补齐输入再实际执行，未重跑任何已过C。独立peer `build/f5/opl-timer-peer01/report.md`（`5604269b…bedce`）确认叶数值范围及接线前置；其接口注释意见已修，C tokens不变。

父五路径应用 `build/f5/opl-timer-root-integration01/apply-receipt.json`（`081aeee7…8d951`）后，与IRQ同候选构建App／两新target及直接BIO／0300：`F:/fd2sdl-evidence/f5-irq-opl-root-20261118-01/build.json`（`da0ad5f0…ff250`）通过。八项定向CTest通过0.36s，`check01/receipt.json`（`3158083b…fd41a`）核511实际输入和原件／SAV／旧App相同；只重验新21／timer与共享body直接影响的旧BIO四例／AH9／公共0300，没有历史全批或GUI。IRQ与timer独立fixture分别仍为 `4c1a96e7…a4d7`／`e1da0c86…3294c`。

最新C盘App为 `cdd57ac71d199dcfd4be98a449f345372f4c69cdc9ccdd8143a4944c45de8b05`；**Main SOURCE尚未绑定，F5仍未完成**。正常MDI构造、DIG service／raw401／402源费用、真实图像sink、共享provider和首相位／严格PIC／全非例外输出继续补。全部给定态、源语义、当前Root执行和正常实机身份仍分账，Linux本轮未执行。

### 13h 行 RGB 采样与新的 native 锁来源

`build/f5/vga-scanout-candidate01` 的四新文件只借现VGA索引页／当前DAC，保唯一320×400 ARGB输出cache；两相邻物理LINE分别采样，DAC三分量完整后才更新真实DAC。八份固定官方源、128000地址join和normal零start／skip／pan提供独立依据。START false、abort／未知尾、模式中间态、两latch和source／OS首错均明确分账；没有第二Game页或palette真值。

父五路径应用 `build/f5/vga-scanout-root-integration01/apply-receipt.json`（`838d2719…72f04`）保现IRQ／OPL／MDI。首次父读seal字段不合，在任何intent／Root写入前停止；第二入口按实际 `after` 清单核完才应用。子侧冻结的是配对旧bridge C／H，故其C05不代当前Root ABI。父重新链接当前唯一clock库：`F:/fd2sdl-evidence/f5-vga-scanout-root-20261118-01/build.json`（`322b7077…d4c45`）记录App／新target通过，`check01/receipt.json`（`9c0dd9f0…604e`）记录两新增CTest通过0.20s，457实际输入及保护原件相同。独立fixture仍为 `a52a53e4…3dd2`，两族800行／256000ARGB逐点相符；旧calendar／资源／原CPU没有重跑。

当前App为 `d62488e95e6abcadfa9c6e1e419fc7977bf52578e0bd930260f2a04433c65e13`。该行采样尚未接真实SDL纹理或Main；新的输出适配继续只在源END进行终端作用，不调用会推进输入／clock的App present。首次未写尾不上传，后续abort保既有尾的身份；当前given pixels不能签WGC或自然音画相位。

构造的锁依赖另有完整来源进展：`F:/fd2sdl-evidence/f5-native-lock-dig-source-20261118-01/source01.json`（`b28b9ec9…50d07`）取得五INT386包装及三缺失DIG叶；后续有界stub04只核31h表项的INT／RET。结合既有完整70入口和8B表的独立原EXE核对，固定600／601清原返回CF后直接到44ff恢复，不读取锁区间。`native-lock-qualification01.json`（`c895e457…1ac8c`）及16例原PC／IF／结果槽expected（`f837e42a…b9211`）分开原EAX、source completion和carry写点；不能据此省掉包装／分派／返回费用或改用0300。

这里没有新原CPU／GUI／原RAM读取，也没有相应C consumer通过声明。stub02／03对既有data表的create／显示检查失败保留；04用Domain decoded instruction与strict字节确认，旧data显示另存，不保存数据库或猜测ABI。当前loaded gate、逐源cut的AH／段／地址、原heap和guest ENV、真实Main首phase仍是必需门，具体合同在 `build/f5/native-lock-source01/contract.md`；后续C只借现owner，不造CPU／GPR镜像或0费native成功。

### 13h SDL 终端与实际 400 行 WGC

`build/f5/vga-scanout-sdl-candidate01` 封八文件候选（qualification `d555edfb…d944f`、seal `fd90f983…838c`），父复审后应用收据为 `F:/fd2sdl-evidence/f5-vga-sdl-root-20261118-01/apply01/receipt.json`（`4629fe55…d9c89`）。唯一 `vga.texture` 改为320×400，SDL逻辑640×400直接横2／纵1最近邻提交；不新增Game页、palette、clock或输入队列。END只上传已知输出行并调用终端SDL提交，不再次执行App present／observe。首次未知尾不上传，partial保旧已知尾但不称本轮完整采样。

父复审发现原候选虽记录 `output_error`，公开入口仍可继续observe／泵键；现以同一错误owner的纯getter在入口及真实source完成后检查。END内OS失败保已达source完成点，随后在输入／后继poll前返回ERROR，不伪装HOST或锁死SourceClock。带VGA的deadline等待借现owner逐次检查；无VGA普通等待保持原路径。关闭恢复原logical设置并销毁唯一纹理；这是VGA teardown，不提供中途退回legacy的第二纹理。

实际父五目标构建 `build01/build.json`（`54f13c2d…59c4c`）通过，当前App为 `196b9100cc86ad54ca12774d6d4c22502a5010acac943a23ab29f727730daadb`。新SDL C／Python两门和直接受影响的 `bios_wait`／`clock_error_vga` 共四项CTest通过0.53s；`check01/receipt.json`（`eca4700c…7cacf`）保存660项实际输入和原件／SAV／旧E盘App前后守卫。没有重跑旧calendar／PCM／资源批次。C检查中的SDL终端与clock边界仍是替身，实际设备结论只认下一段。

三次原生入口分账，前两次失败不改签：`F:/fd2sdl-evidence/f5-vga-sdl-native-20261118-01/report.json` 在SDL初始化前因同值环境hint优先而退出，无窗口／输入／WGC；02的phase0两次完整RGB已相同，但后台SPACE被probe额外窗口ID过滤丢弃，phase1未通过。固定SDL3.2.12源码 `SDL_hints.c:105..114,253..314` 证实SetHint失败不代表effective值为true；`SDL_windowsevents.c:1530..1576` 与 `SDL_keyboard.c:510..637` 证实后台无focus时事件windowID为0。父只修probe：核effective禁止激活值，唯一SDL窗口中允许无focus的ID0，拒异窗／其它focus／repeat；不覆盖环境、不设focus、不注入SDL事件。父postfix三个收据及未运行的build03均保留；最终probe独立build04／prepare04，不倒签旧PE。

03实际通过：`F:/fd2sdl-evidence/f5-vga-sdl-native-20261118-03/report.json`（`fd7bdc743fe064da539fcc41f63e106d26d93d36513bd62d7277562dd9a2790a`），outer（`6b904021…fedcb`）。真实probe PE为 `bfbefe96e48a225d5407d9fee39407939fcae722c8ea7be960fa63d40cf1adb3`，prepare04保存470项实际编译／链接输入。两phase各取两个新离散WGC HWND client原图，均640×400／768000 RGB字节，独立公式SHA分别 `789a987a…245fe`／`c6842002…af33b`，四次全部零差，无resize／ROI／mask／容差。真实日志确认Space／Esc的SDL事件windowID0及正常业务ACK；父投递仍绑定PID44368／本轮HWND并保持非前台。

本次显示原已ON，无需唤醒或改电源计划；需求由线程36836取得并在owned退出、守卫核验后同线程释放。Job43876记录normal0、residual0、forced0；原资源／用户SAV／旧E盘App和实际输入守卫不变，不改混音器。此结果只签给定两页的真实SDL终端、后台输入和清理；离散静态新会话没有导出WGC帧QPC，不冒连续帧／PIC相位。probe不加载游戏／存档或Audio，未绑定Main SOURCE、正常03h图像／模式编程、真实VRAM／DAC writer或首phase，不能代签F5全部音画。

### DIG 服务原写点与当前 Root 组合

`F:/fd2sdl-evidence/f5-dig-service-source-candidate-20261118-01` 封25文件（patch `31197f1b…8226e`、qualification `cb271d84…a880`）。同一D／P0／P1、build／DMA、API／0300、SB和clock执行原404c0的busy／flags／half、16slot扫描、mix／formatter、empty／402及恢复写序。P.position只在497e0提交；首错保实际partial build与旧position，不先算完整输出再补费用。其它14个pool slot的free1须有normal producer来源，不按C数组长度2缩原池。

raw401／402共用DSP和DMA owner；C6最后OUT作用真实传输，ab2／ab8才发布原half／state，resume借D4，402不代后继IRQ的D0。旧304 helper只对DMA16更新probe投影，已修其c<4拒门对正常DMA8 channel1的拦截。382d6／382db按完整flags thunk纠正协程误名。公共source start／stop仍缺上游费用：健康启用态显名H_PROFILE、已有首错原样返回，disabled按原早门返回，不零费改P；该正常能力尚未实现，不能算闭合。

父应用 `F:/fd2sdl-evidence/f5-dig-service-root-20261118-01/apply01/receipt.json`（`c1bde097…f9b9e`）窄合冷生成／同native库／新target，保此前VGA／IRQ／MDI。CTest目录另用两文件overlay（`d9f20a9a…f11f7`）：原 `--output` 保精确新目录，`--output-root`只选一次UTC微秒子名，碰撞拒绝、不重试或覆盖；原件／junction保护不变。父overlay收据为 `output-overlay01/receipt.json`（`2df83549…5aefb`），原seal不改。

实际六目标构建 `build01/build.json`（`9ca91306…de23e`）通过，App为 `1752d8e8ee5dcf860ab417f7fdf6546dd14ae035756c596b3cc03adbb09f1169`，Main SOURCE仍关。六项适用CTest通过0.99s，`check01/receipt.json`（`4f21d17b…d4ce3`）核552项实际CL／链接输入及原件／SAV／旧E盘App守卫。当前Root同一probe的26例对上393659次新DIG／rawSB原PC访问、每例完整8192B build＋4096B DMA及本域IO／状态；active-DMA例经真实Audio跨3个边界，读69个旧active字节、left1979。完整实际产物只归档、不重执行：`executed-artifacts01/report.json`（`7bcf5c87…6295c`），归档收据 `97cfa437…b54c8`。

其余五项为新source工具12方法、更新的304冷门15方法、目录协议5方法，以及共同数学体／物理DMA start被移动而直接受影响的external-DIG和DMA回归各一次。没有重播111资源、旧rate／IRQ组、原版或GUI；IRQ／304目标本轮只重编链接。子actual09／10保两PE身份，当前Root26不倒签子actual08 collector失败。父把47d88名精化为 `ail_build_s32_to_u8_unclipped`，C仅首注释变化、其后全字节相同；registry原10项齐全，不重复追加，统一Domain发布待收口。

本组只签显式given的U8双P服务、API／0300及同SB物理效果，未签40cf0正常source安装／attach、25a96／39521／39805等公共cue全部费用、正常warm／IRQ上下文／全日历或Main。后继所缺七根完整源已一次取得：`F:/fd2sdl-evidence/f5-audio-crt-leaf-source-20261118-01/source01/source01.json`（`cab68b45…899f`）；七个音效owner与四个CRT叶分账，整文件11函数／318连续head，无原CPU／GUI执行。

### 正常启动输出闭包的新增静态依据

只读报告 `F:/fd2sdl-evidence/f5-main-source-bootstrap-review-20261118-01/report.md` 区分正常pre13成功域、资源／分配错误文本和未知传递边。真正模式调用是25d7b，八次资源加载与三malloc在它之前；较早SDL资源分配不代原切模。已过03h／13h给定日历不能提供这个before或绝对phase。

`F:/fd2sdl-evidence/f5-crt-output-source-20261118-03/review-receipt01.json`（`7859a660…b7d0`）保12owner／442代码头／1391B chunk及48B初始化表。start@3ccb4首jmp跳过120B版权／静态数据，不能从字符串存在推出正常打印；CON写在另一错误入口。两次连续检查失败保原件：Domain get_between会decode数据头；03改用Domain代码头／item大小、完整raw间隔和CFG分账，不强解码、不保存数据库，也不把带数据的函数签为全段连续代码。

四个补取CRT叶不代全传递闭包：guest ENV／NO87和x87 before、正常FILE／allocator／运输及loader到LE前的画面仍各有边界。无新原CPU／窗口或完整启动资格。无新FD2文字writer不推出StartUpdate=false；原03h LINE／END、旧未完行及13h真实Setup仍照源排队，不重置预算／phase。Main opt-in前还须把78个生产等待点／68个静态业务读点的OS错误沿现VGA owner传播；隔离候选正在修此项，不以读点数量签运行覆盖。

### 正常 MDI 构造候选的末写入更正

正常构造合流前的复审撤回了「cap.env=CS:0并读header」前提：完整raw300在首次cap写入后，又于3d7f／3d85把260／262清零；264=590h、266=CS保留为扩展名指针。正常43ad0因此跳过getenv／strlen／strncpy，仍按 `.OPL` 生成 `SAMPLE.OPL`。来源为完整driver-source03（`83619c91…68b94`）逐byte核原16541B模块；新勘误 `F:/fd2sdl-evidence/f5-mdi-constructor-candidate-20261118-01/normal-capability-erratum01.json`（`fad6d901…4079`）逐store登记。此前WIP／checkpoint的首写推断不再作为正常构造依赖或费用，不改写旧原件。

同份原data的21个角色范围为零，raw300没有后续writer。SOURCE工厂须按这些初态及305／controls真实写点更新；legacy reset的127／64／8192不能提前放进原构造前缀。该修正只改尚未交付的SOURCE候选，保legacy已验默认；旧raw304／native16／305给定态和SCAS九例资格不重跑、不扩签whole构造。SCAS仍用于43c25拼接，AIL_STARTUP／debug／CRT guest ENV是另一来源门，不能因cap.env=NULL一概删除。

### VGA 输出错误的正式 consumer 接线与直接回归

`F:/fd2sdl-evidence/f5-vga-aware-consumer-candidate01/final02/` 的53文件候选已逐一核对 before 后合入；Root收据 `F:/fd2sdl-evidence/f5-vga-owner-root-20261118-01/apply01/receipt.json`（`f3e0468e…0d36c`）记录全部 after 与封件相同。78处正式等待接同一VGA，68个静态业务读点的既有观察次数不变；fixed-now叶仍为纯函数。输出首错只归原scanout，源时钟已完成的前缀不撤销，也不把OS错误锁成SOURCE错误。Game音乐callback改用同步完成返回，负值中止其后阶段／RNG写集。离屏scope检查真实`scope.app`，不以临时target的NULL输出代健康。

Root实际构建App及26个直接变更测试目标通过，`build01/build.json`（`890e35d8…681ae`）；App为`630a483aa3f929eee591358bb2c907a59985c2914b6084669f6410b93ff8567d`，Main SOURCE仍关。`check01/receipt.json`（`d212b887…1f714`）对应34项、42.44s：7个新输出owner故障入口和26个直接旧入口通过，`field_loot`一项失败；1,234项实际CL／link／PE输入及原资源／SAV／旧E盘App的前后守卫相同。它不是34项全绿，也不倒签子包较早6族与最终2项的不同PE。

该失败是旧私有夹具仍要求script7整段预拒绝。原21B脚本@62a1b首组mode2/raw12，夹具count2；F1既有合同保其身份读之前的零组前缀，不能为修测试恢复提前拒绝。新夹具保不存在的script106拒绝，并补script7核心预检→实际move叶、零RNG／无额外人物写集断言；21B另与strict原镜像及原SHA逐byte相等。生产语义未改。`loot-fixture-correction01/receipt.json`保前后源码／依据；只重编该目标，`check02/receipt.json`（`654e67c0…cb53f`）实际单项通过、523输入守卫相同，不重跑已过33项。失败的旧PE仅保当次hash／日志／闭合守卫，未另留该旧PE本体；修后身份独立登记。

该轮复审发现Prepare／Play／Battle等同步音频返回边仍有OS错后的UI或业务后缀，后续修正见下节。原7个输出负例没有覆盖这些遗漏，不倒签为全域错误传播通过。无新GUI、原CPU或音频设备执行；此前SDL两相位WGC只严格继承其给定终端像素，不扩签正常Main、全部producer或连续帧。

### 同步 Audio 返回后的共享错误边界

`F:/fd2sdl-evidence/f5-vga-aware-audio-postfix01/` 的21文件补丁已核封件后合入，Root另窄加一个bridge目标及六个CTest入口；`F:/fd2sdl-evidence/f5-vga-audio-postfix-root-20261118-01/apply01/receipt.json`（`5eee22c8…85710`）核22文件及原件／SAV守卫。Prepare声音／options、Play三类cue、Battle共用play／stop、Scene BGM／transition／teleport／cinematic、标题声音与ANI返回均在真实同步边界检查同一输出owner。普通可选音效错误政策不变；Source／OS优先于HOST，已经读到的键、options及回合写集保留。删除一处Visual重复Source纯查询，不建Audio→VGA依赖或第二全局错误。

调用账 `callsite-audit.json`（`bb8e6384…1ac2c`）的分母是**补丁前来源**45个API CallExpr：42个非borrowed加3个borrowed；三个互斥ternary合为42个同步选择点。24处接新门、19处原已紧随检查、2处由真实调用者紧随检查（ANI hook／Game detail callback）。补丁后Battle等已收敛为共享helper，这个45不是当前源码物理调用数，更不是运行覆盖率；原口头41漏算同一行Scene的另一个CallExpr，明确撤回。构造／终态306另账，其纯边界只经静态和对象编译，不由UI负例签整Main。

Root实际App及10个直接变更测试目标构建通过，`build01/build.json`（`bb7d3c0c…6dfd1`）；新App为`17469698aadfda9fd3d36251252e983e9d14e079577fc0c4ab68573e4a60ef62`。六个新`clock_audio_return_*`入口实际6/6通过，CTest总时0.27s；`check01/receipt.json`（`e1327b60…04409`）核824项CL／link／PE输入及来源／SAV／旧E盘App前后同。不重跑旧34项、原窗口或音频批次。181份仍可读取的构建／检查输入只归档不重执行，收据 `executed-archive01/receipt.json`（`518db96c…560a3`）；其中App和部分仅构建目标没有执行，不能把归档数当测试数。

子包首链接配方漏两正式源、Scene夹具缺PRESENT、标题声音前缀计数误判及消冗暂存错误均保原身份。最终子包六例与712输入／64tlog晚守卫通过，Root六例另有当前候选身份，不混成同一PE。Audio内部在公开返回前的已完成前缀不回滚；这里只拦UI／业务后缀。Main的306是独立终态卸载，按原source成本执行后再报告OS失败，不能当继续玩法。Main SOURCE、正常首phase／03h未完行／INT10／IRQ1及完整非例外音画仍未签。

### 正常DIG构造所需的DOS分配来源

新静态来源在`F:/fd2sdl-evidence/f5-dpmi0100-source-20261118-01/`至`-06/`，分别保存12个新根及共享尾的Domain CFG、日志、只读来源守卫；六包552条引用去重为527个代码头，不作功能覆盖率。它们使用同一retained selector70（SHA `0f720e98…842ce`、原file base9ae0），没有新guest／原运行、寄存器写入或可执行修改；该视图不是LE canonical VA。此前未保存可复用的70数据库，本次在隔离目录仅重用原字节及已证16位元数据解码，不保存类型／数据库。

0100／0101实际根492b／4972经490d／495c、48dd／450e、41ac／c9e运输。c9e由原AH48／49选择table66a的6fc／6fe两WORD，落1526／1564，再调用2cf9／3192；不是0300或600／601的零成本别名。六包SHA依次为`33a54a6b…00f40`、`f191358d…b4a25`、`765ad116…3b9fd`、`a53319e3…f4915`、`b5c6d3e2…9367e`、`6055df93…4df04`。管理器的分支／直接callee尚须按实际正常大小、显式原heap／selector before和状态写序收口，不因取得完整CFG即签正常分配成功或Main初态；仅按必要分支补源，不扩成通用DOS内存／CPU执行器。

后续07保留2299整分支的静态源，但它**不是0100实际选中分支**：490d在4910／4912临时把allocation strategy置3，491d才恢复原值；五份旧运行外态980=0不能代调用内部模式。因此停止扩2299的helper，不将该早期推断签成原义。按完整写序补取08的1538／1570／418a／40c3／3f92五根，共90代码头（SHA `36960484…00012`）；实际callee与typed before仍由同一构造残余账收口。07原件和旧checkpoint推断保留，08不构成新原运行或正常Main资格。

09补上述明确路径的六个descriptor helper（98头，`7ee8b872…840f6`）；其899实际仍调用原INT31入口，故继续按原表只补0000／0001选中的4541／45f8（10包107头，`1292c739…1877b`）及4508普通验证wrapper（11包33头含已知共享尾，`7d76d2cf…17316`）。这些相交CFG不相加作唯一覆盖数。已有D73／18:1068..10d7／mode来源复用，不重复取源；实际native DOS AH48／49还需MCB的原分配／释放写效应、同CB21／A20／CF尾及合法GDT before，不能以零成本成功代替。

### 校准延时与正常 pre-main 相位的新增来源

既有CRT source03的`_InitRtns@460ea`在callee返回后才写初始化表state2；正常`__delay_init@3dfef`先等DOS秒改变，再计数至下一秒变化，写唯一541B0。版权字符串被正常跳过、没有已证FD2文字writer，均不能据此省略这段正常pre-main时间／旧03h队列。

新增只读完整源 `F:/fd2sdl-evidence/f5-delay-consumer-source-20261118-01/source01/source01.json`（`8676781c…2d8f3`）核`__delay@3e01d`的27代码头／59B及全部strict原字节。非零ms做DWORD乘541B0、加500、无符号除1000，结果0时至少一次；循环每次实际AH2C／INT21、CMP、DEC／JNZ，不能以固定宿主时间或无费用调用替代。零ms有真实序言／返回、无INT21；旧`3b765`为legacy标签而非另一个canonical函数。

固定官方host的**实模式DOS分支**经真实INT1A读BDA046C并清0470，再按原double求时间。该分支的`overhead()`使IP加2，从CB_INT21的快IRET跳入PUSH／MOV／320次LOOP／POP／IRET慢尾，共324个尾代码头。它不是正常FD2保护模式所有AH2C调用的默认费用：新70:149b完整源有直接BDA分支，受DS15 bit0与DS34控制；五份既有有界运行外态均为30h／15h，选择快分支，但这些外态不证明全部启动writer或当前Main before。该分支在各真实读点依次取046e、046c、046c，不得提前快照成一个DWORD；D9F复制原返回frame的IF，和C9E入口的IF0分开。原件见`F:/fd2sdl-evidence/f5-pm-time-source-20261118-01/`及`-02/`（`8d4b3279…ae3e`、`a4ed3d13…913`）。

正常BDA／校准字段／budget／IRQ／旧未完行仍须来自同一真实owner，不能设固定校准值或frame0。F2共享等待主体、当前VGA-aware墙钟deadline及给定日历不代该资格。实模式组件的局部通过不扩签保护模式运输或正常启动；本轮没有新GUI或原CPU执行。

### 共享堆与正常 MDI 构造的 Root 合流

正常构造release02与独立共享heap包经逐文件before／after核对后已合入，Root的`F:/fd2sdl-evidence/f5-mdi-constructor-root-20261118-01/apply02/receipt.json`（`fa4c29ca…1e58`）记录29文件。唯一`source_heap.c/h`负责有限单环自由节点的分配元数据，MDI与DIG借同一堆和时钟；原payload与完整堆字节不由该元数据接口证明。冷定义中未被CPP消费的三函数／127头已从MDI表移除，由`source_heap_costs.json`唯一维护；当前MDI表为40函数／3016头，原块内容和新生成器的旧schema输出逐字节继承核对。

构造从raw300的最终cap.env=NULL、`.OPL`和21个零初角色出发，Source路径保305与controls的真实写点；legacy默认不变。配置对象只在调用期间有效，driver保留实际live preference指针，后续公开play读取最新值；cache_mod32来自实际pool地址加d0h。复审另修`load_ail_bank`在vector分配失败时泄漏FILE的错误边界，不改正常解析。43ad0规范语义登记为`ail_mdi_driver_construct`，完整IDA发布仍待本阶段统一生成，不能因输入已更新就称发布完成。

`build01/build.json`（`f6674013…ab7`）实际构建App及五个直接目标，App为`7e6b616ee910f00a7900187a0d5f9012bc1cf2e9053009899167454ef8c7da3f`。`check01/receipt.json`（`d0eb4624…93f8`）五项通过、1.35s，776项CL／link／PE输入和来源／SAV守卫同：当前heap四例、七项工具检查、bank失败四例、构造八项工具检查及完整构造→live pref23→443d0／503首FILEseek前缀。普通构造三例沿用其独立早期PE；本轮未注册或重跑，不签同一候选整组。sample-file／options只链接未执行。116份可用产物按实际身份归档，未缺失；App未开窗。首apply的旧注释断言和首prepare的schema参数错误都在执行测试前失败，保原件，不记为原版或安全拒绝。

### DOS 内存、校准延时及共享 native 的 Root 合流

三个封件共25文件加窄CMake接线已合入，`F:/fd2sdl-evidence/f5-mcb-time-native-root-20261118-01/apply01/receipt.json`（`b4569384…16c5`）。AH48／49借同CB21、A20和时钟处理合法完整MCB链的first-fit、压缩／拆分／释放及AX／BX／CF；内存分支不继承文件的disk delay或unmask0。此native first-fit与DOS4G内部暂用strategy3是不同层，不能混写。未知外部内存不伪造error9，当前Main的MCB／PSP／GDT初态尚未签。

延时只借唯一541B0校准字，按52个独立来源头执行零ms、DWORD回绕、逐AH2C和校准末写点。实模式time层借同CB21与唯一BDA／0470 owner，保真实INT1A、回调、IF、date与慢尾；AH2C没有CF writer，不擅自清零。正返回的非法provider完成值锁存具名SOURCE首错，不补尾。该组件没有接成正常PM AH2C的替身。

`native_source.c`收敛五个LE wrapper与retained70共同入口／返回，CPP原核改薄适配；600／601及0100／0101使用有限typed接口，0100的具体descriptor／内存body仍另验。93个LE／45个70头及两个stub和BGM的35函数／2923头分开，LODS仅新增bare单元素族，不扩成VM或第二寄存器owner。

Root `build01/build.json`（`40a831c5…9aeb`）六目标构建通过，当前App为`a2146905c194defdb99caddd1b0de437b3b6b276ea9fbf0d989ad8f4de02930c`；Main SOURCE仍关。`check01/receipt.json`（`3fbbb3cf…3a6b`）八项通过、0.79s，635项CL／link／PE和来源／原件／SAV守卫相同：MCB12／共享CB21六例、延时14／RMtime八例及非法正返回三例、共享native两完整轨迹／四LODS边界／两失败前缀，加独立工具检查。constructor／sample-file只链接，不重跑旧file15／normal3／GUI。`executed-archive01/receipt.json`（`922bce1d…2e18`）保116份实际产物、缺失0；归档数不作执行数。

该次复审发现固定host的MOV／POP SS有同预算`cycles++`，当时的普通head投影尚未包含；正常PM后半链可恢复IF1，不能以旧given PC数或全IF0假设忽略。后续共享修正和新的局部资格见下节。真实Main启动／IRQ cut producer、严格PIC与非例外完整波形仍是本阶段待办，不因这八项通过关闭。

### SS 预算、IRQ 栈宽度与 pre-grant 借用

唯一`source_clock_ss_loaded`只在原已获准的MOV／POP SS点对同tuple补`cycles++`；不新增grant、shadow、队列或IF状态。MOV按合法SetSeg前、POP按成功后执行，LSS没有这项host writer。新`source_ss_budget`直接验证待处理中断／日历边界、负预算、溢出与冻结：`F:/fd2sdl-evidence/f5-source-ss-budget-root-20261118-01/check01/receipt.json`（`1db827cb…94b2`）一项通过、348项输入守卫同；49份产物归档、缺失0。

全部现consumer的静态join见`F:/fd2sdl-evidence/f5-pm-int21-time-route01/ss-consumer-closure03.json`（`4ddfe14a…b0a1`）：19模块实例／18个唯一view-PC，不作功能覆盖率。IRQ80为646／6b8／ecb／ee0／c41；BIOS为80:c41、70:46e／4e3；bridge为real:28ab／298／1fa4／1fb7／284c／2ed、70:4bdf／4ca3；AIL为canonical3eacb／3eb63／3eb97。原源存在但未被当前C消费的80:d5f、70:9de等另列库存，已删除其冗余条件，不把库存当实现。

第一轮五项直接transport检查通过、0.11s，524输入守卫同（`F:/fd2sdl-evidence/f5-ss-transport-root-20261118-01/check01/receipt.json`，`d2c624dc…ab95`），当时App为`17b12c35…1513`。随后独立复审从原contract01完整raw17确认normal0300还含real:1fa4；旧五点候选及PE保留，不回签完整费用。修后正常0300确为六个SS补额，head数不变，夹具按独立原字节／源宽度修正预算，未按新C输出拟合。

另一个独立差异是IRQ front的SP。完整same-CPL386 gate／nearCALL及PUSH／POP净变化为26B，80:59b／59f实际消费低WORD(S−26)，此前直接比较入口S。原推导和12个合法A0／C8给定值见`stack-width01.md`（`4361b2ae…80a0`）及同名JSON；这些例的真实栈范围为6fe6..6fff、在已证7cff limit内。原W=6ff0／S=7000应写6f68，旧实现会写6d28。修复保原对齐／界限顺序、SS16高ESP和SS32语义；数学下溢示例不冒合法原栈执行。

`instruction_at`／`string_at`在每次真实同步grant借C栈的必要投影，nested保存／恢复指针，plain调用标为未知。IRQ入口才能冻结该次字段；AH只需原返回读者的==9谓词，SP只在kernel重叠分支需要，CS／IP只在diagnostic ring启用时需要。没有持久GPR像、PC游标或默认Main cut。nested element／IO期间可能恢复等待中的外层借用，getter不可当任意effect的当前PC；其自身不派IRQ，下一grant重新提供本次cut。

新Root `F:/fd2sdl-evidence/f5-source-cut-stack-root-20261118-01/check01/receipt.json`（`dfd9001a…01fd`）三项通过、522输入守卫同：pre-grant借用的IRQ／nested／unknown／string／故障，12个合法SP边界及修后六SS的0300直接组；旧21／构造／GUI未重跑。该轮App为`d8b443c9…2429`，90份可用产物归档、缺失0。静态peer review另存`build/f5/source-cut-peer01/`（seal `e7e85072…20c8`），无当前同步effect合同内阻断项；建议的nested string与IRQ内首错展开另补新入口。`F:/fd2sdl-evidence/f5-source-cut-nested-root-20261118-01/check01/receipt.json`（`cdc2322a…4947`）只执行这两个新场景的一项CTest、485输入守卫同；64份产物归档、缺失0，不重复先前三组。当前App为`50b2d4dea7c17681b15c75d30d18a2350f030c7209721aea298da123794c020d`，Main SOURCE仍关。

### 正常启动外层的来源续接

`build/f5/source-main-minimal-dependencies01/`的只读报告（Markdown SHA `4fda8f11…4aab5`）核现Main仍为legacy attach→Audio→BGM→PCM，已有source factory不是正常整层bootstrap。报告区分已有完整callee、七个尚未定位的外层、真实初态／旧03h队列／INT10／IRQ1和原退出费用，不以API或给定masked日历签自然Main。原构造费用模型也须补独立SS subeffect，旧PC／写集／PE资格保原身份，不按末尾总差拟合。

Parent随后一次只读Domain取得这七个明确缺口：3aa72、3ab49、3aca3、44180、44240、37b8c、3f4e0，共347个代码头，完整chunk／CFG／xref／strict原字节均保留且连续，无新语义、ABI或运行资格。来源`F:/fd2sdl-evidence/f5-main-audio-callers-source-20261118-01/source01/source01.json`（`f957292a…f7ff`），执行收据`execution.json`（`7848824d…65ce`）；无数据库／类型保存、原CPU、guest或GUI执行。七个root不再列静态来源缺口，正常caller接线、实际branch与before仍按原完整门继续。

按上述caller的真实直接依赖另补四个INI／文件／通用驱动叶38463、36cab、36c56、38f2b，以及原5275c初值所指free3776e→nfree3777e→MemFree3d670：`F:/fd2sdl-evidence/f5-main-audio-callees-source-20261118-02/source01/source01.json`（`54aaf638…e01a`），七函数548头、完整strict／连续。5275c的静态值不证明所有运行writer，normal binding仍需资格。当前共享heap仅allocation单free节点，不能以host free代真实释放、freelist合并与后继分配费用；正常扩展继续归同一个owner。

INI的3f656完整parser已在G1既有闭包，不重取。二进制文件叶下的Watcom open3d074、read3cf9b、close3d36c、filelength3d3a6原先只是library stop，本次单独补其四函数153头：`F:/fd2sdl-evidence/f5-startup-file-crt-source-20261118-03/source01/source01.json`（`2b4d8bfc…6fee`），strict／连续均核同；执行收据`316fbed1…9afe`。这只补静态依据，不把下层CB21已证或host FILE成功当完整CRT费用、合法handle映射或正常创建失败合同。直接callee先join既有完整源，缺者按真实正常分支续接，不扩成整套DOS／CRT模拟。

### PM AH2C 快路径的 Root 合流

七文件封件`F:/fd2sdl-evidence/f5-pm-int21-time-route01/final01/`经原件／before／seal核对后合入，Root另窄改CMake，`F:/fd2sdl-evidence/f5-pm-time-fast-root-20261118-01/apply01/receipt.json`（`245fc23f…3d9c`）记录八文件。只在真实DS20:15／34选择fast时执行149b→D9F→三次独立BDA WORD读、原整数算序、两个结果WORD写点，止于1475→D18。返回flags来自原frame而非当前IF0；callback正完成、BDA失败、DIV边界及两个结果的晚错前缀各有独立模型。未知fallback到1478记为缺完整RM运输，不能称原版拒绝或U。

Root实际构建App及唯一新目标，`build01/build.json`（`efb3f93c…fb1b`）。三项新CTest通过、1.01s，实际273个源／WORD前缀例及六项纯检查；`check01/receipt.json`（`bf90c398…2950`）的489项CL／link／PE与来源／原件／SAV守卫相同，未重跑旧私有PE、其它历史组或GUI。64份可用产物按当次身份归档、缺失0（`executed-archive01/receipt.json`，`a4074721…338e`）。当前App为`838b5772ec1c6669e821f1d2aaa51f60fa4221ea712bc95e04dfe92eec3dc865`，Main SOURCE仍关。

完整PM21入口／返回、D1F后活动IRQ cut、同kernel 996／99e／1182／F52／workspace的临时写集以及normal配置writer仍继续实现和取证；fast叶不直接绑为delay.time21整链成功。新运输只借现kernel及同C栈必要投影，不造第二份clock／通用GPR frame。旧五caps的30h／15h仍只是其有界运行身份，不能作为所有未来启动的默认值。

### native 回调已锁源错时的展开

共享复审补到一个可复现的C接口缺陷：native hook已对同clock锁存SOURCE首错，但返回completion0时，旧代码只看返回值，仍可能推进Tick、报告IRQ完成或回收未完成的event再取指。它不是原版玩法反例，也不把普通OS输出错误当SOURCE。`F:/fd2sdl-evidence/f5-source-latched-error-root-20261118-01/before/check01/receipt.json`（`70d1fd89…6ac6`）保存实际失败、354输入晚守卫相同；先失败于await_tick前缀断言，没有把余下五例记为执行通过。故障PE／库／tlog共49份保留，缺失0。before源码另从早先已冻结的实际文件按当次原guard SHA和size逐字节核同后保留，没有从修后代码反推。

修正仅在await_tick、mixer_tick、IRQ、SB_END、SB_DAC及external handler返回后优先服从既有first_error；正常0／未锁错和原负返的路径保持原序。已到达的queue pop、tick预算或ISR入口不回滚，未完成的event不伪作成功free。`after/check01/receipt.json`（`ee385895…e16f`）一项新CTest的六类边界全部通过、0.03s，481输入／来源／原件／SAV守卫同；旧source／构造／GUI组未重跑。58份可用产物归档、缺失0（`32ec5569…8d5d`）。新App为`b706a9d116b6b5def88aea87e9e2fba34a275350b3f15fdde7e569135956b047`，Main SOURCE仍关；正常运行、原CPU或完整F5不由本接口负例签署。

### 共用 Watcom 栈检查与真正的堆 metadata

`source_stack.c/h`把3702f／37042的有限控制与费用从DIG依赖中分离，借同clock和调用局部cut，不读写任何原版或宿主栈、不操纵返回地址。caller的PUSH imm／CALL两头另收；完整probe有11头，但正常有空间只走其中七头，加wrapper四头合计11，原口头“15正常头”勘误而不改旧收据。`source_stack_costs.json`以两个既有完整源／strict字节为唯一输入，普通构建不读取原资源。请求显式提供frame、真实probe ESP、live52814／52794及可选已资格cut／code delta；没有默认Main栈或room1到固定ESP的转换。

正常有空间与另一SS的合法返回已实现，同SS不足走到3705c后以具名TERMINATE保留3705f终止支路缺口，不假原版正常返回或U。`F:/fd2sdl-evidence/f5-source-stack-root-20261118-01/check01/receipt.json`（`65593b11…2a99`）的11个新族通过、493输入守卫同：不同帧、完整PC／SP／IP／AH谓词保存、foreign SS及metadata／horizon首错。64份可用产物归档、缺失0（`f770c20c…25fc`）。App为`781bab5e…44e6`；原有DIG薄调用须先提供真实元数据，不能用这项能力签所有caller的栈初态、ABI或Main。

共享heap的metadata／free九文件封件已合入，Root另窄接CMake，`F:/fd2sdl-evidence/f5-heap-metadata-root-20261118-01/apply01/receipt.json`（`993b0483…f7c8`）核十文件。唯一控制字段与同heap已初始化tag／free-link／sentinel相配，不新增allocated-list或另一套堆；metadata访问口检查所属范围／4B对齐，未知不能返回0。真实free、两节点再分配、双向合并、physical walk和list search均走原读写点；在metadata域free_bytes仅是旧projection死字段，不作第二大小真值。旧metadata=NULL单节点投影保持其原有限资格，不能用于非NULL free或Main的多节点堆。

独立原模型的新physical-walk例发现WIP漏了3d6f7的tag再读，首次失败原件保留；修正后12例按三个子包PE分别封存。Root当前六函数267头与strict原镜像一致；四个其它cold schema输出逐字节不变，旧三函数／CFG及旧allocation主体（仅符号改名）亦核同。`prepare01/receipt.json`（`849bae98…79cc`）保存这些继承界限，没有用新生成器自证语义。

`build01/build.json`（`61922a29…931b`）实际构建App和五个直接目标；除新metadata目标外均只链接。`check01/receipt.json`（`d0701941…9260`）三项通过、0.49s，784输入／来源／原件／SAV守卫同：12个新metadata族、五项新工具及七项直接修改的cold／协议检查。旧四C例、构造、raw、native、IRQ和GUI均未重跑。116份可用产物归档、缺失0（`9d8e3838…af67`）。当前App为`ddc3abee23c4407f7b80e8fdfb71aad5408900f10799c7485562edf846d3b084`，Main SOURCE仍关。Main的真实heap初始化、ExpandDGROUP及IRQ重入条件仍须沿实际来源核定；较大的given arena不是默认初态或已证明不grow。

启动文件闭包再按真实直接依赖补sopen3d093、seek wrapper3cf50、errno46632、IOMode466a2与SetIOMode466f4：`F:/fd2sdl-evidence/f5-startup-iomode-source-20261118-04/source01/source01.json`（`46973b09…2b85e`），五函数274头，strict／连续完整。只补静态依据，具体IOMode／handle初态与正常路径费用继续接原owner，不代签运行或扩成整套CRT。

### 调色板逐头写入与部分 DAC 前缀

新 `palette_source.c` 共用已有 `source_stack`／SourceClock，以 `11d40`、`11df2`、共享尾 `1951f..19522` 和真正的 `outp @37ae5` 为来源。唯一冷输入含 123 个产品头；112 个 palette 头、7 个 outp 头及4个返回头的严格原字节分别核对，不把生成头当语义 oracle。来源准备记录为 `F:/fd2sdl-evidence/f5-palette-source-preparation-20261118-01/report.json`（`a5189869…542d`）；outp 的完整来源为 `f5-palette-outp-source-20261118-05/source01/source01.json`（`57e3cbc5…4e5`）。先前 `37c9c` 是诊断日志函数的线索已在编写产品代码前排除。

源费用与实际效果逐点交错：`MOV AL` 保留 AH；第一次 index OUT 不猜入口 EAX；每个基色读取前重新消费原 `53a65`，已取得的局部指针在两头之间保留。SS／SP／AH==9 和已知 CS／IP 在真实 grant 前投影，栈 helper 仍唯一，未访问 guest 或宿主栈内存。普通 OUT 费在真实 handler 前，缺 provider 或晚错保已完成的端口及 DAC 前缀，空区间仍走原序言与返回而不要求 palette／OUT。没有默认 Main 栈、IO 设置或初相位。

`F:/fd2sdl-evidence/f5-palette-source-root-20261118-01/prepare01/source.json`（`8df91b11…f331`）保来源／冷数据；`build01/build.json`（`31ae5daa…220f`）实际构建 App 和新目标。`check01/receipt.json`（`7af13886…0899`）两项 CTest 通过、0.10s，497 项 CL／link／PE／来源／原件／SAV 守卫同。独立字面原 CFG 模型给 34 例／41065 个源头，另有 horizon 与既有首错前缀；逐 grant 的 SP／AH、端口顺序／值／费用、全部 768B DAC 和未改 base palette 均核同。每个成功或 R／G／B 失败后缀另经真实 LINE consumer 比完整行；这是给定 before 的部分写入／行读取资格，不是按自然日历采到该行。

该轮 App 为 `3c0d4036cf8f6fa971c4a547d554084be0f09e0d856d455f0a2a2e0ef5149604`。64 份可用产物归档，缺失0（`02b830d6…43b5`）。归档模板的 scope 沿用了旧“五项”字样，实际只执行上述两项，另存勘误，不倒改旧原件。没有新原 CPU、原版窗口、GUI 或真实 Main 绑定。固定 host 的 full-load=true／6bit／pel-mask／render-on-demand 配置、真实 DAC 控制状态与 deferred 更新及正常图像 writer／完整扫描相位仍须闭合；最终 RGB 正确不能代签这些门。

### 0100／0101 的共享内核合流

独立封件 `F:/fd2sdl-evidence/f5-dpmi-memory-source-candidate-20261118-01/` 的 12 路径经 manifest／seal／before 核对，Root 只另改窄 CMake；`f5-dpmi-memory-root-20261118-01/apply01/receipt.json`（`372ded7a…8098`）记录13路径。descriptor／40B frame／996／99e／1182／F52、mode、CB21 和 MCB 均借现唯一 owner；F52 必须与 IRQ kernel 同一指针。不建立第二 heap、CPU、时钟或寄存器像。只有 1..4096 paragraphs、一个 descriptor、已证 mode3／paging0 与控制 flags 的有限正常路径；未证服务明确为本实现 H，不冒原程序拒绝。

`prepare01/receipt.json`（`71def781…6cc`）核旧五 schema 和 native 生成头逐字节不变；824 产品头、927 reference CFG、1144 字节锚及四个原表分别计数，不作功能率。9 个 mode 的运行时 after 字节只继承旧封存合同，本次可复跑源码核验只证明其 file-before，不称当前 loaded／BSS 身份。D73 头注释 D9d→D9e 的父侧更正只改注释，正文与 Media 冻结版本逐字节相同；CLD 与 RET 的真实费用均原已实现。

Root `build01/build.json`（`48dd22d0…ee31`）构建 App、新 memory 目标及 native 链接目标；`check01/receipt.json`（`5fd61c7f…0af7`）两项通过、6.54s，538 项真实输入及原件／SAV 守卫同。12 个完整 source-CFG／全元数据／返回及失败前缀、新工具九项实际执行；未重跑旧 descriptor13／words9／math4、native C 或 GUI。77 份产物归档，缺失0（`2660f35f…4021`）。该轮 App 为 `312f3465ebe021880593c66499d5e94af7d39e1b55aafdf90bc536f8d6f81036`，Main SOURCE 仍关；真实 GDT／MCB before、活动 caller cut 和绝对 PIC 未签。

### 公共 sequence 分配包装的合流

新 `3aca3→44330→43160` 封件 `sequence-release02` 接现唯一 S／API／AIL／clock；不重建 S 或填原 EVNT／volume 等未写初值。正常首次 index0 的 status1→2 和 reset、pool0／唯一槽占用的无 handle 返回、两种 funding 失败、debug 未实现前缀及 depth DWORD 回绕均保原写序。结果是否非 NULL 与 C completion 分开。`3916a` 六头共享尾和已有 `ctl_defer` 只提取／薄导出，未为 C 分解增加原 CALL，既有算法正文另核相同。

`F:/fd2sdl-evidence/f5-sequence-allocate-root-20261118-01/apply01/receipt.json`（`9e921c61…e305`）记录14封件路径、窄 CMake 和 `3aca3` 命名输入。首次自动 merge 非0／stderr 空，在写回前停止；不能据此认定文本冲突。后核两者改不同函数，按精确 base 替换 `generate_fd2`，保当前 DPMI 分支和 dispatcher。`prepare01/receipt.json`（`3ee38df5…bf46`）核其余五 schema 和 native 输出相同。目录由40 owner／3016头变41／3081；其中共同 native5／93只作为同一输入的另一投影，BGM 实际成员由35／2923变36／2988，不能把两投影相加。

Root 正式 App 首次 `build01` 发现 private 头读 opaque clock->ail，而候选测试 TU 先 include owner 掩盖缺依赖，日志（`ffb2c175…4ac9`）保留。父仅在该 private 头补 `source_clock_owner.h`，没有放松校验或改变函数正文；封件原物理身份不改签。`build02/build.json`（`de9f4e5d…5204`）正式 App／新测试／constructor 链接通过，App 为 `3d236b889a166cbd2e408e1969b3a096338c8610f4ca75ad8513719ccaef6fe3`。

`check01/receipt.json`（`16ae0d86…5cb2`）实际两项通过、0.50s：七个新场景／2473 PC 及五项新工具，696 项输入与原件／SAV 守卫同。其 requested 列误写不存在的 `native_source_tools`，不能据 exit0 称三项已跑；只在 `check02` 补实际注册的 `native_source_protocol` 四项纯检查（`a1085cab…cf8`），显式核注册名与实际启动，不重跑七例／五工具或旧 native C。77 份可用产物归档，缺失0（`aa833881…1339`）；归档模板写三项的字样由独立勘误限定为原轮两项加后轮一项，原件不改。Main before、S 的全部1748B、其它 pool index／cache 对齐、IRQ cut 与自然启动仍未签。

Main heap 已沿真实来源发现初始空 region／自环 sentinel 会要求 Expand，不能用较大 given arena跳过。新增只读来源 `F:/fd2sdl-evidence/f5-heap-grow-callees-source-20261118-06/source01/source01.json`（`5d7287d5…928f`）补 `__LastFree @3d77b` 的12头和 `__brk @467c5` 的64头；原 `__ExpandDGROUP @3d842` 151头复用旧 F3，不重复取得。AH=ED 的原结果和 PM AH=4A 的真实 handler／native resize 继续作为具体依赖，尚未以 capture／伪 C 猜 EDI 或 brk 成功。

### 完整 PM AH2C、delay CF 与真实 IRQ 入口

22路径封件 `F:/fd2sdl-evidence/f5-pm-int21-transport-candidate01/final01/` 经 seal／before 逐项核对，Root 只增窄 CMake，`f5-pm-transport-root-20261118-01/apply01/receipt.json`（`f1b5c31d…2d1f`）记23路径。运输经原 `80:84→550→BA4→88:D67→80:08a0→70:C9e/149b` 到完整 `80:6d5`，沿原读点检查 IDT／node／managed gate／表／descriptor，借现 A0 scratch、原 caller 已初始化的 source slice 和 App 写回窗口。996／99e／1182／F52 及40B frame仍借同一个共享 kernel，不新建31WORD寄存器像、返回地址执行器、CPU或clock。

149b 保唯一分支权威，实际到1478后才要求 fallback；该后继接共享 D73／mode／RMtime／INT1A／0470／CB21慢尾，不直达 native 时间冒 PM 调用。`delay.time21` 明传原 CF：calibrate 每次0，wait 首次0、后续由原 CMP／DEC 写集得来。旧 delay 测试只迁移 callback 签名并编译／链接，没有改 golden 或重跑22例。D1F 开放 IRQ 后，每个 head／string 重取给当前 C 栈 SS／SP／CSIP／AH9；同clock实际 IRQ 入口冻结其谓词，BIO094f不能晚读活跃driver或借最初App cut。

Root 的唯一 runner 增 `--output-root`：每次 UTC 微秒 kind 子目录保当前 raw，沿原 `run` 的 resolve／保护目录／拒覆盖／首错／late guard。`prepare01/receipt.json`（`4eb4d544…4ced`）核旧38头 header 逐字节相同、原 cold 字段相同，runner 的 `fact/same/run` AST 不变；两个新工具检查核连续运行不覆盖旧件及原资源路径仍拒写。不是重写 oracle 或复制另一个 runner。

`build01/build.json`（`21ad12f3…eced`）五目标通过，App 为 `27733078d4e3b2b785895f4cc413c9b868375f1b25baf6a2c7c59f4b71d60a35`。`check01/receipt.json`（`5f4de38d…364a`）四项 CTest 通过、1.07s，658 项实际编译／链接／执行与原件／SAV守卫同，已显式核请求测试名和实际四次启动。实际16个完整／失败前缀、六个IRQ冻结入口、四个CF完整delay组合及九项工具通过；旧273／IRQ21／Time22和GUI未重跑。当前三组26例的55份raw／stderr／report按逐byte及目标输入身份另归档（`raw-source-C01/receipt.json`，`0bfa4443…23ce`）；103份PE／LIB／PDB／tlog归档、缺失0（`9778f176…0c6`）。这些是分层产物数，不累加为功能覆盖。

新增第15场景是在完整 PM call 内预置正常 pending IRQ0：原 D1F STI 与下一条 D20 后、D23 前，借当前 SS=A0／SP=W−274／CS70:D23／AH9=true，实际进入共享 IRQ→AIL→BIO，再续完PM；1412源头、BDA10000→10001、旧AH getter调用0，F52／996／99e按原恢复。它证明该给定 before 的真实插入与返回，不能冒原版实机或任意Main初相位。全部full case的65,666B是原已初始化typed语义窗口，并非完整guest RAM或所有机器栈字节。

Main loader／GDT／IDT／配置writer及真实caller before、首相位仍未签；fallback D73／mode／Time 的plain grant区间仍缺活动cut，目前只签显名masked日历。若该处出现真实IRQ，strict reader须保缺字段前缀，不能继承outer值。Main SOURCE仍关，绝对PIC及非例外波形仍是完整F5门。调色板另有只读窄复审 `f5-palette-source-peer-20261118-01/report.json`（`873efa51…fb45`），八个输入末守卫同，无该有限合同内阻断项；0新C／原CPU／GUI，仅继承已有34例资格。

堆增长的源域继续收窄：新 `f5-pm-dos-resize-source-20261118-01/source01.json`（`97cead1f…3503`）只补 `70:1589` 23头，真实callee `2f29` 在第二份来源237头（`6e794fd4…58af`），直callee仍按实际分支复用现来源，不把所有16个都列必须实现。第二份包装首次参数替换断言在IDA派发前失败；修后只执行一次新的只读Domain，旧件不改。复读F4的完整AH30合同只能排type9，尚不能决定FF00的0／1；FF经INCAH回绕到原表项0，不同于ED走CFC。`52832` 的真实writer、OEM分支与有无必要resize先按来源判定，不用宿主DOS默认或伪C变量名猜域。

### 正常堆域与 IRQ1 来源的续接边界

新五根有界来源在 `F:/fd2sdl-evidence/f5-irq1-oem-roots-source-20261118-02/source01.json`（`c47ac316…163d`）：70:42ed五头、70:24和80:24各一头、70:8dd二十二头、real_copy:12cc一百零一头，按view分别保存。四IRQ根借原400／550／5220／mode，不重新取已有callee；两个24 veneer的CALL后09是原中断号数据，共同handler以IRETD回到被中断代码，不能按普通RET把09继续解成指令。首次01的create断言没有记录准确view／root／EA，失败原件保留，不倒签具体失败位置；02按这项已有源合同停止该inline数据，严格raw byte门不变，输入前后同、overlap0，无原CPU／guest／GUI或DB保存。70:42ed的raw far target是真80:24，IDA生成的`unk_824`名字不作地址依据。

`f5-irq1-source-inventory-20261118-01/report.md`明确当前确有正常实现缺口：SourceClock拒IRQ1，route也只0／7。原96槽controller、0.300f升double的Transfer、p60背压与1ms repeat ticker不等于已有15槽BIOS FIFO；IN60先lower12再lower1，不能因普通键没有AUX数据就删掉前者。旧五RAM仅有界读345B和另50B的尾指针，保元数据守卫，不假新运行或完整16MiB重验。INT15实际先过0c5d:12cc，再由CS:[12c4]链到stock F000:cba0；12c8曾被误当旧尾的假说另件更正。IRQ1 ROM的真实7304为JNC，不能按官方注释的jc签分支。普通handler、完整controller producer、slave12／cascade2必要状态与Main checked输入泵仍在同一F5实现，不把给定p60或支持位当完成门。

堆类型不得只由最后一层得值：抵达70并保DX0078时，08b4→08dd的正常局部路径会写AXffff、保原CF1，并由D18吞掉内部return；该局部费用／CF不代表正常完整FF00。新增原两表复读显示12f4[ff]=18、13f4[ff]=0，实际前层须先到88:e41。其完整128头来源 `f5-pm-oem88-source-20261118-01/source01.json`（`36fb89a3…ec5d`）已取得：FF00与DX78命中E54／E59，在本层写4734ffff、清CF并跳213e，不去70。正常CRT只消费AL，但完整保存／拷回／caller合同仍由同一join封口；不能以局部相同AL忽略原CF和全部费用差异。此前一次“完整正常type1已证”的进度表述过早，已当场更正；旧来源原件和F4仅排type9的范围不改签。

为该条件分支取得的 `3d79e` 六十六头（`f5-normal-heap-grow-source-20261118-07/source01/source01.json`，`93b21e06…17b5`）及 `70:4a77` 三十二头（`f5-dpmi-linear-alloc-source-20261118-01/source01.json`，`1c92ba22…f09e`）仍是原源资格，不是Main已执行或类型已无条件签。前者直INT31 0501，不得多收五INT386包装；SBB之后NOT保ZF，随后的JNZ消费SBB的标志，不能按伪C逻辑值反读分支。后者真实41ac／2982／4a6d等caller依赖继续同源收口。已有InitRtns顺序补充 `heap-startup-order02/result.json`（`0fb97ec0…f418`）只读核12函数／48B表／8记录：ENV的首次分配早于49acd，Main的32KiB min不能倒灌为首grow默认。真实初heap、前Main ENV／region写集、当前source成本与正常启动仍未完成。

## 3. 当前接线顺序与完成门

1. 正常续档的有限descriptor参数已取得并独立资格，整体TMP守卫失败仍保原身份；不为抹掉该失败重跑同一窗口。未来守卫按已证写集修正，strict ON、来源／SAV及owned规范不变。
2. 单clock／主线程transport／MDI服务已接线。继续迁移直接受影响的控制／取证／设备测试，保原独立golden；实际DIG安装必须由真实owner成功条件驱动。安装相位、callback内CPU／端口耗时与严格PIC仍补独立证据；释放不擅自清原pending。
3. 用实际参数对齐共享 PCM 叶及 DMA 加和／夹幅／host SRC；独立字面／整数模型覆盖容差、半相位、服务边界、loop末点、两个 handle、replace／stop／取消／失败。源和 C expected 不互相生成。
4. 对所有正式 BGM／PCM cue、驱动分派与资源绑定批量合签；只在新时点／owner／反馈增加最小原版代表。严格 PIC／原 WAV 与 preOS 比较、真实无设备／错误／关闭／销毁收尾均保独立身份。

阶段实现、直接影响检查、原版必要证据、复审修复及消冗齐全后才独立提交和集成。任何必需非例外声音／时刻证据未闭都不签完整 F5；F6 再接全部 A01—A22、历史回归及[最终门](development.md#f6统一全历史审查修复精简与最终验收)，不以测试数量或局部 C 成功代替。
