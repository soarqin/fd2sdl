# 分阶段提交与验证记录

本页保留候选、失败和当时状态，不作为当前待办。当前执行入口见[统一计划](development.md)，已完成阶段的最终提交／证据入口见[完成归档](completed-stages.md)。读到旧“进行中／未提交”时须按对应最终收口节理解，不将历史状态覆盖当前入口。

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

实际采集worker源码归档于 `tools/testing/a7-evidence/`：两套共11份原字节Python源，附来源、摘要及文本转换约束。它们是依赖原sealed C3／C4输入的历史重放源码，不能直接换当前二进制，也未注册为永久Suite；常规入口由后续R1单独实现和验收，不能回填为本段新增次数。归档只做AST与原字节／Git blob核对，不新增GUI、录音或设备验收次数。

## R1：Windows 本地交付、许可与常规音频回归

2026-11-07，以 A7 `15d262a` 为基线，在独立工作树完成整个 R1；阶段内的实现、审查修复及定向验收一并提交。自有代码按用户明确选择采用 MIT，根 `LICENSE` 与 `THIRD_PARTY_NOTICES.md` 已接入。SDL、Nuked、AIL model 和历史第三方代码继续保留原许可；原版游戏、素材和反编译证据的原作品权利不由 MIT 授予，原资源不分发。

本段本地证据根为 `C:/Users/Soar/source/repos/fd2sdl-work/`，工作树为 `E:/Projects/fd2sdl-work/r1-delivery-20261107`。Windows 是本轮验收平台，未执行 Linux／macOS，不声明跨平台发布通过。没有新逆向语义、IDA建库或机器码执行验收；本阶段使用既有原版业务／资源依据和新运行证据。

### 实现与审查

- 启动前按入口检查必需的 archive／B24；缺失、损坏和读取失败列出具体路径，在 SDL 窗口及音频初始化前退出，不下载或修改资源。FDMUS／SAMPLE 仍为可降级音频资源，BG／TAI仍由实际战斗路径加载；正式资源加载保留原 owner。
- `record_windows_release.py` 连接构建前源码快照、实际 CMake typed SDL target、匹配 installed SDK 与 PE 依赖扫描；`package_windows_release.py` 只收完整 App 构建源码、实际 SDK、所需许可、主程序和明确支持的 SDL DLL。静态候选没有 SDL DLL，不从 System32 复制系统／VC DLL。
- 复审修正实际 `original_game` junction 目标保护、Windows/Temp误判系统目录和 direct import 无依据放行；虚拟 API 仅支持八项明确 UCRT 合约并绑定实际 `ucrtbase.dll`，未知依赖拒绝。新扫描同时绑定 EXE 路径及 SHA，不复用旧 EXE 的扫描结果。
- `run_audio_checks.py` 提供可配置的五场常规入口，默认 dry，显式 `--execute` 才启动本轮 owned 进程；每端新录音，后台输入、WGC 单窗口及有界清理。保留系统静音，SDL录设备提交前混音，DOSBox用本轮内部 `mixer nosound=true`。高亮 ACK 顺序读完整 JSONL，首次明确 NO 直接失败。
- 复审补齐异常时先有界封闭录音、再关闭目标，首错不被收尾错误覆盖。未增加事件框架、后端选择、BGM增益补偿或音色门；常规工具复用现有 owner、oracle 和采集设施。

### 构建与纯测试来源

| 候选／记录 | 实际结果 |
|---|---|
| `r1-sdl-release-20261107-c1` | 从固定 SDL commit `2813f2630904ea29806fcfaeeb3287ed555e42e2` 新建真正 Release 静态 SDK并安装，不复用 Debug 库 |
| `r1-app-release-20261107-c1` | Release 全目标构建；启动资源、标题资源、无音频快速入口3项CTest通过，原版50文件及源码守卫通过 |
| `r1-headless-review-20261107-c1` | 88项非窗口 CTest 全过，30.87秒；唯一排除的实际 `field_play_input` 未改写为本轮通过。之后依赖扫描递归进系统可选 DLL 而失败，原记录保留；C2才修正系统递归边界 |
| `r1-python-review-20261107-c1-tail.txt`及后续定向输出 | 首轮909项：906通过、2失败、1跳过。修正 JSONL夹具改写换行及已有输出断言后，18项音频＋17项记录器定向通过；记录器新增4项，17项打包器此前通过。当前 R1 新增52项纯测试按候选通过，不称新单轮913全绿；跳过项缺开发者本地LE种子中间物，不计通过 |
| `r1-inspector-review-20261107-c3` | 真实22项 direct import／8项UCRT绑定通过；独立修改PE的缺失 `vendor-api-ms-plugin.dll` 被拒，隔离 junction 的真实资源目录写入被拒，原 EXE 未变 |
| `r1-capture-fix-20261107-c2` | 只重建受影响 driver／纯捕获测试，`window_capture_series` 含新增模块PIN成功／失败 fake回归通过；无WGC／D3D／HWND调用。App及包内运行源码未变 |

构建 worker 均为0；部分 MSVC 构建的外层返回125，日志明确仅回收本轮 `vctip.exe`，不称这些构建正常全零。真实测试和下面最终运行 Job 均另核退出／清理；不按进程名清理用户窗口。原版资源与历史失败未覆盖或删除。

提交收尾只移除音频Python helper末尾多余空行，保留实测前原字节并核AST相同，不为该格式变化新增实机次数；App及包内源码不变。三份SDL／HIDAPI原许可中的空格及末尾空行按上游逐字节保留，Git空白检查只排除这三份原文。

### 实际本地包与替换库重建

`r1-package-qualification-20261107-c1/result.json` 记录实际源码快照→Release增量构建→新扫描→输入→241项载荷及清单。154项App源码／许可、81项匹配SDK、6项根文件；主程序 SHA256为 `1d97c068d54451e781d172934a76d5b96b847206ccbb8ca5c851a9768b0a2544`，SDL静态链接，无额外runtime DLL。VC++ x64运行库列为前置，不附系统目录副本。包不含原版数据、SAV、截图、录音、DRO或反编译输出，也不输出原版兼容TMP缓存。

独立 `offline-copy` 用随包 `source`、`sdk/sdl` 和 `rebuild.ps1` 完成 `BUILD_TESTING=OFF` 的全量重建。子进程 HTTP／HTTPS／ALL代理指向不可达 loopback，无CPM或外部下载步骤；未修改系统防火墙，编译器／Windows SDK使用预安装工具。输出 `e7168261…139b` 不要求与原程序逐字节相同。再仅改 Nuked `OPL3_Reset` 的volatile标记，日志实际重编该源并重新链接，输出 `63e34915…a02a`；修改版未运行，原包逐文件摘要保持不变。这支持可替换源码并重链接，不扩写为修改后音画验收。

本地 ZIP 为 `fd2sdl-windows-x64-release-r1-20261107.zip`，5,658,617字节，SHA256 `cb88a4ea62eac41d275515f28517c047e1f52807cc8276d462e261e26c974517`。`r1-final-review-20261107-c1/result.json` 核包内文件、ZIP完整性和源码／原版50文件守卫；实际资源前置的 dry 在拦截 `WinDLL` 与 `Popen` 时返回 `ready_not_executed`，不创建测试目录。包内工具保留 `offline_rebuild_verified_by_packager=false`，实际验证另存收据；没有上传、推送或对外发布。

### 五场双端与无音频原生窗口

同一 Release App 的实际五场由两个 driver 候选联合验收：

- `r1-audio-gui-20261107-c1/comparison.json`：options两端通过，11→01→11→10→11及每次完整Save守卫；每端47个连续捕获helper。随后原版active-load的第19个series停止后摘要已写，但worker以 `0xC0000005` 退出，整份C1仍为失败。
- WER `e115ce9e-4572-4064-b499-fb5deb295462` 定位 `GraphicsCapture.dll_unloaded`，版本10.0.26100.9549，execute AV offset0x105ac。捕获器在factory持引用时固定已加载System32模块到短命进程退出，保留正常session／pool／COM清理。内部异步卸载时序仍是推断，无完整dump栈，不称整个系统问题根因全部证明。
- `r1-audio-gui-20261107-c2/comparison.json`：修复后只运行active-load、opening、end-turn、sfx-reenable，共8端及146个serieshelper，全过；开场各ACK29页后停在首次fragment2，Load覆盖ESC／NO保持00与YES恢复完整基线11，END至round002及19→12→19请求。所有录音均为本轮各端新采集，不继承旧WAV。
- SFX仍为13候选、sample8完整2652点、阈值0.8／margin0.05：原版得分0.947826、次名0.805158；SDL0.976044、次名0.783745。只证明有限身份，重复次数、唯一ESC实例、绝对静音与精确时延仍未证明。

五项合计10端、240个通过的serieshelper，另有开场／稳定画面的单帧WGC；这是分候选回归，不是同一driver单轮五场。C1失败及其PNG、WAV、输入／来源记录保留；C2通过不抹除它。最终C2外层Job为 `worker_exit=0 timed_out=0 residual=0 forced=0 cleaned=1`，8端均正常关闭，显示需求同线程释放。此前C1的3个已启动目标也均正常关闭，无遗留或强制清理。

`r1-native-null-20261107-c2` 另以包的独立运行副本、12个用户资源和无效SDL音频后端，从正式标题CONTINUE读隔离存档，WGC验证浏览→菜单→ESC返回，观察到discard null后端，资源、完整Save及二进制守卫通过，正常退出并释放显示需求。缺资源的 `play.cmd` 与损坏资源的正式启动均退出1并保留只读文件。早期 `r1-native-null-20261107-c1` 误以独立 `--field-play` 会读活动存档，预期光标错误而超时；日志显示实际debug初态，改走正式CONTINUE后才通过，旧失败保留。

R1本地交付范围完成。历史B0—B7已审查，不因打包重跑全部旧GUI。未关闭原有END／跳页未定位症状、对白透明底图13像素差异、严格PIC／绝对时序、完整波形／物理输出、共享RNG／水色和后续关卡边界；本轮不复现旧症状不能当作已定位。stage2战斗、其它未开放功能及其它平台继续按各自工作包处理。

当前支持范围据上述事件／业务证据与用户明确后端边界收口。完整波形、严格PIC／绝对时序、后续曲目／场景和其它平台仍未验；不因该事实抹去已有行为验收，也不将其写成全保真通过。A8 `ec8e699` 的host／设备边界继续有效，不被本阶段覆盖或重新命名。


## SD1：静态冷表来源、严格生成与构建链（2026-11-09）

用户已将原调研任务扩展为在同一工作树连续实施 SD1—SD3。SD1 从 `c6d2c57` 开始；主分支仍为 `88b47d7`，主树未跟踪的 `FD2.SAV` 保留。仅在隔离工作树建立指向原资源的只读使用 junction，所有测试产物写本树 build 或 runner 隔离目录。

### 实现与来源

- `data/static/manifest.json` 和九份 table JSON 成为 43 张当前已确认冷表的唯一编辑来源；CMake 用 Python 3.9+ 标准库生成私有 `const` include。所有本目录目标均依赖生成步骤，包含独立列出 `.c` 的测试。现有 getter、原 ID 与支持门保留，不为单纯转接增加查询包装模块。
- 单位基础与初始库存共用 32×24 raw base，成长和敌表也读唯一 raw 记录，删除重复私有结构投影；保留初始构造与运行成长的不同公式、未知 byte、提交写序与失败不写合同。其它范围为物品／装备、法术／移动、准备／曲目、地形／暴击、脚步／cue、FX／战斗演出参数。
- 生成器校验重复键／符号、类型／数值、固定消费 ABI、来源身份引用和已解码跨表引用；先验证全部输入再写输出。错误或旧 `.inc` 残留明确拒绝。复审发现并修复了「同时缩短维度和值仍通过」与「删除定义后旧头仍被消费」两项风险，已加负例。
- 发布源码白名单包含静态 JSON 和生成器；开发／重建需要 Python，但运行不需要 Python、JSON 或原 EXE。工具说明与规范入口同步。SD2 的事件条件、分支、顺序及等待尚未据此宣布完成。

### 实际验证

Windows 原生新构建位于本树 `src/build-win`，VS18／MSVC 19.51、Windows SDK 10.0.26100、Python 3.12.10。使用已有系统 SDL 3.2.12 SDK；当前仓库 CPM fallback 为 3.4.20，本次没有下载或声称验证该版本。构建成功，当前注册 91 项 CTest；本阶段按直接影响范围执行 61 项 field／scene／FX／animation／audio_pcm／static_data 等回归，61/61 通过，不称 91 项全部执行。

独立来源验证器不导入生产生成器：单位等 9 表 8,528/8,528 byte；准备等 6 表 973/973 byte；另核 11/11 cue 映射和 11 个内存变异拒绝。二者使用指定 EXE SHA `bb35004c…8861cd` 的严格 LE 页读集，不是原机器码执行。FX／演出参数逐值对原 C blob 及非数据 token 核对，沿用既有原局部向量判据，不把其值计入原 EXE 字节数。Python 静态数据／单位验证器单测 11 项、发布包和来源 producer 各 17 项通过。最终另执行两个生成／schema CTest，通过。

`src/build-win/graphics-artifacts/20261109-001822-237073` 保存本次 Suite `--b6-only`：物理 FULL hit、玩家 magic0、heal13 三种场景均有新原版／SDL 采集，三个 aggregate comparison 的 core_verification.ok 均为 true，包括规定完整 64,000 像素资源／相位帧 pair 和各端有限 SFX 身份／录音顺序。不是整个动画、完整动作音频、所有 FX0 实例、精确时序或 A7 全保真。Suite owned Job 收据为 `worker_exit=0 timed_out=0 residual=0 forced=0 cleaned=1`；WGC 单窗口、后台 PID/HWND 输入与临时显示需求均走原规范，未改混音器。

`build/sd1-offline/source-files-final.json` 记录仅按更新后的 source_inventory 复制的 165 个 App 构建输入；独立 `source` 不含 tests、原资源、EXE 或开发生成目录，以 `BUILD_TESTING=OFF` 完成 Release 干净生成／编译。使用 R1 installed SDL SDK `E:/Projects/fd2sdl-work/r1-delivery-20261107/build/sdk/cmake`（3.2.12、Release 静态库 SHA `df524931…49c0`），日志无 CPM／网络下载步骤；未重新打包／发布，不改写 R1 历史包身份。复审后的 JSON 仅重排记录格式，assert 解析值完全一致；最终 schema／引用与离线重建再次通过，没有为格式和生成器拒绝检查重复运行 GUI。

Linux 本轮未执行。未运行新的局部原机器码、未执行全部历史 GUI；统一全量历史回归留在 SD3。下一步直接进入 SD2 的全部当前受支持事件驱动数据化，不启动新玩法。


## SD2：当前受支持事件驱动进入冷定义（2026-11-09）

从 SD1 `6b24b33` 连续实施；未开放新玩法、未改变 SAV 格式、未替换 session／库存／单位 owner。四个领域的 30 份真实流程共 517 节点，六个 runtime 入口以 bindings 共用事件节点；生成定义在 `static_data.c` 仅编译一份。数据表示和修改流程见[冷定义工具](../tools/static-data.md)。

### 实现与边界

- `scene.json` 保存六种既有回合／普通格事件、自然 stage32→31→0、初始化和 game over；删除原逐 stage/action 手写编排。Game、Scene 与资源预检消费同一节点来源，逻辑与呈现视图明确。notice 仅新增旧运动状态快照，避免对已提交单位重复移动，不增加新热状态 owner 或持久字段。
- `stage.json` 保存 stage0／1 结果与覆盖顺序、胜利／结算、stage1／2 入场及当前支持门。App 的预检／停音乐前后关系、入场亮度／呈现／淡入也进入冷节点。stage2 入场、准备和浏览存取继续可用，战斗／END 继续拒绝。
- `loot.json` 的主图与 `action.finish` 共 212 节点：条件、空位／满包分支、选择／取消、text_ready／ACK、掉落／特殊格、奖励／AI 续行及动作种类的收尾顺序均由边控制。领域叶保留库存操作、既有恢复算法和 owner／controller API；不再把旧特殊格、满包交换或 command 收尾整体包成 native handler。
- `progress.json` 保存奖励门、等级／成长／学习的写点与提示，以及毒伤、timer 到期、聚焦、重算和 phase 续行的顺序。预检、单位奖励预览与实提交流程共用图；执行游标沿用当前 loot／progress 内存，不写 SAV。
- 36 条原移动脚本成为唯一冷流，共 794 byte；原 getter ID 门继续保留。独立来源验证器以严格 LE／fixup 定位，不使用 JSON 自报地址和长度作为 expected。

### 审查与自动检查

生成器增加有限操作／参数／端口 ABI、跨图／脚本引用、别名、必需 runtime ID、两视图、入口／可达／退出、结果查询无环与有界扫描检查。真实图边变异测试证明满包选择、动作收尾顺序和入场前缀确由数据改变；新增 Scene 复用绑定证明同一组节点在另一 stage/action 入口重放，不新增生产 C 分支、不开放 stage2 战斗。

自审修复：资源预检不能前移正式 handler 的单位数／装备断言；旧 actor 移动需要呈现前态；stage0 与 stage1 胜利的预检／停音乐顺序不同；纯预检即使坏边也不能写 live owner；领域叶自查索引／槽；同步 `action.finish` 不能保存另一图的等待 PC；controller 普通 `-1` 原样返回，不额外锁存 ACTION。保留失败日志后复验，没有放宽原语义判据。

Windows Debug 构建成功。`test-sd2-c5.log` 64/64；后续新增 schema／别名／淡入测试的 C6 为 64/65，新测试误认终点 label 导致一项失败；改为按真实 end 操作查找后，C7 受影响 14/14；最后 controller 边修复的 C8 受影响 5/5。当前 65 个阶段定向检查均有通过结果，不称 C6 单轮全绿。`test-sd2-static-c6.log` 的 SD2 生成／来源测试 26/26；另有 SD3 审计测试单独登记。

Python 全库审查首轮 935 项，1 失败、1 跳过：旧装备许可测试仍解析已删除的 C 数组；改为冷 JSON 对独立 strict LE 读集后，整个装备模块 76/76。该轮其余测试通过；跳过的 `test_current_object1_anchor_and_chkstk_evidence` 缺开发者本地 LE 种子中间产物，不计通过。首轮失败和定向修复日志保留，不宣称单轮无跳过全绿。

单位等独立来源 8,528 byte、准备等 973 byte、36 脚本 794 byte 全部相同，另核 11 cue 和坏数据拒绝；这属于只读来源验证，不是新原机器码执行。`build/sd2-offline-final/source-files.json` 冻结仅发布白名单的 175 份 App 输入；全新 `BUILD_TESTING=OFF` Release 构建通过，与当前源码逐份 SHA 相同。使用 R1 已装 SDL3.2.12 Release SDK，无原资源／EXE／开发生成目录／测试源码或网络下载依赖，不重打 R1 历史包。

### 代表原版对照与阶段完成

第一次 GUI 预检 `20261109-014707-020926` 因显示 off/未知未开窗口；Job 无遗留。恢复 ON 后，`20261109-020527-122173` 的准备／入场、箱子空位、满包交换通过，选槽取消因 E: 空间不足（WinError 112）中断，owned Job 全部清理。只将本任务图形证据移至 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-sd-20261109-tranquil/graphics-artifacts` 并保留原路径 junction；2,446 文件的 SHA／大小／mtime 全部一致，收据为同 C: 任务根的 `graphics-move-receipt.json`，源码和增量构建仍在 E:。

`20261109-021840-631904` 补跑的选槽取消、事件回调和剧情双端比较均 `ok=true`。之后执行预算中断，stage1-turn3 无完成报告，未将其或未执行的村民败北记为通过；续接核实无残留测试进程后只补这两项：`20261109-022816-989517` 的 turn3 增援和村民败北均通过，原资源守卫通过、display 正常 released、Job 为 `0/0/0/0/1/0`（worker_exit／timed_out／residual／forced／cleaned／result）。

`20261109-023329-476319` 的 counter-multi／learn／status 三种成长与状态场景全部双端通过。随后使用本轮 turn3 原版端的 completed Save（SHA `2942f66c…7ddd3`）和同目录 source report，在 C: 任务根 `sd2-representatives/` 完成 victory-all-villagers 与 victory-villager-loss 两条胜利／结算／stage2 入场与保存对照，顶层 `ok=true`、端点与最终守卫通过。串行总收据 `sd2-representatives/receipt.json` 的三项退出0、显示需求释放；外 Job 正常退出且无超时、遗留或强制清理。使用 WGC HWND／后台投键，未改混音器或电源计划。

上述检查完成后 SD2 按完整阶段提交，直接继续 SD3；全部失败／中断产物保留，没有重复重放已有通过报告的用例。此处证明当前定义迁移及所列业务／资源判据，不提升为原版全画面、完整音频、绝对时序或全关卡等价。

Linux 未执行；未证明全关事件语义或原版完整音画等价。全量受支持历史回归与本地集成仍由 SD3 收口，完成后不自动续跑新玩法。

## SD3：批量来源审计与统一收口（2026-11-09）

以下按时间保留实施、失败和续接记录；最终结论与候选范围见本节末的「SD3 最终验收与提交门」。早期的「当前」只描述其记录时点，不覆盖最终结论。

SD2 已提交 `ecb154a`，本阶段不自动开放新玩法。`audit_stage_data.py` 对真实四份原文件身份、严格 LE／fixup 分发表和已发布 IDA 清单／chunk／历史 C blob 作只读核对；schema／消费 ABI 复用生产校验，不另维护事件真值。实际使用与 Windows／Linux 命令见[冷定义工具](../tools/static-data.md)。

`build/sd3-audit/root-full-v3.json` 复核 33 组／99 项资源、三类各 528 槽、1,899 模板、2,322 placement；主要分派 180 槽与独立 magic 28 槽共 208 槽均有 IDA chunk owner，目标可能重复。90 项 event callback 后的两个 DWORD 和 28 项 magic 表不再误算为事件 callback。12 条未配对模板原样保留，不用邻近／stage0 值兜底。

自审将 450 条默认 `(255,255,0)` 改列为 turn255／phase0 的潜在匹配：未证明可忽略，故进入严格阻塞。v3 潜在集合共 1,171 条记录引用（含重复），51 supported／652 unmapped／468 unopened；不是不同事件数或执行覆盖率。仅 stage0／1 的 `bound_definitions_closed` 为 true，含义限于排除未证默认填充后的已列集合；严格语义支持集合为空，`semantic_complete=false`。stage2 单列 entry_supported=true／battle_enabled=false。v2 及早期结构报告保留，不回写成严格全范围通过。

独立执行 v3 全表结构审计 exit0、0／1 的严格审计按预期 exit1；无未知项静默转为成功。原始来源／chunk 字节不是原指令执行或未知 CFG 解码。新增工具负例和现有静态检查已通过，另收紧 loot 只读定义必须含 logic 视图，防止合法结构直到执行才暴露不受支持视图；当前生成内容与 SD2 基线相同。

当前尚未完成本优化大阶段统一历史回归或本地集成，不能以工具就绪宣布 SD3 完成。验收计划冻结为当前全部 CTest／Python、默认完整 WGC Suite、追加 B7 八场、AI 十八场、五项常规音频及正式无设备 CONTINUE；已有 A7／B6 的 collection-only 与原版完整声音／动画等价边界继续分开登记。只有实际运行报告通过后才补本阶段完成记录并提交。

### 已执行的统一检查与续接点

- 最终 Debug 候选构建成功，App SHA `2d3392bf19df00d55be59b25a0017e352ebb728eeae2d2d7da2cd33e8e1c3b6d`；新增实际 C 回归确认默认 turn255 槽匹配后为 EVENT_UNSUPPORTED，保留此前已提交写点、阻止后续 handler／毒伤／状态／RNG，不因填充外观静默成功。
- 统一 CTest 排除唯一由完整图形 Suite 执行的 `field_play_input`，其余 91/91 通过（76.14 s）。记录在 C: 任务根 `ctest/{ctest.log,receipt.json}`，外 Job 无遗留／强制清理，显示需求 released。当前共注册92项，不能在 Suite 输入项尚未完成时称92项全部通过。
- `build/sd3-python-full.log` 单轮955项，954通过／1跳过（257.125 s）；跳过仍为开发者本地 LE 种子中间产物缺失，不记通过。SD3 schema门及原数据生成／来源／ABI／审计负例均包含在本轮。
- C: 任务根 `native-null/comparison.json` 为当前 App 的正式标题 CONTINUE→浏览→菜单→ESC，无效 SDL 音频后端下通过；观察到 null 后端，discard 依据当前源码合同，不伪称观测到不存在的独立计数器。原资源、二进制、完整隔离 SAV 守卫及进程／显示清理通过。不是物理拔插或音色保真验收。
- 相对 SD2 的 App-only 源码仅生成器增加 schema 拒绝门，生成四域内容与 SD2 完全相同；离线副本更新该生成器后，175份输入与当前 source_inventory 全同，79表生成 check 通过。收据 `build/sd2-offline-final/source-files-sd3.json` 保留，不把此前干净 Release 构建改写为另一平台／另一 SDK。

完整图形批次于本地03:00启动，单次后台串行进程 PID30548，启动身份在 C: 任务根 `history-launch.json`，逐项总收据 `history/receipt.json`，标准日志 `history.{stdout,stderr}.log`。入口是本树 ignored 的 `build/run-sd3-history.py`，不是周期任务；每个子任务独立 owned Job、有界超时、失败即停。续接必须先核此 PID／启动身份及总收据，活跃时继续观察，完成项不重放。当前批次收据只覆盖其28个任务，早先声明的 native-null 缺口由上述独立新收据补足，不回写原批次声明。

03:14 续记：完整 Suite 产物为 `20261109-030047-878167`，自然开场、焦点／镜头、后台输入已通过；`input.log` 的 `field_play_input` 为1/1通过（48.09 s），owned Job 正常清理。至此92项注册 CTest 均有本轮通过结果（91项批次＋Suite单独1项），不是单轮92项调用；其余图形／业务／音频历史批次仍进行中。

冻结期间只读自审另发现一个需在提交前补齐的 schema 门：C 通过 `fd2_static_flow_label` 使用 loot 的五个固定入口（preflight.drop／preflight.cell／cell.begin／drop.begin／finish.common），当前合同只固定 flow ID；同步重命名 node 及图内引用仍可通过生成，直到运行时才拒绝。当前定义未改名，故不影响正在运行的候选。待本批次结束后仅补 `required_labels` 消费合同及逐项负例，核对生成值／C／图均未改变后定向复验；不为此打断冻结批次或重放全部 GUI。具体清单保存在 ignored 的 `build/sd3-required-labels-plan.md`。

另确认移动指针表的全表盘点仍须补齐：`0x627d8..0x62980` 连续106个 object3 fixup（424 B），原 getter 没有106项检查／分区；当前36条／794 B只是已有运行支持子集，未包含 ID20..89。全表 payload 至 object3 端 `0x634d2`。部分项存在候选跨度交叠、未消费尾或 actor≥64，不能直接套当前runtime parser判原资源损坏，更不能自动扩容／开放玩法。待当前冻结批次结束后补 audit-only 的106槽／原始payload／候选跨度／解码与支持分类，保留未知，并加相应负例；不改变已支持36条或C执行逻辑。只读原证据为 ignored `build/sd3-audit/movement-full-table-readonly.json`，实施清单为 `build/sd3-movement-coverage-plan.md`。这些待修正项完成前，SD3不得提交为完成。

### 04:20 批次中断及已落地修正

默认 Suite 在 `original-item-use-19` 的原版端 `use-commit-selected-slot2` 超时后停止，SDL尚未启动。原summary保留53个passed及1个failed，8个直接宿主窗口场景已全部正常清理；外层Job无遗留／强制清理。失败37个raw帧均为720×400，尺寸门正确拒绝，DOSBox日志亦为 final=720x400；最后有效PNG是先前槽0，不代表UP没收到。只读诊断确认非法raw图的游戏区域吻合槽2模板，但不将裁切诊断计为合法验收。没有确认Use，也未进入待测物品效果阶段；此记录不证明原版UI循环的RNG消费不变。尺寸变化的底层原因未证，不放宽尺寸／容差或补发按键。

批次退出释放显示需求后，定向use19重跑的 `20261109-041941-288364` 检测console/session非ON或未知，未申请显示需求、未创建窗口，Job正常清理；等待显示恢复后仅补失败与未执行范围，不重放原53个通过项。原后台PID30548已退出，不能当仍在运行；下一轮使用新的续接入口与新收据，保留原failure。

当前批次已停止并解除冻结后，已应用两个经过独立候选测试的工具补丁：`required_labels`固定五个外部node入口；完整移动来源补106指针／2,898 byte payload及70个未知项分类。`build/sd3-static-final.log` 50/50通过；`root-full-final.json`结构审计通过、`root-strict-final.json`严格审计按预期非零。ID24跨候选边界、ID84尾4字节、原actor最高83和direction5等原样保留，不开放新玩法。

`build/sd3-final-c-equality.json` 核对14个生成C include逐字节未变，App仍为2d3392bf…1c3b6d；两个补丁不改C执行器／冷图／运行支持集合，原通过图形证据继续有效。离线175份App输入同步当前manifest／生成器后再次全同，14个C include保持不变，收据 `source-files-sd3-final.json`。只做工具与schema定向复验，不重跑已经通过的历史GUI。SD3整体仍未完成，尚待图形／音频续接及本地集成。

04:28 续接入口已准备并通过无GUI模拟：`build/resume-sd3-history.py`，计划／操作说明为 `build/sd3-resume-plan.json`、`build/sd3-resume-instructions.md`。只跳过旧summary的53个passed，从use19开始执行默认Suite剩余31项，再接原历史计划其余27任务；8个宿主UI场景所属5个方法整体跳过，防止重做副作用。旧证据不覆盖，新summary逐case记录复用或新执行，后续Entry／Turn4按实际端报告的completed_save定位。

恢复ON后的单次命令为 `python -B build/resume-sd3-history.py --artifacts C:/Users/Soar/AppData/Local/Temp/fd2sdl-sd-20261109-tranquil/history-resume --execute`，目标尚未创建。无GUI模拟验证在 `build/sd3-resume-smoke/20261109-042724-152436/verification.json`；明确登记4项schema／audit源码变化，其它冻结GUI工具／原资源／二进制守卫不放宽。04:25只读显示快照的console/session均为OFF，未申请显示需求或启动窗口，当前无残留测试进程。等待显示恢复后按此入口继续，不再调用原全量入口重放53项；完成剩余验收、审查和SD3提交后才本地集成，主树目前仍为88b47d7且保留未跟踪FD2.SAV。

### 再次续接的非窗口复审

新一轮只读预检仍为active console、console/session均OFF，无残留测试进程；没有申请显示需求、启动窗口、循环唤醒或重复已有全量检查。核实SD1／SD2提交及SD3差异，主树仍88b47d7、FD2.SAV未跟踪，App仍2d3392bf…1c3b6d。`build/sd3-audit-guide.md` 标明为合并前草稿并指向正式指南，避免把草稿当当前验收结论。

复审修复独立来源核验的一处兼容缺口：生产schema允许递归related_sources，旧audit只核root和一层child；现改pending栈遍历全部层级并复用Git blob缓存。正确嵌套leaf／重复缓存／伪摘要leaf拒绝的定向模块22/22通过，未重跑全量、构建或GUI。当前四域来源树最大深度为2（root＋child），原报告所核blob无遗漏，历史报告保留。当前audit／test SHA分别为a1ccc8f7…e9715b、fd4a58fc…35c9a5。

ignored续接helper的B6断点也已补：旧core passed后用其实际旧comparison复核，不在新空目录误查；无GUI模拟分别验证已有1／2个core时只派发余下2／1个。当前53项尚未含B6，此修正不改变已有运行证据。另继续核对summary身份绑定和B7／AI／audio外层断点，完成后同步续接说明；所有模拟均不调用真实窗口／显示／subprocess，不改原Suite或原history。当前实际环境卡点仍是显示OFF，剩余验收未完成前不提交SD3或本地集成。

本轮非窗口收尾完成：`build/resume-sd3-history.py` 已补显式summary与所选history的完整fact绑定；已passed的Suite／B7／AI／audio按报告身份、资格及来源文件逐项复核后复用，恢复Entry／Turn4／stage2来源链，收据标reused。最终无GUI模拟 `build/sd3-resume-final-smoke/20261109-044725-734427/verification.json` 覆盖当前53项、B6部分完成、错summary拒绝、B7／AI尾段断点、28个outer全部复用零runner及音频封闭文件篡改拒绝。Entry／Turn4语义oracle和B6内容检查器在模拟中替换，不能把模拟计为真实玩法通过；正式执行仍调用原验证器。

helper SHA为90184f8f…e88f32，原history仍a8aa57d8…8cd94，完整身份在 `build/sd3-resume-helper-frozen.json`。当前仍用上面的初次续接命令；若未来再次中断，须传最近失败的 `--history-receipt` 和新的 `--artifacts`，不能重开原全量入口。说明已同步 `build/sd3-resume-instructions.md`。本轮未重跑已有92项CTest或Python全库、未启动任何窗口、未改主树；仍需本机点亮显示并进入活动桌面后才继续剩余真实验收，SD3提交与本地集成保持未完成。

### 11:20 显示恢复与原版输入诊断

用户本轮明确授权直接唤醒屏幕，故本次允许一次显示电源 ON 请求；没有修改通用入口的 OFF 拒绝门。只读核实进程仍在 active console session 1，输入桌面为 Default，屏保未运行，未发现 request override。`powercfg /requests` 因缺管理员权限未取得结果，不据此判断请求计账。ES 请求及单次显示 ON 消息曾出现父进程 ON、子入口再次 OFF；未开游戏窗口，失败收据保留为 C: 任务根 `wake-resume-{first,second}-attempt.json` 和 `history-resume/receipt.json`。

随后以具名 PowerCreateRequest 分别申请 SystemRequired／DisplayRequired，持有句柄期间两次 snapshot 均 ON，原续接入口独立 ON 门也通过。没有全局输入、改电源计划或混音器；结束后两个请求均 Clear、句柄关闭，详见 `wake-process-request-receipt.json`。此运行从原53项通过证据之后续接，产物 `20261109-112055-014276`，原版 use-19 在 raw0-zero-move-command 超时，SDL未启动；外层收据 `history-resume-process-request/receipt.json`，来源守卫保持不变、owned窗口正常清理。

本次合法640×400截图仍为移动选择态，资源范围LUT由6变化为9，非旧帧冻结；原资源oracle离线复核没有把浏览态误判为选择态，也不是COMMAND选项／容差错误。第二ENTER的后台PostMessage收据成功，但缺实际业务ACK，不能声称原版已消费或判为SDL产品回归。保留此前720×400尺寸失败和本次输入失败，不改判据、不补键；先单独复核当前环境的后台业务输入。SD3仍未完成，不提前提交或集成。

独立后台校准 `input-recalibration/artifacts/comparison.json` 通过，原版／SDL业务、WGC、原资源及二进制守卫、owned清理与前台哨兵均通过，外Job为0／0／0／0／1／0，显示需求已释放；没有修改输入工具或等待时序。11:27启动单次续接PID23476，启动身份为 `history-resume-after-input-launch.json`，总收据将写入 `history-resume-after-input/receipt.json`；本批次Suite为 `20261109-112757-938838`。use19与use20现已双端通过，确认次数仍为各一次，原失败原因不因此宣称已定位。正在补其余未执行项，原53项不重放。

11:40检查点：use19／20／21／22／23／24及area2共7项新配对通过，联合summary累计60项passed，正在执行use23-mp0。summary在每项结束时原子保存；批次结束前保留历史status，不把进行中快照当整个Suite完成。独立只读复审覆盖SD3来源／严格拒绝／消费合同、loot与action.finish取消和ACK写点、reward／status顺序、SAV／Load原owner边界，以及CMake／发布源码／开发规范一致性；没有新增必须修改项，也没有重跑全库或把静态复审计为运行验收。

11:58检查点：本次剩余物品8项和成长／状态6项均已通过，联合summary累计67项passed（复用53＋新执行14），当前进入original-prepare-controlled。用例来源仍为同一App 2d3392bf…1c3b6d及冻结工具，未改等待／按键或判据；后续17项默认Suite及27个outer任务尚未全部执行，不计SD3整体通过。活动进程／续接位置以本批次summary、启动身份与最终receipt为准。

12:29检查点：准备入场和10项拾取／特殊格／取消／掉落／回调／剧情均已通过，联合summary累计78项passed（复用53＋新执行25），当前进入original-stage1-turn3。默认Suite尚余stage1-turn3、villager-defeat、natural-stage0与三个B6核心；之后仍需原计划27个outer任务。SD1为6b24b33、SD2为ecb154a；SD3没有提前提交或本地集成。C:可用约287 GB、E:约32 GB，证据仍通过原junction写C:，未移动源码或增量构建。

12:40检查点：stage1-turn3与villager-defeat均已双端通过，累计80项passed；natural-stage0的原版已完成完整引导并进入第3回合，SDL端及后续B6／outer尚待执行。活动PID仍为23476；ignored的 `build/sd3-active-checkpoint.json` 汇集启动身份、续接收据和主树保护点，供实际进程／收据复核，不代替本轮最终结果。

### 13:15 复审补齐正式 START 初始化的唯一来源

后续覆盖复审发现 SD2 的一处遗漏：`load_stage0_units/validate_stage0_units` 仍在正式 START 的 Game 路径保留四人、末四槽、group1／2、12人及旧装备／派生属性数值锁，与 `scene.init.000` 形成第二份冷定义。合法修改冷表或初始成员可能在 handoff 前被旧 C 断言拒绝。确认后未再继续旧候选全套：只对本轮 owned 原版 PID31296 正常 close，保留原版 natural 的第8回合等中间证据、不计本场通过。`history-resume-after-input/receipt.json` 于本地12:53完成，旧冻结守卫通过，Job无超时／残留／强制清理；80项passed保持原来源。中止原因单列 `review-stop-natural.json`，不伪称原版自然流程已完成。

修复让 Game 消费 `scene.init.000` 的 logic 视图，继续使用原 persistent／active 与模板构造算法。Scene 以额外的 present-only actor_hidden 保持开场前态；args hidden=0仍表示不覆盖构造已有hidden，不能错误清除原模板位。删除旧生产成员／group／人数／装备／属性锁，只保留资源合法性与引用边界。无handoff调试入口的旧camera／cursor seed保留，正式 START仍由开场handoff覆盖，不把两种生命周期混为一份配置。当前图增加1节点，其它流程与13份非scene生成include保持不变。

Windows新App为 `eedd635302b71b6fa1cfd91bae77bd259a2d01866139eeb18ee628c9cfb6e187`，planner为 `0ad2b3b5e4ee3c29e0b2fc4d34a41c09d533700d3442669610a93ce8fffeff46`。6项受影响CTest通过，日志 `build/sd3-startup-fix-tests.log`；静态数据模块52/52通过，日志 `build/sd3-startup-static-tests.log`，未重复全库历史检查。新增回归覆盖完整旧记录、同源成员／模板变异、派生属性变动、原hidden位保留、错误引用和视图拒绝；没有增加生产测试入口或公共ABI。

实际旧core与新core分别经无窗口 public Game open导出全部units／roster／未用槽／步态／模板索引及count，7,888 byte逐字节相同，SHA为 `0f64854938e211872c559f4e59b8cd5228d361c5a9a9bba4f2c924c31c32b01f`。另在独立JSON副本把item0 AP10→11，严格生成后仅重编相应item模块，正式初始化成功、AP16→17，仅active／persistent对应两个字节变化；生产JSON未改，其余13份生成include相同。实际证据为 C:任务根 `startup-fix-equivalence.json` 及 `before-startup-fix/`，不是新增原版执行或完整GUI通过。

旧App／planner与源已归档。新 ignored 入口 `build/resume-sd3-after-startup-fix.py` 以精确修复见证和默认等价结果限定继承旧80项，明确旧binary来源；不会把这些记录改写为新App执行。新候选仍须补自然流程、B6和27个outer任务，全部完成前不提交SD3或本地集成。独立续接审查还收紧实际所选工具与真实JSON／生成收据的匹配，不放宽资源或GUI判据。

### 13:50 非窗口收尾及真实显示卡点

新定义下的33组结构审计再次通过，引用错误0；106移动槽／36已支持流／70未知或未开放、450默认turn255候选及1,171条保守记录引用的分类均未改变。0／1严格审计仍按预期返回1，不把结构成功提升为语义全覆盖。报告为 `build/sd3-audit/root-{full,strict}-startup-fixed.json`。

离线副本原缓存指向的外部R1 SDK目录已不存在，重建在重新配置时误入网络回退并失败；旧失败日志 `build/sd3-offline-startup-fixed-build.log` 保留。随后只读确认本地SDL源码为release-3.2.12，实际Git HEAD为 `5ac37a8ffcf89da390404c1016833d56e2d67ae4`（2813…是既有缓存目录名），用本任务普通E:构建目录重建并安装自有静态SDK。App关闭测试、显式使用该SDK及disconnected配置后，当前175份源码的独立Release重建通过；81份SDK文件、安装库与实际构建库SHA一致，CMake配置无原source／build／安装根绝对引用，14份生成include与当前Debug候选逐字节相同。证据 `build/sd3-offline-startup-fixed-receipt.json`；Release App SHA为 `e813c86ca8dd1a526eacfa64c1f5b0ab6637d1367d2b98945a0c88b107fcd5e1`。未生成新发布包，未改写旧R1身份，也未把这次构建计为Release原生运行通过。

软件显示唤醒于13:29再次出现after1s均ON、5s均OFF；未启动游戏。13:38只做一次不同的受控诊断：真实ON后才创建本轮owned可见／noactivate／非topmost辅助窗并处理消息，5s后仍OFF，未开游戏；辅助窗正常销毁，SystemRequired／DisplayRequired均Clear且handle关闭。收据 `wake-startup-fix-receipt.json`、`wake-startup-visible-receipt.json` 保留，无循环唤醒、全局键鼠、混音器或电源计划修改。

只读检查为active console、Default输入桌面、屏保未运行、AC供电；当前Balanced方案ALLOWDISPLAY AC/DC均1，VIDEOIDLE AC为300秒。InputDirector可读的屏保同步及其它匹配DWORD均0，未取得的受保护子键不改权限；没有证据将显示反复OFF归因于该工具。对应C:诊断JSON保留。已发一次环境动作请求：在测试机活动桌面实际移动鼠标／按Shift以重置空闲，未重复询问唤醒授权。

新helper最终SHA为 `8e172bc32edb0e4f49718cf1a9ddaf1889e59d0a9a352dc0cf2f31610039275f`；23项无GUI合成检查及真实dry-plan通过。实际修复见证为C:任务根 `startup-fix-witness.json`，SHA `1c6aa24c57e5ef7f553c02ca231990fa71084c2a30c25a69ed7114deef0b9a8e`。新的 `history-after-startup-fix/` 尚未创建、没有活动owned测试窗口／进程。恢复稳定ON后按 `build/sd3-resume-instructions.md` 的新入口补剩余natural／B6和27个outer；80项保持旧App范围来源。SD3仍未提交／未集成，主树保护和不续后续玩法的要求不变。

### 14:56 授权唤醒、临时常亮与新候选续测

用户本轮明确允许唤醒显示器并临时修改电源计划。14:33 原 Balanced 的副本已成功把 AC 显示／睡眠超时设为 0，DC 和原方案不变；一次 SC_MONITORPOWER 后仍出现 1 秒 ON／5 秒 OFF，因此没有启动游戏，随后恢复原方案并删除副本。收据 `power-enabled-20261109-1433/receipt.json` 保留。硬件 D6 与持续系统通知的只读检查未证实通知实现有取值错误，不放宽 ON 门。

14:55 在确认没有游戏／driver 窗口进程后，执行一次用户授权的零位移鼠标唤醒，未按键或点击。SendInput 插入 1 条 MOVE(0,0)，LastInputInfo 从旧空闲值更新，指针保持 `(1491,201)`；这是本次显示唤醒，不是游戏输入回退。真实 1 秒和 5 秒 console／session 均 ON，临时副本 `b6aab6d7-6d56-4f20-9523-da5eda76065e` 的 AC 超时仍为 0 后才启动 owned 验收 Job；游戏输入继续绑定 PID／HWND。原方案为 `381b4222-f694-41f0-9685-ff5bb260df2e`，父监督进程负责子 Job 全部退出后恢复／删除临时副本，最终恢复结果尚待收据。

监督 PID8068、外层 driver PID45432，于14:55:54启动当前 eedd6353 候选；正式总收据为 C:任务根 `history-after-startup-fix/receipt.json`，Suite 为 `20261109-145556-429077`，当前原版 natural-stage0 进行中。临时电源及唤醒收据为 `idle-wake-power-20261109-1454/receipt.json`；本地薄封装及5项纯模拟只属于测试宿主设施，不进入产品或放宽冻结输入。新批次继续限定继承旧80项，只补4项Suite及27个outer任务，完成前不提交SD3或本地集成。

15:07 续记：原版在实际第4回合横幅后，于 `natural-0132-settle-15` 等待静止焦点门超时；SDL端未启动，尚无新增整场通过。最后 fragment3／token364 的 ENTER down／up 各一次且投递成功，此后没有补键；合法640×400 WGC帧及全部剧情ACK保留。最后实际活动Save仍为turn3，不能把计划、本地坐标投影或失败画面当作turn4完整热状态。当前正在离线诊断资源焦点门，不通过放宽尺寸／像素、猜测HP／AI状态或直接重抽RNG处理。

本轮总收据仍为failed、冻结输入守卫true；全部owned Job无超时／残留／强制清理。监督finally已恢复原Balanced方案，删除本轮临时副本，原AC／DC值不变，详见同一电源收据。联合summary保留旧80项passed和本项failed，B6／B7／AI／音频尚未执行。后续输入不变时使用同候选续接入口及本轮最终收据；若验证器确需修正，先保留旧源／证据并审查新的来源边界。SD3提交和本地集成继续保持未完成。

### 15:49 修正已证到场重叠的自然验证门并续测

独立原 DAT／EXE 复核定位镜头 `(4,11)`：action0 的 group3／脚本7 追加 raw12／unit1 至 `(11,13)`，group7／脚本8 追加 raw13／unit3 至 `(11,12)`；本轮主角恰好停在后一格，旧 count12 焦点模板不含新到场遮挡。按原资源合成完整 `24×30` 地形、光标、同格与下邻精灵，在既有通道容差3下720像素坏点0，主角仍有38个可见像素；缺主角、缺其它层、错身份、方向、acted、层序或地形的13项负例均拒绝。证据为 C:任务根 `natural-arrival-focus-proof-20261109-152843-230287/receipt.json`。它只解释原失败帧，不是重新执行或完整新回合热状态。

修正仅涉及 `fd2_natural_oracle.py`、`run_original_natural.py` 和其现有测试文件。只有各端本轮完整ACK fragment11、3且满足原版自身round4横幅／SDL既有phase日志门时，才读取独立源构造该视觉组合；SDL当前不存在对应横幅演出，不伪造横幅通过。无关焦点保留旧路径，未知重叠、前景／水色继续拒绝。返回的仍是count12源及视觉坐标，后续真实UI Save须联证14人、raw12／13的身份／坐标／可见／未行动，并继续检查所有可见单位acted清除，不能把视觉投影回灌SAV。

72项自然纯测及6＋4项第二关共用回归通过；实际失败PNG再由新验证器离线通过，正式round4 UI Save仍待真实执行。实际命令／退出码／输出和新旧三文件facts登记于 `natural-arrival-focus-validation-20261109-154602-345628.json`；没有另存raw stdout日志，不把其结构化记录说成原始日志。游戏App、planner、C／JSON和生成结果未改，不重复92项CTest／Python全库。

由于本次仍只有原80项passed、没有任何新增整场passed，复用既有入口建立独立新来源见证 `startup-and-arrival-focus-witness.json`（SHA `ee33d582cbdf135db600ad8e0a973a3807a5ecc071015acf055584a74dccb648`），精确保护初始化6＋工具3项映射、旧源归档、全部失败／纯测／原帧证据与原见证；原 `1c6aa24c…` 及冻结helper不改。不是同候选下忽略工具变化，也不丢弃新通过项。新来源只读计划通过后，于15:49启动监督PID39632／owned外层driver31324，临时方案 `f249bbb1-ef9e-4baa-ae70-0711bd6a1b80`；1秒／5秒均真实ON。总收据为 `history-after-arrival-focus-fix/receipt.json`，Suite为 `20261109-154923-490010`。当前重新执行未通过的完整natural，再接3项B6及27个outer；新批次若中断，固定本见证并用同候选入口续接，不能重放已通过项。SD3尚未提交／本地集成。

15:58 检查点：原版已通过旧超时焦点，正式UI Save生成 `original-natural-stage0/original/turn-004.SAV`；独立解码实测turn4/count14，raw0为unit0／`(11,12)`，raw12为unit1／`(11,13)`，raw13为unit3／`(11,12)`，所有可见单位acted均清除。新验证器已据该真实Save继续规划，本次同格修正取得原版实际联合证据。整场natural及SDL端仍进行中，不新增整场passed或提前记SD3完成。

本批次随后在 `natural-0142-to-actor-1` 导航UP进入同一重叠格时停止，SDL仍未启动。普通导航的单actor／上下两带模板尚不含同格两个actor，原失败帧保留；不是第一条720像素／真实Save门失效。真实turn4快照SHA为 `9b7bb5b76b086e5ed7bb45dc124ccec1e176817573852fb0944de194a88f4d41`，其联合校验在最终comparison中为true。原版owned窗口正常清理，监督39632已恢复Balanced、删除f249临时方案，原值不变；总收据failed但冻结输入守卫true，整场passed仍为原80项。

16:09 继续收口导航工具：基于该端正式Save确认的14人源，复用完整同格资源，在普通光标门未命中时补该已证组合；不改C／JSON／App或扩展未知重叠。旧ee33见证、三文件及本轮失败先归档，再建立新的明确来源epoch后续跑。阶段记录中的15:58局部通过保持其实际范围，不提升为整场natural或SD3完成。

16:22 续测：导航修正只在旧cursor门失败、stage0/turn4/count14及raw0／12／13的静止、未行动、身份／坐标完全符合时复用既有720像素绘制器；未知邻居和变化后的动作状态不借该门通过。实际0142失败帧与真实Save离线通过，74＋6＋4项纯测通过且三份raw日志保留。下一ENTER的原版 `0x18b84` 和SDL只在此步骤绘制全场再叠摘要，没有COMMAND阶段的selected尾画；既有同格顶24行及完整6258像素摘要预检通过，但不把合成预检称为下一ENTER实机执行。最终三源及证据在 `same-cell-navigation-validation-20261109-161657-012572.json`，测试后仅纠正一处docstring来源措辞：原版127a9直接按raw绘制，SDL按y/x稳定排序；本例同格raw顺序一致，下邻头与本格sprite像素区不相交，既有720结果不变。

新见证为 `startup-and-arrival-navigation-witness.json`（SHA `82641f7f44b0b11359ab2f7c7bdabc852cb7bcb8640d8a8f7f0a605f57d5a691`），仍9映射／167当前源；只更新工具3项after，保持原before及old80锚，严格保护ee33、实际Save／PNG、84项日志和增量diff等。旧epoch的36个live事实经真实归档核验，其余1466事实仍在原路径核验，未修改事实API或冻结helper。只读计划通过后于16:22:48启动监督PID45376／外层driver32708，Suite `20261109-162256-509737`，总收据 `history-after-arrival-navigation-fix/receipt.json`。临时方案c2e7cf78及1秒／5秒真实ON登记于 `arrival-navigation-power-20261109-1623/receipt.json`，最终恢复待该监督收据。整场natural／余下验收继续运行，SD3仍未提交／集成。

16:35 检查点：当前原版已到第6回合；第4回合的真实Save、同格导航、选择、主角移动及下一角色攻击目标等资源门已连续实机通过，确认W2导航补充门实际可达，原有selection门无需改动。仍是自然实战prefix，SDL及整场结算待继续，不新增整场passed。

16:42 检查点：原版natural整场已通过，含8回合自然战斗、胜利对白、城镇结算、准备、stage1入场与正式Save；原资源／夹具／mtime／planner／工具／owned清理守卫均true。新入场来源为该端 `natural-stage1-entry.SAV`，SHA `c2ba6b6f3711e38fe1e3a89a7ac187a85e8b53ab63706fc644179a9d3638c6f9`。原版窗口已正常退出，SDL于16:41:18串行启动完整对应流程；配对及整批仍进行中，尚不计一项配对passed。

17:04 检查点：natural双端整场及既有比较门均passed，累计81项（旧App限定来源80＋当前eedd/W2新执行1）。两端各据自身真实Save独立规划；原版最后规划回合8、SDL为12，不声称随机战斗过程／RNG同轨。两端完整胜利／结算／准备／stage1入场、存档与清理均通过；SDL入场Save SHA为 `c8da043ea811fed6494df557acd079b796e244b2bae83ed4d73e6f7e5124ba4f`，原版新来源仍c2ba6b6f…。随后继续3项B6核心与27个outer任务。今后有新整场passed，必须同候选续接并继承W2及最新收据，不再重开初始化过渡丢失新证据。

18:05 检查点：默认 Suite 已完成84项（旧App限定来源80＋当前eedd/W2新执行4），3项B6核心和自然首关均通过。当前外层的村民全存活／损失／满包结算、箱子129／201／193及stage2存取共7项也已通过；natural-stage1原版端已通过，SDL端仍在自然战斗中，不计配对完成。余下18项AI、5场景音频及当前App的native-null尚待执行。当前总收据仍running，监督45376仍持有本轮临时方案，最终恢复结果未出。

本轮另以只读方式完成冷定义、生产消费、生成契约、来源审计及原热状态／SAV owner的收口复审，未发现新增阻塞；不把该审查计为运行验收。主树仍为master@88b47d7，唯一未跟踪的根FD2.SAV摘要仍为 `b0dd9349f449d252910994d796434efa14a1149de8a4ea116de22e0ec62965d7`。待全部验收后再提交完整SD3并集成，仅保留本任务提交，不重放已通过场景。

18:28 检查点：natural-stage1已双端passed，含自然胜利、村民分支、结算、stage2入场和活动Save；最终原资源、来源、工具／二进制及快照文件守卫均true。8项B7全部通过，18项AI中full00..07已通过8项，full08进行中。默认84项及所有新整项来源继续保持W2/eedd界定；音频5场景、当前App native-null、最终SD3提交和主树集成仍未完成。

18:45 检查点：18项AI全部passed，含full00..09、full03／05／06／07／08多目标、item-score20、behavior11-recover及behavior5-chest；每项原版／SDL、完成Save与封闭采集证据均由现有资格器检查。当前audio-five正在执行，尚不计5场景完成。没有为此重复92项CTest／Python全库或更改App；SD3仍待音频、当前候选native-null及最终提交／集成。

### SD3 最终验收与提交门（2026-11-09）

18:56，SD1—SD3 的已批准实现及当前 Windows 支持范围验收完成。SD3 补齐全部33组／真实分发表的独立来源、引用与语义分类审计，收紧外部流程入口／logic视图契约，验证默认turn255明确拒绝；统一审查发现的正式START重复初始化已修复。最终为79张冷表、四域30份流程／518节点及6个共享绑定；条件、分支、顺序、等待、取消和写点由冷定义控制，领域算法与原热owner／SAV边界保留。未留下生产双后端、重复手写编排或通用脚本VM，也未开放stage2战斗或后续玩法。

| 验收层次 | 实际完成范围与证据 | 结论边界 |
|---|---|---|
| SDL 与纯回归 | 92项CTest均有通过结果（91项批次＋Suite中的输入1项）；Python955项为954通过／1跳过。初始化修复后另有6项受影响CTest、52项静态数据回归；自然验证器最终74＋6＋4项纯测及实际旧失败帧离线复核通过 | 各候选／修复范围按上述时间记录，不说成最终App单轮重跑全部；LE开发者种子缺失的1项仍为跳过 |
| 原资源与静态／局部对照 | `build/sd3-audit/root-full-startup-fixed.json` 完成33组／99项、三类各528槽、1,899模板／2,322 placements及真实分发表闭合；完整106移动槽／2,898 byte payload与36条可运行脚本分开。已解码引用错误0；严格0／1检查按预期exit1 | 12条placement未配对、70条结构级移动记录及450条默认turn255候选继续隔离；1,171条保守引用按51支持／468未开放／652未知或未绑定分类。原字节／IDA chunk不是局部原指令执行或所有关卡语义等价 |
| 默认WGC Suite | `graphics-artifacts/20261109-162256-509737/summary.json` 共84项passed；继承原App `2d3392bf…1c3b6d` 的80项，当前App `eedd6353…b6e187`／W2新执行自然首关＋3项B6核心共4项 | 80项保留原二进制和工具来源，经7,888 byte默认初始化等价、其它13份include未变及精确来源见证限定继承；不是当前App fresh84项。原版首关8回合、SDL12回合，不声明RNG同轨 |
| 当前候选追加原版／SDL | `history-after-arrival-navigation-fix/receipt.json` 的8项B7、18项AI、音频5场景全部passed；音频为10个双端记录，封闭证据及来源守卫全部true。B7自然第二关已完成胜利、村民分支、结算和stage2入场／存取 | 该收据仍原样为 `passed_with_declared_gap`，其声明的native-null缺口由下一行单独闭合，不回写旧报告 |
| 正式无设备降级 | `native-null-startup-fixed/comparison.json` 由当前App实际运行标题CONTINUE→浏览→菜单→ESC，observed_null_backend／observed_continue及全部8项守卫true，正常退出0 | 十二项资源隔离副本，无FD2.EXE、debug入口或dummy video；discard由原代码合同支持，没有独立计数器观察，不是物理拔插验收 |
| 构建／离线材料 | 当前175份App源码、81份自建installed SDL SDK及14份生成include核验，独立Release离线重建通过；`build/sd3-offline-startup-fixed-receipt.json` | 新Release SHA `e813c86c…fcd5e1`；不冒称新发布包、Release原生运行或Linux验收，不改写旧R1 SDK身份 |

上述C:证据相对 `C:/Users/Soar/AppData/Local/Temp/fd2sdl-sd-20261109-tranquil/`；原E:图形路径继续通过junction定位，迁移的2,446个文件核验与所有旧失败均保留。`build/sd3-final-closure-w2.json` 严格复核28个外层收据、223项冻结输入、84项默认结果、10项音频记录、当前native-null和两份最终电源收据，闭合当前声明的历史支持范围；没有改写旧80项为新运行。

全部外层Job均无超时、残留或强制清理，显示需求已由各owner线程释放。两次最终批次仅在用户明确授权下使用一次零位移空闲重置及独立临时AC常亮方案；游戏输入始终是PID／HWND后台消息。监督45376和32776均正常完成，临时方案c2e7cf78与1b397a65已删除，原Balanced `381b4222-f694-41f0-9685-ff5bb260df2e` 恢复且原AC／DC参数不变。混音器未更改；实测没有遗留SDL／DOSBox／驱动窗口进程。

收口复审覆盖唯一冷来源、实际消费ABI、别名／引用、来源递归、取消／失败／重复通知与text_ready／ACK写点、原session和SAV owner、CMake及离线分发清单，未发现剩余阻塞；已删除正式START的重复数值锁。SD3按一个完整阶段提交，本地集成只允许本任务的计划／SD1／SD2／SD3提交，主树用户FD2.SAV须保留。实际提交与集成HEAD、主树构建和隔离快检结果另记 `build/sd3-local-integration.json` 及最终交付，不在文档中预填自身提交哈希。

未覆盖边界保持：Linux／macOS未执行；完整音画／波形、绝对时序、物理设备拔插和所有未知关卡语义未验。既有collection-only／严格PIC时序等限制仍沿原记录，不由本次行为回归提升为全保真。没有需用户先作决定的技术取舍，也不自动开始SD之后的新玩法阶段。

## 通用逻辑完整性审计与路线整理（2026-11-09）

本次为纯文档交付，审计基线 `c1b6598`。已核实 Git 中 SD1 `6b24b33`、SD2 `ecb154a`、SD3 `c1b6598` 均在本地 master，并只读复核原 SD 会话的 `build/sd3-local-integration.json`：主程序已重建、用户根目录 `FD2.SAV` 保留、未远程推送。SD 中途显示卡点和未集成状态保留为历史，不作为当前状态。

新增[通用逻辑审计矩阵](common-logic-audit.md)，按 C01—C28 核对真实源码、冷定义／执行器、支持门、测试判据及既有原版证据。区分逻辑未实现、呈现／正式入口未接、原版语义未确认、仅缺测试、仅缺场景绑定和未审计项。loot／progress 及通用事件条件／等待不因进入冷数据就归为关卡专用。旧 B／A／R／SD 完成历史保留原支持范围；统一优先级当时已更新，旧路线见[完成归档](completed-stages.md#旧优先级归档)：G1 通用取证 → G2 战斗机制及动作呈现 → G3 持久队伍／准备服务 → G4 共享流程与运行 → G5 统一审查回归 → S 关卡专用适配。当时所有G／S均未实施；后续完成与当前待办不由本历史段落判定。

| 本次实际核对 | 结果与限制 |
|---|---|
| Git／代码／资料 | 当前隔离树先从 `88b47d7` 快进至 `c1b6598`；读取 AGENTS、README、计划／阶段记录和相关系统／格式规范；本树未发现 CLAUDE.md／`.cindy/project-knowledge/TOC.md`。按战斗领域、事件／准备／存档、共享运行三组交叉审阅，再复审分类、来源与依赖 |
| 原资源结构 | 既有 `audit_stage_data.py` 只读主树 original_game，输出 ignored 的 `build/common-logic-audit/source-inventory.json`；结构成功，33组／208分发表槽、三类各528槽、1,899模板／2,322 placements、106移动槽／2,898 byte与SD3一致；语义仍为 false。51支持／468未开放／652未知引用、70结构移动、450 turn255、12未配对均保留后续分类，不计全语义通过 |
| 文档与自审 | 核对源码／测试文件和关键符号，纠正「成长表已有＝32..67构造已支持」「法术记录可读＝玩家可用」等误判；明确 G2／G3 必需执行器随阶段交付，模块合同互引不构成硬依赖环；同步战场／音频／静态数据工作包与文档索引 |
| 验证边界 | 只执行结构来源审计及文档链接／格式／Git 检查。未运行 SDL 自动测试、局部原机器码、DOSBox 原版、WGC 或历史全套测试；未新建／更新 IDA、生成伪 C或功能代码，不改变支持门 |

关键剩余通用范围包括：其余合法玩家／AI法术和完整动作族、派生形态／转职写集、>16人出阵、商店／教会服务、未开放设置、未解输入与事件合同、Round横幅／raw26状态抖动／跨框提示及共享缓冲来源。它们不能凭当前场景通过或继续禁用宣告完整。G1须分类全部来源未知和未审计服务；通用未决阻塞相应阶段，确证只有参数／绑定的内容才进 S。G5完成门及残余表见审计 §6，不承诺未取证的全游戏绝对完整。

本次没有已确认且必须由用户先决定的重大技术取舍；采用既有冷定义、有限领域执行器和热 owner，不建 VM／事件总线，不实施全部缺口。文档按一个独立提交交付并仅集成本地主项目，实际提交哈希／集成 HEAD 随本次交付报告，不预填自身哈希；不修改原 SD 工作树或启动其后续开发，不做远程推送。

## G1 通用语义取证与来源分类（2026-11-10）

计划 `b4c4bfa` 集成后，用户授权在原 `silent-hopper` 工作树连续实施 G1—G5，替代此前仅做审计的范围；不自动开始 S。G1 保留原 SD1—SD3 完成历史，按28项矩阵追到真实分发表、共享尾、字段写点和有效域。新增依据为[战斗](../reverse-engineering/common-battle-semantics.md)、[准备服务](../reverse-engineering/common-preparation-semantics.md)、[事件来源](../reverse-engineering/common-event-semantics.md)、[共享运行](../reverse-engineering/common-runtime-semantics.md)。

| 本阶段成果／实测 | 范围与结论 |
|---|---|
| 逐源分类 | `audit_common_scope.py --require-classified` 实际通过；208分发表槽、1171保守引用、106移动槽、12前缀外模板共1497行。622仅缺绑定、388通用缺口、450原表外、1执行default无业务操作、36既有支持脚本；未归属为0。原结构审计仍保留语义false，不把分类通过提升成全部冷图还原／新玩法开放 |
| 领域与有效域 | 确认96 actor原容量、metadata有效模板计数／同索引placement、stage32有效ID48／66的邻接构造窗、script24零步方向5、450 turn255潜在表外调用。正常初始／学习／合法转职的位图来源经376个非FLIRT外部callee闭包及owner核对，最大12法术；不推广任意外部SAV。买卖／复活／转职／出阵16或20人及选满后取消的写点已静态确认，产品实现分别留G2／G3 |
| 共享运行归属 | G是原版browse无操作；Esc／Z／KP5共享轮换、设置XOR、Round／raw26／嘴型／水色及动态token有共同规则。13透明像素有先前资源写入，归G4共享页历史／绘制顺序，准确最终色及修复未验。旧END症状仍待定位，不宣告消失 |
| 实际IDA发布 | 三组Domain只读证据与严格LE／fixup镜像逐chunk一致；独立副本真实应用12名称和34f38共享尾后再导出。1312函数原型／边界未变，306登记，2项既有Hex-Rays失败；11项清单校验通过。原350be尾owner保留；无原字节修改、猜测类型持久化或新fixup基线 |
| SDL与工具验证 | Windows Debug程序及后台输入driver构建通过。106项本阶段Python专项通过（来源分类12、G1读集／共享尾24、显示／电源38、音频冻结18、发布14）；719处源码地址全为canonical。未执行SDL全历史CTest或G5统一回归 |
| Windows原版／SDL定向运行 | 新显示路径首次仅ES请求未唤醒OFF的失败保留；单次零位移空闲重置后实测OFF→ON、Default桌面、指针不变、同线程释放。随后串行双端标题／菜单后台输入和WGC门通过，4个owned目标正常退出，无强制清理／残留；原资源、二进制、来源守卫true。仅覆盖既有入口校准，不是新服务／法术或全历史验收 |
| 电源授权与恢复 | AGENTS及图形规范已纳入用户主动唤醒和必要临时计划授权。默认只申请显示需求；可选临时副本采用写前日志、owner身份／冲突检查、失败恢复。真实原计划／AC300／DC180只读ABI通过；本轮未切换电源计划，38项显示／电源纯测包含恢复竞态。原Balanced不变、混音器未动；临时方案写入路径尚未原生实测，不冒称通过 |

证据保留于 ignored 的 `build/g1/`：`common-scope-final.json`、`common-scope-receipt.json`、`source-inventory-final.json`、`ida/`、`display-native.json`（失败）、`display-native-idle-reset.json`、`display-background/comparison.json`、`display-wgc-receipt.json`、`power-native-readonly.json`。早期读集中的伪C摘要保留采集时Git输入身份，不替换成此次新导出的摘要。

交叉复审实际发现并修复：分类器遗漏containing chunk的保护、未决family误算通过、共享尾的文本／地图／masked AI及回合条件漏归类；临时计划激活／恢复期间的用户切换竞态增加写前复查和最终核验。OS缺少计划切换CAS，仍如实记录最后检查至调用间的系统竞态，不宣称原子恢复。最终专项复审无剩余来源归属阻塞；376callee闭包由采集者核查，复审者未独立重跑，不扩为双重执行签名。

G1 完成门限于本审计来源的语义族分类与必要依据；`all_game_semantic_complete`、`original_runtime_executed`、`new_gameplay_enabled`仍为false。没有局部原机器码执行、新服务原版运行、所有对话参数轨迹或Linux验收。G2—G5尚未完成，接续G2完整战斗机制与必要动作呈现；本阶段不解除玩法支持门、不开始S。

## G2 通用战斗机制与动作呈现（2026-11-10）

以下前段保留阶段内候选身份；最终收口见本节末尾，不将中间检查点误作当前状态。G1提交`5b9619f`保持已集成；G3—G5未实施，S未启动。当前矩阵覆盖更新和剩余阶段门见[审计§9](common-logic-audit.md)。

已执行Windows/MSVC Debug定向27项CTest（包含96容量／同索引构造／cache、成长、法术／Item提交与失败、呈现、存档和直接受影响的旧资格断言），当前均通过；不是G5全历史回归。冷定义当前168表、五域36流程／565节点／6绑定，原9单位表8528B＋alias48B、106脚本2896消费字节独立核验。具体后续候选的呈现修复仍需其自身直接复测，不把此前27项视为未来改动已通过。

局部原码已执行descriptor default760例／BIOS清队771次；派生核心1c81f与实际C API 23040向量一致，包含完整80B、贡献DWORD与RNG次数，未直接观察C中间写序。原4e127完整127B又经180向量验证，其arg_C颜色被忽略，实际按stride低BYTE填色；含literal0／alternate／skip与全部目标guard／写地址。三类局部执行都不能替代DOSBox实机或完整呈现验收。

`build/g2/native-teleport23-01/`已双端通过首层／目的地层取消和完整22987B存档门；`native-derived28-01/`已双端通过真实stage22 unit19模板派生、固定FIGANI59八effect marker、相关HP／EX候选、唯一奖励值488自动页与一次END后的全存档；原件PNG身份补核为`derived28-reward-identity.json`。`native-composite34-02/`和`-03/`业务／存档均过，但呈现门没有通过，不能据业务pass签动作呈现。它们均正常清理owned进程、保持原资源和存档守卫、同线程释放显示需求；未改电源计划或混音器。

当前34离线原件门从原资源证明intro157→special67→三组FX／flash／绿浮字的顺序。SDL旧192常色不符；首次按caller色表146／72／216修改也被完整callee与原截图反证，随后撤除这个无真实消费者的新增表，改为fixed456域有效常量200。动态stride的1da16其它caller须分别处理，不能照抄200。原`native-composite34-01/`因脚本误将含125透明像素的回合横幅当不透明矩形而超时，保留失败、没有盲发额外键，后按原透明写mask修正；该失败不记产品bug或通过。

最终200共享getter已经Game与MAP／Item共用，新增6色参数×3裁切全像素／owner回归及最终5项CTest通过。`native-composite34-04/`业务门及`composite34-04-visual-callee-gate.json`双端11个有序资源阶段均过；`native-teleport23-success-01/`成功目的地／MP／焦点／一次END全存档双端过；`native-item-use-05-01/`、`-19-01/`、`-20-01/`三代表正式路径及独立cache门均过。112项相关Python源／发布／schema／oracle回归通过。所有目标正常清理，无强制退出／残留，显示需求释放；这仍不是全动态RGB或声音等价。

上述为前一保存点的候选与证据。后续已接续的代码、修复和未闭合门见下节；该检查点尚未提交／集成。END批量恢复与回合通知编排仍归G4，G2的必要单动作反馈不得由此延期。


### G2 接续：SHORT、单恢复及 FULL9（2026-11-10）

SHORT AI物理现有面板、HP条、六武器FX族、bank64声音、实体64与反击重画；恢复复用同一有限同步服务，bank31/cue4、实体200→normal及三wait后HP提交。`progress.json`新增三个流程，现171表、五域39流程／606节点／6绑定。玩家物理经完整caller核实始终FULL，已同时修正算法与呈现分派；`1e611`是地图／面板重画，原第四实参未提供，安全省略该未知索引HP条，保有证固定55及明确第二条。来源和有效域见[通用战斗语义§9](../reverse-engineering/common-battle-semantics.md#9-g2-short恢复与-full9-跨目标补充)。

FULL9移除整动作多目标灰门，只对不存在的beam part省绘制，保共享BYTE／原效果／贡献／HP插值／RNG。真实public Game探针21例在`full9-c-probe-21.jsonl`，SHA256 `d2ded3e6a09f3aeb63d5418f1eaf233e6e39624a38c4dd82570db5bb6f5bb203`；其中17例原机器码对照全等，最终复审收据`build/g2/full9-wrapper-comparison-final.json`，其余四例95目标只属C容量。原完整31266、2ce1a、伤害core与真实RNG执行，绘制／音频／等待为显式替身。C侧为每次RNG前及TARGET／FINISH观察，没有逐C内存写trace或renderer调用；原目录外请求与C省绘制的证据分开。旧16例与初版17例收据保留原身份。

本次先在同一候选重建并通过六项直接相关CTest（attack、AI commit、progress、magic FX、battle animation、full magic）；随后交叉审查发现raw5合法缓存状态被新renderer过早拒绝，修复并定向重建，battle_animation现30组、field_unit_render／field_progress_play三项同轮通过。只读资源probe修复了AI11先magic后physical／恢复的晚资源失败；只读资源身份按scene去重，不新增planner或持久缓存。当前源／发布／flow／新恢复工具31项Python通过。旧27项、112项与七组双端按其自身候选保留，不记本轮全量或G5回归。

原生恢复首轮`native-recovery-01/`在原端有序实体200→normal和bank31/cue4身份门通过，但测试包装器误将`source_guard`传给`wait_state`，保存门未完成，SDL未启动。修复包装器后，第二轮`native-recovery-02/`声音门通过而未捕获solid200，仍未开始SDL。两轮均source守卫true、owned进程正常退出、无残留／强制清理、显示需求同线程释放，原计划／混音器未动；两轮均不算双端通过。新增真实Save协议纯回归及可选采样间隔：旧共享请求默认33ms不变，恢复固定请求16ms，收据分别记请求与真实WGC时间戳间隔，不把请求频率当实际帧率。

`native-recovery-03/`以新16ms请求完成实体normal→solid200→normal、bank31/cue4与全22987B存档双端门，source/owned清理/显示释放均true；两端实际时间戳中位间隔分别33.35／16.68ms。随后实际PNG复审发现SDL恢复页多info板，旧实体ROI门不覆盖此差异。正在只改恢复背景投影并增加原地图tile正门，保留03为旧信息板负例，不把它扩为全画面通过。

`native-recovery-04/`新增独立原FDFIELD/FDSHAP tile156的完整576px正门，实体solid／normal-after均与背景同时匹配；原03离线通过、旧SDL03被525/576像素差拒绝。当时恢复首背景改现有mode0；反击按tiles＋127a9顺序重画，但当时冻结terrain的判断后来被11eee完整callee反证，最终修正见下文。App原raw26 jitter门保留，SHORT_MAP原11cac点不动。30组呈现及相关play定向CTest通过；04原版／SDL的实体、背景、bank31/cue4和全22987B均真通过，source／owned正常清理／同线程显示释放true，无电源计划或混音器改动。App SHA256为`f1137e10ae3481f4d15491b30b6146d290199773cc79f8dff3bfbdaa3716f288`。仍不承诺全动态RGB或精确音频时序。

上述为收口前检查点；最终状态如下。

### G2 最终定向验收、复审与提交门

SHORT01原版全门通过，SDL业务／声音／面板／后续FX通过，但80ms首FX落在100ms采集间隙内，未捕获完整solid64，不记双端通过。独立CPU探针核真实Game／资源的首呈现449个不透明点（436个64＋13个FX6）全等，只替换时钟／present／音频，不当作WGC。SHORT02试图外部改640客户端，Win32返回true但SDL固定960未变，SDL动作未执行；失败工具与SDK源码摘录归档于`build/g2/short-resize-attempt/`，永久工具已删除resize路径，未加产品窗口功能。

原1eb05只等待十次25ms；1e7f6初始HP条直接写VGA，无独立等待／翻页。撤除SDL新增的零时长中间上传后，HP条仍先写CPU，首次STRIKE与实体64／FX6一并上传，80ms不变。`native-short-03/`默认960×600双端通过：全22987B、HP42→31、面板／完整实体454点（441个64＋13个FX6）／后续FX／HP条及bank64声3。声音身份分数原版0.9764235、SDL0.9883871；source守卫、owned正常退出／无残留、同线程显示释放均true，无计划／混音器改动。实际App为`cdcc279ad5e628e40c161dd20cd6d47e9135ea277bf8619ff43ef4db5429def8`。

最后来源复审还修正两处上下文：玩家WAIT的12cea总刷新，信息板须结合option3与COMMAND／selected／side2 caller保留，AI END抑制；仅在现有exchange局部表达，不新增Game或SAV字段。1e611中的11eee会推进53A40地形门，故counter仅推进terrain两字段及127a9的App jitter，不推进1297d idle/walk或水DAC。真实Game提取原地形逻辑，覆盖55ms／同tick／长间隔不追赶／时间倒退／非法对象和组合字段写集；呈现31组覆盖caller八组合与counter写集。最终构建和4项直接CTest（attack、AI commit、unit render、battle animation）通过，记录`build/g2/final-counter-terrain-{build,ctest}.log`。保留既有一条测试常量cast警告，未宣称零警告。恢复04及SHORT03保持各自实际二进制身份；后者没有反击，不称最终counter已原生跑过。

最终相关Python 49项通过；实际Domain数据库已再次应用注释、保存、导出及重新反编译，1312函数边界／原型和全部segment字节不变，314条命名登记、2项既有Hex-Rays失败保留。最后刷新11项SHA256SUMS，发布交叉校验14项通过。171表生成检查及runtime独立源门（11表、10额外cue、22负例拒绝）通过；这些不是重跑静态LE全验或原版运行。

正常caster专门门以32base／68growth／20learning／合法转职、1887有效模板和408FIGANI核50个玩家FULL组合、238模板实例／30独立组合均非empty。原FIGANI count是BYTE，+1为split；初次WORD诊断被真实split1头与目录反证，未进入产品。stage13 unit103／30的零分／MAP排除单列；任意外部SAV或未来来源不在声明内。FULL9最终差分也使用BYTE／split，17例原机器码全等，95目标四例仅C容量，报告`full9-wrapper-comparison-final.json`。

G2最终保留171表、五域39流程／606节点／6绑定；冷图、有限领域操作、现有热owner／80B／SAV／RNG合同无新平行状态。交叉复审修复了raw5、solid stride、caller信息板、初始上传及counter时钟问题；无剩余已识别G2机制阻塞。完成G2阶段提交后接续G3，不等待“继续”。阶段Git身份与本地FF／主程序重建／用户存档守卫写入ignored `build/g2/local-integration.json`；G3—G5仍未完成，S未启动。早期27项CTest／112项Python、七组新增双端与本节补充按各自候选联合成立，不冒充最终程序单轮全历史、全RGB／波形、Linux或G5回归。

## G3 持久队伍与准备服务（进行中，未提交／未集成）

G2完整阶段提交为`61d58560174b8c2893a1e33a76b4bdf07fbd754e`；本地master由G1快进，主App Windows Debug重建通过，用户FD2.SAV摘要`b0dd9349f449d252910994d796434efa14a1149de8a4ea116de22e0ec62965d7`未变。主树仍只留原未跟踪FD2.SAV，无远程推送。15个生成include正文一致；五份raw JSON SHA注释因主树CRLF checkout不同，初次逐byte相同断言失败后逐项确认只有这64hex注释变化，JSON值及生成C正文不变，没有伪造两App同摘要。完整集成见`build/g2/local-integration.json`。

G3现有候选采用同一preparation owner；新增纯领域分写、通用原库存／1b750 adapter、准备表、真实服务资源及动态文本共享，不新建运行期队伍或SAV字段。首批build及五项CTest通过，记录`build/g3/service-domain-{build,ctest}.log`；分别为prepare_item、unit_stats、prepare_play、text_expand和直接受影响progress_play。第二批prepare领域7组、field_info与转职scope测试通过，prepare_play新数字门因误要求字形1也为6x8失败；原三bank字形1确为5x8，其余6x8，修复后field_info／prepare_play两项重建复测通过，旧失败`prepare-promotion-ctest.log`保留，修复收据`service-number-fix-{build,ctest}.log`。后续源修及新页面仍需自身验证，不沿用旧候选通过。

来源复审确认：28cbd／28f65列表压缩可见ID，但提交仍同rank直接raw，不作反向映射；self满8拒，非满同一80B先移再追加flag0、只重算送方。密集保持只是明确前提下的有限原语性质，未冒称穷尽全部事件直接store；新准备原语不以密集假设拒已知稀疏。28632比较专用DWORD预览不套1b750状态／窄化。资金signed JGE与回绕业务、滚动sprintf首8字符及187d6末态负数夹0三层分开，尚须完整滚动取证／呈现验收。

本阶段尚未完成：全部准备冷flow／服务正式输入、16／20选阵和NoTown、必须成员／隐藏键、金额动画、合法empty转职idle图形安全域、必要双端P1—P6与双向SAV、最终来源／支持门复审及完整G3提交集成。最新源域和未提交内容续接于ignored `build/g3/CONTINUE.md`，G4／G5未开始，S未启动；上述纯测试与局部源阅读不能替代原版运行或G5历史回归。

G3本轮最终保存点：187输入表、六域41流程／907节点／12绑定；preparation的services221节点／deployment80节点已定义并注册查询，但69项有限op尚未由正式prepare_ui消费，不能把图结构通过当服务已实现。独立源337B及服务／部署／portrait锚、7个坏输入拒绝通过；源／图／oracle共18项Python通过，13组受控fixture只为预期模型，P5后期／P6没有post-action酒店Save。一次CLI误用不存在的test_static_preparation_flows模块已保留失败日志，改为实际test_preparation_flows后通过，不作产品修复。

最后源复审修正service四头像的特殊origin：128..132原1956b／16559各有位置，不得套酒店107,5。五原点迁入唯一冷表，portrait getter供Game提示与准备共用；后续frame0为正向，首次notice reverse仍须正式UI实现。复审也将准备Load预检的Game资源载体删去，使用只持必要档案／字体／sprite／info的小资源owner，Game及准备共用纯详情上页门，不伪造ready Game。新成员／复活／选阵纯页及preview遵4e29c将skip填73，选阵仍按4e22a／4e1a6分别普通／灰色。转职16→48真实empty idle只省未定义sprite／目录读取，保BG／固定等待／渐变与外层业务；不提升为原未定义像素等价。

这些改动后实际构建工作树Debug App及相关targets通过，8项定向CTest再次通过（info、save、prepare、prepare_view、stage_transition、prepare_play、progress_play、battle_animation），记录`build/g3/checkpoint-{build,ctest}.log`；测试输出最后去除过时的硬编码“31组”字样，逻辑未改。此前source-view八项还包含共享库存原语直接影响的attack／item_commit。此联合证据不计G5全历史或G3正式服务验收。当前所有子agent冻结、无G3 GUI／电源改动，G3内容继续未提交／未集成，后续从`build/g3/CONTINUE.md`接正式UI／源差分／双端验收，不自动开始S。


### G3 正式服务接线续接（2026-11-10，阶段仍未完成）

在同一 `silent-hopper` 工作树续接，保留 G1 `5b9619f`、G2 `61d5856` 已集成状态。G3 未提交／未集成，G4／G5 未开始。正式准备入口已消费 76 个有限 op；services 247 节点、deployment 80 节点，仍借唯一持久 roster／资金／RNG owner。NoTown 与26行城镇来源独立于 battle 支持门，不开放晚关战场。

源复审修正：购买／Give目标作为显式冷参数；装备选择／兼容／写入循环展开到图；490..494文本内部 -3 已负责 ACK，移除重复等待；music 第二参数保 loop_count；服务入场 sfx1／loop3 与音乐由冷图编排。26996只释放底页，DATO／origin保留给后续26eda；成员／装备详情临时借四帧当前头像与原3208坐标。酒店Load拒NoTown，成功Load重建精灵目录；无效目录只省未定义sprite，不拒整服务。2b843重复text取最后匹配，完整持久表保留。

本轮 `formal-service-build-01.log` 构建及六项定向CTest通过。随后新增完整服务图的menu→product回归，暴露并修复菜单后漏26996及raw图标零色应复制；`formal-service-build-04.log` 与 `formal-service-ctest-04.log` 覆盖修后正式整图。局部原码差分探针、其余业务／原版验收仍在进行，不计G5全历史回归。

Windows原版 `build/g3/native-p1-buy-yes-04/` 的original端完成标题LOAD→购买武器／选择装备→酒店Save，全22987B符合独立模型，来源和正常清理守卫通过；SDL欢迎页后exit1，整场失败，不记双端通过。该冷图关闭底页缺口修复后，以严格复核原工具／资源／真实PNG／完整Save的 `--original-evidence` 继承此原版成功端，仅复测SDL。新旧来源分别保留，不能把旧原版PASS提升为新SDL或全G3通过。

图形运行申请必要显示需求并确认实际WGC；已结束的03／04各owned窗口正常清理，需求同线程释放，未修改电源计划／混音器，用户FD2.SAV摘要保持原值。正在进行的05结果与最终续接状态以 `build/g3/CONTINUE.md` 和各收据为准；不把运行中或模型输出记验收完成。

### G3 定向验收与审查修复（2026-11-10，候选）

正式 `prepare_ui` 已消费 76 项有限 op；services 为 247 节点、deployment 为 85 节点，187 张输入表。新增 NoTown 保存 caller 参数后的 ABI 为 `cbf4db56f301521f2152c72539580a6264802afbd914118b9f96d0d71782e5ec`。旧 69 op／221／80 节点的保存点保留为开发历史，不再描述当前实现。G3 尚未提交／集成，G4／G5 尚未开始。

当前正式验收分开记账：

| 范围 | 本地证据 | 已通过与限制 |
|---|---|---|
| P1 购买四分支 | `native-p1-buy-yes-05`、`native-p1-buy-equip-no-06`、`native-p1-buy-insufficient-06`、`native-p1-buy-full-06` | 原版／SDL 正式输入、必要页面及完整 22987 字节 Save；非逐帧／完整声音等价 |
| P2 出售与自给 | `native-p2-sell-equipped-06`、`native-p2-self-give-07` | 已装备品出售、自给 raw 移除／追加及完整 Save；不同成员和独立 EQT 路径另补 |
| P3／P4 | `native-p3-revive-09`、`native-p3-revive-insufficient-09`、`native-p4-promote-normal-10`、`native-p4-promote-special90-10` | 原费用／flags／HP、原耗物／各通知增量／终态与全 Save；无候选真实等待先正验，不用额外键代替就绪 |
| P5／P6 | `native-deploy17-01`、`native-deploy21-sdl-06`、`native-notown22-01` | 17／21 人选阵最终 NO 与重入、缺必需成员；同一安全点原版单 RAM 与 SDL 停态 PDB 核完整热 owner，全 SAV 与 mtime 不写 |
| 双向手工存档 | `cross-save-native-01` | 两端实际交易输出交给另一端正式 LOAD，再 Save 新槽；全文件、源槽、active 和未知尾保护，不以同模型双跑冒充互通 |
| 返回音乐状态 | `native-audio-p3-01`、`native-audio-p4-01` | 真实等待中原版 RPC／SDL PDB 量得曲 11、loop 1／0、完整 FDMUS 资源及播放状态；瞬时 17／16 仍属原 caller 必经路径推断和旧 WAV 候选，未声称 PC 轨迹／全波形 |

P5 的旧 SDL02—05 都通过页面，失败分别在观察器协议或输出：05 已正确取得 PDB 类型、2572 字节长度及字段地址，`.writemem` 无法创建文件；改为同一 CDB 停态内经本轮进程句柄一次完整 `ReadProcessMemory` 后，06 完整 owner 验收通过。记录 PID／映像／地址／长度／SHA，拒绝短读并正常 detach；未改成扫描猜 roster 或把前置 Save 当后态。原版独立成功端只在严格复核工具、数据、截图和 dump 身份后继承，来源与当前 SDL 身份分列。

审查修复包括 Give 按原出售型列表显示 3/4 价、`16e24` 仅作两个合法 origin 的 CPU 滚屏而不新增 96 ms／采样、`2ac7d` 的 MV 行用 y176、不重复滚屏，以及每次 prefix 重排后显式重绑 sprite 目录。完整转职已采集序列的离线内容门见 `promotion-series-content-03`：四端均有完整旧／新身份及有定义的 320×50 白化带，按真实 PNG／PID／HWND／SHA 核序；普通原版未命中完整白化帧，限制保留，不升格为全帧等价。

后续审查发现 NoTown YES 保存不能借酒店消费模式：原 `26152→2968d(0)→29bcb` 直接读键，不耗 `16c57` 等待 RNG／画 129，且非零 NoTown 跳过 660 及其 ACK。现以冷 `manual_save(caller_kind)` 指定原消费者，不按 stage 重建业务分支；酒店与标题合同保留。另复核 `4ed34→4ed4f` 确为零透明，撤销此前把菜单／确认 raw 图统一完整复制的误判，完整背景合成门覆盖透明角；`4ed0b` 真正 raw 复制的零色行为不改。

修后 Windows Debug App 和六个直接 target 构建、CTest 6/6 通过，见 `notown-transparent-{build,ctest}-01.log`；包含存档、部署交接、准备领域／页面／完整 UI 及转职 scope。新增 NoTown 纯测以坏 660 文本验证不应预检或显示，核一次写档、无多余 ACK、RNG／上半页／贡献不变；透明绘制以非零底色核完整 64000 字节。Python 费用页的 120000→前五字符边界修后 18/18，通过日志 `fee-width-python-02.log`；首次测试夹具把 immutable bytes 当可写所致失败保留，不是产品失败。

独立原 x86—C 差分为 47 个正例和 6 个负例，含原库存、费用、重排、复活／转职分写；UI／libc／资源 hook 的限制单列，不冒称 UI RNG 或声音等价。IDA 已从最新 G2 发布库隔离复制，真实应用／保存 33 项新命名，347 项登记、1312 个函数边界／签名守卫不变；21 项静态检查、14 项交叉检查和最后 SHA 清单通过。两处历史 Hex-Rays 库函数失败保留，源库未改，未新作严格运行时 BSS 验收。

隐藏入口完整像素门、NoTown YES 实际 Save、独立装备及不同成员转交正由本阶段补验；当前纯测／采集／模型均不能代替这些运行证据。各结束 Job 正常清理，显示需求释放；本轮未修改电源计划或混音器。Linux 未执行。完整 G3 门关闭后才作一次阶段提交及本地集成，随后连续 G4→G5，不自动 S。

### G3 阶段收口（2026-11-10）

上述剩余门已关闭。G3 的完整成果包括持久队伍／16—20 人选择、原库存与交易、教会复活／转职、服务必需的冷流程／等待／取消、完整页面和金额／转职呈现、真实四槽保存及跨场景 roster 合同。业务状态仍沿 App／Game 原 owner、原 SAV 和 RNG，不增加 VM、事件总线或第二业务状态。各旧候选和失败记录保留；本节不把它们改写为同一次最终二进制全历史通过。

新增正式证据（均在 `build/g3/`）：

- `native-eqt-reselect-01`：两次装备确认及完整中间属性／库存页、重选和 Esc，再实际酒店 Save；双方全 22987 字节通过，中间强门防止最终恢复原字节造成假通过。
- `native-give-different-01`：收件取消后重新选择，raw2→raw0 真正不同成员转交，收方 7→8 槽、只重算送方、钱不改，再全 Save 通过。
- `hidden-entry-native-03`：Shift+F1→caption500→独立 Enter→bank63 六商品页，取消返回及整档／mtime 不写，两端通过。01 的原版严格角点门错误、02 的 SDL 修饰键丢失均保留为失败。
- `native-notown-save-02`：NoTown 410 默认 YES，完整 21 人一次存入槽 1，不经 660／额外 ACK 直接回已更新槽页，再 Esc 到完整选阵页；全容器原槽、active、未知尾均符合原 writer 写集。01 的新工具漏标已知低色槽页而在确认前退出，修为只有完整原资源匹配才接收，未降低像素门；6 项纯测包含白屏及改单点拒绝。
- `promotion-series-content-05`：正式只读工具从实际四端 WGC 序列核完整旧／新身份、原定义 320×50 白化带和返回页的有序轨迹。普通原版完整白化帧仍未命中，限制保留。
- `money-series-content-02`：购买减钱、出售加钱、复活减钱的三组六端，完整 48×9 原 raw 数字条 ROI 核前值→至少一个与前／末态均不同的滚动切片→后值；实际整档另解为同一后值。原资源、源码字节、PNG／PID／HWND／SHA 均封存，不声称全部 phase 时序或波形等价。

收口又修正保存提交边界：原 `2968d` 确认后先真实 writer，再进入可中断 `26996` 收框；酒店先纯预检 660 资源，NoTown 不预检／显示该消息。新回归覆盖酒店／NoTown×收框 hostquit／cue 错误，均恰好一次真实写盘、完整载回、未授权容器字节不变；坏 660 酒店零写。独立源复审通过，Load 继续先收框后交接。

后台组合键故障来自当前 Windows SDL 的 pump 末端 `GetKeyState` Shift 修正：传输成功后 SDL 物理快照已无 Shift。现只为 F1—F12 首按保存修饰元数据，仍需本帧键态脉冲才能消费、下次 present 过期，keyup／失焦清除，repeat 用本帧物理位；没有普通键 FIFO、额外脉冲或全局键态修改。`input_test` 的真实 SDL event-poll 路径加受控键态覆盖未读过期、UP、失焦和 repeat；独立只读审查及上述隐藏正式入口通过。

最终共享修改后 `save-edge-modifier-{build,ctest}-01.log` 的 App／2 项 CTest 通过，`final-shared-header-{build,ctest}.log` 的 5 项直接 CTest 重建通过。5 个新增内容／入口工具的最终联合纯测为 24/24，日志 `final-new-tools-python.log`；此前 Python、47+6 局部原码和实际 Domain 发布保各自范围。图形／业务、局部机器码与 SDL 自测分别记账，不合称 G5 回归。

本阶段完成门已关闭；完整 32 条部署投影、未出阵尾和结算回投由原构造来源、C 回归及既有正式交接联合验证，17／21 人最终 NO 代表不扩成整段后期 entry／settlement 的原生通过。曲 11 返回 owner 实测与中途 17／16 必经源调用／录音候选的层级继续分列。所有 owned 窗口正常清理，显示需求已释放，未更改电源计划／混音器；用户原 FD2.SAV 摘要保持 `b0dd9349f449d252910994d796434efa14a1149de8a4ea116de22e0ec62965d7`。Linux、逐帧完整白化／全部相位、全波形及严格运行时 BSS 未新增验收。

此次作一个完整 G3 提交，不拆微提交；提交与主树安全集成／重建记录在 Git 及 `build/g3/local-integration.json`，不远程推送。之后连续进入 G4，随后 G5 统一审查、全历史回归和消冗；S 尚未启动，通用总完成门尚未签核。

## G4 共享流程、菜单与运行（2026-11-10）

本阶段续接已提交并集成的 G3 `08f61f6`，在同一 `silent-hopper` 工作树完成共享执行器、正式消费者及对应定向取证。保留 G1—G3 与 SD 历史，不重做原阶段，也没有开放 S 战斗或增加生产 common stage 绑定。来源合同见[共享运行语义 §8—9](../reverse-engineering/common-runtime-semantics.md#8-g4-共享菜单与-phaseround-实施依据)、[session](../systems/battle-session.md)和[冷定义工具](../tools/static-data.md)。本记录只签 G4 工作包的已识别实现与下述证据范围；G5 尚未开始，通用总完成门未签。

| 本阶段实现 | 来源、owner 与实际边界 |
|---|---|
| 唯一 App 工作页与共享视觉 | main／Game／Scene／Play／准备页共借153216B page，保选择框、透明 LUT、地形／range、raw26、菜单高亮、水色及嘴型 DWORD 的调用者写点；不把 App 字段写入 SAV，活动 Load／新 Game不重置共享历史。确认框的整 snapshot／bottom 双复制及原0透明写有窄桥，不回灌任意整页 |
| 水色与场景合同 | 原11cac参数0才调用4e31c，非0跳过；Game runtime_refresh mode0为含水普通刷新、mode3为对应原实参1的无水刷新，common参数名为skip_water。移动／脚本尾／entry／纯delay分别按完整caller接线。arrival保12帧App页和新actor基底写序，尾不额外加1297d／water／11cac；背景重建、换图、视口投影沿现有限服务 |
| 回合、阵营与整组恢复 | runtime冷图驱动两个Round caller、82／80标题、END／自动整轮／March和整组HP→normal→acted顺序；统一处理等待、临时信息门与caller hold。活动Load不增回合、不重跑phase2事件；热队伍、贡献、RNG及一次提交仍归原Game |
| 设置、输入与文本 | 四项设置保原raw byte和各自归一／XOR写法；第4项与App临时51AAC构成双门并在低左右焦点切换板侧。G无操作，Esc／Z／KP5按已证资格和索引轮换。动态正文／UI0名称分bank复用原展开器；跨文本嘴型用App owner，普通等待不凭日志替代底框或立绘 |
| 共享modal与真实反馈 | system／loot／prepare确认共消费有限choice图；详情两次16c57等待、两页与portrait0／3随正式F2／acted Enter交付。hostquit锁存与真实错误分别传播，Core ACK／reply返回后立即截断，防无notice忙循环或继续奖励；清理不把ERROR洗成HOST |
| common事件父子续态 | 47 op／15 flow／115 node，无生产stage绑定。冷图决定分支、写序、等待、ACK／取消和晚错冻结；Game保父PC／真实actor／item，原loot owner只加深度1 child槽，不复制Game／roster／RNG。正式turn、player／AI、March、loot和const结果查询均接有限域；资源预检无业务／RNG副作用 |
| 死亡、AI物理尾和特殊格 | 1db65按冷图13次逐raw方向写／绘制，之后HP0 flags精确1，再12次消散、cue3、清理和refresh0；30锚只限未定义FX绘制，不截单位业务。AI physical kind5两次outer refresh0保持collect后／finalize前及descriptor后／reward前，child不借父尾。特殊格0／1移除无原义stage<2内门；其余mode按cue12→300ms→200ms→close→完整WORD callback(actor)，90项表域预检及执行时重验，未知绑定仍拒绝 |
| 等待与原时钟 | 3790a固定延时不推进actor／water或清键；loot MESSAGE的外wait0也无额外入口clear，最后一帧合法pulse可由真实16c57消费者读取。1e5c0限时等待保持do water→查键→signed WORD差值→必要第二次读钟→尾clear，STATUS在文本后由caller清键，CLOSE保已排键。列表等待删多余raw26、保water／blink RNG；总览按原入口TJR／稳态AWTJR顺序推进，不借通用tick补不相关deadline |

最终冷输入为 **193表、8域62流程／1390节点／12绑定**；runtime为68 op、6图321节点，loot为88 op、2图227节点。12绑定仍是已支持准备／Scene范围，不把common的未绑定规则或结构提取计为新关完成。冷定义、分支／顺序／等待／取消仍以 `data/static/` 为唯一输入，没有通用VM、事件总线、第二套热状态、SAV格式或RNG owner。

### SDL、独立原码与发布验证

本阶段按改动直接影响范围分候选构建和验证，没有重放无关全历史用例。`notice-wait-ctest-19.log` 的 progress／play_result／detail三项通过；`queued-key-ctest-20.log` 新增固定延时末帧与真实TEXT_READY后脉冲消费回归通过，旧吞键会在blink RNG前立即断言，另有帧上限。`modal-clock-ctest-21.log` 六项中 visual／player_controller／unit_render／progress四项通过，overview／magic_list两个旧夹具失败：前者缺新正式bind所需资源，后者仍hook旧render、未构造正式只读preview。保留21失败原件，未放松原完整像素／owner断言；明确补dispatcher测试替身及正式preview后，`modal-clock-fixture-ctest-22.log` 两项定向通过。其他direct CTest按各自 `build/g4/*ctest*.log` 的候选身份保留，不合算成一次G5全量回归。

`current-g4-python-22.log` 为6组新G4 runner／oracle共 **48项通过**；`current-source-python-22.log` 为reward协议／四设置／死亡原码源门／特殊格／notice源门共 **38项通过**。此前冷schema、runtime原字节、common来源及资源预检专项仍保各自原范围。193表独立生成检查通过，源码地址核887处均为canonical；这些不等于真实游戏运行。

| 独立原码或源验证 | 本轮实际范围，不能外推部分 |
|---|---|
| `common-event-machine-01.json` | 实际30类 C／原x86局部差分：完整35e0e／3623c／4df4c及登记helper、20926有限分支；C观察96×80B／32B状态／48B事件／cells边界和每次真实RNG前状态，不是逐C内存写trace或全部15图 |
| `visual-clock-native.json`／`page-lut-native.json` | 20类时钟差分，以及2例／1152像素工作页LUT差分；名字含native不代表窗口，收据明确不作SDL／DOSBox运行等价 |
| `scene-water-original-calls.json`／`arrival-owner-original-calls.json` | 10类原水色分支／caller参数，以及完整32999的12次投影和owner重定向；绘制／PCM／分配等替身边界保留 |
| `defeat-original-cpu.json` | 完整1db65有／无见证者两类，逐raw方向和flags实写、25／0次等待、12 LMI、后6次临时页恢复、cue3与尾refresh0；原画图和声音为替身 |
| `special-cell-original-cpu-02.json` | 完整190ac的9类callback前缀、No、caller与WORD；高WORD在间接调用前停止。90项分发表和原地图引用另核，slot12 `(2,0)` 初始未引用不被当成无需求或no-op |
| `notice-wait-machine-comparison-19.json` | 完整1e5c0九类与实际C helper差分，锁已有键、0tick、有符号跨界及第二次读钟；不是精确BIOS墙钟／WGC |
| `effect-page-owner-source-repro.json` | 12个完整函数和count 0／1／16／96的实际原指令stack／参数委托；不保存伪C猜测ABI或宣称像素／PCM通过 |

IDA已用隔离Domain数据库实际应用23个名称及33次G4注释，最终372登记／1312函数，持久签名／边界／chunk不变，不在保存前固化Hex-Rays猜测类型。原G3数据库不变；21项严格LE、14项发布专项和最终11项SHA清单通过，2项既有反编译失败保留。最终数据库SHA256为`b7ad629195bad11a70d4a6919d396cd8220a1a48d11b9175b6bd352504eb782d`；最后生成并逐项核验的`SHA256SUMS`摘要为`a886d6e3be05d484dc0f1ac770784de5a0c5652e61016eaab100cc316be9971b`。发布收据在 `build/g4/ida/publication/receipt.json`；命名／哈希通过不增加原版运行范围。

### Windows 原版／SDL 定向验收

以下均保留各自真实二进制、输入、WGC／原资源判据、保存及清理身份。`fresh_pair=false` 是复用已通过原端与新SDL端的端点比对，没有重新启动一组双端，也没有把原任务整体失败改写成通过。

| 实际收据 | 结论与限制 |
|---|---|
| `runtime-endpoint-comparison.json` | 原02成功端＋SDL03：Round001 Load→一次END→82→80→Round002、HP40／28与完整encoded Save相同。`fresh_pair=false`，原02整体含SDL失败仍为failed |
| `controls-native-02/comparison.json` | browse资格／轮换／设置三代表共六端通过，含G、Esc／Z／KP5、信息板完整OFF两侧ROI、低左右侧位及实际Save／Load raw选项恢复；不是全部键／typematic或共享RNG轨迹 |
| `details-native-15/`＋`details-sdl-series-18/`，`detail-endpoint-comparison.json` | 原15 F2／acted Enter两成功端与SDL18两成功端拼接，12完整portrait页0→3→0及无箭头；F2实际整档raw_equal，acted只签Save禁用／磁盘mtime。`fresh_pair=false`，原15的SDL眨眼超时及整体failed保留；不签native PC／完整热owner／精确RNG |
| `common-defeat-native-18/`／`common-full-native-20/` | empty与满包Cancel各真实双端通过：原真实callback39，对照非shipping SDL alias，child211一次授予或取消、父story11、完整Save。18另通过13旋转／12消散及cue3有限身份；alias始终不是生产stage绑定、任意common图或S开放 |
| `short-native-12/`、`teleport-native-20/` | AI SHORT弓代表以及Teleport23成功正式路径双端通过，对应原资源／HP或MP／焦点／完整Save等有限门；不等于重跑G2全部动作族 |
| `status-endpoint-comparison.json` | 原20与SDL22各44检查点：成长／学习／七状态页、真实WAIT→Load→Save，encoded整档相同，摘要`c3f8e52ef1871ae665da959e9a53d466020087e9574b9a8881baabe1d80472f0`。`fresh_pair=false`；原20整体SDL失败保留，SDL22是明确endpoint/probe而非新双端 `ok=true` |
| `overview-native-22/` | START总览稳态矩形／字段／状态、任意键关闭、只读业务与宿主退出双端通过；开合瞬态像素、整地图idle和完整声音不在门内 |
| `opening-native-22/` | 自然START→stage32／31／0，两端各117个原资源已证wait，首次browse及正式UI Save通过；不签整Scene逐像素、共享RNG或未知raw／roster全等 |
| `prepare-confirm-native-02/` | p1-buy-equip-no正式共享确认的原／SDL业务、页面和SAV通过；收据明确 `audio_identity_pending`，不把它升级为新cue／曲目身份通过 |

失败迭代原件继续保留：原runtime复制目录的FD2.TMP被错误列为静态输入、common首次speaker前置不合法导致story11 oracle超时、details旧SDL眨眼及状态旧SDL端失败等都没有抹成通过。修复只改各自来源／前置／消费者或夹具，未以截图反造原义、额外ACK或继续禁用整能力掩盖缺口。后续通过按表中实际候选和端点范围覆盖，不覆盖失败时未到达的检查点。

本轮所有原生目标按owned Job串行控制，最终均正常清理，无强制清理／残留，原资源与存档守卫真；显示需求恢复／释放结果随收据保留，未修改用户混音器或本轮无关系统设置。未以临时唤醒请求代替WGC可用校验，未新增周期轮询／跟进任务。用户原FD2.SAV、主树集成与重建摘要由最终集成收据再核，不预填未执行结果；本阶段不远程推送。

### G4 收口与 G5 首项阻塞

本阶段实质审查修复了共享页写点、水色方向、详情漏wait／眨眼、死亡完整呈现、cell非原义stage门／callback／cue12／WORD晚重验、AI物理外围刷新、hostquit续态忙循环，以及错误raw26和两处无源清键；未决通用机制没有改名为S残余。冷流程／有限执行器与原热owner合同保持，G4按完整阶段独立提交；实际提交哈希、安全集成与主程序重建由Git及最终本地集成收据记录，不预填自身提交。

**G5尚未开始，首项阻塞是跨帧输入语义审查。**本次只恢复App当前仍可见pulse的消费边界；原BIOS可以保留固定延时／滑入中更早到达的普通键，现App在新帧重建脉冲，二者尚不能称全等。G5须先核早到确认／取消对后继16c57／choice／callback、blink RNG、额外等待、一次提交与交互的影响，并据证据闭合必要缺口；不得另建第二输入owner或通用事件系统，须在现input owner按实际原caller闭合保留／清键，不能把该差异归为纯图形精度或未经审查签通用总门。随后才按统一计划复核28项、正式入口／支持门和当前Windows所有已支持历史功能，修复去冗后提交。S不自动开始。

Linux／macOS、全部15 common图原生、完整共享RNG轨迹、全像素／全音画／绝对时序、设备路由及严格运行时BSS等仍按既有边界未验；未取证的全游戏绝对完整不作承诺。当前没有需用户重选的重大架构／行为取舍。

## G5 审查与修复进行中（2026-11-10）

G4 已以 `4ada76bc09ce508639045a584c1bd25a98aceef1` 独立提交，安全 fast-forward 集成本地 master，主 Debug 重建完成；收据 `build/g4/local-integration.json` 核 11 项发布 SHA、22 份生成 payload 与用户原档不变。G5 未提交或集成，完整历史回归未完成，S 不启动。以下是分候选的真实进展，不能登记为一次统一全绿。

- 输入四项旧实现反例已实际执行；同一 input owner 的 FIFO／读一／peek／原 clear caller 修复后，`input-green-ctest-03.log` 通过。继而逐调用者修复 Scene／ANI／Town／菜单与控制器尾，清键与 INFO 返回点分开。定向 CTest 的早期构建／失败和修后通过均保留：`candidate-build-02.log` 完整构建成功，22 项直接测试曾有9项失败；其后 title／overview／detail／magic、aux、loot／prepare及progress／result分别在03／08／10／09／11收据通过，不合成同候选22项全绿。
- 实质审查修复 common HOST 误走 ERROR 边，以及正式 present 后 ERROR 被同时到达的 HOST 覆盖；新增真实前缀、重复回复拒绝、真错误优先与 finally INFO 哨兵回归。title AV 经 CDB 确认为测试传 NULL VGA，新正式 owner 路径使用真实 VGA，未给产品加虚构 NULL 默认值。
- `early-cell-native-01` 已实际 fresh pair通过：原版／SDL 两个独立 Enter 跨300 ms名义窗口，后续 END／SaveA／WAIT／Load／SaveB及全22,987B通过，源、正常退出、owned清理和显示释放均真。`f1-stage1-native-02`／`f1-tall-native-01` 同样 actual fresh pair通过；高图仅3槽资源view alias，不改EXE／热状态或开放晚关。首轮F1的600帧cap失败保留；改为每个完整modal独立连续scope后重跑，没有放宽像素、显式stop、source或清理门。
- F1 源核3完整函数1,434B／33图域，补原缩放、INT16读一与App raw-prefix64000写集。库存来源另核205函数99,318B／两owner262+28 xref／count写点，原3 helper共23,049向量保持正常密集0／40／80；C0外部反例保留，不外推任意畸形SAV。源／CPU／真实窗口的边界见通用语义文档，不互相替代。
- 六个旧事件已迁入现 common/event_resume，逐叶保留真实 ACK 和已发生前缀，移除整段 LOGIC 预提交／Scene 重放及 rollback。到场每帧 clear／wait 与134e4各真实 caller 的20 ms等待已接入有限冷图。新 `field_legacy_event` 用例对 G4 隔离头／已建库取得目标真红：合法前置后 active=0、units=14，违第一叶前应 active=1、units=13；同源绿13通过。此前资源工厂错误和缺编译旗标的失败不算目标反例。完整 caller 原码、arrival／方向来源与新 C 通过各保自身有效域，正式自然场景仍须由本轮统一回归完成。
- 最后复审取得 Scene ERROR／HOST 优先级、F1 关闭尾及共享刷新顺序的旧序真红25／27；修后四项直接 CTest28通过。完整 build28成功，**112项无窗口 CTest29在同一轮全部通过（67.46 s）**，8组局部原码与当前 C 新探针对照30通过。Python33使用项目当前3.12.10及完整依赖，运行1376项，无失败／错误、2项跳过；准备原码6负例用实际strict路径在34补跑通过，余下旧函数seed本地产物未执行。此前20／23的失败与解释保留，未把3.14 AST指纹差异改成放宽证据门。
- 完整 Windows 套件32的原版／SDL117页开场、focus、background-input实际通过，随后原生 input 因旧「敌DP0」伤害断言失败，批次停止并正常清理／释放显示，原件保留。独立原资源与既有源函数证明正常敌DP4、基础伤害20；两seed对应普通21／HP7／GAIN31、双击20+8／HP0／GAIN42。只同步 Python／Bash 结果常数，输入执行 AST不变，11项协议自测通过；原生 input37实际64.54 s通过。其余套件从该失败点续行，三组前置经原guard／summary／来源及执行 AST核对继承，行状态明确为`inherited_passed`，未重放或称作新执行。

本轮目前没有新增主树集成、远程推送或 S 绑定。完成剩余通用修复、直接验收与 source freeze 后，仍须全面回归当前 Windows 已支持历史功能、逐 C01—C28 实质签核并精简，再以 G5 完整单元提交／集成。其它平台、完整音画／共享随机流同轨等沿既定测量边界，未因此通过。


### G5 观察器修正与分段复验（2026-11-11）

以下保留同一 G5 阶段的中间收据与失败，不作为独立阶段或提交。当前产品候选为工作树 Debug App，SHA256 `2c19b70daf576228ee6aa9b7e98e9fca188bd0b64581bc5b1e17e51f583b3718`；后续这些修正只涉及验收工具，未重建或改写产品程序。G5 整体状态以本节之后的最终收口为准；本保存点仍待最后音频四场及总签核。

| 证据 | 实际结果与修正范围 | 保留的边界 |
|---|---|---|
| 默认历史套件 N50、总览 N48 | 默认 84 项登记由 61 项严格来源继承和 23 项本轮实际执行组成；总览 baseline／换行资源别名另由 N48 两组新配对替代对应门 | 不称一次 84 项全新实机；早期 32／37／43 的失败不改写 |
| post52／56／61 | 分别保留其前 23／15／10 项已完成结果；52 的合法物品 MISS 由独立证书 `item-miss-proof-54/certificate.json` 补有限动作正证 | 各旧批仍为 failed，MISS 不是命中、整段 FX／SFX 或新实机；56／59 的转职漏采与 cap 失败保原件 |
| G3 连续等待观察 58／60／62 | 先复用连续帧 backlog，再在原确认键前取同系列边界；音频专用入口另接无 PCM 的视觉 scope，正常停止和双帧绑定后才原一次 RPC／CDB 音乐状态取样。最终 62 三模块 42 项定向测试通过；61 的两次转职及 63 的复活／转职音乐真实配对通过 | 不扩大 600 帧或 30／45 秒上限，不补 ACK、不改 SAV／RNG；旧失败与新通过分开 |
| N63 与 F1 来源 64 | N63 前 11 个命令通过，含 2 个闭帧内容复核；stage2 F1 在窗口前因仅识别自然流程旧报告格式而拒绝。64 复用严格 B7 来源，核原资源生成的 15 页 ACK、18 个原件及 manual0／entry 同槽完整 2,570 B；15 项定向测试通过 | 旧 63 没有失败子报告，display 汇总 false 保留；事故收据 `f1-preflight-review-63.json` 只证明该前置失败没有进入窗口／显示 API |
| N65 与四设置观察 66 | stage2 F1 新双端通过（64.079 秒）；音频 SDL 设置被旧“后两行透明禁用”谓词误拒。66 改为四行原资源正匹配，23 项定向测试及旧 390 闭帧有限红绿复核通过 | 仅校正验收器；旧 65 整组仍 failed，未完成的 SDL 设置业务不补签 |
| N67 与读档观察 68 | N67 设置双端完整通过；活动 Load 原版通过，SDL 的 cursor-only 浏览谓词误认关闭 phase177，下一键仍早于原 Load 尾清键。68 同一采集中先实见完整 Round，再排五种 Round 相位和六种关闭边沿、核返场正资源及已有两秒稳定门；31 项定向测试与旧 370 闭帧有限复核通过 | 原唯一确认键不增加，整个 Load 仍一个 45 秒观察域，其余 15 秒确认、PCM 120 秒不变；旧 67 保持 failed。收据命令字符串的漏空格另在 `audio-load-observer-68/command-erratum.json` 更正，原收据／测试日志不动 |

本保存点已有 60 个实际完成登记及 1 项 MISS 离线补证；音频组按已完成设置双端与待验四场分开，不增加原 62 项登记分母。后续只用既有 `--scenarios active-load opening end-turn sfx-reenable` 执行受影响 Load 配对和未运行三场；设置双端须重核原判据、完整 SAV、闭合 WAV 与来源事实后才有限继承，不把旧整组失败变成通过。所有已结束窗口／子进程正常清理，显示需求释放，原资源与用户原档守卫通过；本保存点未提交、未集成、未推送，S 未启动。

## G5 通用机制统一审查与历史回归收口（2026-11-11）

**G5 的实现、实质审查、消冗和当前 Windows 所列历史回归已完成，通用完成门在已审计来源域内通过；S 未启动。** 上述「进行中」和各保存点保留其当时状态，最终结果以本节和[审计 §13](common-logic-audit.md#13-g5-通用完成门与最终验收2026-11-11)为准。完整阶段提交由 Git 记录；安全本地集成、主 Debug 重建、发布 SHA、生成 payload 和用户档守卫由 `build/g5/local-integration.json` 记录，不预填自身提交哈希。

本阶段完成了以下实质修复，沿用冷图和现有热 owner／SAV／RNG 合同：

- 输入改由唯一 owner 保留 15 项 FIFO，按原 caller 分别读一、peek 和清键；INFO 真实返回点独立处理。修复了早到确认／取消、菜单尾及 HOST／ERROR 同帧优先级，未引入第二输入状态。
- 六个旧正式事件改由已有 event_resume 逐叶推进，文本、运动和 ACK 前后的写入次序由冷图保留；移除整段预提交、Scene 重放、回滚和不再使用的等待接口。到场每帧、134e4 caller 的 20 ms 等待及共同尾按来源接通，event_log 只保留未绑定来源的诊断边界。
- F1 复现原缩放、完整 64,000 像素共享工作页、读一和关闭尾；共享刷新按真实 caller 保留 actor／water／terrain／range／raw26 的顺序。总览补齐普通跨关片段及 `-2` 换行，stage16 的私有条件绑定仍归 S。
- 库存来源、方向 caller、完整 Loaded owner、原构造未写区域和实际支持门经过复审。合法动作未因未定义图形锚而整项禁用，96 个业务单位与 32 人持久队伍容量未截断。测试替身、外部异常状态和正常来源有效域分别登记。

最终证据分账如下，未把多候选结果冒充一次全新全量：

| 类别 | 完成范围 | 保留边界 |
|---|---|---|
| SDL／工具自动测试 | 当前生产候选 A47 的 112/112 CTest 通过，77.15 秒；A33 使用 Python 3.12.10 全量运行 1,376 项，无失败／错误、2 项 skip；准备原码负例另有 strict 补测，后续观察器按各冻结候选直接测试 | 后续定向测试不相加称新一轮全量；legacy 本地 seed 的 skip 未改为通过 |
| 局部原码／原资源 | 八组有限局部原码、当前 C 探针、库存 23,049 向量、F1／方向／事件与 arrival 来源分别通过各自门；IDA 发布只补两处已证注释，11 项发布清单有效 | 不等于全部 28 模块完整 CPU、全像素／声音或全 RNG 等价；原替身和参数域保留 |
| 默认历史套件 | N84 登记 84 项：61 项严格继承、23 项新实做；总览 N48 的 baseline／换行两组 fresh pair 替换对应门 | 不是一次 84 项全 fresh；旧 32／37／43 等失败保持原件 |
| 补充历史及最后尾验 | 现后续登记 62/62：60 个既有完成登记、1 项合法 MISS 离线补证、1 个音频联合组。N63 的共享运行／四设置／详情／common39、F1 stage1／高图及服务音乐，N65 的 F1stage2 均按实际记录通过 | 含离线内容和严格继承，不与默认 84 相加称全新配对；旧 52／56／59／61／63／65／67 整体失败不改签 |
| 最终音频 N69 | 新 active-load／opening／end-turn／sfx-reenable 四场原版／SDL 八端全部通过；联合 N67 已通过的 options 两端及其 qualification、5 份 SAV／端、闭合 WAV／1,124 份事实重核，原五场十角色门通过 | 精确 2 old＋8 new，不构造十端新 GUI 报告；SFX 只签声明 sample8 的有限身份正证，绝对 OFF 静音、唯一播放实例、幅度、首 PCM 时钟及设备输出未获证明 |

N69 原件 `F:/fd2sdl-evidence/g5-audio-remainder-20261111-1115/comparison.json` 的 SHA256 为 `27a2126e602d7e8739f6fca2e0b75a8ef2a87dc11fa5917eb5b34266135a6359`，子音频报告为 `d81ab28b28dae012f904e122c6d55094cc4fb0695efecf0d4607780e1b2742b1`。parent `ok/status=passed`、62/62、末源／旧新闭帧／用户档守卫通过。outer Job29520 和子 Job4596 均正常退出，timeout／residual／forced 为 0、cleaned 为 1；显示需求原线程释放，原 balanced 电源计划未改，未改混音器，无遗留窗口。runner 的 `full_G5_signed_off=false` 保留，它只签运行门；通用总签由本节、逐 C01—C28 矩阵及 `build/g5/final-gate-review.json` 记录。

已识别的正常来源通用机制、正式消费者、必要呈现及错误／取消／一次提交边界没有剩余实施阻塞。未借禁用合法能力缩小清单，亦未把未决通用项改名为关卡残余。后续只按统一计划处理 S 的地图／编成／触发绑定／剧情顺序及真正特例，当前不启动 S。fresh 未写 9B 的全游戏读者、严格运行时 BSS、全部 FX／声音／共享 RNG 同轨、设备路由／拔插／可听、Linux／macOS，以及旧 App 的 NULL／Release 材料边界仍按审计 §13.4 单列，不承诺未取证全游戏绝对完整。

用户主档保持 22,987 B，SHA256 `b0dd9349f449d252910994d796434efa14a1149de8a4ea116de22e0ec62965d7`，mtime_ns `1794076388211866700`；集成不得暂存或覆盖该档。无需要用户重新决定的重大架构或行为取舍，不远程推送。

## S前批量读取／转换调研与计划修订（2026-11-12）

本轮在新隔离树核实master `4d7ed15` 后快进到该基线，只做调研、最小只读候选转换与计划更新。G1—G5及SD完成历史保留；S玩法实施未启动。详细来源、样例、完整复跑命令和限制见[关卡批量调研](stage-batch.md)，当时规划的S1—S4见[历史路线归档](completed-stages.md#旧优先级归档)：来源账、共享语义转换／闭合、按闭包批量接入、统一回归；不以每关／每事件为新阶段。

| 验证类别 | 实际范围与结果 | 限制 |
|---|---|---|
| 只读全量来源 | 原FDFIELD 99项／33组、43,734格；turn／lookup／action各528槽；1,899模板中有效1,887／尾12、2,322位置；有效模板缺位置0；主要分派180槽／166目标，加独立magic为208／193；106移动流2,896 B | 已解码引用错误0不证明未知CFG引用合法；完整原始报告仅本地build |
| 归属与规范化 | 1,497来源行／20族归属完整；1,171保守需求引用仍为51／468／652；研究规范化全部1,584固定槽，结果提案11（1既有／10候选），新语义转换／正式绑定均0 | 分母／历史分类不互换；result原hidden-only与现HP防护有效域未证，明确拒绝候选自动发布 |
| 原字节／生成 | 194表生成通过；stage 6表973 B／11cue／11负例通过；106移动流来源通过；common 21图／19来源控制路径／2表17 B通过 | 无局部机器码动态执行或C轨迹差分，无SDL或原版运行 |
| 工具回归 | 最终56项Python通过，无skip；另主项目同基线22项来源／契约测试通过，重复的scope不累加 | 工作树曾有1项缺原资源错误，另误写模块名；已纠正后复跑，不冒称首次全绿 |
| 支持门负例 | stage0／1及全部组的 `--require-supported` 均如预期exit1，报告保留 | 默认turn255潜在表外引用保留，不删槽求绿，不宣称原版运行后果已测 |

独立复审纠正了placement读取说明及结果复用有效域，并补研究输出不得覆盖输入审计的负例。确认审计descriptor分类滞后于G5完整WORD／default语义，列S1同步依赖；新图动态身份分支的未来资源投影上限及私有CFG未恢复列S2接入依赖。不复开G历史，不把未知偷换为纯绑定。

本轮未跑全套历史GUI、未启动原版窗口、未申请显示需求或更改电源／混音器，Linux／macOS未执行；无新关运行通过声明。主项目用户 `FD2.SAV` 为未跟踪文件，仅按路径校验保护，原资源亦只读。完整阶段独立提交和本地fast-forward集成以Git及本地 `build/research-stage-batch-20261112/local-integration.json` 记录为准，不预填自身commit hash、不远程推送。

## 当前计划分离与结果恢复补充阶段登记（2026-11-13）

本轮核实主项目master与本会话工作树均为 `bf714f3`，无后续S实现提交；主项目仅保留未跟踪的用户 `FD2.SAV`。G5最终门及其引用收据、研究集成收据及证据摘要只读核对匹配，没有重新运行游戏测试。

本轮仅修订文档：`development.md` 改为当前待办／状态／依赖／分层测试入口，B／A／R／SD／G完成摘要移至[完成归档](completed-stages.md)，长候选及失败仍只保留本页和原报告，不复制另一份流水账。当前顺序 **S0 → S1 → S2 → S3 → S4，均未开始**；历史G5／SD完成域不重开。

独立补充S0的发现依据是现有 `field_save_test.c::test_import_result_uses_shared_query` 接受HP0且visible的snapshot并按HP判胜败，`field_save.c` 导入后直接查询，及结果冷图／consumer与原`0x205b4` hidden-only谓词差异。只读核实源码／现有测试内容，未跑该测试，未做原版Load动态复现，不把静态差异扩签为自然场景运行结论。S0修正旧stage0／1结果与恢复合同；审计表达滞后仍唯一归S1，新结果族／别名和未来身份路径能力仍唯一归S2，不重复造待办。

本轮检查只含文档链接／锚点、范围和状态一致性、Git差异及主档／原资源守卫；SDL自动测试、局部机器码动态对照、原版运行和历史GUI均未执行，未改代码／原资源／支持门。提交及本地集成身份以Git和 `build/plan-readability-20261113/local-integration.json` 实际收据为准，不预填自身hash、不远程推送。

## S0 结果判定与活动恢复合同修正（2026-11-13）

本阶段从`fdad3b6`实施，只修旧stage0／1结果／活动恢复及直接输入路径，未开始S1—S4。产品、独立来源、定向验收及实质审查已闭合；完整阶段提交／本地master集成、主Debug重建和用户档守卫由Git及主项目`build/s0/local-integration.json`实际登记，不预填self hash。当前入口移至S1但不自动实施。

### 修正与独立原义

- `stage.result.000`两个HP参数、`stage.result.001`敌方HP参数禁用，按原`205b4`的visible side0／raw0 hidden和`206c5`村民raw5..10全hidden最后覆盖败北；原结果1／2正确映射SDL败北／胜利。保留count0／缺raw0、stage1不足11人的SDL安全门，不模拟原版越界。
- 原`10010`无53ECC读写，故同stage导入不再查询，跨stage资源替换显式带旧结果锁存；Continue新Game初值ongoing。删除Play每帧查询，只消费已有controller／phase实际写点。不改HP／flags／未知SAV、RNG、原owner或存档布局，不增加pending／第二热状态或整体回滚。
- 原`117e7`方向／Z／F1／F2／空格菜单早退，actor Enter动作／详情返回才经1197b查询，main随后分派。实审补齐Play F2、Enter详情和正常player初选的三处额外HP0拒绝；只改controller入口，AI／攻击／法术／物品独立提交门不变。

完整读集及合同见[运行语义§12](../reverse-engineering/common-runtime-semantics.md#12-s0-结果锁存与活动恢复合同2026-11-13)和[session边界](../systems/battle-session.md#s0-结果查询与恢复边界)。没有新确认命名或修改IDA生成证据；复用原已登记函数语义，只修消费者。

### 分层验证与最终候选

工作树候选02 App SHA256为`a0377afa8e7b076edf2873cba85d390a0fa414f2a50fcf1386644a7f9658de33`；最终主项目重建另记新二进制身份，不能冒称主项目新App做过同一轮GUI。

| 层 | 实际结果 | 结论上限 |
|---|---|---|
| 独立完整来源 | Domain只读隔离库的9完整函数5,979 B与固定strict relocated镜像一致，另8 B Load共享epilogue；`result-domain-02.json` | 原EXE／CFG／xref身份，不是原版运行；DB未保存，原资源未写 |
| 原码与真实C差分 | 最终`result-cpu-06.json`：576完整结果函数向量与当前C probe一致，另60 controller替身、3Load尾、10main控制点向量；C probe执行前后摘要守卫与负例通过 | Load只跑尾段，不是完整文件Load CPU；键盘／action／页面／奖励／phase等替身逐项列明，不证明全随机／像素 |
| SDL／codec／输入定向 | 候选02的13项适用CTest通过（6.85 s），后续只扩充并单复验field_save安全域通过。240个结果／导入组合（stage0 48、stage1 192）各3次purequery；完整80 B／snapshot不写、cache保留、RNG0、成功／跨stage／取消／失败owner；另5个正式CONTINUE dispatcher输入场景通过 | 测试TU呈现有显式替身；13含codec／生成检查，不是13场原生窗口。没有重跑G全历史 |
| 工具与静态校验 | 最终64项Python工具／静态／RPC／CDB测试通过，0skip；构建生成及`git diff --check`通过 | 含坏源／缺chunk／差分不符／探针变更、精确端口和采样拒绝边界；不是原版玩法验收 |
| 原版／SDL WGC | 下列6类、12端点实际完成；每场恢复后、Z后只读owner证明HP／flags／身份和cache0，selection ESC后才核ongoing或出口；完整测试SAV字节／mtime、原资源／源代码守卫、正常owned退出与显示释放通过 | 受控接受域，非自然产生或整关实战；胜利仅首对白资源，F2／HP0敌详情由上列dispatcher及原码另账 |

| 代表与本地产物 | 实际门 | 来源身份 |
|---|---|---|
| `native-c02-leader` | HP0可见raw0本身实际选中／ESC，结果仍ongoing | App候选02、工具02 |
| `native-c02-hostile-hp0-visible` | 可见HP0敌方仍ongoing | App候选02、工具02 |
| `native-c02-hidden-defeat-priority` | raw0 hidden覆盖敌全hidden的胜利条件，GAME OVER并返标题 | App候选02、工具02 |
| `native-c02-hidden-victory` | raw0可见、敌全hidden，首次controller返回后首胜利对白完整资源 | App候选02、工具02；不继续整段结算 |
| `native-c03-stage1-villager-priority` | 真实已证stage1 entry派生，全村民hidden覆盖胜利，败北并返标题 | App候选02、工具03；原自然entry／来源报告独立复核 |
| `native-c04-active-load-hidden-defeat` | 正式系统Load：先健康CONTINUE，隔离文件替换并YES加载；恢复／Z后cache0，selection ESC后败北 | App候选02、工具04；当前有效快照逐份完整owner／状态通过 |

前4类／stage1／活动Load分属工具02／03／04，保各报告实际来源，不拼成单轮同工具fresh6场。各SDL内存观察均由CDB/PDB独立绑定上述App身份，原端用两原码锚／合法fixup取得本轮owner，不按单位内容扫描。早期报告保留进程命令、原夹具、程序／PDB身份及`sources_unchanged`结果；完整逐文件输入摘要map为之后仅报告元数据的补充，未冒充原先报告已经写入或重跑。

集成前另核到主项目缓存仍为SDL3 3.2.12／CPM 0.40.8，候选02使用工作树默认SDL3 3.4.20；未升级主项目依赖。使用`src/build-s0-baseline`导入主项目既有3.2.12静态库，生成冷定义逐文件一致，App SHA256为`c6cb0d120a76187245848c0b40e8a640ef766df57134379ed20b8865129c1d11`。该候选的完整13项定向CTest新一轮通过（6.82 s），`result-cpu-07-baseline.json`同域576／60／3／10比较通过；`native-baseline-leader-hp0-visible`及`native-baseline-active-load-hidden-defeat`两组原版／SDL新配对通过，含输入摘要map、程序／PDB身份、SAV／源守卫、owned正常清理和显示释放。两组是上述代表的SDK兼容复验，不新增语义分母，不累加原码向量。候选02及3.2.12候选的程序／PDB分别封存于本地`build/s0/candidate02/`和`candidate-baseline-sdk/`；主缓存重建身份仍由集成收据另记。

### 保留的失败、修复与审查

- 红回归`red-field-save.log`确认旧实现跨stage Load覆盖旧cache。候选01移除提前查询后，旧stage1终局夹具因依赖每帧查询而超时；改为显式已controller提交的fixture，保留新的真实输入时点测试。候选02的旧field_turn败北夹具、详情“无目标”夹具仍分别用HP0当hidden／无目标，按独立原义改为hidden；前失败不改签。
- `native-c01-leader`只验证选择其它健康actor的旧工具路径，虽双端通过，未计入最终raw0 HP0门。最终使用c02实际选择HP0主角。
- `native-c02-active-load-hidden-defeat`失败于RPC前后guest从PM进入real mode；descriptor字段实际相同，旧guard正确拒绝。`native-c03-active-load-hidden-defeat`的16MiB dump精确为64KiB重复256次，原码锚不成立，仍失败。独立同tag DOSBox源码`Is16BitSegment/GetAddress/SaveMemoryBin`解释real／v86下16bit地址环读；未接受、清洗或复用旧dump。
- S0仅对上述两种精确坏视图，在同一最多3次预算内从零取完整新快照，每次重新过双锚／fixup／owner及HP／flags门，所有拒绝attempt落独立dump／请求／摘要。普通锚缺失、状态错误、before坏模式、真正descriptor变化、短读／全零／非精确周期仍失败。共享guard／游戏未改；新协议、debugger BREAK、写内存或通用诊断框架均未引入。c04实际取到完整有效快照，原失败保持原件。

独立源码／原码工具／输入及新reader实审后，无剩余S0确证问题或阻塞；补上C探针执行前后身份绑定，保测试-only入口和源/采样拒绝。没有逐关C逻辑、VM、新生产状态、放宽坏数据或静默跳过。复跑入口见[图形规范S0](../tools/gui-testing.md#s0-结果活动恢复定向入口)。当前只完成S0，审计表达同步仍归S1、新族／别名／未来身份投影仍归S2；严格BSS、全音画／全RNG、设备与其它平台等旧未验边界不扩签。

## S1 全量来源账审计同步与批量夹具（2026-11-13）

本阶段从已集成本地主线的 S0 `02bf7d8` 在现有工作树实施；只修改工具、对应测试和文档，不改 `src/`、`data/static/`、正式支持门及热 owner／SAV／RNG。S1 的完整实现、来源验收、实质审查和精简已闭合，独立提交／本地集成身份、主项目复验及用户档守卫以 Git 和主项目 `build/s1/local-integration.json` 为准，不预填自身提交哈希。当前入口移至 S2，本次不启动 S2—S4。

### 完整交付与来源口径

`stage_source_ledger.py` 直接复用现有严格 audit、common-scope、DAT／LE／fixup、计数流及 RLE 审计，发布固定 schema 的全量规范化输入、每来源夹具、target 去重、语义族／来源去向和异常索引。完整原字节只留 ignored `build/s1/`；App 仍从原资源及唯一 `data/static/` 冷定义读取。规范、源样例及 PowerShell／Bash 复跑命令见[来源账合同](../formats/stage-source-ledger.md)。不造 VM、第二状态或通用 CFG 翻译器。

| 独立统计域 | 本轮原来源实际值 |
|---|---|
| DAT／地图／metadata | 199 entries／3 个完整目录；FDFIELD 99 项／33 组，43,734 格／622 个事件标记格；turn／lookup／action 各 528 原槽 |
| 模板／位置 | 1,899 模板，有效 1,887／尾 12；2,322 WORD 位置，静态分为同索引模板前缀 1,887 和声明部署容量 435。后者保 ordinal／条件读取边界，不声称都实际消费 |
| 资源引用 | 33 帧包／8,256 帧的 RLE 语法、9,366 属性；34 文本目录／1,016 片段。已核 40 个 story 引用及 43,734 格至属性／lookup／action／潜在图帧引用，44979 条潜在图帧边不是运行帧数 |
| EXE | 主分派 180 槽／166 目标，加独立 magic 后 208 槽／193 目标；187 IDA owner／208 chunks，caller 与 shared tail 不造新函数 |
| 移动 | 106 指针／106 流、350 groups／1,045 actor-direction pairs。计数和 2,896 B／原跨度 union 2,894 B／payload 2,898 B；ID24 重叠与 `0x63221..0x63225` 的 4 B 未消费字节分别保存 |
| 规范化去向 | 72,470 来源／72,470 夹具；19 来源结构族／20 既有审查语义族；confirmed 69,628、excluded 2,077、rejected 450、unresolved 315，数量闭合 |

confirmed 指已核结构或 caller 规则，不能作为运行通过率。父子／函数共享／drop 子记录重叠不能相加当独占字节覆盖。新语义转换／正式绑定均为 0；已有 20 条定义引用和六个精确 callback0..4／6 caller 定义不作为新转换。未决和拒绝共 765 条均进入 `exception_index`，逐条保留绑定／语义族、反向引用、原证据和下一取证动作；同 `0x205b4` 的新来源复用 S0 已证函数，只要求新 caller／参数／资源闭合，不重复逐槽恢复或自动提权。

### caller 审计同步与历史差异

独立核原 `1aa1d`／`1b6b7`／`1b653`／`190ac` 四个完整函数 2,148 B 和两个共享尾 14 B 的固定 EXE／raw 页摘要，复用原 G 的 Domain／原码及运行证据。drop 执行与两个 collector、cell caller 分开；callback 使用完整 WORD，0..89 查精确 caller，90／`0x0100` 不低 BYTE alias；直接 drop255 的 default 与 collector255 哨兵不混合。未新增语义确认或修改 IDA／命名发布物。

2427 条原 descriptor（528 cell＋1899 drop）的规则分别为 item700、money102、callback36、collector_sentinel1548、story40、default_noop1，合法 callback36／表外0。固定来源中实际分类表达变化仅两处：未被初始地图引用的 stage0 cell12 mode2／WORD0 从 UNKNOWN 改为既有精确 action0 的 SUPPORTED；stage14 template33 mode203／WORD0 从 UNKNOWN 改为已证 default 的 UNOPENED。原 bytes／mode／param 不变，前者未开放新 caller，后者未开放 stage14。

1,171 条保守需求旧 supported／unopened／unmapped 为 51／468／652，当前 51／469／651；缺省 turn255 的450条拒绝域保留，严格支持集合仍为空、bound closed 仍为 `[0,1]`。common-scope 1,497 行及 622 binding_only／388 common_gap／450 out_of_table／1 default_noop／36 existing_supported 仍为冻结 G1 历史投影，当前 caller 路由另列，不把 388 当当前 G5 缺机制。旧报告和研究原型保留原身份；新原型输出只去掉已由 S0 闭合的旧 HP blocker，当前输入以 S1 schema 为准。

### 五层验收及审查

| 层 | 本轮实际证据／成功判据 | 未扩签范围 |
|---|---|---|
| 独立原来源 | 独立脚本不导入新 normalizer，逐条以原 DAT 切片、原 EXE strict raw／fixup／发布 IDA TSV 核 candidate07 的 72,470 条 ID／坐标／跨度／raw／SHA／引用，0 errors。DAT 细分叶跨度 union 完整覆盖 243169／3557794／120502 B；`resource-review-independent07.json` 绑定 ledger SHA `1a47357ba1f179db74bd0d99bf2096748596e6fc46c1ab1aabf30950eb3e6202` | 初始源结构及有限资源投影，不证明动态换图／身份或任意 SAV 域 |
| 静态与工具 | 最终四模块 76 项 unittest 全过，15.270 s、无 skip；包含 caller／WORD边界、collector255、default255、尾、同字节错槽、缺源／坏 offset／摘要／重复 ID／引用／schema、输出和原副本／Junction保护。candidate07 `--verify` 从原资源新生成逐份比较通过；单来源夹具与按族队列覆盖闭合 | 夹具派生不是独立语义 oracle，不称 72,470 次玩法执行 |
| 原码／轨迹 | 复核四 caller／共享尾固定读集；独立单位表9／8,528 B＋两 alias48 B、stage6表973 B＋11 cue／11负例、移动106／2,896 B通过。复用 G 已证 descriptor default／collector／cell 原义 | 本轮没有新增 CPU 动态执行或状态轨迹，不把摘要／工具测试写成新实机 |
| SDL 正式流程 | 不改 runtime；318 个受保护 `src/`／`data/static/` 跟踪文件摘要守卫，194 表生成成功、22 payload 与主项目已构建输入逐份一致 | 本轮不跑 SDL 游戏测试或重建 App，旧 S0 二进制／运行证据身份保持 |
| 原版代表 | 复用对应 G 的已证 caller／初始化与资源来源；本轮只做只读校验 | 不运行原版窗口、WGC、电源或音频 API，不重跑 G／S0 GUI；Linux／macOS 未执行 |

审查实修了固定 source ID 与同字节相邻槽可以错配、输出可能落入 `build` 内原资源副本两个保护缺口；补全 story／图帧引用、已证六 callback 精确来源、4 B 尾及原部署容量角色。候选06的同 target 证据列表曾与真实 references 共用 list，新增测试在旧代码取得真红，`red-reuse-reference-alias.log` 保留；修后 candidate07 只向证据表增加复用来源，不伪造原指针引用或 CFG。早期 candidate01 因 raw object2 BSS 不属于文件映射而正确拒绝，修为严格 reader 仅缓存初始化文件页；其余候选和旧通过只保本身身份，不改签为07。

两次独立实质审查后，无剩余 S1 实现或来源验收阻塞。315 条未决的完整 CFG／具体绑定与450条明确拒绝域是已交付的 S2 输入，不被静默忽略，也不表示 S1 未完成。文本全部 token／字形／头像、图像像素、动态资源／身份投影、新语义／绑定和原版运行仍按 S2—S4 适用门处理。


## S2 共享语义转换与能力补充（2026-11-13）

本阶段从已集成本地 master 的 S1 `35e3f26` 实施。实现、适用证据和独立实质审查已闭合；本完整阶段提交及本地集成的最终身份由 `build/s2/local-integration.json` 与 Git 登记，不以中途候选或研究转换标完成。只实施 S2，未启动 S3／S4，未改正式关卡门、用户 SAV 或原资源。

### 全量冻结域与独立原来源

新冷定义126个：common_event新增71（66 callback＋5效果结果）、stage新增55（44来源转场／helper＋11纯结果），旧图分别21／12保留。主表208槽／193不同target，去向为135转换来源槽、32旧定义引用、28既有G2原语caller域、13隔离槽。S1的315未决引用为301源图／原语可用、14隔离依赖；450拒绝域保持。三集合source-ID并集778；均非独立机制数、全游戏支持率或新关运行数。完整定义／源样例／转换规则／拒绝域只查[S2报告](s2-shared-semantics.md)，schema与字段见[来源账合同](../formats/stage-source-ledger.md#s2-转换叠加合同)。

输入沿S1 `ledger-candidate07/ledger.json` SHA `1a47357ba1f179db74bd0d99bf2096748596e6fc46c1ab1aabf30950eb3e6202`；不重写其72,470条记录和夹具。callback正式读集为103 owner／helper、9,736B，transition为80 owner、23,170B，result为20函数、1,663B，visual为4函数、494B；相互有共享函数，不能合计成独占覆盖字节。Domain／idalib完整chunk、严格镜像、fixup、实际入口与owner区别逐项保留，未知CFG无伪造边。

### 最终静态／原码／C证据

| 层／范围 | 本轮实际结果与本地收据 | 适用边界 |
|---|---|---|
| 全来源去向 | `source-disposition-final03/disposition.json` SHA `59fd72f29377e12249a7bb077338ba05d651484517e39cb2c1172f4ee6fab120`；208/315/450三集合闭合，4别名保留完整caller，复现性28项校验及S1四原文件摘要重核 | 图／原语存在不提升为正式绑定。12转场＋callback82仍隔离；后者连stage29 turn引用，所以13槽／14引用 |
| callback | `callback-candidate05/`：90槽／90target／88owner，72来源→66图/2,040node、17旧引用、callback82唯一阻塞；actual C 1,944正常＋141等待错误／HOST／wrong-ACK＋1容量＝2,086；31原条件两边均达 | 逐叶原活动80B前缀、写序／状态／roster／display／贡献、未到达后缀；新增构造区和全共享RNG沿既有原语域，不称整库constructor重验 |
| result | `result-cpu-final20.json`：30槽／18target／9族，832/832实际原码与C，124blocks/158edges全达，832例内含6个story停点 | 13纯target与5效果target分开；纯查询不代替锁存／story写序。count0／缺raw／固定索引不足防御域拒绝，不模拟原越界 |
| 转场 | `transitions-candidate18/conversion.json`：60槽/58target，45转换来源=41caller图＋4别名，另3helper；原caller161、roster14、布局40、flash4；真实C布局40过 | caller叶有明确替身；layout实际C比较96×80B及视口，不称45整关原生。342B/27原EXE布局源窗无重复编辑发射 |
| 加色／时钟／减色 | `visual-cpu-final20.json`：335＋11＝346实际C对照；`fade-source-C-final20.json`：85减色向量及完整原fade序列；`fade-shared-static-check.json`逐叶核唯一共享冷图 | 单独2条flash caller为source-only。真实时钟、BIOS相位、逐2ms捕获不由此证明；旧flag0比例fade不借此倒签 |
| 生成及工具 | `tools-source-final22.log`：58项来源／转换／schema工具测试通过；`tools-native-final31.log`：93项原生工具／旧S0桥／WGC／CLI前置合同纯测全过；`existing-common-source-final20.json`仅继续签原21图，新图明确交S2专属门 | CLI/生成器不是自身原义oracle；默认turn255严格域仍拒绝。Linux命令保兼容、未执行 |
| SDL直接消费 | `ctest-final20.log`：12项直接影响Ctests全过；`ctest-transition-format-final.log`补最终source探针，`ctest-failure-direct.log`另验直接受影响的failure owner路径，共13项不同Ctests。真实Game／Scene／Play、15类busy owner、未来身份／map／容量、短等待、HOST／错误前缀、结果／存档／loot／post路径 | 无窗口与原生分账；只测本阶段及直接影响，不将G全历史重新跑成S2 |
| IDA发布 | `ida-publication-final20/`：495来源名验证、17新语义名／133注释、1,312原函数边界；21/21静态、11个SHA清单全过，生成器最后刷新 | 无字节／猜测类型／函数拆分；静态fixup通过不等于严格运行时BSS或全游戏验收 |

证据目录均相对本地 `build/s2/`，原始dump／资源／完整转储不提交或分发。canonical摘要和命名产物由生成器重发，不手改生成文件。

### 原生代表：当前候选逐项登记

Windows native使用SDL3.2.12，已封存 `native-candidate20/` EXE／PDB；仅机制夹具，不分发。输入为owned PID/HWND后台投递、WGC单窗口；原资源与完整用户档守卫、串行窗口清理及同线程显示需求释放分别登记。旧G专属电源计划权限未扩用，本轮没有修改电源计划或混音器。

| 代表与入口 | 最终收据／状态 | 实际门与未扩签 |
|---|---|---|
| result29 `20bf5` | `native-result-final21/comparison.json` 双端passed（最终numeric工具） | 原真实CONTINUE／controller查询与SDL显式common源图，query前0／story入口0／ACK后2，完整受控SAV不变；不是stage29正式dispatcher开放 |
| callback70 `35e5b` | `native-callback-final32/comparison.json` 双端passed | 三段全白、18页ACK、AI写前缀、END至turn2、UI Save完整容器及cache byte2独立差异，未知字节保持；空group夹具不证明自然stage22或完整构造／RNG |
| ticks callback47 `35468` | `native-ticks-final31/comparison.json` 双端passed | 原stage20真实特殊格→190ac→WORD47，SDL显式源图；四角有序／稳定跨度与active前缀已验、相机／焦点、caller acted后缀分账 |
| victory2 `230f2/233c6` | `native-layout-final22/comparison.json` 双端passed | 原CONTINUE→真实胜利2，SDL显式Scene；布局／原资源12页对白已验、黑界／恢复、招募及stage3静态标记。原活动owner释放后禁止再读它 |

所有夹具修改逐byte登记：原记录复制、身份／位置、raw数、descriptor或metadata边界均明确是受控注入，不称自然产生。WGC只核声明反馈和轨迹，完整共享RNG、声音、所有中间像素、设备和其它平台仍未验。终态Source-only Game写stage3后保旧资源，夹具不再运行新controller；S3须用现App／Progress真正交接。

### 失败、审查与去冗

- early callback01／02／03保留：原探针假count80再append越96，及Python ID／TEXT／direction映射错误造成的假阳性被真实C拒绝；修成真实源最小前缀并独立ABI断言，030／075冷图按原字段／顺序修正。04与最终05图／原轨迹逐项相等，最终签的是05探针身份。
- direct12 的私有Scene测试缺新API宏重命名导致链接重复；unified投影漏旧move核心域、源初始化错误期待无匹配group0新增人数，及新source story误套旧stage门均已修。12旧失败／14一项夹具失败不改签；final20十二项全过。probe漏声明头导致`0xC0000005`另修测试头，candidate18真实C40再过，不拿旧纯CTest替代。
- `native-result-c11-01` 原端过、SDL因wrapper `wait_log`参数漏process失败，正常清理；修后C11-02过，产品最终20又双端实做。callback wrapper缺`capture_defeat`在开跑前由实际CLI bootstrap回归修复。
- `native-callback-final20` 原端到story6但只有两段全白，旧33ms主动丢15个新WGC帧；扫描混合帧不可拼接为第三白。独立源确认两个直接call和一个明确JMP共享尾合成三次35b78，工具仅将本代表改16ms、保原3段／252门；final21双端完整通过。`callback-wgc-missed-frame-review.json`保逐帧身份和不可恢复丢帧限制。
- `native-ticks-final20` 首DS为已见16bit protected小段、无dump即拒；final21补门后又因错误以DS.limit约束EBP而拒，DS并非SS，已移除这个取证假设。`native-layout-final20` 原端布局／fade（含两张全黑）／12页均过，末snapshot动态`SELINFO DS`在beforeEV后切换selector，严格后descriptor guard正确拒。失败RAM不解码、不重用；本轮集中修S2数字selector绑定及有界就绪，完整原S0 dump/双锚/fixup/owner规则不豁免。
- 根与两次独立复审闭合真实caller／target、位宽、写序、owner和失败前缀。预检不另造Game／RNG，固定三类Scene冷helper复用原栈／预算；common旧重复walker已删。整图去重／4alias／共享fade及格式压缩完成；8图的32个有源条件尾node是有限条件树表达，不为少量静态尾另造DAG编译器。13 uncertain站点的具体child／finalize域独立证据闭合，不外推任意未知child。

已识别旧flag0 Scene 17级比例fade与原64/65级减法的差异，唯一归属总计划S4，原G完成范围不重开。S2新来源域用精确唯一helper，不借未来S4掩盖本轮必需门。隔离源只阻塞后续依赖它们的批次，不将它们标成已实现或删255求绿。


最终 ticks31 原版前三角连续观测跨度316.82／300.14／316.81ms，末角2帧16.68ms；SDL对应416.86／416.85／450.21／3518.27ms。末角不要求200ms，仍由原版acted后缀、SDL done日志／event inactive独立证明最后等待已返回。原版使用3个新鲜短WGC会话（上限4，最后角由主流已有两帧覆盖）；SDL用完整primary16ms原表，未构造辅助会话。所有新帧按真实SystemRelativeTime／QPC核验，完整raw合并、相同stamp去重／冲突拒绝、非匹配断run及2帧／前三200ms门不变。没有补帧、强制重绘、延长游戏等待或默认通过。

取证修订历史按实际候选保留，不从失败日志改绿：ticks20／21／22／24为前置位宽、错误DS栈假设或accessed分类的严拒，ticks23／25为连续帧不足；26初始只查地形过弱，首次selection输入没有业务响应，加入完整focus／单次预定Z资源前置后才再跑，未在超时后补键。27在源帧后200ms才启动又耗280ms冷启动，错过平台；改即时启动但接受时间门不变。28原端完整过、SDL因无用aux启动晚失败；29前三角过但末角静态单帧，按原门扩到最后角按需补采；30原端完整过、SDL同样因无用aux失败。最终31原端静态BIOS等待用按需真实补采，SDL生产等待循环每16ms present，故其完整primary直接进同一门，不构造无用aux。旧28／30的SDL主流离线可通过不等于旧整轮通过，也未当作31的新实做。相关 observer、numeric、输入前置均经独立正负测试和代码复审，不增加产品热状态。

最终四份双端收据分别为 result21、callback32、layout22、ticks31，全部 `ok=true`／`sources_unchanged=true`，owned Job为0、无强制清理或残留；同线程display需求释放真，电源计划未变。它们使用同一 `native-candidate20` 的对应EXE／PDB，各工具的实际SHA和轮次保留；不是一次历史全套fresh。产品源冻结后只发生测试头修复、JSON等义排版和取证工具修订，未借新工具改签旧失败。工作树字节SHA与 Git规范换行后主项目的新来源身份分列；集成只证明同Git内容／冷定义语义及新构建，不伪称主项目新二进制又重跑了四组窗口。

S2冻结域无剩余实现或必要证据阻塞。13来源槽／14引用仍是有源隔离依赖，不属于已运行绑定；默认turn255拒绝域仍非零，当前正式支持门未扩大。开发入口移至S3（未开始），本轮不实施它。完成提交与集成收据、实际必要重建／直接检查、原资源和用户 `FD2.SAV` 的51文件守卫统一写入 `build/s2/local-integration.json`，不远程推送或发布。


收口补验：callback32 只因旧host缺少持久化输入摘要而重取，业务／像素／输入／存档判据不变；现 `input_sha256` 与 `product_source_paths` 保留实际freeze及动态追加config／隔离只读资源。既有CLI纯测新增实际SHA／路径／追加项断言，6项通过；旧callback21通过留其原身份，不回填旧值。最终签核改用callback32。`final-evidence.json` 列四对最终报告与封存二进制摘要。

`qualification/qualification.json` 单独核旧轮次工具资格：result21／ticks31／layout22的全部产品源码与当前逐字节相等；G4 host 仅新增摘要输出，删除该单一AST语句后与按旧SHA精确重建的源码一致。layout22的S2桥随后仅将descriptor前态比较屏蔽accessed bit，精确重建旧源码匹配该轮SHA，5个不同实际采样（报告重复嵌入为11条）在旧／新谓词结果完全相同；其余dumper和owner守卫未变。这是离线源影响资格，不是新GUI或将旧失败改绿。最终报告保留每轮原工具SHA，不声称全部工具文件字节相同。

## S3 按依赖闭包批量接入正式流程（2026-11-14）

从已集成S2 `54ebc72` 在同一工作树实施，完整阶段／一次独立提交；没有启动S4。批次为stage2—10、11—19战斗／END／来源事件和胜利交接，附stage20仅入场／浏览／存取；13隔离槽／14引用、450默认255拒绝保留。详细绑定、源修正和五层边界见[S3报告](s3-batch-integration.md)，当前状态只查总计划。

### 源闭包、共享补充与最终逻辑候选

- 正式来源绑定96／86不同目标，19 entry、18 victory、18 result、41 callback；这是新增或替换的绑定数，不是机制数。原资源读取不复制；source capability的slots0由DAT metadata实际读取部署数。18个胜利出口与下一个3..20准备行／entry闭合；20战斗因victory20私有生命期隔离，25不跨未知自然链单开。
- 完整原caller复核修callback41共享尾文本4；原child1文本11图保其旧域。五个结果CALL续点、12／16／18条件raw有效域、带故事latch与event_resume、AI奖励尾查询、状态notice、父子奖励owner和checked查询安全拒绝一起集中闭合，不写逐stage C故事链。
- 原1b41d闭合stage16总览持久text18分支，新field_info冷表／共同roster查询；Domain生成名称35346与1b41d，1,312函数边界／3段原bytes不变，21 LE＋14发布＋11摘要门通过。初次发布13／14因误把函数内25e0b作owner拒绝，修成main25bf4内片段后完整发布，旧失败留本地。
- 候选09产品、PDB、161生产来源／23实际生成inc及探针封存于 `native-candidate09/manifest.json`。实际通过system package导入主项目SDL3 3.2.12，未使用工作树回退CPM／SDL版本；主项目CPM0.40.8不升级。23项直接影响CTest同轮全过（16.92秒），未跑全历史CTest／GUI。
- 同候选09收据：`oracles-candidate09`916结果／27 caller；`callback-binding-source04`180原caller／192 C；`callback-candidate07`1,971正常＋144等待＋1容量；`binding-traces-final09`38绑定／36图、1,026原码／C正常＋90等待＋1容量，另3既有模板caller单签；`overview-cpu-C-final09`242。各域有重叠、原码有明确叶替身，不能相加当完整AI／所有运行状态。
- 来源闭包 `release-scope-candidate03` 保55条callback metadata引用（战斗50、仅entry20的5条未绑定）、新战斗254及entry20的11条255拒绝；全33组450＝406战场＋44资源域，严格支持集合仍空。旧S1／S2账与失败不重写。

### 保留的失败与修正

候选02新存档测试漏声明使指针截断，补正确头；候选03旧stage3禁用expected按新源绑定独立校准；后续候选发现合法Load晚期缺raw曾被旧安全谓词解释死亡／旧enum接口abort，改为真实读点checked拒绝，未提高Load人数门。奖励返回值、状态已消费notice和nonplayer child的实际C红件均修后才封09。旧候选和旧probe SHA保留，不能用早期CPU通过替最终09。

首组 `native-turn3-final09` 已到真实CONTINUE，但END前构造ProgressReader时，误用首关专用奖励reader的六portrait／stage0资源门失败；无奖励页运行证据，owned正常清理／无残留、来源守卫不变。工具改为S3显式stage2资源域，按真实active与FDFIELD[7]读取0／2／4／8／9／30／51，37页保完整portrait／文字／growth／箭头门；B5默认不变，preflight提前构造。14项S3纯测和11项既有奖励／stage1协议通过后，以新目录r2重新整对取证，未改产品／受控夹具或给旧失败改绿。

根与独立复审另发现新Play等待恢复必须有正式App失败出口门：S2 result21机制夹具不是正式App，stage12单独post0也不证明故事ACK前不离场。因此在本次stage12代表追加受控活动Load／真实controller／story10／原实际败北出口，不能靠旧机制门豁免。

### 原生验收与本地集成

早期候选时仍在串行原版／SDL正式WGC验收，未标完整S3完成、未提交／集成。报告按每轮实际输入／工具／候选保身份，最终必需门齐全后在本节补收据；不预签原版或将多候选累计成同轮通过。S4的旧Scene比例fade与全历史回归保持未开始。

候选10收口补记：仅修main标题CONTINUE按已发布capability判断（旧0／1／2硬编码），产品EXE为eb682106…0b02c；23项定向CTest同轮通过（10.86秒）。四个原码C probe／全部生成inc及其源保持09身份；`turn3-candidate10-qualification.json`精确还原09 main并核stage2门同值，第一组仅按来源影响继承，不称10新GUI。`field_prepare`另单独补跑通过，复核本次后期roster部署直接路径，不重复全历史。

`native-turn3-final09-r2`原版／SDL两端通过（57／53条trace、52／51检查点），Job3524正常退出／无残留／无强制清理；`native-victory-final10`两端各两个owned进程通过（88／77条trace、69／65检查点），Job28208同样正常清理。两份完整源／用户档守卫为真，同线程显示释放为真；每轮原工具身份保留，53份旧导入工具源另封存`native-tools-r2/`，不改写失败或假称所有轮次工具相同。

`native-stage12-final10`在原端phase0 fullbrowse等待时，根审查发现新增败北工具的QUERY常量没有实际caller／actor尾，旧mock也复用该不完整常量，自证失效。经核本轮owned DOSBox PID31396的可执行路径／独立conf后正常WM_CLOSE中止，报告保failed／目标提前exit0；Job38408残留0／强制0，source守卫真、display释放真。没有把开场七页通过记成该轮正式entry通过。

离线同时定位stage12 fullbrowse预测错误：Python直接抄15条roster，漏完整1088d的stage<13保留active6/text2门；本实际late记录槽6为unit58/text8，原应清零80B并hidden，roster／placement不推进。正式field_prepare已有正确冷gate，产品不用改。此场只消费14人，末text13不出场；旧工具虚构其(6,18)故事移焦而推camY12。按完整部署、对话和原12cea重算自然得到camera0,11／focus3,15，原PNG的原有完整focus498px门通过（bad0、maxRGBdelta3），独立terrain扫描亦吻合；没有改坐标常量迎合截图，也没有引入玩家nearest-free。日志门另补真实整行、早查询／出口、wrongcaller／actor／result、重复／缺失拒绝；旧mock红件与修后纯测分账。当时安排新目录r2重新完整双端验收。


### 最终原生门、审查与阶段收口

stage12工具另两处owner／观察边界修正均只在研究工具：观察view补本次手动Load的完整32条roster区域，不能以旧B7六人owner处理未部署说话人；r2在完整GAME OVER／自动title后仍因把三条capture_error计成补键失败，原报告15449f2c…108e0保failed。final07改为末ACK后仅拒key／delivery，原观察trace保留，真实ENTER／ESC／delivery-only负例先红4后绿，不提前记录零输入的title通过。20项对应纯测及最终121项直接影响工具测试通过。

`native-stage12-final10-r3`原端完整通过，SDL亦过phase0正式entry／Save／活动Load，但phase1 CONTINUE由main另一处0／1／2执行白名单拒绝；这是本轮产品漏点，不是环境。C11将执行门与显示门统一已有capability；完整main→run_field_chain→Play→open_resources→field_save复审无其它公共旧白名单。stage0 START／调试和stage1／2专名兼容wrapper的合法约束不放宽，stage20仍可恢复浏览而不可战斗。

C12仅在C11上同步三文件旧范围注释（精确回替、行数不变），完整24项直接影响CTest同轮通过10.79秒；3个变化探针的新实际C差分重新完成：916结果、27caller、180原／192 C caller、1,971＋144＋1 callback和1,026＋90＋1正式绑定。overview探针与09原SHA完全相同，242项严格继承；scope03的51输入仅field_game.c注释不同、其余50相同，按精确来源影响继承，不称新结构运行。旧原件仍原身份。

最终 `native-stage12-final12` 新双端完整通过：各2个owned进程，原／SDL分别89／72条trace及69／65检查点，story10各1 ACK；SDL在末ACK后恰有一次完整player/raw0/result2查询返回及一次Play败北行，之前均无query／exit。原版完整源合同、WGC和SAV证明等待及出口，但没有转储runtime latch。trace含无按键的采集观察，不将条数当真实按键数。正式CONTINUE、活动Load取消／成功／再存、受控hidden村民、真实选择取消、完整故事、64000像素末GAME OVER及无补键自动回标题齐全；不称自然战斗产生。

三组最终代表为turn3-r2（C09）、victory（C10）和stage12（C12）。前两组只在精确还原main、stage2两能力谓词同值及其余注释／不变逻辑的范围继承；不是C12重跑前三组或主项目新二进制又跑GUI。后者是C12新原生。全部owned正常退出／Job残留0／强制0、来源守卫真、同线程显示释放真，无电源计划或用户混音器变更。`final-gate.json`核三个原报告、当前161生产源／23inc／探针及51原资源／用户档，所有摘要与mtime守卫通过。

根与独立实审闭合本轮来源／分类、参数／宽度、结果调用／资源／owner／SAV／RNG、正式门和取证工具问题，无剩余S3阻塞；13隔离槽／14引用及450默认255域仍未开放，不因本阶段完成而抹去。下一入口S4未开始，旧Scene fade及统一历史回归不在本次实施。完整S3为一次独立提交；本地主项目FF、同版SDK重建／定向检查、来源资格与用户档守卫由后置 `build/s3/local-integration.json`登记，Git为提交身份，不远程push。

## S4 统一审查、全历史回归与精简（2026-11-14）

从 S3 `ad5a63e` 在原 elegant-pasteur 工作树连续实施，仅 S4；未恢复旧工作树、未启动后续功能。支持域沿 S3 冻结：stage0／1 原域，stage2—10／11—19 战斗批次，stage20 仅入场／浏览／存取。96 来源绑定／86 目标不是运行覆盖率；13 隔离槽／14 引用与 450 默认 turn255 拒绝保留。

### 实现与来源

- 旧 Scene 的17级比例 fade、App／准备页重复循环统一到唯一原减法冷子图，65／64级、各级尾2ms。普通 void／clamp brightness 委托同一原语，独立真插值保留；没有第二热状态或通用VM。
- 完整 `1f894/1f73f/1f81e/25ebb` 补标题确认尾、START分派、cutaway的真实fade；标题图前改为原 `11df2+64` 全白，不再误作置黑。`1f81e`尾为fall-through到1f882，不由附近CALL猜流程。
- 完整24618证明既有Game调试转场尾应500ms后32级加白／步2／尾4ms；只迁移该尾，完整page/mask/ABI隔离仍不解除。Domain隔离发布只修证据，1312函数名称／边界／chunk／signature与段bytes不变，LE21／发布14项通过，未重签全运行时BSS。
- G4旧六绑定门补严格S3 source join：固定S2去向→已证callback41细化→原fixup/真实DAT caller/cap→唯一冷定义，保旧21图／19路／六caller原义。44额外alias在此工具只签来源，不能靠同时改scope、alias和inputsha伪造通过。方向源工具仅放精确stage2单跳alias，reset/20ms不降。

### 最终候选与实际门

产品始终为 C01，EXE SHA `993470f3e483c5ab238a4c0fa5eba129f1fd12ef4ccb86e46bb0ea77f23e2a19`；MSVC14.52、SDL3.2.12静态system package，实际F盘普通构建目录已核FileTracker/tlog。161生产源／23实际生成inc封存，不混用旧build-win。后续仅修测试／工具，不累计不同产品候选。

| 层 | 实际最终收据／范围 |
|---|---|
| 来源／静态 | 33组、208槽／193目标、106移动流、72470源对象与S1冻结相同；fresh scope96／86、13槽14ref、450／265拒绝；196表，211图／5112节点／112全部alias含历史，未新增玩法绑定 |
| Python工具 | `source-final02/python312-full-final02-receipt.json`：3.12.10固定依赖，单次1661项／0skip／1098.796s，682工具／数据／src含tests文件前后不变；早期1655／1659各保原工具身份，不加总 |
| 当前原码／C | `source-final01`新执行结果916、受控caller27、overview242、prep47、callback来源180／C192、callback1971+144等待+1容量、绑定1026+90等待+1容量、原六callerCPU；有替身／切片域，不称全AI／共享RNG／原生 |
| 调色原码／C | `fade-scene-final02.json`：85减法、68兼容setter、51实际Scene dispatcher／fast／转场轨迹，逐DAC／尾等待／owner对照；HOST/error及不执行后缀独立C门，非物理时钟 |
| SDL全注册回归 | `ctest02`单次120/120、114.12s、0skip、前后来源守卫真，真实input wait／attack／lethal三场、正常owned Job及显示释放；含Python／逻辑注册项，不称120项WGC |
| 历史有限资格 | `history-audit/actual-finite-qualification03.json`闭合30组／146登记的有限子门；604旧source事实桥接，602整体匹配含全部产品／静态源，652函数token／Game275及旧冷图独立核。两G3旧tool身份不足及B7变化门由新pair替换；不是146场全fresh |

六个新原版／SDL正式代表：自然START117页；标题LOAD／四槽／Town／旧entry1；stage12受控entry／取消／存取及另CONTINUE／story10败北→GAMEOVER／标题；G3装备重选和不同物品Give取消两场；B7受控胜利至stage2的30结算页＋15入场页／启用菜单／活动存档。详见[S4报告](s4-unified-review.md)的实际检查点／身份，不逐关实机重验。全部sources／原资源／用户档／owned正常清理及同线程display释放通过，不改混音器、不变电源计划。

额外调试CLI加白观察 `native-effect-observer03` 通过：49张原WGC完整序列在strict stop后重核，实际两个逻辑VGA页的64000像素逐通道不降，60860像素至少一通道上升、3140像素饱和保持，minRGB97→162；采集helper stop／0、CLI自然exit0。只签实际加亮反馈，不声称见到末4ms／add62或32级均有物理帧；它不是旧84／62遗漏门，也不替原版整转场ABI门。

### 失败原件与修复资格

- 初次main资源测试及全CTest `ctest01` 的116／120失败来自旧stage2／3未发布负例、全图预检和整Game回滚断言。负例改entry-only20及未发布21，HP0动作域不改；源entry2按完整32e8c及原DAT验证每叶／已到PAN前缀。`history-fixture-fix01`又暴露fast PAN原本会呈现1帧，修正精确门后`fix02`通过；不编造旧8段GUI故事（8日志原为4预检+4渲染）。
- 较早Python环境／读集／依赖及旧六alias拒绝的红项留 `source-audit01`；使用3.12证据解释器、补只读输入并修严格来源工具。初审scope因stage／Game变化真实拒绝，`release-scope-drift.json`保留；共同篡改错图负例推动source join补严，最终完整单套另跑，不把早期全绿改签。
- stage12首次F盘输出被现工具build/保护门拒绝，未开窗口；`stage12-01-wrapper`为false，改合法新目录02后整pair通过。
- B7 `native-b7-entry2-01`原端全过，SDL30／15页和browse过，旧工具仍要求March／END禁用而超时。保截图／输入／false；真FDOTHER完整资源正反例11项后，两端严格启用（只Overview默认项允许blink），新02整pair通过。原失败不补签，五个此前新pair的工具closure通过精确函数外不变且未调用该predicate的资格桥接。
- 额外observer01误将960×600物理缩放像素当原调色表，02标准整幅normalize后又因批量／二次解码积压81ms错过strictstop。两个false和raw不改；离线分析只解释原因，不当新runtime。03复用既有latest归一化帧实时门、stop后全量核raw，160门及像素规则未降，无CDB／hold／补帧／target-exit错误重签。

根及独立实审闭合本阶段源码／来源账／冷热／owner／SAV／RNG／资源生命周期与错误前缀问题，已消除旧近似、重复数据算法和过期测试门；无剩余S4必修或验收阻塞。完整S4一次独立提交，不远程push／发布。主项目FF／同SDK必要重建／直接影响复验／用户档及51文件守卫由后置 `build/s4/local-integration.json`登记；不称集成二进制又跑了全部原版。当前没有自动启动的后续阶段，未验精度／平台及隔离域仍保原边界。


## S5 隔离语义闭合与后续批量接入（2026-11-14）

基线S4 `0cca15f`；继续同一`cindy/elegant-pasteur`工作树。阶段一次独立提交并集成本地master，不远程push／发布、不自动启动后续阶段。完整交付、适用范围和命令见[S5报告](s5-batch-expansion.md)；阶段内候选／原HP疑点与旧计数见[研究历史](s5-investigation-history.md)。本记录不重开G／SD或S0—S4已完成范围。

交付stage20—25普通战斗／END／事件／结算准备交接及26—29分支／终章两批。原13槽／14引用闭合，source10为141完整owner、20图／17002节点；stage2—29正式163绑定／149目标／112callback来源引用。315原未决均已有源去向，450默认callback255拒绝保留；这些身份／引用数字不是独立机制数或可运行覆盖率。resource30仅内部31active尾声，stage31／32开场另账。

主要实现与修复：有限Scene转场／传送／临时页／credits／montage共用原owner及唯一冷图；RNG、分支、循环／等待／取消／marker及写序仍在数据。修callback51旧story11误归属为真实story3，保旧图和报告；热stage变化后借实际文本owner，不重开错误FDTXT；Game失败前缀及teleport ordinary cursor/display保持；NoTown统一既有flags并恢复原清屏后的normal DAC，未扩展普通城镇表。真实Play曾把source2000ms等待投影成旧common最多1000ms而退出，改为source自身0..65535校验，保其他资源门；薄binder让正式Play和快慢测试绑定同一回调。真实ANI0／2无decoder缺失，不补op3／padding。

原蒙太奇HP不确定性已由完整自然caller消除：`3771c push esi`在`P−112`留下`0xAF8C0`，原DWORD与marker低WORD63680各保宽度；旧240poison向量不改签自然caller。20对次序／每part／tick／两侧RNG／HP／提前返回均在六固定子图中，最终59没有正常返回边。原PC驱动有限C叶40组、3596命令／2779完整64000B索引帧逐byte相等；与实际Scene全图（宿主I/O替身）及原生首末观察分账，不扩签全RGB／声音／物理帧。

最终检查：`candidate-final-C22-ctest.log`单次18／18适用CTest、41.34s，其中`s5_source_tools`124项、0skip。额外8门及旧红／修绿各留原候选，不与最终计数加总。HOST六场移到真实commit后、pre-wipe的私有Scene节点，保成功／ERROR／HOST×display0／1、缺cursor、Game整byte／RNG／队列／scope owner；旧delay资源钩子注入已移除，不声称两像素前缀相同。删两临时诊断fprintf，保返回／flags和原失败日志，strict单行PASS／空stderr不放宽。

来源最终入口`build/s5/source-final-C22-index02.json`：fresh module8／408、82双TU+CMake/target、正式参数1890正常＋147等待负例＋1容量、scope／disposition／G4／G5及资源门全过；C21独立Game三矩阵36／C39、180／C192、1998／147＋1按声明输入全等继承。7报告235路径／225实际文件，合资源门229current文件及5探针全SHA复核；1873引用／154archive entry、3sheet／232part与80FIGANI／630part分账。source10生成前快照保历史，其逐源／图／ABI由当前门重新连接，不改SHA求绿。

正式原生与有限资格：

| 身份 | 实际通过范围 | 严格边界 |
|---|---|---|
| C17 stage21／24原双端 | stage21入场／真实取消／活动Save/Load及受控END；stage24 NoTown选择／quake／Save | 旧UI标题两项／首次Load禁用和24初始NoTown RGB不签当前；21早期两份复用临时SAV已被最后测量替代，不签现读复核，最后Load与turn2原件仍同SHA |
| C18 stage20双端 | 原count前缀6物品→真实结果／sheet101→追加24,23→Town21→四手工槽 | 受控；6有序anchor不降门，非101物理帧全捕 |
| C19 stage26good双端 | 真实结果→27 NoTown／选19／强制text9→24618入场／34active／活动Save | 各79业务检查点；总88／83另含WGC瞬态。最终有效域经1446输入／成功路径资格承接，不称C22重跑 |
| C20 original29＋C21 SDL29 | 分别完整CONTINUE→胜利→31文本／通知→credits首／末卡→montage0／19→完整59保持／HOST | C20整pair报告仍false，仅original case true；C21为SDL-only true。C22只删错误日志资格承接，不拼成同候选一次fresh pair |
| C22 stage26bad SDL-only | 正式输入→坏臂story／FDTXT27七通知→共享credits／montage／终态 | 原版共享ending代表与完整250cc原码／写集另证；不冒签坏臂新原版 |
| C22 entry29原双端 | 标题LOAD／NoTown真实选19／script87／29页ACK／七传送／脉冲／27active／整活动Save | 各61业务检查点；4＋3段WGC每caller一原LMI正证，全部写序由原码／C另证，非全相位。ff5200为turn255／phase0，未造“自然82” |

均用WGC HWND／owned PID后台输入与串行正常清理，原源／用户档51文件大小、mtime、SHA守卫真，显示同线程释放，未改电源计划／用户混音器。当前Windows通过，Linux／macOS未执行。原版32通知快照门只执行原glyph／portrait／UI／scroll／memmove／memset，开闭页／输入／BIOS等替身明确；不改签WGC。

失败原件均保留：21标题／Load错域、20首hidden物品位置及35号anchor采样失败；26白DAC；29两轮notice遮挡／箭头与fixed95滚行模型、蒙太奇bool→dict协议错误；C20 SDL source等待错接旧界限及C21旧HOST测试seam红。原数据／不同候选结果未累加，工具修正未补按键或放低像素门。

人数边界另证`credits-roster-bound04/receipt.json`：正常一次性招募ID0..31中18／19互斥且不删其记录，上界31；原1088d仅初始化31active，外来count32会令31c49读未初始化raw31，当前明确拒绝，不造原写点。SAV32容量不改，不签外来／修改SAV或人为重复callback。450拒绝、严格BSS／全共享RNG／完整音画／绝对时序等原边界继续保留。

根及独立实审已修复本阶段问题、消除错误预检／重复真值和临时诊断，无剩余S5必修或验收阻塞。准确提交、本地主线快进、必要重建／集成复验和证据转移最终见`build/s5/local-integration.json`；不声称集成后又跑全部原版。该交付时点没有已授权后续阶段。

## 完全对齐审查与补完计划（2026-11-14）

本会话隔离树`cindy/fierce-newton`初始为88b47d7，核主项目master为495e7d9后无冲突快进。S5完成及本地集成收据与真实源码／绑定交叉核验，原13槽／14引用已闭合，不重开stage20—29或旧HP／NoTown问题。新交付为[差异审查与A01—A22矩阵](full-alignment-audit.md)及[统一F1—F6路线](development.md)，只审查／取证／文档，不启动补完实现。

重新生成72,470行来源账，校验196表和8个flow域；Domain／idalib对既有数据库隔离副本只读核main启动RNG推进，源库与副本摘要不变。原目录11DAT合994顶层entry、B24独立；51个原资源／用户档守卫、IDA发布11文件、S5八份F盘原生报告摘要匹配。旧elegant-pasteur部分路径缺失，保留副本与主项目重建二进制分账，不假称旧App原件仍在。

SDL主项目已有Debug的6项无窗口组件通过，未重建本树、未跑新GUI或原版CPU／实机。IDA产物14项通过、927源码地址token均canonical。来源单测首次因本树无original_game而setUp失败；主项目重跑17项中16通过、1个stage23未绑定旧断言失败，纳入A01，未通过改expected掩盖。静态／继承／推断和严格BSS未过的资格均保留。

用户本轮明确选择Windows最终验收，继续保留Nuked声音并记为差异。终态除`EX-AUDIO-NUKED`外不静默沿用旧精度豁免；最终须逐差异实现和证据闭合，未知语义／必需缺证不能过门。本轮完整文档独立提交并本地集成，实际提交／证据索引和保护核验由主项目`build/full-alignment-audit-20261114/review-receipt.json`登记；不远程推送。

## F1 实施检查点（2026-11-15，未完成）

用户随后授权在同一`cindy/fierce-newton`工作树连续完成F1—F6。当前仍以`39422ae`为HEAD，F1尚未满足完整阶段提交门，未提交／本地集成、未开始F2；不将本检查点列为完成阶段。

已修37整流预检及66纯SET／前46组有效前缀、82的inactive29／30 xy-only容量门，并剔除75提前拒绝的误判。9项直接CTest通过；12个原码／实际C向量中8例完整7,680B等值，4例保留既存F3构造九B差异。82 fresh03重跑8状态／408前缀；正式pair04两端完成CONTINUE／Load取消确认／两次END／story4逐页ACK／三传送及完整Save，所有来源／owned／显示释放检查通过。旧失败不回签。

来源09当前69,935／450／2,085／0，保历史69,901／450／2,077／42；预算08无未知回边。新A20正式工具startup01／Domain六窗／有源layered门以独立66B写账全匹配293,547B及7,959记录／7,948唯一fixup source、零豁免；旧L1静态零模型仍FAIL。当前F1工具CTest实际84项通过；这些与原版／C／GUI各自分账，不称全历史或全游戏通过。

真实剩余为defeat全部8 caller的活跃寄存器／回读与正常32+屏内域。原31临时页alias及状态caller32..35有界执行不能普判U，也不能补签其余caller。该内部原码复审遭工具内容检查中止，已向主对话报告；没有重试被拦截执行或降低完成门。A20整理中一次上游502和Domain wrapper路径错误均恢复，已取得新正式工具证据，不再列为阻塞。原资源／用户档只读，全部原码／RAM／媒体保留本地，不推送／发布。

全部当前身份、失败保留、未验边界与收口门统一见[F1证据报告](f1-reference-domain.md)，顺序仍是[总计划F1→F6](development.md)。域缺口关闭后完成整个F1复审、阶段提交与本地集成，再主动汇报并继续F2。

### F1 阻塞诊断补充（2026-11-15）

仅只读原生transcript／精确日志行补充[最小诊断](f1-blocker-diagnostics.md)，不重触发复审或重建原检查点。已确认失败发生于Codex子agent模型轮次，`codex_error_info=cyber_policy`，UTC `2026-11-14T15:04:45.305Z`；同thread响应头`x-oai-request-id=43d72018-f20e-43c8-9ab6-086e909cb720`。最后工具在`15:04:12.060Z`已exit0，04报告已产出，其后没有新工具调用记录。因此不把它写成已证的IDA拒绝或某条未派发命令。

八caller函数来源已齐；未关的是其它七caller返回后ABI／读回、状态caller其它状态／结果、正常32+屏内域及各路径有效前缀。当前没有受支持的恢复通道，不换模型／工具／参数／会话重放，F1继续阻塞；不重复无关测试或计划批准。诊断只输出必要元数据及原错误，未复制凭据、提示词／推理正文、完整日志或二进制。原`recovery-checkpoint-20261115.json`及取证原件保持原摘要，未提交／集成，F2未开始。

## F1 正常调用合同续审（2026-11-15，未提交）

本节最新状态：静态兼容性审查已能执行，八caller表／18个result入口性质、原12帧LMI几何及C边界已有新成果；独立复审保留Q／K真实写集缺口，撤回“逻辑K后即U／子门可关闭”的不充分建议。该子模型续审轮次于UTC `2026-11-14T17:54:28.509Z` 新以 `cyber_policy` 结束，准确thread／turn／CLI元数据和已执行边界见[新诊断](f1-blocker-diagnostics.md#7-本次正常兼容性续审的新中断)。按最新要求停止该项，不重派、换模型／工具或继续试探；F1未完成、未提交／集成，F2未开始。

新结果分账：`defeat-caller-normal01`核9owner／1526指令身份；`result-incoming-normal01`核30槽／18目标／22owner并有人工读写复核；`defeat-normal-c01`仅定向field_defeat 1／1通过（69人数、192筛选组合及96人owner／退出／失败），/RTC1可核而非ASan；`fx-resource-geometry02`取原12帧和已核LMI规则，最大72×71，失败标题parser原件保留。独立review的K／Q反例表明已初始化scalar可被读成额外descriptor，低D时collector也可改保存值；正常可达与已定义条件性行为不混作已复现产品bug。callback66错接35677已由原DAT／表目标反证，A03假线索剔除，不重开旧turn门。

用户更正此前把子模型轮次错误概括为工具封锁的说法，要求在现父会话用既有完整来源、正常行为和独立C写集继续；申诉不是个人开发的前置。当前[诊断页](f1-blocker-diagnostics.md)保留原错误／请求ID及执行边界，但不再以“没有恢复通道”冻结合法复核，不重发失败子任务或切模型／供应商。

新静态身份检查连接八caller及1db65共9 owner／1,526条指令，逐chunk匹配strict image及每caller直接调用目标，未执行原码。源指令另证1a866两次side比较来自同一栈实参，EBX仅作除数、defeat后未读，纠正此前可能影响side的猜测；完整caller及正常参数合同另账。新增field_defeat只构建／执行一项CTest，69个人数向量及192个筛选组合、96记录owner／HOST／失败与已提交前缀全部通过，原资源／用户档守卫未变。无GUI／新原码／sanitizer资格，不重复84项或82配对，不据C支持96签原版正常可达或31+像素等值。当前阶段门仍以[F1报告](f1-reference-domain.md)为准，未提交／集成、F2未开始。

## F1主体交付与QK后置（2026-11-15）

用户新授权取代此前任一F1缺口一概阻挡后续的顺序。已据真实C依赖与现有证据复审：启动RNG／BIOS clock／FIFO／repeat、构造及Load字段、decoder／页、孤立音频与设备机制不读collector原栈；相关死亡→掉落→AI／controller／phase、victory→prepare→title及其清键／RNG／FX／cue整轨迹仍有联合资格缺口。原形式证明没有改签通过，被拒子项不重派；支持跟进不作为开发前置。

本阶段完整主体提交包含固定20文件／字库及profile、当前69935／450／2085／0来源去向、已证turn／Save及raw／资源／预算域、37／66有效前缀和82容量修复、正式82 pair04及A20 startup01／Domain／有源分层资格、相应工具／回归及QK移交。历史失败和不同候选不合成一次全绿；本轮范围调整没有重新运行84项或82 pair。原C边界定向、18result接口和12帧LMI资源复核沿原收据身份保留。

QK作为F6最终门独立列在qk-behavior-followup.md，仍关联A04／A05／A08及受影响输入／RNG／音画项。下一步先做合法存档／正常操作的少量行为代表，按原资源、完整22987B SAV、WGC动画阶段和后续动作／多回合比对扩证；历史受控HP／人数夹具不能冒充自然边界。缺夹具、随机和不可保存中间态的局限如实保留；未闭合不得宣布全计划完全对齐。

主体经审查提交并本地集成后直接继续F2；提交／重建／直接集成检查身份见本地build/f1/rescope-20261115/local-integration.json。主项目用户FD2.SAV保留，不远程push／发布，不改混音器或扩大显示／电源授权。

## F2共享机制主体交付与最终联合门（2026-11-15）

本次以完整共享机制及已证consumer为一个提交单元，接F1 `503fe54b`，不是逐内部步骤提交。修默认RNG启动198次／共享App流；完整BIOS AX、15槽FIFO、500／33ms单owner重复与修饰键；唯一整数PIT时间、原signed／unsigned及逐读／fresh顺序；正式浏览／选择／菜单／移动／Scene／准备／结算等等待、有限UI相位和HOST／首错前缀。去重复focus和无来源的视觉推进，保冷热和原session owner。

全读点库存21 owner／42 caller／96静态CALL不是功能覆盖率。独立原源新增确认并修复：1cff0双selector之后的1d6c8公共闪光；AI item79的15055／2dfc8完整有限编排，及149f8从actor向selected扫描。冷表／progress图为唯一编辑输入，没有新通用VM、持久PC或第二RNG。当前C接口／原part literal、目标顺序、取消／HOST／错误／费用／RNG／acted回归与原版实机分别登记。

`ai-ray-ctest01`的旧夹具错误、`ctest02`的旧observer未识别新增前缀，以及方向oracle修正均保原失败；最终只按原义补显式phase／无诊断log消费的正例，不放宽生产支持门。最终新普通构建 `C:/Projects/fd2sdl-builds/fierce-newton-f2-20261115` Debug成功，`c-build-ctest01.log`46项直接影响检查通过且无skip；冷表3项、冷流程来源／负例11项、IDA发布14项及有界读取工具19项另账。旧E盘收口构建因磁盘满失败，没有算成功；仅移除108个可再生产`.ilk`缓存，保清单、PDB、二进制及所有取证。新根经FileTracker API和实际关键头tlog核验未排除。

App a57的 `input-native03`、`motion-native04`、`prepare-native03` 原版先／SDL后三双端代表均通过，strict ON、WGC HWND、隔离存档／来源和正常owned清理闭合。新构建ce与三原生代表的源／工具／冷数据完全一致，但PE节并不逐byte同一；原生证据保a57身份及解释继承，不写成ce实际新跑或一次全候选全绿。运动03错误启动路径发生于首窗口前，04正确重跑另目录保原件。

`normal-clock-native01`因命令时模式与补零语义未资格；修有界纯读取模式门后的02两页映射／人数／code/fixup/长度均过，aux五EV三本体仅支持当次FS0／GS20，overview12秒172EV零本体，整体仍失败。没有BREAK、寄存器／返回地址操纵或重放QK实验；callee不代本体／首比较。原件及独立只读复核保留。

隔离Domain发布仅五个已证函数名及18处合同注释；1312函数strict bytes／边界／签名／flags／chunks不变，C仅五标识正规化后token相同。528登记及11发布SHA逐项核验，旧静态／原型报告只继承源身份，不重新签语义。无完整二进制／媒体入提交。

实际依赖复核后，三门（18b84首指针、2000a首FS／GS、1b1e7继承EBP全steady）、type9 provider／21db2正常入口、完整输入／RNG联合轨迹仍明确未过，详见[F2残余表](f2-time-input-rng.md#主体交付与最后联合门)。这些不是构造／Load／存档、解码或PCM算法的实现前置，继续F3；F4／F5补全像素、非例外PCM／PIC及新反馈原版代表，F6逐性质合签。发现正常反例必须修复，不能靠拒绝、改名或文案豁免。QK仍由原专项负责；Windows／EX-AUDIO-NUKED选择未变。

本地提交／集成、原始构建／定向检查、物理字节及用户SAV保护的实际身份见两树 `build/f2/subject-delivery-20261115/local-integration.json`；收据仅按已执行结果写入。提交后继续F3，不远程push／发布、不改混音器或扩大显示／电源授权。

## F3实施检查点（2026-11-16，未提交）

在F2 `232aca6`之上修模板构造及两个正式outer的原保留写集、活动Load的raw2写回、完整Loaded metadata来源；保手工2986f独立合同、原58／22持久写集与已有session／RNG／资源owner，不新增热状态或通用VM。当前仍未完成整阶段复审／提交／本地集成，统一状态见[F3报告](f3-gameplay-lifecycle.md)。

实际四项state/save/unit/Scene直接检查通过；11个玩法consumer首轮7过4失败，三个过期S5负例和一个Load旧class期望按原分派／写点修正后，仅复跑4项通过。Scene三个全record控制门的非零九B回归通过；AI11的保守资源key可多分组，成功／错误代表保持动作／RNG／字节合同，首轮漏算拒绝后早停的测试失败保留，修测试后只复跑AI通过。正常254／255存取、256／DWORD上界H拒绝的文件bytes／mtime及owner无副作用也已直接通过。

`load-native02`实际原版／SDL正常END、取消／成功Load及Save，两个端点各11处class重排和完整22987B／checksum通过。输入producer未证，不执行或代签Z；strict ON、WGC HWND、后台owned输入／正常退出、原资源及用户档守卫通过。首轮原生c4与注释重建6654分账，来源影响继承单独有收据；补齐snapshot／copy／配置守卫后，仅追加最后03双端实际复验6654，788源及App／driver均已按实测SHA／mtime归档。两端各11处class及完整容器、reference前后、strict ON及job43776正常清理通过。01在首窗口前的Z预检失败及工具原件保留。

新增静态Domain六函数902B和两函数67B，结合完整writer及全部33条目录，定位27个短fresh metadata Save的首越owner读，以及新目标未赋值checksum输入的首语义读；两种条件、方向和文件副作用时点分别登记。已有Loaded／三个全长fresh／合法opaque来源仍为严格合同，短尾保目标和新文件ff明确为原首U之后的H继续，不猜原heap或新增Save拒绝。8个已证合同同步命名输入及隔离IDA发布，1312函数边界／签名／flags／chunks／bytes和C tokens不变，0改名，529登记含新增allocator证据；11SHA最后刷新并核验，14工具检查通过。静态／原型旧报告只继承，没有重跑其语义。

正式root分母与97参数四栏账已落，实际来源／旧原码／C／自然producer分离。新增缺例正在通过原入口和脚本前态修正后核验，失败保原件，不按测试名或表存在合签。0dh自然prefix与九B动态别名读者仍在源闭包复审；QK相关掉落提前结束和App返回条件仍留原联合专项，不能按人数臆造门。F4只读盘点已准备，不混入本次F3提交，不签全像素／音频或全自然链。


## F3主体交付与继续F4（2026-11-16）

构造71/9及weaponFF保0d、活动Load raw2、完整Loaded metadata、App完整A00跨败北／标题／START和四raw options均已修复。Scene原四点58/22只构造一次，Game窄入口消费；Game关闭前交回完整表，失败Load不回交空表。没有第二热roster／RNG／VM；未知byte不猜通用默认。完整范围与失败、当前候选、独立来源分账见[F3报告](f3-gameplay-lifecycle.md)。

97参数四栏账补37例；12生命周期直接检查为CTest01十过、02 Play过、03 Prepare过。链接漏改名、Load不是结果提交点和两处准备测试初态错误保原日志；不提高timeout／放宽memcmp／生产门。Load03实际6654经严格完整容器／11 class写点；START03实际DDC9经两端各117资源等待、首浏览和UI Save字段。它们是两个不同候选，788源及各App／driver精确归档，不能拼成一个全fresh。两端进程均正常退出，所有owned job无残留／强杀。

START01因OFF在首窗前停止；用户随后明确授权主动点亮与必要临时电源计划。02实际一次有界唤醒成功，但参照缺nosound配置又在首窗前停止；03补隔离配置后通过，同线程12872释放显示要求。未改电源计划或系统混音器，前后powercfg记录相同。03共用running追加的幂等quiet CLI经实际command与已守卫配置事后join，全声明／派生设置不变，不补造运行前manifest已含该CLI。原资源与用户档只读，所有来源／配置守卫成立。

有限读者／返回值闭包采用修正后的184关系／91实际调用点；漏外部条件边的01闭合撤回。完整Load→turn3两模板的具体mask与首checksum U、fresh族来源归纳另账，不以未测直接判U；wholeLoad／live／合法sync继续V。IDA末次发布4生命周期注释、530登记，1312函数strict结构／C tokens不变、0改名、11SHA及14工具检查通过。原码3774等仅保给定before80旧身份，不重跑或扩签。

QK三个条件consumer仍未一般表达，Town0先继续后续battle的时点已纠正；正常可达／联合资格及有限outcome实现留同一专项。后期first-App R实际sync／reorder来源、全自然／动态链和败北重开后缀也仍待F6。主体交付不是A04／A06—A09完整终态通过；这些缺口不读取F4解码器或F5采样算法，依已授权真实依赖继续F4。实际提交、物理源、主线构建／定向集成及保护记录只认两树`build/f3/subject-delivery-20261116/local-integration.json`，不远程push／发布。


## F4共享呈现主体交付与继续F5（2026-11-17）

以完整共享资源／呈现实现、26行给定有效before的算子／正式绑定资格为一个提交单元。修direct／batch hidden、80h读钟、线性页及背景／actor边界、App／palette／嘴型／UI历史、SHORT真实三参／双HP、地震三表／actor圆心／11..12漏演出及HOST续行，保持唯一热owner／RNG和冷定义；三组quake参数唯一在field_game_effects.json。没有通用VM或第二场景状态。

完整分母16300唯一span，资源reader、typed／动态文本、对话／标题／credits、FULL／SHORT／MAP／DERIVED／COMPOSITE及正式cold walker分别保原CPU、独立source算式、真实C全页／DAC或组合依据。大量期望／媒体留本地，只提交可复跑工具与无完整原资源的合同。ID6数值provider／有限geometry及五fresh App回图按源分类，不把旧dormant C页作原新owner。原U首点、H后缀和未证normal producer分账；详见[F4报告](f4-presentation.md)，不凭计数宣称完整对齐。

34项直接回归初次32过／两失败；scene_transition漏地图upload观察、s5_stage_source合成raw32超45×50地图边界，均仅修夹具后定向通过，原32项不重跑。SHORT新双端严格63／3条及31页／30ACK、完整Save保自身候选；其它flash／对白／准备代表各保实际来源。PUB05的1312函数字节／类型／边界保持，538登记及11SHA通过，14发布回归；294C／H的998入口审计通过。随后仅工具／文档变化不改C快照。

Magic12四轮原生全部保FAIL：01取消应返回列表的旧oracle错误；02整Q0采样缺证且旧投递时间／普通range假阳性后来收紧；03逐帧识别积压超helper45s；04修同义像素比较及内deadline／活性后正常stop100帧，原ring、Q0局部及HIT322正证仍不足原组合门。四轮均未完成SDL／目标／F2 MP261／禁Save后缀；无额外键／Save来凑过门。最后Job10720及原4360正常exit0、无残余／强杀，来源／参照／原件及同线程显示恢复通过；03／04显示本来ON，没有电源／混音器改动。

独立复审确认：原3次sampler／组合9＋真实Game C9整页／palette／DAC＋Battle完整60圈／共用upload可关闭给定有效before的有界软件显示机制；45个src／cold相同，局部只签source-class，不解释未证的37／59非Q点或冒全部物理present。停止同条件GUI赌采样。当前真实normal before、双端后缀、完整END→Save、输入RNG／PIC及fresh来源联合明确移交[F6必需门](f4-presentation.md#5-f6必须接回的完整域)，不把完整A14—A16写成通过。这些门不读取F5 AIL／PCM／设备机制，按已授权实际依赖连续推进F5。

完整主体提交／本地快进、当前主线构建／定向集成、物理字节／换行继承及用户SAV字节mtime的实际结果，以两树`build/f4/subject-delivery-20261117/local-integration.json`为准。旧失败、候选／继承／未验不改签；不远程push／发布，不自动开新功能阶段。


## F5音乐音效与生命周期交付（2026-11-18）

用户终态改为复刻游戏表现；纯CPU／IRQ／PIT／PIC、驱动ABI／原栈／完整RAM等形式门退出必做。裁减前386文件完整封存，移出141个未调用研究C/H／冷定义、77个纯研究工具与四个未编source_memory文件；通用路径守卫因F6真实依赖而保留。旧失败／来源／媒体保历史身份，不改成通过。用户另增独立DOS-CLEAN，顺序固定F5提交／集成／汇报→DOS-CLEAN提交／集成／汇报→F6，前述未落地研究删除不代替后续已落地代码清理。

共享音频交付保单sequence／常驻Nuked chip及同App epoch／Audio owner；修原PCM采样／相位／双P混合／共同DC和采集S16回译，保stop8／replace／尾音／volume／pan。离场text416完成后／200ms前及开场text9后／script98前补停曲。FDMUS20=15score＋5空条目，PCM26bank／111绑定／78payload分别核独立资源、源算式、真实C及正常代表，不由生成器／重复引用或测试数自证全游戏。

原G1／G2七键运行取得两完整59904像素LEFT和30.261s连续WAV；SDL初次无skip Title45s超时、未进G，整体FAIL保留。源与原录音确认G2从旧score开头重播，修掉SDL内部资格门造成的后续无音符：同player／DAT重播已解码score，不模拟释放后的heap。7音乐直接检查通过；SDL-only实际PID18572完成七键和27.923s连续录音，原端不重跑。离线四段多声部乐句与粗节奏相符，不warp／拟gain／设相关率PASS，不签sample onset／完整loop或自然入口。LEAVE双端另完成415／YES／完整416／正常exit0；全部WGC／owned／来源／SAV／显示需求守卫及清理分账，未改电源计划／混音器。

实质复审补有限上传后的首错传播：Scene／Play／Prepare／Battle／Effect／Game与Main，在读HOST／键、发新cue、下一画面／回合前负返；保已达绘制和free-only出口。首构建在Effect测试struct误写指针处停止；修测试后23项20过3失败。Main夹具多排早键、Scene cwd缺资源路径及Prepare合成时间／共享clock回拨仅修测试／登记，定向03为5／6，04 Prepare单项通过；旧失败原件不回签、不改相位／RNG／功能断言。生产语义在首构建后未再变化，提交时只规范SB缓冲provider末尾空行，App `f0d79404a8c09d12aec0c71ea146c2dbed397fd7c90de30fe35b47f839c63eca`。普通音频owner／混合未发现额外must-fix；独立Main peer及本轮完整收据见[F5报告](f5-audio-device.md#7-失败传播审查与阶段交付)。

现有隔离IDA库同步21真实命名／22注释，1312函数／573登记；边界／类型／flags／chunks／字节不变，C tokens仅登记名映射，11SHA／14交叉检查／1001规范入口已核。原严格端口时刻RED仅研究；不重开硬件／原loader疑虑。Windows平台与EX-AUDIO-NUKED保持，物理扬声器／真实拔插仍未验。

实际完整提交／本地快进、物理文件及换行资格、主线Debug构建／定向集成和用户SAV保护，以两树`build/f5/subject-delivery-20261118/local-integration.json`为准。阶段交付不是全部A01—A22已通过；正常输入／RNG／QK及FX／cue联合、无skip Title时长仍归F6表现门，纯形式不再要求。向主对话汇报后直接执行独立DOS-CLEAN，不远程push／发布。


## DOS-CLEAN独立清理交付（2026-11-18）

在F5 `b6462edf`之上核正式consumer，16槽／4字节句柄改为MDI／DIG两服务和必要游戏tick；删pending／busy／defer／EOI及program hooks、BGM资格／PC／gap和未读投影、buffer ACK／未用DSP／rate／filter／重复相位／始终enabled状态，保真实整数节奏、消息／loop／fade、采样／滤波／双声部／短尾及关闭前缀。35代码／构建／测试文件净减565行；14产品C／H净减358行。输入、SAV、工作页、cold预算及来源工具依据实际消费者保留，未改原资源／用户档／混音器，未追加硬件研究。

普通110ms独立模型前后5280双声道帧完全相同，不读C生成期望。21目标构建、21CTest首轮20过，余一DIG旧编号断言FAIL；修替身／编号及固定rate上界后仅四项通过。删人工未读block_ready迟交付族，改真实quiesce取消；03关闭后sync旧期望FAIL、04局部补丁误匹配相邻行FAIL保原件，05仅audio_original修正通过，生产合同不改。三共享机制交叉复审及外围调用盘点无生产must-fix；全部候选和运行／静态／继承边界见[清理报告](dos-simulation-cleanup.md)。

本轮不重跑原G／LEAVE或历史GUI，按score／cue／stop不变、数值／consumer检查继承有限资格，不声称新PE原生。实际独立提交、本地快进、物理源、主线重建／定向执行及保护记录只认两树`build/dos-clean/subject-delivery-20261118/local-integration.json`。按用户新增顺序独立汇报后直接F6，纯硬件形式退出，QK等真实游戏未知继续保门；不远程push／发布。

## F6 范围内的 Windows 通用维护（2026-11-18）

基于 `50272b0` 构建 Windows Debug 程序及 9 个定向测试和工具目标。8 项 CTest 通过，覆盖无资源启动检查、SDL 输入事件、音频传输、设备失败降级、关闭和源释放。未使用原版对照。

正式程序使用独立目录及启动所需的数据副本。真实 Windows 窗口出现后，仅向本轮所属窗口发送 `WM_CLOSE`。程序退出码为 0，Job 确认无残留窗口或进程，未强制终止。另用独立 WASAPI 探针提交 294 次静音数据，全部成功，三个源各释放一次。未测试扬声器输出、物理拔插或画面一致性。

合成存档首次检查因历史 `START.SAV` 依赖失败。新增 `save_synthetic` 和 `--synthetic` 入口，将自身存档检查从历史夹具中拆出。修复后，空目录下直接执行和 CTest 注册入口均通过，覆盖校验和、完整读回、覆写、坏文件拒绝与保留。不改运行时存档实现，原 `save` 入口仍保留。原件及用户存档的摘要和修改时间保持一致。

实际运行回执在 `F:/fd2sdl-evidence/f6-windows-maintenance-20261118-01/`。两树 `build/f6/windows-maintenance-20261118/local-integration.json` 登记维护提交、本地集成及复测。Windows 通用维护独立交付，F6 完整复刻验收仍未完成。

## F6 Windows 既有测试回归（2026-11-18）

本节记录 F1–F6 大阶段收尾的全量回归。以 `3fdf087` 为基线在独立检出恢复全目标 Debug 构建。首轮无窗口 CTest 166 项通过、5 项失败；修复构建接口、过期断言和夹具后全量复测，171 项均通过。原生 SDL 输入和 WGC 测试在同一应用二进制通过，合计 172 项，无跳过。首次 Python 全量测试及定向复测按用例名合计 2371 项通过，两组历史原码对照跳过。

详见[F6 回归清单](f6-sdl-regression.md)。本地集成、物理文件与保护记录以两树 `build/f6/sdl-regression-20261118/local-integration.json` 为准。原版完整对照及 DOS 硬件等价属于历史目标，2026-11-18 已退出当前 F6 验收。未在 Linux 执行。

## 全仓代码审查修复与精简（2026-11-18）

以 `9bbe523` 为基线，按模块分组审查产品源码，只修复经代码确认的问题；与 DOS 原版无关的重构保持逻辑和表现不变。

修复：

- credits 资源安装（`fd2_field_game_source_credits_setup`）原先先换入 resource30 再追加 group0，追加失败会留下半装状态。现在全部可失败步骤都在局部资源上完成，成功后才换入 Game，见[session 文档](../systems/battle-session.md)。
- stage 入场／胜利／结算共用逻辑解释器补齐与 source 节点相同的参数域检查。`branch_any_hidden` 负下界原本会越界读取，另有三个叶会把越界参数截断成 BYTE，见[冷定义工具](../tools/static-data.md)。
- headless 的 range 和菜单高亮周期补追改用同一辅助函数。时钟回退时只重取起点，不再因无符号差回绕而长时间循环；时间正常前进时相位推进不变。
- 战利品仓库选择关闭动画中，音效后的时钟／显示错误原先按成功返回，并继续提交领取；现在返回 ERROR。
- credits 素材打开失败时只回收本次新开的 FDTXT31，不关闭 roll 阶段已经打开的文本。
- `FD2SDL_APP_PCM_CAPTURE_MAX_MS` 非法时补 SDL 错误信息。
- `original_reference_contract` 的 junction 用例在 `setUp` 清空环境后启动 `cmd`，Windows 因缺少 `SystemRoot` 报 WinError 87。改为向子进程传入导入时保留的最小系统环境；断言不变。

精简（行为不变）：删除无调用者的 TAI 预览 tileset、`fd2_map_sprite_decode_frame`、旧存档单槽读取／字段访问器、`fd2_field_move_query_step` 与 `fd2_field_audio_stop`；合并 AFM 两份相同的 RLE 解码、tile 两份 blit 循环、存档三处整文件影子拷贝（校验通过后原地写并统一重封校验和）、`field_unit_base` 重复的物品属性回调、指令菜单／系统菜单两份过渡函数、目标范围清理和 6 处场景出口表达式；两个仅文件内使用的 `field_game` 导出改为 static。测试会以 `#include` 引用的私有函数和测试专用包装保留。

验证（Windows Debug，当前平台）：全目标构建通过，无新增编译警告。CTest 172 项中 171 项在 `-j 4` 下通过，`original_reference_contract` 在并行负载下超时一次，单独重跑 8 次均通过（约 0.9 秒）。Python `unittest discover` 共 2381 项，12 项错误均为环境缺失：10 项缺少 `unicorn`（F6 记为历史原码对照跳过或使用本地缓存），2 项缺少历史夹具 `build/f1/domain-boundaries/source09.json`。`check_src_addresses.py` 1001 个地址均为 canonical。本轮未执行原版对照或 Linux。
