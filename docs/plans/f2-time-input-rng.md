# F2：时间、输入与共享随机轨迹

2026-11-18 用户终态纠偏：首actor指针、FS／GS、继承EBP、真实CPU绘制成本及PIC首相位只在会改变等待、输入、RNG或输出时保留行为门；不再为内部形式相同追加硬件研究。旧来源、失败和未验内部性质保留历史身份；本页旧“必须”要求按[当前总计划](development.md)解释，不将纯形式项继续移作F6产品阻塞。

**状态：共享机制主体随本次整阶段提交交付；A10—A13 的完整正常域联合门仍未通过。** 起点为 F1 主体提交 `503fe54b`。QK 仍按[专项](qk-behavior-followup.md)留作 F6 收尾门。下文分别登记已落实机制、当次证据与最后必须补齐的性质；不把主体交付等同全部时序一致，也不重派被拒的复审。

## 已落实的候选改动

| 对象 | 独立依据 | 当前实现与范围 |
|---|---|---|
| 默认共享 RNG | F1 原启动 RAM 与原 CPU 五个 BIOS 向量；CRT rand=1、共享 WORD=0、首次 rand=16838，推进198次后5885 | `field_rng_bootstrap`、App context；显式有效调试 seed 仍直接指定推进后的流。普通 Load 不重建流，`--new-game-play-once` 不再丢弃 prologue 已用 context |
| 键盘 AX 与重复 | 固定 DOSBox-X `b6abbd5` 的 keyboard／BIOS 源；FD2 标题及16c57读者 | 保存完整 AX；默认500/33ms、单一 last-key、modifier/锁键也可夺取重复 owner、15槽满丢新；修饰/锁/小键盘/Alt数字/F13 fake Shift 的输入状态按来源表达 |
| 共享 BIOS clock | 同版本 PIT=1193182Hz、divisor65536、BDA日界1800b0、RTC整秒初始化 | 新 `bios_clock` 使用整数有理数与NS边界；唯一App clock，Game/Load不重置。原低WORD signed/unsigned门仍由各consumer分别决定 |
| 原 tick 等待 | 原17aa9及既有完整clock读集 | VGA正式等待按低WORD原差值与下一tick边界，支持0..65535、回绕及HOST，替换 `N*55ms` |
| 普通镜头／聚焦／移动 | 完整12cea、13185、13460、135dd、1366a及各方向helper | 4e381恢复为FIFO clear；135dd不增加名义17ms等待；12cea保入口刷新及mode0不滚屏时不刷新、mode0/6免wait的原门。普通移动借唯一VGA内53A0C锚，判断不等后重读clock，不清队；脚本各分支保各自wait/clear位置 |
| 视觉与等键 | 1297d、11eee、127e0、4e310、16c57、26eda、2000a、26152、2b67f、28b41既有来源 | 视觉API统一NS；16c57仅signed差>=2，26eda另有倒退门，不能合并。恢复portrait后才RNG，再重新读clock写锚；不补追视觉相位 |
| 准备页输入 | 原26152/2b67f的AL20与AH比较、原冷行参数 | 补Alt数字空格AX0020确认及任意raw AH隐藏入口；不是只允许物理F键。新用例尚在复核，不签全准备页 |
| 浏览、目标与菜单等待 | 完整11aa8、115b6/12dac、17898与19953的独立来源 | 浏览借App 539F2，目标等待借本次caller索引；均先peek，tick变化才11cac并fresh写锚。四向菜单每个空等都water→highlight→actor→tiles→units→info→icons，不借浏览外门 |
| 逐读时钟与工作页 | 完整1297d、11eee、127a9/127e0、4e31c | live叶保条件短路及成功后的重新读钟；11eee按terrain→实际背景→range→tile。127a9过滤hidden后逐actor读取抖动钟，相机外及raw26=0仍读取；冻结投影保固定时间接口 |

`input_begin_test_frame`、真实DOS毫秒延迟和宿主action/result期限仍使用毫秒；NS转换只作用于原BIOS读者。音频驱动PIC相位及严格事件时间归F5，不能用本clock算式通过代签。

独立上游来源固定为 [keyboard.cpp](https://github.com/joncampbell123/dosbox-x/blob/b6abbd5980a885f5f310a4088c59a8688d1b116c/src/hardware/keyboard.cpp)、[bios_keyboard.cpp](https://github.com/joncampbell123/dosbox-x/blob/b6abbd5980a885f5f310a4088c59a8688d1b116c/src/ints/bios_keyboard.cpp)、[timer.h](https://github.com/joncampbell123/dosbox-x/blob/b6abbd5980a885f5f310a4088c59a8688d1b116c/include/timer.h)、[bios.cpp](https://github.com/joncampbell123/dosbox-x/blob/b6abbd5980a885f5f310a4088c59a8688d1b116c/src/ints/bios.cpp)。本地blob与SHA见 `build/f2/reference/manifest.json`；完整上游副本不入提交。

## 本轮证据及限制

以下各轮保留原候选身份，不相加为一轮完整通过。

- `build/f2/input03/receipt.json`：独立input入口1/1、38个测试函数；没有GUI、有效locale或实际typematic全轨迹声明。
- `clock-rng-ctest01.log`：时钟有理数边界及启动RNG两个入口通过。`rng-bootstrap/oracle-match.json`将198个调用前状态及末态，与五份既有原CPU向量逐项比较；不是C生成expected。
- `visual-clock-cpu01.json`：更新后的20个C向量对完整1297d、11eee至range门的原指令通过；明确替身及绘制/设备未覆盖不变。默认Python缺少unicorn的失败保留，随后使用项目既有 `uv run --no-project --with unicorn` 入口成功，没有修改全局Python环境。
- `wait-consumer-ctest03.log`：正式BIOS等待、详情、法术/物品列表、奖励等待四入口通过。`wait-schedule-independent01.json`由原signed差与独立PIT有理数推导预期，不读取生产实现结果。
- `wait-order-ctest01.log`：对话、总览、详情、Scene通过；详情额外在RNG回调时核整64000B portrait已恢复。该轮准备页新隐藏入口夹具失败记录保留；修正测试的cold音乐观察点及过早HOST后，`clock-consumers/prepare-hidden-ctest02.log`中准备页1/1通过，确认实际曲序10→stop→14及raw AH隐藏入口。它不覆盖长等重开眼的draw→RNG门，后者另补回归。
- `movement-ctest01.log`：BIOS等待、单位渲染、Scene交接3/3通过；`movement-host01/ctest01.log`中Play结果／单位渲染2/2通过，核退出后不继续改坐标、相位或AI后置。各轮候选有别；随后新增聚焦mode及Scene精确中断点尚待同候选复测。
- `movement-anchor-initial01.json`：只读既有F1装载前／App入口完整快照及strict镜像，53A0C的4B均为0；使用当轮object2 base映射并核原SHA，不重新启动原版。这不是新取得的post-CRT动态快照；完整13460写者资格单独来自source09。
- `rng-consumer-coverage.md`：将既有原4ebe3调用者逐族连到当前C owner，区分实际分支与静态CALL数。`rng-21db2-qualification01.md`另核两条遗漏CALL及孤立子图：已解析应用入口／分派未绑定，但82个系统间接点未全闭，不能称全局死代码，也未证明C遗漏正常玩法。该项留在库存，最终资格须再join。
- `keyboard-profile01/report.json`：正常DOSBox KEYB/config查询得到 `Codepage 437 has been loaded for layout us` 与 `auto`；WGC得到720×400的真实DOS命令画面，进程和Job正常清理、二进制及bootstrap守卫不变。脚本预设输出文本不匹配而记失败，原报告保留；它不是FD2运行后有效键盘配置或500/33动态证明。

普通标题长按工具 `tools/run_f2_input_check.py` 与driver `hold-key`只发送一次DOWN/UP，让目标自身生成重复；两端串行、ON先验、owned HWND、正常清理及SHA／mtime守卫。`input-native01`在原版收到一次DOWN/UP后因粗时钟1188ms不满足1176±4ms门而失败，SDL未启动，旧报告保留。driver增加QPC单调计时、成功释放资格及受限高精度等待后，`input-native02`双端通过：原版DOWN/UP为1,176,000,300／780,000,200ns，SDL为1,176,000,300／780,000,300ns；三项标题菜单最终选择分别为1／0，来源、二进制、bootstrap及用户档守卫不变，owned进程和Job均正常退出、无残留。14项工具回归与driver焦点回归另账。最终画面只证明菜单模数状态；投递时长不等于目标处理时长，预测22／10次make不是实际FIFO计数。该次原生候选早于本节后续浏览／菜单接线，不能签后续整个App。

`motion-native01`双端正式路径通过：原START进入浏览、两次导航、资源／选择、一步移动到COMMAND、取消恢复、零步COMMAND、再次取消及浏览取消。原版12个捕获点（含一次启动WGC重试），SDL 11个业务点；SHA／mtime、显示需求释放、正常owned清理与Job零残留均过。工具先构建独立参照再启动原窗口，不使用C生成expected。该次候选也早于新浏览／菜单／live读取接线；最终新候选须重验受影响的最小运动代表。它不证明精确帧时间、全部FIFO／RNG或全像素；旧比较的调色分量容差3不能代签F4。后续复审确认该次入口未显式传 `require_display_on=True`；原收据实际before两种显示通知均为ON，未请求唤醒、未调用独立唤醒API或改电源，但不能称入口已强制ON先验。现已显式传True，`motion-tool-tests02.log`三项工具回归核该参数及停止／清理边界，原收据保持不变。

`motion-native02`在上述live／菜单／准备／战斗返回候选上重新执行同一最小运动链，双端通过。App SHA256为 `99061b23a10705fdb4fe38b06a5d83e2593adee050c0dee84fd9b2d84af01955`，报告SHA为 `1996d84bcbd41924ed3073e1d6a9015fdf18270e0fa15cbadb5df4e0a6f8fdea`；23个双端检查点。入口明确ON先验，显示需求正常释放，未唤醒或改电源；原件bytes／mtime、src/tools／冷数据、二进制与bootstrap守卫均不变。原版PID33944／SDL PID21364均正常退出，Job44004未超时、无残余或强制清理。已查看两端一步COMMAND截图；比较范围沿原工具，仍不签全像素、逐时刻FIFO／RNG或绝对时间。

### 后续正式读钟接线与定向回归

`input-wait-source01/source01.json` SHA256为 `6f112a51399bdae47e82d208393a27ca9b4f78c451e71aeb21bc254ba6a248c1`，含11aa8、117e7、115b6、12dac、18b84、2000a完整Domain指令和strict字节比对；`source02.json`补18890，`source04.json`补_brk。source03的完整覆盖断言失败，未产出合格JSON，不计来源资格。源数据库及镜像只读，未改类型或运行二进制。

`menu-wait-source01/source01.json` SHA256为 `53f1e84db5e2973f2774cdaadc148ac4083ec70a024b012ea2cc404207571952`，补1f894、1741c、175a9、17643、176b4及五个菜单caller的完整指令／调用边。所有chunk连续覆盖且与strict镜像逐字节相同，原库／隔离库／镜像SHA和mtime不变；没有保存、autoanalysis或类型变更。标题现按原INT16一次读一、每项返回重绘，删除批量读队列后的50ms；仅空等使用现input owner的1ms宿主pump，不推进视觉或另设队列。该最新标题已随menu-wait构建；首轮CTest先被下述旧chain断言阻断，新读序／未知键／HOST回归及原生定向结果另记。

PrintScreen静态复审确认make／break都清当前last-key，而旧候选漏了break。现已修复，并区分真实物理修饰位与BIOS fake Shift；`input-printscr-build01.log`／`input-printscr-ctest01.log`中独立input入口通过，内含40个测试函数。新增反例核PrtSc DOWN→B DOWN→PrtSc UP之后保B的FIFO但不再重复，以及三种修饰、未配对fake Shift和真实SDL事件pump顺序。`input-printscr-source01/us-hook.json` SHA256为 `986cd466cfed7c73630f527205cabf71b6197c43eb8360ed1ad6511f74cc085c`；同commit的layout源及内置US437字节独立核hook路径，完整上游仅保本地。该静态US分支及C回归不是FD2实际有效布局或新原版运行证明；App随后已由menu-wait构建，未将旧原生候选改签。

live API替换正式caller读取；固定now_ns接口仍供同tick向量，不能以固定时间测试代签多次读取。新 `field_visual_live` 只替SDL时间，独立PIT边界核各次读取、写锚前的phase/DAC/背景像素及错误写集；`field_unit_live` 使用真实unit＋visual，仅替时间，核每actor即时绘制、空／全hidden零读取、相机外／raw26=0读取和资源失败前缀。无窗口，不登记为原版运行。

- `live-clock-build01.log`／`live-clock-ctest01.log`：新live叶构建及1/1通过。
- `live-consumers-build02.log`／`live-consumers-ctest02.log`：4入口中3过；Play结果因合成夹具未替换新增menu-refresh而失败。补测试依赖替身，未修改生产门或业务expected。
- `live-consumers-build03.log`／`live-consumers-ctest03.log`：10入口中8过，含新unit_live、input_wait与Play结果。unit_render的无工作页诊断及aux旧每outer调用Game_tick计数失败；恢复无工作页诊断分支，并将aux断言改为原11aa8门下不调用通用tick。
- `live-consumers-build04.log`／`live-consumers-ctest04.log`：unit_render、aux、Scene交接3/4通过；S5文本owner夹具当轮失败，未降门。失败轮次和原始日志保留，不累加成一次新全绿。
- `live-consumers-build05.log`／`live-consumers-ctest05.log`：补S5合成夹具缺失的正式visual绑定后，S5来源／unit_render 2/2通过。新menu组合用真实资源按原叶顺序独立构造153,216B页和64,000B显示像素，覆盖COMMAND／ITEM／SYSTEM；不签多次读钟或原版运行。
- `live-consumers-build06.log`／`live-consumers-ctest06.log`：unit_render、Play结果、准备页3/4通过；总览dispatcher合成夹具未适配新的menu／browse入口而失败。补其有限依赖替身后，build07／ctest07总览1/1通过；真实App页另由unit_render总览入口组合核，不用替身签它。
- `live-consumers-build08.log`／`live-consumers-ctest08.log`：十入口九过；新增Battle返回夹具先缺基本FULL费用marker，补后ctest09又暴露MAP费用／模式前置遗漏。按真实FIGANI／正式MAP事件补齐，build11／ctest11 Battle单项通过，原生产校验未放宽。新增22个adapter向量仅签同App页投影、phase1／3、skip／HOST／失败前缀；Game真实页与clock另账。
- `live-consumers-build10.log`／`live-consumers-ctest10.log`：App重建与准备页单项通过。2b4fb直接live写App actor phase，2b67f失败不回滚已写53F46；新四读向量按独立tick5→6→6→7核anchor与绘图H失败前缀，早确认不读钟、不吞余键。H故障注入不是原版坏资源运行oracle。
- `summary-body-build01.log`／`summary-body-ctest01.log`：Play结果通过，unit_render发生SegFault。CDB只调试自有C测试，定位新增夹具释放VGA后遗留全局active_input；改为本地FIFO向量、不注册临时owner后，build02／ctest02该入口通过。调试器首次相对路径未识别的0n2记录也保留，后用绝对路径取得c0000005堆栈；未修改原版进程或二进制。

summary-body候选只补18b84已证body：入口锁定所选raw及屏内focus决定的板侧，actor→terrain／背景／range／tile→selection→逐actor→149×42 summary写回同一页→info→project。真实资源整页及错误前缀新增五组组合与两个坏参数；没有输入、RNG或宿主deadline写。该候选当时尚未接完整DWORD比较、每16ms空轮都执行body；不能将body通过写成整个等待通过。`motion-native02`早于该body修改，保其原候选身份。

后续menu-wait候选已恢复18c65之后的门：成功body后fresh读取signed低WORD，后续先water①，完整DWORD不同才water②／body／fresh写锚。`have_anchor`只标本实现已到18c65，局部生命结束即消失，不是原字段；首次仍保候选的先画行为，**完整原actor地址首次比较仍未资格**，没有用0、当前tick或宿主C指针冒充。`menu-wait-ctest01.log`的input_wait以独立时钟核同tick、高位非0、负WORD／回绕、早键、两次water／body失败及HOST；Play结果同轮通过。保留的16ms宿主采样仍是未签绝对时序，不据此宣称完整18b84通过。

同轮 `menu-wait-build01.log`完成App及五个直接入口构建，CTest五项中Play结果、aux与input_wait三过。main_title_resource暴露旧stage21未支持断言；改用真实非战场30并加21／29正例后，build02／ctest02该入口通过，含新增逐键／空等／HOST回归。unit_render新增前缀夹具当轮失败保留；其纯EVENTS上下文未登记SDL主线程，worker的注入提前执行，不能冒作首present后HOST。按锁定SDL源码修正线程身份／原子握手及phase观察，另保入口HOST全字节不变向量；补正常声明头后，ctest03又在后续whitening发生SegFault。自有C测试堆栈 `menu-wait-crash01.log`核为新夹具释放后遗留全局active_input；改为本地受控FIFO、不注册临时owner后，build05／ctest04中unit_render与aux两项通过。没有为测试新增生产API或改变原正常前缀。

菜单开合候选沿唯一Game菜单owner保存原5184B底图，取消每相位全图重画及无源16ms；真资源工作页／VGA、五表族、历史底图及HOST／direct失败前缀已有上述定向证据。Play六个新局部变量仅改名消除遮蔽警告，App由build03重建；各轮不累加为一次同候选五项全绿。`menu-input-review01/notes.md`独立只读复核输入、菜单、概要板、sizeof／value-copy／Load及显式SAV序列化，没有发现新的可证实现错误；它没有新运行或扩签。

`ui-transition-source01/source01.json` SHA256为 `9aaac5b34cb503ceacb54a1f5bf97e45b2352c2b237db9f925847381a1fc4902`，含详情／列表、底框、选择框的14个完整函数和直接caller。每个chunk连续原指令／strict bytes核对及原库／隔离库／镜像SHA、mtime守卫均通过，无保存、类型或原版运行。现已按共享相位族移除无源16ms，保有源2tick等待、10ms Scene对话、尾刷新和clear；相位CPU提交与WGC可见帧、绝对耗时分别登记，不以删除delay或调用次数代签。

`loader-allocation-source01/source13.json` SHA256为 `1b9649ba9c5d9edd789010466c4c4e20c223fed299f2d0c30526a982f47525dc`，将旧L0绑定的16bit模块正确映射后导出13个完整函数／1,418条原指令，逐字节匹配；新manifest／末守卫保旧9源bytes和mtime。已核object大小对齐、0501请求、base回写、stack ordinal消费及失败分支；逐object路径清base再申请，不能由object1大小推连续几何下界。早期未映射far selector的错误候选、失败日志均保留，只有source13用于完整链资格。DPMI／DOS handler gate及成功分配／扩张的地址界仍未绑定；这不是缺少原RAM或新的工具限制。另核[Delorie 0501接口说明](https://www.delorie.com/djgpp/doc/dpmi/api/310501.html)，该页仅给linear返回和对齐等合同，没有所需地址界，不能靠API名称补齐首次比较证明。会话未提供ketch／Pi Web Search；外部检索用Cindy搜索，读取用可访问的同一说明页，首次镜像读取失败不算取得来源。

`ida-live-publication02/publication-receipt.json`记录本次14函数注释同步及完整Domain重导：1,312函数的名字、边界、签名、flags、chunks及strict字节不变，去注释后的伪C token完全相同。登记函数为523个，不作覆盖率；发布6份产物后最后刷新／核对11项manifest。原数据库／镜像守卫不变，旧prototype／fixup报告按不变身份继承，没有重新签语义。更新命名输入后、重导前的publication-tool-check03单项失败保留；重导后check04的14项通过。

准备页新入口复用G3唯一 `p2-self-give` 受控场景及原资源／完整22,987B SAV oracle，另加ON和当前来源门；不是自然到达或完整RNG／精确phase证明。`prepare-tool-tests02.log`九项纯工具回归通过。首次 `prepare-native01`在实际配置资格门拒绝旧G3的 `mixer.rate=44100`（固定参照要求48000），首窗口未启动；Job38700正常清理且来源守卫不变。后续只调整新隔离实验的显式命令参数，不改系统混音器、bootstrap或旧收据。

`prepare-tool-tests04.log`的12项回归和 `prepare-reference-preflight02.json` 的真实只读预检通过；前者的03失败记录保留。入口只对已知旧G3的44100配置追加本次实际 `-set mixer rate=48000`，目标同时核冻结参照与bootstrap；已有48000不追加，未知值和其它来源差异仍拒绝。实际启动收据核相同argv，配置bytes／mtime在资格前冻结，不能在检查后重新取身份掩盖改变。

`prepare-native02/report.json`在App `ce8c2e6188aff9f54d07829299dec59662c0cb2e4067add61e706f55ad96c543`上双端通过，报告SHA256为 `f38de5b8c83fb17d51516c31507e397bdbe45c172df1892a3bd6a4e6ad636480`。28个业务步骤包含标题Load、service self-give、返回城镇、酒店Save；两端各自完整22,987B容器检查均零差异，未知尾保留。原版PID9500／SDL PID43888正常退出0，Job696未超时、残余或强制清理；严格ON先验及同线程显示需求释放、原资源／来源／二进制／bootstrap守卫均通过，未唤醒或改全局设置。已查看两端service-greeting截图。原版启动几何过渡及两次空白WGC重试原记录保留；继承资源门的分量容差3、有限portrait／标记相位与受控初态仍是限制，不签自然到达、完整RNG／FIFO、严格phase、全像素或音频等价。

准备页现在用两个App DWORD表达原53F52／53F72。26152城镇四tick只推进53F52；26eda入口只置53F52=2，27079在其奇相位推进53F72，即使当前mode不画成员。酒店、服务、名单首绘／导航和重开服务均借该owner，不再由局部结构清零或用highlight冒充sprite。19953重开眼先实际portrait0再抽 `%30+10`，高亮先改phase再重新取clock写锚。准备页新增跨入口、town重入、早键、HOST和失败前缀回归已包含在ctest06；这一候选的连续正常原版行为仍待取证。

源1d80b／1d8ba直接遍历raw并调用13a9f；正式Play去掉人为150ms自动行动间隔及其空等期间的额外actor／water推进，保动作内有源等待。Play结果用未来宿主期限夹具锁住误用期限的旧路径；ctest06通过。`field_game_tick`无窗口诊断仍保旧deadline，不声称其为原时间语义。

主体交付时记录的旧余项（现行判据见末尾表现层表）：18b84首次ESI为原actor地址，当时要求完整DWORD比较排除signed16值域；既有本轮heap实例可证首比较不等，不能外推所有正常启动。其余等键时点、准备页新owner的原版动态代表、全部真实RNG调用／写序、旧通用outer tick残余和FX／战斗读点仍须闭合。已发现差异、内部形式未证与动态证据缺口分别登记，不按测试数宣称完成。

21db2静态绑定后续收口在 `rng-system-indirect-source01/report.md`／`qualification03.json`（SHA256 `90051e7bc7be7b643613ccde67dcd217b81a967a27e43ccb4097d66959645ecd`）：原82系统间接点分为78个有限正常target集、2个正常ABI续点、2个外部provider未决。前80点都有完整来源及目标形成合同；不按函数地址高或名称排除。两处精确未决为4624d／4633c的 `[527ec+30]`，仅host-type9分支；F1同进程三个CRT后快照均type1、provider0，只证明该实际进程未走该分支，不当作三次独立启动或所有正常启动证明。取得129个完整owner／144个chunk成员，head记录含数据声明，不作机器指令执行数或功能率。独立DB、EXE／镜像及旧证据SHA／mtime守卫不变；没有新原CPU或原版运行，21db2仍不判全局dead或确定缺C能力。

`build/f2/normal-behavior-pilot-inventory.md`核得真实自然首关turn5及第二关turn3存档，可用于死亡/掉落后存取和增援剧情代表。现runner尚需短程续档/止点，不将受控S5档或可解析用户档升格为自然可达证明。

### 有限 UI、返回边界与全读点盘点

有限 UI 开合现共用原相位族：详情12／12与下页7＋空条＋7，底框6／5，选择框4／4，总览12／12，辅助图7／6。保留各 caller 独立的全页 snapshot 恢复、清键及刷新；`196cb` 在最后 `11cac(0)` 前释放三份 buffer，准备页 `26996` 不额外刷新战场。局部 H16ms deadline 只由产生它的 modal 撤销，外来或已经替换的 deadline 保留。源合同及候选身份见本地 `build/f2/ui-transition-review01.md`，不是逐相位 WGC 可见性证明。

`ui-transition-build01.log` 完成 App 及九个直接入口构建。首批 CTest 九项中五项通过，另四项暴露测试夹具对有限相位零耗时、实际关闭边界和正式菜单 owner 的旧假设；修正仅限夹具，原业务、资源和总提交数断言保留。`ui-transition-build02.log` 随后重建 App、四个修正入口及 S5 直接受影响入口；`ui-transition-ctest02.log` 六项全部通过：`s5_cinematic`、`s5_stage_source`、`field_play_overview`、`field_prepare_play`、`field_progress_play`、`field_system_message`。两批分别保候选和失败身份，不拼成一次全历史全绿。

Play 现按原 `main` 的 `25dce→25dd5／25e02` 返回合同，在 drain／resume／终端处理之后、任何下一轮等待／渲染之前消费胜败 latch；保尚有剧情／掉落续态时的延后及错误／HOST 优先。`outer-wait-reachability01` 的完整正常调用分区没有把 legacy 防御分支误认作每轮可达；六个新向量从真实 controller 或 completion 返回后观察零额外 water、读钟、输入、呈现及 RNG，已随首批 `field_play_result` 通过。它不代签所有结果 caller 的 F3 行为。

全读点库存 `clock-consumer-inventory02` 从 76,605 个真 code head 区分数据声明，得到 21 个读钟 owner；原 `17aa9` 有 42 个 caller／96 个 CALL 点。这些是静态分母，不是运行覆盖率。该交叉核对已发现正式 Scene `cin_water` 把毫秒交给 NS API，现接 `water_tick_live` 保逐读顺序，生产绑定为 `stage.source.victory.020` 的68个节点；上述两个 S5 入口已通过，不能把68个绑定当68份独立语义证据。

同一盘点发现的两项遗漏已实现：玩家 `1cff0` 的 id9..27 且非24在双 selector 成功后、handler／费用之前调用 `1d6c8`；现由唯一冷表执行四轮彩色／黑色交替，各半段等1tick，23取消第二目的地不进入前缀。AI `15055` 的高射程 item79 现按冷图执行原 caller 的清格、200ms、face／refresh／wait、`2dfc8`、远端 focus／八相位／首目标与原效果；未建立第二业务 owner。item79 的射程为31，value WORD为3，属于当前正式支持域。原 `2dfc8` 从 caller EDI 所给的目标数开始播 part，并非从0；局部图形释放后仍尾等6tick，后续效果只提交一次。完整来源为 `battle-wait-source01/source01.json` 及 F1 `domain-boundaries/source09.json`。

这次同时修复共享 `149f8` collector 的扫描方向：原从 actor 向 selected，旧 C 相反。结论由两个真实 caller 的 push 顺序、固定栈深与完整 callee 原指令交叉证明，不按伪 C 参数名猜测。评分与提交共用同一修复；玩家法术30的独立 collector 原已正确。正式 App 共用的 `REFRESH(3)` 仅一次刷新／present，`WAIT_TICKS` 和 `DELAY_MS(200)` 不额外 present、查 info 或过 round 门。

`ai-ray-build01／ctest01` 首批八入口五项通过；其余失败保留。Magic／Item 的旧负例把已绑定 common 的满诊断日志误判为无效，原业务不消费此日志，现用 `CAP+1` 验真无效计数，并补满日志的正向无消费性质；没有改产品门。AI 新方向断言依完整 `1f0cc` 改为2，旧观察器另计射线前缀，仍保全效果／RNG／acted断言。`ai-ray-ctest02` 的 Magic／Item、`ai-ray-ctest03` 的 AI 均通过；它们不拼成单轮全历史通过。Battle 的真实 FIGANI58 七 part、count2／count7／零时长和 HOST／cue失败、Game七种停止前缀、纯 collector 正交／边界／raw顺序分别有直接回归；不是自然获得 item79 或原版演出全像素证明。

首值的源合同仍独立保留：`2000a` 初始比较字节可追到前次正常 `int386` 的 FS／GS，应用与运行库正常返回内的 writer 已盘点，但外部服务、设备 ISR 和旧 IRQ 链返回合同尚缺；`1b1e7` 的比较用继承 EBP，该函数不更新它。新增 `overview-ebp-source01/source01.json`（SHA256 `59b077971a1141f031460bb8141d907c8fb45d1797ab7f60263cbc48ae7df5e3`）完整原指令确认 `__CMain` 在 `4609d` 把 ESP 赋给 EBP 后调用 `main`；这补充首值来源，尚不等于全调用链保存及地址域通过。普通游戏操作、只读观察和当前 profile 的证据另记，不能用当前 tick／0／宿主指针替代未知初值。

## 当前候选的正常代表与来源发布

App SHA256 `a57b092390db7dbf6daa3771257a8150846303ea78bc35d4a04fbc6146116bb0` 的 `input-native03`、`motion-native04`、`prepare-native03` 双端代表均通过。分别覆盖标题实际长按、浏览／导航／一步和零步命令及取消恢复、准备页既有 `p2-self-give` 的实际输入与完整SAV。三轮均保 strict ON、WGC HWND、原版先行／SDL后行、来源及隔离存档守卫、正常 owned／Job 清理且零残留；不是全键盘或逐调用轨迹。`motion-native03-job.log` 另保错误 `.conf` 启动参数造成的开窗前失败，未执行游戏动作，随后正确命令采用新目录04。

`normal-clock-native02/report.json` 的新读取门只采用同一 `MEMDUMPBIN` 命令内的固定长度表达式，PE／非VM／非PG不符时零长，不解析为字段值；不暂停、改寄存器或设置断点。两页分别重新取得 descriptor／code、两独立合法fixup锚、前后 pointer与count12。auxiliary五条EV中三条命中 `2000a` 本体，见FS0／GS20；overview在12秒172条EV中零本体，整体仍失败。callee／runtime样本不得代签继承EBP或首比较。原01失败、02原件和只读复核 `normal-clock-native02-review01` 均保留；不是原版业务反例，也不是全称初值证明。

`ida-ui-ray-publication03/publication-receipt.json`：隔离 Domain 库只改五个已证名和18处注释；1,312函数的边界／类型签名／flags／chunks／strict bytes全不变，伪C仅正规化这五个名字后token相同。528登记是来源条目数，不是功能覆盖率。六个生成物及命名页由生成器发布，最后刷新并核11项SHA；14项crosscheck通过。旧静态／原型报告保源身份继承，没有重跑或升格语义。

阶段收口构建首次因E盘满失败（`final-mechanisms-build01.log`，含112／PDB／FileTracker写失败），不登记通过。仅删除经绝对路径校验的108个可再生成 `.ilk`，清单在 `disk-full-ilk-cache-manifest01.json`，未删程序、PDB或原证据。C盘普通独立目录经当前FileTracker确认未排除后新建构建；后续结果按实际新候选登记，不冒充原E盘一次成功。原生已验a57程序及mtime另保 `native-a57-preserved01.json`。

## 主体交付与最后联合门

F2主体及历史结果保持原身份。2026-11-18 后续门改为游戏表现：不再要求F5补ISR／寄存器／原栈保存证明，也不把它们移交F6卡住全计划。

| 原事项 | 纯内部部分的结论 | 仍需的游戏结果 |
|---|---|---|
| 18b84首次ESI／actor地址 | 原pool地址与signed16相交的全内存域证明不再要求 | 正常首次刷新、等待、早键及后续状态／RNG；存在可见差异才修相应逻辑 |
| 2000a首次FS／GS | 外部服务／ISR／旧IRQ的全寄存器保存形式不再要求；native02样本只保当次身份 | 正常辅助页第一次打开、连续重开、确认／取消的响应与节奏；postfirst两tick合同保留 |
| 1b1e7继承EBP | 不重造原栈／地址／保存寄存器布局 | raw26相关正常概要刷新、后续操作与RNG仍可能受steady门影响，必须以独立正常行为／既有源语义判定，不能只称内部无影响 |
| type9 provider／21db2 | 已有固定普通成功启动来源条件排除保留，不新模拟DOS宿主环境 | 参照内正式输入／分派去向不变；真正新增provider改变可达玩法时重开相关资格 |
| FIFO／typematic与共享RNG | 不要求键盘控制器或逐CPU指令／IRQ形式相同 | 多键、确认／取消、清键、正常等待、跨场景与QK后的游戏结果和RNG使用继续联合 |
| 画面／音乐／PCM时间 | 逐扫描线、端口绝对时刻和严格PIC RED退出产品门 | 完整游戏画面、动画／音乐节奏、音效起停／叠加和音画同步由F4／F5及F6正常联合验收 |

新门以正常操作、结果、存档和完整输出为对象，源内容／合法参数／已有代表互补，不用C自证或任意宽容差。若后续更改这些consumer、RNG、输入等待或cue，只测直接影响。内部研究标「不再要求」，真实行为未知／必需缺证仍保留；不因未观测反例就称无影响。

## 主体收口记录

新普通构建目录 `C:/Projects/fd2sdl-builds/fierce-newton-f2-20261115` 的完整Debug构建成功；`c-build-ctest01.log` 的46项本轮／直接影响入口全部通过，无skip。当前App为 `ce6265b1877df8a9d0cd2f46a15f09a11989402af9c81c0d61b42078ca939e05`。实际 `CL.read.1.tlog` 含 `input.h` 和 `bios_clock.h`，不是仅凭没有MSB8029认定增量安全。

三轮原生代表的源码／工具／冷数据与新构建完全相同；新PE的各节并非逐byte相同，因此原生执行身份仍只记a57，不升级为ce新跑。`c-build-native-code-inheritance01.json`保两候选节摘要；共享源码相同仅支持有解释的来源影响继承，不是二进制同一或原版绝对时序证明。GUI三轮／新构建46项／局部原码／来源与工具检查分别保原收据，不能相加成一次全fresh全通过。

只读实质复审 `root-clock-leaves-review01.md`、`f2-closure-dependencies-review01.md` 和 `normal-clock-native02-review01` 均未发现本轮范围中的新增确定功能反例；它们明确保留首域、callee、同命令读取和异常测试的边界。此前已修overflow、重复focus、单位错位和未实现公共前缀，不把旧失败重列为阻塞。主体包含必要实现、直接验证和精简；三门、联合轨迹与F4／F5非例外原版证据继续按上表收尾。

Windows是本轮平台。Linux保留Bash构建说明但未执行。`EX-AUDIO-NUKED`、原资源/存档只读、显示ON先验、串行owned清理及不改混音器/电源计划的边界不变。
