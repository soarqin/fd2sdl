# S3：按依赖闭包批量接入正式流程

本阶段从 S2 `54ebc72` 连续实施；执行状态只由[总计划](development.md)维护。本页登记实际批次、共享补充及五层证据，不另维护优先级。**实现、五层适用验收与实质审查已闭合；完整阶段提交及本地集成身份以Git和 `build/s3/local-integration.json` 为准。下一阶段S4未开始。**

## 1. 批量交付域与边界

同一完整阶段内按两个依赖批次接入 **stage2—10、stage11—19** 的正式战斗、END、来源事件和胜利准备交接，并接通 **stage20入场、浏览及存取**。共同图、领域原语和调用续点只实现一次，绑定参数及资源前置按全量来源校验，不逐关开发、实机重验或提交。

stage20战斗／END保持关闭：`victory20 @0x240fa` 的私有动画和资源生命期仍隔离。stage25虽有独立图，自然前后链跨隔离域，不越过它单独开放。S2的13个隔离来源槽／14引用保留，仅阻塞相应依赖；stage31／32开场链与30槽战场表分账。默认turn255拒绝不变，不承诺无限回合或任意外部快照等价。

| 统计域 | 实际值与含义 |
|---|---|
| 正式来源绑定 | 96身份／86不同目标＝19 entry＋18 victory＋18 result＋41 callback；不是96种机制或运行覆盖率 |
| 冷图复用 | stage新增52别名、common新增44别名；stage2预计算／重放入口改为source图；补正确callback41及共享胜利前缀 |
| S1／S2来源身份 | 33资源组、208分派槽／193目标、106移动流、72,470来源账不复制或改签；原315未决中的301可用引用、14隔离引用沿用原账 |
| 当前callback输入 | 90原槽／88 owner；73转换槽→67图／2,045节点，16既有引用、callback82一个隔离槽。与旧S2候选分账，不以图数充关卡数 |
| 选定callback引用 | 55条metadata引用：50属于新战斗域必需调用，stage20另5条仅保来源而不绑定战斗 |
| 默认turn255 | 全33组450＝战场0..29的406＋资源30..32的44；新战斗2..19为254，entry-only20为11，选定2..20共265。没有删槽求绿 |

新绑定只引用唯一冷图及原DAT模板／placement／metadata／文本／移动，不复制资源。source capability的 `preparation_slots=0` 表示实际准备初始化读DAT metadata byte1，不表示无部署；`exact=0`允许增援。活动记录下界不反抄部署表：2..20依次15、24、42、33、34、29、34、53、39、25、59、67、74、76、52、53、64、83、75；stage16保留52／53两条入口。

## 2. 正式消费与共享修正

`field.c`核正式alias与source图节点／入口身份，Scene逐叶执行。胜利在唯一音乐前缀后执行来源图，只读导出原持久roster、选项、钱和未知尾，由App接管准备并关闭旧Game／资源；没有预算整图终态、重复招募／同步、整体回滚或第二热状态。

| 原来源发现 | 本阶段共同修正 |
|---|---|
| 活动导入及标题CONTINUE仍有旧stage0／1／2门，与已发布capability不同 | 导入／可用性统一capability，保原活动记录、动态metadata、未知字节；失败不换资源／owner／RNG |
| stage12初始59条活动记录，20765仅signed turn>5读raw59；stage18初始64条活动记录，20926仅signed turn>6读raw64 | 先走原条件，在实际读叶检查边界；早期可Load，晚分支缺raw明确拒绝，不填假actor或默认败北 |
| stage16有持久text18时52人，否则追加group1为53人；20872同样先查roster才读raw52 | entry、结果、恢复保两分支，不统一提高到53屏蔽合法状态 |
| S2误把callback41复用文本11的child1；完整35346／共享尾实推文本4 | 新common.callback.041为story3→child1→story4；旧图原域不变。完整caller／C复验；旧S2收据及错误分类不改签 |
| 原结果不只1197b | 恢复1197b、1a94d、1d8a0、1d96c、1d9fc五CALL续点；状态调用在HP／死亡后、六timer前，side1和side0两pass保每raw查询时点 |
| 带故事结果不能在const getter／每帧执行 | 原caller启动共同图，沿既有event_resume及PLAYER／AI／STATUS／LOOT完成tag续行；等待ACK前不按中间latch离场，重复ACK／HOST／失败前缀单列 |
| AI奖励返回1吞查询、nonplayer子奖励及状态旧notice影响续行 | 修共享奖励尾、父子owner及已消费notice；不加热字段／SAV字段／事务框架 |
| 合法Load晚期缺raw会走旧enum getter abort | 正式路径统一checked查询，拒绝保输出／记录／RNG／latch；不提高Load下界隐藏状态。旧ABI仅兼容，产品无调用 |
| stage16总览拒绝且败北文字取决于持久roster | 完整1b41d：胜利597＋2×stage；stage16有text18败北630，否则628，其余胜利＋1。唯一field_info冷表／共用roster查询，不按HP／hidden／side或活动actor代替 |

这些是正式接入的共享依赖，不重开G／S0整阶段，也不把S2当时显式机制门改签为新caller已过。旧Scene17级比例fade仍唯一归S4。

实际绑定样例：S1 `stage:02:turn:00000` 的原metadata引用，沿 `dispatch:event_callback:009` 定位到完整原函数，再由 `stage.002.action.009` 指向唯一 `common.callback.009`。图保留 `branch_actor [6,0,0,0]` 两边、`append_group [2,0]`、镜头 `[3,0]`／800 ms／`[3,17]`／200 ms、story4及ACK／错误出口；DAT记录和原引用顺序没有另抄。正式Game／Scene消费相同节点，批量差分按stage2实际组表核写序，turn3原生代表再验证真实触发与15→27活动表。一次性／重复性仍由原触发调度和状态字段决定，不凭同target加完成位或重复执行。

## 3. 五层证据与候选

最终产品候选12通过system package导入主项目SDL3 3.2.12及MSVC14.52，未调用CPM；工作树缓存的3.4.20／0.43.2回退值未使用，主项目仍保留3.2.12／0.40.8。实际构建 `src/build-s0-baseline`。`build/s3/native-candidate12/manifest.json`封存EXE／PDB、161生产来源、23实际生成inc与探针。游戏SHA256 `14964f6c09c4c7b5d97aeed26a3a5a948a3f664cb3bef922a68f093dfbe85a52`。本工作树的 `build-win`只提供输入／WGC驱动，不能将其旧inc混作游戏来源。

| 层 | 实际证据 | 限定 |
|---|---|---|
| 独立原来源 | S1原账／DAT、完整函数／caller／LE原页；本轮Domain补五caller、41与overview。命名及伪C由生成器更新，1,312边界和3段原bytes不变；21 LE＋14发布＋11摘要检查通过 | 伪C非源码，未知CFG不造边；完整dump本地不分发 |
| 静态闭包 | release-scope-candidate03核96绑定、真实WORD／参数／资源、entry两分支、18胜利到3..20准备／entry；13隔离无绑定，255保拒绝 | bound不是全玩法supported；严格支持集合仍空，不删拒绝域 |
| 原码／实际C | oracles-candidate12：916结果、27 caller；callback-binding-source-candidate12：180原caller／192 C；callback-candidate12：1,971正常＋144等待＋1容量；overview-cpu-C-final09：242；binding-traces-candidate12的38绑定／36图：1,026正常＋90等待＋1容量，另3既有模板caller单签 | 各域重合不相加作机制数；绘制／奖励等有显式叶替身，不是完整AI／ABI／全RNG／WGC |
| SDL逻辑／正式 | 同候选12的24项直接影响CTest全过（10.79秒），含entry2..20、16双人数、恢复／结果、失败取消、App／Scene／准备／Play出口。三组正式按键代表分别有实际候选／继承资格，下行列明 | 无窗口entry用受控六人roster及真DAT，不是自然历史；未跑G／S0全历史GUI／全CTest |
| 原版代表 | turn3-final09-r2、victory-final10、stage12-final12三组双端通过；最后一组为C12新实做，前两组按下段严格来源影响继承 | 受控注入不称自然整关；API／S2机制夹具不代正式App，其它平台未执行 |

候选09→10修标题显示门，10→11补齐真实CONTINUE执行门，11→12只同步三文件旧范围注释并保持行数，精确回替证据见 `candidate11-source-qualification.json`、`candidate12-source-qualification.json` 及 `comment-sync-candidate12/`。最终12已新跑24项CTest及结果／caller／callback／逐正式绑定的实际C。overview探针仍与09逐字节同SHA，242项按原执行身份严格继承。turn3-r2的EXE仍09、victory的EXE仍10：只在stage2新旧两能力谓词同值且其余逻辑未改的范围继承，不冒称12新GUI；新接纳的stage12正式CONTINUE已由12新配对完成。

overview242实际执行原条件／计数／roster并比较C，绘制／栈／文字有明确替身；stage16未单独新WGC，沿未变四底板／字形绘制原语及模态的历史呈现门，不能说所有总览像素新实做。Play败北返回／余留事件与新锁存有直接回归；正式败北原版门的严格继承资格在最终代表矩阵单列。

## 4. 正式原生代表与复跑

均运行无参数正式 `fd2sdl.exe`，真实标题CONTINUE／LOAD、系统菜单、酒店及出阵。夹具从有原版B7证据的完整SAV隔离复制，逐byte登记，不称自然产生：

| 代表 | 受控域及检查点 |
|---|---|
| stage2-turn3-storage | NPC加已证不行动flag，3玩家移至独立合法空格；两次END到turn3，15→27增援／镜头／story4；Save、Load取消／成功及完整再存 |
| stage2-victory3-entry | 仅敌人hidden；CONTINUE后真实controller→victory2布局／story7→准备3酒店四槽；第二次标题LOAD→出阵3／24人entry及活动恢复 |
| stage12-entry-cancel-storage | 保序15条原持久记录／stage12手动槽；LOAD→准备／出阵／59条活动记录entry（本夹具14名实际部署、1保留hidden槽、44个group0记录）；真实选择取消取得后置结果0，再存及恢复；追加受控村民hidden活动档→正式CONTINUE／controller→story10完整ACK→败北出口，不用WAIT伪造可保存状态 |

工具先写fixture／preflight，加 `--run`才启动窗口。Windows实际命令形式如下；artifacts必须是不存在的新目录，三个scenario串行替换：

```powershell
& src/build-win/Debug/fd2_field_play_input_driver.exe run-job 900000 python -B tools/run_s3_stage_check.py --root . --sdl src/Debug/fd2sdl.exe --driver src/build-win/Debug/fd2_field_play_input_driver.exe --dosbox C:/Users/Soar/scoop/apps/dosbox-x/2026.10.01/dosbox-x.exe --save-fixture F:/fd2sdl-evidence/g5-post-20261111-0305/b7-victory-all-villagers/original/b7-stage2-entry.SAV --source-report F:/fd2sdl-evidence/g5-post-20261111-0305/b7-victory-all-villagers/original/comparison.json --build-dir src/build-s0-baseline --scenario stage2-turn3-storage --artifacts build/s3/native-turn3-new --run
```

Linux可运行来源工具及纯测试；本阶段Linux构建／原生未执行，非Windows WGC `--run`显式拒绝，不skip标绿。通用命令见[README](../../README.md)，来源／轨迹双平台参数见[冷定义工具](../tools/static-data.md)。

实际代表矩阵：turn3原／SDL各1进程、57／53条trace、52／51检查点；victory各2进程、88／77条trace、69／65检查点；stage12各2进程、89／72条trace、69／65检查点，story10各1 ACK。trace含采集观察，不能当成全部是真实按键。三组均sources_unchanged、owned正常清理／无强制残留、同线程display释放；stage12原runtime latch未转储，按完整源／实际输入／故事与出口／SAV证明，不伪造RAM轨迹。

每端保PID／HWND后台输入、WGC检查点、完整22987B存档及byte豁免掩码、程序／工具／来源摘要、owned Job清理和同线程display释放。原版fresh heap尾只允许明确未写域；SDL fresh须保完整DAT末尾至0x8a3的原SAV未知尾；Loaded两端验证完整原档，不延用fresh豁免。无前台抢占、桌面裁切、全局输入、电源计划或用户混音器修改。

## 5. 审查与未验边界

根及独立实审覆盖来源门、原caller／记录下界／身份、只读导出、父子owner、异常拒绝和工具判据；候选12的实现／适用验收无剩余S3阻塞。最终总门 `build/s3/final-gate.json` 记录全部实际收据及摘要，提交／本地集成另由后置 `local-integration.json` 记录。中途红件与工具失败在[阶段记录](stage-log.md)保原身份，不累计成最终一次全过。

未开放13槽及后续自然链、默认255、全随机同轨／全音画／全对象BSS／其它平台边界保留。S4负责统一历史回归、旧fade及总审查；本次不实施。
