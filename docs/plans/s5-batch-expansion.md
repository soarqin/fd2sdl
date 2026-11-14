# S5：隔离语义闭合与后续批量接入

**已完成既定范围。** 基线为 S4 `0cca15f`，本阶段仅扩展 stage20—29 的已证依赖闭包。唯一当前优先级见[总计划](development.md)。本阶段一次独立提交并集成本地主项目，不远程推送，不自动启动后续功能。

## 1. 交付域与支持门

两批共同取证、按依赖接入，不逐关拆阶段：

| 批次 | 本阶段交付 | 明确边界 |
|---|---|---|
| A：stage20—25 | 地图／编成／placement／触发／文本／移动、entry/result/victory/callback；正式战斗／END、结算准备和普通交接，含 entry26 | 保原参数、条件／分支／顺序／一次或重复语义；不以入场成功冒充自然整关 |
| B：stage26—29 | 26好结局到27 NoTown、27—29战斗／END／事件／交接；26坏结局与29终章的credits、特殊战斗及非返回终端 | 26坏臂热stage仍26；29热stage变30。内部resource30不是新战场，不新增普通stage30能力／entry绑定 |

stage0／1及stage2—19的既有完成域保持。已证0—29域的发布门已按本报告闭合；source可读、图存在和target去重均不能单独授权运行。默认turn255全资源450条有源拒绝保留：战场0—29为406条，资源30—32为44条；当前stage2—29范围内379条。stage31／32开场来源与30槽战场表分账，不扩签无限回合。

## 2. 来源闭合与唯一冷定义

| 原来源 | canonical VA | 共用能力 |
|---|---|---|
| entry22／24／26／27／29 | 336a0／3396a／33af1／33c9d／33e3c | 转场／page和mask、借用音频、相对焦点、teleport、初始化和移动 |
| victory20／21／22／23／26／28／29 | 240fa／244b6／24754／24c1e／250cc／2548c／25757 | 物品前缀条件、滚行／临时页、败亡、结果交接、credits与终章 |
| callback82及stage29 turn00000引用 | 362e8 | 原32byte事件状态、五参teleport、追加／状态分支及共享尾 |

原13槽／14引用均有完整来源和冷定义去向。source10含141个完整owner、20图／17002节点；这些是不同层级的身份统计。S1原33组、208分派槽／193目标、106移动流保留；原315条未决引用现315条来源图／既有原语可用，未知源去向0；450拒绝完整保留，来源ID并集778。不是315种独立机制或玩法通过率。

当前stage2—29有163绑定／149目标、112条callback来源引用。82另走严格分层门，不向通用ACK探针补伪造向量。callback51改为新`common.callback.051`：完整`355f0→35631 push3→3530d`为descriptor2／mode0／item101后story3；旧G的story11图及旧报告保原身份。

原DAT继续为唯一生产真值。冷图只编辑`data/static/`并构建生成，条件、分支、重复、等待／取消、写序、marker／HP和RNG调用均在图中。C只提供有限领域操作，沿Game／Scene／App owner及逐叶提交／失败前缀，不建VM、第二热状态或整体回滚。固定cinematic子调用只有ending／credits／card_normal／card_last／card_first／montage_round六种，参数域、调用者、深度和预算复位逐项限制。

## 3. 实现、修复与精简

- 复用现Scene walker，新增有限`scene_transition`、`scene_teleport`、`scene_cinematic`模块；普通战斗绘制只抽取原有相同draw／tick叶给片尾共用，不改原攻击提交规则。音频bank只借用，montage的原override使SFX在实际stop／play前受门控，不伪造clip PCM。
- `24618`四参保持相对屏格和radius／step；两个bank80 load不是stop／free。`22253`借实际ordinary_cursor，display0／1保持；缺cursor在取owner前拒绝。坐标极值防御不冒称原版越界等价。
- Scene保真实入口文本owner：victory22热stage变23仍读原FDTXT23，victory29明确重开31。统一preflight借用同一text view，删除按变化后stage重复开文本的旧检查及旧entry2重复预检。
- Game叶失败后同步已到写点，Scene不回滚已写HP／方向／身份。stage28第二次ROTATE_ACTOR ERROR／HOST保第一次写入，不运行后缀。所有unknown SAV／owner、RNG和容器合同不改。
- NoTown资格统一引用既有30byte flags，27—29不索引只有26行的普通城镇表；删除save模块重复表输入。部署人数仍读原metadata，内部resource30为31个active槽，不制造最少两人限制。
- 第26关真实胜利→27暴露旧NoTown冷图漏恢复DAC：原`26152`清屏后先`11d40(0,255,0)`，才notice75／text410。新增唯一`notown.normal`冷叶，退出darkness255保持。实际入口黑／白／normal DAC到HOST的三例回归先红后绿，原版／SDL整链复验通过。
- 正式片尾又暴露source等待错接旧common预检：原2000ms节点被旧`delay_ms<=1000`拒绝。source纯等待改由自身0..65535合同校验，不再投影成另一caller的资源操作；text／pan／move／refresh资源门与旧common界限不改。Play与测试用同一薄binder绑定原有preflight回调／fdother两字段，快／慢完整ending均经过真实生产预检；实际ANI0／2解码通过，未添加不存在的opcode或放宽尾部解析。
- 片尾按真实`31529→credits→20对montage`顺序执行。ANI0／2临时palette不改Scene base／DAC，ANI1明确拒绝借入此caller。RLE SKIP保像素、literal0不透明；最终59终态不提供正常返回端口，业务键不能代替HOST退出。

曾怀疑特殊战斗HP任意，仅因callee局部向量不足。完整自然caller已证明：令`P=2e2b0.entryESP−64`，最后一行memmove的`3771c push esi`写`P−112`，正是后续var4C；ESI为`0xAF8C0=719040`。原DWORD读及marker低WORD63680写分别保留，不采用固定一轮安全偏差。60个真实intro、40个连接caller及80个派生WORD向量分账；原240个poison向量及已撤回取舍留[调查归档](s5-investigation-history.md)。此值仅属于该20组非split caller，不能当通用HP默认值。

## 4. 五层测试与证据身份

所有完整证据留本地`build/s5/`索引及`F:/fd2sdl-evidence/s5-20261114/`，原始资源／转储／WGC不分发。不同候选不累加成最终一次全绿。

| 层 | 实际证据 | 适用范围／边界 |
|---|---|---|
| 独立原来源 | strict镜像逐chunk、完整CFG／xref／ABI；source10、自然栈CPU03、G1完整26152原字节 | 13槽及必要callee；未知CFG不补边，不按附近CALL猜语义 |
| 静态／全账 | 最终C22严格82／参数／scope／disposition／G4／六旧caller；7主报告235路径／225实际文件，含资源门229声明文件逐SHA复核；source10生成前快照保历史身份 | 163绑定、资源／身份／宽度／条件分支与拒绝账闭合；1873引用／154 archive entry不是运行覆盖率 |
| 原码／实际C | callback51原36／C39；五caller原180／C192；generic1998、147等待负例及1容量；正式绑定70条／67图的1890正常、147等待及1容量；82另有8／408有序前缀、5状态projection与真实Play桥 | 每份保冻结probe／源码、替身和前缀资格；不能用生成图作自身expected，不能签全部共享RNG或原版资源失败 |
| 新共享视觉 | C18原码PC驱动实际C的40组：3596命令、2779完整64000B索引帧逐byte相等；80RNG、2419等待、199受门SFX | 完整索引帧／HP／分支／RNG，不等于物理全帧、全RGB或声音；C14转场12／108、teleport8／408、19caller观察和250decoder页按来源资格保原身份 |
| 当前SDL组件／集成 | C22单次18项适用CTest全过，其中`s5_source_tools`124项；真实source26坏臂和29到终端、既有0／1活动快照门、2..29 entry／snapshot、合法1..29准备域、Play82、错误／HOST、旧战斗动画和NoTown直接影响回归 | 无窗口／I/O替身和正式输入分别登记；stage0／30无正式准备入口；不重跑无影响旧G全GUI，不签Linux／macOS |
| 正式原生代表 | 见下表；新运行、跨候选有限资格及各自排除明确分账 | WGC HWND、owned后台投递、串行正常清理、完整容器和原源守卫；不以API入口代正式可达 |

## 5. 正式输入代表与严格继承

| 收据 | 实际路径 | 状态与资格 |
|---|---|---|
| native-stage21-C17-03 | 标题LOAD→Town／entry21→选择取消→Save、Load取消／成功→受控nonplayer后真实END／turn2 | 旧双端通过；只继承逐源／绑定／ABI不变部分。旧标题两项／首次Load禁用不改签当前cap29。早期可复用临时Load快照已被后续同路径替代，不能声称现重新读验该早期原件 |
| native-stage24-C17-01 | NoTown真实选1..15→entry24／完整对白→震动／活动Save | 旧双端通过；118／110原WGC、3静态ROI观察40／16次源行变化。后半entry先完整initialize／fade屏障，可有限继承；C19新NoTown normal不倒签旧运行 |
| native-stage20-C18-02 | 六个真实count前缀物品→正式CONTINUE／选择取消→victory20 sheet101／完整story→append24,23→Town21／四手工槽 | 双端新运行通过；6个有序raw资源anchor `[1,35,68,69,85,100]`不降门。C19唯一NoTown增叶不在此Town21路径，资格另列，不改写为C19重跑 |
| native-stage26-C19-01 | 正式CONTINUE／真实player结果→victory26好臂→27 NoTown／真实选19人／强制text9→34active／24618入场→活动Save | 双端新运行通过；两端各79业务检查点。88／83总记录另含9／4采集瞬态，不计业务通过数 |
| 第29关最终片尾代表 | CONTINUE／胜利→resource30／FDTXT31→通知／credits skip当前卡保留→montage首末caption→完整59终态无补键／HOST | 原版`native-stage29-C20-02`仅original case完整通过（整report仍false）；SDL `native-stage29-C21-final01`仅SDL新运行通过。C22只删失败日志，经逐源码资格承接；不称C22一次新pair |
| 第26关坏臂正式SDL | CONTINUE／真实结果→坏臂story／FDTXT[27]的fragment17..23通知→共享credits／montage／终态 | `native-stage26-bad-C22-01` SDL-only新运行完整通过；原版共享ending代表、完整250cc坏臂原码／写集及32原CPU通知快照门复用，不称坏臂双端新运行 |
| 第29关正式入场 | 标题LOAD→NoTown真实选择→script87／story0..2／七次22253传送／脉冲→浏览及活动Save | `native-entry29-C22-01`完整双端新运行通过：各61业务检查点、4＋3两段WGC每caller一原LMI正证、最终27active整容器一致。旧G2另一renderer未代签新Scene；不声称捕全相位或自然从头通关 |

全部源注入均为登记受控副本，不冒称自然从头通关。蒙太奇原生仅观测首末caption、credits首卡／末卡和最终完整59；独立来源／冷序审查、实际Scene整图20次open集成（ANI／宿主I/O替身）及原PC驱动有限叶2779帧差分分别登记，后者不是完整cold walker的像素oracle。不声称物理捕全20组或31张卡。原波形／设备可听、完整像素、绝对硬件时序及其他平台不扩签。

82绑定的原turn00000记录为`ff5200`（turn255／callback82／phase0），不会在入场后turn1的首次player-post触发。保留原码／真实Play机制门与同helper的entry29原生代表两层证据；不改回合造“自然82”，也不扩签turn255整表。450条默认拒绝是另一组callback255默认记录，不能与已知82混为同一状态。

严格继承索引为本地`build/s5/native-finite-qualification-location.json`，最终`native-finite-to-C19-06/matrix.json`保10行有限资格：5旧C/H及两旧JSON完整SHA恢复、55旧cinematic函数token、11绑定和实际调用切片分别核对；close仅新增空指针尾清理另证。24／20连续WGC按owner／尺寸／时间／stop／原PNG重读。21最后second Load→Save和后续turn2原件仍逐SHA相等，可继承共享活动恢复／END门；两条早期临时原件已被后测替代的边界不抹去。24初始NoTown显示、旧标题／首次Load两项不继承；新26共享NoTown叶另列fresh，不能将10行称10场新运行。

## 6. 复跑与本地保护

Windows使用普通F盘构建目录，SDL3 3.2.12静态包、MSVC19.52；FileTracker与实际CL依赖已验证。CMake从4.4升级4.5造成旧模块root失效，记录失败并刷新该内部路径，未改产品或假报构建通过。旧S0缓存已逐文件SHA／mtime归档F盘并保原路径junction，只读历史证据不再增量构建。

```powershell
cmake --build F:/fd2sdl-work/s5-elegant-pasteur/build-win --config Debug --target fd2sdl fd2_scene_cinematic_test fd2_s5_stage_source_test fd2_field_prepare_play_test --parallel
ctest --test-dir F:/fd2sdl-work/s5-elegant-pasteur/build-win -C Debug -R '^(s5_.*|s3_stage_batch|field_prepare_play|field_prepare_flow_domain|field_save|battle_animation)$' --output-on-failure
python -B -m unittest discover -s tools/tests -p 'test_s5_*.py'
```

来源／矩阵、strict scope和原码命令见[冷定义工具S5](../tools/static-data.md#s5-扩展正式参数与独立-82-门)；正式原生新目录命令与受控状态见[图形工具S5](../tools/gui-testing.md)。输出必须新目录，完整原件只读；Linux命令保兼容说明，本轮未执行。

## 7. 审查、失败与归档

阶段内候选／早期计数和已撤回HP疑点见[研究过程归档](s5-investigation-history.md)，实际原件不覆盖。主要失败包括：旧callback51 story11误绑定、text owner误重开、Game错误前缀丢失、teleport缺cursor、标题／Load错域、20物品不在count前缀、20漏物理anchor、NoTown白DAC，以及29文字／肖像遮挡和continuation箭头坐标模型。各项先保失败，再修适用consumer或独立工具；修工具不降低判据、补键或伪造运行。

31bdf模型另作全族核对：29八通知／九等待页、26坏臂七通知／七等待页，各取两种未写背景seed，共32个原CPU快照。真实字模、portrait、UI、scroll、memmove／memset执行原指令；文件装载／输入等明确替身。逐已知文字、完整6400像素肖像与140像素箭头比对，返键13为原opaque16×16全74，已知256像素保留；scroll固定95基点，不用文字起点84。未写背景保各seed原SHA，不伪造全帧相等。该门不改签前两轮原版false，也不代替整链原生。

**实现、适用验收与实质复审均已闭合，无剩余S5必修阻塞。** 本完整阶段独立提交并集成本地主项目，准确提交／集成与必要重建结果见本地`build/s5/local-integration.json`；完成摘要归档至[阶段记录](stage-log.md)与[完成摘要](completed-stages.md)。S4原范围保持；不存在自动授权的S5后续阶段。51个原源／用户档守卫、本地master快进、重建及证据转移由本地`build/s5/local-integration.json`最终登记。

最终来源入口为本地`build/s5/source-final-C22-index02.json`（最终index SHA `651a1bba35d3245ba192d2e696ebcd21499738389314879974772678cfcb54d9`）。C21独立Game三矩阵声明输入仍逐SHA相等；C22 Scene模块、82、正式参数、scope、去向及资源门重新运行。82强制记录main／scope两TU、CMake与唯一实际target，缺项、改摘要或删配方均拒，严格单行PASS／空stderr不变。

最终有限资格见`native-final-C22-qualification01/index-final01.json`：1446项输入、168产品／冷输入和适用等待参数核对；26good为2..500ms，21／24／20旧成功路径的等待均在旧门有效域。C21新binder等价原两字段绑定，source纯时间预检的删除不改变这些已证合法输入的执行；C22仅删除失败诊断。原21／24／20／26成功域可有限承接，旧排除仍保留，不称C22新实机。29分平台成功另见`native29-success-C22-qualification01/qualification01.json`；三次原版／工具失败及C20 SDL退出1不改签。

人数证据`credits-roster-bound04/receipt.json`（SHA `ee8a0953e4c57a8f920b1924f8d0c55344678624979c3aaf617c7ef6fc30c31d`）从完整原函数、真实招募DAT和初始集合取证：初始4人加后续28个ID覆盖0..31，18／19互斥、正常一次性路径不删除其记录，所以上界31。原1088d(30)虽malloc96，仅初始化raw0..30；31c49对count32会读未初始化raw31。当前消费者拒绝该有效域，不复制第32roster造原写点；32人SAV读写容量保持，外来32／重复callback／原未知字节不签。
