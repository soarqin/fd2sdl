# 分阶段提交与验证记录

2026-11-05 用户修订推进规则：项目所需IDA Domain／idalib数据库操作不再逐次授权；B3及后续同级编号为最小交付／提交单元，阶段内部只测试自身新增、修改与直接受影响路径，大阶段结束再统一全面复测历史功能。完整 B3 至 B7 已按同等粒度分别交付，B0—B7 大阶段统一审查与历史回归已封存于 `8fdfb2f`；后续阶段另行登记，不在内部功能完成时停报或提交。下文旧微阶段划分、当时的授权等待和重复全量实机计划仅为历史记录，不再构成当前要求；已有提交及实际测试范围保持原样。

B1 已列控制场景和用户反馈后的自然开场／边缘镜头补充复验均已通过，累计验收及补跑边界见下文。结果与菜单合同、原版运行证据、未开放页面分别登记。结算属于 B5，完整第一关的玩法／视觉／声音验收属于 V1，不因核心合同通过而自动关闭。

| 阶段 | 提交 | 完成与验证 | 未覆盖范围 |
|---|---|---|---|
| Windows 原生开发与测试基础 | `946f989` | 从暂存候选独立构建；31/31 CTest、39/39 工具 Python，无跳过。含 WGC、真实输入、Job 超时与遗留子孙清理、移动取消、临时路径及 BGM 迭代器修复 | 原版标题／战场配对与 B0/B1 业务留在后续阶段；Linux 未执行 |
| B0 失败边界 | `d282503` | 从暂存候选独立构建；32/32 CTest、39/39 工具 Python，无跳过。首错停止、notice 容量、AI 分段提交、演出一次确认、停止状态保存拒绝与有效恢复 | 这是 SDL 支持边界回归，不等于原版整关玩法验收 |
| B1.1 结果与标题分派 | `542aeb6` | 从暂存候选独立构建；32/32 CTest、39/39 工具 Python，无跳过。胜／败、标题、宿主关闭与错误独立返回；START／LOAD／CONTINUE 分开 | 真实胜败结算、手工 LOAD 页面未实现；窗口配对留在后续阶段 |
| B1.2a 持久玩家构造 | `2148632` | 从暂存候选独立构建；32/32 CTest、39/39 工具 Python，无跳过。逐项复核原版 32 条 base 初值，覆盖精确 58 字节写集及 22 字节保留 | 这是静态表、已发布伪 C 与 SDL 回归证据；持久 owner／招募和原版运行分别后续验收 |
| B1.2b 菜单、END 与持久 owner | `3187c33` | 从暂存候选独立构建；33/33 CTest、39/39 已提交工具 Python，无跳过。修复首次保存、坏档保护、64／96／32 边界、metadata、独立整表 roster 与招募回滚；手工 Load 明确拒绝 | 原版肯定 END、新回合焦点、Load／leave 运行去向仍需 B1.3；march／辅助页／四槽入口仍限制 |
| B1 图形自动化工具 | `e2cb5ec` | 13 组 WGC 全部通过；独立暂存候选 93/93 Python，无跳过。原版标题／菜单严格资源比对、输入失败日志、隔离目录与 Job 清理 | 仅所列标题／取消菜单证据；肯定 END 与 Load／leave 分别进入 B1.3 |
| B1.3a 正向 END／Save | `7d1dfa6` | 独立暂存候选 33/33 CTest、107/107 Python，无跳过；真实 START 的原版／SDL END→round2→actor0→Save 配对通过，修复焦点与 Z 起点 | 不比较未对齐的随机结果、伤害、AI、相机；Load／leave 和剩余页面继续推进 |
| B1.3b 活动 Load | `1004d1c` | 两端 round2→Load 来源 A→turn1→显式 Save 配对通过；测量前内容与 mtime 均不变，候选 117/117 Python，无跳过 | 取消只验浏览／磁盘，不声明全部 runtime／RNG；leave 与 B1.4 独立推进 |
| 源码地址审计范围 | `f79dd3d`、`d294bf0` | 9/9 发布证据与地址回归通过；排除 src 内部 build／build-* 与第三方，真实源码保留检查 | 修正本地构建候选被误扫、重复统计和独立导出源根被误排除的问题；未改 IDA 字节基线 |
| B1.3b 离场与控制回归 | `c649883` | 独立候选 33/33 CTest、126/126 Python，无跳过；原版／SDL Esc、NO 与自然 exit0 通过，完整16组 WGC 复跑通过 | 精确对话布局、提示时序／声音仍待 V1／A5；B1.4 仍未完成 |
| B1.4a 字形基础 | `38c2bb0` | 独立候选 34/34 CTest、126/126 Python；受影响原版／SDL 菜单 WGC 配对通过，进程正常清理 | 原版总览字形 ROI 在 B1.4b 单列验收；Linux 未执行 |
| B1.4b1 总览纯绘制 | `b8f25fd` | 独立候选 34/34 CTest、126/126 Python；真实资源无窗口烟测核对 START 7／4／0、四底板、条件与数字；审查复用公共文本 span 校验并修复越界读取 | 尚未开放正式入口；原版页面 WGC、开合与任意键关闭在 B1.4b2 验收 |
| B1.4b2 总览正式输入与配对 | `400891a` | 独立候选35/35 CTest、135/135 Python，无跳过；完整17组WGC及六份原版配对通过，四区含条件文字／数字共25,289像素，四种键关闭、显式Save测量和宿主关闭通过；无强制清理或遗留 | 精确动画时长／瞬时帧、完整地图与声音未验；march、F1与四槽重建仍限制，B1未整体完成 |
| B1.4c1a AI路径pending | `833427a` | 全新暂存候选35/35 CTest、135/135 Python，无跳过；独立审查通过；修正后真实START的原版／SDL END→round2→Save回归通过，窗口正常清理 | 沿途特定格事件仅有静态指令和SDL合成回归，待原版运行；所有cell handler仍明确拒绝，玩家顺序在c1b修复，march未开放 |
| B1.4c1b 玩家controller与取消 | `214b76b` | 全新候选36/36 CTest、147/147 Python，无跳过；独立审查通过；原版／SDL一步移动取消、selection→COMMAND可达及返回browse配对通过，内容／mtime不变；受影响END／Save继续通过，所有窗口正常清理 | 仅图标presence（上14行／其他20行），完整runtime、特定handler与镜头未验；COMMAND actor遮挡和acted映色发现差异，单列下一视觉修正；总流程已注册18组，未将旧17组记录改为本轮完整通过 |
| B1.4c1 角色绘制修正 | `6b460e4` | 独立候选37/37 CTest、160/160 Python，无跳过；独立审查通过；真实 FDICON 全1,680帧 opaque0 审计、COMMAND 完整图标／actor 遮挡及满HP WAIT→acted→详情配对通过；完整19组 WGC、八份原版配对通过，文件与窗口清理检查通过 | raw26 抖动、受伤恢复、全详情页及完整runtime／RNG仍待对照；首次标题过渡黑帧失败及修正保留，未扩大像素容差；B1仍部分完成 |
| B1.4c2a 共享移动表与几何 | `3406782` | 全新暂存候选38/38 CTest、169/169 Python，无跳过；独立审查通过；纠正580字节表及175字节错位、原anchor／整长路径／对称tie／profile／起点ZOC，补非法起点预检；真实START四终点及END／Load／玩家取消／WAIT四组受影响WGC通过，文件未变、窗口正常清理 | 原版特定近敌ZOC／路径handler、完整机器执行与March运行仍未验；未开放March，未把旧19组记录冒充本轮全套；保留DOSBox窗口尺寸变化导致的失败现场 |
| B1.4c2b 批量行军 | `a777310` | C盘独立暂存候选39/39 CTest、189/189 Python，无跳过；独立审查通过；真实START原版先行／SDL后行配对验证取消、NO、肯定一次、整轮、四玩家终点及完整容器保留区；完整20组WGC、九份原版配对通过，无强制清理或遗留 | 特定handler、未对齐RNG／伤害／AI位置、完整动画／声音、长路径中途即时退出及延后剧情clean仍未验；B1仍部分完成 |
| B1.4d0 水色DAC前置 | `69bca9e` | C盘全新候选40/40 CTest、193/193 Python，无跳过；独立审查通过；严格LE93字节与全部16窗口/C常量审计、app视觉owner／Load保留／无追赶／纯绘制不推进回归通过；受影响原版／SDL总览WGC通过，文件未变、窗口正常清理 | 55ms为宿主近似，未覆盖全部剧情／通用对话／独立timed wait caller；完整水色RGB、F1冻结／关闭恢复的正式配对在下一阶段，不把静态审计记作动态验收 |
| B1.4d1 F1全场缩放图 | `fb165a2` | C盘全新候选41/41 CTest、214/214 Python，无跳过；独立审查并修复周期漂移跳过／rawRGB报告判据；四场景每端完整64,000像素、每页至少两个同标记相位、任意键关闭／焦点不变／actor0灰标覆盖及页内hostclose原版配对通过，文件未变且窗口正常清理 | 双端打开前水色周期不同，实际原始RGB并非相等，未归一或排除水；精确时序、首次undefined tick、held/repeat实机、音频及完整runtime/RNG仍未验；总流程注册21组，全套将在B1审查修复后统一跑 |
| B1总审：Load owner与图形资源 | `87b5db7` | C盘全新候选41/41 CTest、219/219 Python，无跳过；成功清轮询起点、失败全owner/RNG/文件保留；普通可见actor完整12帧门、hidden未知ID保留及共享胜败查询回归通过；新原版／SDL五次Z→Load→首次Z回raw0配对及原容器守卫通过，窗口正常清理 | 坏ID只做SDL拒绝测试，不声称原版畸形数据安全；取消完整runtime／RNG仍未验；必需提示、标题错误及四槽边界文档继续收口 |

| B1总审：必需系统提示错误 | `4b51200` | C盘全新暂存候选42/42 CTest、219/219 Python，无跳过；完整fragment预检先于绘制，正式prompt／取消／Save／Load结果失败返回ERROR；真实隔离存储验证保存已写和读档已恢复后不假回滚，成功Load清局部诊断计数 | 损坏资源为SDL错误边界，不声称原版同样安全；正常流程将在B1最终21组WGC统一复跑，未将旧20组当作本轮通过 |

| B1总审：标题必需资源错误 | `667b3b2` | C盘全新暂存候选43/43 CTest、219/219 Python，无跳过；标题ANI失败、调色板缺失／短、底图解码失败返回ERROR；8条私有TU路径区分正常、主动skip和hostquit，释放资源；旧实现4条错误反例失败 | 错误替身仅覆盖标题分支，不冒称整个损坏归档启动已实测；正常标题WGC将在最终全套复跑 |

| B1总审：删除March图形旧豁免 | `e2df4f6` | 独立暂存候选220/220 Python，无跳过；禁用March图被拒绝，全部已识别March opaque像素纳入双端比对，两条旧实现反例已复现；源码根查找精简为固定父目录 | 此处为oracle回归；更严格的真实原版／SDL配对将在最终21组WGC跑，不追改历史像素覆盖 |

| B1.4e 四槽owner研究与计划调整 | `94ee234` | 静态调用、严格LE fixup、原始局部指令、真实SAV和文本资源确认准备页／酒店owner；更正29620误名、独立2968d writer和picker第二参，生成204函数registry及最后SHA；10/10发布／地址回归通过；同步已开放March/F1使用说明 | 未执行新的机器码或原版准备页；可见四槽／持久同步／stage1重建与实际运行验收完整迁入B5，不以归属研究冒称UI完成；B1胜败可见去向仍待运行证据 |

| B1总审：详情资源错误传播 | `89466a5` | 独立候选220/220 Python；42项CTest首轮通过，真实输入因前台AkagiMS失焦停止，保留首轮后仅重跑该项通过，共43项已通过；6条详情解码／开合失败和2条合法无目标回归，失败不继续controller后置 | 首轮并非无失败整套通过；正常详情受影响WGC留最终总跑，片头必需资源错误另行修复 |

| B1总审：默认片头必需资源错误 | `b04bdb7` | 独立暂存候选43/43 CTest、220/220 Python单轮无跳过通过；标题私有TU扩为29例，默认片头调色板／初始及切入图／ANI失败首错停止，offscreen释放；旧片头15条漏门被反证，保留主动skip和hostquit来源 | 损坏资源用SDL私有TU，不冒称原版安全；正常片头与标题WGC留最终总跑 |

| B1／B2交界：AI无演出MP费用 | `3b23696` | 全新候选43/43 CTest、220/220 Python单轮无跳过；独立复审通过；依据1ca89真实写集和完整wrapper链纠正旧浅查结论，25个真实评分ID、多目标／首RNG观察／无效果仍扣／五种预检及后段失败保留回归；生成205函数registry/SHA | 这是静态／局部指令与SDL回归，不是原版施法运行；START首END静态无AI法术，不追改旧运行结论；FIGANI零／多费用标记和玩家施法另留B2／B6 |

| B1.3c 受控胜败原版结果门 | `8fe860f` | 全新候选43/43 CTest、231/231 Python无跳过；两端由同来源只改hidden位的精确manifest派生快照，原版controller取消后到达胜利首对白及败北frame0→GAME OVER frame1；SDL分别唯一结果log并自然exit2，4进程正常清理、文件内容／mtime和原版hash不变；208函数registry/SHA校验通过 | 受控终态不是整关实战；只验原版胜利首段／败北两帧和SDL独立结果分派，不验B5结算／回标题／持久同步；完整22组正串行执行 |

| B1终审：共享March菜单判据 | `119f2bb` | 170组资源组合复核判定与匹配数据一致；委托共享strict菜单，保留strict_parent报告标签和额外secondary关闭门；19项局部及最终231项Python通过，独立复审无阻塞 | 首轮曾移除测试依赖的常量导出而失败，已恢复并保留日志；完整22组使用此精简候选，尚在运行 |

| B1大阶段终审与验收 | `2035e02` | 冻结候选单轮43/43 CTest、231/231 Python、22/22 WGC（11项原版配对任务）通过，无跳过；已修复发现的问题并复审，合并共享胜败查询和March菜单判据；外层Job46160正常清理，无遗留／超时／强制终止，全部原版任务文件守卫通过 | 当前Windows与已声明stage0控制场景；受控胜败非整关实战，F1原始RGB水色未对齐，实际原版施法、B5结算／四槽准备／下一关、完整玩法视听、未知handler和Linux本轮未验；继续B2.1a |

| B2.1a 共享法术位图查询 | 本提交 | 独立fresh全构建与43/43 CTest（含真实窗口输入）单轮通过，10/10地址／发布审计通过；复审确认新helper函数体与旧AI私有实现逐字等价，真实AI caller复用；40位／NULL／容量0／截断／未知36..39、actor状态／MP及输出不变回归 | 正式MAGIC仍禁用；只完成共享纯查询，不冒称玩家列表／取消／目标／提交或原版施法通过；B1的231Python／22WGC是前一冻结基线，本次未重跑整套原版 |

独立候选及测试产物位于本地忽略目录 `build/commit-validation/` 的对应阶段子目录，含 `b1-font/`、`b1-overview-render/` 和 `b1-overview-modal/`，不提交资源、程序、截图或报告。每个候选由对应暂存树导出，使用独立构建目录和只读原版资源；本地 LE 测试证据仅复制到该验证目录以避免跳过，未发布新镜像。

`f79dd3d` 的提交说明将 9 项误写成 10 项；实际日志为 9/9，以上表为准。源码审计范围还须保留位于独立 build 候选根下的真实 src，只排除该 src 内的构建副本，避免审计空跑。

## 2026-11-04 用户反馈后的优先级调整

B2.1b 暂缓，先完成自然新游戏第一关引导交接和单位聚焦镜头对齐。已定位：开场尾缺原版方向复位／聚焦主角，Z 轮询只保证可见，普通左／上阈值提前一格卷动。历史 43 项 CTest、231 项 Python、22 组 WGC 记录仍有效于当时列明的夹具和控制场景，但不证明自然站位与边缘镜头。重新打开这部分 B1 验收；每项修复独立测试提交，再统一原版复验。整关实战、结算和下一关边界不变。

镜头坐标核心修复已完成独立候选验证：44/44 CTest（含真实输入）和 10/10 地址／发布检查通过；独立复审无阻塞。左／上改为严格 `<2`，Z、END、攻击取消和 March 复用私有逐格聚焦函数；保留地图边界和 same-target 例外。新回归覆盖精确阈值、四向离屏、轮询门、非法目标及整个 session／RNG 无副作用；旧实现被新断言拒绝。原回合夹具补有效地图尺寸，相机旧错误期望改为原版 `(6,13)`。这只是坐标核心与 SDL 回归，边缘 WGC 和自然开场运行仍在后续复验中，B1 尚未重新关闭。

镜头核心已提交 `1315142`。自然开场收尾另行修复 `08f5661`：TEXT2 后全体方向归零、原名义 20 ms 等待、复用已有逐格聚焦移到主角；不修改原移动脚本或用捕获坐标居中。新私有 TU 用真实资源完成 32→31→0，验证 12 名角色终态及已列资源失败与退出边界；最终预览期间退出不再导出 valid handoff。独立 fresh 45/45 CTest 和 10/10 地址／发布检查通过。

原版自然 START 的独立 WGC 校准已完成 117 个逐页资源门及正式 UI Save：初始 focus `(7,14)`、camera `(0,13)`，12 名角色方向全为 0；测量前仅向左一格，Save focus 为 `(6,14)`。该结果验证静态推导，但报告仍为 probe_only，不冒称 SDL 配对。源目录与原存档 mtime 未变，PID24792 正常退出，外 Job6484 无超时、遗留或强制清理。双端自然流程及边缘聚焦 WGC 仍在后续复验中。

边缘镜头配对工具已完成：14/14 专项 Python 通过；原版先行、SDL 后行的四场 WGC 全部通过。仅派生 actor0 坐标和 checksum，左上 `(2,5)` 对应 camera `(1,4)`，右下 `(19,21)` 对应 `(8,15)`；两块完整干燥地形 ROI 在 204 个相机候选中唯一识别结果，未新增水色排除。选择范围与 COMMAND 四个完整图标／角色遮挡、两级取消及内容／mtime 守卫通过；命令能力和默认选择差异明确不作一致性声明。四进程全部 exit0，外 Job18776 无超时、遗留或强制清理。独立复审无阻塞；不是自然开场或完整 runtime／RNG 对照。

自然正式配对首轮原版完成 117 页和 Save；SDL 在第一页头像门失败，未发送补键。文字 3,840 像素及框边 620 像素通过，底部 DATO 头像缺少原版水平镜像。两进程正常退出，外 Job23792 无超时、遗留或强制清理；本轮自然配对未通过，保留失败现场并修复后重跑，不扩大像素容差。

开场画面审查修复：按已有 `lmi_rle_blit_reverse @0x4ec31` 证据补底框头像镜像，上框保持正向；只处理独立解码图像。7 个真实 DATO 身份的 0／3 帧、上下框共 28 场全像素与范围外哨兵回归通过；frame3 由测试替身送入共同路径，不新增或冒称眨眼／RNG 调度。必需 FDOTHER[0] 调色板缺失／短于 768 字节时立即 ERROR、不呈现、不导出 handoff。两项均已独立复审，fresh 候选完整 45/45 CTest 通过；自然正式配对和 24 组 WGC 总流程已从此候选重跑，尚未据此登记通过。

开场资源复审进一步修复立绘解码错误吞没：沿已有私有对白调用链返回 ERROR，失败后不再呈现或导出 valid handoff；首个下框 ID0 与第二个上框 ID48 的独立 decoder 故障覆盖错误传播及档案回收。独立复审无阻塞，无新增缓存或状态层。含此修复的新 fresh 候选首轮 44 项 CTest 通过，真实输入因前台变为文件资源管理器而失败；保留首轮日志，仅重跑输入项通过，累计 45 项已通过，不记作单轮全绿。同候选 255/255 Python 通过，无跳过。

上一冻结候选的图形总流程在原版第 105 页资源门通过后，输入驱动 key 失败，未启动 SDL 或继续其他组；错误日志不足以确认根因。原版 PID52012 正常退出，内 Job57364／外 Job57852 无超时、遗留或强制清理。输入诊断随后追加 PID、退出码及 Windows handle／SendInput／wait 错误，不改注入顺序、保持时间、松键或失败即停规则。全部失败现场保留；新候选已重新串行启动总流程。

新候选首轮两端各 117 页及首次焦点／镜头均通过，但 SDL 的 Save 测量在次级菜单门超时：同来源 stage29 活动档在原版可 Load、SDL 不可导入，工具却固定要求普通 Load 图。生产限制正确，未修改；共享菜单判据新增显式 Load 能力参数，禁用时必须完整匹配 frame44 的 476 个不透明像素，不新增豁免。stage0 来源暂以已验证 START 完整明文 SHA 收窄工具前置，不复制 import codec；原版文件存在门与标题 stage `0xff` 门分别处理。新增正常／禁用、坏像素和来源边界回归，独立复审无阻塞；原失败内 Job46268／外 Job59064 正常清理，保留现场。

自然开场正式配对最终通过：冻结工具候选 257/257 Python 无跳过；179 份 `src/` 文件与前一已验证 C 候选逐份 SHA 一致，复用该 Windows 程序。两端各 117 个逐页资源 ACK、首次浏览及 UI Save 成功；12 名角色的 identity／side／position／direction／hidden／acted，以及 turn1、stage0、active12／roster4、resource0、camera `(0,13)`、首次 focus `(7,14)`／screen `(172,28)` 完全一致。测量只向左一格，Save focus `(6,14)`，manual slots 保留；未知字节／RNG／全场景像素／时序／声音仍明确排除。PID57560／17236 均 exit0，内 Job51232 无超时、遗留或强制清理，原版资源及存档 mtime 未变。总流程已将自然开场与边缘镜头放在最前，24 组整体回归尚在继续，B1 此时未重新关闭。

首次浏览整帧另作原始 RGB 诊断：64,000 像素中，通道容差 3 下有 22 点失败、最大差 4，仍为不通过。独立逐点复核确定全部位于信息板外的地图 world `(0,20)`／`(1,20)`／`(2,20)`，资源帧 98／97／99、索引 227／229／230／234；均属于已知 DAC224..239 水色窗口。严格 LE 周期表唯一解释为原版 phase7、SDL phase6，各自逐点 delta0。这是已登记的水色周期未同步范围，不是面板内容错误；不放宽容差或归一颜色，也不将本次站位／镜头通过升级为整帧 RGB 通过。

完整图形首轮 22 组通过，F1 第二场 SDL 等待原版 marker phase4 超时，第 24 结果组尚未运行。超时图仍完整匹配 phase1／water5，首帧为 phase3／water5；与目标的差异仅为 12 个标记的 108 像素，没有绘制损坏或水冻结故障证据，但当时没有逐次采样日志，不能断言原因。失败现场及外 Job2568／内 Job52628 的正常清理记录保留。随后仅修 F1 局部采样闭包，未命中按 17／43／79／131 ms 轮换错开观察；原 15 秒期限、完整 64,000 像素、水色漂移首错、游戏时钟和按键不变。fake-clock 回归复现固定采样漏 phase4、错开后在期限内命中，新增每次相位／目标／延时日志。

F1 专项 24/24、最新完整 260/260 Python 通过；实际四场双端均取得同原版两个 marker 相位，每帧全 64,000 像素验证。原版 12 次、SDL 43 次观察均记日志，PID23648／54236 正常退出，Job50752 无超时、遗留或强制清理。尚未执行的受控结果组另行补跑通过，Job9140 正常清理。累计 24 组已完成，不是单轮全绿；首轮失败 summary 不改写。13 项原版任务的 30 个进程均按预期退出／清理、文件守卫通过，汇总脚本已逐项读取收据并生成本地 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-alignment-20261104/acceptance.json`。

### 补充复验终审与恢复后续计划

本轮独立提交为：计划调整 `6a02174`，镜头核心 `1315142`，自然交接 `08f5661`，边缘配对 `94a727c`，底框头像／调色板 `0ee4a5e`，立绘错误传播 `284e4ce`，输入诊断 `ea82417`，自然流程优先与来源 Load 门 `b26756a`，F1 采样 `88375a1`。每项均按上述边界验证；独立复审发现的立绘资源错误已修复，没有遗留的确证阻塞。逐格聚焦合并重复循环，资源错误沿现有调用链传播，采样保持 F1 局部闭包；未引入新 session owner、缓存或通用框架。终审另去掉两份新工具的多余文件末空行。

Windows 复验累计 45 项 CTest、最新单轮 260 项 Python、累计 24 组 WGC 完成，原版配对 13 项及30进程收据均通过。原失败及补跑日志保持独立；Linux 未重跑，整关实战、结算／四槽准备／下一关、UI RNG、全动画／声音与原始水色 RGB 同步仍未验。关闭的是本次自然开场、镜头和已列控制场景的 B1 补充复验，恢复 B2.1b，正式 MAGIC 的选择／提交开放门保持。

### 后续工具审查：解析原版目录别名

B2 校准工具复审发现同类路径防护缺口：当候选树的 `original_game` 是 junction 时，未解析的路径比较可能允许把产物写入真实原版目录。现有原版入口统一先解析实际目录再判断，未改输入或像素门；两个纯模型逐入口验证原版目标子目录在 mkdir 前拒绝、外部隔离目录仍到达 mkdir。旧实现已被模型复现，创建操作全由 mock 阻止，没有实际原版写入。本轮所有实机产物路径本来均安全，既有文件守卫与运行证据不因此失效。

### B2.1b1 原版列表校准

B1终审 `0842f05` 后继续此阶段，路径防护另提交 `38e3e3e`。永久工具复用既有资源／输入／WGC／清理判据，不依赖忽略研究目录；9/9专项、2/2逐入口路径回归及10/10发布审计通过。原版真实START raw1逐键到达火炎术0列表，Esc回COMMAND再重开：底板30,690、姓名561、MP352，共31,603像素矩形均匹配，重开前后矩形差异0；当前／最大MP8/8、费用02、选中0。副本bytes／mtime与原版hash不变，Job28244正常清理。工具保留 `probe_only=true`／`ok=false`，以 `probe_cases_passed=true` 表示原版校准成功，未加入B1的24组配对套件。

语义登记同步加入 `field_unit_magic_list_draw @0x1ceed`（原sub_1CEED）与 `field_player_magic_list_select_step @0x1d51d`（原sub_1D51D）。完整259／427字节、xref及资源相互支持：前者仅叠文字／费用，后者每次返回0继续、-1取消、1确认，索引留53C57，完整modal由1cff0负责。命名表与registry由CORRECTIONS生成至210项，最后刷新SHA；复用已发布IDA输入，原伪C物理符号和类型不改，未新建／写入数据库或重新反编译，不能将此登记声称为新的ABI验证。全部导航和低MP门仍是静态证据，运行只覆盖上述首项；目标／施法、SDL输入、UI RNG及acted／pending完整测量未完成。接着验证B2.1b2纯绘制，正式MAGIC继续禁用。

### B2.1b2 纯列表绘制

原版校准工具与语义登记已提交 `8869a5b`。本阶段沿现有 `field_info` 资源owner增加20×8 raw frame92，复用40bit法术查询、7字节record费用、现有字模／文本与bank42两位数字；完整预检后才恢复下半空白底板并叠名称／费用。支持0..12项、selected=-1及合法空名31，未知36..39、13项、非法选择和末项坏文本／字模均在写屏前拒绝。12项是当前SDL可见支持边界，不声称原版只有12项；清底板是现有详情合并装备后的SDL整合，不归给native1ceed。单位／输入／调色板不变，没有新增session状态、缓存或框架。

独立审查无阻塞，并删除已确认费用最大80时不可达的溢出图资源预检；其它消费者的资源owner保持。新真实资源／合成回归覆盖frame92完整160字节及失败释放、12项完整画布与选中映色、空列表／空名／原始0像素标签、错误整VGA不变和unit不变。新隔离源码仅4份src文件相对B1候选变化，fresh完整Windows构建、单轮45/45 CTest（含真实窗口输入）及271/271 Python无跳过通过，产物在 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-alignment-20261104/b2-render/`。

正式MAGIC仍禁用，本阶段没有SDL列表输入循环或WGC配对；B1的24组为此前冻结候选证据，未冒称本次重跑。下一小阶段接列表选择／逐级取消，目标／提交和实际伤害／恢复另验；原版UI RNG、完整取消owner、多个法术列的实际运行尚未验收。

### B2.1b3a 单次列表输入

纯绘制已提交 `f541bbb`。共享 `field_magic` 新增单次选择逻辑，对应 `field_player_magic_list_select_step @0x1d51d`：上下整表回绕，左右按4项跨列且边界不动；确认只在当前MP达到record费用时返回1并输出法术ID，取消返回-1，其余返回0。SDL非法参数／未知record另返回-2，在改索引或输出前完整检查40bit列表；查询层可处理36项已知法术，绘制层12项支持门保持独立。

独立复审无阻塞；新回归覆盖稀疏ID、1／4／5／8／12／36项、MP不足／恰等／上限、未知后项、非法输入及单位／输出无副作用。仅导出Git暂存内容的隔离候选完成fresh Windows构建、单轮46/46 CTest（含真实输入）和271/271 Python，无跳过，日志位于 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b2-20261105/selector/`。本阶段不包含UI等待、头像闪烁或SDL列表WGC；正式MAGIC仍禁用。后续真实等待必须复用原版共享RNG消费，不能把本纯函数不耗RNG扩展为整个取消流程不耗RNG。

### B2.1b3 原版输入与取消校准

单次输入核心已提交 `f432d4f`。新增原版controls场景仅在独立START副本修改raw1的五字节法术位图和checksum，能力为0／1／2／3／13，保留MP8/8；完整明文／密文SHA及三个差异偏移写入manifest，不作为自然学习证据。17项专项与2项路径防护回归通过，独立审查后实际WGC运行通过13个导航检查点，覆盖上下回绕、左右跨列和部分列边界；费用20的ID2用Enter／Space均停留。选择ID13后以frame1光标、实际角色及范围LUT识别目标页，Esc重开选中0的完整31,603像素列表，随后逐级回COMMAND／移动选择／browse，并执行正式UI Save作严格测量。

本地证据为 `C:/Users/Soar/AppData/Local/Temp/fd2-magic-controls-original-20261105-01/comparison.json`；33个检查点、36条输入记录与保存结果均通过，外Job9620正常退出、无超时／遗留／强制清理。原版目录未变，显式Save前副本bytes／mtime未变，完成后保存文件与已验结果一致；未经允许的未知字节变化仍拒绝。此项为原版单端校准，不增加B1的24组配对计数；SDL列表输入、实际施法、原版UI RNG计数与精确时序继续分别验收。

### B2.1b3b 列表session与取消边界

原版controls校准已提交 `8ad4b80`。SDL追加独立MAGIC_LIST及临时选项索引，begin预检整表／12项画面能力、玩家状态与原版禁止状态；低MP仍允许打开。step保留列表直到宿主关闭动画，end仅回COMMAND并选择首个已开放命令，不调用移动回滚或controller后置；当前正式MAGIC继续禁用。成功Load清索引，失败Load保留全部session，索引不进入SAV。

连接调用者复核发现18890的移动后禁法门：raw `18a38..18a7c` 在非零且非ff路径后，unit ID不为18／19／34时把同一disabled[1]置1。已纠正旧四步直接施法预研，列表公共状态检查加入该门；保留临时移动的合成回归明确使用例外ID18，另测各例外／相邻ID及角色9零步与移动差异，不暗改原版能力。初版候选只完成构建即被此发现取代，未作验收。

独立复审后，修正候选 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b2-20261105/list-session-fixed/` fresh完整Windows构建及单轮47/47 CTest通过（含真实输入）。新状态回归检查整session、底层path/range、单位、pending、镜头和RNG不变，以及取消first-enabled双分支、错误与Load边界。此项尚无SDL列表WGC或正式入口；UI真实等待／目标／提交继续下一小阶段，不将本纯状态RNG断言推广到原版等待。

### B2.1b3c 列表呈现与等待

列表session已提交 `ccf4b88`。play owner新增私有同步modal，借用现info／字体／文本，局部持有两张像素缓冲及DATO头像0／3；先统一检查core状态与全部必需资源，再开11→0、关0→11。关闭使用原版最初未选中页；确认ID到正常关闭完成才输出，宿主退出优先，不标acted、不扣MP、不处理pending。原版16c57每次等待入口先消费共享RNG，110ms步按postdecrement眨眼／复位，未知键也开启下一等待；保留当前呈现帧与500／92ms repeat适配，不声称复制BIOS FIFO或DOS墙钟。无键时保持水色更新。

复审修正了单项列表Up／Down仍须播放原版sample0，以及无效actor必须在首RNG前由core统一拒绝；没有引入新输入框架或UI随机流。无窗口私有TU使用真实资源、替换宿主时钟／呈现，验证导航、三种确认、低MP／OTHER、单项声音、全部开合相位、等待／眨眼消费边界、退出优先、坏资源及分配／头像释放。新隔离候选 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b2-20261105/list-play/` fresh完整Windows构建、单轮48/48 CTest通过（含真实输入）；新增modal／测试未报编译警告，完整构建仍有既存模块／第三方和Temp目录警告。正式循环暂未调用该modal，MAGIC仍禁用；此项没有SDL列表WGC，不增加B1冻结24组证据。接着实现普通0／13目标及提交门。

### B2.1c 普通法术目标核心

列表modal已提交 `34c93b3`。新增独立MAGIC_TARGETING，仅开放普通area0的ID0／13；cast范围复用14818的profile0传播，按原filter和raw顺序列出非hidden单位，不加HP／acted或占格门。入场选首槽，Z／小键盘5先递增再回绕，unit121保留槽位而只跳过聚焦；不复用browse轮换索引。方向可离开castmask，合法查询要求当前格在范围内且同格有符合filter的目标；无候选仍进入。目标页用FDOTHER[1]frame1及既有范围LUT，取消逐格聚焦施法者再回LIST第0项，保留临时移动／pending。

新增独立法术target输入上下文，Esc取消与Z轮换分别映射，普通移动／攻击上下文保持。真实扫描码回归首轮误用SDL的KP_PERIOD枚举，导致输入项失败；改为项目实际映射的KP_DECIMAL后仅补跑该项通过，生产输入未为该失败调整。隔离候选 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b2-20261105/magic-target/` 完整Windows构建及累计49项CTest通过：首轮48通过，input补跑1通过，不称单轮全绿；原失败和补跑manifest保留。10项地址／发布审计通过。新增回归覆盖范围外、空目标、隐藏／HP0／acted、121与raw轮换、错误无副作用、远处取消镜头、Load及frame1／LUT。

规范命名输入补登记18890移动后禁法门及115b6的局部轮询证据，由现已发布IDA输入生成210项registry／命名表，最后刷新清单；没有新建／写入数据库、改伪C类型或重新作ABI验证。此阶段没有正式施法或SDL法术WGC配对，提交仍未接入，MAGIC继续禁用。

### B2.2a 原版受控伤害校准

目标核心已提交 `73bb9e4`。原版新damage场景只派生真实START的raw1坐标 `(10,15)→(7,16)` 及checksum，明确不是游戏内移动或自然第一回合可达证明；HP／MP／EX／能力／roster保持。原地COMMAND后ID0唯一目标为raw11 `(5,19)`，mode2／frame1与576像素范围资源门通过，再仅发送一次施法确认。随后纯观察，不用跳过键或自动重抽；击杀要求caster acted、完整720像素目标空地与浏览框状态共同成立，miss显式失败。五步逐帧资源ACK返回caster，F2的姓名／MP6/8／LV1／EX42共1105像素通过。

24项专项及2项路径回归通过，另一agent独立只读审查并重放旧controls资源门无阻塞。原版实际证据 `C:/Users/Soar/AppData/Local/Temp/fd2-magic-damage-original-20261105-01/comparison.json` 为probe_only，cases通过；PID41228正常退出，外Job22996无超时／遗留／强制清理。没有END或Save，原版目录、副本与来源bytes／mtime保持。本项不读取runtime hidden字节、未对齐RNG／FIGANI／声音，也未启动SDL，不能增加双端配对计数；自然多回合施法、治疗及SDL对应效果继续后续验收。

### B2.2b 基础效果与非升级经验准备

原版受控伤害工具已提交 `1f4d685`。本阶段仅扩纯接口：ID0／13继续复用原伤害／恢复及shared RNG；新增结果记录HP前后、hit、未夹HP的rawDamage及原始经验贡献，不扣MP或写caster EX。伤害死亡给完整目标level×enemy系数，未死按rawDamage／maxHP；治疗按实际恢复差与目标权重计算，满HP仍消费一次RNG。caster归一只在整次目标贡献累加后执行，profile>8加30、cap99；未知系数、坏数据、溢出和可能EX升级在动作提交前拒绝，不把非升级子集冒充B4全实现。

敌军经验系数补入现有68条基础record的末字段，没有第二张表。strictLE提取VA61af9/file7ab0d完整680字节，逐条验证旧9字节和新增10字节，SHA `bc304be826fe3ff0ff129155da846126350852c9b67c6b08a2d7c2d103cc99df`。实现注释与CORRECTIONS同步登记1c81f／1c916／4e84f，1e292只登记已确认非升级分支；从已发布IDA输入生成214项registry并最后刷新清单，未写数据库、猜ABI或手改伪C。

独立复审无阻塞；28000组profile／HP／spread组合验证既有效果、RNG与可达紧上界，另测overkill／miss、实际恢复、目标／caster的8／9／24／25权重差异、等值levelcap、cap99、EX边界、错误输出不变和整次统一归一。隔离候选 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b2-20261105/magic-reward/` fresh完整Windows构建、单轮50/50 CTest（含真实输入）及286/286 Python无跳过通过。正式MAGIC仍禁用，本阶段没有SDL施法WGC；FIGANI／488奖励提示／附加呈现RNG、升级成长及自然多回合路径保持未验。

### B2.2c 玩家基础法术提交核心

效果／奖励准备已提交 `2c61eed`。新提交接口从独立MAGIC_TARGETING重建castmask、重新检查当前法术／MP，按同格raw顺序预检全部目标、基础效果、总贡献上界及非升级门。当前能力限stage0的0（caster unit9已有单费用标记证据）／13，完整列表含其它ID时不开放；目标或已见HP0携带非ff奖励描述符保守拒绝，不静默丢掉落／脚本。最终格与有效pending、即将进入side1的未知回合action及notice容量均先检查，之后才扣一次MP并消费效果RNG；miss仍扣费／行动，错误不退款或重试。

收尾沿现私有函数以WAIT／PHYSICAL／MAGIC小枚举分支，避免法术误走原地恢复或物理全方向复位；效果／败亡后acted→caster归一→最终lookup1→非升级EX写回→controller。死亡清理同时补齐1db65可观察方向终态：扩展视区内一个非hidden HP0触发全体非hidden HP0方向0，最后全部HP0 flags精确1；无见证者不改方向，hidden方向保留。现五类caller共用，未增加magic专用重复清理或持久奖励owner，帧演出仍留B6。

独立审查修复了另一个提交前预测遗漏：非目标HP0玩家将在败亡清理后隐藏，不能算作仍能行动而绕过side1未知事件预检；13同格、当时非hidden的HP0目标恢复后仍须算可行动。13565本体继续不加HP门。能力查询另要求真实共享RNG可用。回归覆盖三种预测路径、kill42／miss／非致死经验97、同格多目标与混合raw顺序／cap99、满HP／自身恢复、费用时点、17类拒绝、过期mask、异步phase、提交后首错冻结与重复拒绝，以及七组死亡视区边界。

隔离候选 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b2-20261105/magic-commit/` fresh完整Windows构建、单轮51/51 CTest（含真实输入）和10/10地址／发布审计通过。正式菜单仍禁用，这不是SDL施法WGC；奖励488提示、FIGANI／额外呈现RNG、升级、带奖励目标、自然多回合与整关仍未验。接着连接正式输入，并与原版damage／heal逐项配对，不因本次核心完成关闭整个B2。

### B2 接线复验发现的装备详情坐标修复

受控治疗的 F2 对照促使复查 `field_unit_detail_equipment_draw @0x184c0`：属性图标应为 `(110+150列,107+22行)`，bank42 三位数应为 `(135+150列,107+22行)`。修正旧图标偏左／上各2像素、数值偏左5／上2像素；名字、分类图、资源owner和透明规则保持。新增真实资源回归用6个可见物品及空槽覆盖两列、第四行和装备标志，逐像素比较完整310×99底部区域，断言单位记录不变；合成库存不冒称真实START库存。

独立复审无阻塞，隔离暂存候选 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b2-20261105/equipment-layout/` 构建和1/1 `field_info` CTest通过，10/10发布审计及11项SHA复核通过。命名输入补入原版指令坐标证据并由生成器更新登记。此项未作新的库存WGC配对；frame41占位等库存边界仍留B3.1。治疗配对同时暴露F2详情的Esc归属与法术第二页缺口，另项修复并重新验收，不将本坐标修复当作治疗配对通过。

### B2.2d 原版恢复校准与双端工具

提交核心 `306b376` 后，工具增加heal场景：仅派生真实START raw0 HP42→20、raw1增加ID13及checksum，其余字段保持；原版WGC列表／目标取消与raw0→1→2→3→0轮换通过，确认一次后HP42/42，再按资源ACK返回caster，MP5/8、LV1、EX20通过。证据 `C:/Users/Soar/AppData/Local/Temp/fd2-magic-heal-original-20261105-01/comparison.json` 为原版单端成功；PID23348与外Job40800正常清理，无超时、遗留或强制关闭，没有Save／END，原版和副本bytes／mtime守卫通过。

同一入口可选 `--sdl`，严格原版结束清理后再启动SDL，逐端最终文件守卫参与状态；独立故障模型复现并修复了“原版副本mtime异常只写诊断、仍继续SDL”的收据缺陷。37项法术工具、2项路径回归以及完整299项Python单轮通过。冻结候选 `magic-formal` 的单项列表配对02与伤害配对01通过；列表配对01的原版标题输入未到达检查点，失败现场保留，未弱化门或追加盲键。治疗配对01原版通过，SDL已达到HP42/42，但F2详情Esc未关闭而失败，另行修复后复验；这些是工具与故障发现记录，未据此开放正式MAGIC或宣布B2完成。全部运行使用WGC及正常串行清理；FIGANI、完整RNG／时序／声音、自然学习和多回合路径仍未验。

### B2.2e 详情翻页对照与关闭证据补强

正式连线候选 `magic-detail-fix` 完成fresh Windows构建及单轮52/52 CTest。治疗配对02的两端均达到recipient HP42/42、caster MP5/8／LV1／EX20，并以Esc翻到完整30,690像素未选中法术页，再关闭；各端文件守卫通过，外Job39604正常退出、无超时／遗留／强制清理。此记录修复了配对01的Esc阻塞，未据此关闭整个B2。

工具审查随后用真实DATO 0／3及边框资源证明：旧关闭门可能在18409的phase2提前接受仍有下板的画面。新门预检完整静态无水、无actor／overlay地形，并核对576像素；三个纯资源回归拒绝全部12相位及坏角落，仅最终恢复背景通过，耗时13.415秒。该加强门尚待下一完整WGC复验，配对02不追改为已用新门。

完整Python首轮300项出现同一故障模型的两个subguard失败：隔离source的original_game为junction，mock仍比较表面路径，fixture bytes／mtime故障没有实际注入。测试改为与runner相同的resolved parent；原目录和junction布局下hash／bytes／mtime三个subguard均通过，38项专项、2项路径回归通过，没有放宽生产守卫。新验收树与52项CTest候选的186份src文件完全相同；但继续复审发现1a866中毒／状态归零聚焦前置遗漏，最终整套验收先暂停并另项修复，不能将这个候选称为最终通过。

### 图形复测中的B1取消恢复判据修复

最新C2候选 `magic-phase-fix` 已完成fresh Windows 52/52 CTest、303/303 Python单轮通过。整套WGC首次在input因外部前台AkagiMS失焦中止，自身窗口和外Job13120均正常清理；自然开场和边缘镜头已通过。续跑严格核对同一程序SHA `71357736215b0e411113ff084051a73b6110d010cabbffaa2a2f18e5bdbbd69a`、冻结源码清单及原版身份后复用这两项；input补跑42.48秒通过，随后14项完成，在原版NO取消的恢复门停止。两次失败均保留原summary／日志／PNG，没有追加盲键或追改通过。

原版实际已回browse：204个光标像素通过，菜单／按钮／412提示均消失。旧恢复框 `(16,184,32,196)` 的最后一行y195越过frame130面板，13个差异全部来自地图tile49的DAC224..239；基准／超时图分别精确符合合法水周期4／13。改用面板内 `(16,190,32,195)` 的80个纯静态边框像素，同时匹配原资源及本进程baseline，保留容差3与原消失门，另明确拒412。17项专项及15项共享END工具回归通过；80像素逐个损坏、坏资源／baseline／光标和YES／NO／412都拒绝，没有新增解码器或地图扫描框架。

修后工具以独立SHA绑定到同一C2受验程序，尾部复验路径为 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b2-20261105/magic-phase-fix/build/graphics-artifacts/20261105-025247-048986/`；原版／SDL战场菜单配对已通过，两端正常清理，原版和夹具不变。后续原版场景仍在此路径依次执行；累计报告逐项标继承14项和新执行13项，记录两次失败与工具修正来源，不称单轮27项通过。完整RGB水色周期同步仍未关闭，本修复仅恢复完成判据的资源边界。

### F1重复采样复审与最终图形收口

菜单恢复门已提交 `246c5b9`。后续同一C2累计22项通过，F1原版完成而SDL等待native phase2超时；逐图离线确认超时图仍是合法phase3／水15、全64,000像素通过。SDL首帧其实已完整取得所需phase2，之后也取得phase4，工具丢掉首帧又重复等2。仅将本页已通过完整门、属于所需集合的首帧收入结果，随后只等缺项；native分支、完整像素／水色锁、15秒期限、键和清理均不变。25项专项及独立复审通过，覆盖坏首帧不能校准、first0／2、首帧不在目标集合及后续漂移硬失败。

补跑包装第一次因F1审计依赖根目录误指而在打开游戏前拒绝，processes为空；修正启动器仅绑定冻结依赖根，判据函数SHA保持，失败也保留。最终 `.../magic-phase-fix/build/graphics-artifacts/20261105-032737-397257/summary.json` 累计27/27通过：22项继承实际已通过收据，5项本次执行，完整记录5次失败／预检和逐项来源。16项原版任务的36条进程收据及10条SDL宿主收据均正常清理；外Job14900 `worker_exit=0 timed_out=0 residual=0 forced=0 cleaned=1`，只读前台记录器也正常exit0，无窗口、截图或焦点控制。程序／冻结源码／工具SHA与原版守卫均通过，不称单轮27全绿。

最新F1四页各自完整资源门通过；实际两端水周期不同，原始RGB仍不等：每个同标记相位有466／1085／1085／806个像素超容差，最大差4／8／12／12，未颜色归一或排水。这关闭的是相位证据采集冗余与本轮列明图形场景，不能关闭水色周期同步、整关实战、结算或下一关。

### B2.2e正式基础法术与受支持边界终审

F1采样修复已提交 `815f96e`。正式COMMAND以同一能力门连接MAGIC_LIST→MAGIC_TARGETING→一次效果提交，目标取消先聚焦caster并重开列表0，列表取消才回first-enabled COMMAND；modal结束耗尽本帧键，不将同一次确认用于下一步。正式门限stage0全列表0（caster9）／13，空表／禁止状态／移动后禁法按原入口处理，低MP只阻止确认。未知结算／可能升级／奖励描述符及必经未知事件在费用前拒绝；已提交后错误冻结，不退款／重试。ITEM保持禁用。

同一C2图形候选最终三组法术配对均通过：列表31,603像素、MP8/8与重开相等；伤害caster MP6/8／LV1／EX42；治疗recipient HP42/42、caster MP5/8／LV1／EX20、目标取消与raw轮换、未选中法术详情30,690像素及最终背景恢复576像素。各端final guards通过，仅一次施法，无Save／END，夹具／来源／原版不变。整套为上节27组累计收口，非旧B1冻结24组或原版-only校准冒充新增配对。

详情owner补齐任意键的装备→法术→关闭、Enter仅最终关闭后controller一次／F2不controller、两页资源预检及Load释放。终审还修复1a866两段原版顺序：每个中毒actor扣HP后聚焦（HP0／damage0也走），统一败亡清理；随后每个状态byte归零即聚焦并重算，删除聚合重算。439B完整原码SHA与xref独立复核，turn新回归使用旧代码确实在镜头断言失败；新代码验证跨round／export方向、raw顺序和提交后首错停止。专门毒伤的原版运行、提示／等待及其RNG尚未对照。

fresh Windows单轮52/52 CTest通过；菜单修复后完整307/307 Python通过，F1修后25项专项另通过。独立复审无剩余确定阻塞；上页资源预检提取给详情／列表共用，复用原owner、范围和随机流，未加通用事务／UI框架或重复资源缓存。整个B2的其它法术、升级／掉落、完整FIGANI／488提示／随机流／声音，以及整关实战、结算和下一关仍未验；继续B3.1纯库存查询／选择，正式物品入口保持门限。

### B2正式提交与默认图形套件登记

正式基础法术及审查修复已独立提交 `b20dc53`；提交前逐份比对暂存区186份src与受验C2冻结源码，差异0，未混入下一B3库存实现。默认图形总流程在原24组之后追加列表／伤害／治疗3组，传同一受验程序、driver和原版路径，保持自然开场／镜头前置优先；注册顺序／参数34项模型测试通过，完整307项Python已包含这部分。前述菜单／F1工具修复分别独立提交，原始失败不覆盖。

本地自包含复核页 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b2-20261105/review-report/index.html` 内嵌18个同名检查点的36张WGC PNG，图、角色PID、程序及来源SHA均校验；明确累计27=22继承+5本次及5条失败摘录，完整RGB／时序等边界保持。该页仅用最终C2证据，未拿旧C1补图；源码生成器、manifest与图留本地不提交。B3.1后续仅纯库存查询／选择，正式ITEM仍未开放，另行注册／验证／提交。

### B3.1共享库存计数与纯选择

B2正式入口 `b20dc53`、默认图形登记 `6603241` 后，继续此独立小阶段。`field_item`增加共享8槽计数，AI删除同体私有实现并替唯一caller；位80、装备位、未知ID和空洞的原count行为保持。新增单次选择仅导航改raw索引，上下回绕、左右跨4项；确认模式非0只查record+0d非0，允许已知非零code16等，不把handler支持表混为可选门。稀疏前缀／未知ID在任何导航前以-2拒绝，不压实、不改stale ID，取消／错误／其他键均保持单位全记录。

原1b8a6完整65B与1b9de完整430B、调用者及23字节物品表相互支持，独立strictLE原码与SHA复核；后者登记generic `field_unit_inventory_select_step`，不误名完整modal或Use专属，发布命名由CORRECTIONS生成216项并最后校验11项SHA。没有新建／写入IDA数据库、改伪C物理类型或ABI。两个独立审查均无阻塞；保留既有模块与资源表，删除AI重复计数，未加框架／缓存。

新回归覆盖256占用mask、256flag、1／4／5／8项导航、非零Use模式、空／坏索引／未知215..255／内部空洞和所有字节不变。START样例只用已确认raw0三槽字节及真实静态表，不直接读取SAV、不运行原版。独立1项目标与AI TU编译先通过，最终隔离候选 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b3-20261105/item-select-acceptance/` fresh完整Windows构建、单轮53/53 CTest（含42.87秒真实输入）及308/308 Python无跳过通过；10项发布审计和455个规范地址检查通过。

C3程序SHA `37ca0f8feff67baeb0a6cc03a01e09a1a0f680c7f121bb9c4e254b13e266b372` 与B2图形候选7135分开登记：本小阶段没有新增物品UI或原版配对，不将前述27组冒充C3整套重跑。正式ITEM仍禁用，B3整体未完成；下一B3.2先校准原版子菜单／Use列表与取消，再接SDL。B1整关、B2其余法术及B4/B5/B6边界保持，暂无需用户选择的等优决策。

### B3.2a原版物品子菜单与Use列表校准

承接B3.1独立提交`66e69ea`，先确认完整1b932 modal及1bbdc→18d8c→18890的0／-1返回链。新增generic `field_unit_inventory_select`，172B原码SHA与三个调用者支持bool返回、归0索引及三缓冲释放；不改发布物理类型或写入IDA数据库。完整1bbdc及xref确认Use／Give／Equip／Drop、仅Give的相邻候选门、目标取消和53C53的Give专属写入。由CORRECTIONS生成命名与发布清单，不手改生成文件。

新增工具复用现有WGC／资源门／owned-PID及Job helper，从严格LE读取object3 `602ad`完整4,945B物品表，SHA `bb64df69e73dfc4d642d342aac7bad8d7c2f4fb0527bb55d35c153b9f79432ca`；未用SDL内置表作为oracle。14项工具回归及输出路径守卫专项通过，独立审查无阻塞；没有新建场景框架或复制法术runner。

Windows原版首轮运行 `C:/Users/Soar/AppData/Local/Temp/fd2-item-list-original-20261105-01/comparison.json` 通过：真实START raw0原三槽、不改坐标，28个实际检查点及1条白名单WGC过渡帧重试，30条输入含3次启动Esc。四向ITEM图标、禁用Give、完整31,955像素列表、code0 Enter／Space拒绝、导航回绕、取消与重开均有资源ACK。没有确认草药或执行Save／END，来源、夹具和副本内容／mtime守卫均通过。DOSBox PID40348正常exit0，外Job18696 `worker_exit=0 timed_out=0 residual=0 forced=0 cleaned=1 result=0`。

报告保持`ok=false/probe_only=true/probe_cases_passed=true`，这是原版校准，不是SDL配对，不增加B2冻结候选累计27组。实际Use／目标／效果、Give／Equip／Drop、完整UI随机流／时序／声音、整关／结算／下一关未验。继续B3.2b纯绘制和B3.2c会话／等待；正式双端配对与B3.3完整支持候选合并执行，避免为中间层增加生产测试入口。正式ITEM仍禁用，B3整体尚未完成，当前无等优选项需用户决定。

### B3.2b纯物品列表绘制

原版校准已独立提交`fb6f9bc`，217项命名、10项发布审计通过。本小阶段在现info资源owner增加`fd2_field_item_list_draw`：先完整预检8槽、名称与尺寸，再恢复下板及绘制；选中用raw槽，显示按可见项压行，保留隐藏stale ID和合法空名称。补齐无主属性的frame41及raw59..67的完整复制，不新增缓存或UI框架；详情复用同一绘制代码，删除重复库存循环。格式与命名证据同步由生成器发布。

独立审查无阻塞，覆盖1／4／5／8项、raw2／7空洞、全部215条及27项fallback、数值边界、晚坏项与失败全VGA／单位不变。曾发现库存预检失败阻止MAGIC合法上页，已改为只跳过坏下页，MAGIC随后整板覆盖；新增跨MAGIC故障回归，旧实现实际构建运行在该断言失败，新版通过，不扩原void详情API的原子性承诺。

隔离暂存候选 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b3-20261105/item-render-acceptance/` 完整fresh Windows构建、单轮53/53 CTest（真实输入42.83秒）及322/322 Python无跳过通过，10项发布审计及冻结源码455个规范地址检查通过；程序SHA `babf2c77a9c9cabaebf7444e791dee0584633b8c0dbe3d9f7c3ee34d6c22c7bc`。冻结只含本小阶段代码，不混入正在开发的session／HP helper；原有Temp目录与历史转换警告未抑制。此处没有SDL物品WGC或新增双端组，正式ITEM仍禁用；继续共享HP小函数及B3.2c会话／真实等待，完整图形配对归B3.2d/B3.3候选。

### B3.3前置：共享按power恢复HP的最小函数

B3.2b已提交`c284b5d`。草药192的power40与法术ID13的power70不同，不能借法术ID替代物品value。沿原`1c8ed/211a4→1c916`，在既有field_magic提取纯HP helper，由MAGIC及AI item code5／13实际复用，删除AI手写公式；仅HP写入、每目标一次spread，满HP／power0仍消费。费用、贡献、MP／EX、压槽、acted与各自尾链保持caller所有权，没有新模块或结果缓存。原函数的全局贡献累计仍由已有法术结果路径处理，不把纯HP API冒充完整native函数。

独立暂存候选 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b3-20261105/hp-helper-acceptance/` 只缓存AI末尾替换hunk及helper／测试，不含在开发的session struct。全新编译所有core和四个相关target，`field_magic_reward/field_ai/field_ai_commit/field_magic_commit`单轮4/4通过；10项发布审计与冻结源码459个规范地址通过。4,200组HP／power／spread覆盖全记录仅HP变化、空参无RNG、溢出与夹顶；AI5／13覆盖满HP仍耗首roll、raw目标顺序、费用与消耗差异、重复提交拒绝，以及提交后事件失败保留HP46／压槽／acted／RNG1并冻结。原有转换／Temp警告保留，无新增helper警告。

这是共享计算整理，不是新的完整CTest／Python或图形轮次；此前53／322及27组图形候选继续保留各自来源。原版草药的实际使用、玩家提交／目标和正式ITEM仍待后续验证；B3.2c拆为session与真实等待两个可独立提交的小阶段，不因单层完成关闭B3。

### B3.2c1物品子菜单／列表session

共享HP前置已独立提交`a3de439`。本小阶段追加ITEM_MENU／ITEM_LIST与临时raw槽索引；子菜单复用command的四向、高亮及动画owner，能力表为SDL的`[0,1,1,1]`，不冒称原版START的`[0,1,0,0]`。列表step只报告是否可确认，不提交物品。取消先关闭当前层：LIST→ITEM_MENU0→COMMAND首可用项，COMMAND再取消才回滚移动；下级不改单位、路径、镜头、pending或RNG。物品不套MP、+27禁法或移动后unit例外门。成功Load清临时索引，失败保持原状态，SAV不新增UI字段。

独立复审无阻塞，沿用既有owner和B3.1纯选择，不增加第二套业务状态。新增`field_item_flow`覆盖正确／错误context、重复调用、禁用方向、code0／非零确认、坏actor／稀疏／未知库存拒绝、全owner及heap不变、Load成功清理与失败保留。正式COMMAND capability及确认ITEM仍拒绝，尚无生产调试或正式物品入口。

从仅缓存本小阶段5份源码的index冻结 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b3-20261105/item-session-acceptance/`，全新编译全部core／应用／测试，避免新struct与旧对象混用。Windows单轮54/54 CTest无跳过通过（真实输入42.87秒），10项发布审计及冻结源码465个规范地址通过。Python实现未变，沿用B3.2b实际322项结果，不声称新跑一轮。没有SDL物品WGC，原版28检查点仍只是B3.2a单端；继续B3.2c2私有列表等待及B3.3目标，不关闭整个B3。

### B3.2c2物品列表真实等待与子菜单回显

B3.2c1已提交`28810d7`。在field_play同文件提取仅MAGIC／ITEM Use两类的共享列表主体，原MAGIC保留薄wrapper，不复制开合、portrait0／3、16c57共享RNG／眨眼、水色tick、quit／drain及资源释放。分派仅涉及各自core预检／step、索引和下页绘制；ITEM仅在完整关页后输出raw槽，取消／错误不写out，terminal后仍由外层结束LIST。正式ITEM及目标／效果没有接入，也没有新CLI或生产测试入口。

game的菜单呈现沿原owner只扩ITEM_MENU：IDs8／9／10／11对应frames24..35，复用静止／开关动画及220ms高亮，仍最后重画选中actor。COMMAND的IDs0..3等价保持，ITEM_LIST等非菜单状态不泄漏图标。两个独立只读审查均无阻塞；不保存第二份菜单ID或资源缓存。

隔离index候选 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b3-20261105/item-modal-acceptance/` 仅包含固定presenter补丁和列表实现，没有后续ITEM_TARGETING。首次临时校验路径少`src`层，未进入编译；修正后先复核完整source manifest及固定副本，再完成全新core／应用／测试构建。Windows单轮55/55 CTest无跳过通过（真实输入42.92秒），10项发布审计及冻结源码475个规范地址通过。旧8组MAGIC回归和新增ITEM的1／4／5／8项、剑甲拒Use／药raw2确认、逐级core取消、共享RNG／眨眼、20类首错／后段失败及退出释放均通过；整帧合成render另验全部状态／相位和actor覆盖。

本阶段Python实现未变，沿用B3.2b实际322项；草药原版校准工具的后续变更没有混入冻结。没有SDL物品WGC，原版单端与双端累计27组继续分别登记。继续B3.3原版草药校准、目标和提交，B3.2d正式配对仍须完整支持候选；B3尚未完成。

### B3.3a原版草药192单次使用校准

B3.2c2已独立提交`9eb84f6`。原版工具保持controls默认范围，另加herb-full与herb-injured：后者仅改raw0 HP42→20、EX0→99及checksum，完整明文／密文manifest和XOR往返通过，站位、能力、roster及其它字节保留；派生SAV SHA `15cdaa8ce74038f2031d94dcf1983c0597b506e776a2b6028c87ffacb45ec17d`。范围取严格LE profile0和item192的player `+10/+12/+15`，不借AI `+11`；原版20c6f／211a4／1c916／1b8e7的完整原码与xref支持power40、移槽及全局53EC8清零，不清unit EX。

两场Windows原版首轮WGC通过：`C:/Users/Soar/AppData/Local/Temp/fd2-item-herb-{full,injured}-original-20261105-01/comparison.json`。每场16个实际检查点加1条白名单过渡帧重试，18条输入含3次启动Esc；目标打开2次、取消1次归列表0，仅一次实际Use，之后不跳演出。满HP仍消耗并acted；受伤者HP20→42，两场MP0／LV1、EX0或99保留，完整32,147像素详情显示剩余IDs0／132，最后576像素干燥地形证明关页。原版、夹具及隔离SAV的bytes／mtime守卫均通过；未发Save／END。

DOSBox PID6468／57332各正常exit0、无强制终止；外Job9736／5668均`worker_exit=0 timed_out=0 residual=0 forced=0 cleaned=1 result=0`。三工具SHA实跑前后保持；独立只读审查无阻塞，21项专项及2项输出路径回归通过。仅缓存工具／文档／原版证据登记的隔离候选 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b3-20261105/herb-calibration-acceptance/`，完整329/329 Python无跳过通过；没有后续目标state源码，不重称55项CTest为本阶段新跑。生成217项命名及发布SHA由Python审计验证，不改IDA数据库／物理类型。

本地自包含 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b3-20261105/original-herb-review/index.html` 内嵌8张实际PNG；两报告各44项capture_hashes全部校验，Job日志JSON与comparison逐项相等，内嵌bytes再解码SHA也相等。页面未启动浏览器，静态检查与原截图复核完成；生成器和报告均留本地，不提交。

报告仍`ok=false/probe_only=true`，不增加B2冻结候选累计27组或当作SDL配对。raw16B／末槽stale运行量测为未覆盖：因acted后本回合Save禁用，另设计独立END→下一玩家phase→合法Save，不强行绕过门或扩大allowlist。静态预期`40004084800080008000800080c980c9`与可见两槽不替代隐藏字节实测；完整浮字／RNG／时序／演出／声音、整关／结算／下一关未验。继续B3.3b目标、B3.3c提交、B3.3d正式入口及B3.2d配对，B3整体未完成。

### B3.3b草药纯目标session

原版单次使用校准已独立提交`8d3d7ac`。新增ITEM_TARGETING和四个纯目标API，当前raw槽必须仍为192/code5/cast1/area0/filter1；不套Magic的MP／+27／移动后门。复用move_range及原target候选buffer，不新增slot／ID缓存或SAV字段。候选仅排hidden与side0，HP0／acted仍保留，121占槽但不移动焦点；方向可出范围，合法性另查。取消完整focus使用者后清候选并回LIST0，不改移动路径／origin、单位、pending或RNG。

初稿审查指出重复的范围／候选和轮换逻辑，提交前改为同文件两个小helper，MAGIC与ITEM各自保留原record／列表／上下文门，共享数据准备、owner交接与121轮换；没有新模块或通用目标框架。重构后独立复审无阻塞，旧MAGIC写入／焦点顺序保持。定向全新46源core的目标／MAGIC target-flow-commit／item-flow／Save共6项通过；Save临时命令首跑cwd不符，按资源工作目录复跑通过，未归因于生产代码或隐去失败。

随后从固定target补丁独立冻结 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b3-20261105/item-target-acceptance/`，不含后续resolve或列表UI修补；fresh完整Windows构建、单轮56/56 CTest无跳过通过（真实输入42.75秒），10项发布审计及冻结源码478个规范地址通过。新6组用例覆盖raw／filter／HP0／acted、121、越范围取消与镜头、15类晚错原子拒绝、当前slot变更、Load成功清理／失败保留、合成cursor1和range LUT。正式confirm_cursor刻意仍拒绝，ITEM仍disabled。

Python实现未变，沿用B3.3a实际329项，未重称新跑。原版herb两场是8d3d7ac的校准，不是本SDL候选配对；本阶段没有新增WGC组。继续修复列表预检的UI侧位owner边界，再接B3.3c提交与B3.3d正式路径；B3整体及全演出／RNG／声音、整关／结算／下一关仍未验。

### 列表预检错误的面板侧位修复

纯目标已提交`b6dac3e`。connected审查补出真实renderer的UI owner边界：离屏`fd2_field_game_render`会按焦点修改`info_panel_right`，随后末项坏字模拒绝时，原modal虽未写实际VGA／耗键／RNG，却遗留了换侧值。此前替身render回归不能证明这条真实路径的全session不变，原有测试范围按此澄清。

最小修补只保存初始侧位与是否开始实际呈现：预检ERROR／quit恢复，首次drain后保留正常render更新，不回退已经进入的UI。现有TU加入真实game render，对MAGIC／ITEM、左右换侧及晚字模错／预检quit／确认／取消／首present quit共20组合验证；精确旧`c78f7df8...`源码与同一新TU／新ABI core链接，实跑在完整game不变断言预期失败，反例有效。独立只读复审无阻塞，没有通用回滚或复制session。

隔离index候选 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b3-20261105/modal-owner-acceptance/` 不含草药resolve，全新core与三个相关target构建后，`field_magic_list_play/field_detail_flow/field_play_result`单轮3/3通过。没有新增完整CTest／Python或WGC轮次；既有56项／329项及原版两场各保留真实来源。继续草药提交与正式双端候选，未关闭B3。

### B3.3c草药提交核心与玩家收尾

列表预检修补已独立提交`a2ebfa4`。新增`resolve_player_item`，只接受ITEM_TARGETING及当前raw槽仍192/code5。所有当前geometry、完整可见坐标／目标HP、未知loot、pending及已知phase／容量门先于首RNG。之后按raw目标依次以power40恢复HP（满HP也RNG1），原slot_remove只执行一次，保留末stale ID，再败亡清理、acted、最终lookup1及controller。MP、各unit EX（含99）及存活者方向保留，不借Magic升级上界、WAIT恢复或AI／物理尾链。

共享玩家尾预检只增加“本次会恢复同格HP0”的布尔，Magic0／13保持等价；受药必复活目标的drop描述符不会误拒，仍HP0的非目标未知loot提前拒绝。未知phase在写入前拦，已知执行路径后段失败保留HP／压槽／已到达acted和RNG并冻结。新增ITEM action kind只承接必要尾，不建新事务或丢弃奖励缓存。独立connected复审无逻辑阻塞；静态注释把1b8e7的memcpy勘误为memmove，未改执行逻辑。

按完整75B／98B及xref登记`field_unit_inventory_slot_remove @1b8e7`、`field_hp_heal_group_execute @211a4`，与实现注释同步、由CORRECTIONS生成219项及最后刷新SHA；20c6f仍保留中性原名，仅列已确认herb分支。不写数据库或伪C类型；HP group不是item专属，也不负责MP／压槽／acted，完整演出／浮字另验。

独立固定patch候选 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b3-20261105/item-commit-acceptance/`，没有后续正式gate／play。全新完整Windows构建、单轮57/57 CTest无跳过通过（真实输入42.83秒），10项发布审计和冻结源码487个规范地址通过；另新46源core的item commit-target-flow／Magic target-flow-commit／AI commit／controller／turn／Save共10定向回归通过。新6组验证受伤／满HP／同格raw顺序、RNG前槽与acted未写、MP／EX／方向及未知字节、slot中间／末尾stale、未知loot／pending／phase前拒、HP0复活预测和后段失败不重试。Python实现未变，沿用329项实际来源，不重称本阶段新跑。

正式ITEM仍disabled，confirm_cursor／play未分派物品；没有新SDL WGC或原版raw16B量测，不能把57项自测或旧原版两场当作双端。继续B3.3d的最小whole-list能力门及正式输入候选，经双端实际运行后才提交开放；B3.4装备／丢弃／Give和B3整体继续未完成。

### B3.2d／B3.3d正式草药Use与列表配对

B3.3c已独立提交`b2b384c`，正式入口与输入已提交`ecef415`；配对工具及默认注册单独提交。正式ITEM现通过同一只读whole-list能力门：stage0可行动玩家、共享RNG及密集已知库存，非零Use code仅192；普通code0装备只读，无草药也可查看。不借MAGIC的MP、移动、状态或升级限制，EX99可使用。COMMAND、ITEM_MENU及目标confirm重验全库存；LIST由完整关页后的raw槽交给目标层，confirm_cursor不能跳过modal直接确认。退出／逐级取消先于此能力门，临时移动及pending保留。

play复用既有FIELD_COMMAND和FIELD_MAGIC_TARGETING，不增input上下文或生产测试入口。列表开合／共享RNG只有一个owner，层间耗尽普通键并结束当前帧；非法目标不提交，成功Use正确记录item而非子菜单索引0的attack。Give／Equip／Drop仍按SDL能力禁用，原版START仅禁Give；Magic图形oracle同步验raw1普通code0库存的ITEM普通图6，保留旧B2候选禁用图8的历史范围。

完整fresh Windows Debug候选`C:/Users/Soar/AppData/Local/Temp/fd2sdl-b3-20261105/item-formal-c1/`通过单轮57/57 CTest（真实输入42.81秒、总62.92秒）、341/341 Python无跳过、10项发布审计及491个规范地址检查。程序SHA `0bb6ceceb23f8241520c483848afef88313784c626bfe08074004a6ee710ed4c`。后续C2只变两份item比较工具，C3再只变两份启动捕获工具；191份src原始字节相等，复用该已测程序和C1构建，不把工具修订当作新编译。

C1首个controls实跑的两端28个业务检查点均通过，但原版多1条WGC transient诊断使旧比较器按数量拒绝；原失败与正常清理收据保留。修补要求完整且唯一的业务名称集合，诊断仅接受typed backend／错误白名单／关联检查点／时序PID／重试数量及附件SHA，不能按后缀任意过滤。静态离线重放392,562累积像素bad=0只用于定位，不增加运行配对组。C2随后controls、herb-full、herb-injured三个串行双端实跑顶层均通过；对应外Job43400／37372／61096均worker0、timeout0、residual0、forced0、cleaned1。

完整30组C2运行先有18组passed，第19组original-overview在720×400启动logo尺寸门超时，业务尚未开始且原版PID16364正常清理。原summary保留为failed，外Job62300无强制或遗留；不将此轮称为单轮全绿。根因是外层捕获尺寸错误重入等待，每次重置next_escape。`c8bc137`仅在Capture.wait的startup阶段接受与真实raw尺寸精确相符的已知非整数倍率错误为诊断，保存BMP，沿同一deadline及既定1秒／后续2秒Esc节拍继续；后续合法尺寸、完整标题资源和内容门不变。小尺寸／非startup／未知错误不放入此分支，blank过渡无键。18项专项及真实旧logo的无窗口新旧反证通过，反证明确不当运行验收。

C3补跑通过总览、玩家取消、满HP WAIT、March、F1、结果页及法术列表7组，随后原版火炎术在`damage-return-caster-1`超时：RIGHT输入后仍停原空目标，SDL尚未开始。PID44232正常exit0，内Job18372及外Job48920均无强制或遗留；C3原summary保留failed，25组passed不改写为fresh30。代码复核确认acted及无框图可能早于1e292的经验提示，1e5c0(11)及controller会清键；现有截图不能单独证明该次清键时刻，故只登记已观测无移动及判据缺口。

修补仅在original damage的同一120秒检查点内增加无键资源观察：原有kill／closed_browse门至少两张有效帧匹配且窗口满2秒，任何已观测失配／本gate的WGC transient重计；miss仍直接失败，SDL不等待。2秒是host稳定参数，不冒充native常数。后续仍逐键ACK、F2验证MP6/8／LV1／EX42，不加补键／重抽或新捕获后端；奖励提示完整时序另验。C4只变Magic runner与对应回归，保持app／driver、WGC、像素门和cleanup不变。

C4最后5组全部实跑通过，最终累计30组为C2的18、C3的新7及C4的新5，不能写fresh30。最终summary为`item-formal-c4/cumulative-30/summary.json`，明确`inherited=25/executed=5`；外Job32848 `worker_exit=0 timed_out=0 residual=0 forced=0 cleaned=1`。旧报告、截图及来源／二进制／完整工具manifest的前后守卫均通过。C4定向Damage11项及Magic／Item／output-path共80项通过，另18项启动捕获回归；完整341项Python及57项CTest仍保留C1真实来源，不冒称C4全量Python新跑。

复审又发现恢复术的首个F2有相同1e292早帧风险，本次heal通过不能排除竞态。正式入口已独立提交`ecef415`，三组配对／默认注册已独立提交`ad8ddee`，未混入后续C5审查修补；本地`src/Debug/fd2sdl.exe`已更新为同一已验收0bb6程序。下一小阶段仅把damage／heal无键观察提为同一owner、保留各自资源门，再fresh配对两场；其它28组按冻结app与未变helper、截图／输入／工具来源累计。

C5删除单damage专用门，改为仅两caller共享的`magic_resource_quiet_gate`；各自结果predicate、deadline、checkpoint名、按键及数值门不变。治疗额外拒绝既有`dialog_top(5,112)`完整资源，不更改已修的42行actor组合；每场保存独立quiet收据，记录首结果／当前窗口／有效样本／重置和host参数。原版2秒／两帧、当前gate WGC重置、damage miss首见失败与SDL两场零等待均保留。独立connected复审无新增阻塞；删除旧单用helper和兼容wrapper，没有第二套等待循环或输入owner。

C5完整Magic46项、最终全量356/356 Python（217.436秒，无跳过）通过。最终`item-formal-c5/review-30/summary.json`为passed，明确28项继承加2场本次复验；两场原版／SDL真实运行，damage MP6/8／LV1／EX42，heal recipient42/42及caster MP5/8／LV1／EX20。四PID26616／23644／38796／37736均正常exit0，子Job60944／58940及外Job63232均worker0、timeout0、residual0、forced0、cleaned1。原版quiet实际为7帧2.360秒／4帧2.188秒，区间无键；SDL无quiet。不是用离线重放、C4旧Magic或原版单端校准补作本次配对。

最终源manifest326文件、其中191份src，以及原版50文件在全量Python后再次实核未变；工作区与冻结源码仅CRLF差异，本地程序同0bb6。两场及其祖先收据／输入／截图／binary／工具守卫均通过，保留全部旧failed报告。图形仍为累计30组（C2的18、C3的7、C4的ITEM3及C5的Magic2），CTest保留C1单轮57项真实来源；Linux未执行。此处只关闭已列基础物品纵向路径及观察竞态修补，不关闭整个B3。

B3整体仍未关闭；之后继续B3.4丢弃校准。raw16B／末槽stale、完整UI随机流／浮字／演出／声音、水色同步、整关实战、结算和下一关仍未验。Linux本轮未执行。

### B3.4a原版Drop取消、一次删除与正常Save校准

正式Use与配对工具已提交`ecef415`／`ad8ddee`，同类法术观察竞态已整理为单owner并提交`059d546`。继续B3.4时先选未装备草药192的Drop：它不结束行动，可从正式菜单合法Save量测；来源沿用未修改START与原版初始化，不注入RNG种子。静态依据仍为1bbdc selector3→1b8e7→1b750，native取消也重算，但本exact START四派生原已正确，取消重算没有动态可区分的数值变化。

工具加`--scenario drop-herb`，只接受原版；`--sdl`在mkdir／读文件／启动前拒绝。复用同一资源、checkpoint、Save解码与整容器比较，没有新截图后端或第二套Save框架。mode0页面记录为Drop而非Use；先slot2取消回ITEM0，再重开slot0后导航slot2只确认一次。之后仅Use只读两槽、逐级Esc回未行动浏览，LEFT到空格`(6,14)`，camera保持`(5,11)`，正式Save一次。Save前bytes／mtime每页及肯定前检查；Save后先落真实`drop-measured.SAV`，严格比较失败仍保留文件，成功及cleanup后都核实当前copy／snapshot的bytes、SHA和完成mtime，晚改写撤销通过。

独立冻结`C:/Users/Soar/AppData/Local/Temp/fd2sdl-b3-20261105/drop-herb-c1/`仅变Item runner及其回归，191份src未变；作者42项和冻结后root单轮42/42 Item回归（12.525秒，无跳过）通过，独立复审无阻塞。全量356项仍是C5实际来源，不把本次专项写成新全量轮次。

`native-01/comparison.json`首轮原版校准通过：`probe_only=true/ok=false/probe_cases_passed=true`，28业务检查点、27业务键及4启动Esc，无重试Drop、Use／Equip／Give确认或END；仅一次取消及一次Drop、一次Save。列表检查点覆盖完整32,147像素资源门，原版PID40032正常exit0；外Job30668 `worker_exit=0 timed_out=0 residual=0 forced=0 cleaned=1`。原目录50文件、fixture bytes／mtime、Save后snapshot／当前copy及清理守卫均通过；登记截图附件72份（PNG／BMP），不增加30组双端／SDL累计。

实测SAV SHA `0552e8890f890691e186129b2d6f30ea90851deefd9e00599e3f0e97a70296ea`，有效size `0x59cb`与checksum，active12／roster4、turn1／stage0。库存raw16B为`40004084800080008000800080c980c9`，末ID `c9`保留；HP42/42、MP0/0、EX0、flags0／未acted、方向0，AP／DP／AC／EV为16／12／97／2。整容器相对START只有3库存字节`12b1/12b2/12ba`、测量焦点4字节`30c8..30cb`及checksum最低字节`59c7`改变，既有允许的active+2 cache实际零处变化；camera、全部roster、未知区及尾部逐字节保留。

这证明共用移槽函数在Drop分支的真实写入，不把它扩成Use后acted全raw测量；取消重算调用仍标静态依据。SDL Drop、装备物品／最后一项删除、Equip／Give、完整RNG／演出／声音及B5仍未验。下一小阶段接SDL mode0列表与移槽／必要派生重算、逐级取消，再补已装备及最后一项原版场景，满足条件后才启用正式Drop。当前无需要用户选择的等优方案。

### B3.4b1纯Drop会话与失败边界

B3.4a原版校准已独立提交`187871c`，继续在现有game会话接纯Drop，不开放正式菜单。仅改game实现／接口和既有field_item_flow回归；无新模块、CMake目标、play分支、CLI或UI字段。ITEM_LIST保留的command_selected3区分mode0，code0装备可确认；新增resolve_item_drop与原end_item_list取消复用同一private尾，Drop3不能误进Use targeting。正式disabled3仍为1，测试仅在低层夹具显式开放。

尾部先用80字节actor副本，确认才调用原slot_remove，确认／取消都调用现combat_stats_recompute；隐藏equipped坏ID等晚错不会写原actor或UI。全通过后交接：非空回ITEM0，删空回COMMAND首可用项，WAIT保证存在；基础actor门与非空库存门最小分开，refresh返回的其余前置已验证，无提交后拒绝。没有acted、目标、EX、cell/controller/phase或RNG调用，不回滚移动／pending及既有库存变化。复审确认没有第二份槽缓存或通用事务。

既有目标内追加4组：取消将陈旧派生999重算而库存不动；装备／草药／8槽与末槽stale；删最后一项返COMMAND及重复拒绝；20种坏context／visible或hidden装备ID与晚失败无actor／owner写入。EX99、HP／MP、方向、acted、camera、移动path／range实际内容及pending逐项保持，RNG0，既有Use／target／commit回归不漂。known1.15缩放对i16基础及8装备之和不会溢出int32，不添加生产失败注入钩子。

`C:/Users/Soar/AppData/Local/Temp/fd2sdl-b3-20261105/drop-core-c1/`从仅3个暂存文件冻结，全新Windows完整构建、单轮57/57 CTest无跳过通过（真实输入42.76秒、总63.03秒），498个源码函数地址token均为canonical；全326源manifest在测试后未变。作者另fresh46core的item_flow／target／commit三项通过，独立connected复审无阻塞。Python未改，356全量及42项Item工具专项分别保留C5／B3.4a来源，不重称本阶段新跑。

本纯会话程序SHA `5c32793ea77a5439df93bab985028088403d31220924bb56a730516a3cdd1ea2`；没有新的Drop WGC或正式操作入口，既有累计30组仍对应0bb6正式候选，不能复用为本程序的完整图形验收。原版192 Drop的Save证据仍只属B3.4a。下一项为B3.4b2的正式输入／能力门与装备、最后一项原版场景及双端对照；Equip／Give、完整RNG／演出声音、整关／结算／下一关均继续未验。

### B3.4a 补充：装备物品及最后一项原版校准

冻结目录 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b3-20261105/drop-boundary-c1/` 仅更新物品资源 oracle、原版 runner 及对应回归，未混入正式 Drop 源码。冻结后专项 48/48 Python 通过（18.486 秒，无跳过）；旧 34 个模板的位置、尺寸、索引和调色板逐字节保持一致。新增装备场景检查四项派生数字，列表门共 32,723 像素，原有整容器差异范围未放宽。

`graphics-artifacts/20261105-081841-661641/summary.json` 四场原版首轮全部通过，均为 `probe_only=true/ok=false/probe_cases_passed=true`，没有 SDL 配对。草药、装备武器、装备防具分别使用 exact START；最后一件只在隔离派生档中保留草药并把四派生重算为 6／4／2／2，明列源字节、校验和及摘要，不改坐标或站位。每场一次取消、一次 Drop、一次合法 Save，无 Use／Equip／Give 确认、END 或 Drop 重试。

| 场景 | 实测库存 raw16B | AP／DP／AC／EV | 实测 SAV SHA256 |
|---|---|---|---|
| 草药 | `40004084800080008000800080c980c9` | 16／12／97／2 | `0552e8890f890691e186129b2d6f30ea90851deefd9e00599e3f0e97a70296ea` |
| 武器 | `408400c0800080008000800080c980c9` | 6／12／2／2 | `3be05a20c27df94a9874fca7fb3dce058ecc6c3e66fd3b55b2e9e3036fc580a6` |
| 防具 | `400000c0800080008000800080c980c9` | 16／4／97／2 | `1a1f580f797da93d5c55ccb51ea17af8a9dc16240254cf9ee07508b2db465e1b` |
| 最后一件 | `808480c0800080008000800080c980c9` | 6／4／2／2 | `2532562c4ba6916874b0170a8e9fd96b19e4b031c3fda8e633c4c3c7d9be4bd5` |

四场末 ID `c9` 保留；flags0、EX0、HP42/42、MP0/0、turn1／stage0、active12／roster4 均保持。真实 Save 均通过完整容器比较，既有允许的 active+2 cache 实际零处变化；camera、roster 和未知字节不变。最后一件空库存检查 COMMAND 完整资源及首可用 WAIT，不虚构空 Use 列表。原版 PID36300／34236／45236／13576 均正常 exit0；外 Job34796 的 worker0、timeout0、residual0、forced0、cleaned1。原目录 50 文件及每场来源／保存完成态／cleanup 守卫均通过。

本批只扩充 B3.4a 原版依据，不增加原有 30 组双端／SDL 累计。取消重算因初值一致无可区分写入，仍只记机器码调用依据；Use 后 acted 的 raw16B、正式 SDL Drop、Equip／Give、完整 RNG／音画、整关结算和下一关均未因此验收。下一步继续 B3.4b2 正式输入及同四场双端比较，不把这四份单端报告移作配对通过。

### B3.4b2 正式 Drop、四场双端及新程序完整回归

原版装备物品／最后一项校准已独立提交 `17cf5f5`。正式入口沿用同一 ITEM modal 和 command_selected，不增 slot／ID 缓存或新输入上下文。Use／Drop 能力分开：Use 限已确认 handler，Drop 独立预检全 8 槽重算；任一可用才开放 ITEM，仅 Drop 可用时首项为 Drop3。Give／Equip 保持能力禁用。列表完整关闭后才提交 Drop 或进入 Use 目标，关页退出不提交，资源错误优先；已提交后菜单失败不退款、不重试。删空回 COMMAND，余项回 ITEM 首可用项；取消移动保留已提交库存。

`drop-formal-app-c1/` 全新 Windows 构建及单轮 57/57 CTest 无跳过通过（真实输入 42.88 秒、总 63.07 秒），498 个源码地址 token 检查通过。程序 SHA `64175bed9a41cc04f39a73c0418afa173d072c6b63d1a62c6f40bd4b6467e605`、driver SHA `2f587d46862b4c313f89c1ddab65c80a272ad35cf5321cc654e16b082939f9ba`。专项 53/53 通过，随后 `drop-pair-c1/` 冻结 326 文件，其中 191 份 src 与实际编译源逐字节一致；全量 374/374 Python 通过（232.978 秒，无跳过）。最终提交仅把 header 的“余项回 ITEM_MENU0”修正为“回 ITEM 首可用项”，无编译逻辑变化；受测源码及 manifest 保持原冻结值，不追改历史来源。

`drop-pair-c1/graphics-artifacts/20261105-083850-033829/summary.json` 是新程序单轮 **34/34**，`inherited=0/executed=34`：先重新执行自然开场两端各 117 个等待点、角色终态／保存测量和镜头留边，再执行既有控制、存取／回合／结果、基础法术及 Use，最后实际执行四场 Drop。没有把旧 0bb6 程序的累计 30 组或四场原版单端校准算作本次通过。

四场每端各一次取消、一次删除和一次合法 Save。各自先过原版资源与完整容器模型，再比较两端实际 PNG 静态 ROI（每通道容差 3、bad=0）以及两份真实 SAV 的完整编码／解密字节；不归一 cache、焦点或扩大差异允许范围。草药／武器／防具／最后一件的共同静态像素分别为 236,845／109,985／175,431／74,690；raw16、四派生、flags0／未 acted、EX0 及 SAV SHA 均与上节原版实测一致。最后一件直接回 COMMAND／WAIT，未打开空 Use 列表；余项检查完整四数字与只读 Use，后续取消不恢复库存。SDL 子菜单 `[0,1,1,0]` 与原版 `[0,1,0,0]` 分别核完整图标，不把 Equip 能力差异冒称等价。

外层 owner19208／Job root3592 `worker_exit=0 timed_out=0 residual=0 forced=0 cleaned=1 result=0`；全部原版／SDL 流程串行并清理。源 326 文件、原版 50 文件和两二进制前后守卫通过。保留全部输入、截图、SAV、批准的 WGC transient 诊断及 stderr。四条 `no visible window for PID` 对应 host-close 窗口的关闭后探测，四 PID47732／57552／12792／38240 最终正常 exit0，收据均 cleaned、未强制；不删除诊断或视为残留窗口。提交前另核工作区 191 份 src：25 份仅 LF／CRLF 差异，唯一额外改动为上述 header 注释；规范化后可执行源码文本一致，`review-source-equivalence.json` 单列这些差异，不称工作区 raw SHA 完全相同。connected 终审无新增执行性问题；保留必要的独立能力查询和共享 modal，没有引入新 owner 或通用框架。

这是 B3.4b2 的受支持正式 Drop 交付，不关闭整个 B3。下一小阶段为 B3.4c Equip 原版校准：先核职业许可、同类 flag 写集及确认后留页重绘，再接纯 session 和正式 modal；Give 及满槽交换独立处理。Use 后 acted 的全 raw16、完整 UI RNG／演出／声音、水色整帧同步、整关实战、结算和下一关仍未验，Linux 本轮未执行。

### B3.4c1 Equip 兼容替换与拒绝的原版校准

正式 Drop 已原子提交 `7fe8008`，本地程序更新为同一受验 64175；继续 Equip，未改 game／play、正式能力或默认配对组。新工具在独立副本使用现有资源 owner，把两个实际 caller 共用的测量导航／Save 提为单 helper；旧 Drop 按键、检查点、完整容器规则及末尾守卫不变。profile1 行 canonical `0x61891`／file `0x7a8a5` 的 `01 15 16 ff ff ff 01` 由严格 LE／fixup 取得，只比较前6B。依据复用已发布 IDA 的 `sub_1BFFE/1C1C3/4E88E/1C142` 及 xref／原码，没有新建或保存数据库，也未发布新函数名或伪 C。

`C:/Users/Soar/AppData/Local/Temp/fd2sdl-b3-20261105/eqt-native-c1/` 冻结326文件，只更新三份物品工具及阶段计划。作者59/59专项（27.823秒）和2/2输出路径守卫（0.177秒）通过；root在冻结包全量380/380 Python（235.517秒，无跳过）通过。source程序没有新编译或新SDL Equip，57项CTest及34组图形仍保留Drop正式冻结候选来源，不当作本工具阶段重跑。

`graphics-artifacts/20261105-093956-527508/summary.json` 两场原版首轮通过，均 `probe_only=true/ok=false/probe_cases_passed=true`。`eqt-incompatible` 使用exact START，草药192/category32确认Enter及Space后仍在slot2，raw16 `4000408400c0800080008000800080c9`、四派生16／12／97／2及全部玩法字节不变，实际SAV SHA `ef7c27c00e15835866622d53ee734fec008e70a11fbc5f1780421d4dbbd202c1`。`eqt-weapon` 明列隔离派生档，只把slot2 ID192改1及checksum，来源密文SHA `ec01712ee1c8055eeba4071554af0284f3b89a8fbac24e56d28666ac58e99b4c`；一次确认后仍选slot2，raw16 `000040844001800080008000800080c9`，四派生26／12／97／2，实际SAV SHA `9a27c6d4d76d404c1fd39e94d8c8bf50d768bfbcc4fad915e48d95a8b162dc6d`。相对派生来源的玩法写集仅 `0x12ad/0x12b1/0x12eb` 两个整flag及AP；不是消耗／压槽，也不置acted。

两场均检查完整32,723像素页面、确认后留页、Esc回ITEM0、重开EQT0、逐级取消及一次合法Save，无Use／Drop／Give确认或END。整容器相对各自来源只出现已列玩法写集、测量focus及checksum，允许的active+2 cache实际0处变化；HP42/42、MP0/0、EX0、flags0、camera、roster、未知区及末ID均保持。取消不重算的调用结论来自发布伪C，本次一致初值不证明陈旧派生的动态写入缺席；两场仍非完整UI随机流／声音验证。

原版PID41796／5192均正常exit0，外owner45380／Job5940 `worker_exit=0 timed_out=0 residual=0 forced=0 cleaned=1 result=0`。源326文件、原版50文件、fixture bytes／mtime、测量后snapshot／当前文件完成态及cleanup守卫通过。此批只关闭B3.4c1两个原版场景，不增加34组或开放Equip。接下来B3.4c2纯兼容查询／flag写集与重算、c3留页重绘及c4正式双端；Give、完整RNG／音画、水色、整关／结算／下一关仍未验。

### B3.4c2 纯 Equip 兼容、整 flag 与留页会话

原版两场已独立提交 `052e146`，继续在既有 field_item／game 中接纯子集，仅6个src文件，无play／struct／CMake或新测试目标。profile1行来自static原表而非stage0捕获默认，未知profile／ID明确拒绝；仅比较前6B。command_selected2复用mode0及ITEM_LIST，低层夹具可显式开启，正式菜单和confirm_cursor仍禁用Equip。

应用先预检全8槽equipped ID，使用一个80B shadow按ID `<128`／`>=128`清整flag并选槽写40，复检dense-known结果、现有四派生重算全通过后再交接。不兼容返回0且不写、不重算；成功1仍LIST／原索引，已装备重选不toggle-off。取消不重算陈旧派生，也不走Drop尾。未知ID、隐藏equipped唤醒出空洞等晚错原子拒绝，不用仅清bit40来掩盖原版整byte写集。原库存已提交变化不随后续移动取消回滚；无RNG、MP／EX、acted、cell/controller尾链或额外slot／状态owner。

作者独立fresh46core中的两个既有测试目标通过；root先修正数据VA注释的函数地址token写法，再从仅6个暂存源冻结 `eqt-core-c1/` 全新Windows构建，完整57/57 CTest无跳过通过（input43.14秒、process12.18秒、总63.36秒），10/10地址／发布检查（0.076秒）通过，507个函数地址均canonical。六组新flow及compat回归覆盖profile边界、整flag、127/128同category但不同ID组、连续隐藏唤醒允许／空洞拒绝、21种数据失败全game不变、取消陈旧派生、重复确认留页、外层移动取消保留和RNG NULL／0。独立connected审查无逻辑阻塞，数据注释问题已修，没有新增通用框架。

源326文件及原版50文件前后守卫通过；当前6份src与受测冻结文本仅LF／CRLF差，额外注释修正已在本次编译前纳入。纯程序SHA `7d091257994333928bbd04bc43ca9e28b4f455462564ec313f0db1d1d15eefa8`，driver `029e33462533c92fb712f19c7799c07ae298752b1810513e5b926b607fd4eddd`。没有SDL Equip WGC，旧34组继续只属64175正式Drop候选，不继承为本纯程序验收；本地公开程序仍保留已验收64175。全量380项Python保留c1原版工具阶段来源，不冒称本切片再次全量执行。

原版四函数保留中性sub及canonical依据，没有手改生成pC或声称完成新命名发布；项目／既有临时证据未找到数据库，隔离建库与真实生成方案已准备并按AGENTS发出单独授权请求。下一步c3在原modal内接确认后详情／列表重绘并继续等键，c4双端通过后才开放受支持正式Equip；Give、完整随机流／音画与整关后续仍未验。

### B3.4c3 Equip 私有留页、重绘与退出边界

纯会话已提交 `085e845`，继续在既有modal接Equip2，只有game实现／接口、play及既有modal TU共4个src文件。等待query复用纯支持门及共享RNG，COMMAND仍只OR Use／Drop，MENU2仍灰，confirm_cursor拒2。没有新字段、上下文、输入owner、模块、目标或生产测试入口。

确认先drain本帧普通键，再take_quit及重验门，才调用纯API。兼容成功以发布 `sub_1BFFE→field_unit_detail_draw @0x17eef` 的portrait0重画详情／未选底板，当前VGA和同一关闭page均更新，留原索引继续等键；不兼容不重绘上页，保留现眨眼画面。Esc才走唯一关闭，confirmed输出不写。关闭page用换装后内容，不闪回旧flag/AP。潜在hidden+equipped名称在首帧前用80B临时visible记录借现renderer预检，正常hidden stale不误检；错误恢复info_panel owner且无真实VGA／输入／RNG写入。提交后的quit或upper/lower重绘错误保留actor，不退款、不重试；各路径释放既有背景、page及两portrait。

作者独立fresh46core的4个既有target通过（3.21秒）；5组新增modal回归覆盖一次开关7键、连续成功／重复／不兼容、同帧Space排空、完整64K新页与关闭page、portrait3时成功复位／拒绝保留、5种quit时点、4种数据／晚绘制错误及资源释放。root冻结 `eqt-modal-c1/` 全新完整Windows构建，单轮57/57 CTest无跳过通过（input42.90、process12.21、总63.18秒）；10/10地址／发布专项（0.073秒）与510个canonical函数地址通过。两个connected独审无阻塞；新声音、精确墙钟及完整UI RNG没有因此验收。

源326文件、原版50文件及当前4源文本前后守卫通过。程序SHA `0a070e5fedd526378c45ffa83a7b92280aa4311f0fb92f930d158f66a412542d`，driver `1443db5a1c65d09aa18bb2c3334f0a27cb7c873a042086433639a31c7a09bebd`。此程序只有私有Equip modal，没有新Equip WGC；34组仍严格属于64175正式Drop候选，不跨binary继承。本地公开程序仍为已验收64175。最新全量380项Python保留c1工具阶段来源，不冒称c3再跑。

下一步c4先处理项目命名纪律：四函数仍引用原sub与canonical，项目及既有证据中没有数据库，不能用registry／SHA刷新代替真实pC导出。`ida-equipment-proposal.md`及更详细`ida-equipment/workflow-proposal/`已准备纯C临时工作区、四函数映射、真实版本provenance、缓存／probe隔离与生成校验步骤；建库／标注／导出尚未执行，AGENTS规定的单独授权请求仍待答复。授权后按真实生成结果同步注释／命名，再冻结正式Equip和双端工具，完整重跑新候选36组（旧34加Equip2），不得仅更模型计数或复用旧图形。B3整体、Give及完整音画／RNG／整关后续仍未完成。

## 本轮审查结论与后续

已实现 B1 子阶段审查发现并安排修复：runtime 单位区 64 条与存档格式 96 条边界混用、首次保存不能创建活动文件、错误重建坏档会覆盖其他槽，以及外层 runner 超时绕过子进程清理。修复按所属小阶段提交；窗口清理采用原生 Job，删除未使用的旧截图热键分支。未为这些修复新增场景框架或另一套存档 codec。

真实 `START.SAV` 的 active／roster 数为 12／4，后期本地档为 27／31，推翻旧 count-copy 假设。B1.2b 因此增加独立 roster owner、原版构造与招募事务，收回不正确的手工 Load 活动恢复；B1.4e已确认该路径属于准备页，实际可见入口与关卡重建留在B5。审查又修复了已恢复的 raw roster 尾在跨文件保存时丢失的问题，使用整表 owner 并删除旧活动表手工恢复分支。以上是有原版数据和调用链依据的计划调整，无待用户选择的业务分歧。

总览、AI 沿途 pending 和玩家动作／取消后置顺序已分段提交；角色绘制修正另行验收。行军的原版 START 几何基线揭示共享移动成本表提前 16 字节，先按严格 LE 映射修表及几何，再接批次入口并新跑原版配对。F1双端资源与控制配对已通过（实际原始RGB水色未对齐）；四槽已按真实owner迁入B5；未对齐的伤害、AI 位置和镜头不作一致性通过判据，不将 B1 整包记为完成。

B1最终证据位于 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b1-20261104/final/`：`ctest.log`、`python.log`，以及 `build/graphics-artifacts/20261104-204240-393412/summary.json` 和11份原版配对报告。全部22项status为passed，外Job46160 `worker_exit=0 timed_out=0 residual=0 forced=0 cleaned=1`；11项配对的24个进程全部正常清理，其余SDL窗口及输入子任务也由各自Job收口。截图／报告不提交。先前失败现场保留，最终单轮通过不追改历史记录。

此前 `2035e02` 关闭的是B1第一关受支持控制与独立结果分派，不是第一关完整实战、自然学习、结算或全游戏；用户反馈后的自然开场与边缘镜头复验见上节。B2前置发现的无演出MP费用已修复；动画中零／多caster标记、玩家法术和实际运行仍按B2／B6验收。四槽的准备owner归属已查清，实际可见存取和出阵恢复完整留在B5。B2.1a从共享现有五字节位图查询开始，不提前开放未完成的MAGIC命令。

B2.1a完整fresh候选位于 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b2-20261104/list-query/`，含源码manifest、原版／LE只读输入摘要、全build及43项CTest日志。299份跟踪源码只变更4文件；共享查询沿用原40bit枚举，不在枚举层套AI或玩家状态门。接着实现B2.1b列表资源、输入和逐级取消；受控恢复术场景及自然获得路径分开验收。

## 2026-11-05 完整B3：Use／Give／Equip／Drop

本次以整个B3为一个提交单元完成剩余功能，不重写原微阶段提交。项目数据库操作改用Domain／idalib自主执行，阶段内部只做本阶段及直接受影响路径的定向验证；B0—B5及所需A5的大阶段完成后才统一全面回归历史功能。AGENTS、总计划及IDA／图形规范已同步清除旧授权和重复回归关卡。

正式ITEM开放全部已确认库存操作。Equip采用完整29×7B原许可表，仅比较每行前6B；兼容确认重绘并留原槽，不兼容及逐层取消遵循原版。Give按首个可见相邻合法角色选择，空槽追加、满槽双方remove后append，仅sender重算，recipient即时派生保持；sticky 53C53使成功交接后的外层取消保留移动／库存并结束行动，不调用WAIT恢复。移槽／追加复用现有helper，保留末槽stale ID及未知字节，没有新增第二套库存或存档状态。

Use按玩家20c6f而非AI表接入code5..24全部20类：恢复HP／MP、先恢复9HP再清状态、固定WORD成长、临时状态、伤害与两段瞬移，分别保留消耗规则、目标顺序和controller收尾。cast≥16为十字，真实item102补两格area2双目标；code23只查原版maxMP／身份条件，当前MP0仍扣20并回绕65516。所有可确定的坏profile、未知loot／格事件及后置容量先于首RNG拒绝；提交后错误不退款或重试。

终审发现并修复详情显示的两个既有差异：17fc0对9个WORD数值作signed16读取，负值经187d6显示0；HP／MP条必须使用首／中／尾及空条共8张1×5资源，不能重复首端或把负值画成满条。统一资源owner及首帧预检覆盖全部8张，max0不绘、signed除法及零／负长度沿原版；只改显示，不改unit／SAV原值。新候选实际检查HP和MP各103×5整条，原始u16、signed输入和可见数字分开记录。

### 源码、自动测试与局部原码

证据根目录为 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b3-20261105/`。C1冻结328文件，全新Windows构建后11/11定向CTest通过（3.72秒），涵盖物品选择／流程／目标／提交／modal、相关Magic、attack、AI、Save与play结果边界；不是本轮全57项。C1 app SHA为 `e847ca211a3bc68a1868ac0bf6a83a21bda27aa8b7ac9c40ff6ae7f44180f1a6`，driver为`940d63640bea1e66f8d78dfafe7047fe5aed45c8b8831feeea9863eeffe39ce1`。

显示修复后的C2另冻结328文件，全新Debug构建及field_info／field_detail_flow／field_magic_list_play三项CTest通过（0.50秒）；编译输入152份C／header／CMake与最终工作区逐字相同。app SHA为 `000090fe0110906eed1bb5feff92efe3d9cca6d8fa16e75ee0873fc23bc886f3`，driver为`a94e878da509fce17f20d35e9178a83ddb5327e10704634494ded61bb9ccfb31`。C2最终图形工具另冻结81份Python，manifest为`980c6d9c37685d8860ef5ff3120d666a92500086f1826265cb57724280c74f9c`。C2不可变清单所登记的14份命名生成输入／输出与最终发布逐字相同（含新pC caa72da3…）；后补本阶段记录等非编译文档另列，不改写冻结清单。

Python按实际工具版本分轮登记，不相加为唯一用例数：最初Item／Suite／路径110/110；导航修补45/45；数字溢出修补18/18；新增边界58/58（49.354秒），最终只改语义注释和错误文案后50/50（1.717秒）。相应日志分别在`b3-tools-use-c1/author-target-tests.*`、`b3-use-navigation-repair/`、`b3-use-number-repair/`及`b3-use-boundary-additions/`。显示修复另有独立无窗口field_info 1/1，最终三项以C2来源为准。

隔离IDA真实数据库已创建并发布，无GUI ida／idat；实际IDA／Hex-Rays9.5、win32／lx.dll，1307个函数，少于历史9.4的唯一区别仍为21B putchar_0识别。新增语义由CORRECTIONS→真实DB→TSV／pC同步，最后加入18795／17d6f后registry230项；21/21镜像／fixup静态检查、最新12/12发布及地址专项、全部11项SHA／LF检查通过。持久类型仍恰原9条，3702f不保存猜测原型。详细收据见`ida-equipment/run-20261105_111831_688108/current-stat-bars.txt`所指目录，不能把刷新SHA当全部语义新验收。

AP／DP临时增量以原CW127f、原x87块及真实__CHP分别穷举2×65536个u16输入，等价`(15*old+100)/100`；未改指令或以模拟handler替代。独立1.15派生重算舍入保持原规则。该局部原码执行不是DOSBox完整RNG／玩法验收。

### 原版与SDL的实际WGC来源

共35种场景、36份正式双端执行收据：旧e847候选33场，最终C2仅直接受影响3场（其中2种新增）。没有把历史34／36全套或旧候选结果登记为C2重新跑35场。两端均为HWND WGC，各端原版资源门通过后，再比较明确静态区域的实际PNG（每通道差≤3、bad=0）。

| 通过场景 | 实际产物目录（上述根目录内） |
|---|---|
| 菜单、草药2、Drop4、Equip2，共9场 | `b3-complete-c1/build/graphics-artifacts/20261105-122117-292923/` |
| Give取消／空槽／满槽3场 | `b3-tools-give-c1/build/graphics-artifacts/20261105-123343-058156/` |
| 移动后Give1场补跑 | `b3-tools-give-moved-c1/build/graphics-artifacts/20261105-124251-589229/` |
| Use05／06两场 | `b3-tools-use-c1/build/graphics-artifacts/20261105-124852-250214/` |
| Use07..11五场 | `b3-tools-use-c2/build/graphics-artifacts/20261105-125949-795243/` |
| Use12／13两场补跑 | `b3-tools-use-c2/build/graphics-artifacts/20261105-132435-521600/` |
| Use14..24十一场补跑 | `b3-tools-use-c3/build/graphics-artifacts/20261105-134213-862639/` |
| C2正常Use23复验、新增MP0瞬移／真实item102双目标三场 | `b3-tools-final-c5/build/graphics-artifacts/20261105-142439-815231/` |

C2三场各端完整保存容器均通过且双端逐字相同；零MP的raw65516由真实Save验证，两目标分别HP42／28，源槽只移除一次。其余随机Use的期限／命中／伤害由严格LE及原码给出有限完整结果集合；每端即时F2索引必须对应该端最终完整SAV，不互借结果或重抽。code12／14／15／16／20／21／22／24本次跨端有合法随机字节差异，不宣称整容器或随机流相等；其他确定字段仍严格。controls及旧草药2场未Save，其余保存量测与完整未知字节检查以各报告为准。

全部已启动游戏串行且正常exit0；C1最后外Job60088、C2外Job61564均`worker_exit=0 timed_out=0 residual=0 forced=0 cleaned=1 result=0`。原版50文件的SHA／size／mtime与初始一致，冻结源码、工具、截图及最终Save完成态均有守卫。旧Give顶层submenu.disabled常量是元数据笔误，端内动态图标资源门实际正确；保留旧报告并勘误，不篡改原证据。

### 失败、复审与未覆盖范围

失败报告原样保留：旧Give移动范围ROI误写LEFT，原版PID53636已运行到movement-selection才触发断言；修复后将几何校验前移至窗口启动前，按真实合法范围补跑。Use07导航遇到已行动角色，修正完整灰人／光标组合判据；Use14的EX255原版显示两位溢出图93，修正oracle而保留原始255；工具C4漏src/tests两个Python依赖，零窗口即导入失败，Job54172无遗留，完整81依赖另冻C5后实际运行三场。上述离线旧图重验不计新的双端运行。

Use12首次SDL END确认框观测到NO，原因仍未定位；原版端已完成。隔离instrumented程序的诊断及同e847正式补跑通过，不表示根因已修，也不把诊断程序计入35场。此项留到大阶段输入审查，不降低YES资源门或改写旧failed报告。

完整B3连接复审与新三场工具独审无其他确证阻塞；合并重复append，沿用既有库存、modal、范围、Save和资源owner，未增加通用框架或第二套状态。Equip实机仅profile1两场；Give叠格／HP0／赠出最后项、瞬移MP1等组合未逐一WGC，本阶段以原码及定向回归覆盖，不能声称这些组合都已实机验证。受控START派生的flag04／库存／身份等在manifest明列，非自然整关获得路线。Linux、完整共享RNG、FIGANI／声音／精确时序、水色同步、整关实战、结算及下一关仍未验收。B2剩余法术未关闭。

之后直接推进完整B4，不按成长helper／列表切片提交。原成长68×11及学习20×12表、奖励时点和固定增量已取得只读依据，隔离纯逻辑草稿2/2通过；正式贡献owner、分段升级／学习提示、状态呈现及本阶段DOSBox对照仍待完成。大阶段统一审查门仍为B0—B5及所需A5的自然新游戏到下一关可操作交接。

## 2026-11-05 完整B4：经验、成长、学习与状态提示

本次以整个B4为一个交付／提交单元。正式动作接入唯一的raw贡献owner，恢复逐步奖励、成长／学习与毒伤／状态到期提示；B5及所需A5继续推进，第一关完整纵向大阶段尚未关闭。本阶段不重复B3历史35种GUI或全仓库历史回归。

物理每击按原门覆盖53EC8；仅玩家MAGIC在controller按有效等级归一，玩家奖励再cap99；AI反击无该cap，AI法术／物品在原尾清零，不移到首效果之前。共享效果的贡献写点先于后续duration RNG，恢复以signed实际差累加并保留DWORD回绕。贡献与单位EX、提示数字分别处理，不写SAV，活动Load不擅自清零。坏数据／未知loot／必需资源先于首RNG预检，提交后失败保留事实并停止，不退款或重试。

成长使用完整68×11B表及20×12B学习表。已有ID0..67记录沿原奖励规则，不套新建构造器0..31门；LEVEL先加等级再提示，非零WORD成长在相应-3 ACK后写，学习位先于587写。每级末重算，最终关闭后才写EX；最大HP／MP增长不补当前值。状态tick按raw／byte顺序暂停，毒伤HP→focus→487／wait10；全部毒伤后死亡清理。六状态归零即481..486／wait10／关闭／重算后才继续下一项，自动页不补键。

### 冻结候选与实际验证来源

证据根目录为 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-b4-20261105/`。各候选336文件／194src，source清单、原版50文件bytes／size／mtime和程序／工具身份分别保留；后补文档不回写旧清单。Python与CTest按实际版本列出，不相加为互不重复用例数。

| 候选 | 编译／工具与定向检查 | 图形结果 |
|---|---|---|
| C1 `b4-complete-c1` | manifest `11c4b323…403`，fresh Windows候选构建（main／driver及所列target）；18/18 CTest，4.26s；18/18奖励Python，4.537s；38/38Suite，0.347s | 首原版到SaveA／真实WAIT后，Save-disabled oracle超时；SDL未启动，不计pair |
| C2 `b4-tools-c2` | manifest `639233ec…572`，194src与C1逐byte相同，复用app；18/18奖励，4.427s＋16/16END，1.238s | 原版完整Save／WAIT／Load／Save通过，SDL物理目标首焦点失败，不计pair |
| C3 `b4-complete-c3` | manifest `ea4ba944…6e9`，修物理目标后fresh app／driver；7/7定向CTest，3.45s；20/20奖励，5.038s | 原版通过，SDL目标控制与99奖励后因错误横幅门失败，不计pair |
| C4 `b4-tools-c4` | manifest `be8df273…2d0`，194src与C3相同，只修角色横幅门；22/22奖励，5.742s，无CPP重编 | 物理99、双击覆盖、治疗、升级学习四组实际双端通过；counter原版通过、SDL失败，status未运行 |
| C5 `b4-complete-c5` | manifest `e995ca9f…174e`，仅MOV0修复及两个回归；fresh app，7/7定向CTest，0.23s，含真实文件Load→完整scheduler→630经验与22987B保存模型 | 仅counter-multi与status两组实际双端通过，未重跑前四 |
| C6 `b4-complete-c6` | manifest `299d38dd…311`，Src相对C5仅field_play.c诊断printf guard；fresh构建及2/2 play无窗口CTest，3.18s | 无新GUI，不冒称此binary重新跑六组 |

C1／C2 app为 `ab43d03ba6d6fd40ac9027328b774608da382b04a0e12ea9363a93f9d99be49b`。C3／C4受验app为 `c6216c7eac8036e27d546b9bb3241d2028c1d7d0256cca9c690b83cc07842309`，driver为 `57a837c4046799f22c0735c6d472d56b261da87895c31b0745f32ee9432fee51`。C5受验app为 `d023b241b6b7bbac346d820f1a1023a375e06e4571552833edb8702d46ea8a80`，复用该driver。最终C6交付app为 `a6ab9e7bee256b21d94c31515ee647fec0ff689bb0cb5fbfca535885366d88a8`。C5定向target编译既存AI／geometry代码有3处C警告，不能记为整轮无warning；主app构建无新增error。

C3沿18d8c→14818→115b6修复物理首raw目标聚焦、Z／小键盘5轮换、121占槽不聚焦、取消／成功清互斥cache；不改范围算法或耗RNG。C5修复game wrapper错误拒MOV0：原4e390起点预算0合法，146d1跳过mover，14b16保留非ff原点，14237仍能对邻敌攻击。真实MOV0夹具保持不变，未改成MOV1绕过；玩家零MOV原地COMMAND／两级取消及完整AI scheduler均补回归。

### 六组真实WGC与完整存档

最终范围为C4四组＋C5两组，共六种、12个正常退出端点，不是C5或C6单轮重跑六组。截图均为HWND WGC，逐消息资源门通过后再比较明确静态文字／列表／详情区域；完整帧、完整FIGANI／音频、宿主精确时钟、共享RNG及跨框嘴型全局相位未宣称等价。

| 场景 | 实际目录（上述根目录内） | 共同检查点／关键结果 |
|---|---|---|
| physical-cap | `b4-tools-c4/build/graphics-artifacts/20261105-163902-830195/` | 45；玩家99封顶，LV1／EX99 |
| physical-overwrite | 同上 | 45；双击各贡献3，最终EX3而非6 |
| heal | 同上 | 69；20经验，caster MP8→5，recipient HP20→42 |
| learn | 同上 | 84；LV4／EX99加5→LV5／EX4，学17；HP28保持而max28→37，MP5保持而max8→16 |
| counter-multi | `b4-complete-c5/build/graphics-artifacts/20261105-171102-868046/` | 67；AI两击后玩家反击，630经验连升6级至LV7／EX30，HP38保持而max114 |
| status | 同上 | 43；487毒伤与481..486七页全自动，HP42→38，六状态0，派生16／12／97／2 |

学习页保留[0,13,17]：17可见／可导航，SDL确认／空格明确拒绝未实现效果，0／13仍可用，13目标取消回列表0。原版未对17发效果确认，不能将其纳入B2已完成法术。状态页发生在原版Round002之后，不补键也不回头等待已过去横幅。

六组每端都执行合法SaveA→真实WAIT改变acted／可恢复HP→Load→SaveB，24份实际快照由strictLE模型独立重生并核完整容器；只沿原checker允许active+2cache及明确测量导航，不豁免camera、roster或未知尾。每场原版与SDL最终编码SAV逐字相同。独立复核1504个capture文件摘要，按各报告既定静态RGB写集离线重放结果逐字段一致；原版50文件、fixture、冻结source／tools／app及清理守卫均通过。

C4前四组进程PID为39072／8708、42752／32244、22684／41000、19608／61364；C5两组为61600／6236、52768／50400，全部exit0且正常清理。C4整个batch仍因其counter失败保留Job40176 worker_exit1，不能篡改为batch全绿；最终两组Job48596为worker_exit0、timed_out0、residual0、forced0、cleaned1、result0。各场真实passed与outer batch状态分别展示，不以正常退出代替业务门。

SDL Round002编号横幅仍未图形对齐。原版非状态场景单列横幅资源门；SDL使用一次有序END1→side1→side0→turn2／side2日志，加后续真实quiet浏览、详情和完整turn2 SAV，不把日志当横幅像素。C6另修AI残留last_attack_valid延迟到玩家菜单后误用HP前值0，产生4294967258等printf total下溢：只限定玩家TARGETING诊断打印并在原位置清同一latch，实际HP38／SAV原本正确，业务、RNG及呈现不变。

### 失败与未覆盖范围

四个失败报告、截图／存档／输入及环境现场原样保留，均不计最终pair：

1. C1 `20261105-160559-518567`：原版WAIT后次级图标实际[37,41,42,45]，旧工具要求Save39；修完整disabled41门。Job53712 worker_exit1，timed_out0／residual0／forced0／cleaned1。
2. C2 `20261105-161413-380253`：原版完成，SDL范围576像素合法但仍聚焦caster而非首raw目标；C3修真实代码，不放宽图像门。Job4560同样正常清理。
3. C3 `20261105-162639-036568`：原版通过，SDL已过新目标控制与99奖励，工具误要求未实现横幅；C4按角色分派且保留后续WGC／SAV。Job40664正常清理。
4. C4 counter：原版31页与完整保存恢复通过；SDL因MOV0 wrapper跳过AI，等待488后段另出现28×19／WGC139f。原场景及WGC错误分别保留，不把截图错误认定为AI跳过根因，不加截图fallback／重抽。Job40176正常清理。

原码／DB和SDL回归还覆盖早退cap、signed贡献与溢出、错误资源、失败／quit前后写点、Load owner、未知loot等；它们不等于全部组合已WGC。运行中成长可读0..67，不代表68种角色逐一原版实测。当前Windows通过，Linux本轮未执行。转职、其余法术、掉落交互、整关结算／下一关与完整FIGANI／音画仍保留各自工作包。

大阶段输入审查仍保留旧非ITEM COMMAND收起动画忽略返回值、宿主quit可能先WAIT提交，以及历史一次END确认NO原因未定位两项风险；B4新进展入口／scroll quit已独立修并测，不声称旧风险已修或全历史输入已复验。

### 真实IDA发布与局部机器码

本阶段一次Domain应用11个全函数语义，真实IDA9.5／Windows lx.dll、Hex-Rays9.5重导全量1307函数、registry240项；保持原9条已证持久类型，3702f无猜测原型。11项canonical产物最终刷新／校验SHA256SUMS，保留旧putchar_0识别差异及真实decompile失败来源，不用token替换伪造导出。21项static／13项IDA产物回归各保实际来源；源码规范地址实际最终545/545，通过日志SHA256 `ee0289abccea613870a261c581c43f485db1224a6f63f56dd7cd4f3f1a3d892d`。

原1b750的AP／DP两块与真实__CHP、原CW127f各执行完整-65536..65535，共262144向量，和实际MSVC C11／volatile binary64后向0cast逐项相同，向量SHA `55dafb59f3c3576c68a43d3f241cc735c5160a4b3eb48e362fb54315d0ea32c2`。12→13、100→114、-100→-114，推翻旧nearest-even及整数115/100；临时增益0.15加1仍是B3独立公式。复现脚本`tools/ida/verify_fd2_x87_stat_scale.py`，真实发布收据为`ida-equipment/run-20261105_111831_688108/b4-publication-20261105_155414_534319`。生成器输入SHA `00ee193541d479dc3f399253c843ad71c8bfe2b69c80f525e8115d33f12a1116`，pC SHA `de1272523ecd0deec9993220a897df540e84bc4fa147eef6926ee0ed9b8fe546`。

B4连接复审、必要修复与局部精简已收口，复用既有范围／效果／dialog资源与进展owner，不建通用事件框架。自包含审阅页位于本机`b4-complete-review/index.html`，实际PNG内嵌、逐场来源可查；B4统一提交哈希在root完成提交后只填该外部页面，不改测试原报告。下一步完整B5及所需A5；大阶段全面历史回归尚未执行。

## B5／A5：结算、准备与下一关（受支持范围完成）

B5按一个完整阶段交付。当前Windows已完成受支持的stage 0胜利结算、持久同步、stage 1城镇／酒店四槽、标题LOAD、出阵及下一关入场；败北播放完整GAME OVER后回真实标题。活动快照支持stage 0／1跨关恢复，四槽只交接准备owner。64个运行单位、96个格式槽和32条持久记录分别处理；32人保存／浏览不等于超过16人的部署已开放。

### 原版运行与图形证据

本机证据根目录为Windows临时目录`fd2sdl-b5-preflight-20261105/`。15类受支持场景分别由以下冻结来源验收，不能记为最终程序同一次运行15组，也不能把原版单端、纯测试或修复校准混入业务场景数。

| 范围 | 实际来源 | 通过范围 |
|---|---|---|
| stage 0受控败北 | `b5-defeat-paired-c2/`，App C2 | 完整GAME OVER复合帧及回真实标题 |
| 准备／酒店／四槽／下一关入场 | `controlled-reference-sdl-c3/`，原版端继承C10、SDL端App C5新执行 | 81个共同业务检查点、四份完整编码手工存档、完整入场与实际活动Save；不是同一次新鲜双端运行 |
| 宝箱、地形、掉落和callback（10类） | `loot-paired-c3/`六场＋`loot-paired-c4/`四场，App C4 | 空槽、满槽替换／取消／拒绝、宝藏金钱、物品／金钱掉落、callback和死亡剧情；每场实际业务→END→Save A→WAIT→Load A→Save B |
| stage 1村民失败 | `stage1-paired-c1-villager-defeat/`，App C5 | 只修改已登记村民hidden位的受控来源；完整GAME OVER及标题，不改变主角HP或身份 |
| stage 1第三回合 | `stage1-paired-c4-turn3/`，App C7 | 受控21人来源隔离旧非玩家AI，新六人实际运行AI；每端34个业务检查点、9页剧情、25,289像素总览及六终点一致；原版另1条WGC诊断不算业务 |
| 自然新游戏到下一关 | C25完整原版＋C27独立新SDL，App C13；另有344图离线复核和C26定向双端校准 | 两端各自完整START117→首关战斗→完整胜利fragment9→城镇／酒店手工Save→可操作stage 1及实际活动Save。不同工具来源、不同Job联合验证，`fresh_pair=false`、`same_tool_snapshot=false` |

stage 1第三回合两份22,987字节存档仅排除新构造raw21..26各自的缓存byte `+2`、九个保留字节`+0x28..0x30`及校验和，其余22,923字节相同。六终点为`(13,5)、(11,3)、(12,4)、(15,3)、(14,4)、(13,3)`。

自然流程的来源与范围：

- C25原版端931条记录为912条业务资源记录及19条WGC诊断；C27新SDL端851条记录为849条业务资源记录及2条WGC诊断。两端均实际完成117页引导、11份轮次Save及结算／入场Save；共26份真实SAV逐摘要、checksum和最终bytes／mtime复核。
- 每端完整持久roster、四槽、未知尾及1,509个已知初始化字节均按自身实际来源核验。21名角色的固定身份／几何、16名NPC的hidden一致，主角均存活；raw2／unit4原版存活而SDL阵亡，作为未对齐随机流的真实战果单列，不豁免NPC字段或伪造全SAV相等。
- C25原版端SHA为`a66846b421b82f7aa0edf7e326a8c6baed7a89743cc60888abdaa827a130b640`。其旧整组因SDL导航误拒仍是FAILED；top `d4679846219086b6cb84a467e2fde786927d9c3a7a7b13bbde4fc178a79831e0`不改写。
- C27独立新SDL端SHA为`8b2ba344966fa26e141de8ad093ef1241a940f841aaa7a1c4b8dcdc16f4d1365`；实际Job为`bbdb2a77bb04ae6ad94a8b4d858c1cfa1b0bb00f995cff23fad2794d77158669`，只运行SDL，不伪称本轮重跑原版或制造跨轮统一Job。

### 修复、审查与测试范围

- stage 1新增单位在shadow内调用原`1b750`重算派生值；Scene借已提交属性，只恢复演出起始位置／方向，避免维护两套终态。`4e680` mode 2预算门、hostile witness及`14b78`长路的占格／ZOC与mode 1次序均依据原码修正。
- 活动Load先在独立资源／地图owner内准备，再一次交接；完成态地图从资源重建，旧档能恢复未开宝箱。标题四槽切FDOTHER调色板并清底图；四槽保存完整32人表，未知字节和未使用槽保留。完整fragment9成功后才提交结算，宿主退出不交半场。
- AI镜头按原`1548e`先聚焦caster、再移动、再聚焦目标，普通`12d7b`不套玩家选择器的unit121过滤；actor-local地形遮挡复现`129ec`／`12ac6`，同时替换Game普通／范围和Scene的全图前景误绘，地图预览接口保留。
- Windows驱动绑定自有PID／HWND，后台PostMessageW输入及WGC单窗口捕获；串行启动，成功、失败和超时均清理自有窗口和进程。后台校准已有原版／SDL及遮挡哨兵实际证据，永久新ownership guard的全套运行仍留给大阶段统一回归。
- 导航资源只比较固定光标格24×24。当前格角色与下邻头部可分别占上18行／下6行，两带必须同时严格通过；不比较伸入固定地形板的身体，不按实际坏像素拟合裁剪或放宽容差。三条有效红绿回归分别覆盖跨板误拒、光标末六行漏检及双角色并存。
- 新判据离线复核C25原版全部344张实际导航图；C26以真实Save6经合法CONTINUE和导航重现右侧信息板，双端光标资源238／252像素各自通过，独立右板完整2,346像素RGB完全一致。C27自然流程又实际越过旧停止点，原图与C25失败图完整相同；旧判据拒绝、新判据通过。C22非法屋顶图仍被新判据拒绝。

本阶段按变更路径验证，不重复历史全套：早期C7定向CTest首轮24／25通过，修复后复验失败项；C9收框退出保护及后续镜头／渲染各自执行定向回归。最终C13渲染相关4项CTest通过，自然流程Python模块63项通过；这些不合并成最终源码全CTest／Python通过数。旧19条主失败、6条修复失败和预GUI启动配置错误均保留；4组修复校准通过与15类业务场景分别登记。

只读连接审查覆盖掉落／奖励／controller尾、准备页Load shadow、跨stage资源与地图swap、完整32人持久表、16人部署门和Scene交接，未再确认其他阻断。结算对白前资源预检与对白后重验各有必要，不为精简删除提交边界。大阶段审查已备菜单关闭／结果提示输入帧屏障及AI物品同步掉落尾修复，私有真红绿通过，但尚未并入本阶段或执行统一历史回归。

本阶段经IDA Domain发布确认命名，registry 260项、全量1,307函数；原有3条反编译失败保留。发布产物的14项校验及当前617条源码地址检查分别通过，生成物清单最后刷新；不因此重开既有静态LE结论或宣称严格运行时BSS已通过。

### A5与剩余范围

App持有BGM，在已确认的stage／phase、城镇10、酒店13、Load及离场边界请求曲目；同曲不重启，既有stop由render执行四秒淡出。C25实际30.704秒原版录音经六参考／五固定窗分析和正式来源绑定，支持19→12→19身份／切换；历史C10的10／13／19和失败自然候选录音仍按各自有限来源保留。指纹排序不是运行期指针证明、SDL实时声音配对、精确墙钟或A7音色保真。

B5受支持范围完成后，先独立提交本阶段，再统一审查B0—B5及所需A5：全面复测历史功能、修复并精简，验证后提交，再继续B6。当前Windows验收；Linux本轮未执行。

历史“首次END默认NO”仍未定位：旧图仅证明当时为NO，旧输入记录缺SDL实际接收及首框呈现链；补跑正常或关闭帧修复不能关闭该根因。完整FIGANI／音画、共享RNG、水色同步、剩余玩家法术、stage 1整关战斗／结算、商店、超过16人部署及stage 2以后仍未验收。

## 2026-11-06：B0—B5／A5 统一审查与历史回归

本轮在 B5 提交 `557155d98303d38e7740035ffa4c7ce5a5fe7e17` 后统一审查全部历史支持范围，修复与本轮证据作为一个提交，不把候选修复拆成微阶段。B6在隔离副本准备，不混入本次源码或通过数。

### 修复与精简

- COMMAND 收起过程中检查动画错误、Game 首错及宿主退出，先停止再决定是否提交下一动作。SYSTEM 收框复用一个 helper，保留子菜单重开与回浏览的不同输入帧边界；结果提示回浏览时不把刚呈现的按键继续送给下一层。Load 不重开旧菜单，不移动恢复后的焦点或清经验贡献。
- AI 物品的同步掉落终止不再被误当作待续态。只有确实存在奖励／掉落续态才延后 common tail，普通同步分支恰执行一次。业务测试覆盖 mode0／1、尾状态及失败边界。
- 实机驱动从逐键定时串改为每个页面／移动的资源 ACK。CLI 首关焦点实际在 unit0 `(7,20)`，先移到合法空格 `(6,20)` 再打开系统菜单；Save、Load、取消及继续行动检查完整资源与源文件 bytes／mtime，不再等待实现已不输出的旧日志。
- March 只为原始 START 的 raw11／raw2 固定交战建立原算术有限域，逐对检查损 HP 与最后一击经验。不允许任意 EX 字节变化；原版本轮无伤害、SDL 有反击伤害不冒称共享 RNG 或已观察到原版非零奖励页。
- F1 全帧候选先用 PIL 原生差值拒绝明显失配，成功仍检查全部 64,000 像素及原最大 RGB 差3。观察调度按实际采集成本错开目标相位，保留15秒总期限和水色冻结门；不改游戏时钟或扩大容差。

### 无窗口与输入证据

完整 C1 的68项 CTest 首轮60通过、8失败。修复后 C2 定向11项中9通过，剩余 March／SYSTEM 为旧夹具预期，分别由 C3、C4 修正后通过；68项按来源联合覆盖，不能写成新一轮68项全绿。C2 的真实 Windows 输入项通过，并新增11项无窗口输入边界回归。所有旧失败保留。

工具 Python 完整580项首轮有2个失败方法，均为输出路径安全门变更后的旧测试；1项历史 LE seed 依赖缺失跳过。修正后仅复验失败2方法，含19个 runner／38个路径子例，无进程启动；另对本轮后续改动运行 Suite57、March24、F127项定向测试。确认观察先以58项模型收紧NO，再依据实际blink-off证据完成61项定向回归；旧No→Yes模型真红及中间过严候选保留。未为凑全绿重跑580项，历史 LE seed 跳过不作为运行或图形验收。C2 源码规范地址621条全部通过。

### 当前 Windows 图形来源

所有候选共用冻结 C2 程序 `5be08db27c6c674cba5f949ec7674a383eac48c0a1eb175aa94e729ed59d2f3d`，不覆盖公开 B4 程序。driver 为 `8116d65f48f71495e37576e534114929030021c421bb94161eb56dbffbc53615`；原版 DOSBox-X 为 `a82adcfa7af2784566cfeff74c077cefed08505396db2948b7e6aa7d4cee0832`。

| 来源 | 实际结果与范围 |
|---|---|
| C2 | 首8项通过，含后台输入四端点、遮挡哨兵、WGC与声音校准；first-save 因旧焦点路线失败 |
| C5 | first-save 新实机通过；后续旧 DOWN 日志等待失败 |
| C6 | 后续11项通过；March 两端完整运行和保存，旧 checker 误拒合法反击经验 |
| C6 离线复核 | 新有限域门复核原有完整 SAV 与55个资源检查点；只有派生复核，没有新启动或新双端运行，旧 FAILED 保留 |
| C7 | F1 原版通过，SDL 已有完整资源帧，但固定目标相位观察超时；失败图与26个有效相位样本保留 |
| C8 | 剩余59项全部通过；含本候选完整原版／SDL自然首关，外层 Job normal0／residual0／forced0／cleaned1 |
| C9 | 仅 F1 原版→SDL定向配对通过，86.031秒；完整64,000像素／水色冻结门保持，正常退出及清理 |

本轮81项登记范围现已按来源覆盖：C2／C5／C6的20项实际通过、C8的59项、C9的1项，以及C6 March实际记录的1项离线重判。后者没有新启动，不能写成新一轮81项全绿或81组双端配对。所有实际批次正常关闭、无强制清理，原版资源与公开程序守卫通过。候选目录以 `%LOCALAPPDATA%/Temp/fd2sdl-major-regression-20261106-cN` 登记；F1 C9 为 `fd2sdl-major-f1-c9-20261106`。

C8 summary为 `b26d1ef89798b7bca2fdfc2aff9f69695c38f8674eda6c1716942a5dd0a924f9`，外层完整收据为 `ef5d8bfc8f06abfdd5e366d3ef7514dddd57a5f41625928bfe022c0f6b5b7420`。C9配对为 `ab03d0fd79d0af7bb5041d580428cd43b169445e5b64db15efa8475111aac42b`。旧C2／C5／C6／C7 FAILED不改写，修复后仅复验受影响部分。

### 自然流程与 A5 的本轮来源

C8两端分别实际完成117页引导、首关胜利、酒店手工保存与21名角色的stage1交接。原版555条检查点含542条业务及13条类型化WGC诊断；SDL849条均为业务检查点。两端分别保留9／11份真实轮次存档，包含结算／入场的归档共12＋14＝26份完整SAV，按各自完整roster和1509个已知初始化字节核验。12名初始角色固定投影相同，16名入场NPC含hidden完全一致；raw4／unit1在原版隐藏、SDL存活，其余raw1..4的hidden相同。不要求共享随机流或跨端整份SAV相等。派生只读复核收据为 `c86fb4352313aeaa851368469a6393d30f08738a6c078f0cc74101d374aadf68`，不增加实机场景数。

资源值差异有具体端内来源：原版第3轮走到 `(3,12)`，通过421确认和426钱格提示取得3000，并置对应completed位；SDL没有取该钱格，第6→7轮由raw15／unit96的 `[1,e8,03]` 掉落和435提示取得1000。原 `0x190ac` 与 `0x1aa1d` 分别实现这些写点，胜利对白／roster交接没有固定资源奖励；不把差异归咎于漏写Win奖励，也不由终档猜原版该敌的最后击杀者。

C8新原版录音33.322秒，SHA `4cfa4e2fd88166e1dc972ac75c44db09f63054fa754955f923c621176d2daead`。离线原样复用C25六参考／五固定窗口，首候选19／19／12／12／19，支持既有场景编排；前两窗19与12接近，不单独声称唯一身份。新派生收据 `c7fcd1e0c7b2164179e0dcd02c82b5fd9ab3fb04c46fa39604fb9a71162c3884` 不改原WAV或旧参考，也不扩为SDL音色、精确切曲或A7通过。

### 确认过渡与仍未定位的历史症状

复审收紧确认观察：保留最早完整按钮原图，任何明确NO都必须立即失败，不能等待后来的YES洗掉；未高亮的48／51是过渡或闪烁相位，需在同一有界观察中继续等明确选中。已有11张实际首帧均为YES，离线重判不增加窗口或配对数。

只执行一次私有Use12诊断，原版→SDL及外层Job均正常结束、守卫通过。SDL真实146条接收／消费／呈现trace未达到4096上限：open为selected0／yes1，首present为48／51，最早WGC为49／51。本次未复现旧48／52默认NO，不能关闭旧原因。诊断比较为 `5de542b3d109b3b78f301545ab80ee71061a4ace59267063bbf4f1cfedc384bc`；它是额外诊断，不增加唯一场景覆盖数，也不把earliest-observed当literal首present。

连接复审未再确认其他生产阻断。没有根据未定位症状猜修复或建立通用输入／事务框架；保留旧失败与新诊断，今后复现时继续追接收链。当前大阶段的受支持功能、审查修复与历史回归完成，按计划转B6；完整音画、共享RNG、水色同步、其余玩家法术、stage1整关／后续关及发布许可仍按原工作包处理。Linux本轮未执行。

## B6／A6：FULL 战斗演出与动态音效

2026-11-06，B6／A6的受支持实现及既定最低原版验收完成。19种场景分为3个核心最低帧／SFX、15个AI最低casting帧及业务／SAV／采集、1个ITEM评分业务正证；不把19种场景一律写成完整图声通过。只验证本阶段及直接受影响路径，未重新执行fresh74项CTest或全部历史图形；B0—B5及此前统一历史回归仍保留原来源。

### 实现、RNG与资源边界

已接物理FULL、正式玩家现有0／13，以及已确认AI FULL0..9与合法多目标。共享纯FX状态机按原phase推进，Game使用真实RNG并记录有限序列，renderer只读重放；费用、效果、HP插值、奖励和清理保持原写序，不由呈现重复结算。ID9仅side0单目标支持，非0side／多目标在首提交前拒绝；不扩大玩家法术能力。FULL完整播放与其主动跳过、静音及null音频保持同一业务结果／RNG；原版SHORT与FULL的攻击次数、FX随机调用本来可能不同，不要求两条原路径等价。

动态声音复用已有PCM owner，物理bank按47+selector，普通FULL法术为82／82／83..90，恢复术13为80。`sfx_play_secondary @0x25b45`已确认使用独立handle，不是调色板遮罩；同VOICE字节别名只证明声音等价类。最低声音门使用预先声明候选和录音内顺序，未宣称全部cue、七次FX0实例、精确墙钟或A7。

### 定向无窗口与发布校验

C2直接受影响12项CTest通过；C3新增audio／capture-series两项并重复两项，C4五项直接回归通过。评分审查C5新二进制构建成功，但field_ai_commit旧期望8失败并保留；HP1／300、阈值250的多目标应为18+8=26，不是等阈测试。修正该夹具后，C6最新field_ai／field_ai_commit两项1.125秒通过。均按各自候选登记，不求和冒称单轮fresh74。

工具八模块最新结果联合为161项：collector43、C5 Suite68、frames15、C5 SFX8／声明11／core-SFX7／输出路径2，以及新重叠角色模块7；这不是一次运行161项。C8定向collector43＋重叠角色7＝50项通过，原旧门反例与528像素离线资源复核保留，不替代实际图形验收。

本阶段canonical发布含FULL0..9的FX／资源证据及25b45真实更名`sfx_play_secondary`，不是只有cursor注释。最后增量仍为279个登记入口／1307真实函数、0更名，单独同步1cff0／2ff01／15311／1548e的四条cursor／display注释；14项发布检查、684个源码地址检查和最后11项清单通过，不是runtime验收。最终发布收据SHA `7b5cb5985692114b74879f3458f7b4b5a4821cc64f32c26b9d48d45032c44246`，未保存猜测ABI／类型或手改生成物。

### 已有实际与派生证据

| 来源 | 已证范围 | 保留边界 |
|---|---|---|
| C5物理原报告 | 两端业务、保存、采集及最低SFX完成；旧图门仅认首个染色33的单tick而失败 | 原FAILED不改写 |
| C5物理同raw离线复核 | 按原资源接受非零shake5..1受击族；攻击／受击各实际完整64000像素pair，bad0／max0 | 派生比较`92f516485cef0e4ac9837d074df468f2be1772d37f1b6281ce7e529bd75316a5`，无新GUI，不冒称fresh pair |
| C5玩家Magic0 | 两个不同施法姿态的完整帧pair及两端最低SFX、业务／SAV／清理通过 | 比较`6ec5121f752114ecf6d4ff38567ad716fa0a0570390202b51e08b67abaa80fe4`；七次FX0实例与全帧序未验 |
| C5恢复术13 | 采集／声音通过，完整帧门失败；已确证多余cursor，其余纯帧差异来自合法idle相位不同 | 原FAILED与原图保留，不用混合57／58帧作同相正证 |
| C4修复＋C6恢复术13 | 纯Game effect_background修正；同series合法新idle1／2→0后唯一确认。两端实际FX57／59各64000像素bad0／max0，三个有序SFX组及完整SAV、清理、晚守卫通过；原版81／SDL167连续帧 | 比较`e75b587ba02f80cdba1a42d6fb5fc9b93c9415f9a92cfd8054634dedddc27638`；ACK最多15秒且非时钟／最终相位保证 |
| C5 AI00失败→C6夹具修正 | 旧复制行为0xCE低位14走13a9f默认无动作，MP250未变而期望248；新夹具明列behavior0及八空槽 | 保留旧失败，不归因为MP费用或staff选择bug，不放宽完整有限SAV门 |
| C6 AI十四项＋C8仅08multi | 全15项业务／SAV／采集及每项两个完整casting帧pair通过；08multi原raw0 HP999→601、SDL999→567，raw2均999不变，各端独立合法随机 | C8比较`254947aa69419c501f1f63e35569dddb16edc28b50f91ab50a2c8d9fc99fc7d7`，137.797秒、owner13392／root21336、全0／cleaned1；C6旧失败`473bfee483f6def86c8eaa89653bd081dde78016c101a3832d36dcf597964752`及160.297秒清理记录不改写 |
| C7评分等号回归 | item56／code20／value0，Hero HP=max50；原版50→5、SDL50→1，MP250与单槽不变，完整SAV／采集／晚守卫／清理通过，91.891秒 | 比较`f55445ac135c343acc424648953946a1e019fecfa514d167e4d6fafe8ea9a719`；不宣称全部FX／声音，合法miss仍单列inconclusive，不重抽 |

连续WGC与进程音频共用本轮owned Job，ready后才唯一提交，stop／wait／正常关闭后检查最终来源与原版bytes／mtime；静音或无声不能替代有效录音。原生WAV没有首packet QPC，精确发送时延仍待证。物理受击族和恢复术idle边沿的严格资源门见[GUI规范](../tools/gui-testing.md#b6-战斗演出采集与判据)，没有扩大64000像素门或容差。

### 连接复审与未覆盖

本阶段审查收口已修旧AI物品评分15880遗漏：阈值≥目标HP给18，否则给8。六个临界值与两个多目标用例检查评分及units无写，随后以C7真实ITEM动作与完整SAV补充实机证据；不再延期至B7，也不混为MP费用问题。root／UI／EVI连接复审及两处无效果close精简分别保留范围，不由此宣称全部未覆盖代码无bug。

08multi旧失败属于重叠角色的资源复合漏项：同格原raw0／raw2应按原raw顺序绘制，独立原资源组合528像素bad0，反序270坏点、错位447坏点。共享helper修复经七项独立纯测、旧门失败反例及C8单场实际复验确认；不修改C6旧FAILED，也不重复其余十四项。

15个最终AI比较报告的`independent_frame_pairing.ok`均为true，每项两个实际casting完整64000像素pair，共30组；目标FX与完整声音未逐项对照。三核心另有6组，共36组实际完整帧pair，其中物理2组来自同raw离线重判，没有新增运行。第19项ITEM评分没有casting pair，不计入30组；它只由两端实际HP下降、完整SAV及MP／槽不变证明动作。

AI15场及评分场仍保留默认`ok=false/collected_only=true`的报告合同；实际存在的casting pair单列登记，不将其升级为全部FX／SFX通过。所有窗口及owned Job均正常清理，无超时遗留／强制清理；验收运行期间公开程序保持原a6ab候选，最终项目构建在验收结束后执行，不把私有候选反写为运行期间已公开。

本阶段实现与既定最低验收据此完成，所有实际／派生来源和旧失败分别保留。完整动画／每个目标FX、完整音频与精确时序、全局共享RNG、水色同步、A7、stage1整关／后续关及Linux本轮实机仍未覆盖；历史END默认NO原因也不因本阶段通过而关闭。

## B7：stage 1 整关与 stage 2 交接

2026-11-06，本阶段以自然 stage 1 实战、村民存活分支和本关三箱，到 stage 2 准备、真实入场及存取为边界。对应实现作为完整 B7 提交；阶段内只验证新增、修改及共享代码直接受影响的路径，后续大阶段历史回归另行登记。以下数量按实际候选分别记录，不等同于最终程序单轮全部验收。

### 实现与开放范围

按 `field_stage1_victory_play @0x22f37` 完成六名村民全活／有伤亡的30页结算。全活奖励198按 `0x1c220` 给首个 side2 空槽，允许隐藏／acted角色，全满静默无奖；group4、脚本14..16和完整结算成功后，先构造持久角色8，再按 `0x11506` 同步完整队伍。投影和预检不改来源、Game或RNG，坏前置及未完成演出不提交准备 owner。

stage 2 按真实入口 `0x32e8c`，使用六个部署槽、脚本18、group1九人、脚本17／19、15页入场与最终方向重置；角色终态、焦点和最终camera `(3,18)` 由完整说话人／PAN链推导并测量，不能在纯Game准备阶段提前断言Scene终态。准备页使用 `0x623ac` type2和FDOTHER62；支持酒店四槽、标题／城镇Load、活动Save/Load、详情和总览。第三回合 action9 `0x34818` 未实现，END与玩家战斗提交仍拒绝，不以逐人WAIT越过能力门。

### 定向测试与逆向发布

12个直接相关CTest按各自候选通过，包含准备／提交、存档、单位与控制门、自然规划及Scene／App／准备页。旧夹具对配置choice消耗RNG的错误假设、Stage2假资源和提前对纯Game断言Scene镜头的失败均保留；修正这些预期不改变原版规则。Scene光标复审另有真实旧代码红测与修复后定向绿测，见下文；不将跨候选结果求和冒称fresh74项全量CTest。

Python最终单轮9模块124项通过，91.767秒，零失败／错误／跳过：B7新增7模块54项，直接受影响的旧natural63项与Overview7项。301个源码和50个原版文件的SHA、大小及mtime前后不变。第一次同样124项通过但跨文档字符串修改的守卫失败保留，最终只取重新冻结后的C2收据 `da1345c8b4823c8771bd0bf281cce45f9715905198e1d29ef78a9ebf0272a1c2`，不重复计数或当作GUI。

初次canonical发布新增5个独立入口、3个语义更名及15处注释；最终284登记入口、1312真实函数。结算显示复审再以隔离Domain／idalib作0更名／1处真实注释更正，所有去注释body、names、strings及7948个fixup不变；保留旧3个反编译失败，不保存猜测类型，不改3702f、SP、边界或原DB。14项发布检查及700个源码地址检查通过，最后刷新SUMS。最终收据 `33075a6cfc5a47e2d91bbb5e0f84c430288c3b5363092c6123a43dc0e8edd2e5`。这不是重做LE21项或新增运行时验收。

### 实际配对来源

| 来源 | 已证范围 | 比较报告 SHA256 |
|---|---|---|
| C8背包全满 | 30页结算、静默无奖、15页入场、手工与活动保存 | `371f8a96e34ec115cd82aff259050df974c50a0b802b8e02c753774e55218d78` |
| C9 stage2-storage | 每端四次手工保存、两次活动保存、四槽／slot3 Load及取消、详情和总览 | `4c3378522873262faadaa379d8d2a0026e355136008e1fc8a94ad88975b3c710` |
| C9 villager-defeat | 全灭判定、GAME OVER、回标题及原存档不变 | `7ff0dd5d617835074b9c692cea00fb861aad41a351e80b01c124a34e2457a71a` |
| C12 chest129 | 合法移动／三次WAIT和END／turn7真实Save／只领一次 | `a8a81d71a993b7f654afb7bf3a99dea5b86f5f25071fdb3bdc71d50bf2e7104a` |
| C13 chest201／193 | 同上；201屏外WAIT选择只记未观察到，后继及库存／Save正证 | `3ef0fdf60ec734025b1c41c590b6faf5463f3e4a9adf784fb06fb7b31dd4e9f8`／`fe35b78bf315985e974f29334b0ccb88160b8886743c778d69371bbb4bcd749e` |
| C13 natural-stage1 | 原版20回合／19份逐轮Save、SDL21回合／20份Save，各9页中途／3页死亡／30页结算／15页入场 | `1f9c243f2580b8bb7fa6a96ed69627328928e85f37a4c25aa2f1ee01fdff6e1e` |
| C14 all-villagers | 修复后完整全活结算、198、准备／入场／保存及首对白无光标格 | `bb4629db06d5e41703ad1a6f9d05e9b6287623b9472d07a372873f2261d44d44` |
| C14 villager-loss | 修复后伤亡分支及完整交接／保存、首对白无光标格 | `f758d5c0450aa713623679f96e31c32b0a60bcd18bc9767076994516be943717` |

共9种唯一场景，C8原全活／伤亡加C14两场共11份实际配对报告；不是同一个最终程序fresh9场。C5程序SHA `91260e95b53fc8666457d3e3cdf71ff2b4e8f0a0747839f9f6e4c2d0c810a673` 的自然项仅保留该候选业务／剧情／存档范围，C14程序 `98e5428108e62265f5266951c1e7d9fb42865c4fd5a8d18a46f25e0808abac90` 证明共享结算显示修复，不将旧图升级。自然两端各自实际保存stage2／turn1／15活动角色／6持久成员，保留原阵亡队员与本端成长／物品／资源。自然全链2672.015秒，Job42624为worker0／timeout0／residual0／forced0／cleaned1。

### 失败定位与连接复审

首受控原版酒店Save高亮可能处于所有按钮均正常色的闪烁相位。门只将其记为selection未观察到，随后唯一Enter及完整picker0证实Save；明确错误／双选仍拒绝，不多发输入或改写旧失败。第二次原版完整结算和入场通过，但原工具错把最终camera写为 `(3,17)`；完整text speaker链确认实际 `(3,18)`，修正oracle而非改Game迁就预期。

C9三箱失败已定位为旧受控Hero的MOV40造成原版递归工作区越界：第33个8字节栈帧覆盖mode，返回0D被解释为向右。原构造表和自然入场存档都证明此Hero的MOV为4；只给三箱夹具按身份／装备恢复MOV4，原roster及其他记录逐byte保留，合法预算1..31，每步真实成本1。旧失败保留，不在SDL克隆越界行为，也不把改夹具当生产寻路修复。

C10自然原版在第四回合死亡对白停止，原因是工具只读前11个片段，而FDTXT2实际有17个。11／13／15与12／14／16各是一组资源完全相同的村民死亡单页；新reader只确认资源等价族及一次ACK，明确不能从相同画像／文本推断唯一死亡raw。原有mode3剧情链已能处理，不改生产Game，也不重抽RNG或复活旧阵亡角色。旧失败和原图保留。

连接复审发现并修正结算光标：只有首fragment6／7入口继承caller模式；`0x165ac` 在聚焦前清0、之后写1，但文字和闭框仍恢复无光标的缓存背景；后续 `0x135dd` PAN清0，group4和script14均不写，所以fragment8入口0。旧Scene一直沿用入口指针，旧测试也自洽地误断fragment8继承。用真实资源及入口0／1、raw6显示／隐藏、无说话人和旧开场NULL路径建立红绿回归，Scene只增加本地借用与背景投影，不改Game、库存、RNG或旧开场能力。

旧C8首结算页独立重建同一576像素区域，原版唯一匹配无cursor／idle1，旧SDL唯一匹配有cursor／idle0；光标差异为131像素，各端idle分别识别，不拿动画相位差当bug。原图和失败派生收据 `aa09004e950a55495f175e63aad035422d3ecf5ccd269dd188b8020e6f4b31d2` 保留。C14两分支各完成新原版→SDL配对，两端均唯一匹配noCursor族，576像素bad0／max3，原带cursor反例仍差131点。两份资源派生收据 `f15c5602bcd63c6c6e10b167250c517c276019a6792d10c1170b6f6cd6e4691e`／`3e379a28dc6da933cae34b175c30d6b6aea10741658df3fdbf7f9b4f4a3206e2`；C14两项Scene关联CTest通过1.64秒。离散页截图不能证明聚焦／PAN每个中间帧，后者仅由原码及定向CTest支持。

可再生产门已纳入 `tools/verify_stage1_settlement_cursor.py`，另4项定向纯测通过0.030秒，不叠入此前单轮124项。新脚本对C14两场实际图离线通过，对旧C8含cursor反例保持exit1；三次来源／原图／资源晚守卫均通过，收据 `fb20d8a6ff1b79a49a88299b724225b64759b70e1a3070b17ce50fe444ec9fdd`。这三次派生没有新增窗口或实际配对。

### 验收边界

每个配对场景由原版先行、SDL后行，使用WGC HWND与后台PID／HWND输入，正常关闭后再启动下一端；Job及原版、来源、工具和公开程序晚守卫分别留证。对照检查已声明的原资源、业务、完整持久表／槽及未知尾；cache、精确构造器未初始化字段和原版metadata heap尾的豁免分别列出，不宣称整份跨端SAV相同。

图形来源按候选保留。自然战斗以各端真实成长、伤亡、库存和奖励为准，不要求伤害、HP、战果或共享随机流相同。水色同步、整帧RGB、聚焦／PAN完整时间序列、全部FX／声音与精确切曲、后续stage战斗及Linux本轮实机仍未验收。历史首次END默认NO原因继续未定位；新通过不关闭旧症状。已确认的AI11成功施法后错误提前返回，以及AI5到箱后地图／行为byte与前置focus遗漏，留给整阶段提交后的大阶段统一审查修复，不混入本B7提交。

## B0—B7 纵向大阶段：统一审查与历史回归

2026-11-06，基于B7整阶段提交 `a72c54a` 继续统一审查。以下定向修复、无窗口检查和实际图形来源分别登记；历史实机回归完成前，不以部分通过宣布整个大阶段结束。所有实现留在原项目会话，最终统一提交，未推送。

### 审查修复与精简

修复 `field_ai_unit_execute @0x13a9f` 的behavior11续态：成功施法后不能直接返回，仍需重新查询物理候选；priority≥6时提交物理攻击，再进入common tail，否则按原 `0x13e45` 调用 `0x14121` 尝试移动，未实际移动才按 `0x13fd4` 恢复。仅删除一行错误早退；`cast`仍用于已提交魔法后的失败冻结，不重扣MP、重放RNG或重复common tail。独立旧实现红测包含MOV0恢复、MOV1移动不恢复、无witness／noop与两种story续态；旧三类提交失败及物理成功用例也保留。

修复同一handler的behavior5三处遗漏：`0x13c39` 在完成位／查格之前聚焦，即使随后无动作也保留原镜头；抵达箱格后按 `0x13d1b` 调用 `0x12263` 立即重建live map，不等Load才显示开箱；`0x13d20` 整byte写07，不能保留旧高四位成为A7。提交前借现有资源作只读开箱帧预检，不建立第二张地图owner。满包仍完成开箱；坏帧只允许已发生的前置focus／display，不写库存、地图、完成位或RNG，并冻结错误防重试。真实248／249资源、满包、A5→07、坏249、已完成且无动作仍聚焦等七个业务真红已记录；早先资源cwd缺失仅是harness前置失败，不计业务红。

删除Play中没有正式入口的旧MANUAL_SLOT确认／SaveLoad／取消／方向／context分派，共60行。唯一进入该state的API只由两个Game单元测试调用；正式四槽继续由`field_prepare_play`负责。保留Game纯picker、codec和后端，不删除仍被active Load使用的音频／UI辅助函数，不为精简重做架构。

连接审查还覆盖正式输入逐帧消费、菜单逐级取消、active/manual Load影子owner与旧Game释放、stage2存取，及主程序／Play／BGM／FIGANI／PCM的借用、销毁与跨Load地址、提交后失败／quit／skip。除上述修复外未再确认阻断；这是本次连接范围，不保证全库或所有音画分支无缺陷。历史END默认NO原因仍未定位，不因本轮YES或既有诊断而关闭。

### 真实新增业务配对

| 场景 | 实际证据 | 范围 |
|---|---|---|
| C1 ai-behavior11-recover | 原版→SDL，78.422秒，比较 `054618bd9f23f343b0d3505a75a728b35caf51399c8e0e1fe0a1ece0bef4f400`；Job57476全0／cleaned1，源、原版、程序晚守卫全通过 | 成功施法后恢复到HP60、MP8，目标伤害与完整SAV；不由结果推唯一witness/RNG调用 |
| C1 ai-behavior5-chest | 原版→SDL，74.000秒，比较 `d7644de09ed9003e7e8afeccc9b40e154e64d5d6a4798ec5bc5335af32d94fa5`；Job38264全0／cleaned1，全部晚守卫通过 | profile5／MOV1真走(6,13)→(7,13)，Save前实见live249全576像素，含下格Hero头与raw11原序；整07、192单次入库、完成位和完整SAV |

两场严格保留 `ok=false／collected_only=true`，以 `business_regression_passed` 判业务；不启动无关FULL／WAV采集，不扩成完整FX、音效或共享RNG验收。无目标focus只由原码和定向C测试支持，没有为它伪造不移动的实机场景。

### CTest与Python联合覆盖

C1全新Windows构建启用必需GUI测试，74项CTest串行执行，71通过、3失败；root Job63008正常清理，源码／程序／公开B7程序／三套原50文件守卫通过。C2只修并复验3项失败，3/3通过；其所有生产代码与C1逐byte相同，未重新跑已过71项。这是当前74项的联合覆盖，不是单轮fresh74全绿。

三项失败均为测试前置过时：Title替身仍把stage2当unsupported，现按已开放的stage1／2消费准备owner，保留stage3拒绝、错误／退出owner无写和未交接准备的VICTORY拒绝；Overview仍错误禁用stage2，现验证0..2开放／3拒绝，并保stage2 March／END禁用、完整Game及RNG无写。两击奖励回归默认FULL导致weapon71合法三击、Hero36／RNG11；显式SHORT后仍严格Hero38／两攻一反／RNG9／奖励630与升级、零RNG预检。Load／END仅在派生RAM中声明SHORT并验证导入／保存均1，原START未改。

C1全量Python实际执行773项，769通过、1失败、2个错误子项和1跳过，旧日志保持原样。C3只补验两个修正模块及原跳过单项，共15项通过，零失败／错误／跳过；按当前方法ID去重，773项全部覆盖，没有把15叠加成新全量数量。生产Python与C/C++代码均同C1，改动只在测试。泛化输出路径夹具补B7必填source-report、合法scenario及隔离的prepare／Job前置，保留证据目录在mkdir前拒绝、外部路径真到mkdir、禁止Popen／run的原断言。Overview测试按实际601／602资源补stage2正例与坏像素／缺LMI／截断字体反例，保stage0／1和stage3拒绝。

原跳过项所需code0.bin与relocation manifest仅从E已有资料只读复制到新C3开发输入中；核对已发布raw object1切片及既有fixup来源，未生成新LE／DB或重开静态fixup验收。输入SHA分别 `c0514237998e1b9b094d36222b549439d696f5ffd092131f205e08e14c7330fd`／`a8180201c292f3efcfb18e6ccc5d45e9bb6b6b71c6cc636d8e272868203fbe54`。该单项只回归legacy工具schema，不回灌canonical命名、猜测ABI或3702f类型，不声明新BSS运行验收。

### 正规逆向证据发布

复用B7最后隔离DB再克隆，仅更正13a9f的一条真实repeatable comment与对应命名输入，0更名／类型／边界变化。最终284登记、1312真实函数、全部去注释正文相同，旧3个反编译失败不增加；原DB、raw、持久类型、SP、chunks及3702f守卫保持。14项发布及702个源码地址检查通过，最后刷新SUMS。最终收据 `d4571c6552ab9cf48072e8a0236323eaf85b40d92f869a02b70df9946ec5bf5f`。

首轮多传`--no-analysis`使旧exporter跳过函数／名称／字符串统计，schema校验明确拒绝function_count0与旧TSV1312不一致；partial-export与失败日志保留，去掉参数后重新真实全导出。`--idb`分支本已固定不跑auto-analysis，数据库及反编译正文并未因此改变。没有拿刷新SUMS当作语义或运行验收。

### 历史实机回归与最终边界

首轮完整默认 Suite 实际登记 81 项，前 80 项通过，最后的自然首关在原版实战败北后停止，SDL 自然端未启动，后三个 B6 核心未执行。此前 117 个开场等待点和六回合真实存档均保留。Suite 摘要为 `3975db2fbf3912457c8f22f5cf9f0c02de82e4c302ee2e1b335743850e830487`，原版失败报告为 `acf1ac610f86a8085f71c7dbe7896f3cbb31635c10af3e533315f5a61559251e`。外层 Job44972／内层36416均 worker1、timeout／residual／forced0、cleaned1，全部外部来源晚守卫通过；不能把这轮写成完整通过。

复盘发现原自动战术每轮只执行一个 primary，其余固定 WAIT。第三回合法师有合法火球候选，却被主角撤离挤掉，带着 8MP 阵亡；第四回合援军的攻击机会也被同类撤离挤掉。修正为必要时先执行主角的普通格安全 WAIT，再从只投影坐标与 acted 的影子中选择一次效果动作，其余仍原地 WAIT。先行 WAIT 排除财宝、箱和 lookup1 事件，避免猜测资源或剧情结果；同轮最多一个效果动作，保留真实目标和治疗观察，不预测 HP、击杀或 RNG。原版已有 actor acted 后禁存档，因此仍只在实际 phase 完成后 Save，不伪造逐动作保存。B5／B7 使用同一显式动作合同。实际第三、四回合快照的旧查询两项失败、新查询两项通过；新 CTest1项及 Python 定向9项、随后新增失败屏障1项分别通过，尚待实机验证。

随后首次执行物理 FULL 核心，两端业务、完整 SAV、正常清理与全部来源守卫通过，两组 64000 像素帧也配对；最低声音门在 SDL 端失败。原版52/3身份分数为0.987897；SDL整段5.65秒仅有−1／0／1 PCM，分数0.067975与margin0.040705均未达事先声明的0.8／0.05。用户随后确认自己已在系统混音器静音游戏进程，原因是测试声音太吵。旧失败保持原样，不降低声音门，不解除用户静音。事后 CoreAudio 快照没有找到该已退出进程的 session，不能把后来的 API 快照当作当时音量证据。后续以安静的实际应用混音采集补验，设备输出另行登记。

B7 新全员存活复验停在结算索引11。离线穷举同一30页资源门，保留的超时图唯一匹配索引29（fragment10／token12），因此不是停在旧页面，而是中间页面未被逐页验收。日志只证明 driver 各投递一次 DOWN／UP；其间目标曾成为前台，并出现两次 WGC 几何变化，尚不能据此确认跳页根因或归因用户。补充防护仅给 owned HWND 加 NOACTIVATE 并读回，投键前后发现目标意外成为前台时先释放本轮按键再失败，不抢回焦点、不改变全局键态。旧 driver 在同一新测试下真红，新无窗口 CTest1项、协议9项通过；之后共用静音启动参数加入两项回归，协议11项通过。这不等于完整硬件输入隔离或跳页根因已关闭。

计划据此调整：保留已过80项，只补验上述改动直接影响的自然战术、输入与声音路径，并继续尚未执行的历史项。原版静音录音校准已实际通过；B6其余核心／附加16场、B7新配对和光标离线门的来源、数量及最终窗口清理将在完成后补齐。当前大阶段仍未完成，不用旧B7通过、单端采集或纯测试补齐缺失实机证据。

### C4：安静采集与受影响路径

原版校准使用 DOSBox-X 自身 `mixer nosound=true`，不更改用户混音器。实际原生 WAV 仍有预期52/3音效，NCC为0.987891，沿用事先声明的0.8／0.05最低门；25次按键均有后台投递证据，两个HOST_W正常闭合。Job35512正常结束，原版、源码及二进制守卫全通过。此项为原版单端校准，不计SDL配对或A7保真。

SDL新增真实音频回调的应用混音采集：只render一次，在成功提交后复制抑制前PCM；测试安静模式向设备提交同长度零样本，不改变bus、曲目位置或游戏业务。回调不做文件IO／分配／无界等待，工作线程写WAV；溢出、非有限值、无实际回调及不完整关闭均失败。报告为48kHz PCM16、`pre_OS_session`，明确`device_output_validated=false`、`A8_validated=false`，不伪称WASAPI。默认正式核心取证使用`app-mix`，仅显式`process-loopback`保留旧设备录音路径；业务或原版单端场景不创建SDL采集器。通用自动启动默认抑制owned新进程发声，也不依赖旧PID静音继承。UTF-8 Windows路径、停止并发、实际Audio关闭及失败清理均有定向回归。

C4全新Debug构建408份冻结源码，通过11项受影响CTest；当前共注册77项，并非本轮重跑77项。Python当前791个唯一方法，由C1／C3的773项联合覆盖及C4新增18项共同支持。C4实际242个唯一方法通过，其余549项保留原候选资格；没有删除旧方法，也没有把重复模块运行相加成fresh791。另完成当前704个源码地址检查及已发布SUMS的11文件核验，不扩大为新BSS运行验收。

C4后台标题及战场菜单实际四端成功，原生WAV、失焦哨兵、NOACTIVATE／逐键前台检查和正常退出均通过。私有汇总worker误把标题的二进制指纹`inputs`当成事件列表；标题实际使用`input_events`，战场菜单才用`inputs`。外层因此以失败退出并正常清理，旧worker／汇总与子报告均保留。后续仅修私有校验，并离线读取、绑定这次成功子证据，不重跑四个窗口；此错误不涉及游戏源码修改。

续入口随后通过三项SDL真实输入及END双端；B6三核心中物理与Magic0的业务、完整帧、有限VOICE门均通过。恢复术13的业务、全部晚守卫、正常退出和两端声音门也通过，但完整帧没有可配对项，入口按首错停止，未执行后续AI、B7和自然流程。这证明新安静采集已补齐本轮三核心最低声音门，不能把帧失败改写为三核心全部通过。

恢复术13的75张原版、156张SDL系列中，两端各有五张正识别地图效果。原版角色为idle1，SDL为idle2；唯一FX59的一对有1678个坏像素，1374来自演员、304来自底栏小头像，全部由原FDICON相位变化解释，无未解释差异。各端底栏2346像素分别与自己的原资源模型完全相符；相关地图和效果无水色DAC224..239像素。57与58有透明包含关系，锚点命中不证明唯一FX编号。诊断收据为`fbcec644e9b821244b4e80ccf09490a3ca07db525e58b5367b1b742c2237e18d`，仅分类，不掩码、不更改原图或完整帧门。

进一步原码复核发现，1297d在BIOS差值超过4时只推进一次idle并重取锚，现有阻塞演出却从旧Game锚按经过时间累计投影视觉相位。该策略差异将沿现有Game唯一视觉owner修复；不以修改效果、等待时间或再赌一次输入边沿替代。新候选定向复验与实际配对仍待登记。

### 确认框瞬时异常：实际复现与修复

用户指出确认选项出现、消失时短暂变色。原稳态图形门只比较按钮及有限资源，未覆盖首次提示帧；无窗口测试还错误断言整框角为0xcd。沿既有END取消流程，原版先行、SDL后行，各录制出现及ESC取消的连续WGC，保留全部原PNG。四段分别23、18、52、41帧，底框310×86中0xcd颜色最高占比为原版2.73%／4.96%、SDL97.45%／97.92%。稳态按钮仍通过，来源及清理均正常，确证该测试盲区。

根因是Play首提示／结果将整块填为字形前景0xcd再present并停250ms；稳态另用详情面板底图，头像和文字原点也未对齐。修复统一为只读系统notice绘制，复用FDOTHER[5]真实底框及DATO立绘；16f55 END与19df7次级菜单使用原参数75，March取实际raw0的unit_id，不能从画像猜成通用Hero0。原push为696099与702179，减VGA基址0xA0000后实际为99,127与99,146；结果行保留原提示。首present、稳态和结果使用同一实现，资源全预检，不新增运行期UI真值，不改按钮位置、输入提交、Game或RNG。

首版C2在相同测试下还原旧三个消费者接线得到红测：首present有26130像素错误；恢复新接线后通过。但独立Domain复核随后发现，此版实现和期望共同把上述地址误算为95,119与95,138，因此不能把该绿测当作原版坐标正确。旧包和错误期望原样保留；修正版期望将直接从原literal地址计算，并单独验证旧坐标真红。测试包含五类提示各三种首present完整64000页、五类稳态选项、五类结果、March空raw0及坏字形／末尾UI资源／缺立绘的原子反例。初稿链接和夹具初始化错误也保留，不计为业务红测。实际连续WGC验收另行登记，未以纯测代替原版运行。

用户随后说明不确定剧情对白最后一句是否也有同类异常。自然START首段七个资源等待点后，原版29帧、SDL30帧的关闭过程未复现整块0xcd填充，最高占比相同为0.58%，并各自进入下一片段首等待、正常关闭且未写SAV。严格完整帧尚未全配对；最接近一对只在y141有13像素不同，保留为独立差异，不据此声称所有对白过渡相同，也不为未确认的闪色猜改代码。本轮只覆盖该首段，不是重新验收全部117页。

独立复核以原 VGA literal 地址生成期望，旧 C2 坐标真红1583像素，C3修正后通过。正式回归新增连续过渡取证：WGC ready 后再投触发键，ESC／NO的出现与取消各一组，两端共八组；保留全部原始帧，检查已知大面积亮底，并仅从完整按钮资源已就绪的帧选择整块面板配对。旧 C4 真实记录仍被新门拒绝，不能只靠稳态按钮通过。

C6实际复验共288张原始PNG，四个操作的310×86完整面板分别零坏像素、最大差0，全部帧未触发已知亮底门。两个目标及八个采集助手均正常退出，Job18148的worker、timeout、residual、forced及result均0，cleaned1，来源晚守卫全通过。比较报告SHA为`dfa5f8bd3e4ddc29c656fa79d5463ea9938c0726e29a91cbca157d4b24cbfae8`。该门不声称每张过渡帧或过渡时序相等。此前私有父入口与子入口对同名runtime.log冲突在启动GUI前被预检拒绝；修正为host-runtime.log后才有本次实际验收，旧失败保留。

### C5—C7：构建与定向回归资格

C5全新构建因测试直接包含scene.c，新增notice符号没有重映射而链接重复；仅在该测试补充define／undef后，C6及C7完整构建命令通过。两次外层构建Job均记录一个剩余子进程并强制清理，result125，不能写成外层全0；该事实与后续正常退出的实际GUI分开登记。

C6运行14项直接受影响CTest，12项通过，两项因旧假资源夹具缺少新增真实面板前置而失败。保持生产资源预检与原子失败门，修正系统菜单及结果流程夹具，并给这两项设置准确资源工作目录；C7仅复跑这两项，2/2通过，共5.93秒。C6其余12项与C7两项形成定向联合覆盖，未重跑全77项。C6新增工具及受影响模块的100项Python通过；当前806个唯一方法保留逐候选来源，不等同本轮fresh806。C7实际707个源码规范地址通过。资格清单SHA为`ce94e05e333e1c116cbbcb5e14c731421656ddf9bcadec4b7936b606f4c073c2`。

### C7：短帧采集不足的保留失败

C7继续B6时，物理攻击业务、完整存档、两端有限音效、来源与正常进程清理均通过，但原版31帧中未取得完整受击帧，SDL21帧中有一张；严格图像门拒绝并停止，后续Magic0、恢复术13、AI、B7及自然流程未执行。不能以SDL逻辑或声音门补齐原版图形证据。

两端已保存WGC时间间隔中位约250毫秒，主动interval过滤及重复计数均0，正常显式stop，未触及600帧或时限。C4同一采集源码则取得90／135帧及更密到达，因此不能把C7缺帧直接归因33毫秒过滤。原版第6帧严格等于C4受击页22的0—69行与页23的70—199行组合，全64000像素并集差0，但不等于任何完整合法页；该诊断不修改图像、不放宽oracle。诊断收据SHA为`7ddd74c4788a88adff4f3e99da57ef2cda674bd50171b85aab4cd8b343b75d2a`。

后续只读系统检查实际收到显示器状态off，注册／注销均0；当前空闲26分钟，现用电源计划显示器关闭阈值为交流300秒。此项只证明检查时状态，尚不能倒写为C7当时状态或根因。下一步使用同C7程序及判据，在一次测试内临时请求显示器保持唤醒，结束恢复原线程请求，进行受控鉴别；不更改电源计划、全局输入或原版资源。

该次控制入口实际在GUI前停止：请求`0x80000003`返回原线程值`0x80000000`，但五秒内显示状态仍off，未启动任何游戏或采集。随后同线程恢复`0x80000000`成功；外层Job64584的worker／result为1，timeout／residual／forced为0，cleaned1。这个结果不构成显示器on条件下的物理攻击复验，更不能支持4Hz已被修复。后续交叉读取确认进程与active console均为session1、未锁屏，两种显示通知均off；没有发现显式request override，ALLOWDISPLAY设置为允许，具体未唤醒原因仍未定位。

原计划说明过隐藏自建窗口的另一唤醒方式，但连接中断前尚未编写或执行；恢复后核实无对应脚本、产物或遗留进程，不重复尝试唤醒。B6连续帧失败保持原样，先用新增poll／回读／PNG完成时标诊断，不把显示器状态当作已证根因。

计划允许并行关闭独立证据缺口：B7稳定资源ACK、受控结算／存取和完整自然交接，不依赖失败的B6瞬态帧结果，因此另起C7冻结身份的独立业务链。保留全部原判据、原版先行、WGC与后台输入、逐场owned清理和首错停止。链内12项为10项GUI及2项离线光标复核；不重开旧80项，也不把本链通过补记成B6演出图形通过。原END、跳页及对白末尾13像素未定位项继续单列。

### C8 分段诊断与当前规范补充（2026-11-07）

C8仅重建采集driver及其纯回归，复用源码未变的C7应用、planner和audio recorder，明确为mixed来源，不是fresh完整构建。采集纯CTest1/1通过0.59秒，Job63848全0／cleaned1；当前77项CTest仅该项为本次实际执行，其余76项继承C7，806项Python本次未执行。资格清单SHA为`0fbd150413dd6301fe22dedee95ac34a7e678f21895986ec2d426947b7d0325b`。

单物理实际执行104.532秒，原版29帧、SDL23帧；业务、完整存档、两端有限VOICE、来源和正常清理通过，严格图像仍失败。与C7不同，本次两端各识别一张impact，但没有完整64000像素同态配对；attack一组零坏像素、最大差0。外层44624及内层63556均worker1，timeout／residual／forced0，cleaned1，保留FAILED，不补跑。

两端WGC间隔中位均250.1145毫秒，主动过滤及重复计数0。成功TryGet中位0.001／0.0011毫秒，回读及owner复核中位0.7021／1.1384毫秒，PNG编码／写入关闭中位6.3826／35.3047毫秒。PNG完成到下一成功poll仍约243.456／220.476毫秒，空poll2556／2033，故本轮PNG开销不能单独解释取得节奏；这些统计不证明空poll调度、上游present或显示状态因果。229份原产物的SHA／size／mtime保持，诊断收据SHA为`2c3c2ad775b6f2a716c4d7e9b3a74011a12a4a2b718d5bd904a47c6d60f0deee`。

另一个构建问题已闭合原因：本机VS18 FileTracker对LocalAppData目录排除项目依赖，R3生成项目与命令均正确，tlog却缺项目头／cpp；公开API正反路径验证与此一致。因此修正AGENTS和platforms中的旧LocalAppData增量建议，示例源码／build改用普通项目路径；历史全新构建原身份保留，不因此重写或全部撤销。A7私有R3迁到普通E盘目录后fresh4/4纯测通过，tlog实际包含项目依赖，仍不计本轮B0—B7生产交付或声音实机验收。

SDK与官方资料另有MinUpdateInterval局部属性可调查，未查得其数值默认或保证FPS。C9私有探针已准备，仅显式测试环境才设置1毫秒，保存读前／读后值及HRESULT；不支持或调用错误就停，读回量化不虚称精确应用，实际帧门仍保留原33毫秒／600上限。此时尚未GUI执行、未合入MASTER，不作为已证修复。全部B7串行链关闭后才允许root唯一执行一次，不并发、不改显示器或系统设置。


### C9：MinUpdateInterval 实测未改善，保留未通过

私有探针随后唯一实际执行：原版与SDL均支持Session5属性，默认读回160000（16毫秒），显式请求1毫秒后读回10000，相关HRESULT均0。因此不是属性不支持或设置失败；实际取帧中位仍250.092毫秒，原版29帧、SDL22帧，过滤及重复均0。PNG中位5.946／24.462毫秒，空poll2566／1977，不能以处理耗时单独解释取得节奏。两端各一张impact，没有完整同态配对；attack一组完整64000像素零差。业务、有限声音、晚守卫和正常清理通过，但严格图像仍FAILED。

外层Job58428、内层4248均worker1，timeout／residual／forced0、cleaned1；只读收据`3ee7a02375176a70d1f521245f0d14f2defd2559395be37fdda6b3e7a754224f`。没有证据支持保留这个额外开关，因此不合入MASTER，不继续盲调间隔或重跑。下一项鉴别需要主机显示状态on的同环境对照，已请用户恢复条件；仍未取得这份对照，显示off不是已证根因。禁止抢前台、全局键鼠、电源或混音器设置变更及非WGC回退。短帧验收保持未通过。

### C7 独立业务链：七场通过，自然项分端保留

C7实际完成村民全活、有伤亡、背包满、129／201／193三箱和stage2存取七种原版→SDL配对，分别280.329、277.656、276.687、194.359、202.359、203.750、199.438秒；另两项结算光标离线复核通过。它们不是失败B6动画图像门的替代。旧business-summary保持FAILED，未执行的villager-defeat和natural-stage1不计入。

本链自然首关原版端实际完成全部117页引导、第12回合胜利、结算、酒店手工保存和stage1入场25页／活动保存；SDL端完成117页引导和真实Save2至11，Hero0在(10,8) WAIT后完成acted，却在`natural-0577-settle-1`超时。两端各正常退出0；外层Job20276与自然内层18644均worker1、无timeout／residual／forced、cleaned1。原版成功端、SDL失败端、输入、真实Save和PNG全部保留，不把总FAILED重写为成功。

81份输入／源码／PNG／SAV的只读诊断收据`df62b5acdde38a53f4aa3c29572efdd8362637efb36adc5fb06bd74204e04d4c`显示：tile278的attrs80要求原FDSHAP279前景，旧裸sprite门漏掉屋檐。按129ec／12ac6原绘制顺序，地形／光标／acted角色／RLE前景的完整720像素与原失败PNG坏点0、最大差2；无前景模型408坏点，十二个未acted模型至少210坏点，无光标14坏点。此项修测试观察器，不改游戏、动作、RNG、像素容差或策略；走动、邻近角色重叠和动态水色仍明确未覆盖。

可提交纯测试独立从原资源合成，不依赖历史PNG、Turn11或绝对路径，MASTER实际4/4通过8.647秒。首次补充旧模块测试的调用遗漏tools/tests导入路径，67项通过，1项和2个模块仅报ModuleNotFoundError；保留该次日志并只补对应入口，不修改生产实现求绿。

原版同格校准封装c1在启动游戏前误用仅限START的SDL Load判据而失败，Job6700未开窗口且清理完成；c2修正入口范围后仍直接确认START0，进入stage32剧情后20秒超时，原版正常退出0、Job65288无强制清理、全部守卫通过。两次工具错误及原图均保留。c3复用已验证UP→CONTINUE2资源确认顺序，实际完整720像素坏点0／最大差2，十二个未acted候选均拒绝、最少210坏点；Job57892全0、cleaned1，所有来源守卫通过，报告SHA`3f146dcfa7be60b697fbe2d35dd4d6b55171096e10f8c0723cc4e39078549724`。这是原版单格视觉校准，不计新自然实战或业务配对。

c3之后只启动一次SDL完整START纵向补验，继续全部117页与原自然规划／结算／保存协议，不从Save11续跑冒作完整START。其最终结果另登记；既有成功原版与新SDL是不同Job／工具快照，只有各端成功后才能联合比较既有entry投影，不声称fresh pair、共享RNG或整份Save相等。


### C7 独立 SDL 完整 START 补验与联合复核

新SDL端实际完成全部117页引导、第12回合胜利、酒店手工保存、stage1入场25页及活动保存，804个检查点、784条输入、12份动作计划及11份逐轮Save（turn2至12，turn1另有首次UI量测）。所有来源与文件晚守卫通过，目标22856正常退出0；外层Job5948的worker／result0，timeout／residual／forced0，cleaned1。报告SHA为`7e60afd9e117a93eb7bf060b37e59c2e712009e35a10cb88d67dcc3ac29c695f`。原版完整端与旧SDL失败及旧总FAILED均不覆盖。

独立只读联合QA重新核验两端各117＋25张实际剧情PNG、各自22,987字节Save校验、完整2,560字节roster、1,509个已知actor初始化字节和未知尾保留合同；SDL1,753／原版1,857个已登记产物、动作计划、逐轮Save及全部后台投递收据均通过SHA复核。新旧entry几何与NPC字段投影相同；这次四名非Hero玩家的阵亡状态恰相同，但不由此推断共享随机流、整份Save或全帧相等。旧自然Job18644为1是后续旧SDL失败，原版目标57644独立正常退出，不能将外层错误倒写到原版端或将外层改为0。QA收据`887eb7d378db44ebf8e8bc06087e1828a843970c6b99e15163d6c17f633d9b1b`。

新SDL的记录为Hero(7,8)，没有记录旧超时的(10,8)；不能称重新实机经过该屋檐。旧实际SDL截图的正反例、原版同格720像素校准和新完整START成功分别登记。自包含四图HTML直接嵌入检查点／capture_hashes绑定的实际PNG，SHA为`f2c9bdd444c04290e5d5972886546bef40456d2eb48c99759a22aea80b9e8013`，不改色或合成通过图。

### 对白末尾13像素：地图透明跳过的底色差异

两组实际同态帧全64,000像素各只有13点不同，均为y141，逐点等于stage32可见地图全部13个RLE SKIP：tile18的1点和tile46的12点。原版保留F5／F4／62／61／60／BE底色，SDL预清后的对应点为黑；原版frame6至28／SDL9至29持续出现，非水DAC E0–EF、文字、头像、框或等待箭头，也不是一次短收框相位。末页合同为fragment-end／arrow=null。图中camera(3,15)是原资源配准推断，未声称读取了运行寄存器。

原`11cac／11eee→4e22a`的SKIP保留目标像素，当前`tile.c`先清0再透明绘制，足以解释差异。原版旧scratch底色的初始来源尚无直接运行证据；同位置标题遗留假说已有反证：原实际标题和资源RGB均0/13匹配，换palette后索引仍12/13不同。因此不填固定捕获色、不猜前一buffer，不将此差异称为用户所述闪烁原因。只读收据`7ce3e1393201d10d0252e1e9ed5b24de26e052196657d8d2f323b2ffd0c98d7c`及标题反证`674d4b48cf2d151fb087d7590fbfaf0cfd60d34745a37a41e4970e9bbf1adf62`；126项原来源及新增7项晚守卫通过，没有改图、数据库或生产代码。确认框CD占位闪烁修复与这项底图差异分开登记。

### 显示 on 条件已由用户恢复

用户随后明确恢复显示。只读console／session通知均为on；本轮测试期间另以有界临时显示需求保持条件，未更改电源计划、系统音量或发送全局输入，结束后必须同线程撤销。当前B7长链与后续短帧对照仍串行，只有实际严格图形门通过后才登记；on本身不等于已证明先前4Hz根因。最终撤销、on条件取帧与B7剩余两项结果待完整记录后补齐。


### B7 剩余两场实际闭合及复审

剩余villager-defeat／natural-stage1两场已按原版→SDL完成，外层Job41700的worker／result0、timeout／residual／forced0、cleaned1；四个目标进程均正常0。原版第二关自然实战至turn20，SDL至turn21，真实逐轮Save分别19／20份、规划20／21份；两端各9页script13、3条村民死亡等价资源、30页结算和15页stage2入场。败北检查点原版6业务＋1诊断、SDL3业务；自然项原版759业务＋5诊断、SDL796业务＋1诊断。所有required Save／PNG／计划及源、工具、二进制SHA／mtime复核通过。

自然项两端均走fragment7村民损失分支，实际item198数为0，不计全村民存活奖励验收；198给奖和满包无奖仍各引此前受控配对。两端队伍增至6，新增unit8按原表独立复核70个已知字节；raw0至4的完整80字节跨端相同，新raw5只有构造明确未写的offset2及40至48共10字节不同（原版旧值／运行缓存与SDL0），不声称整480字节相同。每端完成酒店slot0保存和stage2活动保存，本自然项没有Load。

此前七场加本次两场构成9种场景的联合来源，不是最终单一快照fresh9。stage2-storage另覆盖每端四槽Save0至3、仅slot3一次手工Load，以及两份活动Save／一次活动Load；活动恢复focus(6,21)至(6,22)和本端后继完整容器同SHA。overview文字03与活动stage2分开，不扩为stage2整关、turn3或四槽均Load。

新只读QA为`ae5fc3939ad2366caaed7b89c28ec0602f078b20152a0e7624a5c417fea4786a`，七条prior业务来源也逐份复核当前SHA／size／mtime、14端和七个外层Job正常清理。顶history-summary的scope残留“no GUI executed”是旧准备文案，与真实steps和四个进程收据冲突；原报告保留，本节按实际执行登记，不用该旧措辞抹去GUI或改写旧失败。

### C10 显示 on 条件：三核心已重新通过

用户恢复on后以相同C8 metrics driver／tools及C7应用等三二进制重新取证；不启用C9间隔开关，帧上限600、33毫秒过滤、资源判据和64000像素门均保留。物理、法术0、恢复术13分别113.531、121.422、112.687秒通过，包含业务、完整存档、最少完整阶段配对、有限VOICE、晚守卫及正常清理。物理原版89帧／SDL141帧，WGC间隔中位50.0232／33.3489毫秒，攻击和受击均有正配对；新on条件下采样明显变密，但仍有较长间隔，不等于全部present已捕获或仅一因素因果已完全证明。前后只读均on，根临时显示需求另记，不假称worker整场连续监测。

19场续入口仍逐项执行后续15种AI casting及ITEM20，整体状态待全部结果后登记；三核心通过不改写旧C7／C8／C9失败，也不替代完整音画、共享RNG、水色或A7保真。


### 最终闭合：当前 Windows 受支持范围

C10最终19/19通过，外层Job40524的worker／result0，timeout／residual／forced0、cleaned1，报告SHA`cc31faa0f0ba1d382eb0b69c9fc04d998267ddf678456334b8526793914b602c`。独立三核心QA重算6组完整64000像素均坏点0／最大差0，729张原PNG、6份WAV及本端完整Save和正确owner的live mtime通过；有限VOICE十二项门全真，最低相关0.933222／margin0.419940。附加十五项AI各两组casting，共30组完整64000像素亦全部零差，没有仅靠容差通过的pair；6700张原PNG和32份本端完整Save重核。ITEM20原版HP50→5、SDL50→1，MP250及16字节库存保持，均符合原有限结果和动作正证。六次核心及32次附加前后离散通知均on；不声称持续显示状态监测或全部原始present已采到。

核心QA为`e3bb9eb8a858ab4a474cc35417f51565cc02935bd04b53669b358ec0585b96ab`，附加图形／业务QA为`fed853090582e691cadb02c175332c67bd3c3ae42c454a71f8fce321aae56ea2`。32份附加WAV及appmix／原版HOST_W owner、静音与收尾另经独立采集层QA；它未逐项做SFX身份匹配，不扩为完整目标FX、全部音效、A7／A8或扬声器输出。两端normal0、对应helper／子Job及外层均无强制残留。原C7／C8／C9失败和各自raw保持原状。

最后补充第二张地图的可移植前景回归：由START声明stage1单角色，三格85／87／89对应前景86／88／90，独立原资源RLE构造完整720像素正例、36未acted反例及单像素损坏，新增方法在MASTER1/1通过0.906秒，原四项及setup不变。图形门修复不假称新自然首关实际重经(10,8)。

永久统一Suite／独立B6入口现共用Windows临时显示需求。纯fakeAPI与两入口错误报告先19项＋旧2CLI通过；独立复审再发现register在try外的中断缺口，旧实现新反例真红，新版只补该清理范围，新增与直接受影响通知用例2/2通过0.003秒，保留旧19／2CLI日志为联合资格。MASTER合入只复验新增登记中断1项0.002秒及两项CLI0.002／0.117秒。真实headless校准由root完成：注册／注销、一次80000003申请与同线程55232恢复previous80000000均成功，Job26620全0／cleaned1，无新窗口或音频设备；不把这份API校准当FPS或完整设备恢复验证。根原临时需求在所有当前图形完成后由线程65148恢复80000000，Job38876全0／cleaned1，撤销收据`bdf471fa026bed42c5ac1b207c4c4362ba7c22c83ef90af1469da7125950e6f7`。初始ready中的failed为初值而非最终API结果，最终stopped／restored_same_thread才是此项结论。

现有77项CTest保持逐候选联合资格；本次最后Python增量为831个唯一方法／55模块，其中105个当前方法有定向实际绿测，其余726继承既有资格，不是fresh77／831。所有编译C／C++及头文件与已验C8相同，新增的是工具／纯回归和说明，不为新源码副本重复跑旧历史场。资格收据`a18ac7734db7818f83b6070d89d97786d46b49a11217afd1f108bcfbd46b3483`。

另校正标题`boot_intro_title_menu @0x1f894`旧注释误引six-entry selector：它自有BIOS输入循环，ESC不命中返回分支。只改命名输入evidence和该函数comment，0 rename，284登记／1312函数、29持久类型＋9个approved原型及3702f中性无型保护保留。14项发布检查、707个源码地址及11个SUMS条目通过，三项旧反编译失败未增加，机器码／边界／SP／chunks无变；用生成器刷新最后的SHA256SUMS为`f8ccdea4a127d43ce315d7588f9297a6f53145fe59c058799951cf34332ed081`。未来A7的292名私有提案没有混入本大阶段。

B0—B7当前支持范围至stage1自然实战、结算和stage2初始入场／存取的统一审查、修复与精简现已完成。END默认NO／跳页旧症状没有因补跑成功而关闭；13个地图RLE透明底色、全部动画／声音／共享RNG／水色、其余玩家法术、stage2战斗及后续handler继续单列，不猜旧buffer来源、不填捕获常量。Linux本轮未执行；只按当前Windows适用范围验收。

### 后续 A7／A8 预备与原版采集（不计本阶段声音保真）

A7／A8代码在独立普通目录完成私有整合编译及定向纯测试，尚未合入本次生产代码。初次标题18采集错误地要求静态WGC series在8秒后仍有fresh帧，只取得一张完整标题，按原门FAILED并正常清理；其closed DRO／WAV／FILES后来只读解析通过，旧FAILED不覆盖。静态页面流程因此改为同PID唯一可见HWND的首末两次WGC资源正证，中间8秒无测试键／目标存活／后台，明确没有连续FPS或逐动画结论，不放宽动态B6图形门。

修正后原版单端采集通过，Job35804全0／cleaned1，目标和原资源／工具／静态49及Save守卫全真。首末完整64000像素坏点0／最大差3，中间8.015秒无新增测试键；DRO8852字节／4352pairs／3748writes、WAV916884个44100Hz双声道PCM16样本帧，文件头尾闭合，非零样本1629446／peak24995。报告SHA`38a1ae061145235480be4dd8e08f3086e47f7cbb8876b989f952a9d3f8b191bf`。单次Capture接口没有capture-HWND返回元数据，owner证据限定为driver唯一可见PID及既有input HWND前后仍owned，不伪造字段。此项仅关闭采集流程，未证明loaded-driver指针、纯曲无SFX、SDL波形、精确时序、A7或A8完成；后续按音频工作包继续。

## A8：宿主与设备生命周期

本阶段从 `8fdfb2f` 独立提取本地降级与资源生命周期改动，按本阶段已明确的本地降级与资源生命周期范围交付；A7 的驱动调度、默认 PCM 增益、音色／响度与命名提案没有混入。以下音频 C3／C4／C5 指本地 `audio-head-8fdfb2-20261107-c*` 候选，不改写前文同编号的大阶段图形候选。

### 实现与交付候选

两个 bus 保存可合并的最新原子目标，PLAY／STOP／master 仍用原 SPSC 队列；读取目标不表示设备已应用。Open／Resume／Put 首错在对象生命周期内保留，只读查询不清错；callback 的 Put 失败还通过既有首错门使当前 AppMix summary 失败，采集结果保留提交失败，不把部分成功录音算完整通过。允许 null 时的首次打开／启动失败保持本轮 discard-null，不自动重开或恢复播放位置。

`--field-play-once` 的旧内嵌玩法／绘制自测迁到既有无窗口测试，保留原断言和错误／quit 路径，使正式快速入口只验证本身。新增无效音频后端的实际主程序快速入口检查，保留正式 Game 和图形资源门。独立 SDL 内部 ABI probe 位于 `tools/testing/sdl-local-disconnect`，不成为 App 的私有 SDL 头依赖，不重新构建 SDL。

最终交付树为 `E:/Projects/fd2sdl-work/a8-delivery-20261107`，基于 `8fdfb2f` 的独立 A8 提取；主程序 SHA256 `3521a0504385e5dabee8d8a36aafc9c00b2bda4374f19e5075c0d66e152f29e0`。全新 Windows Debug 构建后，10项定向 CTest 在4.96秒内通过；独立 SDK 的两个目标也全新编译，九组纯边界的1项 CTest及设备 probe dry 通过。707个源码地址均为 canonical，`git diff --check` 通过。结果位于本地 `a8-delivery-verification-20261107-c1/result.json`，不是历史全部测试的新一轮验收，也未重新执行 GUI／设备采集。

独立审查修复了私有 ABI 再生产入口只固定库、未固定头文件的缺口：现在同时校验真实 `SDL_sysaudio.h`、`SDL_internal.h` 与生成配置头。C2实际验证坏头拒绝、匹配 SDK 接受；清除迁移测试的局部变量遮蔽警告后，只重编／补验 `field_unit_render`。另删除迁移测试中恒为真的可选参数及不可达分支，C3只重编／补验 `field_progress_play`。两项均通过，分别保留 C2／C3收据，不写成 fresh13 或重复全历史回归。独立 owner／错误／销毁／覆盖迁移复审无剩余阻断，正常 Play 路径没有改动。

配置和编译 worker 均返回0，但 MSVC 留下所属 `vctip.exe`，外层 Job 记录 residual=1、forced=1、cleaned=1、result=125；对应 PID与完整映像路径已留在每份构建日志，不把编译成功改写成正常清理通过。CTest、dry、地址检查 Job 均正常0且无残留／强制清理。本轮原版独立副本50文件不变，主工作区未提交的两个 shell脚本和公开主程序未改。

### 已有证据及复用边界

本地收据均保留于 `C:/Users/Soar/source/repos/fd2sdl-work/` 或其明确记载的历史目录；不提交原资源、WAV、程序或报告。

| 来源 | 可复用事实 | 限制 |
|---|---|---|
| `a7-three-scenes-gui-prep-20261107-c3/actual-four-episodes-c3-c3`；独立 QA `a7-four-episodes-actual-readonly-qa-20261107-c1/receipt.json`，SHA `1c1a50000b8d5518269db022a636e19965b13d0c05fa23abd305f4c73348ea59` | 音频 C3 的 options／Load 两个 SDL 目标，opening／END 四个双端目标，共六个正常关闭目标（四 SDL／两原版），含正式切曲、活动 Load、后台与关闭的共同 owner／pre-OS 采集 | 不是六个 SDL 目标或本交付树的 fresh GUI，不回填 C4 后续逐 Put 成功，也不证明 A7 音频保真 |
| `a7-appmix-put-failure-connected-review-20261107-c1/receipt.json`，SHA `4ef504763b469a15799296d2812242a5cd7b8b9fc2a7a4519873aa42f601f150` | C4 中途 Put 失败使 summary 失败的真实红绿；lifecycle／boundaries 两项绿加修正重编后的 hook 一项绿 | 首次旧 mtime 导致 hook 未重编的失败原样保留；三项是联合资格，不称 fresh3 |
| `audio-head-8fdfb2-20261107-c5/receipt.json`，SHA `1ab2e053f3f1a431aa8a0804cbc1659d7d3679db323efc46a54eba0d14cfd55d` | once 自测迁出、无效音频后端快速入口及两个受影响测试的三项通过 | 原 cursor／info 自测失败保留；不改正常 Play 的 A7 接线，不计新 GUI 或实际设备打开 |
| `major-b0-b7-20261106/a8-device-actual-c2-receipt.json`（历史 LOCALAPPDATA 工作目录），SHA `93c61c69a67c50d005c9adb8cc67c0a9d6406992c2c88d06ca0a7210224d0acc` | 真实 WASAPI source 替换、两 bus 恢复、153 次成功 Put、三 source 各 retire 一次，Job41112 正常清理 | 是 API probe，不是正式 Game Load 或物理拔插 |
| `a8-sdl-local-disconnect-prep-20261107-c4/actual-loss-c3-probe-c4-pure/receipt.json`，SHA `23104f884b54d1a8761153fd4ac4a4baceaeed46da23c183ca3459bf10aae589` | 继承 C3 已链接 probe 与 C4 九组纯回归；实际 WASAPI 48k／双声道 F32，151 callback＝151 Put＝151 success，非零设备提交0；三个 source 各 retire 一次 | 不称 fresh 双程序，不执行全局设备／混音器操作 |

最后一项的同 logical／physical owner 前后保持，ref 取得／释放各一次，zombie0→1，Wait／GetBuffer／Play 三函数均切换；断连后 callback 与 Put 从1增至2，并继续到151。原 source 替换、两 bus 配置恢复、stop／retire／close 都发生在该进程的后续真实流程。Job28288 的 worker／result0，timeout／residual／forced0、cleaned1。独立只读 QA 收据 `a8-local-disconnect-actual-readonly-qa-20261107-c1/receipt.json` SHA `6eea5f5a1dda444ccace8e051e8c0aff43128d13878f7d76fd8713a0b6e0dae4` 复核 raw JSON／Job、573个可复核来源事实及已命名程序／收据；不将父端578输入计数伪称为另一次完整重建。

### 未覆盖范围

完成范围是当前 Windows 的本地降级与资源生命周期。稳定后台已有实际证据，真实前台→失焦切换未另取证；不新增自动暂停策略。原 SDL 的进程内 removed→Zombie 不等于实际物理拔插、WASAPI `AUDCLNT_E_DEVICE_INVALIDATED` 恢复成功或默认路由迁移。设备 probe 只提交等长零，未验证扬声器输出；任意未通知 SDL 的 callback 停滞不在本项保证内。Linux／macOS 和私有 SDK probe 的其它 ABI 未执行。A7 的响度、完整波形和绝对时序仍未关闭，不能由上述采集或生命周期结果替代。

## A7：当前受支持音频行为与后端边界

本阶段以 A8 提交 `ec8e699` 为基线，合入原 EVNT／SBPRO2分支、场景与选项／Load、PCM名义默认值及对应正规命名证据；保留 A8已提交的本地降级和资源 owner。不把内部向量、某次曲目采集或一个辅助函数拆成独立交付。

### 2026-11-07 用户决策与完成口径

用户选择「保留当前后端原始响度」：不为贴合 DOSBox增加 BGM补偿。另明确 MIDI/OPL解码音色与驱动／库有关，音色相似度不再作为本阶段门，也不新增这类测量。既有标题素材约2.1倍响度差异按后端边界保留；该素材混有 startup声音且缺同相纯曲锚，不据此推导所有曲目的倍率。原严格时序／完整波形记录不改值、不改判据，但不以新增音色测量无限阻塞当前行为范围交付。

当前完成范围是事件解码、voice／controller、曲目循环／fade，以及已可达场景、选项和Load行为。它不等于所有游戏声音、完整波形或原硬件绝对时序一致。后续进入 R1 发布准备及其必需的阶段审查和真实平台前置；新玩法／场景另补自身证据。

### 实现与原版依据

原 EVNT／32 duration槽取代运行时 BW／XMIDI converter事件路径，保留第三方源码和许可证。按原首空槽、到期升序、同 tick复用、duration0、满表、EOT／loop及fade先后推进；20逻辑槽／默认18硬件channel按 SBPRO2.MDI双算子分支和 `SAMPLE.AD` 分配。channel9不另造硬件rhythm owner，无原caller的CC120 hard-off删除并保原ignored语义。

Music开关按非零→0／零→1及原1秒sequence-volume淡变；SFX只门控后续cue／stop。UI可规范化显示，但一次只写被选中的raw配置字节。正曲同曲请求不重启，重复-1仍发原4秒fade；活动Load／CONTINUE成功后的stop→primary，与手工槽Load后的场景请求分别处理。App跨标题／准备／战场交接只保已提交配置，null路径也不丢标志；坏档保持SDL安全无副作用扩展。

PCM默认值是另一个有原码依据的修复。`0x40240..0x404b2`完整原函数的256值向量确认volume100／pan64为每声道 `(u-128)*202`，volume127／0对照分别为 `*256`／0；应用cue名义gain改为 `101/128`，通用PCM显式gain不变。此结论不假设DIG master127，不把最终DAC／OS混音幅度称为已闭合；用户的BGM选择不禁止这项修复。六个实际cue调用点经连接复审，main的两个static点原阶段只编译／审查，不冒称它们已有新GUI。

两个离线probe补齐输出保护：固定Title18的JSONL／metadata trace与原离线WAV共用C ABI小helper，按canonical原目录／实际parent共同预检并独占创建，拒绝已有、重名、原目录及指向原目录的junction别名，修复trace可写原资源及WAV先rb后wb的覆盖竞态。Windows传入UTF-8 argv；只有离线helper用C++17，正式runtime仍为C++14。新增路径回归仅在owned资源副本中执行，不修改原版50文件；最终定向结果在下节补录，不回写旧候选。

### 分层证据与保留失败

| 来源 | 实际支持范围 | 保留限制 |
|---|---|---|
| R3／R4原读集、局部原kernel和15首实际曲目一次／两次loop向量 | MIDI消息、duration槽、voice分配和整数sequence volume／fade | I/O／delay被记录或替换，不是实际PIC时序、载入指针或完整波形 |
| 前文原版Title C2 closed DRO／WAV／FILES与后续写序消费者 | 预声明窗口3748/3748映射后寄存器写序通过 | writer含cache和同值过滤；严格PIC单一1ms相位交集为空，仍记RED，不改timer／sleep或裁区求绿 |
| `a7-three-scenes-gui-prep-20261107-c3/actual-four-episodes-c3-c3`；独立QA `1c1a50000b8d5518269db022a636e19965b13d0c05fa23abd305f4c73348ea59` | options／Load共两个SDL；opening与END各一原版＋一SDL，共四目标；合计六目标（四SDL／两原版），覆盖正式资源、配置、完整SAV、切曲和正常关闭 | 不是六SDL或本新树fresh GUI；谱排名、carrier TL变化与采集闭合各保原范围，不冒完整wave或精确1秒轨迹 |
| `a7-opening-end-offline-consumer-20261107-c1/receipt.json`，SHA `3c1a4945bd1177295d82bdc3f0cd7709bc6a960bf2569478e3fa839690b97e30` | 固定窗／六参考支持opening末11及END19→12→19的有限观察 | 原opening首窗18弱，不据短窗唯一认曲；不移窗或拟合增益 |
| `a7-options-load-sound-readonly-20261107-c1/receipt.json`，SHA `ecf72430c13a5ad39c25bc8624492dfd31f7fbc8147375c8808d187cd6225d8f` | 配置、取消／肯定Load和有限menu VOICE观察，原TL衰减／恢复状态 | TL63不等于PCM绝对零；mute下不能声学排除restart，原SFX重开混曲窗margin不足的失败保留 |
| `a7-sfx-reenable-gui-prep-20261107-c3/actual-sfx-reenable-c4-c3`；独立QA `380edbb5bfd2d57b791e6caca1d72fedeff89d96f84e0c9d8041c01378353695` | Music全程OFF的SFX00→01资源／Save，新SDL full2652点sample8身份分数0.97607；原13候选／0.8分数／0.05margin门不变 | 原声音显式复用旧失败C1的闭合录音，新原版只补业务／Save；是联合证据，不是fresh音频pair，不证明唯一ESC实例／绝对缺声或精确触发时刻 |

所有路径均指本地 `C:/Users/Soar/source/repos/fd2sdl-work/` 的封存目录，原版、工具、WAV／DRO／PNG／SAV和late guard按各自收据绑定。旧C3长AppMix达到120秒容量后失败、同名PNG覆盖、SFX C1 Save高亮超时、C2初始parent亮相失败及后续修正分别保留，不把已完成业务或可复用闭合声音反写为旧全case通过。SFX C3保存的00→01两端完整容器同源，原版／SDL目标与采集helper均正常关闭；静音提交是pre-OS取证，不改用户mixer，也不证明扬声器输出。

### 最终交付验证

交付树 `E:/Projects/fd2sdl-work/a7-delivery-20261107` 基于 `ec8e699`，主程序 SHA256 `2dbab3cd7bf7cf054cbe85ec58a995b606739fa3c05cc677331e560545bd9bbc`。新 Windows Debug 构建的15项定向 CTest通过22.05秒；30项 DRO解析／比较 Python 和14项命名发布检查通过，当前720个源码地址 token均 canonical。293登记／1312函数的14文件发布取自已封生成器结果，11项SUMS匹配；发布时的源码地址快照707与本交付树720分开登记。

首轮根汇总脚本错误假设新版CTest仍含“0 tests failed”字样，并误取内嵌Job收据；原脚本失败和C1结果不改。`a7-delivery-verification-20261107-c1-close/result.json`只读复核已闭合raw，确认真实外层Job62448全0、无残留／强制清理。两trace目标的固定短程PCM hash均为 `00d16e4f558bea67`，支持观察hook没有改变该短程输出，不是音色测量或原版波形门。

阶段审查补齐两个离线probe的输出边界：canonical实际目录共同预检、原资源／重合／已有路径拒绝，独占创建取代WAV的rb→wb预查。旧程序在新反例上真实失败；C2重编仅三个probe目标，两trace自测通过，CLI停在测试自身的junction命令引号问题。修正后C3仅补跑该1项CTest，9个真实离线CLI用例全部通过0.66秒，含中文文件名、二输出共同预检及原目录junction拒绝。C2失败保留，不重复15项或任何历史GUI。资格合计16个唯一CTest，按上述候选联合覆盖；不是最后程序fresh18项。

所有定向测试／静态检查Job正常0并清理；配置／编译worker0，但所属MSVC `vctip.exe`残留由本Job强制回收、outer125，日志保留PID和映像路径，不改记正常清理。移除了未注册的旧baseline测试分支，保留当前默认路径的全部断言；原版50文件副本与A8保护路径保持。当前复审没有剩余阻断。Linux本轮未执行。

实际采集worker源码归档于 `tools/testing/a7-evidence/`：两套共11份原字节Python源，附来源、摘要及文本转换约束。它们是依赖原sealed C3／C4输入的历史重放源码，不能直接换当前二进制，也未注册为永久Suite；R1仍需补可配置的常规A7图形／音频回归入口。归档只做AST与原字节／Git blob核对，不新增GUI、录音或设备验收次数。

当前支持范围据上述事件／业务证据与用户明确后端边界收口。完整波形、严格PIC／绝对时序、后续曲目／场景和其它平台仍未验；不因该事实抹去已有行为验收，也不将其写成全保真通过。A8 `ec8e699` 的host／设备边界继续有效，不被本阶段覆盖或重新命名。
