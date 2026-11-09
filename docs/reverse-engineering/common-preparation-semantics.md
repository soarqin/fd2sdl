# 通用准备服务：G1 原版语义依据

本文对应[通用审计](../plans/common-logic-audit.md) C16—C19。取证日期为 2026-11-09—10；输入为 `c1b6598` 已发布 canonical 伪 C、独立 IDA 9.5 数据库副本及既有严格 relocated 镜像。这里确认静态控制流、读写和原始数据引用；**未运行原版城镇服务，不声明 G3 已实现或原版运行验收通过**。优先级仍见[总计划](../plans/development.md)。

## 1. 来源、地址和本轮结果

函数主键全部是 canonical relbase VA，object1 offset 为 `VA-0x10000`；不能套用历史 runtime 绝对地址。通过 `ida-domain-api`、`ida-domain-scripting` 的 Domain／idalib 只读打开 `build/g1/ida/preparation/FD2.EXE.i64`，输入与结束摘要均为 `f3fb61198b00660e17e3b131be1339350c345209fbbb4d8b38fc07bd2d0ede53`，未保存反编译推断类型或修改共享数据库。

本地读集 `build/g1/ida/preparation/evidence.json`（SHA256 `87c5965c265064137228238188a2bdbec2a7615ee57a9bca319ab3f5ea2ebf09`）包含 40 个完整函数、694 个 CFG block、4,828 条指令、16,923 byte 函数 chunk，及费用引用邻接区、26×31准备记录、30个NoTown标志、6个服务头像引用共四组数据。所有 chunk 和四组数据与既有严格 relocated 镜像逐字节相同；镜像 SHA256 为 `8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1`。结果见同目录 `strict-byte-verification.json`。这些是静态字节对照，不是原指令执行。

初次调用遇到当前 Domain `get_pseudocode` 不支持文档中的 `remove_tags` 参数，已按实际 API 改为 `str(get_pseudocode(func))`；其后 `0x2a29d` 新反编译失败原样登记。该函数仍有完整指令／CFG／原字节，菜单分支同时与已发布 canonical 函数交叉核对；不能把本轮报告说成 40 个函数全部重新反编译成功。

复现脚本为 [`g1_preparation_evidence.py`](../../tools/ida/g1_preparation_evidence.py)。按[IDA 工具规范](../tools/ida.md)在隔离微秒时间戳目录创建下列只读 wrapper，并通过 skill `run.py <wrapper> -f <独立i64>` 执行；Windows 每个 runner 使用本轮独立 `TEMP`／`TMP`，避免执行器清理其它任务目录。不传 `--save`：

```python
import runpy
collector = runpy.run_path("<repo>/tools/ida/g1_preparation_evidence.py")
print(collector["collect"](db, "<repo>/build/g1/ida/preparation/evidence.json"))
```

独立字节核验不需要 IDA。Windows PowerShell：

```powershell
python -B tools/ida/g1_preparation_evidence.py --verify build/g1/ida/preparation/evidence.json --image <strict-relocated-image> --json build/g1/ida/preparation/strict-byte-verification.json
python -B -m unittest discover -s tools/tests -p test_g1_preparation_evidence.py -v
```

Linux Bash 使用 `python3 -B` 和本平台独立数据库／证据目录，参数相同。本轮 Windows 4 项取证保护测试通过，覆盖错误镜像、漏函数／CFG、字节篡改、代码范围与表长度／内容拒绝；Linux 未执行。测试不代替服务语义验证。数据库、原始读集、机器码／表切片不提交。

## 2. owner 和服务分派

| 原字段／入口 | 本轮确认的合同 |
|---|---|
| `dword_53BF7`／`dword_53BFB` | 持久队伍记录基址／人数，每条 `0x50` byte；城镇先让通用 actor 指针 `dword_53A45` 借用此表。不能在服务中重新构造整个单位 |
| `dword_53BF3` | 钱／资源的原版运行值；`0x26a7a` 先加给定数值、`0x26b91` 先减给定数值，再演示八位金额滚动。钱的提交不在动画结束时发生 |
| `field_town_selection_execute @0x2670e`（原 `sub_2670E`） | selector0 酒店；1 武器店、3 道具店共用 `sub_279BC`；2 出阵确认，超员时调用 `sub_2AF28`；4 教会 `sub_29DAA`。曲目分别为酒店13、武器14、道具15、教会11，回城10 |
| `sub_26E38 @0x26e38` | 四项服务页左右循环选择，Enter／Space 确认、Esc 返回；`sub_26EDA @0x26eda` 负责实际等待、动画及 blink RNG。不是简单无 RNG 的列表查询 |
| `sub_279BC @0x279bc` | 店内四项为购买 `0x2872b`、出售 `0x28cbd`、装备 `0x28efe`、转交 `0x28f65`。页面保留选中的店内命令；购买还保留商品索引和滚动位置 |
| `sub_29DAA @0x29daa` | 教会四项为成员详情 `field_roster_detail_select @0x29620`、转交 `0x28f65`、复活 `0x2a43e`、转职 `0x2aa00`。没有从这四项推导额外治疗、捐献或商店功能 |

`sub_26A0D @0x26a0d` 从当前准备记录 `0x4e809` 返回的 31-byte 行取商品：武器店从偏移3取至多12项，道具店从偏移15取至多8项，隐藏店selector5从偏移23取至多8项，遇 `0xff` 停。商品 ID／可售阶段是关卡绑定，读取、购买、价格和库存规则是通用能力。

隐藏店的真实 caller 已确认：`field_town_prepare_execute @0x26152` 的普通键默认分支比较当前selector与准备行byte1、BIOS scan与byte2，同时匹配便把selector设5，随后Enter／Space按通常确认进入 `0x2670e→0x279bc`。隐藏店图像为FDOTHER63，头像由 `byte_52387[5]=132` 给出；原入口先请求非3店曲14。stage1行头为 `[0,0,84]`，即酒店焦点遇scan `0x54` 的分支；具体宿主组合键映射仍须原版输入证据，不仅凭名称猜测。其31-byte行及引用已通过本轮Domain／严格镜像对照（806B SHA256 `e6332bf1c878f962553757101e3b45422a51ac1eead96ddbb80381a44af3c094`）。通用热键条件／隐藏店服务需G3与输入合同覆盖，具体按键、焦点和货单参数归S，不能把整个能力当关卡剧情。

另有NoTown准备流程：`0x26152` 读 `byte_523E7[stage]`，非零时先提示410并可调用四槽Save，再反复运行 `0x2af28` 直到返回非零；不经town／商店。30B来源的非零stage为22、23、24、27、28、29，SHA256 `d26cecc9a5e738c9286bcbcaa4d44060a016d144090ddb7262dac3c065650c17`。因此后期20人部署并不要求凭不存在的stage27准备行取值；普通town与NoTown是两条通用准备合同，其具体启用stage属绑定。这里的静态调用证明不等于这些后期stage已运行。

## 3. 购买、出售、装备和转交

### 3.1 购买 `sub_2872B @0x2872b`

1. 商品列表确认后按原 23-byte item record 的 `+19` 读取 `u16` 价格，写动态显示值；先不改钱或库存。
2. record byte0 `<32` 时枚举 `field_unit_can_equip_item @0x1c1c3` 允许的成员；没有成员则提示并返回商品列表。byte0 `>=32` 时候选为全部持久成员。不能把购买候选与物品 Use 的效果码混用。
3. 商品确认框 YES 后检查钱是否足够；不足时提示并 ACK，回商品流程。随后选择收件成员，装备类使用带派生属性预览的候选页，其它用完整成员页；取消不购买。
4. 确认收件人后检查其库存计数是否等于8，满包提示并返回。成功先 `field_unit_inventory_item_append @0x1bb8c` 追加 flag0／item ID。
5. 装备类追加后再问是否立即装备；YES 才 `field_unit_equipment_slot_set @0x1c142`、`field_unit_combat_stats_recompute @0x1b750`。这层 NO／Esc 只取消立即装备，**已经购买的物品继续保留**。
6. 最后 `sub_28B41 @0x28b41` 店员／服务动画及键缓冲清理后，`sub_26B91` 扣钱，返回保留商品选择的列表。追加、可选装备与扣钱不是同一瞬间的原子写入；实现需预检资源并保留确认后的已提交边界，不能通过重播整个流程重复买入。

原始指令写点包括 `0x28a78` 追加、`0x28b0b/0x28b14` 装备／重算、`0x28b2c` 扣钱。`0x28632` 装备预览另算四个派生值，不写 live 单位；不能为了预览把候选装备先交接给持久 owner。

### 3.2 出售 `sub_28CBD @0x28cbd`

成员页确认后遍历全8个 raw 槽，仅收集 flag byte 为非负的物品。空包提示；物品页取消回成员选择。成交价为 `floor(3 * item_record.u16(+19) / 4)`，写动态金额后问 YES／NO。NO／Esc 不成交；YES 后先 `sub_28B41`，再于 `0x28ed7` 加钱，然后 `0x28ee1` 移除原 raw 槽、`0x28eea` 重算送出者派生。已装备物品可进入同一原槽路径；不以出售页的视觉压缩索引替代 raw 位置。

列表构造会压缩可见 ID，但提交调用使用记录访问 helper 的原 slot 语义；当前既有密集库存门与原版全部有效库存域的关系须联合原始持久记录核实。此处不以合成稀疏库存断言原版保证无副作用或任意稀疏均合法。

### 3.3 装备与转交

`sub_28EFE @0x28efe` 反复选持久成员，复用 `field_unit_equipment_menu_execute @0x1bffe`；保存并恢复私有页面 owner，随后恢复店员头像资源。职业许可、同类装备 flag 与派生重算复用既有领域规则，不复制另一套服务专用算法。

`sub_28F65 @0x28f65` 在商店和教会均可达：提示512／ACK→送方成员→物品→提示510／ACK→收方成员。收方库存等于8时提示并 ACK，返回选择；有空槽时先取送方 ID，再移除送方 raw 槽、向收方追加 flag0、**只重算送方**。无相邻距离门，没有满包交换分支；这与战场 Give 的空间／行动尾合同不同。成员和物品取消按原层级返回；成功后继续服务循环，不扣钱、不标记战场 acted。

## 4. 教会复活和转职

### 4.1 复活 `sub_2A43E @0x2a43e`

`sub_2A07A @0x2a07a` 按持久 raw 顺序收集 `field_actor_is_hidden @0x34894` 为1的单位，不另加 HP0 门。无候选显示588并 `dialog_wait_key(0)`。有候选显示589／`wait_key(1)`，进入三行可滚动列表 `0x2a29d`；取消返回服务页。选中后取单位 `+0x20` 的职业／移动 profile 和 `+0x21` 等级，费用为 `word_at(0x52397 + 2*profile) * level`。列表显示 `word_52399[profile-1]` 与确认时地址相同，不是两张费用表。

确认590的 YES 后先检查钱，不足显示504／ACK；成功顺序为扣费 `0x2a60f`→`unit.flags(+5)=0 @0x2a617`→`HP(+0x40)=HP_MAX(+0x42) @0x2a61f`→刷新钱栏／关闭底框→曲17及服务提示→回曲11／继续候选循环。它清的是整个 flags byte，不是只清 hidden bit；当前 MP、MP_MAX、其它状态字节、库存、等级及经验在这段写集中没有重置。不套用新建构造器或通用全恢复。

`0x52397..0x523d5` 读集只是消费引用邻接区，不宣称31条都为合法费用。首 word506是费用引用前的邻接常量，不能凭连续读取证明职业0可复活。交叉核对32条原玩家基础表与36×2转职表，两者profile均不含0；其联合有效引用集合为 `1..6、8..25、28`，最大28。该集合内的实际系数来自上述原地址；7／26／27无当前玩家来源，仍保留原字节，不发明职业费用。外部畸形存档不得据此绕过有效单位检查；若后续证实事件能改写玩家profile，须把它补入同一有效域审查。待实机最少覆盖价格精确相等／不足1、hidden且HP非零候选，以及 flags 含其它位的实际有效存档来源；不把畸形合成值当正常玩法。

### 4.2 转职服务边界

`sub_2AE0E @0x2ae0e` 仅按等级 `>=20`、unit ID `<18` 且不为7枚举；基础目标 `ID+32`，持有 `byte_523D5[ID]` 指定物品时改为 `ID+50`；ID9持物90优先目标52。原 inventory 查询 `sub_2AEDB @0x2aedb` 返回对应槽，不猜道具名称。

候选提示592／ACK→候选选择→确认594。NO／Esc 不转职；YES 后若目标52消费物90，否则目标 `>=50` 消费当前原 ID 对应的特殊道具槽；普通目标 `<50` 不消费道具。然后曲16／`sub_2FB2C @0x2fb2c` 转换演出→回曲11→写职业和unit ID→重建整队 sprite cache→`sub_2AC7D @0x2ac7d` 增量成长／提示与最终恢复。**未见扣钱调用**，不能套用复活费用。

转职不是新建角色，不重置 text ID 或整80B记录；五项成长、移动增量、等级／经验／HP／MP的精确写集、`0x4e7dd` 职业表有效域和道具映射见[通用战斗语义 §4](common-battle-semantics.md#4-单位3267与转职的精确合同)，G3 必须引用同一来源。FIGANI转换演出读取旧／新 unit 的资源，不能把播放成功当已经提交职业变化，也不能在呈现重播时二次消耗道具。

## 5. 超员出阵：选择、重排和关卡约束

`0x2670e` 在 stage `<27` 且人数 `>16`、或 stage `>26` 且人数 `>20` 时调用 `sub_2AF28 @0x2af28`。`0x26152` 的独立调用来自上述NoTown流程，未套普通town超员门。选择页固定 raw0不参与网格，其他 `count-1` 成员可选，要求选15或19人；包括 raw0合计16或20。不能把原版后期20人上限改成全程固定16。

选择位图初值全零。左右循环、上下跨10格，Enter切换当前选位后向右；`sub_2B749 @0x2b749` 计数达到15／19即退出选人并调用 `sub_2B777 @0x2b777`。后者复制完整 `0xa00` 持久表，raw0保持原位，将选中的 raw1.. 按旧序放前，未选中的按旧序放后；**不减人数、不删除未出阵者、不改其80B记录**。未用尾也保留原缓冲内容。随后重建 sprite cache。

之后才执行具体 stage 的必须成员检查和最终提示658／YES确认。这意味着选满即已提交 roster 排序；**最终 NO／Esc 或必须成员检查失败返回0，并未恢复旧排序**。只有在选满前 Esc 才尚未调用重排。不能把整个出阵 UI 取消统一写成无副作用。

| 必须成员规则 | 专用数据与通用执行边界 |
|---|---|
| stage16且 `sub_33499(18)!=0` 要求text18；stage18要text16；stage20要text21；stage21／22要text24；stage25要text9和29；stage17／19／`>25`要text9 | 这些映射是关卡约束／S绑定；检查选中前缀及反馈能力是通用执行能力 |
| `sub_2B439 @0x2b439` | 检查重排后 raw1..15或19 的text ID；缺失显示657并返回0。不是把指定成员偷偷加进选择 |
| `sub_2B843 @0x2b843` | 对stage17／19／`>25`、20、21／22、25规定的text ID继续移到raw1，原其余相对顺序保留；stage25先9后29，最终29在raw1。发生在最终658确认之前 |

按语义拆成有限规则即可：同伴选择→达到数目→稳定重排→当前场景必选检查→特定前置重排→离场确认。不能把整段 `2af28` 藏入一个 native 动作，也不能为取证提前实现stage16以后的完整剧情。

## 6. 冷流程和失败／取消合同

G3 的冷定义应控制服务分支、提示、ACK、每级取消去向、物品／钱／职业写点与返回位置。领域代码只提供已确认的候选、价格、库存、派生、排序与owner操作。现有 battle `loot` 的 ACK／TEXT_READY 可复用设计，但服务不自动具有战场行动结束、相邻目标或满包交换语义。

| 场景 | 提交前 | 已提交后取消／失败 |
|---|---|---|
| 购买收件前取消／钱不足／满包 | 不加物品、不扣钱 | 立即装备问句取消仍完成购买；资源／关闭错误必须报告已有追加，不能重试全部商品流程 |
| 出售确认 NO／Esc | 不加钱、不移槽 | 原成交顺序先加钱、后移槽和派生；实现预检失败需停在准确写点 |
| 复活确认 NO／Esc／不足钱 | 不扣费、不改flags／HP | 原扣费在恢复前；晚错误不可重新扣款 |
| 转职确认 NO／Esc | 不消费道具、不变职业 | YES后道具消费先于演出／职业和成长；不能声称晚错误能整步回滚 |
| 出阵选择页Esc／最终658取消 | 选满前无排序；最终确认前可能已排序／前置重排 | 返回城镇仍保留已提交顺序，完整 roster不截断 |

原版没有为坏资源、畸形容量或写盘故障提供 SDL 同级校验的证据。SDL 可以预检并保护原档；这不允许改写正常玩法的费用／取消／提交顺序。`sub_26EDA`、`dialog_wait_key` 的等待会耗共享 RNG，不能把“服务本身无随机效果”误写成整个交互零 RNG。

## 7. 原版定向实机取证步骤和通过判据

以下是待执行范围，由主执行者串行安排；本轮未启动窗口。使用合法原版资源／SAV独立副本，经原标题LOAD／酒店进入准备态；fixture必须记录来源、checksum、真实单位/资金/库存和必要修改，修改夹具不等于自然到达证据。Windows先确认console／session显示ON，WGC只抓owned HWND，后台输入只投递owned PID，逐页确认业务响应；最后保存、正常退出并清理本轮窗口，不唤醒、不改电源或混音器。

| 组 | 执行路径 | 判据／证据 |
|---|---|---|
| P1 购入与取消 | 现有stage1准备→武器店→商品→YES→合法收件；立即装备YES和NO分别一次；另跑商品Esc、钱=price-1、收件满8 | 购买后追加flag0；装备YES仅相应同组flag/派生改变；NO仍扣全价并保留新物品。每层WGC及酒店四槽前后完整roster/钱/未知尾；失败分支业务字节不变，等待RNG单列 |
| P2 出售／装备／转交 | 武器／道具店→出售已装备物；NO再YES；装备页重选／Esc；转交选另一成员及满包收方 | 售价floor(3price/4)、移槽、末flag80/stale ID保留、送方派生；转交只在有空槽时生效，flag0追加，不触发战场行动或满包交换；各层取消返回页有WGC |
| P3 复活 | 有来源的hidden成员→教会第3项；NO、钱不足1、恰好够钱三种 | 用原四槽Save比较钱、flags整byte清0、HP=max；MP及其它未写字段保持。无候选／列表Esc也取画面；费用系数必须来自实际profile引用 |
| P4 转职 | 等级19与20、ID7、普通目标、特殊道具目标、ID9持90优先分支；YES与NO分别 | 候选域与目标ID正确；道具消耗/不消耗、职业改变、成长ACK先后、level1/EX0与HP/MP恢复；text ID和未知字节保留。动画与费用/写序分别判，不以完整片段未崩溃代替 |
| P5 超员出阵 | 同一准备态17人选择15同伴；选满前Esc、选满后最终NO和YES分别；后期21人作为20部署容量代表夹具 | raw0不变，选/未选分别稳定重排，32人持久表与尾保留；最终NO也保留排序。确认进入stage时只部署允许前缀，不截断持久表。后期完整entry不在本组交付范围 |
| P6 关约束代表 | stage有来源且缺required text时选满，及stage25两次前置重排代表；禁止由普通stage伪造“自然”来源 | 缺失提示657/返回0但保留排序；前置reorder顺序和最终NO后持久顺序可见。仅确认通用条件执行/数据绑定，不开放整关 |

每组报告分开列SDL逻辑／输入（G3实施后）、静态／局部原机器码、DOSBox运行。现阶段P1—P6均为**待原版对照**。若本轮G1需这些证据才能解除某个语义阻塞，必须实际补采，不能把上表当通过收据。

## 8. 后续归属和命名建议

通用服务、候选／价格／取消／等待、转职增量、隐藏店条件分派、NoTown准备流程和出阵排序归G3；它们需要的单位／装备／状态有效域归G2或同阶段前置。商品表、town类型、NoTown／隐藏店启用条件、必选成员和特定前置排序参数可进入S，前提是上述通用执行器已有证据。费用profile0未见正常玩家来源，须按已证来源界定有效域，不能把邻接常量或外部畸形档转成新玩法需求。

建议由主执行者统一复核命名输入、数据库和生成证据：`0x279bc`商店服务、`0x2872b`购买、`0x28cbd`出售、`0x28f65`准备态转交、`0x29daa`教会服务、`0x2a43e`复活、`0x2aa00`转职、`0x2af28`出阵选择、`0x2b777`持久表稳定重排。本文仍用原 `sub_` 登记名定位，不修改共享命名表或手改生成伪 C；新语义发布前按项目规则同时完成命名／实现注释登记。当前已实现服务范围仍以源码支持门为准。

## 9. 追加发现：原模板计数和 placement 索引合同

为确认SD3的12条未配对记录，本轮另取 `sub_1088D @0x1088d`、`field_actor_group_append @0x10b4e`（原登记同名）、`field_unit_stage_template_append @0x10c50`（原登记同名）的完整Domain函数、CFG、指令和计数变量xref。三函数1,932B与相同严格镜像逐字节一致，读集 `build/g1/ida/preparation/initialization-evidence.json` SHA256 `3db9f0631d0405b80f751142dd5808775bab0afe9f2e492c0df986b296ab7eee`；此补读不增加原版运行结果。复现wrapper将上述collector调用改成 `collect_initialization(db, output)`；独立验证命令加 `--initialization`。

证据链为：

1. `0x1098b/0x10995` 将metadata byte1写入部署名额 `dword_53BE7`、byte2写入模板有效计数 `dword_53BE3`。`0x10a5b` 以 `6*53BE3` 为偏移取得placement后续部署位置，再按roster顺序构造玩家活动记录。
2. `0x10bcc..0x10bf6` 按 `i<53BE3` 遍历模板，只在 `metadata+0x83+26*i+0x15` 的group匹配时调用 `0x10c50(i, ...)`。循环界限不来自metadata总文件长度。
3. `0x10c85..0x10c9f` 直接以相同 `i` 读取placement `+2+6*i` 的X低byte、`+4+6*i` 的Y低byte。此路径不读取placement第3个WORD来搜索unit ID，也不按同ID出现次数选择位置。
4. `dword_53BE3` 本轮Domain xref只有启动恢复 `0x10291` 和资源初始化 `0x10995` 两处写入、`0x10a5b`／`0x10bcc` 两处读取。活动存档恢复的有效metadata仍须按对应owner处理；不能由静态初值30推断所有SAV永不变化。

| 原资源组 | 资源长度可容纳模板 | 原metadata byte2 | SD3 ordinal未配对记录 | 原资源正常group消费结论 |
|---|---:|---:|---|---|
| stage0 | 31 | 30 | index30／unit76／group10，共1条 | index30在原group循环外；placement index30属于后续玩家部署区，不能用其unit ID反推可追加模板 |
| stage31 | 31 | 30 | index30／unit76／group10，共1条 | index30在原group循环外；placement总数30，不能把越界补默认坐标 |
| stage32 | 40 | 30 | index30..39／unit96／group255，共10条 | 这10条均在原group循环外；不是凭group255名称判断无用 |

上述分类解释12条为何不具备原正常group消费所需的位置；**不删除资源尾、不将整个来源报告改成全部语义通过**。G1核查时SDL `src/field_unit.c` 的 `placement_for_template` 使用unit ID／ordinal匹配，`metadata.unit_template_count`主要来自原资源长度；它与原版按有效计数／同索引消费不是同一个合同。当时受支持夹具可能因前缀恰好同序而通过，不能推广为跨关正确。

这是G2必须修复的通用初始化缺口：区分保留的完整raw模板区与有效可消费前缀，按原有效计数划分模板位置／玩家部署位置，按原同索引和低byte语义消费坐标，保留有效SAV metadata的owner。不把修复藏为stage0／31／32特判。验收包含同ID重复、placement第3个WORD与template ID不相等但索引位置合法、有效前缀与额外尾、不同部署名额、活动Load有效metadata和失败不写；定向原版fixture比较完整活动表／持久表／坐标／派生和初始化顺序。G1只有静态取证；G2实施与验证边界见下节。

## 10. G2 单位运行时合同

`FD2_FIELD_MAX_UNITS`恢复原96条，单位记录仍为80B。`fd2_field_unit_template_count`从原metadata byte2取有效前缀，并拒绝超过保留模板资源的计数；group追加和Scene初始化按该前缀消费同索引placement的坐标低byte。部署位置由同一前缀终点加roster序号取得，原byte1限制部署名额。资源总条数、完整placement第三WORD和尾部记录仍由原owner保留。追加或构造失败在副本中结束，不清写调用方原有尾部。

对白资源预检单独使用`fd2_field_units_project_group_identity`在借用副本投影身份／位置，不做基础／装备派生或cache登记。这个副本不能提交为真实单位；坏装备、外部hook等错误仍在正式callback原构造写点处理，不因补齐构造器而前移业务失败门。

基础构造保留0..31／68..135域，并加入stage32真实调用的48／66读窗。`unit_base.json`只记录两条alias视图：48→growth+384、66→stage_prepare+68；生成器从唯一冷源投影24B，不新建可编辑基础表，不开放其它32..67构造。模板构造在base之后完成原`0x1b750`的装备派生，成功后才能追加actor；未知基础源或坏装备不会留下部分单位。`field_unit_render_test`直接读取原EXE基础／成长／敌军／装备原窗，对33组资源的1887条有效模板逐项计算基础和四项派生WORD，独立于生产JSON／getter。此为静态原源字段比对，不是1887次原版运行。

`field_event`提供全部106条已核计数流，仍不解除stage／callback支持门。全106流计数长度合计2896B，原指针候选payload仍为2898B；二者分别报告，不改写SD3历史。script24读取13B跨下一候选边界，script84只消费5B、候选余4B保留。96 actor、原pair数组32上限和完整消费校验在执行前完成；actor越界、坏方向／截断不写单位。原mode0不更新朝向；mode80零步保留raw5，所选actor仅在演出工作页上抬12px、等待2tick后恢复正常页，业务格坐标不变。

### 10.1 原cache顺序与活动Load

`fdicon_cache_append_unit @0x11019`按首次出现的unit ID去重，原`sub_127E0 @0x127e0`按`class*12+direction*3+frame`取帧。SDL在`fd2_field_units`旁路保留相同cache序列／runtime class；会话内所有方向优先投影该映射。因而`0x2548c`／`0x34dd0`只写unit ID而未重建cache时，地图仍显示旧cache sprite；raw5跨到下一原class的方向1帧，不能改成unit ID+1或`direction&3`。缺少目标class时明确拒绝该呈现，不猜帧。

原活动Load并不恢复旧cache：`sub_10010 @0x10010`在`0x1035c`清cache计数，`0x1036a..0x1039c`逐raw读取当前`actor+7`调用`0x11019`，在`0x10391`重写原record+2，随后写新的`FD2.TMP`。SDL的`fd2_field_units_sprite_cache_restore`复现该顺序，但将新class写入可重建旁路，保留SAV全部80B及旧raw+2。有效原档、旧SDL全0 class档采用相同Load合同。热改身份前仍旧sprite，保存后Load按新ID重建sprite，是原版自身的可见变化；不承诺跨Load保留旧TMP画面，不新增SAV字段。

该结论另有四函数Domain窄读集：`10010/11019/2548c/34dd0`共2911B与strict LE镜像逐字一致，`build/g2/cache-load-evidence.json` SHA256 `eff244cac03080c8b394489275fbf10018b80b95e31d27f2d4239a3ea10ac84e`。可通过`tools/ida/g2_unit_runtime_evidence.py`的`collect(db, output, strict_image)`复取；工具不保存数据库，原字节报告保持ignored。

### 10.2 有限事件写点和证据边界

`fd2_field_units_field_write`只接受已列举的坐标／方向／frame／flags／阵营／身份／六状态／descriptor／AI／HP／MP字段及SET／AND／OR。`fd2_field_units_ai_behavior_write`复现`0x344f2`的`(old&0xf0)|arg`，不把AI34误当通用状态。参数范围和写点先完整校验，错误不写单位；这些原语不接收任意地址或offset，也不自动隐藏HP0、结算掉落或重算属性。原事件要求的派生、败亡、掉落与ACK顺序仍须在G4领域执行器及S绑定中按原冷图接线，不能用单字段写宣称整段事件已经完成。

raw26视觉复用VGA App级共享tick锚：frame强制0、水平0／1px相位，不改actor坐标或RNG；Scene hidden分支快照使用96条有界数组，避免64位位移截断。signed奖励贡献保留DWORD bits，原signed total低于100（含负数）沿冷图488提示／ACK、11tick关闭／ACK、EX低byte提交，不升级、不降级或重算。低byte回绕可使EX大于99；后续奖励仍以signed total判断，不能按gain负号跳过升级。例如EX95减100写251，再减1仍以total250升级两次后余50。

当前可复核的独立原源检查包括9张原表8528B、两条构造alias窗48B和106条脚本2896B；magic新增四张有限投影另由`verify_magic_contracts`核语义来源，不混算为这里的原字节表。SDL自动回归、局部原机器码与DOSBox/WGC动态对照分别记在G2阶段收据。本文没有把无窗口自测或源码构造批量比对标成新的原版动态通过；stage32 alias、raw5临时页、raw26状态和新增完整构造路径的代表性运行验收仍须由主执行者串行留证。

2026-11-10单位子域的Windows/MSVC Debug定向构建与CTest为6/6：`field_event`、`field_unit_render`、`field_unit_base`、`field_growth`、`field_stage_transition`、`scene_handoff`。其中`field_unit_render`包含上述1887条模板源字段／四项装备派生比对，`scene_handoff`包含96条hidden快照分支及现有开场交接；均不启动窗口。冷定义／来源保护的58项Python回归通过；G2 native夹具模块另有6项完整SAV有限门负例通过，仅是oracle自检。Linux、DOSBox/WGC及真实输入不由这些结果代替；G2整体状态仍以统一计划与主执行者实际收据为准。
