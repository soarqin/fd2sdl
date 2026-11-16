# F4：共享呈现与全资源对照

2026-11-16—17。**F4共享呈现／资源主体已收口；完整A14—A16正常域仍未合签。** 本次以完整主体为提交／本地集成单元，实际身份见两树`build/f4/subject-delivery-20261117/local-integration.json`；随后连续推进F5。给定原定义before的算子、全资源及正式绑定资格，与正常producer／时钟／输入RNG／C11双端后缀分开；后者由第5节移交F6，不把原生失败改签。不是新的微阶段，也不撤回旧S4／S5范围。

本页`build/f4/`指实施工作树的本地证据；主树／工作树交付收据另给绝对入口与稳定归档索引。F盘原件、完整dump／媒体及IDA库不入提交，不能仅从文档相对路径假定主树已经复制了全部原件。

## 1. 已落实的共享修复

| 项目 | 实现及原依据 | 实际验证与边界 |
|---|---|---|
| direct／batch hidden | `field_unit.c`按原`127e0`移除direct叶的hidden筛选；pure／live batch在调用前按`127a9`筛选 | direct隐藏单位真实24格绘制、acted、整页底与坏资源批量预检；4项受影响CTest通过。不是所有自然hidden来源证明 |
| 80h移动组读钟 | `scene.c`按`1366a@13744/13748`在selected判断和direct live clock之前跳hidden | 首／中／末／全hidden及selected组合检查；保后继普通重绘的独立调用时点，不把整个流程误算为一次batch |
| 空EQT等待 | 保原`28efe→1bffe→1b9de`开页／等待／导航，确认后由唯一冷图的`inventory_empty`查询返回；Esc关闭 | 新C回归覆盖空导航、确认、Esc和HOST；原版／SDL正式输入配对及完整22987B Save均通过，详见下节。普通ITEM空门不变 |
| 死亡FX的双页反馈 | `field_defeat.c`按完整`1db65`实际序修第31项借base+0、其余至96项借App；68..73累积，74..79先恢复旧base再分别绘制；移除静默35项截断 | 旧14组显式双页及新增D36／96×两底色的四组分别通过，新144完整owner块／17,780,736字节。base构造、高D自然来源及父caller联合仍另签，不以96容量当正常可达证明 |
| 正式VGA边框／菜单页 | `field_game.c`按`11cac／17898／179d5`保留312×192投影外的4096B旧VGA；菜单先写同一App页，再画focus和投影 | 10组源几何／全页与错误前缀回归通过；菜单72×72桥保页外图标，不以VGA临时页丢弃App margin。普通、live、summary及效果wrapper同步，原生菜单代表仍待补 |
| 准备picker画像历史 | `field_prepare_service_pick.h`按`27079／27d33`只画成员／列表／图标窗口；`2b4fb`部署仍保有源整页恢复 | 21个mode×history及导航／滚页／HOST／资源错误前缀回归通过。新正式双端UI／Save通过，严格RGB仍有9幅不同；同名静帧不足以证明跨框画像反馈，续核见2.6 |
| SHORT反击工作页 | `battle_animation.c`按完整`1e611`将两对象面板／HP条先写App，再作312×192投影；复用现work作完整App桥 | 真实SP证明只有actor／target三参合同；已补遗漏的target比例条并修第二对象，30整owner例通过。两端新正常输入严格双HP条及完整Save通过，旧“第四值H”解释撤回，详见2.67／2.71 |
| AI射线mode6轨迹 | `field_game.c／tile.c／field_unit.c`按`122dc@126f1`在terrain后、actor／FG前写同map raw3；mode0继续读原marks | 实际focus／AI选择提交与整页回归通过；正常构造／活动Load及Item取消／Use按原4df4c点初始化／清理，失败保已达前缀，无第二轨迹。原生射线与完整普通nodes关系另验 |
| 正向移动背景 | `field_game.c`按`11eee／12eaa／13315`，向右／下时背景目的与后继投影源同时加入正offset | 12组有源phase参数的完整App／VGA及palette／DAC与独立派生页相同，5,231,616字节；来源及实际编译／链接快照晚守卫通过。不是自然地图或新原CPU／GUI证据 |
| 成员列表跨相位与酒店箭头 | `field_prepare_view.c`保`4e29c`隔点孔；初底回原开页caller，导航从源26px平移后的当前页继续画；`27079`按mode区分steady写集，酒店补13号资源11..14／17 | 真实五人ID0／9／4／30／51的0→1→2→1→0整页与原token独立解释一致，三处carry孔保254；16酒店参数、滚窗／steady／错误及HOST回归通过。新原版DOWN与SDL周期／DOWN的严格整页反馈已过；不代全部连续帧／时序 |

以上生产改动未增加第二热状态、RNG或通用绘图VM。资源保只读借用，等待／取消仍由既有owner与冷图调度。

## 2. 本轮实际证据

### 2.1 六类资源的完整参数批次

独立原指令执行从固定EXE／strict image及完整Domain来源取expected，不使用C解码器或生成器自证。目录清点为14711个去重`(file,start,end)`span：8256 shape、1680 icon、544 portrait、2118 FIGANI、289 ANI、1824 glyph；另144个FIGANI精确empty标记单列。

新原指令批次实际执行18637个参数状态：8256 tile、3360 normal／acted icon、1088双向portrait、2118 packed FIGANI、167 ANI帧、3648双底色glyph。完整页、临时palette、像素及palette写入mask全部保留；465个实际原head、guest I/O执行0，声明owner和来源守卫通过。7条ANI保持连续历史，不重放旧ANI0／2的122帧或旧2779帧montage。

当前C实际完成20755项配对：上述18637项，加2118项“原写点＋完整`2eb9f`位置公式”的FIGANI派生位置。四段输出逐byte相同，资源loaded-buffer在首请求前与冻结原字节精确绑定，来源及二进制前后守卫通过。后2118项不是新增资源，也不是新执行原wrapper。C mask的双底观察只适用于已审destination-independent writer，不能转用于LUT／RMW。AFM比较temporary palette，未把C的DAC投影冒称原设备I/O。

| 收据 | 实际位置／SHA256 |
|---|---|
| 原像素完整批次 | `F:/fd2sdl-evidence/f4-resource-reader-full-20261116-01/report.json`；`ce755c30f69441f96f0f56ab3985e06f7cdaf977a056a98db920ac0cdd6d9321` |
| 原payload只读复核 | 同目录`qualification01.json`；交付入口`build/f4/resource-reader-full01.pointer.json`。逐payload、mask、未写底及ANI历史复核，不新增原执行 |
| 真实C完整配对 | `F:/fd2sdl-evidence/f4-resource-reader-C-full-20261116-01/report.json`；实际`ok=true`、20755项，无skip |
| C来源／二进制快照 | `C:/Projects/fd2sdl-builds/fierce-newton-f2-20261115/qualified-f4-reader-full01/manifest.json`；`b5e39dbba344119b2638cce7a85c3f099a5e0716ba95164c84be60c40adf8db8` |
| 前置小代表 | `build/f4/reader-C-pilot01/report.json`；19项实际C通过，保其旧工具身份，不重复计作新资源 |

工具见[资源配对器](../../tools/verify_f4_resource_pixels.py)。全量期望解压约4.015GiB，工具验证后压缩驻留、逐项展开；原件仍逐byte校验。磁盘不足时以显式`--output-root`写新隔离证据目录，禁止原资源目录及已有输出。Linux保相同Python／本机构建接口，本轮仅Windows执行。

**此分母未覆盖全部FDOTHER嵌套图、LUT、raw背景、BG／TAI／TITLE、所有compositor、自然输入或最终RGB。** 旧ANI122的当前C配对与所缺mask另账，不能从18637计划中消失。

### 2.2 空装备页的正式双端代表

`F:/fd2sdl-evidence/f4-empty-equipment-native-20261116-01/`保完整输入、WGC、启动参照、隔离SAV与清理收据。原版PID41824正常结束后才启动SDL PID25024；两端分别33／30个业务检查点，均包含空页开／等、UP／DOWN、确认回成员、重开Esc、返回城镇及酒店Save。两份完整22987B容器各与独立原格式／状态模型一致，未知字节不归一。外层Job2184退出0、timeout0、residual0、forced0、cleaned1。

这是原五人受控来源的空库存夹具：以已证remove逐次删首项，保末槽stale ID，再按原recompute得到派生值；完整明文／密文差异清单均保留。它证明该合法参数下的UI行为，不证明自然到达、完整FIFO／RNG轨迹、全画面或声音／绝对时序。

本次使用用户2026-11-16授权的一次显示需求及零位移idle reset：OFF→ON后才开窗，指针未移动；同线程33020释放原需求。没有修改电源计划或系统混音器。源／原件／主项目用户档前后不变。794个实际源文件与App／driver已封存于`qualified-f4-empty01`，manifest SHA256为`9267425f5172f2198d12623c75b6e01cdbec5d01d637514d900f2718db8346f2`。

入口复用[准备参照包装器](../../tools/run_f2_prepare_check.py)的`--inventory-scenario eqt-empty`及[库存代表](../../tools/run_g3_inventory_check.py)，不另建输入／Capture框架。默认F2行为保持；只有明确显示授权才使用`--wake-display`。

### 2.3 死亡FX有限页比较

`build/f4/defeat-pages-actual01/report.json` SHA256为`8b68fdde0d013b32d0d04ed0d5e8b6a62a506911255dc173fdcc79fd85f90a8c`。先执行D35／seed0，通过后执行余13项，不重复首例；参数为`{1,30..35}×{0,1}`。每例保12帧的完整App／base／VGA、真实生成命令、快照producer及194项生成前后守卫。独立expected使用原资源、完整`1db65`控制序及已核LMI写义，不由待修C反推。

### 2.4 页／LUT／常色共享叶

新增66个正常24×24、stride456参数由五个完整原writer独立生成expected；没有重跑旧180个solid例。固定流覆盖全部256源色、全部256个SKIP旧底色、四opcode、opaque0和页内两极界，23张真实LUT分别连接base／overlay两种读义。base的SKIP读取旧destination再映LUT，不能套destination-independent双底mask定理。

当前C实际经过原page、normal／acted／solid actor consumer及真实LUT loader，66有序例、132完整before／after页、20,224,512字节全部相同；原read／write mask保在独立expected，C actual mask未观察，不冒称掩码比较。775项来源／二进制／链接库／实际编译读账在生成和验证前后均同。报告为`F:/fd2sdl-evidence/f4-page-writers-actual-20261116-01/report.json`，SHA256 `d93d75d6f868841d858da83189f31dafb11b75dd823673aedc29b6067f83a0f8`。

工具见[页writer配对器](../../tools/verify_f4_page_writers.py)。新CLI无文件写入／GUI，只由外层独占捕获stdout。该证据覆盖给定正常tile参数和shared writer，不自动签页外裁剪、其它stride、所有正式caller、clock或完整F4。

### 2.5 保留ANI122与当前C

当前C已连续消费ANI0／2的122帧，与旧原报告的每帧完整page／temporary palette SHA逐项相同，未重跑旧原码。资源buffer握手、完整ABI来源及前后守卫通过；实际C两entry首帧均观察到64000像素和768 palette地址写入。原报告没有write mask，故本轮仍只签整页／palette hash，不签原mask或字节首差、设备DAC、自然播放时长。

新结果在`F:/fd2sdl-evidence/f4-retained-ani-C-20261116-01/report.json`，SHA256 `a99fb01c34d29838b9ef34a9d768254b4ac27c1dade93a6011c7e12ad80b1850`。与新增167原帧合计覆盖9条ANI的289帧内容资格，状态／caller／时序与原mask仍按各自证据分账，不能把hash配对改写为新增原trace。

独立只读复核`build/f4/ani122-initial-state-static01.{md,json}`另确认，两entry首帧的两个command是1（完整768B palette）和6（完整frame RLE），不是清零command4。原完整handler及严格流extent均覆盖64000B；首帧后正常连续历史不依赖夹具的PAGE83／TEMP165。原`20421`帧环在0／2没有其它同步page writer。首帧全写区间可记source-derived证明，仍不将它当新执行的原write trace、DAC时点或实际byte diff。

同一完整原handler／资源范围审计随后扩到全部9条ANI：每条首帧均先command1写满768B临时palette；entry1随后command4常色填满64000B，其余8条command6顺序RLE写满。全289帧header链恰至各entry末，最大输入均不越原64000B buffer。静态报告`build/f4/ani9-initial-state-static01.json` SHA256 `7c616044c9ad3f80a75223d41ec5c9f382a1de49571ec2384e18a9c42fe31555`，不生成像素／执行C或原CPU；这关闭9条ANI首帧后page／临时palette的seed依赖，仍不代原DAC、entry1音效写序或自然播放时刻。

### 2.6 正式截图的严格RGB后核

对空EQT已保存的30幅共同WGC截图新增只读整幅320×200 RGB比较：20幅逐byte相同，10幅有差异，未裁ROI、放容差或丢弃差异。结果保在该原生目录的`rgb-postreview01/report.json`，不改旧UI／Save收据。

旧候选在多幅成员页存在固定`(175,90)/(176,90)`两像素差。独立源／资源复核确认DATO128 phase0与未blink底图恰在这两点不同，旧picker每次恢复整页会擦除`16559`写入的画像历史，已作共享修复。源`26eda`入口没有强画phase0，初始未blink底图本身合法；不能为得到同图额外画phase0。

新候选`F:/fd2sdl-evidence/f4-empty-equipment-native-20261116-02/`重新执行原版后SDL：空页UI／完整22987B Save、原件与源守卫及正常清理通过，程序／800源文件快照见`qualified-f4-empty02/manifest.json`，SHA256 `bf0bebd6df529b70511853b882b4fd5e6e7003cac6b40b7480d1d47bea7180df`。同名30幅整页RGB为21幅相同、9幅不同；不是30幅全绿。

只读精确资源归因已把较大差异定位到DATO128／51或FDICON0的不同合法phase，但新members静帧仍含上述两点差：原版已为phase0，SDL仍是合法初始底图。两端checkpoint耗时不同，尚不能从静帧断言SDL再次擦除或反馈已合格。后续须在正常成员页观察各端自身的初始底→blink→phase0，再导航，核后继写集保留；保完整画面与独立phase状态，不用口部ROI或容差签全页。旧／新候选的UI及Save通过均保原范围，严格跨框全页门仍未过。

### 2.7 额外资源与保留montage的当前C

333个额外唯一span按codec与正式reader另账，不计重复引用。75个source-derived byte合同实际C通过：23 LUT、7 raw6 palette、2 wave源及43 raw UI；raw UI依据完整`4ed0b`的opaque逐行copy，不能代签其它transparent0 caller。报告为`F:/fd2sdl-evidence/f4-extra-resource-C-20261116-02/report.json`，SHA256 `bdddba64550f155f5cc613b40750ebd39f8f9f0ebc34caa7ad446c9f6f8194b9`。

另256个独立原像素／当前C配对全部通过：BG56、TAI56、TITLE7、cursor20、UI93、background9、intro8、nested7。完整packed page、palette与写mask逐byte一致，原writer PC／owner与无guest I/O守卫通过。报告为`F:/fd2sdl-evidence/f4-extra-images256-20261116-02/report.json`，SHA256 `d4382319728dd6ea685b99e3e2950cf486be68c9cc5d9f6c4fe7e2cf8e4a316d`。其中52个LMI只签`4ebff＋4ec66`opaque decoder；正式`4ebab`的transparent0合成另验。333中仍有2个未知codec；331个reader状态不等于全部FDOTHER资源／正式binding闭合。BG／TAI producer及TITLE引用身份均不从文件名推断。

当前C还重放了保留的40组montage：3596个原PC命令、2779幅完整64000B帧及HP／RNG／状态全部一致，没有重新执行旧原CPU或GUI。新报告为`F:/fd2sdl-evidence/f4-montage-currentC-20261116-01/report.json`，SHA256 `ecbc62ca5f9a7111360bc6633f59167fc7990fdfc02123d901e2215423b1566e`。独立复核直接比原／新行与frame bytes，不导入重放器作expected；不能代签完整冷walker、正常全App、音频或绝对时刻。

额外reader的首轮Python打包缺失换行导致CLI在请求前退出，以及256批次首轮解释器缺少Unicorn，均保失败原件，修订身份另列；没有把未派发的原码／C请求计入成功批次。

### 2.8 Round的BYTE参数与阵营全phase

新增Round实际C覆盖256个BYTE参数各15幅完整VGA和非像素状态，另保999／1000／UINT32_MAX三组API边界；后者不算正常游戏回合。原`10010／1a30b`立即数、call目标及冷顺序由既有独立原字节校验器核对；源`187d6`按三位十进制格式、每字6像素拼接，真实数字／banner reader资格与上节分账。`round-byte-ctest01.log`通过，0.22s；不把原caller内其它玩法／clock替身当真实全App。

阵营当前C另执行两个kind的完整46幅VGA／DAC／App写集与每个等待边界退出回归，`phase-composition-ctest01.log`通过，0.48s。原`1f1cc／1f30a／1f42d／4eb59`的slide、WORD mosaic与调色参数由原字节→冷图独立比对；共享叶的全phase资格不代替真实背景／actor hook、自然输入或设备绝对时刻。

本轮独立原字节→冷图实际收据为`build/f4/round-phase-source01.json`，SHA256 `91e4f0ce3d91e8f11974e4e398c08bd0e58a9133dc47e8ee74eb5e4a309fae14`：Round42节点／phase132节点一致；没有新增原CPU或GUI。命名／源函数身份和完整writer／consumer组合继续分账。

### 2.9 全文本语法分母

复用既有[原资源审计器](../../tools/audit_g4_render_source.py)增加first-END语法账，未新增文字解码器或运行时状态。固定FDTXT34 entry／1016 fragment含51155个字面glyph token，均在FDOTHER4的1824字模内；speaker控制另消费1450个参数，不把这些参数计作字形。实际控制次数为END1016、换行3620、PAGE435、动态名`-4／-5`分别18／1、动态数字`-6`17、四种speaker813／364／231／42，未知控制0。30个动态片段与原名称plain合同仍另保。

报告`build/f4/text-source-syntax01.json`的SHA256为`0e9493a89e30c7b5973488a16381ef9d139b22d6c098512a477ea32b65ffcd2c`。6项pure回归通过；首轮从错误工作目录运行的import失败保留，正确入口在`tools`目录运行`python -B -m unittest discover -s tests -p test_g4_render_source.py`，Linux使用`python3`，本轮仅Windows执行。此账只核首END前语法与参数身份；END后原字节、所有正常caller、动态值及完整呈现不由语法通过扩签。

### 2.10 SHORT反击的App与投影前缀

完整旧G2 Domain读集`runtime-continuation-domain.json`中，原`1e611@1e672／1e6bc／1e6f0／1e710`先在53A49页上画两面板／已知条，`1e734→11d2c`才投影312×192。此前C仅在board画，App缺失面板，且清零board后全64000B复制到VGA。新增独立App期望在`short-counter-page-before-ctest01.log`实际复现失败；修复后中心／跨视口及HP参数扩域的`short-counter-page-ctest04.log`通过，1.54s。01—03各候选结果保留，不拼成同一次执行。

覆盖中心及相邻actor面板跨投影视口但不越App的参数，完整153216B页、64000B VGA与非像素状态逐byte检查；后者是有源页参数，不称已观察自然caller停留于该camera。HP21／42／63与max42分别得到条长35／70／104，包含HP>max的有界参数，不据此宣称正常玩法会产生该值。条图跨82px面板和逻辑行仍按原线性owner写；实际owner末尾的首错前缀及两侧保护字节另测。错误仍保Game刷新首前缀，不执行后续present；没有新增RNG、clock或第二page，既有work仅扩容到实际App尺寸。真实资源的透明0与原列图写义独立继承，本项不代签反击前后所有自然动画。

原`arg_C`额外HP条来源仍未闭合，当前H省读取保留。此次只修已确定的App owner及外框差异，不改变该未知值、伪造actor或把新回归当第四参数资格。下一步正常SHORT代表须按当前候选核本项可观察反馈，未知内部性质仍按残余门处置。

### 2.11 mode6地图轨迹及加载／取消边界

原`15055`的正常item79射线在`151dd`设mode6，focus每格的`11cac`经`122dc@126f1`写当前map raw3=0；同帧base已画、后继FG见新mark。`1528f`回mode0后仍按该轨迹画八次，`152c6`才清。此前C仅在玩家ITEM_TARGETING逻辑提前改nodes，AI BROWSE无marks，mode0又不读取。现保同map owner，在实际selection点补写；terrain及FG按原raw3消费。

初始化及清理严格接原边界：`1088d@1097a`资源加载后、`10010@10276`Loaded shadow内，以及`1bbdc`取消／确认／Use边界的`4df4c`，复用每cell raw3=ff、byte2&1f、WORD&03ff。33个原map的43734 cells、最大50×64均与原AL×AH count相容；没有把reset塞进bind或每帧。坏Load不清live旧mark，HOST／错误不补业务尾或回滚已达前缀。

`mode6-ctest01.log`中field_save／field_item_target通过；field_unit_render旧stage32资源移植夹具绕过4df4c、raw3=0被正确识别为轨迹，保留失败。窄修夹具只补原stage32→205da→1088d初始化前提，全部13个资源expected不变，`mode6-ctest02.log`通过，2.84s。初次Save测试错用不存在的结构字段导致C2039也保留，按现SAV meta常量修正后才执行。

直接共享6项`field_focus／field_ai_commit／field_play_result／field_input_wait／scene_handoff／battle_animation`在`mode6-shared-ctest01.log`通过，16.61s。原／C不是同一执行层：新例真实AIchoose→commit、原2dfc8演出明确stub；固定phase整App／VGA源几何和RMW期望不反证本次clock。完整合同及11函数strict source见`build/f4/compositor-qualification01/mode6-contract01.md`；普通nodes与原raw预算关系、动态背景初始work及正式原生反馈仍是独立门。

### 2.12 画像连续代表的首轮失败

新窄入口`--picker-retention-only`仅复用正常LOAD→商店→成员，不重跑EQT detail／酒店Save。`F:/fd2sdl-evidence/f4-picker-retention-native-20261116-01/`实际只运行原版PID45608：完整RGB初始B／blink P3／恢复P0均已观测，但`picker-retained-after-eligible`等待35s超时；SDL未启动，DOWN未发送。WGC同HWND保237幅原PNG，约40016ms，正常显式stop，Job17600退出1、timeout0、residual0、forced0、cleaned1。原资源、所有源及完整SAV／mtime不变，显示需求释放。

启动后独立复审发现静态成员家族遗漏`4e29c`隔点writer对旧相位像素的保留，故本轮即便局部匹配也不签完整反馈。原件和超时不改签，后续先用237幅原序列与完整原source重建历史模型，不延长预算、放容差或补键求绿。来源／候选PE封存于`qualified-f4-picker-retention01/manifest.json`，SHA256 `8f0088cd9c3708f47d7781a755f44ed7ff0da019be044773954ac62c89029669`；候选SDL PE虽守卫，未被本轮执行。

只读Domain随后从独立PUB03副本补完整`4e29c`116B／61指令、CFG与xref，全部byte与strict image／发布metadata相同，未保存DB、未执行原CPU或GUI。新来源`F:/fd2sdl-evidence/f4-masked-member-source-20261116-02/source01.json`的SHA256为`b1280a24dc9cb8a2a15b2b3ee36ffef26e4ca9dea1cc0ecf8026607a375a3292`。01仅为准备时expected DB SHA拼写错误，断言在复制／IDA派发前停止，另保失败记录。

### 2.13 剩余资源的实际配对

新增清点1256个唯一物理span，分属command raw78、map6共230、arrival9共12、direct9、准备混合142、FDOTHER内FIGANI701及challenge shape84。与此前14711及333实际平铺合并无交叉重复，合计16300；这是资源extent分母，不是功能覆盖率。

| 文件 | 唯一span |
|---|---:|
| FDSHAP.DAT | 8256 |
| FDICON.B24 | 1680 |
| DATO.DAT | 544 |
| FIGANI.DAT | 2118 |
| ANI.DAT | 289 |
| FDOTHER.DAT | 3294 |
| BG.DAT／TAI.DAT | 各56 |
| TITLE.DAT | 7 |
| 合计 | 16300 |

完整报告`F:/fd2sdl-evidence/f4-remainder-bindings-20261116-03/report.md`（SHA256 `bed7fd820feab65d219a4829043da71e4677b6940010f035f067902a85536fe6`）及同目录`flat-spans.jsonl`（`4bb45be7a6812a9d39ae5dcf722533ab3ceccbb2b1dd29b524cdf9d49def5afd`）保全部catalog binding标签；4254项原／C packed、来源与依赖的末守卫相同。FDOTHER103顶层entry已分角色，其中26个音频bank归F5，不以类似嵌套目录混入本分母。144个精确empty FIGANI另列，不伪造图像part。

原版参数输出2093组实际对应1151个span：328组严格继承首轮已成功部分，1765组为新执行。首轮在map6/125的静态LMI extent门停止，未派发该项；错误及原件保留，未将疑似raw资源判成未定义行为。剩余105个span为86个正常固定入口绕过的challenge条目、13个hotel条目、5个church条目和map6/125；各自的正式reader／绑定资格不从相邻布局猜定。

正式C的Battle／Prepare两实际reader共2093组逐byte通过，加载buffer与原资源精确绑定；另对部署背景FDOTHER5/frame137补完整原指令输出和实际Prepare C的1组配对。frame137属于先前333分母，不再次计数。它已由现有`service_frame(5,137,0)`消费，不是新增缺失功能。原报告分别为`F:/fd2sdl-evidence/f4-remainder-original-20261116-02/report.json`（SHA256 `6c95a250ec75333a35930c026e37160c4cd6c21632d4a7673aace2cf17fddb30`）及`f4-ui137-original-20261116-01/report.json`（`c71922a5729bb7de87aa781ac4d7a2e4d166505320a7b3999fc44e7b6e2631cf`）。

C报告为`F:/fd2sdl-evidence/f4-remainder-C-20261116-01/report.json`（`58379e3d5bccf483ce2edef28f27abccb7605b3169f32ec7e97479425582f4f5`）及`f4-ui137-C-20261116-01/report.json`（`afcf8f35cd008b603b122e5a6a177129bb487ddae3a668c63d16098f8f386789`）。实际两目标构建、600个编译／链接输入和22个库的来源快照在`qualified-f4-remainder01/receipt.json`，SHA256 `52911b58a13506e2af28cc0df3009c9759e0deb512e8315610a20726001600ec`。本次纯工具15项及两个默认CTest通过；没有GUI。后续改动须按冻结来源影响继承，不把这些候选结果改写为新候选重跑。

[原像素生成器](../../tools/generate_f4_resource_pixels.py)现在可从固定EXE、完整chunk／head身份和只读资源重建有限参数expected；默认只生成计划，执行须显式选择。原指令执行共用[有限执行器](../../tools/f4_original_pixels.py)，不复制原完整镜像或媒体入提交。新生成器与历史收据schema分开，未为工具迁移重跑旧18637／256批次。

UI58的正常参数族已由53A81全部77条xref、26个完整owner及真实传参闭包收口；raw／bar／number／queue／phase等并集为`[0..137]−{58}`。新补完整2860a只返回31／42／119；1ceed只取raw92和数字bank42。58是正常族未引用的raw-sized存储，不强选codec、不判U，也不新增功能。相反，map6/125保一条确切escape：ownerless21c01在既有21bd0窗中从参数形成目录index；这接A12未绑定入口／provider，不被常规229项LMI正常producer闭包排除。原source与C能力对照、完整Scene owner和冷walker仍分账。

hotel13个未消费槽为`[1,2,15,18..27]`，church5个为`[18..22]`。hotel只有其正常owner／menu／mode族，不走商品preview；church不进shop preview。未引用结论以这些正常caller合同为限，新增绑定会重新触发资格检查，不是永久豁免。hotel11..14／17五箭头是正常绑定，另由共享接线修复，不在这13个未消费槽内。

### 2.14 动态背景与正向跟随

原`11eee`目的为包含`53AF5`的同一App位置；正常右／下producer按phase加入4px偏移。旧C只加负offset，正向背景因此偏离后继投影源。两处分支现使用完整offset，没有新增clock、RNG、HOST或失败点。

实际`bg-follow-ctest01.log`通过（2.26s）。12组stage17、camera(3,7)、右／下phase1..6的整App、VGA、palette及DAC共5,231,616B，与既有原16／17完整像素和完整caller公式派生的expected一致；Game非像素、map、资源及RNG不变。全SKIP terrain／actor是用于隔离几何的合法图形输入，不冒称自然地图。报告`F:/fd2sdl-evidence/f4-bg-positive-follow-actual-20261116-01/verification.json`的SHA256为`4658f7e3b0087fb21eaa92e327d227ab6e8382ca532834638e0fd2d2a8d546ec`。实际编译／链接快照`qualified-f4-bg-follow01/receipt.json`（`1763640002edacef0de38fddb823f3d8798e346320df5ea5aa28e1bc78bf9383`）及`build/f4/bg-follow-build-join01.json`把当前C终态与来源连接，2130项晚守卫相同；没有新增原CPU或GUI，也没有观测C物理write mask。

四族完整来源合同在`build/f4/compositor-qualification01/dynamic-background-contract01.md`。none为23个stage，parallax为17／21／22／27，scroll为23，wave为9／24／25／28／29，共33绑定。parallax正常四向camera／pan的全部crop不读47半图x461的122个SKIP字节；scroll源42全写且构造立即rotate，正常speed1..14的三次反转与原尾段搬移终态等值；wave成功更新后全部59904B可读work定义。源公式派生与实际C批量参数仍分账。

wave首次rebuild后恰好同tick的work来源仍未知：原malloc新work、保last／phase，只有tick不等才填充；C calloc的已知零底不能作为原expected。正常Load和新stage的fadein均在首读之后，不能借其时长保证首读不等。此项是尚缺合法before／首门关系，不是已证自然反例或自动U；不强制更新、复制旧work或用stage0值补齐。完整合同已将未知收窄到该关系，其它已更新状态照常推进。

### 2.15 成员当前页与酒店资源接线

完整`4e29c`及原237幅WGC的历史复核确认，成员sprite的stride2孔须保前一相位，当前底色不能每帧重新从roster_page取得。五个既有view API（PRODUCT／ITEM共一个）现只写当前页widget；初底回到原`27738／27d33／27f4a／2a29d／2a857`开页点，不新增“已画”热标记。DEPLOYMENT保`2b4fb`明确的整页重建。

steady严格按`27079`分派：MENU仅选中icon，初开四icon仍另画；PRODUCT／ITEM仅箭头；三行candidate先箭头再sprite，不重写姓名／价格／preview；MEMBER逐raw先sprite后name，再箭头。酒店名单复用成员叶并加载、释放FDOTHER13的五张8×9 LMI箭头；这五个正常绑定不再只剩reader能力。

导航滚页从当前页的`(10,115)284×80`窗口作`6+6+6+8`搬移和73填行，合成26px后再画widget；保三次10ms等待和HOST首步前缀。初次候选及后续失败注入修订分别保`build/f4/member-current-hotel-candidate01/`（patch SHA256 `8cf45dcc273009424d388ca7bf1e3c88e81de7a7af8e5d44c85c984d98b90e0b`）与`member-current-hotel-fixture02/`（`aaf08f8a1f4760a2d76f509b3b49a14b5bf4c5909c78bd25b8bc166fa0c9e263`）。根侧另仅清理生产文件的空行／缩进，不冒称候选patch即最终物理字节。

`member-carry-ctest01.log`中View通过0.06s，Play的旧分配失败注入未触发：它只拦私有`new_frame`的calloc，已删除的wrapper整页分配不再存在。保首轮失败，改由现有成员叶替身在frame38重开首调用返回错误，原frame38、clear@24、SAV／owner断言不变；`member-carry-ctest02.log`实际Play通过1.23s。View五步相位历史、ID30三个已知孔254、candidate仅sprite和坏参全页不写均已实际检查；Play的全页滚窗方程、16个酒店箭头参数、opaque0／上成功下失败、初开早键RNG及HOST／空人数前缀均通过。该纯C层没有新原CPU或GUI，原237帧190全页＋16写前缀／27未闭及旧超时身份仍保留；正式新导航／SDL周期代表在修订模型具备资格后执行。

### 2.16 修复后的正式画像／导航反馈

新源历史模型在`picker-history-native-candidate02/`独立保11族整RGB身份：B允许fresh／stable，首次blink最早第9个eligible轮，P3／P0只允许已历完整sprite周期的stable族。模型的selected0全部11张整页SHA集合与旧源历史模型相同，可保旧190幅全页匹配资格；未重跑237幅或把16前缀／27未闭改签。新工具patch为`92be8111f38d7d935c927cc350aadc6f3c30a5a13e7f78d41fc5bd04139988e0`，根侧10项纯检查实际通过0.075s。

`F:/fd2sdl-evidence/f4-picker-retention-native-20261116-02/`实际先原版PID22156，退出清理后再SDL PID45224。原版新增3个完整页点：P0导航前态（sprite1）→唯一DOWN后的selected2 P0（sprite1）→后继eligible（sprite2）；SDL新增6点：B／P3（sprite2）→P0（sprite1）→后继eligible（sprite0）→DOWN selected2（sprite0）→后继eligible（sprite1）。9个点各64000像素、全320×200 RGB、零容差与独立源token／资源历史模型精确相同；两个窗口的ENTER／DOWN均绑定同次WGC HWND及owned PID，没有额外反馈键。两端完整SAV及mtime均不变，本轮不执行Save。

WGC分别保32／157幅、约5343／5625ms的连续采集；明确只签上述完整页检查点和所见有序相位，不将全部采集帧或物理刷新计为全等。外层Job31844退出0、timeout0、residual0、forced0、cleaned1，两端进程均正常退出。显示OFF按用户授权一次申请及零位移idle reset后确认ON，同线程32276释放；电源计划和系统混音器均未改。启动时原DOSBox的640×480／720×400过渡几何照实保留，进入有效游戏viewport后才参与320×200模型比较，未裁桌面或以其它截图方式回退。

配对报告`paired/report.json`的SHA256为`18d08db1067606d464b828ea64e61317e0c5416db4f00ed6cdf6c1caf57ec761`，原资源、808个源文件、App／driver及主项目用户档的前后守卫通过。源码及实际程序快照在`qualified-f4-picker-retention02/manifest.json`，SHA256 `fc6004baa4873467b7cf33d09f043f3230695b1d21f3540f07d58b88bc1e2e06`；实际编译／链接闭包另在`qualified-f4-member-current02/receipt.json`（`f84a960d3dc4927f6a2bca8a7effeb370d0a8ded7373453e670d346a08fce693`），576个实际输入、22库。此处App SHA256为`8222308381acd65574ace03fe5ce3690d5d3729961a08dcddfb4b94d602cd34e`。旧native01仍是原版only、后继phase门超时／SDL未启动；新导航资格不回写旧失败，也不代自然到达、完整输入／RNG／绝对时间或声音。

### 2.17 四类背景的实际C批量

[背景配对器](../../tools/verify_f4_background_pixels.py)沿既有独立原RLE半图、raw15／55和完整源公式派生的343份expected，实际运行同一App background owner及独立目标页、正式background API和viewport projector。23个none、四canvas各四正常camera角、32个唯一更新wave相位各update／同tick加三别名各两次、192个scroll累计状态、速度1..14各setter不转／更新／同tick，共343组。资源首行回传实际加载的完整FDOTHER字节；完整App／VGA、palette／DAC及实际meta、资源／owner保持性质均匹配。

新报告`F:/fd2sdl-evidence/f4-background-family-actual-20261116-01/report.json`的SHA256为`1b6156f2face5f3fad12b723b910255820aaac1d7de7e9e4c993b95fd343824b`，`passed=true`、实际C343、371项输入晚守卫相同，owned console PID36576已结束，无secondary错误。新原CPU／GUI均0；fixed-NS请求不代live时钟fresh-read，C物理write mask未观测。首次wave请求明确为不同tick，padding只签前后不变，未拿calloc零底签原新work来源。正常Game调用层仍接2.14的12组整页、完整原caller顺序及已有live clock资格，不把此probe扩成全部自然轨迹。

实际PE及编译／链接来源在`qualified-f4-background-family02/receipt.json`（SHA256 `bbb585da76b99c31b9fcbc36347e93bf5b5d55a9901dfcd6ecbd338bc511c8ac`）；PE为`b987a06f8991114c5478188f56d65129b4311bcf589982b6c5099874fc1d0977`，559个实际输入、22库，`build/f4/bg-family-build-join01.json`保2126项完整末守卫。工具接受显式原source／image／canonical／dispatch／payload路径，hash-only fixture不含原码或媒体，默认不重跑原叶；独立expected首身份仍为旧01脚本的`b5e99263a5952a2066f66df453f4d84dc67a45193ad46496e2002618bdcba4cb`，未改签为新工具产生。

前置失败分账保留：新test文件最初无`__main__`门导致unittest discovery消费外层参数，尚未运行15项；补门后`bg-family-tools-tests02.log`实际15项通过0.031s。首次pair又在启动C前因Git默认CRLF与清单登记LF不符停止，输出目录也未创建；`bg-contract-eol01.json`证明JSON等值并恢复固定LF。`.gitattributes`固定两个hash-only fixture各自已登记换行，旧kernel清单的CRLF／`f13de1…`身份不改；没有改expected、放宽SHA或重跑原码。01／02生产快照和旧日志均保原件，最后仅02快照签本次actual。

### 2.18 调色参数与固定参照的RGB扩展

原ANI的全部289帧另做只读命令／extent盘点：实际opcode计数为1共9、4共1、6共63、7共155、8共162、9共176；2／5不在这批资源中。九次palette命令共6912分量，全部0..63；所有显式pixel store末端≤63999。报告`build/f4/ani-all-command-parameters01.json`的SHA256为`1a2067c159d74e83f4e3d631e1a29cc2a18dde995b935937883fea4d54137b17`。它复核完整既有Domain chunk与strict image，不生成新像素或执行原CPU／C／GUI；可排除本资源族因高于6bit分量或源内显式越屏store触发当前裁切／RGB转换差异，不能代签DAC时点。

固定DOSBox-X commit `b6abbd5980a885f5f310a4088c59a8688d1b116c`的[VGA DAC实现](https://github.com/joncampbell123/dosbox-x/blob/b6abbd5980a885f5f310a4088c59a8688d1b116c/src/hardware/vga_dac.cpp)已通过Cindy GitHub精确读取，base64解码后的Git blob为`509d42fbead67a56594e49cec55549bc6fcc6d7c`，SHA256 `b439808b4974bb9ceca12b8de39c3f44cd0bef7269f823aff32765ce71ed8a2a`。其6-bit分支先取低6位，再按`(v<<2)|(v>>4)`投到8-bit RGB；现C的正常0..63组件域同式。已有同commit `int10_modes.source`明确modeset清8-bit DAC标志。此公式是固定参照的宿主RGB展开，已修正`vga.c`旧注释将其归为原FD2反编译的说法；不把公式推到外来8-bit RAMDAC或宿主色彩管理。

水色原object3 `0x60003`的93B实际读集已再核：48B周期及其前45B重复，与现16×3 RGB6常量逐byte一致，旋转仅写DAC224..239，不改base palette。64种6-bit组件的RGB展开值及来源／当前代码守卫在`build/f4/palette-render-contract01.json`（SHA256 `a7cc291f6b334c300f3cd80fc8bfd685d55715898a35b34d44601a9733438ccc`）。原S4减法fade及F2 water fresh-anchor实测保各自身份；本节没有重跑或扩签它们，全部palette／fade／water调用交错与绝对采样仍按A13／C05联合门登记。

### 2.19 左边界背景与效果等待

原`1300d`在cameraX=0向左移动的首相位仍取背景mapX=-1、pan=6，背景源x为0。旧C只将terrain坐标裁到0并沿用背景x=3，两源窗口有16627个像素不同。现将背景坐标与terrain边界读取分开。六个正常页参数的当前C与既有原payload派生期望全部一致，完整App／VGA及非像素块共2,615,808字节；报告`F:/fd2sdl-evidence/f4-left-zero-actual-20261116-01/verification.json`，SHA256 `7a616ccd1ecb58feefbdfa374f692d3e7e367d1a0db864dedd7bf39eeaa7f629`。这批地图body均为合法SKIP，不代签左侧WH header地形／LUT读取；后续418组另见2.23，两个候选不混成一次执行。

`field_effect`已按原`1c2da／21548／24618`改为copy／App hook／upload后分别等待tick1、10ms、5ms。flash五轮交替共十次tick等待，末次原页恢复不等待；不借累计宿主帧预算或末尾额外deadline。三族完整顺序、HOST及错误前缀的定向CTest通过，`left-effect-build01.log`及`left-effect-ctest01.log`保实际结果。六页参数与实际App／Unit／Effect／Scene producer封存在`qualified-f4-left-effect01/receipt.json`，SHA256 `09d997a192a3e79364987f3f341769c7156db413090b4f97df0de1c8280ffc11`，2172项晚守卫均同；不把旧12／343批次改称新候选重跑。新正式flash的反馈与显示时刻仍待原生代表。

### 2.20 动态文字与普通对话快照

`build/f4/dynamic-text-contract01/report.md`（SHA256 `8c9f79ba78bc53ecf28af254e64abffd4722729721e4982faa4dc5bfc9b12526`）连接30个实际动态片段binding、13个完整原函数与7张原表。416个已知plain名称、四槽数字1..4及正常价格／成长等有限参数分族登记，未用一个粗DWORD界代替各producer。现有名称展平仅在已核正式parent slow1、非消费peek及plain子串条件下等价；slot549为slow0但不含嵌套名称。其它通用参数不由此扩签。

负`-6`的首原font读取已由完整指令而非伪C参数推定：`15f84`将ASCII减30h后的完整DWORD传给`4ed7a`；负号形成`fffffffd`，左移5位后首`LODSW@4ee12`读font B之前96字节。当前font请求58368B，allocator仅在B前4B有本块tag，故该条件确实越当前font owner。bg非零时此前16×16清底仍是有效写前缀。SDL安全负号是此后有限H策略，不能称原像素一致；负值的自然producer与更早边界仍分账，不模拟邻接heap，也不提前排除正常正数。

四个普通snapshot函数`4ec7c／4eca4／4ecbf／4ecf0`的完整143B／55条指令及`165ac／16b43`caller已连接：8B header与310×86连续快照、320 stride恢复和五级生命周期可映射现有五份像素缓冲，不需第二owner。301个speaker片段中300个完整落模态窗；entry23／fragment12另有13个合法窗外像素，原／C只恢复310宽窗口而保留它们，不能用整VGA恢复抹除。credits的128000B工作页有400行，超可见200行不等于越owner。以上是静态／已有叶证据组合，尚不签整个对白、credits或所有长期message写序。

### 2.21 共用tick与对话毫秒等待

实际14处`present_timed`调用按正式／诊断／宿主节流分类，记录在`F:/fd2sdl-evidence/f4-common-tick-candidate-20261116-01/present-timed-inventory01.md`。已修共用事件`17aa9`：改用共享PIT投影，保start、至少一次signed低WORD比较及fresh尾读；回边只泵宿主事件，不再呈现、消费普通FIFO或推进水色／actor／RNG。goal≤0仍有源三读，>65535仍保原非终止域的显式宿主拒绝。54.93ms首tick、signed边界／回绕、FIFO与HOST前缀及真实common consumer回归通过。

普通对话marker／开框／关框改为upload后独立10ms，五级框只有四次步间等待，末级不补等待。新增回归把每次上传成本设为7ms，核四次开／四次关各计完整10ms、整VGA恢复、已有deadline及FIFO不动和第二次上传时HOST前缀。`dialog-common-wait-build01.log`与`dialog-common-wait-ctest01.log`记录当前Windows实际构建及3项定向CTest通过（11.85s）；没有新原CPU／GUI。宿主空等16ms保其独立节流身份；loot、fixed tick及准备页混合helper随后按2.22完成修订，不以这三项通过签全部等待。

### 2.22 有限拷贝、源等待与线性字形

准备页原`1974c／26ce4／2921a`等有限拷贝不含逐相位16ms等待。酒店底框／菜单／槽导航及服务缩放、暗度／清屏现只上传；4／10／200ms和服务BIOS tick保各自源后等待。酒店416文字后改为一次200ms，不再反复重呈现。输入空等保宿主节流，并在读键、HOST或错误时仅释放自身期限；普通／服务choice共用同一有限叶。独立静态复审在`build/f4/prepare-wait-source-review01/report.md`，SHA256 `f402ca59ae88bb889c304bb7c12f1541a21a996dba48fe64e80094e67c048906`。其中26996完整机器来源本轮只继承现发布119B结论，不冒称重新取得。`prepare-wait-ctest03.log`实际通过1.21s，覆盖独立7ms上传代价、完整10ms后等待、外来deadline保留但不履行、FIFO及HOST；旧失败收据保留。

普通／动态Scene及reward／loot／common notice的共享字形点改为原线性owner地址义，保既有XY宿主裁切接口并共用一份字模循环。不再把x319后的合法同行缓冲地址裁掉；真正越源／目的owner时停在首错，保清底、前行及前景写前缀。原负数字号仍为首U后的H。新增独立四bit点列、bg0／74、glyph10、320／456 stride、零色及源WORD／目的BYTE边界回归；实际font、progress及S5 source通过。旧Scene439期望也曾错误使用XY裁切，现按原资源bit编号和线性仿射地址独立组成整64000B期望，不调用待测linear生成expected；修订后Scene通过11.01s。

`loot_fixed_wait`已直接执行原毫秒等待，不再借帧预算分slice或重呈现；`scene_fixed_ticks`保原start／至少一次比较／fresh尾读且不绘图。公共`fd2_wait_ticks`也补17ae4尾读，HOST在此前返回。`public-tick-tail-ctest01.log`实际BIOS wait通过0.03s：goal0精确三读、独立跨tick比较后fresh尾、宿主退出不续读及原FIFO保护。该修改发生于下节实机记录之后，不将实机旧PE身份改签；它不改变flash的像素／cue写点或快照，完整clock轨迹仍归A13联合门。

本批`left-glyph-fixed-ctest01.log`中S2视觉、Unit及Scene通过，旧GAME OVER测试按“首tick会制造present帧”注入HOST而失败。改为首1ms宿主泵注入，保“clear0尚未发生”的原断言；`fixed-tick-host-ctest02.log`实际scene_transition通过0.80s。构建遗漏旧present调用、旧有限copy应等150ms的期望及新夹具queue_pair自增2ms的基准错误分别保失败，未靠放宽原义或像素比较求绿。

### 2.23 左地图margin的完整参数组合

左移cameraX=0的原`1300d／11eee`无条件取14列。page block现有限支持x=-1：线性index=-1读唯一地图4B WH头，后继行读上一行末cell；普通二维path／query不变。header raw3非ff也必须取得原LUT，即使map body全ff。C0 SKIP映射`LUT[旧目的]`，stride2孔保留，opaque0实写；完整block预检及坏资源宿主错误保持现有原子门。

33张地图合38个实际base／FG相位族，range20序列仅11张唯一LUT，展开418组，未把别名作独立功能。独立expected复用1004份已保存原tile及9份原BG产物，并用原raw opcode区分RMW与无写孔；未重跑8256、343、正向12或左BG6。当前C418组完整App／VGA／palette／DAC全部一致，报告`F:/fd2sdl-evidence/f4-left-margin-actual-20261116-01/report.json`，SHA256 `1201edba4d978b0a36c73a52ff8d6c702d9daa90362b989e011454a9768c59b3`，owned console PID20172正常结束，无原CPU／GUI。

编译／链接快照`qualified-f4-left-glyph-fixed02/receipt.json` SHA256 `66a06baf3c55335df22eeeb24f02d324177dd7b05720257ae14979e8c75711cb`保存582个实际tlog输入、22库及App／Unit／S2／Scene／owner probe。`left-margin-build-join01.json`及下节实机后的`left-glyph-fixed-native-join01.json`各2200项来源／归档晚守卫通过。首次collector01因沿用一个不存在的父收据路径在复制前失败，空目录／旧脚本保留；修订02按真实日志和后续Scene测试构建记录，不补造01成功。fixture只规范CRLF→LF、JSON不变并固定属性，未重造expected。

418只资格化给定源相位／LUT／真实FG顺序，actor使用合法全SKIP资源以隔离terrain组合；不签first App前态、任意range.nodes传播、live绝对读钟或自然全地图路径。

### 2.24 单一flash的正式双端反馈

新增原版→SDL串行实机代表已通过。两端从同SHA合法隔离S5档经正式输入进入stage26坏臂，只确认8／13／14，在15末等候前开启WGC，单次ENTER后观察flash，止于16首等待；不确认16、不播放credits、不Save。独立正常前缀定位camera(9,8)、raw1=(15,9)、cache sprite52；源mask624..626包括opaque0，原6-bit color200固定RGB为(203,227,255)。

每端用本轮完整RGB A作为显式前态，B只允许原角色mask写固定色、其余全64000像素保持，并必须实际见同一对A／B的ABABA。原版63个有限采样中见证相位1、474个变化点；SDL59个采样见证相位0／2、494个变化点。**这证明各端合法相位的有序反馈，不证明两端绝对A相同、所有11次CPU copy均被显示采到或PIT壁钟一致。** 没有ROI、旧图、容差或按结果反造mask。

报告`F:/fd2sdl-evidence/f4-flash-native-20261116-01/report.json` SHA256 `9b1902bb78ed5828da2b7e46c9e5bb195faa4add6c37fb083b1c0d500b1bebb1`，pair SHA256 `71d8fa7494709829ad80719ef3f16a8fcba7fb4da90f90e52a9cc68c180df7ff`。原版PID17928与SDL PID22496均正常退出0、清理成功且未强制终止；外层Job27856 worker_exit0／timeout0／residual0／forced0／cleaned1。用户授权的一次显示需求及zero MOVE将OFF恢复ON，inner strict-ON通过；两层需求均释放，没有电源计划或用户系统混音器改动。原资源／隔离档及主项目用户档SHA、mtime均未变。外层收据SHA256 `6e4ccac290d9a716220bde2e336ecb00987c882a30622458651eb700edb10fac`。

实机App为归档02中的`9c6b2fae737e51ff193264df0181a8ee725691a8f3532eef78da425783dfe2e7`，同构建owner probe在首窗前实际导入该SHA完整SAV。后续公共tick尾读和注释修改另记，不重写本候选身份，不为无新像素写点重复旧GUI。入口见[flash代表](../../tools/run_f4_flash_check.py)，默认只prepare；WGC媒体／完整SAV保本地不入提交。

### 2.25 转职empty144的首读取边界

`build/f4/promotion-empty-owner-review01/report.md` SHA256 `c66f8438174efc9aa1473bb2491ff99eba8ed287c207c7dc33471e2e00361d69`实际静态核七个完整函数、allocator及原FIGANI。新ID48请求entry144的3B，真实payload保守界是8..19B，不能据3B直接判越界。`2fb2c`加载后到首次调用没有目录writer；part0首次`2ebb9`读[B+8,B+12)。payload<12时越owner，payload≥12时虽在分配内，但完整current-owner写集只有资源[B,B+3)，该目录无初始化producer，按已定初始化准则到局部首U。没有读取邻接heap或推算后继地址。

原新work清零／BG绘制已达，但新BG到VGA的copy在该首边界之后。SDL略sprite后继续投影是有限H，不属于原像素一致；不能把“已画work背景”误签“原VGA已恢复”。自然16→48、实际block及全转职其它phase仍分别验。该定界关闭旧“仅凭3B判U”的错误推理，不签整条promotion或全F4。

### 2.26 概要板与HP条的实际owner

新增独立整页回归用HP63／max42及105／42得到原signed条长152／253，实际复现旧VGA按320列裁切的差异，见`summary-linear-before-ctest01.log`。这是原WORD范围内的有界参数，不称已观察自然producer。更重要的正式`18b84→18c6d`桥原只交回149×42，丢失合法板外条；现由`field_info`短时借唯一App像素，与320页共用同一有限renderer，按原stride绘板／HP／MP／LV／数字／姓名，再由Game画信息板和投影。Game不再分配或复制scratch，也不持有page内部指针。数字选图与字模算法共用，不新增热状态。

原`17d6f／18795／187d6／18c6d`四个完整函数已从F1 source07逐chunk再核strict镜像，`summary-linear-source01.json`保来源。新C检查两板侧的完整153216B App及64000B投影，包括板外relative149..172填充列和173尾cap；原输入、单位、RNG、palette及水色不变。正WORD32767／max1会实际触达owner尾，独立uniform fill闭式期望核首错保留前缀、不续绘MP／数字／姓名；未执行原owner外写。

`summary-linear-ctest01.log`实际field_info／Unit／battle_animation三项通过3.99s；新增App板外／首错检查后`summary-App-ctest02.log`Unit再通过2.97s，不重跑其余已过项。首轮battle target构建揭露旧测试TU未提供已新增upload的I/O替身而链接失败，补相同帧观察替身后构建通过，不改变生产等待或原像素期望。新实机／全部数值族及summary首比较门仍分账，不能把有表／API或这两组参数称全A14-C08。

### 2.27 城镇／出阵写回同一App

独立来源把stage11首相机孔App `(110,86)` 接到Town61像素70及原zoom phase10；原出阵不会重建或清空该页。修复前Prepare只改VGA，漏掉正常后继会读的App写集。现在Town的已定义320×200写入App `(72,72)`，画完内部312×192后按原窗口投影；zoom64000B前缀也写同一App。接受出阵的非NoTown冷边复用已有`service_transition`，不在热C中藏第二条编排；取消、NO、HOST和NoTown保原前缀及snapshot清理。

`prepare-App-ctest03.log`实际Prepare通过1.34s，含72个Town及198个zoom源参数、冷接受／取消路径和stage11值70／114的独立整页检查。前两轮测试仍期待旧的VGA外框清0而失败，原件保留；修正为来源证明的外框保留，未改产品来迁就测试。原Town分配的320×200以外padding仍未据此定值，标题／新进程Load的App前史与F3后期R继续另签。没有重跑旧776805投影盘点或GUI。

### 2.28 全量typed文字参数的实际C投影

`F:/fd2sdl-evidence/f4-text-composition-C-20261116-02/report.json` SHA256 `9fe60a57c9bda190a1e1b02b56798ec9771ee48624fe48e9309eb84a4e5049f9`实际通过1179个不同请求，对应1180条binding：832个Prepare文字参数、136个概要姓名参数及212个credits参数，其中一组请求重复。它复用771个保留原glyph的独立写值／mask、原UI叶及完整caller来源，实际调用现Prepare、App概要和cinematic typed接口；没有以C renderer生成expected。

生产者`qualified-f4-text-composition03/receipt.json` SHA256 `6dd40dbce45d8a078de44174290842e0d5e7a6db4d70a63894137dc61ad6bb6c`保575个实际编译／链接输入、22个库及同一对PE；`text-composition-build-join02.json`核运行前后身份。首轮在执行C前因工具错误要求不存在的`field_save.h`停止，改为实际类型声明`save.h`后再资格；另一轮collector语法错误未派发。旧失败不回签，没有因此重建或重跑原叶。

这是正式绑定参数到共享renderer的有限函数关系，不是每段自然对白、动态值、整个credits、输入／mouth／PAGE历史或全部HP数值域。双底观察也不冒称原／C物理store次数。普通对白完整历史继续在C06独立补证。

### 2.29 montage正式冷路径与保留原轨迹

`F:/fd2sdl-evidence/f4-montage-cold-C-20261116-03/comparison.json` SHA256 `23696b56cabd2f986f5a60710d47862bed5111ea66ad8d81b24de0496fe5238d`实际通过40组：20个原pair绑定各RNG0／50，完整匹配3596条命令、2779幅整页及各HP／RNG结果。C输入只有pair／scene／RNG等入口标量，由实际`scene_flow_run`、`scene_cinematic_node`和共享`montage_round`执行；不把旧命令数组喂回C。有限视图只选择真实open入口并在唯一close后接现end，内部节点、参数、分支和round不变。

首轮实际第一组失败是observer把14次cue0的无声API调用误当原25a96事件；现仍记录每次API调用、原FIGANI `part+5` cue及调用前后状态，只按原明确的非0分支生成事件，未过滤其它差异或改原expected。修订后生产者`qualified-f4-montage-cold02/receipt.json` SHA256 `69b78cc6f2c01f7ff45e6616fe0ab3304b7a831ec51ba581992a9b90f07c0ce0`保实际PE及566个编译／链接输入。后一次在执行C前因旧工具路径pin变化停止；新严格继承manifest03只把该pin指向已存同SHA／size／mtime的旧快照，原payload和expected完全未变。失败与来源资格分别保留，`montage-cold-build-join03.json`运行守卫全同。

这新增正式冷walker资格，不签整个ending／credits、自然抵达、caller HP／QK或运行时共享RNG轨迹。测试时钟／upload为显式I/O替身；源tick等待实际执行，但不等于物理PIT、GUI或声音证据。

### 2.30 标题与ANI不消费外来宿主deadline

共享等待复审继续定位到`20421`及`1f894`的有限帧：原每帧写完后等待自己指定的ms，不能先消费先前宿主动画的`frame_deadline_ns`，也不能在上传时另采样FIFO。现ANI与标题的滚动／fade／菜单／确认均只upload，事件泵保在原ms等待或菜单无键的宿主1ms空等；地图preview维持原宿主路径。

`title-ani-upload-build01.log`实际App、ANI input和标题私有TU构建通过；`title-ani-upload-ctest01.log`两项通过0.14s。回归保一个显式外来deadline及interval，检查完整标题、ANI正常／HOST后仍相同，且源等待和clear／skip队列顺序不变。沿既有原`20421／1f894`来源核调用序，不新增原执行或GUI；像素叶和已归档native PE保原身份，不借此扩签全标题RGB或壁钟。

同一复审还覆盖Battle与promotion两个有限公共提交点：显式保原宿主input泵和upload，随后只执行本帧原tick／ms，不消费外来deadline。`battle-upload-build01.log`及`battle-upload-ctest01.log`实际构建与battle_animation通过0.89s；新增正常、skip及HOST直接例检查foreign deadline、interval、pump／frame次数及原tick累计。测试中的旧present现在明确模拟deadline消费，与只记录帧的upload分开，避免无副作用替身掩盖该产品差异。原资源叶／2779旧帧不因这次无新像素writer重复运行。

### 2.31 死亡FX去除35项静默截断

完整`1db65`的登记及FX循环按真实D消费；仅`1dd61`改index30为临时base+0，没有35门。旧C超过35仍返回成功而略去后续像素。现锚容量接已有96个active槽，每条至多登记一次，不增加第二状态；保持原筛选、页owner、资源和返回合同。原12图最大72×71，165个可见锚×12的独立几何均在456×336内。正常高D来源与父调用者／QK资格仍后置专项，不从几何或容量反推自然可达。

新`F4DFX002`用正常页内的15列布局，只执行D36／96×seed0／1四组；旧`F4DFX001`及14组原收据保持。真实C prepare/frame输出12帧App／base／VGA，独立原资源／源写序模型比较144个完整owner块、17,780,736像素字节全部相同。特别核第36项新增像素146及frame74的restore-before-alias：App仍取旧base188／219，新base已为32，不提前反馈本帧新底。

`F:/fd2sdl-evidence/f4-defeat-prefix-actual-20261116-01/report.json` SHA256 `bbbdb80e4cee360f2e60ae5c1d13d8e80e759270bb151c1b2e9e9caaf853d946`；同目录`generation-inputs.json` SHA256 `0cda87ea5ff6d6bcadcc8ab9d606a248469d2ab85fc9c43f0f5c0c8f2ec4b03a`保609个实际编译／链接输入、22个库、exact PE及源码／generated／tlog快照。2207项运行前后守卫相同；PE为`7cd2f1f632300129037f19916f8c7819435e8fe85d5733aaafdb19f7a96d971a`。`defeat-prefix-ctest01.log`真实死亡及输出路径两项通过1.73s，包含HOST／非法frame保前缀、坏晚图不提交和清理。新原CPU、GUI及设备均0；显式初始化双页不代签原fresh底页。

### 2.32 详情页源序及owner首错传播

`17e0b／17eef／17fc0／184c0／2b4fb`完整来源及strict字节确认详情owner为64000B，源先绘portrait／top／bottom，再HP条、MP条、数值、姓名／职业／移动类及状态；装备内容随后绘制，不再次清bottom。旧C把姓名／状态提前、底板推后，且丢弃bar错误，可能继续数字／装备和动画。现恢复该顺序，两个既有painter改为返回状态，全部九处正式caller短路；独立item_list入口仍按自身原合同恢复底板，没有新owner或通用绘图框架。

来源复审`build/f4/detail-owner-source-review01.json` SHA256 `dabd5f3ba2359569a4ce3def31f8b1d060b5c596816bb1e5f4cf2aa23e45212d`与`detail-owner-review01.md`分别保事实和结论；候选01／02原件不覆盖，实际采用03 patch `a7a31571be00f40bd12cdf964f4241306c6412e5c5516496144c1627c12c99d8`。没有新增IDA、原CPU或GUI。

两个完整VGA闭式期望分别让HP／MP条先到真owner末尾，核逐列首错保留的前缀、不先写姓名／状态，不运行owner外访问。四个实际Game开页／magic翻页例核错误前0动画／controller／water／RNG、释放私有buffer并保单位及显示前态。这些WORD参数不声称已有自然producer；正常数值页也保受影响回归。`detail-owner-build01.log`及补item构建通过，`detail-owner-ctest01.log`实际Info、Unit、PrepareView、PlayResult、DetailFlow、ItemMenu、MagicList及PreparePlay八项全过10.77s。尚未把旧summary／文字候选身份重签为本次PE，完整自然详情反馈仍沿对应联合门。

### 2.33 普通对白完整页面历史

新实际C报告`F:/fd2sdl-evidence/f4-dialog-history-C-20261116-03/report.json` SHA256 `a4a5a0921ba8bcc1bb12d402dd84df0f6a42089a170044620f084de5e42d17d9`通过155请求／310幅完整VGA双底：box30、marker42、portrait16、scroll2、PAGE／NEWLINE16、mouth36、空等12及23／12窗外字迹1。复用110个保留原glyph及19个原DATO参数叶，并按完整原raw边框、snapshot、marker、scroll、FIFO／mouth／RNG源序独立组合。actual loaded FDOTHER／DATO／FDTXT首请求前与固定原bytes相等；比较actual DAC，read-old-history不冒用双底推物理mask。

实际修复opening每次marker delay后的clear与第五级末clear、N0不画marker、reverse各自从端点作signed截断，以及marker仅0透明（74必须实写）。坐标原已有`opening_camera_pixels=world−4`，所以actor差值本来就是24relative+4；候选再次加4是错误。C02在前15个box成功后第一个marker实际失败，抓到(32,32)与独立(28,28)不符；修实现后C03全过，expected没有改变。C01则在C启动前因来源清单误含本TU不消费的remainder header停止，0次C；更正最低清单、保真实CL／link闭包，并删除候选三重cleanup冗余后另建候选，两个失败不回签。

producer03收据SHA256 `bc7d8875dd0730a0a9a68019abeba752163dbd6a1b61e806c951fd7607aa1465`保561个实际输入、22库及2167项原／快照守卫；PE为`ea7361c8f0a57493211ca45ecf8abe4e5113d46af4c62231f1c9ff9ddbfc4c26`。C03来源晚守卫全同，无原CPU／GUI／设备。新完整marker两函数31指令只读Domain来源SHA256 `ab7852af90594011224ae162e50ec8382b9edd45d03c2566103a4fd3e4fab84f`，明确4ed34／4ed4f是raw零透明，不是RLE或74透明。自然actor／camera、真实早键窗口反馈、设备PCM和完整动态producer仍不由这批typed组件代签。

### 2.34 Scene有限提交与宿主空等分离

在B2版本中，speaker／glyph／PAGE已保原同input owner泵后只upload；dialog空等的H16只借本轮局部deadline，履行后恢复foreign两字段。早键不建ticket，HOST也恢复本轮借用。随后将同一有限helper用于base、运动、group、arrival、refresh和gameover；各caller继续自己的原tick／ms。terminal hold也封闭自身H16，没有新增游戏帧等待、RNG或业务owner。

`scene-finite-upload-build01.log`实际App／Handoff／S5 source构建通过；S5 source通过8.31s。Handoff首轮揭露fast诊断原已存在的末copy从present换upload后，旧palette spy少算它；保全部DAC／delay期望，仅把该独立末copy计入upload，`scene-finite-upload-ctest02.log`Handoff再通过10.77s。C03的155页继续保其旧PE，新的统一helper不是重跑这批页或2779原帧的声明。

首次wave work另有精确来源关系：Load的64×2ms fade支配重建，但原delay靠校准／INT21，并不能只相加名义128ms证明PIT下界；NoTown24／28／29又不能继承Town fade。`wave-first-work-caller-dependencies03.md`把23→24的真实tick门、27→28的对白／同步及provider／低WORD模周期关系分别接A13／F5／F6。新work全部可读字节在allocation内，不能仅据malloc判U，也不能拿calloc零底签V；已更新wave及其它背景343参数资格不因此撤回。

### 2.35 转场／传送的新正式参数

`F:/fd2sdl-evidence/f4-scene-fx-params-candidate-20261116-01/binding-join02.json`把8个transition及12个Scene／common teleport的原CALL PC、实参、selector和当时focus／camera接到现冷图。stage26 literal `(0,-1)`须先加真实relative focus；登记好臂夹具实际传(6,3)，不能把字面−1当负像素中心或报告产品错误。entry22／27清R0后实际(6,5)，注册entry26的新增中心(7,6)，victory29实际(6,6)。缺speaker、不同合法roster和opaque Load的R仍按真实来源输入，不由登记夹具扩签全域；普通动作还有两个直接teleport caller，另归A15，不把12作全程序分母。

独立源模型在任何新C之前逐哈希复现旧原108帧和408有序前缀，原CPU／C均未重跑；新expected以SHA256 `648afe68bec2d935f1a7f065006ec029bb194f713e5e8ce261b539500d40a657`封存。打包后模型文件身份不同，root在C启动前逐对象确认全部七组封存expected不变，旧模型hash保留，不覆盖原件。新批次仅4个转场／3个传送；top18已有旧四组完整资格，不重复运行。

`F:/fd2sdl-evidence/f4-scene-fx-parameters-actual-20261116-02/report.json` SHA256 `953fbab3cac27e6324f1275a338b9b248b6cf52d532a5eb85a22e0c068db2aae`实际通过198有序状态、399完整owner块／43,468,416字节。producer收据SHA256 `d656687ce2b8b2ebfeb942bd03272c02750a96536eef3dc6378004ae00b8e82b`保561个实际编译／链接输入、22库及2185项守卫，两CLI各一次正常退出且进程结束。两PE分别为`3e62dfaa1cf594b2ab665ffc91a779525370c7bd33e7b015a88c39746be9f03b`、`37e8a9dc652aed583a32ab4d092dedbdfddbd6881959fcd33300d947e38cee46`。

首轮在第一个transition begin因旧手工夹具未绑定F2共享视觉owner而失败，0帧输出，teleport未派发。仅补该夹具的真实owner接线及明确range视觉别名写集；fixture地形无动画标志、12帧actor同源，像素期望不依赖clock相位，原expected未改。第二轮来源／原件／用户档晚守卫全同。新批次没有GUI、原CPU、音频设备或完整cold walker；绝对before／初始底页与父调用联合仍接C04／F6。

### 2.36 标题正式caller的整页与DAC组合

`F:/fd2sdl-evidence/f4-title-composition-C-20261116-02/report.json` SHA256 `17d46bbf0826e6c7b11db55ed35b6059a165d0521adc659ab3e6f8f338ca661c`实际通过668个完整64000B页及768B DAC，并由两者严格导出全部RGB。单进程运行现`boot_intro_title`、真实资源decoder及菜单：536个滚动窗、两个cutaway、41次红色渐变、标题渐变及五种合法菜单的40个确认帧均按源顺序观察。源14个阈值的palette102可见状态实际为152，330处ANI返回已恢复101；不能按14×11扩大数量。菜单有10种typed参数，但只有7个不同整页，45条有序绑定不是功能覆盖率。

独立expected复用15个保留原图叶的写值／mask。scroll原owner明确清零，SKIP只保该零底，未把旧非匀底payload当解码图；七个nested菜单图才具有完整opaque覆盖。完整`1f894／1f73f／1f81e／1ff79／16886／4e98d／2df01`来源与strict字节、原15 DWORD阈值及raw palette共同限定参数。没有新增原CPU或旧资源批次。

生产者02收据SHA256 `59f6f9e567a4401aab7cb5347b8729e8557576aa5e993c6cf9a9e83eee211475`保573个实际编译／链接输入、22库、2182项文件与快照守卫，PE为`f1528741a3cc15c39e7a8ca602faf0bb0e713e82c2eee1c3c954a0198d3ef8b8`。`title-composition-build01.log`及`title-composition-ctest01.log`真实构建与默认标题回归通过；C01因工具最低依赖误写不存在的`static_flow.c`，在启动C前停止。更正为实际`static_data.c`，核PE／真实输入／tlog／库不变后另封生产者02，不为工具清单重建或改旧失败。

ANI在本批仅作为明确污染页／palette的callee边界，核main恢复与实际插入顺序；不签其内部自然画面。Scene fade仅借真实fast endpoint，不签完整65／64序列或绝对等待。有限提交保foreign deadline，actual loaded FDOTHER首请求前逐byte同原件，运行晚守卫全同。无窗口、设备、SAV或原CPU；完整自然标题／WGC色管／时序／PCM仍沿各自联合门。工具见[标题配对器](../../tools/verify_f4_title_composition.py)。

### 2.37 动作返回的页、DAC及复合技能尾

完整`2d80d`只有一次`11cac(1)`，位于41次6ms渐变及MAP效果组之前；`2def4→1d4f6→1a80a`仅stop／free／return。旧C在此前使用`refresh_map`多消费water，缺整VGA清屏，正常FINISH又恢复入场DAC并在末浮字之后刷新。现恢复原位置及清屏，停止cast bank后关闭借用视图，保独立MAP bank；正常FINISH不再追加页或调色板writer。41次渐变的首轮已承担同页同色上传，删去它之前无源的一次pump／upload。

同一返回叶另核普通FULL物理`54133==0`的split0／1、FULL法术0..9及DERIVED24／28..31：原fade-out后分别在`2e8fc／30e03／2d7eb`清完整64000B，再`11cac(1)`，最后fade-in。正常已started路径补该clear；montage非零override通过独立scope返回，不经此普通尾。AI射线冷op6已有clear，其op7仅刷新，私有helper明确传0，不重复清屏。显式skip、错误、HOST及未started诊断仍沿现有前缀合同，不伪称原正常路径。

进一步完整源核定，`1f525`末darkness0经`11d40`写满base palette的全部768个DAC通道；正常尾没有旧DAC恢复。water只写DAC224..239而不改base，旧快照可能与base不同。现FULL／DERIVED在fade-out黑DAC下完成clear／刷新，淡入后保base RGB8；移除这两处无源的旧DAC回写。错误／HOST／skip仍恢复旧快照。新20个尾机制向量以原FDOTHER0和独立water phase5形成明确不同的before，检查整App／VGA、黑DAC入口、最终768B base、失败前缀及skip；九个COMPOSITE实际CAST／组序样本另外保留。

`composite-return-build03.log／composite-return-ctest03.log`真实三项通过4.75s；随后DAC增量的`full-return-dac-build01.log／full-return-dac-ctest01.log`真实三项通过5.06s。02曾漏改第四个私有caller，构建以`C2198`失败；其后误派CTest使用旧01 PE，不能计作02资格，两份原件保留。补四参的fixup03与完整AI源单列；没有放宽旧页／业务／RNG期望。DAC源复核SHA256 `d1ebd0ff00134c7bdfb7ba2a32343722014152046a3f7d9951eb009af3499acb`，含完整fade与共享退出，以及三种正常尾逐chunk／head严格比对；新的palette199B／65指令另核signed IDIV40。这是公共尾及caller合同，不代全部前置FX或自然producer。

### 2.38 复合技能的正常窗口代表

`F:/fd2sdl-evidence/f4-composite-native-20261116-01/comparison.json` SHA256 `f74f71aa4a15f379b4173a6a3be0cdce05a369244dd49017a04df9c93289d5f7`实际先原版PID33840、退出后SDL PID12788：隔离真实donor派生夹具经CONTINUE、一次技能34、一次END和UI Save。两端分别28／27个业务检查点及156／217幅原始连续WGC。两份完整22987B容器分别符合独立原状态合同，且本例恰有相同SHA `07d5ffd66934068b14e14b98ed7599ffc52fb7ce958799560dd79ec1c28500ea`；六个duration字节保原有限值域，不借此签完整RNG。

生产者收据SHA256 `df28345ca9977c43ed041f002e71f17eff081cc695233d01780f0f948749b3ee`保565个实际编译／链接输入、22库及2182项文件／快照，App为`ac08674f16d9431d1657ebe50a5c7d846a9b4bae4f2628ec1ac0cbb175dce255`。窗口在上述DAC增量之前完成；其后普通FULL尾的修复不改COMPOSITE正常路径，旧PE资格保原身份。资源ROI仅用于导航／响应正证，不声称整段动画全部RGB、PCM或绝对时间等值；完整尾像素和DAC另由上节独立整owner回归承担。

外层`f4-composite-native-outer-20261116-01/report.json` SHA256 `9078a352f0b29883d48d1aecbf5f04e39ab2751e95000e62901d924e949a028c`保OFF→ON的一次显示需求与零位移idle reset，指针不变、未改计划／混音器，取得／释放同线程15116。两端实际开窗前console／session均ON，WGC尺寸／内容门真实执行。原G2子入口使用共用设施的授权唤醒默认模式，但实际已ON且没有再次唤醒；随后仅把未来入口显式设为`require_display_on=True`，不把新模式写回旧收据。只读后核`postreview01.json` SHA256 `a23da6ae8841d7868d3eec1926cdcae134f73842c9876f8236b48e8376663271`核该唯一工具差异、来源、完整SAV及清理，没有新游戏／GUI执行。

Job35904为exit0、timeout0、residual0、forced0、cleaned1；原件及主项目用户档前后不变，全部显示需求已释放。入口复用[G2共享动作代表](../../tools/run_g2_battle_check.py)的`--scenario composite34 --bootstrap <固定用户配置>`，由既有owned Job和显示外层调用；Linux接口保留，但本轮仅Windows WGC执行。

### 2.39 全部动态UI片段的正式consumer

`F:/fd2sdl-evidence/f4-dynamic-ui-C-20261116-01/report.json` SHA256 `d18d0d428e944d18ce3d97e5def0f18ee6a377998d884e934e5722932634f7b8`实际通过524请求：Progress264、Prepare260，覆盖30个不同片段及35条consumer-family绑定。复用138个保留原glyph、六个完整portrait和原token／槽位底板，独立组合完整VGA、palette及值无关写mask；C经过真实`progress_text_prepare／loot_text_prepare→field_text_play`、`prepare_service_text`及四行`draw_slots`。无键与先peek的FIFO轨迹分账，PAGE的真实ACK允许消费排队键；没有215²名字乘积或新增原CPU。

producer收据SHA256 `ac31451519a8b65b2813189a006a273834466bf6a63480a7433b67e336ac43a9`保580个实际编译／链接输入、22库及2210项守卫。Prepare PE为`5986a5fff6584ab79180ca972cdd6124e41738be37d9726a45772beda748e558`，Progress PE为`038dc167fb1deacaf0448022c6e910d049e73b39bf6d299b0f2d81bf743f809c`；两CLI实际loaded-buffer与原FDOTHER／FDTXT／DATO同字节，Prepare另核只读START实际解密owner。完整输出、首错、cleanup及晚守卫均保留，实际报告无skip。

默认CTest首次Prepare通过1.37s，但Progress旧断言仍期待固定200ms等待产生13个宿主帧。完整`190ac`实际300／200ms仅调用3790a；产品早已按源分离等待。仅修测试为单次精确delay、零frame／RNG／water、真实完整FIFO保留，以及HOST前／后前缀；后继16c57自己的entry RNG和take另验。early cell第二键在该delay的host-pump替身中由真实input排队，不虚构19帧。Progress单项重建后通过5.64s，未重复无变化Prepare；14项工具合同另通过0.176s。旧失败不回签，两个受控uint16 fragment转换也显式限定在原域。

宽正数488只证明已定义DWORD callee与线性owner，不声称自然会产生；负数仅比较四个先行字与第一次非法font读之前清74的原有效前缀，后继安全H不算原像素。初始dialog phase显式固定，跨全文自然producer、shared RNG／mouth初比较、设备及绝对时序仍接各自联合门。[动态配对器](../../tools/verify_f4_dynamic_text.py)不写原资源或存档。

### 2.40 浮字负号疑点的逐caller排除

原`1e0db`确用signed `%d`，当前C用`%u`；新完整14个caller／15个实际CALL及21个owner的strict来源、MAGIC36／Item215原字节和core返回链证明，当前正常typed producer的入队值均非负。伤害／恢复的原spread返回不超过997，即使HP高于max、净变化为负也不改变该正返回；AP／DP在FILD前明确MOVZX WORD，对完整0..65535域仍非负且保守上界9832。永久Item真实六行最大20，另有literal15／power10。DERIVED的signed负值不进入此queue，不能据它造UI91反例。

`build/f4/popup-producer-sign01/review.json` SHA256 `180cee327c7938fe1589df52b312983195b8c972944625a3c1c3cc0b2d532f03`只读复核来源／资源守卫通过。首稿把Item power称BYTE及printf地址写错，已由真实`20cb5 WORD+0e／501b6`更正，失败和旧表述分别保留。当前非负域两种格式一致，不增加无关产品改动。未来若新增有效负producer，仍须按原线性App写义补glyph91；矩形越行并不等于越allocation，也不登记永久豁免。此项不签queue容量、目标数、first-App或自然动作联合。

完整视区／四slot写集及多个caller支持确认`field_popup_number_enqueue @1e0db`与`field_popup_miss_enqueue @1e1dc`；后者原glyph序是116／117／118／118，重复118保留，不算四个不同资源。实现注释和命名输入同改，生成物已按下节统一发布；本批没有新增C、原CPU、GUI或设备执行。

### 2.41 本阶段命名／来源的实际发布

`build/f4/ida-presentation-publication01/publication-receipt.json` SHA256 `33c3d03006de0d429f34d7685f975cb836a058440a5521c002d4d619eca68eb0`记录本轮真实Domain／idalib操作：从已资格PUB03数据库的隔离副本确认并应用七个函数名、33个函数的注释，再完整导出1312个函数及伪C；registry为537项。新增名称为两个浮字入队、四个snapshot／rows helper及wave逐行copy，依据及原登记名同时保在实现注释和命名输入，没有手改生成文件。

before／after／反编译后复核的全部函数边界、flags、持久签名和strict chunk字节相同；其它名称不变。新旧伪C去注释后仅允许上述七个已确认标识符替换，token序列完全相同。反编译close不保存，数据库hash及持久元数据均未受Hex-Rays猜测类型改变；3702f仍保持中性名和无类型。新数据库SHA256 `c348f1d8deb811f8955b98362c813055ee2e4ca58b9245ad7bc2546cb21d0a53`仅作为本地来源，不提交。

全部发布文件写毕后刷新并逐项核对11个SHA256SUMS条目。旧静态LE／prototype报告及source-address报告明确继承，不把刷新清单当重新验证语义；完整源集合确定后仍须刷新source-address审计及最后清单。这是元数据发布，不是运行时BSS、ABI、原生或完整F4通过。

### 2.42 对白marker与落座的当前SDL窗口资格

保留原版`f4-flash-native-20261116-01`的93幅WGC原件，只读重核77／78／79／80四幅marker及91的最终对白页；没有重开原版。marker使用源`4ed34／4ed4f`的12个位置，24×24区域含324个值74，按固定DOSBox DAC换算逐字节核整64000像素；两处不相交marker还原同一底页。原81／82中间采样未获资格，不将其猜为撕裂或产品错误。

新SDL正常CONTINUE进入26坏结局前缀8／13／14→15→16；只快进15，16保完整等待、不发ACK。定位先用252个非74值，再强制核全部324个74及框外不变；同一轮底页、原box19×5、DATO50 pose2与fragment16的十个字形独立形成最终整页期望。结果见`F:/fd2sdl-evidence/f4-b2-dialog-native-20261116-01/report.json`，SHA256 `6eb48a0b616328c7e1cddba4b3d8cd0c828b9207d0bc1197ac534c8235e52809`；comparison SHA256 `4456aea5e86879f4719032fe505696dcb067d01197c443f73e4b095e8c5e3bbf`。SDL PID41116正常退出，证明受控正常入口的同屏关系，不签所有照片、全部中间时点或自然到达。

生产者`f4-b2-dialog-producer-20261116-01/receipt.json` SHA256 `46308ce1786f29b57f6e568115a61ce65b087e6623c7e8a56748af5a8c758e1a`封569个实际编译／链接输入、22库、2213项文件，App为`5d9cde689cf6dc20044f46c6bf678b91b9b70d256337c5c6450520aa00bc4aad`。外层收据SHA256 `271fca7a88cad1cad13854d232318dac711171195cf49c292bdf55c434d9eea7`保Job43568 exit0、timeout0、residual0、forced0、cleaned1；子入口严格ON，全部owned与来源守卫通过，同线程释放显示需求，未改电源计划或混音器。12项工具合同通过；原失败与旧PE证据不回签。

### 2.43 Credits的完整正式冷流程历史

`F:/fd2sdl-evidence/f4-credits-history-C-20261116-01/report.json` SHA256 `62e99becd4d6e6f24e7a868c62894afd22fef1bf81d0e9613e11cb3239d62d86`通过3592个完整页／DAC及源前缀。真实`stage.cinematic.credits`walker运行全部子图并自然结束，不以命令反馈代替流程。case0按原构造outer2→1→0、实际ID2→0→1，无键，共1688帧；case1为outer2→0→0、实际ID2→1→1，在outer2/frame17和首outer0/frame218送两个普通FIFO键，共1904帧。分别22／27次受控RNG，roll500之后才加载同一TAI buffer，所有copy参数、flow／node／owner／epoch与输入前缀逐项核对。

expected复用完整源与保留原图叶。先前疑缺的56图实际已有`f4-remainder-original-20261116-02`的OTHER-056-000直接56记录；该64000B原payload SHA256 `b8aa9087a82a394fb68aea430f01706a22b13c8a63c149a8a296510d261773e8`。没有重复资源或原CPU执行。constant RNG0和两条有限输入计划不等于自然shared RNG、全ending／PIT／声音／GUI资格。

生产者`f4-credits-physical-producer-20261116-01/receipt.json` SHA256 `b51e4312c642985f31b057eecbceb8ebde580be80b2dbe7558920e7188a0b516`封603个实际编译／链接输入、22库、2254项守卫；credits PE为`def204aa73e14a3b17acd9c4cb117ac2a126bc0b922f38af9a751c5069a2d713`。`credits-physical-build01.log／credits-physical-ctest01.log`真实两项通过9.50s，13项credits工具检查另通过。`stage.json`明确为冷输入上下文，不谎称CL输入。全晚守卫相同，未新开原版或SDL窗口。

### 2.44 FULL物理的正式整页合成算子

`F:/fd2sdl-evidence/f4-physical-composition-C-20261116-01/report.json` SHA256 `fc16e8ce69ed0339d339d4250238ed894c8a88c5caec886341b91f0335cd5b2b`实际48组、24,723,456字节相同。独立expected取完整`2ebe1`及已资格原图叶：320×200 board写入stride400的work偏移16040，再按side／layer顺序绘attack与idle、按源split／shake取视口，tint33按原mask分支。完整128000B原owner、VGA、base／DAC及C额外25216B尾保护均核；两种互补before不是物理store mask证明。

2118 FIGANI静态几何和1368个有效attack／effect-loop参数已校验；四条真实split／layer绑定×side0／2×六个可达机制状态共48组，idle用entry168／part2。交换层次会改变1745至16894个work字节，证明该检查能识别顺序错误，不以调用API代签输出。actual loaded FIGANI为15,310,188B，PE为`975784a279f93e6ee6eab3452a0333d781b84c83c585102ad7f351639d0c173f`；PID15716 exit0且已结束，无GUI／原CPU。

与上节共用生产者，`build/f4/credits-physical-build-join01.json` SHA256 `a9fc788b0cf005f4754cef4f94be77dbd177b9bafda72d21b3e0f7f173e25502`只读核全部2254项、实际tlog／库／PE快照及两个已执行结果，关闭其来源生存期。首次join脚本误按另一CTest措辞匹配而停止，改为日志实际`100% tests passed out of 2`后通过；没有重跑测试。整算子资格不代自然目标／board构造、HP摘要、FX推进、skip／HOST、提交或未知first owner。

### 2.45 普通SHORT的线性VGA条及首错前缀

完整`1e739→1685c`按列、再按五行直接写64000B VGA，逻辑x超过319仍可留在同一owner。普通`1e856`与反击`1e611`的App路径不同；此前修反击没有覆盖普通条。候选`build/f4/short-vga-bar-candidate01/candidate.patch` SHA256 `2c2b30954aa093c90366e25f4bf14e9565d3444ae67858abe0531db5ea28ca7a`已采用：去掉矩形clip，三处caller传播首错，初始`1e7f6`也直接写真实VGA，以保当前列已写行，不到两条都成功后才批量提交。App反击和第四值待证合同不改。

独立原RAW五帧、闭式列分类和整页／两侧哨兵覆盖宽度-1／0／1／35／70／104、首列第二行首次越owner；真实SHORT_STRIKE覆盖105→104两条、提前HOST及owner错误，无额外present／RNG／业务写入。panel(224,54)、HP63／max42是有定义typed参数，未证自然producer，不冒称正常存档反例。固定五帧本身无零像素；另用明确合成RAW0列检copy语义，不算原资源资格。

来源收据SHA256 `479283f263a156d119fc4543291ea3d816b17cbf019472e3727f45becc92eb19`核完整函数和原字节。首次构建命令误用两个未登记目标名，以MSB1009停止；Battle已真实构建，未在该错误后签测试。随后用实际`fd2_field_magic_full_test／fd2_field_magic_commit_test`补构建，`short-vga-bar-ctest01.log`真实三项全过4.55s。复审再将App／VGA的同源条收敛为一个私有size／stride helper，删除重复列分类，保caller及两个owner；`short-bar-shared-build01.log／short-bar-shared-ctest01.log`重建后Battle单项通过1.08s，含两owner独立回归。非零HP／max0在正式PREPARE先拒绝，BEGIN前冷图不改该字段；首错在FINISH前观察，没有借清理抹平前缀。本批无新原CPU或窗口。

转职背景的另一疑点已由完整连续caller排除：`2fca9 CALL memset`后`2fcae ADD ESP,0Ch`覆盖flags，随后两条MOV不改flags，正常恢复ESP非零，故`2fcb9 JZ`不取，背景mod3继续；BDA WORD未参与该条件。片段及source09 SHA保在上一节build-join中，不以callee flags猜语义，也无需新增memset或原版实验。

### 2.46 ANI调用者的base／DAC归属

完整`20421／36ff4`只写临时palette52766，没有改main的base53A65或VGA DAC。当前ANI0首嵌入palette与FDOTHER0有29个分量不同；96幅原页均不用这些颜色1..10，所以这是已证状态归属差异，不能写成已观察可见RGB错误。main新增私有借用hook：每帧解码后先归还入场base／DAC，再链原音效hook；所有正常／skip／HOST／错误返回再归还两色区。三组main caller共用，公开animation／viewer语义不改，没有新跨帧热状态。

`F:/fd2sdl-evidence/f4-title-animation-C-20261116-02/report.json` SHA256 `a3945ff98ee63a209aa471f35277bdb8350251b63db53698e88b6d04c0f0c2bc`实际203点通过：ANI0完整96帧、check0有普通排队键仍96帧、原boot ANI3 check1正常skip一帧、ANI0六帧后HOST，以及四个真实函数返回点。经过同源真实decoder和main helper，完整64000B页、base768、DAC768、FIFO清除／保留、foreign deadline及返回合同均核；原帧通过独立原hash绑定的保留payload，未补造旧mask或称TEMP与base同一owner。四个CLI进程exit0、reader均结束，2317项晚守卫相同，无GUI／新原CPU。

生产者`f4-title-animation-producer-20261116-02/receipt.json` SHA256 `7cf9269f0d61c8f12c4322ecd4d3c9e9c6900df3cd8e7e73a2df4b31a531cd98`封570个实际编译／链接输入、22库、2214项文件，PE为`78f2f9209e5469434653dad80ab169b587290c5d7c582a744f7c1c827d1f84c8`。直接CTest另核畸形帧已写像素后的错误清理与原hook先后；它们不混入203个原页数。首次候选整文件换行已保留并改为最小diff；构建后的旧宏重定义已清理。旧main CTest没有资源工作目录，首两轮停止在opener，修为项目根；畸形夹具的open_mem／close也恢复真实实现，断言未放宽。`title-animation-owner-build04.log／title-animation-owner-ctest03.log`标题先通过0.09s，animation_input在首轮独立通过0.05s；八项纯工具检查另通过。01的203像素／合同结果保留SHA256 `e0d4f719ec4013f5a6073295804afe743cea20f8ca31006fecd96469cf9456b4`；复审再发现测试done过早清真实资源标志，使close落旧替身，改为真实关闭后才清标志。`build05／ctest04`标题通过0.10s，并用02新PE只重验受影响的四段ANI／CLI退出，均通过，不把01补签为真实资源释放已验。各失败保原件，没有重跑旧GUI或资源全集。

现有九ANI共289帧只有九个首帧op1；其它opcode完整源表与TEMP读写另核，不由当前素材推广未来palette差分编码。固定水色`4e31c`读取原data60003的93B窗口，分量上界38，不是从活动base加1取色，故“base63导致水色64”疑点不成立。fade／water／自然clock／WGC完整交错仍按正式caller联合资格处理，不将203点扩大为全部时序或音频。

完整静态合签见`build/f4/palette-lifecycle-review02/report.md` SHA256 `6e679c0dac32d91ec31a54ccc86a7f6068f7e52fb1421b42a0f3cd8acb8b2301`及同目录JSON SHA256 `aeaf68ae0540bf82589774d5ed42a82d449a0d2db8cd41f43da15cff92c96e12`。4e31c为35条完整指令／101B，15个原直接调用点归14个owner；七份raw palette全部在0..63。正式cinematic water仅stage20的68绑定，ending的20段palette57 load→restore区间无water／dialog／未知call，notice等待在restore后。标题其它base也不接gameplay water。palette add／subtract和有限fade会覆盖48B水色，后继eligible water再只改该48B，保持顺序而不改base。以上是完整源码／绑定包含关系，不冒新原生时刻证据。

### 2.47 阶段复审新定位的必要修正（处理中）

以下是交叉核正式consumer后发现的实质差异，不能凭先前叶输出通过而关闭，也不塞回F3历史提交。

- **A09／A14范围与路径。** `18890`玩家入口先按免疫选profile19，否则人物28选profile16；物理AI的`14237`则保原profile，法术AI`14b78`另有自己的28分支。当前共享Game范围入口混用了前两者；人物28原profile28／MOV5即可产生正常成本差异，不只是假想高预算。另`4e4be`先无借位扣BYTE成本，再将剩余CL与旧raw3按signed BYTE比较；预算129、全cost1、初始ff时四邻都拒绝、递归深度仅1，现Dijkstra却扩散。这是有定义浅参数差异，未证自然129来源，不能统一判U或靠限制预算免修。原范围传播与`4e4f6`路径搜索分开；新传播树不得冒作原路径，玩家取消／确认的两次原clear也须同批接线。完整深scratch／正常producer与tie仍逐性质验证。
- **A14有限呈现与宿主期限。** Prepare的宿主空等helper曾先履行外来deadline，数值相等时release还可能误清；四个有限scope尾另无条件等待。该组已修为本调用的局部H ticket、临时借还VGA字段，见下节。Game详情及Play列表／总览／辅助图的同族调用仍在补齐；来源tick／ms、输入泵及成功／错误／HOST前缀各自核验。原API的其它主循环／诊断调用逐类审核，不能无分类全替换。
- **A15 FULL法术线性工作页。** 独立原mask精化排除了28条已不绘制的rain transition，剩余有写点的参数不按矩形越界判U。其中ID2 side0 transition的FX27 part16(1,-1)有40个有效线性点、首点work+82560，part17另48点，都在172800B owner内，旧矩形clip省写。统一有限linear-work painter已修并完成八组实际整owner配对，见下节；真正首越owner后的安全H与尚需精确浮点／自然caller的条件仍另账，不增加整ID拒绝或省略RNG／cue。

这些修正及完整合成资格仍阻挡本阶段交付；不将一次正常代表、叶资源全集或全页相同升级为未知原义已闭合。

### 2.48 复审修正的直接验收

Prepare采用`prepare-host-ticket-candidate02/candidate-final-r2.patch` SHA256 `d35670eca23bac4992870588091f11630d4daebae3a9616bd23b0a75450c2226`。helper在每次调用暂借VGA二字段，装本地ticket，取回本次新期限后归还foreign；release只丢本地值。slots／member_detail／roster／prepare_run四尾无源等待已删。既有七毫秒上传成本向量核初轮t7／next23、二轮t30／next39，future及同数值foreign不等不清、FIFO第二项保留、成功／HOST／water错误及四个真实scope尾均有独立检查。正常detail为26次上传加一次H16，共198ms；H不称原等待。

`prepare-host-ticket-build01.log`实际构建Prepare与App成功。随后两轮CTest暴露旧断言把HOST误作可清foreign：choice沿用150000000ns／10ms、finite_copy沿用999999999ns／16ms。仅改这两处期望为精确保留原字段，产品及页／输入／RNG断言未放宽；`build03／ctest03`实际Prepare通过1.40s。最初release单参候选漏private include两caller，由静态复审在应用／编译前发现，最终R2保原双参；旧候选和两个CTest失败分别保留。此批没有新窗口或原码执行。

Magic采用`magic-linear-work-candidate01/candidate.patch` SHA256 `21e475b4d3c4ff6a18c0c4ff5198448ae9d601e3985190e1d94060bad08cdde2`，work_part／work_idle及FX emit共用有限线性写入。FULL owner172800B、DERIVED128000B，SKIP孔保before、opaque0照写，idle推进仍沿原`311e5 >= duration`，不改intro／物理400stride／VGA painter。真实owner之外继续既有安全H，不把其后像素签原等值。

`F:/fd2sdl-evidence/f4-magic-work-C-20261116-01/report.json` SHA256 `f147c59a76cb0a41b86af023e6936e84ebf26b5681680de2522b03a76a92e9c0`通过八组4,837,376B：真实FX27 part16／17 transition及实际emit、真实idle的显式跨行位置、两个owner末尾首错前缀，另有明确合成opaque0控制，不冒自然反例。每包核全部C work／VGA／board／palette／DAC，派生多出的44800B仍须保原底；两个真实loaded buffer在首请求前逐byte绑定、末尾不变。PID3732 exit0且已结束，无新原CPU／GUI。

生产者`f4-magic-work-producer-20261116-01/receipt.json` SHA256 `cae11dc98d5ef71bad6a72685848d8d92b2f9af2d38f4dbbd80ee12dfb711dd7`保528个实际输入、21库、2184文件，PE为`48ce9ba527bd8531a89bcac3e6fc99a48ce12588f7c7b7a3e07588a36c2b9ff9`。`build/f4/magic-work-build-join01.json` SHA256 `1d5da9a24dab8de74cb42b38e0d6eafa5e9dc0577de471d564dc33900fd71859`核全部真实tlog／库／源快照前后相同，关闭source／PE绑定。三项直接CTest通过4.50s，六项新协议检查另过；完整十handler时间线、精确FP／自然来源仍按原门，不重跑旧资源或FULL48凑数量。

### 2.49 Game／Play有限上传与测试观察边界

`game-play-deadline-candidate01/candidate-reviewed.patch` SHA256 `ebe2f6fc98d207a7aa4183bbfe33dafda9ef396837256059dd8ed3b88f1afce9`已接入Game／Play：有源等待仍由原caller执行，有限拷页只做同owner输入泵与上传，不履行外来的VGA deadline；宿主空等借还本地ticket，三个Play及两个Game无源尾等待删除。主循环和诊断入口按分类保留，不作全局替换。11个实际目标构建成功，`game-play-deadline-ctest01.log`实际十项中八项通过、两项失败，不能记成整批全绿。

两处失败分别是旧HOST注入绑定已经删除的无源尾wait，以及System observer此前未登记的两个pure上传。`game-play-deadline-fixture02/candidate.patch` SHA256 `b7cb984f1de7ee18e2e766a48648bed7d3cb336ff5a26f0c2328cd71dd438ca3`只修这两个测试：HOST改在源最终restore上传后、业务handoff前到达；System独立顺序为八次同owner泵／上传加两次pure上传，逐项核role、等待前计数和嘴型，而非只改总数。两目标重建成功；`game-play-deadline-fixture-ctest02.log`中System实际通过0.40s，Magic随后到达另一条旧导航尾wait断言并失败。第一次失败发生在main中更早执行的Give测试，未经过该导航测试，不是新字段布局或构建异常。

`game-play-deadline-fixture03/candidate.patch` SHA256 `65953459d204a56b3b7207682cde9d069761e23efecd2821124e29b51b2b3c0e`仅改导航的无源尾wait期望1→0；35帧、一次final上传、七步、RNG六次、FIFO／像素／owner均保原断言。只重建Magic一目标，`game-play-deadline-fixture-ctest03.log`实际通过0.40s。旧两轮失败保留，其余八项和已过System不重复；这些是分候选的有限源等待／宿主节流资格，不代签自然绝对时序或整个F4。

### 2.50 战斗状态板的raw索引定位

完整`2facd..2fb2c`的95B及九caller／十五个真实CALL明确：side0默认下方，非0默认上方，只有stage24且**raw单位索引17**强制下方。当前仅按side定位确有条件差异，尚未声称自然实机反例。采用`battle-summary-raw-candidate01/candidate.patch` SHA256 `97608f5fb1d48d757be88fbcff9b86a4ea29cc01c4848b8c8728088ea51c7ddb`，唯一冷参数为`[24,17]`；物理、魔法、平移和AI射线共用有限定位叶，raw由已有actor／targets／caster_index传入，不根据unit_id猜测，不新增热owner。

`source_review.json` SHA256 `89fa497be7bb2419af4ab425cc5bb92e74520227ac01c183df03be15b57dbf75`逐caller连接原push、严格CALL目标及底图写序。新回归核27个stage／raw／side组合、一个真实owner首错前缀、四个真实公开PREPARE组合；板内字体／RAW／数字复用已独立资格的未改painter，expected坐标来自原字面量，不能将组合测试扩签成新的原CPU或自然链证据。`battle-summary-raw-build01.log／battle-summary-raw-ctest01.log`实际四目标构建、四项CTest通过4.55s；未重复FULL48、magic-work8或GUI。新名称`field_battle_unit_summary_draw @0x2facd`已加入命名输入，尚待本阶段最后统一Domain发布。

### 2.51 BYTE范围与正式路径的直接验证

`F:/fd2sdl-evidence/f4-movement-byte-candidate-20261116-01/candidate.patch` SHA256 `e5376c1ef03da3933355cc036f2b2f4d727602537afe9edad9be9400ec877859`已接入。范围以原R/L/D/U顺序传播raw3，先无借位扣BYTE成本再signed改善比较，正式18890保入场owner，纯查询另以已清隔离copy为前提；三个caller的profile策略独立。实际预览／移动改用原finder，范围的previous不再冒作路径；18993、18a0c、14eaf按源接回同一map清点。finder保signed门、首方向计段及BYTE score回绕，没有给深路径加新cap或据C数组推U。

来源推导`source-vectors03/receipt.json` SHA256 `f99d90099cc894e8d148a1983983cdaabf1cb6b2672d89ee7b188eb2058d732d`含863范围参数／2739完整DWORD格、65浅path，范围及path最大activation为7／5。原580B movement表另逐字节核strict image，生成器输出不自证。15项来源／模型正反例实际通过0.013s；`movement-promotion-build01.log`八个实际目标构建成功，七项CTest中path、player、march、geometry、natural_plan、Battle六项通过，AICommit两条箱子成功断言失败。

独立源`13c77→14b78→14eaf/4df4c→13d1b→12263`确定开箱前raw3已清FF；旧夹具错误地把入口6b当未知cache永久保留。`f4-movement-b5-fixture-candidate-20261116-01/candidate.patch` SHA256 `570db22d2506e6d4e406fe5e97197a85d037828a764c8d61142c77594d466690`仅改两轮共享的成功expected为`FF0000F9`及来源注释，物品、位置、完成位、RNG和未到达移动时整map不变仍保留。单独重建AICommit，`movement-byte-ctest02.log`实际通过0.06s；其它六项不重复。旧失败保留，不改签为一次七项全fresh通过。

本批未签深scratch或全部正常producer：frame index32／depth32（第33次activation）会写原6017a mode，不能按逻辑数组尾判U；真实正成本的固定long预算28可独立证明不触及它，但动态MOV仍须原owner联合。14c3a首probe入场raw字节、最终FF返回消费、物理移动后的攻击后置门、Scene raw3与NULL LUT前提均明确保留。六个4e390 CALL分母中14818／1598a另有攻击／法术范围合同，本次三移动族修复不代它们整体过门。已完成浅源／caller修正不因这些专项回退，A09完整域也不因此关闭。

### 2.52 转职七段的完整页／DAC组合

新增私有`--f4-promotion-composition`只调用真实open／step／close；上传观察不替换原绘制或阶段。独立expected用完整`2fb2c／2fe14`、既有BG三图和FIGANI原像素／实写mask生成，不调用C decoder。32个来源参数为17普通、14特殊物品及9→52；它们是源定义绑定，不称32条自然转职已经操作。31组完整七段，16→48仅截止phase3，保留完整旧图／白化前缀，不把首`2ebb9`之后H省绘算原帧。BEGIN的work尚无源初始化，不读／输出；首次原clear后每次上传都比较完整128000B work。

`F:/fd2sdl-evidence/f4-promotion-C-20261116-01/report.json` SHA256 `41675b8d4c3c6fce90970e16ab97616578bd131b89d8815879a9b05401e8ff78`实际32组／8262点通过，比较work、64000B VGA、768B base和768B DAC。完整组263帧，尾tick41／delay586ms；边界前缀109帧，尾tick17／delay328ms；每次上传前累计tick、delay、输入泵顺序均核，foreign deadline及其它VGA metadata不变。BG／FIGANI两个真实loaded buffer首请求前逐byte绑定、末尾未变；73项比较器守卫相同。PID32200 exit0且已结束，无原CPU／GUI／设备执行。

生产者`f4-promotion-producer-20261116-01/receipt.json` SHA256 `28e0caf5c9478a608a2dd8c34f2818ea5e4be1d45d77998e4859c1577eff0e22`封532个实际输入、21库、2200文件；PE为`ad719c2dd4b69fd37dfefaaf9c9bd31b3a8dc0312c78c5ddc60f1148a8201c5e`。`build/f4/promotion-composition-build-join01.json` SHA256 `4693885aab6ae871f205d1b42d250e1d918ea01b32aeaf2c03a79fbfee2e022f`在实际配对前后核全部原件／快照不变，闭source／PE绑定。build02只重建Battle的新CLI解析／来源注释，不把此前build01的Battle CTest通过回签为新PE重跑。

11项纯检查包含VGA边框、work末端、base／DAC单字节、帧序／等待、截断／expected身份及冷边破坏反例，实际通过0.488s。expected01先生成；补完整比较器及静态冷绑定门后，以新目录生成02（SHA256 `10a9116eaa6b1a69247664c2b60eb8251325431128f48ccb9a35e2fdb9142231`），32组请求与每点期望／计数均与01相同，原件不覆盖，没有重跑资源原码批次。七个当前冷节点的args／next／error逐一静态相同；实际C本批直接调用阶段step，不能叫冷walker全流程。无输入mock、自然初始base／进场／物品消费／身份提交、真实PIC／PCM仍依各自正常caller资格。

独立复审`build/f4/promotion-composition-peer01/report02.md` SHA256 `e822ab6d7a1b26a04d319891ccbd6257bc43efa359cc3b929bcecc19fa680c8e`核原模型／计数与薄冷图资格。随后补CLI固定文件名／同目录的显式前置，新增反例后12项纯检查通过0.483s；`promotion-resource-path-join01.json`只读确认已有成功运行的BG／FIGANI正是同目录实际固定名及已守卫原件，未重跑C或expected。此补门不修改旧收据和生产者快照。

### 2.53 actor完整BYTE相位与前景线性源

`actor-linear-candidate01/candidate-final.patch` SHA256 `61b1c5346250b3a92c21a70503021715a3eeff7edcc06106e8e691ca63b15ba7`修复另一类有效owner省绘。原127e0按raw4整BYTE及方向增量1824／-4／-1824／4算线性地址，`1293f`只在临时base加入前检查相对负偏移；1da16没有该门。dir3／raw4=96的offset30552仍在完整App内，旧x456矩形门会省绘。现两caller共用原RLE visitor的窄456线性分支，仅真实实写点检查153216B owner，SKIP不触目标、opaque0照写、首错保前缀；没有新decoder、帧上限或时钟／RNG状态。高相位是源定义参数差异，未称自然producer已证。

原12ac6只有负y／视口门，x=-1读取WH或前行末cell；复用已有left-margin getter，保持其它二维map API。Game／Defeat／已有Game的Scene均借当前visuals的range LUT，不再把NULL参数当无marks依据。只读`compositor-final-binding-review01/cursor20-zero-mask.json`证明20个真实cursor没有opaque0实写，因此旧cached0透明对这20绑定可继承；`map33-header-overlay.json`证明33真实WH地形合法且无overlay，只排header的这一分支，不推广任意地图或前行末marks。

八目标`actor-linear-build01.log`成功；最初CTest实际匹配六项，其中Page、Defeat、UnitLive、Handoff、Battle五项通过，UnitRender失败。Cinematic真实登记名为`s5_cinematic`，补单项通过0.08s，不能写原regex已选中。UnitRender的完整App通过，VGA未投影；新增首差诊断实测case6是最底行向下raw4=6，第三个原前景格(12,8)超出合成13×8 backing。新getter按真实源owner停止正确，旧夹具不应将该读继续当有效域。

`actor-linear-adapter02/candidate.patch` SHA256 `b3fa3fbecff45d30ec77266e96243e90ea7770b85156145dcbcab9e905d07964`只把该夹具补为13×9，全部原App／VGA／业务期望不变；并补Battle独立`map_physical_actor`的同源App adapter，使用同一线性叶。新增1024个direct、24个map action、48个Battle adapter参数及完整页、opaque0／孔、WH／前行末／首源错、目标首错和极值检查。两目标重建后`actor-linear-adapter-ctest03.log`实际UnitRender通过3.11s、Battle通过1.87s；其它已过项不重复，旧01／02失败保留。扫描其余生产RLE调用，tile的有效视口和fixed_page_actor各有独立原合同，没有盲替全部XY接口。正常高raw4／row外单位来源与A09 raw3／node一致性仍按原caller联合门处理。

### 2.54 本轮新增依据的第二次实际发布

复用第一轮F4已资格数据库的隔离副本，经Domain实际保存2facd一个名称及十个函数依据注释；新增注释覆盖五个移动caller／leaf和四个线性呈现caller及2facd。没有改指令、类型、边界或全局设置；全1312函数的持久签名／flags／chunks／严格字节前后相同，反编译后元数据与保存后相同。旧／新伪C去注释后仅有`sub_2FACD→field_battle_unit_summary_draw`标识符替换，registry为538项。

实际收据`build/f4/ida-presentation-publication02/publication-receipt.json` SHA256 `b3cdf001ed0f9c180364f678d73f45fa2c7066fc618c5c781364fb4db257f141`；新库SHA256 `9c91e3f719aedc7906409ce747578b7ad332e5a71fc2f2000f0c146ca7198829`只留本地。原F4源库c348…及strict image守卫不变；11个发布文件全部写毕后刷新并逐项核SHA，11项发布产物检查通过0.054s。静态LE／prototype报告继续明确继承，不回签新运行；最后源集合地址审计仍待其余组合改动确定。此发布不签原生、运行时BSS、实际ABI或整F4。

### 2.55 Battle独立adapter的共享帧相位

后续逐binding复核发现，Battle私有1da16 adapter仍以`units.walk_frames[raw]`代原53C07共享cursor；公共map action已经按源读显式cursor，不能因此代签这条独立调用。`actor-source-phase-candidate03/candidate.patch` SHA256 `91b10d8364e8c8cde58d5ba017a2bbeffdef9247e31b5349a9bb9f2f14cdb65b`为私有helper加一个显式相位参数，SHORT目标及RECOVERY两个正式caller都传现VGA视觉owner，不加新状态或读钟。raw4=0仍取idle，3映1；无page诊断也沿同一个显式来源。

新三帧RUN流用0／37／84三色，局部walk固定2，cursor0／1／2／3须选0／1／2／1，另核静止时选idle2；完整App before、opaque0、无额外wait／pump／RNG均保留。根静态审查在构建前发现该候选测试只给offsets0..2赋色，但dir3实际取9..11；仅将夹具偏移表修成`(f%3)*48`，不改独立颜色期望。原TMP reader允许重复／重排start，三色各48B完整流合法，不使用next_offset差值。

`actor-source-phase-build04.log／actor-source-phase-ctest04.log`只重建及执行Battle，实际通过1.85s，含新五个相位例和原48几何例。此前48例及其它目标保旧资格，没有将原PE回签为已验证共享相位；没有再跑资源或GUI。源函数名称／ABI不变，53C07 caller依赖属于本阶段末尾统一来源合签范围。

静止分支的`game.idle_phase`另按正式caller证明，不新增第二相位：SHORT前的`refresh_map→runtime_refresh(0)`同一写点把owner actor_idle_phase同步到Game，后续UI／wait／terrain／raw26不再调用1297d；13fd4的recovery_focus即使零距也经过FOCUS_BEGIN／11cac(0)，支配后继三次step，期间同样没有1297d。1a30b的Play恢复入口直接读VGA owner。这里只关闭这三个支配关系，不能推广成Scene所有时刻Game缓存与owner恒等。

### 2.56 回合横幅的App中间态

原10010@1054c／1057b先在App绘制91及数字，105a7投影、105b1等待、105ca才恢复；旧Round只改VGA，最终画面相同不能代签等待中的App。`round-app-prefix-candidate01/candidate-final.patch` SHA256 `0ff4ef864a29cdb4994da55095c72230cd1c63e997d260dadee4308fb177d01c`只借本轮App的真实87×20窗口作为底，在对应临时VGA窗调用已有LMI／数字叶，再将这个有源窗口写回App并投影；不依赖旧VGA底，也不开放普通UI回灌。等待成功才恢复，HOST／错误保已达App，page身份变化即停止，不建新owner／decoder。

新增turn0／255的62条有限history，逐次检查完整153216B App和64000B VGA，含square4／9／16／81及每个等待的HOST／error。before故意与VGA intro不同，expected由显式值／透明mask构成；既有原UI91及数字的完整BYTE资格另继承。`round-map-history-build01.log`三个目标成功；同批Round通过0.24s、Battle通过1.87s，PlayResult失败。后者是更早18993真实清图接入后，旧取消夹具只有UNIT_SELECTED却map=NULL；仅补该case的8×8实际map，并增加脏标记清成FF断言，所有原单位／输入／RNG／游标业务断言不变。单独重建PlayResult，`round-map-play-result-ctest02.log`通过5.86s；不将这次失败归为Round错误，也不改签一次全批新通过。

### 2.57 MAP／Item完整页历史

`map-item-history-candidate01/candidate.patch` SHA256 `58b3e9d92fa7ccae197ceff57e755e6f5f0dc38ecd7404c961b1d2cc450dcc02`只增加Battle私有CLI／给定页输入adapter和独立工具，不改产品算法。expected取完整原source顺序、原52006 RAMP表、169张透明LMI／九个FDICON原mask，保三个actor样式、Item20→core4／RAMP、21→core0／DUOTONE、24→core11／RAMP的真实override、hidden direct与batch差别及idle3→2。refresh／terrain是明确给定的页输入，不冒11cac／11eee实时执行或自然初始背景。

`F:/fd2sdl-evidence/f4-map-item-history-C-20261116-01/report.json` SHA256 `237bcfe8f96a149c57e7fe1018b97b9b8bd84cc5a83990975706b1dc5fabfa73`实际98请求、1677上传点加98个return包通过，共1,080,563,200B。每包核完整App／VGA／board／work／map_base／base与DAC，含三样式不同HOST／skip前缀、RAMP最后App与VGA不同的历史、极边界／重叠反raw序及双底；没有用事件hash或帧数签像素。FDOTHER／FDICON实际载入缓冲在首请求前逐byte绑定、末尾未变；140比较器守卫相同，PID25284 exit0且已结束，无原CPU／GUI／设备。

生产者`f4-map-item-history-producer-20261116-01/receipt.json` SHA256 `f1ca5c6de98cfee0bbd9e3bb479e691fa96355b2b4c79143cb821da7e687f81e`封530实际输入、21库、2203文件，PE为`9600819d06eedb70e554f3bbbe80d7250c2fda299c65071bec6a400e67eeee75`。根`map-item-history-build-join01.json` SHA256 `bc346bee113cf1b4c4e84b67b5fa03e9ad5b560bcb4ffa525ac1e9b6c77ecd6a`在实际运行前后核全部原件／快照相同。新文件仅Git自动CRLF与lane的LF不同，`map-history-source-application-join01.json`逐文件证明仅换行，实际编译／PE按物理字节封存。14纯检查沿同代码已有实测继承，不重复原资源、FULL48、magic8、promotion8262或native。MP／物品／逻辑效果提交、teleport23及quake／radial wrapper、自然producer／真实时序和PCM继续各自合签。

独立复审`map-item-history-independent-review01/report.md`未发现本批像素／绑定反例，但指出SOLID模型在上传前的内部cue位置较晚。现将期望模型改为原`1c309`的work底拷贝→cue→actor绘制→投影，明确本批只观察上传／返回边界累计cue，不能签逐store／PIC绝对时序。该调整不改变已保存的像素包，不重跑1.08GB批次，也不将旧producer身份换成新工具身份。根侧首次单独运行协议测试因工具导入路径未设置而在运行测试前失败，保`map-item-history-protocol-root03.log`；仅补测试文件自身的tools路径，`map-item-history-protocol-root04.log`实际14项通过0.587s，未更改生产C或原oracle像素。

### 2.58 范围复审关联的AI已选目标合同

完整14237独立复核确认两处地形门均为1f183非零才修正；14586每target重装`actorAP-targetDP`，count只控制枚举，不能跨target累计。原AP／DP／HP均MOVZX WORD，固定百分比及signed运算在完整WORD域不溢出；按`>2`、严格`>HP`、counter、hero向0的顺序评分，初始0／0也只接受严格改善。另1548e在14b78移动后直接聚焦原已选目标，不重验攻击范围；14237与text28实际路径profile分叉，不能把C额外拒门当冗余安全检查。

采用`F:/fd2sdl-evidence/f4-ai-selected-target-candidate-20261116-01/candidate.patch` SHA256 `6aed6f42577189f50e8f6395502011ccf4784ffd93a0929d21d320cea43a8672`，只修这个评分族和私有AI后置range门；初始candidate／stale／side／HP／hidden／资源前置、事务／HOST与玩家API均保留。`source-evidence01.json` SHA256 `e59af98c73e121500d1ba18cbd4bc26d512abb01f2dc628b83ae70b8221acf8b`及独立peer核完整源push／flags／算式，expected常量不从修复C生成。四目标构建、`ai-selected-target-ctest01.log`三项AI／AICommit／natural_plan通过0.24s，含150谓词类、极性／kill／tie／高WORD／负分，以及SHORT／FULL成功和前置拒绝。受控profile28→1从(1,1)选anchor(6,1)，实际到(3,1)仍攻击(7,1)，保4RNG和PREPARE→strike→FINISH；stage19真实模板slot67只证角色／成本分叉producer，不称自然发生该位置轨迹。F3历史不回写，A09深scratch／最终FF和真实原生代表仍另记。

### 2.59 DERIVED的完整part与marker历史

`derived-history-candidate01/candidate.patch` SHA256 `1b1495de29fadfc9ccf4f1f29c6e549269d48e1662015cb588c71065ae06be8d`只加私有probe／独立配对工具，12项纯协议／数学检查已有实际通过，不改生产接口。期望取完整`2cf30／30e9d／2e9a8／2f4d4／2f631`源序和保留的原像素／mask，结合原BG／TAI／summary／真实魔法费用；按part及marker推导完整work、VGA、board、base palette和DAC。原work为128000B，C额外44800B只作不写守卫，不扩原owner。

`F:/fd2sdl-evidence/f4-derived-history-C-20261116-01/report.json` SHA256 `33061f66fc323c4e1dae6c3fc7d1d097d49cefe9c8396cf87d3ca86c812a0419`实际23请求、3597上传、4001个完整包通过，共1,209,646,336B。五ID24／28..31×两side×双before中，side2的10条属于原wrapper实际left-pan路径；side0的10条为独立right-callee组合扩展，不能签成同一原wrapper轨迹。另保pre-skip及两summary首owner错误前缀。五份真实loaded资源首包逐byte相同、末尾未改，业务Game不写／RNG0、marker费用与HP板、idle `>=duration`、shake／tint／cue及各part等待均逐边界比较。PID36088 exit0且已结束，无原CPU／GUI／设备。

本批producer收据`f4-derived-history-producer-20261116-01/receipt.json` SHA256 `6fb9f563a2b208e47261530eb0223a50b9b65af771732f27678c508174b55a07`封531实际输入、21库、2207文件，实际PE为`08ff3c1cc403662943664f685299e78f5d43fb0722b0ab07ce6fe298bf3eb815`；根`derived-history-build-join01.json` SHA256 `ad1ffe26a097b3f8f81482127f73df7225feafc3ed8814ff8a4a3355b8ab1197`全部原件／快照前后相同。此次只构建新增私有入口，不重复普通Battle、原资源叶、MAP98或原生代表。idle reset在不读该状态的pan前后可交换，仅比较规范化逻辑状态，不冒称原物理store时点相同。正常技能producer、ID31／caster32配对、业务提交、fresh before、设备及自然整链仍分别合签；所有真实slot2 duration均正，不伪造零duration原样本。

### 2.60 深路径的模式与同一输出缓冲

`movement-source-layout-review01/report.md`按六个完整strict函数确认：计数`60077`和结果`60078`在scratch之前，index31的保存EBX不覆盖控制字节；index32的DX高byte才以当前绝对y覆盖`6017a`，pop不恢复。后继真实mode还控制同一`60073`输出：`4e751`先写结果长度再复制方向prefix，`4e703`依X→Y→result1写入。目标检查发生在进入child覆mode之前，root还会无条件检查目标；不能仅修改mode仍保两份found／输出真值。

采用`F:/fd2sdl-evidence/f4-path-mode-output-candidate-20261116-01/candidate.patch` SHA256 `b66f1cf7b0b8bc4092581c5993e173c566fe89ee2d5c1c42993004fdfcbe34fc`：既有finder共用一个有限借用output view，保实际result／mode／已写prefix及尾；旧typed接口只投影这份结果，不建第二range、热owner或通用内存VM。容量错误保有效前缀，是C的H边界，不能据typed NULL的2B局部视图推原caller extent／首U。原root result0未写XY与typed“无完整witness”继续分账。

独立源派生收据`f4-path-mode-output-source-20261116-01/receipt.json` SHA256 `fee4df911f498b99342272e65ed46b0e2b68a736b8979d725176e21ffc2556fa`固定原580B成本表及完整函数，不导入修复C；16组正成本参数、最大34activation，含31／32、方向与XY互覆、回退不复mode、目标先于child、y0／1／2／3、fixed long28、容量首错及root零输出，11项纯合同检查通过。根`path-mode-output-build01.log`八目标构建成功，`path-mode-output-ctest01.log`路径、玩家、march、geometry、AI、AICommit、natural_plan及Battle八项通过1.50s。完整64B输出／原BYTE结果／实际mode／写前缀及输入map／unit不改均核对；不将源模型的内部raw图冒作C实际raw图观测。

frame33+的`60181`外部三字节表仍另账：已登记getter→reader的普通`117e7@118ac`调用在strict CFG中不可达，但尚未穷尽间接escape或全对象collateral。自然高MOV、原真实输出extent、原入场raw图及count0默认anchor的后继消费仍保F6明确门；不新增31门，也不以16组宣称完整source-layout或全游戏完成。

### 2.61 wave新owner的条件首读取界限

独立`wave-first-owner-review03/report.md` SHA256 `d0306a008ba22ddb7f25842274fba026beb80d9b21604c39e806415fe5e63a87`更正旧02中“必须取得所有旧heap／平台初值才能分类”的过强要求。依据当前owner初始化准则，完整`10652→10010／1088d→11cac→11eee`写闭包已足够：旧work明确free，`1087b`新malloc64000并只发布至`53B03`；完整父caller覆盖返回EAX，后继资源／active／cache及App均是其它不重叠live owner，没有wholeLoad／retained运输或其它当前像素writer。新静态检查核七完整strict函数及已发布全部16处显式引用，无新IDA、原CPU、C、GUI或测试。

若该owner尚未实际经过`4eee0`，首次`11f62`恰相等而跳过唯一填充，则`120fe→11eb0→11ed4`首memmove源字节work+0即没有当前像素producer。这是**条件未初始化语义U，allocation内并非越界**；不必先证明该条件自然已发生，也不将原像素猜为零。当前calloc零work仅属该首U后的有界H安全后缀；不能据它签原before一致。首比较不等则先完整写59904个可读字节，随后所有sameTick保持V；4096B未读padding不影响此合同，重建开始新的owner分类。

这关闭wave本体“可读像素来源”的未决R，不强制首更、重置原last／phase或复制旧work。正常时钟何时确实走该条件、PIT／delay及绝对设备轨迹仍按F2／F5／F6原门；旧02、名义128ms分析及343实际候选均保原身份。若后续发现真实合法alias writer或当前owner初始化接口，应按新来源重审，不以本结论永久排除V。

### 2.62 死亡前十三次旋转与base的正式组合

`defeat-composition-source-join01/report.json` SHA256 `18e984b9e981ad2071a5cd7c81144926843e4c46cbc43e1081ec69b6bacabf3d`只读完整`1db65`857B、当前冷图与已资格A14叶。原`1dca9`给13次，`1dd15`给direction=i%4；每次地形→逐raw非hidden actor（死亡者改方向）→FG→312×192投影→wait1，随后全零HP flags=1、独立base地形／batch、cue及68..79十二次FX／wait，free后尾`11cac(0)`。唯一冷图的86节点，正常84操作及无可见死亡时3操作，逐顺序与这些原常量相同；当前Game／Play只消费原owner及既有绘图叶，不新增1297d或water。源／代码／冷输入前后守卫相同，无新原CPU、C、GUI或测试。

该组合连接已有完整actor／BG／FG／page writer与已实际比较的死亡FX双owner历史，不再笼统把十三旋转／base全族列为未审。base原为新allocation：只在其首次语义读已有当前producer的像素上签V，opaque整页运输不自动提供未写margin／C0孔的合法颜色来源；若先发生具体当前owner未初始化读取，按首U准则止等价前缀，不能把C calloc当原值。高D、原HP／QK、自然结果与绝对clock／PIC仍沿各自门，96容量不作自然人数证明。顺便修正源注释把0x359误写成0x359B的长度，不改产品语义。

### 2.63 COMPOSITE完整历史与停止控制的分批结果

`composite-history-candidate01/candidate.patch` SHA256 `f907a46854c70225a2daa81368a5bcba521f05cabe671ec3e78bda4128948874`使用完整`2d80d／2df01`及37张真实粒子原mask，按四ID32..35×两side×互补before推导slot1费用／intro、8步caster与9步粒子slide、part顺序、32／35末两part交替、41级fade和MAP组前return。每包保work172800／VGA64000／board64000／App153216／base及DAC；原work128000外的44800B只是不写守卫。App在演出前已由原2d848释放，C的dormant field_page列仅为宿主保留／误写守卫，不能算原同期live App相等。2dcea发布新App，2dd51回图在41次渐变及MAP组之前；phase3回图及新owner的before为明确输入，不冒称原fresh heap、真实11cac或时钟，分类见2.74。

实际`f4-composite-history-C-20261116-01/report.json`保**失败**身份：前16正常参数、2852次上传及16个return包全部逐byte相同，随后case16 pre-skip在board+177失败（254对0）。源／PE／原件全晚守卫相同；producer01收据SHA256 `740696eeca07623cda336bb5586ce3e377190c64bb483e0f236ab96070d69014`，PE为`b6757bab8468ae8424148b488450039b95d279cb82e742aecd3e54adcc3b3513`；`composite-history-build-join01.json` SHA256 `75c18b5cc4a0c46bfebe2fcc1ada37974e46124459b3823f9440d4d75c7c8535`核532实际输入／21库／2219文件前后相同。不是全19或新候选全部通过。

独立复审定位C在已经停止slide／particle动画后，仍无条件把33／34的particle0画入私有board。只给该行补`!stopped(ctx)`，统一既有H快进／停止次序，仍保已达intro费用／summary／work前缀；原DOS没有此skip入口，故这是宿主合同修正，不称正常原版差异。`composite-stop-build02.log`三目标成功，直接Battle／MagicFull／MagicCommit三项通过5.25s；正常`!stopped`路径不变，16正常仍保旧PE实际身份，不重演它们。

新增`pair --controls-only`先核完整19条expected身份再选择原16..18，实际stream重编号0..2，report保原case映射；不改expected02或payload，不重新生成1.4GB。22纯协议／数学检查通过。`f4-composite-history-controls-C-20261116-01/report.json` SHA256 `880790900b9adfe9e4d8c8a81e665adcd4096f148d163565b1c8b6d5cdab0a9d`实际三控制／252上传／255包共116,165,760B通过，覆盖pre-skip、声明adapter一像素error及回图后HOST。PID6868 exit0且结束，五loaded资源首尾不改，无新原CPU／GUI。producer02收据SHA256 `13943ef76ab7a2600de5e19ca7322960c0550b300d2a76cfa2bf4df173ecf705`，PE为`1b24ddb2780444d47d26ac03692d4eea9a2f05ca9ed9eceda2093d75bee59abc`；`composite-controls-build-join02.json` SHA256 `3336a5a18ece8a10a1e67c70b2314dc6e0b552eeb5bedeb0448afb49e233a033`全部守卫真。后续MAP组序按唯一冷定义与已过MAP98接同页输入，不重复费用、清DAC或末刷新；业务提交／自然producer及真实重建before另门。

### 2.64 开场活动档资格与map125的reader更正

`stage32-active-save-review01/report.md` SHA256 `c2ce283da1fa9777b30be1f306c8b69a0df6b2e3bcf34b2f310340d5083c8ec3`以原`3231b`完整CFG及保守235 callee／68个有完整strict来源的application闭包确认：32→31→0期间没有controller／active或manual Save入口；唯一正常return32974已被stage0写支配。不能仅因`10010`可读stage BYTE32，就把诊断档当正常START生成的fresh32活动档。正常opening的首13×8与后继pan仍须保原资格，外部独立合格文件也须按实际来源另审，不判成U或凭输入门永久排除。

`map125-entry-review01/report.md` SHA256 `7006e5da5104d88f06938d2b7d0b83e695361aa4f68bde3f59682b087a8d315a`更正旧reader归属：完整ownerless窗`21d0d`实际调用`4e795`，不是`4e98d`；map6/125的43B是3×13 raw mask，35非零写、4零skip，非零时写LUT[旧目的]，不是LMI或source-color LUT。现完整发布72B及既存DB chunk逐byte同严格镜像，本次未新取Domain／运行ABI，不能把已有229项LMI或66页批次回签为该raw reader已执行。已闭39个application分派及body无入边排除均保持；全启动只剩A12两个type9 provider条件，不能据某一次type1捕获永久排除。reader数据层已明确，入口／实际reader必要资格仍挂这两个具体条件，未据此宣布A16全过。

同一外部reader还在`4e7ab`入口fresh写WORD6017b，再于4e7bc读取，故它不消费deep finder遗留CX。此前scratch全字段盘点遗漏这一真实scalar，现以新补记更正，旧来源／报告哈希不改；当前六函数mode／共享输出修复与60181表的独立残余不变。

### 2.65 新进程CONTINUE的App孔与真正首投影读

`fresh-app-load-owner-peer02/report.md` SHA256 `b27bab589db6e1986d65a8af4017df7cae089a6d62e75435ab36128cc9debc94`及`conditional-mask.json` SHA256 `3b2a03e0cf83c96ef162c0bee1b4e967e5955687c8be2157ac39fb687ab9b977`给出当前owner／完整后层条件。main新malloc153216只发布App；标题的160B实际是53A45，cutaway／ANI各正式调用只写A0000及自有TEMP／临时页；成功CONTINUE不经过START或Town，10010至首11cac的资源／单位／cache／箱子和声音各有独立owner，无App字节运输。此条件不同于同一live App已存Town／旧场景的Load，后者仍须保真实before。

本次必须更正父`source-check02`与peer01的一项推论：活动Load先把map raw3清成FF，`121fe→1220c`走`4e22a`，其C0只推进目的，**没有在RLE点读取旧像素**；不能从pre-actor mask孔直接宣布已U。旧报告原件保留，peer02撤回该读义，标题／alias闭包继续成立。

原mask新静态重排stage11 camera0恰有36个未写点。限定可见actor／FG足迹均避开这些孔（原14部署都在16,49、其余hidden等给出一个具体参数族）、Load禁用selection、info只写relative y157..190时，后层仍保36孔。按真正viewport逐行读取顺序，最早未有当前像素producer的是App(127,81)，offset37063，对应VGA内区(55,9)：`11d36→11eb0`第9行的`11ed4`源+55。这里登记函数级读字节，不冒称原memmove内部首DWORD地址。该条件首次真实像素读取属于未初始化语义U；当前零底仅在该边界以后为H，不能说原像素应为零或把整个Load逻辑／数据判U。

若后层已将某孔强写，则从该writer起为V；有证同live历史、Town11完整页／zoom及stage32正常开场也不套此fresh分类。原来源和当前共享writer关系允许逐写归纳全部已定义像素，不需要新建taint状态、重放所有窗口或模拟任意旧heap。某份正常Save／camera轨迹是否实际抵达该条件仍按F3／F6独立自然门，不把本静态例叫新原生复现。首静态尝试误用64000布局解析153216原payload而失败保留，修实际layout后新静态通过，无新decoder／C／GUI。

### 2.66 名称的完整BYTE包含域与credits描述残余

`opaque-text-byte-domain01/report01.md` SHA256 `2b2184377f7bea0579b934d4fb8013fff098c7f3fa1db93d734e7e3a3e95fe1f`、`result01.json` SHA256 `9fa26cbd838af0d9fb13551c1cdf129af3403089954dffd18be4e371a27bb3d1`独立核固定FDTXT及九完整strict函数，实际静态输入前后同。原MOVZX BYTE后DWORD加法给出姓名1..256、race140..395、profile150..405，共768个映射全部plain，最长5／4／4字；现summary长度7及详情plain门对完整BYTE超集足够，结合已核字模／几何可扩组合资格，不改136／1179等旧实际执行数量。构造literal并不恰为全部32值：首脚本误断集合相等而失败，保原件，修为28个literal均包含于0..31的正确断言。

credits描述另为BYTE(text_id+12)，`15fb9`实际MOVSX目录WORD。FDTXT31完整6756B内的256个目录WORD本身都有producer，不能用46个逻辑fragment作allocation边界。新静态首分支分账为57个glyph／换行后END（43逻辑内＋14物理别名）、21个首token离源producer、112个首glyph离font producer、66个其它原控制待继续解释；不是自然可达数，也不把后两族笼统判U。text52→index64→signed offset1136是一个有源8字／换行流而现逻辑getter拒绝的条件例，正常producer／有效旧Save范围与真实caller控制合同仍在处理；未据此宣布自然玩法错误，也未给Load加text_id<=31门。

### 2.67 SHORT真实实参与当前C修复

`F:/fd2sdl-evidence/f4-short-counter-args-candidate-20261116-02/report.md` SHA256 `614545ef2e28806dfece1889f1993bfe1ee2d67e02f848a9b6df4a7cd017c9c9`以原17函数来源、六完整函数／共享尾及真实编码反证旧“缺第四参数／未写local”解释。设caller压三参后的callee入口SP为S；saveEBX、再push descriptor后SP=S−8，`1e6a8 ff742414`实际取S+12第三参target，`1e6fc ff742410`取S+8第二参actor。全部中间push／ADD与尾11d2c平衡，RET前回S，不读caller局部未写区。`sp-track-checks.json` SHA256 `c0f190c58926778372cbbe429859a0f02a6d2e8206618ad9142d4465c37c728c`及`source-checks.json` SHA256 `20e38f5acbf426db8f0c86b0a77417fad61ec624ae9d8e97faf71d6616d951e7`由纯静态检查实际生成，无新原CPU／IDA／GUI。

因此旧C有两项真实正常绘制差异：省掉target比例条，第二actor面板又使用target HP。采用两文件`candidate.patch` SHA256 `711323155bb75aab833623f8624a3f171a510b3281907981418b10ea92ddc3ad`，恢复原target背景→55→target比例条→actor背景→actor比例条→同App投影；两个target条之间不重画背景。复用现线性列／work桥，不增owner／RNG／业务状态。HP0不除，正HP/max0在实际比例点返回，首owner错误保前缀。

新增30个完整App／VGA／board adapter例，含两对象不同比例、HP0、WORD32768／65535、重叠descriptor、跨视口但页内、两处除零／首owner、HOST与runtime一字节错误。expected由独立原RAW列、已资格面板mask、原立即数和显式before构造，不由actual反生；高WORD／合成重叠不冒自然producer。根App／Battle／AICommit构建成功，`short-counter-real-args-ctest01.log`两项通过1.97s。当前App SHA256 `fe6a6db10e825f54fb49477329526a4485c4aa04bf14d6b65c25664aa7ac5225`仅为这次构建身份，尚未冻结本阶段最终候选；新普通SHORT反击反馈代表仍待执行。旧错误注释／收据原件保归档，现规范及命名输入已更正，不再保“第四值H省条”作为有效终态差异。

### 2.68 credits正常描述producer与物理目录的边界

`build/f4/credits-normal-producer02/report.md` SHA256 `1f027cf03189c881e3413c7616f09661c1276d98e43a9b6e7f53826ba43296da`及`source-check.json` SHA256 `f5fdbc2e092987419f909d69cf5b8468b274eebc4d7a5da146ae07514e0c8d78`把上节的条件输入与正常人物来源分开。四个完整Town根及保守callee闭包内，service借用人物表期间没有`10c50`或`2548c`的raw8写入口；胜利28的`2548c`持有自己的活动表，不别名到Town借用表。五处count writer、`112a5`的literal初始化、`11506`整80B复制的相等raw8前提，以及service／重排／转职的保留写集，支持正常START及另有独立来源资格的Save归纳：人物描述text_id在0..31，因此credits目录在12..43。当前43个typed描述和已实际通过的3592页history足以覆盖这一来源域，不必为尚未证明可达的目录控制另造文字引擎，也不新增Load的text_id限制。

完整BYTE超集的66个其它控制仍不是未定义字节：36个PAGE、18个speaker、12个portrait。原slow0也不能忽略PAGE等待及后继slow1；speaker可改目标页，portrait／ACK与关闭合同须按实际reader解释。如果独立有效旧档或新的正常producer能产生text52等值，必须重开对应有限parser／目标页／span／origin／slow资格；未证外部档不直接判坏档或U。本次只读来源闭包，未运行新C／原版／GUI；接口owner闭包含人工语义合同，不冒称自动通用alias证明。

### 2.69 FULL整数族与六个完整历史

`full-magic-family-join04/report.md`原SHA256 `1874bd7214a823a4bb57184fcba6cc588ae0711f676af7d7168c6a0149d3616e`及新`corrected-report02.md` SHA256 `d1055d95121c8d8e46ea5ba56c12e7090144e7dfdc7cb86d22a0f8512eb8754c`分开保留。独立整数递推核27个旧原始向量，不含ID6的两个浮点向量；38,773个数学事件不是渲染帧或新原CPU执行。十入口／19个selector／19包371part按原phase、layer、HP后继帧、9tick多目标过渡及返回尾组合。该报告当时的ID6有限数值前提现由2.72闭合，不回写旧哈希；FULL9 part31后的H、自然producer及新App回图仍各按实际域登记。

新增六个给定输入历史为0s2／2target、2s0／2target、4s2／2target、7s0／2target、8s2／3target及9s0／1target，共1,919上传／1,943个455,552B完整协议包。expected由完整原caller、整数递推和保留原pixel／mask生成，分段FX WORD tape及jitter是显式独立输入，不用C recording生成期望。活跃work／board／VGA／palette的像素比较与dormant App宿主守卫必须分角色：原30092已释放旧地图App，30dba才发布新App；演出期间该153216B列不代表原同期活页，返回后的fixture输入也不提供原fresh像素来源。FINISH后的work／board标为最后live快照，不读取释放owner。候选02按LF审阅；Git按本机规则落为CRLF，首post-apply物理SHA断言失败发生在成功应用后、构建前，`full-history-applied02.json`另核六file的真实物理SHA及归一正文完全相同，不回签旧脚本身份。

`f4-full-magic-history-C-20261116-01`首target帧byte67528 actual38／expected129，整批仍为失败／0完整case；全部2,230来源守卫真。独立原mask与两个原MOV证明是oracle错误借物理shake表，FULL应为5269c `[6,4,2,0]`、526ac `[-3,-2,-1,0]`，C正确。只修两个工具并加该点反例；新expected03 SHA256 `fb61e0bc4de3e890d8ec227f9719f19ac29160db80db0ced5136f967205b90c1`保实际生成器及全部源／资源前后守卫。生产C和PE未变、不重建；新producer02收据SHA256 `b69b4f6a4a3ca71627e3ab50d5175ada7874279e402df068a6d2ab24d7dcaf93`对应同PE `e5a53c8ba9b360a9c1ce1714ff667afb26d345c15a2b74f2cde80a79b5787282`。实际pair02报告SHA256 `2afdb1a84c6ec6e8ecfb8d419bbc936c7b856f24f97e5831e5495ad6da068b11`六case／1,919上传、885,137,536B全部通过；`full-history-build-join02.json` SHA256 `4a79b962e570dd835cd59d8cf7b0fdb754e1d2cf8a23dba76d9d5d352eddcdb9`全部2,232守卫真，进程exit0且结束。没有新原CPU／GUI，不称自然技能来源、首次work／App、完整共享RNG或真实时钟已合签。

### 2.70 MAP圆环／地震完整wrapper

`F:/fd2sdl-evidence/f4-map-wrapper-family-candidate-20261116-04/source-join.json` SHA256 `8e7c1d130cb7715f43749d6e14d85e69c94c37e8c2c1830993cec4f0d6a006f6`核23个完整函数及实际SP、三原表。修复五项真实差异：三个quake快照保进入VGA的4,096B边框；cue13先于copy；Magic11／12在圆环后补完整60帧quake；共用圆环以传入actor为中心、y偏移18；三组缩放原参改为`(128,128,131)`／`(0,0,128)`／`(-128,128,125)`。旧72首项来自伪C局部标签错位，不是原表。

普通Magic10／11／12都进入`21548`，其中`215a2→1ca89`仅扣MP，没有`1c4cc`或LMI/style段。11／12先cue2及十帧`2189a`，然后才进入共享体扣费／缩放／震屏。沿既有exchange追加窄同步prelude，让Game在radial返回后、quake CAST前独占扣费；Battle只读业务状态。Item／DERIVED／COMPOSITE的core11保独立路径，不因编号相同追加quake。AI射线复用同actor圆环；Scene既有`+18`不改。Item23两正式入口、popup四槽flat顺序及不可见零槽的组合证明另见同候选README，不能把MAP98反签为执行过popup／teleport／quake。

`map-wrapper-build01.log`五目标成功。首直接批次GameMagic、UnitRender、AICommit通过；新增Battle回归误把跨测试累计`input_pumps`当零起点，原失败保留。只在该新例记录`old_pumps`并比较本次增量，不改600／60／0ms及任何产品期望；Battle重建及单项复跑通过1.91s，其余三项未重复。新增独立三幅缩放整VGA／page0、非零外框、十个正常／错误／HOST wrapper例及35条Game路由的MP／RNG seam分账；原资源、自然before、绝对设备时序和QK联合未因此扩签。

### 2.71 SHORT正常窗口代表及工具首错

`f4-short-counter-native-20261116-01`原版持续WGC取得248帧后，严格双HP条门超时，未补键、未开SDL或保存。离线逐原RAW／DAC核明82..88帧已有63／3两条，首稿工具漏了`11d2c`的VGA目的`(4,4)`。原`1e611`传App+8088h／456／312／192，尾投影到A0504h／320，因此位置应为target`(87,100)`、actor`(111,60)`。按源改仿射及独立反例，13项纯测试通过；不更改C、采集预算、输入序列、颜色容差或完整355px／条判据。旧失败／首稿／来源报告均保留。

新`f4-short-counter-native-20261116-02/comparison.json` SHA256 `722b03541442693fa3f26db73c3b918064903d8fe7f05b04c33be66bc2982282`实际原版→SDL通过。两端helper在唯一END肯定前ready，各以同一完整帧严格命中两条全部五行、端帽及空尾，共710px，RGB容差0；关闭series各保84／87帧与完整原PNG。随后两端各走31个资源页面、其中30次requires_key确认、turn2及一次正式UI Save；不执行WAIT／Load／第二Save。全部22,987B及raw+2首次ID重建合同通过，两份密文Save逐byte相同，SHA256 `3597745359378c214244c3e9e96ad6b49c4cbf80d3bc077640435ec6fb6e040c`。

producer02收据SHA256 `b9b208cb610a4d7af8e6e1cf25c71f44c26ddbb09200c9051966970e1f31f135`绑定实际App `2d8cb13a97dc4f8891d072fa8d08ec9b24b1c8f7492e394e56ef6a547f973f6e`及2,273来源／库／tlog／快照；host收据SHA256 `e9d1df6d469da8dd1b63dc01f6c71933e0499563b1554281a5121a48f8c29fc2`、outer收据SHA256 `713393947f77233b783f65941018b70bcef310fa4b355593757b099315a1eeaa`均通过。固定bootstrap、实际有序conf／quiet argv与参照前后核同；原资源／用户档不改。两次运行的owned进程／helper正常exit0且无强制残留，显示要求均由原线程释放，未改电源计划或系统混音器。01曾按授权一次idle-reset将OFF恢复ON并留收据，不把唤醒本身当采集正证。这里仅签该正常反馈及有限业务／存档，不把条ROI、全帧hash或页面识别称整动画像素、PCM、自然获取、完整RNG或绝对PIT等价。

### 2.72 ID6固定参照的有限数值测量

独立有理模型已有五角／80个side×radius坐标，但通用host数学库没有可直接借用的全域误差保证。固定DOSBox PE的imports／double-FPU字串只作身份与编译分支线索，未拿它们代替测量。`normal-core-source-join01/report.md` SHA256 `dac3cc4dbfe833f2c28655f536772eecdc8ce9a90bfbcfe7814be6676e3e9ca4`连接官方固定`b6abbd5`的16／32bit D9→相同FPU数学叶；代码位宽不另选后端。memory float／double／control-word宽度另外核，不把寄存器分派关系当所有ABI均相同。

原完整初始化及环境来源05、07分别为SHA256 `43a180a56b54a84cb2ea00002dc0f31e6788914a3c1533c40717966016b51430`、`10ff4d501bcdc55b008ca99c910916f0c70cf3b855ddbf2018ff6a38fa6ae26a`：WORD5391c初始127f，正常init／emulator经FINIT后加载该字；全部76,605个既有LE code head含WAIT前缀的环境指令库存为14项／六个完整函数，无ownerless。CHP只在FRNDINT期间暂改高byte1f再恢复；另有注册的FSAVE／FRSTOR叶。直接xref无写者不扩成任意computed-alias或外来FPU context证明，正常ABI／有效保存恢复仍分别成立。所有查询只读隔离PUB02副本，不保存类型或DB，不运行原程序。

自有普通DOS数值程序不加载FD2，按原算序逐CW执行FILD(72i)→FMUL binary32→FSTP qword、两种三角分支的qword返回，再按WORD半径、base90／30、double1.2／30及CHP／FISTP输出。03为主组127f＋十二参数扫描，共65个角、260个trig返回及2,080对坐标。01手抄角bit错误在执行前被静态拒绝；02固定角不能覆盖PC24的真实角生成；两者均未运行且原件保留。03的NASM来源、编译输入及19,848B COM另有冻结身份，未复制或修改游戏代码／内存。

`f4-reference-math-numeric-run-20261116-01/report.json` SHA256 `66f5315a3ab5c5be823b4b0202b525e5a8bbebf6095cbbb50c8b221e03f77638`实际通过：固定reference／bootstrap／有序CLI、guest ERRORLEVEL0、host exit0、owned正常清理、全部来源晚守卫及同线程显示需求释放。输出19,300B SHA256 `d5215df76d6d13b18aa5f3c990411a6f19ecd6e8e3d36e0589f4058074e9352c`；parser报告SHA256 `52e6524d7430742091030a06479255ea0b36a572457eebb60478f55716325f6f`主组的实际CW、角生成及逐运算有理模型通过，其它扫描组有失配并如实保留，不强求所有CW相同，也不称它们均为正常玩法。

独立accuracy报告SHA256 `e38b7ecd1151705a84508cf79dd27c607e3a4668a7a94c75ce73ba8710f92509`重新以有理Taylor界推80个期望，核同旧margin／原两摘要约束的源模型，而不是拿实测trig生成坐标expected；主组两个分支160对实测坐标全部相同，20个trig逐固定点误差在`2^-20`前提内，基本运算在`2^-15`reserve内。这里的界是推导整数截断的充分条件，不是允许像素误差或任意连续输入／provider的uniform bound；旧逐点原trace遗失的边界不改签。

当前C另重建并实际通过`magic_fx`的29个保留原kernel事件向量（含ID6两个摘要），0.04s；59项真实编译／链接／库／PE／源晚守卫全同，收据SHA256 `b09e900a7c845d3d100f8df955a7103f52aad94b2201c8ed8d352ed0eeee81b7`，test PE为`0ba5031f3481060b23967401bd024c4f5de9e3bfcc8e3c35084a38893c1ca42b`。C算法未改，只补有限参照注释；本次数值程序不是FD2运行、原游戏画面／音频／绝对时序或自然caster证据。ID6有效像素前缀与已定位首越work点仍须按完整caller及真实mask相接，不以任一单层结果独自签整动作。

补充的`orbit-final-finite-join01.json` SHA256 `913417cc9cab839de78e12bca58417da4bcc01e4041ed10c7f7bed64caaa7830`已把两分支数值、完整caller及真实mask接合：side2／entry32在首目标frame7／phase5的part9首store为work−422，其前97次FX draw／117525个mask store均在owner内。该frame后置HP／jitter及upload在此边界之后，不能继续签原像素。C的有名H为越owner store有界省写，不是自动ERROR／HOST，也不据此拒整ID6。

side0／entry33的完整给定输入合成可继承FULL共享机制：handler之外没有ID6专用copy／layer／summary／投影／palette尾。`orbit-compositor-induction02.json` SHA256 `4cc03be8548884b94af60ff4318b22f41cbdc89f8d91d246a42733fce919069d`核一次初态与五个后续age／spark周期；12+9tick使目标入口每次前进一类，覆盖任意合法caller目标数，不从三target样本外推。140几何展开为132唯一tuple，两origin全部header线性范围11146..168156落在172800B内；结合独立原mask、160数值／80坐标、当前原向量和六例共用合成，可逐state／frame／target归纳side0演出像素转换。首target impact11、后target12而HP分母仍5，九tick不写HP／取jitter。尚无ID6 side0当前PE完整history实际包，不把此组合证明改记成一次执行；fresh回图独立见2.74。

### 2.73 地震族HOST与已达业务前缀

完整wrapper复审发现一个实际宿主差异：prelude／quake检测到关闭窗口后只在Battle锁存HOST并返回0，Game仍扣费、处理后续目标及AI尾。只给普通MAP Magic10..12的PRELUDE／CAST／TARGET明定0继续或skip、+1 HOST、负值真实ERROR；其它正值保错误。Game唯一锁存HOST，当前目标已提交则保其HP／贡献／RNG，下一目标不读不写；FINISH仅正常清理，不冒action_failed或继续业务尾。AI direct／behavior11两个必要同栈出口同步截住loot、清贡献、acted及物理后段。真实清理错误仍保ERROR及HOST锁，不再FINISH第二次。

八文件候选`f4-c11-host-candidate-20261116-05/candidate.patch` SHA256 `f22487bad329b28581dde4bb97eaa92e14c05259f2a75dceb745f7d229b75039`严格应用；before目录是LF正文，首物理比较在应用前拒绝，随后用收据的真实物理SHA／size／mtime及归一正文双守卫应用，不覆盖其它改动。App及三测试构建成功，新增75个player／public AI／Battle HOST、skip、真实负错、+2错误及清理错误向量所在的三项直接CTest全部通过4.16s。费用18／45／80和伤害72／144／306取原表／算式，关系快照只证明停止后的无额外写集，不自证伤害。它们是H扩展的C合同，不称原DOS有SDL关闭窗口语义；新正常Magic12反馈代表另行验证。

### 2.74 演出返回的fresh App与宿主休眠页

`fresh-action-return-source03.json` SHA256 `66a57ef41eca4978ec4978a4f12c8bba3d68e1cb0b33f962fecf87a95a97fa34`只读复核八个完整strict函数，四族正常尾均释放旧App后新分配153216B；不是将旧地图owner重新激活。以下地址均为canonical VA：

| 正常caller | free旧53A49 | 新malloc参数／发布53A49 | 首11cac(1) |
| --- | --- | --- | --- |
| FULL物理，54133=0 | 2e341→2e347 | 2e8a7／2e8b4 | 2e906 |
| FULL法术 | 3008c→30092 | 30dad／30dba | 30e0d |
| DERIVED | 2cfa7→2cfad | 2d78f／2d79c | 2d7f5 |
| COMPOSITE，MAP组前 | 2d842→2d848 | 2dcdd／2dcea | 2dd51 |
| AI射线演出 | 2e003 | 2e21c／2e224 | 2e272 |

第五个AI入口另由`final-binding-review01.md` SHA256 `78acae8f9d27ffecb75110c7098571bc545effd8fba2ea58212c2df1fb2a5112`补核：source09的完整`2dfc8`共744B／208指令，chunk SHA256 `62df84601dad51dd1b31178801fbec66baccdc38d45852d3924756913458aa70`逐byte等strict。它先新分配App，再重载FDSHAP、等6tick、清VGA和首`11cac(1)`，适用下列同一来源分类；不把八函数／四族source03改签为已包含此新增复核。

FDSHAP重载写独立53A5D；完整23head的`2e95b`仅malloc／fread到53A61角色帧缓存。两者不向新App运输旧页。等待、fade及对A0000的64000B clear也不初始化App；COMPOSITE的41次渐变在首11cac之后，不能倒作此前producer。这里沿正常不重叠资源／heap owner合同核目标，不以单个global无xref代替全部alias证明。原caller临时board／work与VGA为演出中的live页；C保留的dormant field_page只签宿主误写守卫。包中App列、回图fixture的显式底和原新owner像素资格必须分开，旧before的bytes相同不等于原有运输。

首回图源序已足以逐域分类：`1297d`更新标量相位，`11eee`先BG后base，继而`122dc`选中层、`127a9`逐actor／FG、`1acf3`信息板，最后`11d36→11eb0`逐行投影。raw1只跳water，不跳这些writer。返回前缀不调用10652／1088d重建BG；已初始化且仍live的BG是独立合法来源，可以先写新App，不能与已free旧地图App混为一个owner。

| 新App的首消费条件 | 当前已签范围／边界 |
| --- | --- |
| 33地图／43相位族中除None11／32外的既有投影域 | 复用原mask／BG覆盖账：base独自强写，或已获初始化资格的live BG先覆盖，再绘base；首viewport读取像素均有producer，无须原malloc零底。wave若早已在首work同tick处发生U，则不能把后续返回倒签为无早界；有效旧BG则不因新App重新变fresh |
| None11／32且raw3=FF | `4e22a`的C0仅skip，stride2孔也不读旧目的。先扣除真实actor／FG／UI覆写；孔仍存活才在实际像素reader到达首U。不能把pre-actor孔或malloc本身判U |
| 对应格raw3!=FF，base经4e016 | C0会读LUT[旧目的]；若BG或本次更早writer尚未定义该点，可在此读先达U，后层覆盖不能消除已发生的读取。marks须按真实caller绑定，不能套Load的FF前提 |
| stage11 camera0、raw3FF及既有36孔后层条件 | 函数级首未有producer字节为App(127,81)，offset37063，投影row9／offset55。此几何继承2.65独立原mask；只有返回时camera／marks及actor／FG／info同属该条件族才适用，不能把Load的roster条件或旧Town像素机械搬到FULL返回 |
| 此前未遇RMW等首边界，后层已强写孔，或相应camera本来无孔 | 从该writer起为V；不倒销更早已发生的U。stage32还保2.64的正式caller／开场域资格，不由几何超集宣称正常FULL可达 |

以上关闭了「必须先知道全部旧heap初值才可判断」的泛化R：源前缀、已有43族覆盖及具体首consumer已可分类。当前尚需联合的是实际caller返回时的camera／marks／后层足迹、wave原clock条件，以及相应正常producer是否到达条件，不是重新证明整个绘图族。新owner尚未读取的margin／padding既不因此为V，也不预先判U；后继移动或菜单按其真实reader／writer继续归纳。真正首U之后，C零底或保留旧App都只能是各自披露的有界H，不能以旧Town／旧seed补新owner来源。完整保存、共享RNG／绝对时间及全计划自然链仍接F3／F6既有联合门，本节无新C／原CPU／GUI或大mask重跑。

### 2.75 唯一地震冷表与来源发布收尾

三组`(x,y,step)`移入`data/static/tables/field_game_effects.json`的唯一`k_field_quake_parameters`，manifest约束为`int32_t[3][3]`，Game仅消费构建期生成表。独立校验从原EXE的52096／520a2／520ae三个DWORD列转置核九值，拒旧72及误按行解释；四项实际通过0.167s，日志SHA256 `3d684e57365392c7563458fe958689ccdb86e62b70b1cc7303c65e628c117e78`。App／UnitRender重建通过，UnitRender单项通过3.11s；这是新增冷转换的资格，不重算资源span或旧reader输出数。

来源发布PUB04清理八段已被真实SHORT／S5自然栈证据取代的托管注释，保存14个函数依据、零更名；1312函数的原字节、边界、chunks、flags及持久类型不变，反编译不保存，程序token相同。收据`build/f4/ida-presentation-publication04/publication-receipt.json` SHA256 `062c2daf705ce7d264820bff26da81d01031c34583d308288dcacd5cd5e2bdf8`；538项规范登记及11项SHA逐项检查通过。PUB03误driver的开库前拒绝、长重复注释被截断的失败均保留；没有发布缺字版本，详见[发布记录](../tools/ida.md#f4-共享呈现元数据2026-11-16)。

源码地址检查首轮拒绝把内部2c67d标作函数入口，改注释指向完整2b996后，293个C／H的998标记均为规范入口，发布回归14项通过0.433s。此后测试夹具若再改，最终源码快照须按实际新集合补记，不能把本轮身份改签为未来候选。最新App仅因这处注释重编译成功，SHA256 `5a3bc0e8b5149f4bf36406e39ffbff75d6b22e3756a95276458c6ade1807b8ad`；没有因新PE重跑旧GUI。

### 2.76 Magic12首轮原生协议失败与直接回归

`f4-magic12-quake-native-20261116-01/report.json` SHA256 `56c2520e9bea566456f31913c79e4ab82f6089d1874de3f6ba259ef34990043d`保失败：原版PID15756沿真实能力列表选12后进入mode4，Esc已返回能力列表，工具却等命令菜单，故在取消检查点超时。没有施法series、Save或SDL运行。完整`115b6／1cff0／18d8c`表明mode4仍真读确认键，取消返回0后`19017／19019`重开`1cff0`并在`1d0be`重置列表index0；不是列表确认后立即施法。保存的timeout页仍为MP341，和源取消合同一致。修订必须正见index0，再三次DOWN选择12，不靠额外试键或扩大预算。

该轮producer实际冻结568个CL／link输入、22库、2287文件，收据SHA256 `56ffd5577f3c10a0782b837f1eaa89fa99a684b6cb78e715756115d49adac017`。外层收据SHA256 `7d65cf924f9ad23219449758489a7c9b868f51e40f34c18497a87430ca10835b`：一次授权idle reset使OFF→ON，指针不移动；原版及owned Job正常exit0、无强制残留，来源／原件／参照晚守卫真，显示需求同线程释放，未改电源计划／混音器。首错不推SDL功能差异，后续协议和新运行须另记身份。

阶段收尾按实际修改的测试TU选择33目标／34项，`final-direct01.json` SHA256 `5bb609838ebc503f90ffc1a159e84226979f04e6cdbdb87e7b97123217af3934`保32通过、两失败／39.40s。`scene_transition`的旧替身漏计地图帧：现`scene_present_copy`显式上传，完整前缀的65次fade之外还含原地图上传；fast为一次最终上传。只修测试观察合同，原65级／2ms／全部palette trace不变，单项重建复跑通过1.69s。`s5_stage_source`的移动前缀失败仍在定向调查，32项不重复；本批不是原版、GUI或全历史回归。

### 2.77 最后三行的组合证明与测试夹具收口

`final-binding-review01.md`及机器账（SHA256 `9ad576bbdf7632e212b7dffa20eff15d82fd4a8a95ad8e2f6e67fa7723de736a`）按35份输入及28个实际函数体核C05／C08／C09。6-bit palette、fade／water／ANI的三owner按逐写窗口和真实caller顺序组合；Item20行、三override、teleport四段与唯一位置提交、popup四槽及AI ray分别接已验叶。转职七冷节点只向同一scope传原phase／value，无中间绘制／RNG／clock，故31条完整七段加一条首U前缀的32来源参数可以按节点归纳；这是静态正式绑定证明，不冒称新增coldwalker执行。三行未发现新的产品机制缺件，具体自然输入／camera／marks／RNG／clock与设备仍属原联合门。

追加复核AI第五个fresh入口后，纠正Battle注释中过强的“旧page/cache等值保留”，并同步规范输入；仅注释变更，没有更换page／热状态或原heap模拟。PUB05只保存该一个函数依据、零更名，1312函数类型／边界／字节及程序token不变，538登记和11项SHA通过；14项发布回归通过0.466s。当前App注释重编译为`5bad3598062d6c666106c23931187d17e8c0e109c285ff27a20875d76b83bd64`，旧原生／整history PE身份不改签。

`s5-prefix-fixture-review03.md` SHA256 `d70b63deab87d45e4f5db0958bee94655d5c3fe395cc13b6f728b8d423cab365`解释34项批次中的另一失败：真实stage21地图为45×50／9004B，旧人工raw32置(20,50)，对白聚焦后FG可见门仍进入该格，线性index2270超过2250cell owner；当前reader正确停在script65／66之前。只保前16受控部署，其余换成真实group0的50条／可选group1的6条placement，和既有F1原before的tail来源相同。独立16个最终xy、raw72／tail／count／camera及错误／HOST断言均未改，单项构建复跑通过8.40s。诊断中“重复text24抢首raw1”推测已撤回，真正说话人为原text21；不保错误推测作依据。

### 2.78 固定普通启动的两个provider与map125

`F:/fd2sdl-evidence/f4-a12-ah30-contract-review-20261116-04/report.md` SHA256 `8c94a76bfde1199f1b00e3d89fa6f9b3851554a0fa62ff54654493329d5ab4ba`和`checks.json` SHA256 `02f1a23a569b06319d4a34ed98a4efe64e39dae7611e072e569451e11aa2f171`关闭两个精确未决条件。完整原CRT以EAX=00003000进入INT21，固定guest启动、普通mode2／3切换、完整frame运输及官方DOS AH30的写集共同证明正常返回high16=0／BL=0，故不能生成type9。四个multiplex注册者对1687的拒绝、VCPI两次DI0／4及非VM86门、初始guest环境与有序autoexec、可选hook的bit写集均有独立完整来源；不是从Windows宿主名或一次type1快照推断。

四个保留runtime raw view的592个已执行head／原byte／size门不变，另有明确未重定位的MZ文件视图。新Domain来源、11个必要官方源码git blob、七份API交叉及全部SHA／mtime晚守卫通过；新增原版运行／CPU／GUI／C执行均为0。系统82点在该合同内为78有限target＋2ABI续点＋2条件排除，旧未决收据不重写。外来guest Windows、TSR／DPMI／VCPI provider或改启动合同须重审，不签任意computed alias或全进程dead。

因此map6/125的43B raw mask／`4e795` reader在**固定普通有效域**归为source-unreferenced；不再列未知codec、正常漏功能或两个未决provider。没有把它签作原／C实际reader执行，也不为孤立`21db2`盲增三次RNG。F2三个首比较及完整输入／RNG、F3自然producer和其它F4／F5联合资格仍分别保门。

### 2.79 Magic12第二轮窗口采样与时间门修正

第二轮`F:/fd2sdl-evidence/f4-magic12-quake-native-20261116-02/report.json`保失败：取消后重新选12、正常确认施法和返回均到达，137幅WGC帧完整归还。Q2整内页在frame42／53、Q1在58与源RGB精确一致；Q0只出现上下行属于不同snapshot的混合画面，没有完整Q0正证。不得拼接两帧或将部分行一致记为整页通过。MISS浮字121个源写点在frame63相同，仍不足独立签整个反馈／结果。未发送Save及其它补键，也未运行SDL端。原版PID6892正常exit0，owned Job37964无残留、强杀或超时；参照、原件、来源晚守卫及同线程显示需求释放通过。

只读复核又发现两项工具资格缺口。按frame index截取会纳入按键前积压；新工具以同一QPC频率要求WGC `SystemRelativeTime`严格晚于完成tap后的上界，且取帧开始不早于该界。这仅证明采集在投递之后，不冒称原游戏已消费按键。原137记录的部分SRT晚于到达／复制时刻，原因尚未测明，不减固定毫秒纠正。原90个圆环窗口中两窗恰等于合法普通移动LUT2／3，已从witness集合剔除；余88窗须同时反证plain及全部23LUT。修订工具22项pure通过10.619s，没有重跑窗口或回签第二轮。

来源报告`build/f4/wgc-cast-time-review01.md` SHA256 `5d61ce1b181e8427e7e4a560cc223ceae7204af5a69a48ffaea1cc4b1ac16ac2`，对应JSON为`0e10bf8b6ddcd70f3c2fa6bd7aac6c9d3c4136ea86d97bacf8a8277a5234217e`。默认三整Q门继续保留严格失败／待补身份；新增时间规则不是旧运行的真实post-key资格。

### 2.80 原三次采样与当前C九份完整逻辑页

已有固定stage0合法隔离档与真实资源接完整原`1f558`四参及真实stack-check、memset和共享尾，不替换函数、stub、指令或I/O。原先预清零的24×24 tile cache按`1399c`完整解码合同和已签原8256个tile mask建立64-stride指针表；camera5,11的读取均在已初始化范围内。实际只执行Q0／Q1／Q2三次原sampler，再以原`21548`的copy／actor顺序及独立FDICON mask派生三个idle的九份逻辑VGA。它们不是九次原wrapper／原GUI执行。

原临时owner严格为64000B，映射后的89216B仅为不得写guard，不是可用153216B临时页。首qualification把guard长度写错的记录保留；更正02仅复核既有原件，不重复原执行。原报告01／02 SHA256分别为`ce1f27c69bcf7d041ba8d70957cc64028f6d79e111514bc820b87bc871f126fd`、`8527a15bc4aec06a04025e9ade2291c28059fd87cd905659b4875f5b5843220b`；`F:/fd2sdl-evidence/f4-quake-sampler-original-qualification-20261116-02/qualification.json`为`0609dc54627119229e8668ea134ea32143aa3284362477f73e97758c0ffa6141`。完整Domain chunks／heads逐byte等strict，私有库未保存类型或改权威来源。

当前真实UnitRender私有stdout入口从同一已加载plain取得snapshot并走正式Game import／`begin_earthquake`，不建窗口、不改clock、不上传／等待／保存。三idle×三Q的整64000B VGA（含4096B边框）、59904B App viewport及palette／DAC逐byte相同；Q0三份完整153216B现有App另核保留写集，Q1／2新临时App的无消费尾不冒称原fresh字节一致。实际资源loaded-buffer、Game／map、RNG零消费和允许的jitter两字段守卫通过。

实际报告`F:/fd2sdl-evidence/f4-quake-stage0-C-20261116-01/report.json` SHA256 `8fdeb6ad324b927aa534afbc8347772bac029f568fbe7e87a262c46ffcee81cb`，`passed=true`／九项／exit0／PID39696已退出。producer收据`F:/fd2sdl-evidence/f4-quake-stage0-producer-20261116-01/receipt.json`为`74b32ce6d5957c0a57f4277bcecbbed35e931441edcf5368366d633457e98493`，实际PE为`e3a4d68dcee627c5f445d098bb1a4b5ec76cbcd789f2c80fddcbc9fb40207805`。`build/f4/quake-stage0-build-join01.json`为`8960178ab7bfa6c5ce04296eb08546d4f39f594eed88ab48d5d1e55dae278f48`，561编译／链接输入、22库、2290文件前后全守卫真；最低来源集显式包含唯一地震冷表。八项协议负例、实际构建及默认UnitRender单项通过3.10s，未重跑34项／旧实机。

该证据补足了完整Q0及三个snapshot的逻辑像素合同，不替代缺失的WGC整Q0、60次自然提交、真实投递消费、MP中间时刻、PIC／音频或全共享RNG联合。后续原生代表还须纠正一个新确认的前提：施法后可见acted单位使原`19df7`禁用Save，不能期待无END直接UI Save；旧两轮从未到达此门，不记作保存通过。

### 2.81 新反馈协议与第三轮采集失败

候选05删除了原版不可达的无END Save尾：可见acted单位使`19df7`二级mask为`[0,1,0,0]`，对应资源37／41／42／45；正常取消应Esc回parent、再Esc回browse。新协议在唯一施法后，按独立popup确定HIT／MISS，再核目标格、F2的完整44×8 MP261、灰态及Save禁用；只保fixture字节／mtime，不称已量得完整运行态或22987B Save。默认`strict-whole`仍保三整页；显式`compound-feedback`只提供native反馈组件，必须另接原3／组合9／真实C9全页及producer资格。两方法旧失败均保原状态。

最终补丁SHA256 `27315039a5e1d56037fd7acb26ddafbddbea06fdf4dae08db8de141100dffcd0`，候选收据`build/f4/quake-compound-tool-candidate05/receipt02.json`为`6b199b3422fdeb3c0c9f0ad806a5744bc79c1b4446d706e14bd335fb67a5a1c7`。根工作树逐before物理字节／mtime、隔离产物及最终LF比对后应用，fresh24项pure通过18.856s；旧protocol夹具把多块selected底板叠在同页的失败和修订保留。旧`checked_cancel`的“后续fullSave验证”声明已删，只签可见结果及磁盘不变。

第三轮`F:/fd2sdl-evidence/f4-magic12-quake-native-20261117-01/report.json` SHA256 `a89e88ecd96d16f893b0231d22c736b9c3d6b875b3dc22ff519109dfae5c05cc`实际失败于持续采集完整门：helper在45s达到time_limit，父判图仍处理积压，不能以此冒充显式stop完整系列。原19260到达施法后灰态caster，但内层逐帧循环未检查35s deadline，处理约61s后仍接受真值。230原PNG／日志保留，不重签全反馈。没有F2／目标／Save禁用后缀或SDL运行。

该轮producer收据SHA256 `2b9fbd18285fad24fd8be7e818990cde1fc6704ab17dfafc114c63e827054f4d`，实际566编译／链接输入、22库、2291文件，App为5bad。outer收据SHA256 `fb6179a842a9e3ebdbc03b2162c8b29bfbdf000683f87e85a0b3f92c29e7d077`：显示原已ON，未唤醒或改电源／混音器；原进程exit0、Job5000残余0／强杀0，来源在同线程释放显示需求前及释放后均不变。采集helper也正常清理；失败是工具收口，不能推游戏显示错误。

### 2.82 等义识别优化、期限与第四轮真实结果

三张现存原PNG0／40／91的实际profile定位每帧833次`match_indexed`，66格×actor候选的Python逐像素比较占主要成本；不是`RoundOracle`构造。将同一公式改成Pillow的RGB绝对差→channel max→非零index mask histogram，保原透明0、容差3、全部四返回字段和66格唯一性，不缓存可变template或缩扫描范围；非整数ROI造成尺寸不同仍走旧zip。三张图全部返回字典逐项相同，profile由1.188／1.105／1.182s降到0.129／0.137／0.164s。这是识别耗时，非游戏时序。

两新回归覆盖透明0／3与4的边界／空mask／变更template，及P／L／RGB／RGBA／1／I16、多尺寸／整数与非整数位置对独立旧标量定义的等值。menu19、Load24、reward23、quake24直接pure分别通过0.686／1.701／10.213／18.874s。两种结果的caster输入完全相同，返回谓词合并一次；结果仍由popup与目标检查确定，不暗示MISS。`series_checkpoint`现在每帧前后查同一deadline及目标／helper存活，不能接受晚真值或死进程积压图；新增期限和late-death控制所在16项通过2.845s。未增采集预算、跳帧或放宽像素标准。独立六文件稳定复核见`native03-capture-backlog-review01/stable-source-review02.md/json`。

第四轮`F:/fd2sdl-evidence/f4-magic12-quake-native-20261117-02/report.json` SHA256 `af7d07b8caa1efb2d2890b2bdc1f5493f4bf055969be006d8794b45e8b8d105d`仍FAIL，但采集已恢复正常：100帧、8.906s、显式stop，posttap双门选26..99。ring32..36、Q0源局部39／40／44／45／47／48／51／55及HIT322 popup63为严格正证；wholeQ1只在64／68／72／76，已晚于popup，wholeQ2没有。不能把晚Q1当quake PC、插回前面的虚拟观察或拼成全页。正常灰态caster返回已见，组合门未过便停下，未运行目标／F2 MP261／Save禁用及SDL。

本轮producer SHA256 `109f81eb11965e7fdc2c262684410148f611cbd64e17328d95fd8c80749d6803`，同566／22／2291和实际5bad App；outer SHA256 `77d1c779d3804610e1f082c10460475103a5d72a59e0a26b7c399b03336651f1`。原4360正常exit0、Job10720无残余／强杀／超时，全部来源、原件、参照及显示需求恢复通过。诊断`build/f4/native04-feedback-diagnostic01.json`只分析本次原件，不重跑程序或把失败改签；两分支完整组件结果分别保留。

### 2.83 主体合证与明确后置的正常联合

独立`build/f4/quake-physical-method-review02.md/json` SHA256分别为`e0e593767afa4120214489ce6995a0bdc8009a56ecd18fa108dbf714ff126583`、`f20a4acc057034e3297d12a7653781217cc34814cbf6c2627da8846d3929d670`，给出如下区别：原3次sampler／独立组合9、真实Game C9全页、Battle正式60圈及共同320×200 upload，可组合关闭**给定有效before的软件显示机制**。Battle实际协议回归为72上传／9cue／600ms，snapshot内容是替身，故必须接真实C9，不能单独作像素oracle。当前45个src／cold与C9 producer相同；source-join收据SHA256 `b6a0b251ed571025b1ed7ec94fc2ec81595b6f4fac7f30e8935b9dbeea91f771`。没有重跑原sampler或C9。

源先定Q0／Q2局部窗还可作为反馈组件：12窗各有213..388点的RGB不属于同址任何已列其它Q、plain、23LUT或10ring，排除了这些竞争态逐像素拼接伪造。但本轮没有用新增Q2方法给PNG追绿；局部class不等于整物理页。旧37／59个非三Q像素的原因仍Unknown，没有原动态输入或扫描来源证明，不能称已解释纯tearing。原地震循环是**每copy后10ms**，2ms属于后续浮字帧，不混同两种时间合同。

停止同条件窗口采样，不再逐次减门追成功。主体按完整共享机制／资源及26行有界资格交付；C11正式正常before、当前候选双端目标／MP261／Save禁用后缀、完整END→Save和输入RNG／PIC联合移交第5节。该缺口不读取F5 timer／PCM／设备叶，不阻挡它们实现；A14—A16完整正常域仍未过，不以本次调整缩小最终目标。每个短暂逻辑页都获得whole-WGC不是总计划的额外强制门，未测物理性质仍如实列出，不将其暗写为通过。

## 3. 26行主体资格与最终联合门

下表是A14—A16内部工作行，不增设平行优先级，也不把26行作功能覆盖率。完整静态盘点及75输入身份在`build/f4/presentation-consumer-inventory01.{md,json}`；8冷域242 flows／22171 nodes／582实际使用操作是结构量，仍须逐呈现binding归类。

| 工作行 | 尚需补足的独立资格 |
|---|---|
| A14-C01 页／LUT／clip | 五writer／23LUT与66实际C、320／456线性owner、正式矩形／prefix／菜单／info／缩放桥及首错已有逐binding组合；无另一个确定待修页算子，最终源／PE影响合签待收尾，不套双底mask定理 |
| A14-C02 地形／动态背景 | 四kind源／33绑定合同、正向12整页及343共享BG／投影实际C已过；wave首次尚未填充且sameTick的条件首U／零底H后缀已按当前owner闭包分类，正常时刻及父联合仍另签 |
| A14-C03 actor／移动 | 左margin418及整BYTE direct1024／map action24／Battle48已接8direct／18batch／4FG／tile／三action caller与真实App owner、globalcursor和三idle支配路径；算子组合已闭，A09输入producer／camera及真实clock联合另签 |
| A14-C04 正式工作页 | normal0／1、tiles-only、menu各ID、summary body、focus／movement／Scene源序已组合；2.74另闭五个演出入口free→新App→首回图的来源分类。dormant C App只为宿主guard；31地图有条件读前写、None11／32的实际存活孔／RMW首读及wave前界分别登记，真实camera／marks／后层与自然时点联合另签 |
| A14-C05 palette／显示 | 正常6-bit／已绑定fade-water-ANI的base／TEMP／DAC组合已闭，203真实decoder点保原资格；真实provider／设备epoch／自然WGC最终联合另门 |
| A14-C06 对白 | 155请求／310整VGA的开合、marker、PAGE／scroll、mouth／早键／HOST和窗外恢复已过；动态全文、自然入口／真实反馈与时序联合另签 |
| A14-C07 字形桥 | 320／456线性owner、fg／shadow／bg及首错、832＋136＋212 typed及对白历史已组合；完整BYTE姓名／race／profile超集均plain且max5／4／4；正常credits描述producer闭包为0..31，外部独立有效档或新producer超出时须重开signedWORD目录／控制合同 |
| A14-C08 信息／列表 | UI混合reader、signed概要条完整分段／320与456 owner、详情先后／首错、列表滚动／重叠及stage24 raw17选择已有源与实际C组合；姓名族完整BYTE及正常credits人物来源已闭，外部档资格及首比较另门 |
| A14-C09 Round／阵营 | BYTE256×15及两kind×46旧页／DAC资格保留；新62history补原横幅等待中的完整App和HOST／错误前缀，真实Game背景／actor hook按C04及最终联合条件合签 |
| A14-C10 有限UI | 正式binding的开／等／关、early key／取消／错误／HOST、源wait／本地ticket、同page widget／酒店箭头及空EQT反馈已合签；F2首比较／输入provider和真实设备时序保最终联合门 |
| A14-C11 地图FX | 三族源tick／ms顺序及Magic10..12 quake／radial／MP seam、九逻辑页及正式60圈已组合；flash双端全页ABABA已过。native04只有原端局部反馈，原失败不改签；正常before、MP／目标／禁Save双端后缀及未解释非Q像素明确留F6，不阻F5 |
| A14-C12 转场／传送 | 8＋12绑定的参数来源已核，新增7组／198状态／399整owner块已过；旧top18／inactive资格保留，完整before、其它合法R及父联合仍另签 |
| A14-C13 cinema／credits | 40组montage及完整credits3592页／DAC、roll500／skip／epoch已过；自然shared RNG／结局入口和first owner联合另签 |
| A14-C14 标题／宿主显示 | 正式main的668页／DAC及导出RGB、536滚动／14阈值／40确认帧，加ANI真实decoder203点已过；自然插入时点及宿主WGC联合另签 |
| A15-C01 死亡 | 旧14组及D36／96四组完整FX已过；13旋转→flags1→base→十二FX的正常84／quiet3操作已逐原源和正式冷绑定组合，给定定义before接A14；自然高D／HP／QK及具体首owner条件仍另签 |
| A15-C02 FULL物理 | 48组正式split／layer／side／shake／tint整算子及工作页／VGA／palette已过；marker／hit／counter、board来源与自然推进仍合签，普通54133=0返回的新App另按2.74，不把旧宿主页作原fresh before |
| A15-C03 SHORT／恢复 | actor／panel／线性HP条／FX与恢复已组合；真实SP反证第四值H，30整owner通过；新原版／SDL正常END的严格63／3双条、31页／30ACK及一次完整Save通过，时间／RNG自然联合另门 |
| A15-C04 FULL magic | 九整数族／六history的1,919上传保原身份；ID6固定reference／CW127f数值及side0五周期／132几何已接共享逐层组合，非side0有效前缀止于首work−422。dormant App不计原同期活页；四族fresh回图按2.74，FULL9 part31后的H及自然caller／设备联合独立保门，不冒一次ID6完整history实际运行 |
| A15-C05 MAP | 98请求／1677上传及98返回全owner历史已过，含三actor样式／真实Item override；MP／效果／popup正式caller及真实背景／时序依原语义另签 |
| A15-C06 DERIVED | 五ID的23请求／3597上传及part／marker／两目标／双底协议历史已过；side2原wrapper与side0共享callee扩展分账。dormant App列仅宿主guard，新App按2.74逐读前写分类；临时work before、正常技能producer及父业务仍另签 |
| A15-C07 COMPOSITE | 四算法×两side×双底的16正常参数／2852上传及新三H控制252上传分候选通过，不改签一次新19全绿；演出中dormant App是宿主guard，2dd51先在fresh owner回图再接41渐变／MAP组，资格按2.74与唯一冷源序组合；自然producer／业务另门 |
| A15-C08 Item／AI射线 | 20行Item／三override、23两入口四段与唯一位置提交、popup四槽flat序、AI caller／mode6／后续Item已按完整源及当前consumer组合；第五fresh App回图接2.74，正常camera／marks／RNG／clock及设备仍另签 |
| A15-C09 转职 | 32来源参数／8262点为31条完整七段＋1条109点首U前缀；七冷节点同scope／phase／value已充分静态组合，不冒实际coldwalker或自然教堂取证。empty144首2ebb9之后省绘H不签原后继 |
| A15-C10 montage | 正式cold walker的20绑定×两RNG分支、3596命令／2779原整帧已实际join；自然入口、shared RNG／skip与整ending另签 |
| A16-C01 额外资源 | 16300唯一span及75／256／2093＋frame137实际C保身份；UI58正常未引／hotel五箭头已闭。map125为4e795 raw-mask reader，F4补闭两provider后归固定普通成功启动域source-unreferenced；非reader实跑或任意guest死代码 |
| A16-C02 文字 | 34 entry／1016 fragment／1824字模分母、1179 typed请求及新增30动态片段的524正式consumer请求已过；普通对白155历史保独立资格，跨全文自然producer／portrait phase／输入RNG联合另签 |

FULL9的新`build/f4/full9-resource-prefix-review04/report.json`及独立复审核明：part31目录仍在资源内，目录值30产生xy0／12546，原首RUN1 store在work+8048320，超过正常请求172800及allocator上界172811；part0..30编码源／目的extent另核。此结论不代初始像素、自然多目标、非side0或完整renderer；首违例后省绘仍披露为H。01／03的旧schema读取失败及02早期资格均保原件，不回签。

SHORT counter的旧“第四参数H”已被真实实参来源反证并修复；背景首次安全底按2.61／2.65的具体当前owner及首次消费条件分类，不凭malloc无清零一概判U。promotion empty已由真实allocation／完整writer闭包定位首2ebb9：越owner与本次资源目录无producer分别证明，不能仅据3B sentinel判OOB；后继省绘仍为H。其它新来源也须定位真实owner与第一处依赖。源注释／命名输入已按PUB05发布；最终C／H集合294文件／998个canonical入口，快照`source-address-snapshot-final04.json` SHA256 `39f41d3f5d50fbe37dc9a630953a39e561fabcc7a0c9c218ff0158c1abc83cde`。随后只有工具／文档改动，不倒签旧候选。QK、F2三个首比较及完整轨迹、F3后期R保原专项；两个固定普通provider已在2.78关闭，不继续当未知阻塞。

## 4. 实际检查及阶段完成门

本轮新增／直接影响CTest分别保候选：hidden批次4／4（13.28s）；空EQT Prepare通过；死亡批次3／3（6.76s）；reader两TU批次3／3（11.11s）。Python协议工具保旧失败与修订身份；最新资源工具18项通过，准备包装器12项通过。不同批次不拼成一次全历史绿色。

共享Probe补延迟输入和ANI容量后，`f4-tools-review01.log`真实尝试227项，225项通过、两项夹具错误：left_margin模块导入时无条件启动unittest，旧700行容量夹具用`object.__new__`却未初始化`deferred_inputs`。只修测试入口及该显式字段，`f4-tools-fixture-fix01.log`对应10＋13项通过；保旧错误，不把这两批改写成一次全部新通过。没有为夹具更改产品、oracle、原CPU或GUI。

标题、转场／传送新增参数、动作返回尾、30动态片段、credits完整history、FULL物理整算子、普通对白、palette／ANI生命周期和全部26行正式绑定的有界算子／组合资格均按上账收口。动作／owner独立复审32文件无新增确定缺陷或可安全删除的重复，报告`final-action-review02.md/json` SHA256分别为`561065ecf00793b9ab7b01665907f84542613fd4693b5abbb73f93145f5d0d9b`、`1c3242adb87045264bc2626dac6a04f03f1634fcf3c9d0db72ddbb997f5146f6`。深path、全正常producer及C11动态联合没有借此代签。

以完整主体一次提交和本地集成，不逐算子微提交；已有绿色项不无故重跑。主线重建、集成定向检查、物理文件／换行继承与用户SAV保护的实际结果均以两树`build/f4/subject-delivery-20261117/local-integration.json`为准。不存在的收据不作完成证据；提交前的旧PE／失败／分批测试身份保持。随后直接F5，不远程push或发布。

## 5. F6必须接回的完整域

1. 第3节各行列出的真实normal producer／首次before、camera／marks／actor／FG／mouth／water／cursor、合法外部SAV及fresh owner首次读取条件，与当前正式入口、输入RNG和硬件时钟合签。给定输入的全算子不是所有输入自然可达的证明。
2. A14-C11的当前双端正式施法→目标反馈→F2 MP261→禁Save／双Esc及正常END→完整Save。原四轮失败、单端正证与C9独立逻辑页不拼成一次新pair；未解释非Q点保Unknown，补足正常输入／调用与扫描来源，发现真实V域差异即修复。每个10ms页whole-WGC及全60物理present未测，不能写已测，也不以赌相同采样代替针对性质的证据。
3. FULL／DERIVED／COMPOSITE／ray fresh回图按真实camera／marks／后层及wave前界分类；旧dormant C App不能作原新owner。QK与后期R／Save的来源专项保持原完成条件，不能由其它早U遮蔽真正V域。
4. 当前集成候选统一join本阶段分候选的软件／原CPU／静态／原生资格，F5补非例外PCM／PIC／cue时刻及唯一clock影响，F6全面历史复审、修复精简与[最终逐A项门](development.md#f6统一全历史审查修复精简与最终验收)。上述任何必需项仍缺就不宣布全计划完成。
