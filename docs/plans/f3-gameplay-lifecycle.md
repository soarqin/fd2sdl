# F3：玩法、构造、存档与生命周期

**状态：主体实现与下列有界证据已完成；提交／本地集成以`build/f3/subject-delivery-20261116/local-integration.json`实际收据为准。A04及A06—A09完整终态尚未合签。** 基线为F2 `232aca6711274fc3b6489e5e00900309b77f09b7`，本阶段一次提交，不拆构造、Load等微提交。按用户已调整的实际依赖继续F4；第4节的正常联合资格／QK及[F2联合门](f2-time-input-rng.md#主体交付与最后联合门)仍由F6最终门逐项闭合。Windows与`EX-AUDIO-NUKED`选择不变。

## 1. 本阶段真实差异与候选修复

原依据是F1完整 `domain-boundaries/source09.json`，SHA256 `18617904b37d73b066eea7a78889d6da64fc6dad895b9292ae279df4b8f47517`。本轮只读盘点重核10个相关函数chunk与严格镜像，并核22份来源身份，没有重跑旧原CPU、84工具项或82正式pair。旧F1 `cpu09.json` 的3,774构造向量仍保原条件范围，不能据此假设所有fresh堆内容。

| 对象 | 已证原合同 | 当前候选／边界 |
|---|---|---|
| 10c50模板构造 | `28h..30h`九B不写；weapon=ff时另保`0dh`；原明确零字段仍写0 | 共用构造从现有80B owner取底，append及Scene两个正式outer也保目标槽。persistent58写／22保和空部署清80原已正确，不扩大修改 |
| 10010活动Load | `1037c`读raw+7身份，11019按首次ID分配class，`10391`写raw+2，包括hidden记录 | 成功导入临时owner中同步+2，其余79B保留；共享cache helper仍只派生。手工2986f没有这个raw写点，persistent不改 |
| 2211B metadata | `10010@101c3..101cf`恢复全块，`19df7@19fe9..19ff5`写全块 | 同一metadata owner持有序列化来源与有效长度，已知字段仍由原解析结构唯一决定，导出时覆盖。Loaded尾随导入来源，不混用目标文件；没有整SAV热副本或新资源owner |
| App持久roster／New Game | main25d61仅一次分配A00；START25efa仅清count。327f7..32815的四次112a5保前四槽各22B，其余28槽全保；普通败北回标题不11506或清roster | 已修复清零底本重建的来源丢失：复用现App preparation backing，Scene四原节点构造一次，Game窄入口只消费；真实Game关闭／败北前交回完整表，不用磁盘冒充live状态，不新增热roster。旧Load03不代签新路径 |
| New Game四raw options | START不写51AAB／53AF9／51E61／51E62，32969只将资源值53BF3置0 | 已补camera／effect的原App byte投影及manual Load同步；四值原样保存，消费才转bool。新局gold=0，不随roster盲继承旧stage／gold |

后端A→Load→SaveB／新文件的metadata尾来源差异有明确writer依据；尚未把它说成固定FD2.SAV普通UI的自然反例。同路径A→A可能掩蔽。active meta[18..]、磁盘unit-area非active尾和手工slot未拥有尾仍属于目标容器，不随metadata一起改owner。

## 2. 当前实际验证

`build/f3/state-save-build01.log`完成当前C盘普通构建的App及直接目标；`state-save-ctest01.log`四项通过：`field_save`、`field_unit_base`、`scene_handoff`、`scene_transition`。包含独立原字段常量的完整80B、非零preload／重用、两武器分支、append／Scene正式consumer、cache失败／HOST，以及活动Load和完整容器／checksum／取消／错误／重复与跨stage交接。它们是C组件及合成接口回归，不是全部自然链或原版实机通过。

实际只读的33份FDFIELD metadata最大为2211B，未见更长；parser仍不因来源backing定长新增拒绝。`save-preconditions-readonly02.json`核两个原目录存档的class已是首次ID序，用户已有有效容器有11处旧class。后者的生产来源未证；只能作为未改字节的兼容输入，不能冒充原版自然Save。01盘点误把移动相位+4当身份，已在02明确撤回，原件保留；正确字段另由完整原指令1037c证实。

正式Load工具新增严格class oracle，expected仅来自来源A的+7与原11019顺序，不从measured复制；完整22,987B及checksum仍严格核，只投影明确测量导航。历史共用finite-Save helper保原范围，活动Load正式入口另强制新门。工具22项和独立入口守卫6项通过，均是Python自动检查，不计原版运行。

原生代表沿既有`validate_initial`，具体为stage0／turn1／12actor、4玩家／8敌、未行动／无状态、玩家满HP及有效镜头／测量路径，另要求存在旧class差异。该有限代表不缩小产品0..29存取范围，也不代签其它stage／人数／状态组合；这些另接源／参数／C和最终正常轨迹。

`build/f3/load-native02/report.json`已实际完成DOSBox-X与SDL正常END→Round002→取消Load→成功Load→显式Save。两端各11处raw2重排符合独立首次ID oracle，完整22,987B与checksum通过。输入为用户已有容器的只读副本，生产来源未证；本次不投递Z，不代签旧焦点轮换门。默认正式RNG，未保留调试seed；严格ON、WGC HWND、PID后台输入、原50文件和用户档SHA／mtime守卫均成立。原版PID45644、SDLPID36568均正常退出，owned job31116无超时、强杀或残留。SDL实际二进制SHA256为`c4e992a3655f8bf2e98bcf551cacc0f9aac6064bdfd1495051b9b548c3371e21`；后续仅改测试／注释时仍保这个执行身份，不冒称新的源码快照已跑GUI。

首次`load-native01`在首窗口前因旧Z预检需要未验证的scroll退出，没有运行游戏；原报告及当时三份工具SHA／mtime副本保留在`load-native01-source/`。02改为范围明确的无Z入口，不放宽class、容器或导航判据。

后续注释重建产生`66540bfd30079c348be2683cd8e1249d9d1802187d6435f730dd02d5b2039405`，已保存到C盘`qualified-f3-6654/`；该收据时尚未新跑GUI，后由03实际复验；执行时c4文件已被重建覆盖，旧运行身份由原收据／媒体／SHA保留。`load-native02-source-qualification01/receipt.json`按原SHA核回两份旧产品源，证明save.c／field_save.c仅注释变化且C tokens相同，其余产品／冷数据／运行工具字节未变；新增测试和命名证据单列。这是来源影响继承，不是PE同一或fresh原生。

独立复审发现fixture先解码后取身份、临时DOSBox配置缺late校验两竞态，已改一次bytes冻结／只读快照、两端实际copy逐byte核及reference启动前／owned清理后verify；12项工具检查通过。另补原helper两个首错／晚错负例，以及外层report写入失败保首business的一例，均定向通过；该新例首次漏decoder替身的测试失败保留。未引入第二业务错误owner。旧02事后配置／fixture复核仍相同，但不补造当时守卫。

因此只追加这一组`load-native03`实际双端复验，不重跑其它历史GUI：最终6654 App、冻结b0来源、两个实际copy及reference前后均通过；原版PID39680、SDLPID8084，各11处class改写及完整容器／checksum通过。788个source facts、8个输入／产物guard、原资源SHA／mtime和用户档保护成立；strict ON／同线程释放，无唤醒或电源／混音器修改。job43776正常退出，无timeout／residual／forced。`native03-source-archive01.json`登记C盘`qualified-f3-native03/`中实际788源及App／driver副本，逐SHA／mtime与该次运行相同；02的c4与03的6654不混同。03仍不执行Z、不签输入producer或全RNG／像素／音频。

另11项直接玩法／owner检查首轮7过4失败。三个失败是S5后仍断言21／23／29未绑定的旧负例，按原表和当前consumer改为0..29正域、内部30..32负域；另一个是Play成功Load仍期待旧raw2。只重建／复跑这4项，均通过；没有放宽生产支持门。日志为`gameplay-consumers-ctest01/02.log`，7项旧结果不伪记成02重跑。

[97参数四栏附录](f3-gameplay-predicates.md)已补此前10条缺实际C参数例及text12不同caller：23个正常stage参数、2个ERROR／HOST、8个common状态／成功组合及4个晚ERROR／HOST，共37例。`predicate-ctest05.log`的`s5_stage_source`实际通过，6.71秒。原source／旧原码与新C分账，不扩大自然producer或原码配对范围。01..04失败保留：通用64actor前态缺script60的raw83、其它stage原entry前态、物理交换缺基础profile；最终先走正式entry和必要真实callback／base派生，没有猜人数／profile或放宽生产门。

`predicate-save-ctest01.log`中的`field_save`通过0.89秒，包含三stage×追加前后×254／255的真实存取，以及256／DWORD上界H拒绝保owner、文件bytes／mtime和缺文件不创建。未执行默认255 callback，也不称自然长局已跑；手工slot无turn字段，未给它新增活动Save门。

九字节保留的C控制读取也已单独验证：Scene投影、正式story和commit_view接受同owner非零来源；真正分叉一个保留字节仍拒交接。AI11的全record资源key可因非零尾不同增加预检组；当前FULL／SHORT代表的成功动作、6次RNG、HP／MP与保留字节不变，资源拒绝仍在首失败停止。`retained-consumers-ctest01.log`的Scene两项通过，AI首轮测试漏算拒绝后停止而失败；修正期望后只复跑AI，`...02.log`通过。此证据仅签这些C控制门，不代签原全部动态别名读者。

### 2.1 App生命周期直接回归及本轮候选

新增roster／四option修复经12文件独立源码复审，未发现具体产品bug。`roster-lifecycle-build01.log`的App链接失败来自私有头仍调用旧`sync_audio_options`；第12处调用改为`sync_context_options`后，build02的App和11个测试目标构建成功。对应CTest01共12项，10项通过、Play超时及Prepare前态不一致失败。两者均为测试机制：Load不是结果提交点，Play改由一次真实controller返回提交Defeat并加8帧失败界；准备页的合成App初态须在完整快照前同步四个raw选项。未提高timeout、放宽memcmp或改产品结果门。

仅复建／复跑失败项：build03／ctest02的Play通过5.36秒；Prepare又发现stage2同类初态遗漏，保留02失败后，build04／ctest03仅该项通过1.04秒。已过的10项和Play不重跑。这是12项分别通过，**不是一个最终候选一次12全绿**。新例包含完整32→31→0 Scene、四58/22写点／unused尾、真实Game消费／SAV导出、真实Load及controller结算后GameOver前回交、失败Load无回交、raw fe／7b和gold0；GameOver为呈现替身，合成preload不冒充自然档生产。

实际App为`ddc9dce53982cbc90043b14f6852eec925ce5f03969d758e4d5be4440a6ce6e8`，普通C盘增量构建的实际CL.read包含新私有头。`load-native03`的6654归档保留，不冒称DDC9运行了03。为新App／Scene交接选择一组直接影响的普通START→117资源等待页→UI Save代表；`start-native01`在首窗口前检测console／session均OFF而停止，未申请显示需求、唤醒或更改电源／混音器，job30468无timeout／forced／residual。该次未开窗，后续执行另列，不重跑全部历史GUI。

随后用户2026-11-16明确授权自行点亮显示及必要临时电源计划。02实际由一次显示需求及必要的一次零位移唤醒确认ON／Default、指针未移动，但临时DOSBox配置缺`mixer.nosound=true`，参照预检在首窗口前停止；同线程释放，job27184无残留。03仅补本次隔离配置，实际原版PID6168先完成117页和UI Save并正常清理，SDLPID16612随后完成同链；12actor及声明的单位字段、turn／stage／roster／资源／镜头／测量导航一致。job38620正常退出，无timeout／forced／residual。03无需再次唤醒，显示需求在原线程12872释放；电源计划未改，前后活动方案及VIDEOIDLE记录逐byte相同，系统混音器未改。原50文件、用户档、788源及程序／配置守卫均成立。

`start03-source-archive01.json`登记C盘`qualified-f3-start03/`的实际788源与DDC9 App／driver，SHA及mtime均与运行记录相同。普通START不是败北返回标题后重开全链，也不签fresh raw roster／九B数值、全像素／RNG／声音／时序。原03前后manifest核配置；共用running另追加的幂等`-set mixer nosound=true`有实际command记录，完整CLI在`actual-command-reference.json`及`reference-qualification.json`事后join，各声明／派生设置逐项相同。这是来源影响证明，不补造运行前已包含该冗余CLI的manifest，不重复跑117页。

新生命周期合同同步命名输入，隔离`ida-state-save-publication03`实际发布4处注释、530登记；1312函数边界／签名／flags／chunks／strict bytes及C tokens不变，0改名，11个SHA最后刷新核验。14项发布工具检查通过0.374秒；旧静态／原型报告仅保原身份。不是新玩法／全对象BSS或原版动态通过。

## 3. fresh Save的有限来源与安全继续

### 3.1 短metadata的第一处越owner读

完整`res_load @0x111ba`会free旧块、读取相邻目录值求N，再malloc(N)并fread N。它不借用整个DAT，也不承诺沿用旧2211B容量。原`__MemAllocator @0x3d5c0`给出`Q=max(12,(N+7)&~3)`；实际payload extent A保守满足`Q-4<=A<=Q+7`，不提供payload清零。33条资源逐目录检查后，普通0..29的14／20／26恰长2211B，其余27条即使取最大的A也小于2211；内部30..32另账。

| 资源N | 普通战场／内部资源 | A的保守上界 |
|---:|---|---:|
| 157 | 内部30 | 171 |
| 937 | 0；内部31 | 951 |
| 1171 | 1／2／3／5／6／10；内部32 | 1183 |
| 1431 | 4 | 1443 |
| 1691 | 7／8／9／11／15／16／27 | 1703 |
| 1951 | 12／13／17／18／19／21／22／23／24／25／29 | 1963 |
| 2107 | 28 | 2119 |
| 2211 | 14／20／26 | 2223 |

原19df7仍将2211B从metadata B拷至独立Save owner D。完整`memmove @0x3771c`的方向须保留：若`B>=D`或`B+2211<=D`，forward的首跨界DWORD起于`B+4*floor(A/4)`，首非法byte是B+A，此前完成目标前缀`4*floor(A/4)`；若`B<D<B+2211`，backward首WORD已读B+2209..2210，短块上界之外，此次copy此前目标写入为0。两个真实分配不重叠，不能据此排除过长源区与D重叠。

N之后、A之前的slack仍在分配内，不因“未知”自动判U，也没有被取成某次heap常量。这里凭实际extent证明的是A之后越owner；尚未观测某次A／B／D或方向。该首U在wb／checksum／fwrite之前，磁盘未提交任何payload前缀。Loaded的固定2211B和三个完整fresh资源有全来源，不能沿短块结论缩回前缀。

来源报告`build/f3/metadata-fresh-owner-review01.md`，SHA256`1e424df677ea56b990f4d7bed00c4ec197d2431b82a42b0bdfd388ec78b64b0e`；新增六个完整函数902B与strict镜像相同，Source／副DB／镜像守卫不变。这是静态参数证明，不是本轮动态heap捕获。

### 3.2 新建目标的未初始化输入

另一个独立条件是目标FD2.SAV不存在，且metadata、roster、全部count条完整活动记录、cell和已知meta输入均有合法来源；仅metadata完整不够。原19fcd..19fe2只设四个slot stage为ff，其后覆盖这些业务区域，未清整个22987B。正常DF=0的checksum在4df1d顺序LODSB、4df1e ADD：count<96首欠当前owner来源的位置`K=0x12a3+80*count`，count=96为`K=0x30d5`。第K+1次读取／数值使用构成未初始化来源U；此处在合法目标块内，**不是OOB**。96人仍有10,478个checksum输入byte没有本轮writer赋值。fresh人物保留字段若先缺来源，须另核更早边界，不能用本K代签它们。

该结论核完整writer闭包、malloc不清payload及实际首语义reader，不把合法旧容器opaque回读判U。此条件的wb在checksum之前，首U前可能已创建空文件、尚未fwrite payload；不能套用短metadata“wb尚未发生”的时点。crypt晚于checksum，不能倒过来补初始化。两函数未CLD，结论明示正常DF=0合同，没有证明DF=1正常可达或重开外部EFLAGS闭包。

来源报告`build/f3/metadata-new-target-review01.md`，SHA256`ddf59aa51c354083fc34e3a981c0437a3333bbab1bb2d458fa5915768210f556`；新增两个完整函数67B，strict和来源守卫成立。无新原CPU／GUI。后续若取得独立的当前owner初始化合同，应按该合同复审，不能将物理旧bits本身冒充合法slot来源。

### 3.3 SDL策略与资格

沿用现有确定安全策略，不新增Save拒绝：短fresh metadata只输出已有资源／字段，余尾保目标；新目标整ff建立空槽及未知区。这些是到达上述原U后的H继续，**不是原整文件初始化或fresh完整2211B等值**。原磁盘的有效前缀、原已有容器保留、完整Loaded／14／20／26资源来源仍逐项严格核。`field_save_test`的新文件完整容器测试证明该H策略和已知writer，不是任意旧heap的原oracle。原资源和用户档保持只读；不模拟无限堆历史。

### 3.4 保留字节读者与有限来源族

原库存writer／count-prefix、flag40读者、字段跨度／动态learning参数，以及普通EAX输出别名均已按既有完整来源复核。最后十个输出族递推到28个owner、184个关系／91个去重实际callsite，所列frontier闭合；01因漏外部条件边`1a813→190a9`撤回，修正02保原件并完整追到正式外层覆盖。这里签的是列明producer下的非干扰／运输性质，不是九B从未读取或fresh数值已知。空EQT确有读后丢弃；其等待差异归A10／A14。

完整Load12→正常turn3 callback0两模板追加→已有目标Save给出一个具体条件来源案：10010仅把前960B拷入新active 1e00 owner；group3／7构造raw12／13，各保九B。该路径没有whole-copy／clear等当前producer，后续原脚本／方向reset不赋这些字节，也无较早数值消费；在此前前缀有效的明确条件下，首次数值使用为`D+0x168b`的checksum第5772次LODSB→ADD。这是分配内未初始化来源U，不是OOB；wb已截断目标，尚未fwrite payload。metadata和R整表均来自合法文件，旧目标gap仍有来源，不能套其它两种首U。该案本轮未新运行到turn3，旧自然14人轨迹仅保原继承范围。

有限族按`I_after=W∪(I_before∩P)`登记：whole80搬运既有来源、clear80全定义、11506仅在实际匹配的active来源有效时给目标相应来源。已有before80的71/9、70/10及58/22须严格保留；fresh没有初始化来源不能凭一次捕获猜0／ff，weaponff的0d须先接真实库存writer／使用。新旧目标只改变target gap，不能把已Load／live／合法sync判U。first-App stage0首Save先遇短metadata越owner；正常到14／20／26后的具体R记录还须接真实11506／reorder与其它保留字段的来源轨迹，**这项未验仍归A04／A09最终门**，不能把初四人mask与后期资源拼成假正常夹具。

本地可复核入口：`template-retained-readers-review02.md`、`return-use-02/report.md`（SHA256`8a9b6b2e8c043477f6b8fa69487d1b30e69f08cf882ae2e144e461419d126dc1`）、`fresh-load-append-save01/report.md`、`finite-owner-families01.md`。均在`build/f3/`，各自保存脚本／输入SHA及未执行边界；普通源审查不借QK未决性质，也不重跑3774原码向量。

## 4. 继续开发与最终收尾门

1. 普通START03和Load03各保自己的执行候选及来源／清理资格。真实败北回标题→START全链未新运行，其存档写集由原完整caller、独立mask和C交接分账；最终F6补正常后缀及当前候选join，不冒称原生通过。此缺证不影响F4解码／公共呈现或F5调度算法实现。
2. 现有剧情／结果盘点、37个新参数例及各来源继承资格已复核；自然producer／整链仍逐项与F6最终门join。真实绑定数、来源引用数和语义族数各用自己的分母，不能相除作功能率。
3. 已复核QK关联三类条件consumer仍缺表达：callback后当前掉落队列结束、collector改变AI父输入／额外已定义descriptor、victory28跨后续battle的main条件。普通mode0／1非side2早退已表达；Town0先续战斗，不立即跳标题。按[原专项](qk-behavior-followup.md)和实际正常前提在F6关闭有限实现／无影响证据，不能凭D／K阈值造触发门或把延期记通过。
4. 修复、取消／失败前缀、最后两原生代表及12文件独立复审已收口；确认前失败不提交、提交后HOST保前缀仍分开，手工slot／活动Load、Town／NoTown与跨stage资源owner不混用。坏资源／宿主分配失败是明确H边界，不以测试替身称原DOS全部I/O失败时点相同。
5. 保第3.4节已核有限读者／返回别名及来源族资格；后期first-App R的实际sync／reorder／字段来源轨迹与最终候选join仍待闭合。已证当前owner无初始化到首语义读不再索要无限heap数值；真正V且未掌握的before80仍需独立oracle，不能借其它Save U排除。
6. 按完整F3主体一次提交／本地集成并汇报，继续F4共享呈现；最终F6汇总当前候选、来源／原码／C／正式输入／原版及继承资格。原有F2／QK、后期R来源和自然联合门不得由当前参数通过替代；不能称全F3终态或全游戏已对齐。

当前完整盘点和候选源码身份保在 `build/f3/initial-state-save-inventory01.md`、`is01-implementation01.md`、`is02-is03-implementation01.md`；完整原转储、存档与媒体不入提交。后续阶段仍只验证新增及直接影响路径，F6统一历史复审与最终逐A项合签。
