#!/usr/bin/env python3
"""合并 IDA 独立分析与历史命名档案，生成规范函数命名登记表。

输入：
    tools/ida-out/ida-name-audit.json    fd2_name_audit.py 的逐行判定
    tools/ida-out/ida-functions.tsv      从已标注 IDB 导出的函数表
    tools/ida-out/ida-annotate-report.json

输出：
    docs/generated/ida-canonical/fd2-ida-function-registry.json
    docs/reverse-engineering/function-names.md 的表格正文（--markdown）

规范主键统一为 LE relbase linear VA；`OFF` 为 object1 内偏移（`VA - 0x10000`）。
"""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tools" / "ida-out"
CODE0_BASE = 0x10000

# 历史档案里被 IDA 复核推翻或需要澄清的结论。
# 原始说法保留在 `records[].purpose/evidence` 中，更正以独立字段附加，
# 便于追溯「谁在什么依据下改了什么」。详见 docs/reverse-engineering/ida-crosscheck.md。
CORRECTIONS: dict[int, list[dict]] = {
    0x184C0: [{
        "target": "evidence", "was": "库存属性图在(108,105)、数字在(130,105)",
        "now": "visible序号按每列4项布局：名称(42+150col,103+22row)，"
               "分类图(13+150col,101+22row)，属性图(110+150col,107+22row)，"
               "bank42三位数(135+150col,107+22row)。原SDL属性图早2px／2行，"
               "数字早5px／2行；原版分类图与名称坐标不变。",
        "why": "published184c0及raw18523/18530分类图、18592的rowY+4、"
               "18598的nameX+0x5d与185a2的nameX+0x44、18722/1872e/18731"
               "名称/行列；START短剑0 AP010、皮甲132 DP008、药草192 HP040"
               "与frames64/65/66及资源相互支持。新真实资源回归覆盖完整底板、"
               "空槽压行和第二列；不把此前只验上半数值的WGC称为库存布局已验。",
    }, {
        "target": "evidence", "was": "无主属性留空或将raw分类/属性图的0当透明",
        "now": "已知无主属性分支在name+(68,4)绘bank31 frame41（18x8），"
               "不是bank93两位溢出。分类/属性raw59..67经1685c/4ed0b完整复制，"
               "0也写入；显示按visible序号，高亮比较raw槽。",
        "why": "完整184c0分支及1685c→4ed0b的memcpy支持；215条物品中27项"
               "使用fallback，原frame41 SHA "
               "87e4afe486b48af10aaf7ef6dbad5a2508f9ec79e0142f96dda24f2137c53a99。"
               "真实名称108..122合法为空，不猜补文本；当前数字RLE41/42..52无opaque0，"
               "仅该已审计缓存保留0为skip。SDL纯绘制回归不代替正式WGC配对。",
    }],
    0x1DB65: [{
        "target": "evidence", "was": "无演出时只需把全部HP0的flags写1",
        "now": "只省略帧，仍保留旋转后的direction终态：若一个非hidden HP0位于"
               "x[camX-1,camX+13]／y[camY-1,camY+9]，13帧循环会把全体非hidden"
               "HP0（含离屏者）方向最终写0；hidden死者方向不改。无屏内见证者则不改"
               "方向；最后全部HP0 flags精确赋1，不能OR原flags或重置所有存活者方向。",
        "why": "已发布1db65的收集门及j0..12全表循环、strictLE object2 "
               "VA51a87/51a8b原始8字节0d00000008000000，与现13x8 viewport相等。"
               "SDL共享清理用于phase/physical/AI magic/item及新player magic，"
               "扩边界与屏内外／hidden组合锁终态；此项不证明旋转／消散帧已实现。",
    }],
    0x1C81F: [{
        "target": "semantic_name", "was": "sub_1C81F",
        "now": "field_magic_damage_apply",
        "why": "1c75e命中后调用的共享伤害core；逐目标消费spread RNG，"
               "两次整数截断形成rawDamage，再HP夹零。敌军ID>=68时读取"
               "4e84f record+9乘目标等级；死亡给全值，未死按rawDamage/maxHP计入53EC8。",
    }, {
        "target": "evidence", "was": "经验可以统一按实际HP差计算",
        "now": "伤害贡献使用未夹HP的rawDamage，死亡分支给完整level*coefficient；"
               "不在此除caster等级、cap99或写EX。保留原IDA物理符号／类型，不据此猜ABI。",
        "why": "完整206B SHA b5b10e142a437411b5c94381747eef65783fa0a1e4b7e11caef3c31d1570ef60；"
               "已发布1c81f及1c75e/2ff01调用链、680B敌表；受控原版unit9杀unit96"
               "实测MP6与EX42，RNG／完整动画仍未对齐。",
    }],
    0x1C916: [{
        "target": "semantic_name", "was": "sub_1C916",
        "now": "field_hp_heal_apply",
        "why": "1c8ed法术／211a4物品共享HP恢复core；一次spread RNG，满HP也消费，"
               "恢复量夹maxHP。ID<75按实际恢复差*40*目标等级权重/maxHP累加53EC8；"
               "只有8<profile<25给目标等级加30，不等同caster归一条件。",
    }, {
        "target": "evidence", "was": "恢复core可以承担费用或caster经验写回",
        "now": "只恢复当前target HP并累计原始贡献，费用由wrapper、caster归一由18d8c负责；"
               "SDL最坏贡献查询复用相同整数公式，所有目标先预检；"
               "SDL另共享纯HP部分供真实magic power与item value调用，"
               "不把其API等同原函数的贡献累计、费用或各动作尾链。",
        "why": "完整199B SHA 2bfd114f863e4e0f16eea866cdd8e699c925e78c1ebf8c131defd539a791778d，"
               "已发布调用者及raw写集；28000组profile/HP/spread逻辑组合检查既有效果与上界。"
               "该组合检查不是局部机器码执行或原版治疗运行。",
    }],
    0x4E84F: [{
        "target": "semantic_name", "was": "sub_4E84F",
        "now": "field_enemy_base_record_get",
        "why": "完整23B getter返回object3 VA61af9+index*10；10c50构造、1c81f/1ecc7"
               "经验以及2f7b6共用。连续68条record的前9字节与SDL既有基础表逐条相等，"
               "末字节为经验系数，不另造stage0默认表。",
    }, {
        "target": "evidence", "was": "敌军基础record只保留9个已用字段",
        "now": "保留同表第9字节经验系数，SDL只支持unit ID68..135，未知ID不猜；"
               "原函数参数为record index，SDL getter的unit ID接口不是原版ABI。",
        "why": "getter SHA 1b865aa8e9782be6ea4d262678f4f034400ef6db45240d9d42122fad53211554；"
               "strictLE object3 VA61af9/file7ab0d完整680B SHA "
               "bc304be826fe3ff0ff129155da846126350852c9b67c6b08a2d7c2d103cc99df。",
    }],
    0x1E292: [{
        "target": "evidence", "was": "未升级动作可省略EX或用WAIT收尾代替",
        "now": "gain非零、caster未hidden且不在等值levelcap时，EX+gain<100分支只写+3c，"
               "不成长／重算／学习。18d8c先按level+(profile>8?30:0)归一，controller"
               "再cap99；升级分支仍待B4，SDL在MP/RNG前以全目标上界拒绝可能升级。",
        "why": "已发布1e292及117e7/18d8c，完整663B SHA "
               "5d9de2950a3c2f9ce7f68142e139d94176a175328ea9867dd3654abe2d3ef75b；"
               "levelcap为ID30/31等于99、其它等于40，不扩为>=。这里只登记已确认分支，"
               "不命名整个成长流程或声称488提示／FIGANI／额外呈现RNG已验收。",
    }],
    0x17AED: [{
        "target": "semantic_name", "was": "sub_17AED",
        "now": "field_unit_detail_present",
        "why": "完整642B与117e7/29620的xref：打开actor装备详情，16c57等任意键；"
               "法术非空时仅卷下页、画1ceed(actor,-1)并再等任意键，最后整页关闭。"
               "F2直接返回，普通Enter的controller后置在调用者；不扣MP或标acted。",
    }, {
        "target": "evidence", "was": "猜测第二actor实参或Esc直接跳过所有详情页",
        "now": "17af8与17c40在不同栈深度读同一初始actor实参，不据伪C的a2写ABI。"
               "out1839b相位0..6，独立只保留上块的空底页present，然后底板frame21"
               "与未选中法术列表，in6..0；最终18409相位0..11。上下块固定86x86/"
               "223x86，底部拷贝310x102包含底板99行外的缓存背景。",
        "why": "published17aed..17d6f及strictLE raw全642B SHA "
               "fdabaa88e2e33df5cb9b8040e9b8b7d3caabf3bbf3c9fba30537968801f471c5。"
               "17bba..17c0f空底页没有额外等待门；两处等待均16c57。此为缓存IDA及"
               "独立raw核对，未新建数据库或重写伪C类型，完整等待RNG/时序另验。",
    }],
    0x1A866: [{
        "target": "evidence", "was": "只恢复毒伤和状态计时，忽略聚焦并合并重算",
        "now": "raw顺序处理匹配side且非hidden的中毒actor，HP扣除后即12d7b聚焦，"
               "不加HP或damage非零门；完整扫描后1db65死亡清理。第二段逐actor逐byte"
               "递减+22..27，每次归零立即聚焦并1b750重算，不能合并到actor末尾。",
        "why": "published1a866..1aa1d与strictLE file4087a全439B SHA "
               "951bd9b8a4ea02aa53a97dc7dbc808cf9673365bed6cda162d1b53349db00b35；"
               "HP写入1a8d0/必聚焦1a8df/扫描后清理1a941；归零聚焦1a9aa..1a9b0，"
               "重算1aa10。镜头是死亡方向清理的前置读集，漏写可持久化错误方向；"
               "此为逻辑顺序核验，提示、演出及其附加等待/RNG未作动态对照。",
    }],
    0x1B8A6: [{
        "target": "evidence", "was": "AI私有重复库存计数，未共享完整raw槽语义",
        "now": "扫描8个raw槽，只有flag bit80决定是否计数；装备位、物品ID及空洞不参与过滤。"
               "SDL将原AI同体实现提为共享查询，NULL0是适配边界，不改原函数ABI。",
        "why": "published1b8a6..1b8e7与strictLE file418ba完整65B SHA "
               "987981c59e7b3801574e75280fc3848d3f829bfdafa4ef09f3a1707ad86318e9；"
               "1b9de读取count后直接按raw索引选择，不能把显示跳空槽变成库存压实。",
    }],
    0x1B8E7: [{
        "target": "semantic_name", "was": "sub_1B8E7",
        "now": "field_unit_inventory_slot_remove",
        "why": "完整75B及190ac/1aa1d/1bbdc/20c6f的xref：memmove后续"
               "(7-slot)*2字节到当前raw槽，最后仅flag+18写80，末ID+19保留stale。",
    }, {
        "target": "evidence", "was": "删除库存可以把末ID一并清ff或只压实可见项",
        "now": "原始后续槽含hidden旗标及未知/stale ID均逐字保留，不按可见行重建。"
               "SDL既有AI helper由玩家草药复用；slot范围检查是适配边界，不猜原ABI。",
        "why": "strictLE完整75B SHA f7165224c5e59503791effde95df6572cff3dc543da65e52d11d44807902162a；"
               "callee3771c为memmove，非memcpy。原版两槽画面已校准，但原版raw16B"
               "仍未动态量测；末槽stale由原码与SDL逐字回归支持。",
    }],
    0x211A4: [{
        "target": "semantic_name", "was": "sub_211A4",
        "now": "field_hp_heal_group_execute",
        "why": "完整98B及20c6f/2d80d的xref：effect13/眨眼后按raw target逐个"
               "调用1c916并显示浮字，经共享21190刷新/数字尾结束；不是item专属。"
               "不负责MP、压槽、acted或玩家controller。",
    }, {
        "target": "evidence", "was": "物品HP恢复可以借一个法术ID或AI尾链替代",
        "now": "接收调用方原power，物品value40与法术record不同；每个target独立"
               "一次spread，1c916返回raw heal供浮字，实际HP差不能冒充该数值。"
               "SDL只接已证实的HP逻辑；完整演出/浮字/音效和附加等待另验。",
        "why": "strictLE完整98B SHA da661db06971bfcbfd2804de28376a52412b5b68183cfd3c2f59f285eb871919；"
               "与1c916完整199B/20c6f玩家herb分支及原版full/HP20两场相互支持，"
               "未改数据库或伪C物理类型。",
    }],
    0x1B932: [{
        "target": "semantic_name", "was": "sub_1B932",
        "now": "field_unit_inventory_select",
        "why": "完整172B及190ac/1aa1d/1bbdc的xref：17e0b打开单位详情、"
               "53C57归0，循环1b9de直到非0，18409按0..11关闭并恢复背景、释放"
               "三个64000B缓冲；返回selected_result!=-1的bool，不是raw槽索引。",
    }, {
        "target": "evidence", "was": "将列表modal归为Use专属或猜测返回选中slot",
        "now": "普通库存与装备等调用共用完整modal；Use列表取消返回0，调用者"
               "18d8c重开ITEM子菜单。等待16c57及眨眼仍可消费共享RNG。",
        "why": "strictLE object1 offb932/file41946完整172B SHA "
               "1d901f35a51e1072c4f208821fb56e5b7675137c74fb8f202c830c8ded150192；"
               "2026-11-05真实START raw0原版WGC校准列表导航、code0拒绝与逐级取消；"
               "不把单端校准作为SDL配对或内存/RNG相等证据。",
    }],
    0x1BBDC: [{
        "target": "evidence", "was": "物品子菜单可以套用MAGIC取消层级或通用库存回滚",
        "now": "表51f05为Use/Give/Equip/Drop的IDs8..11；count非0才打开，"
               "仅Give因无相邻filter3候选禁用。列表取消返回0后18d8c19055重开ITEM；"
               "子菜单取消返回-1后18d8c返回0、18890重开COMMAND。目标取消聚焦"
               "caster再重开列表0。53C53只由Give完成置1，装备/丢弃已写变化不能回滚。",
        "why": "published1bbdc及18d8c/18890调用者，strictLE file41bf0完整1058B SHA "
               "bf0bb20b11c2de14cd1b5612d8d7ed4224e7a22634391de744fa278536a0d3f9；"
               "2026-11-05真实START raw0三槽、不改坐标，原版WGC逐键验证四向选择、"
               "Use列表code0确认拒绝、raw索引回绕、取消与重开。实际使用/目标/"
               "装备/丢弃/Give提交及SDL对应流程仍分别验收。",
    }, {
        "target": "evidence", "was": "草药可借法术ID13或把53EC8清零当成unit EX清零",
        "now": "item192/code5由20c6f→211a4→1c916传原power40；玩家目标读"
               "record+10 cast1/+12 area0/+15 filter1，不借AI的+11。效果后1b8e7"
               "移槽，20c6f/18d8c清全局53EC8，再acted及玩家尾；unit MP与EX保留。",
        "why": "完整20c6f SHA bd7d202cf348a11c5712ab96a02e5cf75f534b8b5aae2bb0447077ecf7e30c2b，"
               "211a4/1c916/1b8e7原码与xref；2026-11-05原版WGC满HP及exact HP20/EX99"
               "派生各使用一次，target取消回列表0，HP42/MP0/EX0或99、两槽详情通过。"
               "未执行Save/END，raw16B/stale尾运行量测、完整UI RNG/演出/声音与SDL配对未验。",
    }],
    0x1B9DE: [{
        "target": "semantic_name", "was": "sub_1B9DE",
        "now": "field_unit_inventory_select_step",
        "why": "完整430B及1b932/1bffe的xref：184c0绘制选中库存、16c57等单键后"
               "更新53C57，返回0继续/-1取消/1确认；非完整modal，也非Use专属。"
               "Up/Down全表回绕，Left>=4减4，Right<=3且index+4<count加4。",
    }, {
        "target": "evidence", "was": "将Use限制猜成只有已实现handler或装备位决定",
        "now": "模式非0仅查当前物品record+0d非0，0模式可确认有效槽；不读MP或法术"
               "禁止状态。SDL提取纯按键部分，不声称整个16c57等待不耗共享RNG；"
               "dense-prefix和全已知ID是SDL支持门，不归给原版，不改raw槽及stale ID。",
        "why": "strictLE file419f2全430B SHA "
               "b47b912b31ed16be618e4a15cfc46e7ffa3aeedc2e7b599084460d1a17dafb07；"
               "该选择器同时被普通列表与装备循环调用，generic unit命名保留此范围。"
               "发布物理符号/伪C类型保持，未写数据库或猜测ABI。",
    }],
    0x1CEED: [{
        "target": "semantic_name", "was": "sub_1CEED",
        "now": "field_unit_magic_list_draw",
        "why": "完整259B及17aed/1cff0/1d51d的xref：枚举actor法术，按每列4行"
               "绘441+ID名称、frame92费用标签和record+5的bank42两位数字。"
               "selected仅切换205/201前景，shadow76、背景透明。普通详情也调用，"
               "故不是player专用；不画底板/姓名MP、不等待、分配或提交。",
    }, {
        "target": "evidence", "was": "中性符号未登记完整列表绘制写集",
        "now": "x=18+100*(index/4)，y=103+22*(index%4)；费用图位于x+50/y+5，"
               "数字x+73/y+5。SDL现有详情合并装备，纯列表整合先恢复底板不等于"
               "原函数自身画底板。IDA原符号与发布伪C保持，未写猜测ABI。",
        "why": "published函数区间1ceed..1cff0与strictLE object1 offceed/"
               "file42f01完整窗口一致；SHA fc1b0cc845879719340b084d927c069d1393efd809085feece0c052c0660558f。"
               "2026-11-04真实START raw1火炎术首项开/取消/重开，31603矩形"
               "资源门通过；全部36名称只含普通glyph/结束，31为空名。",
    }],
    0x1D51D: [{
        "target": "semantic_name", "was": "sub_1D51D",
        "now": "field_player_magic_list_select_step",
        "why": "完整427B及1cff0内1d0cf调用：先1ceed重绘，再16c57读单键，"
               "上下环绕、左右跨4项，索引留53C57；返回0继续/-1取消/1确认。"
               "确认只查MP>=record+5费用，不扣MP、不标acted或施法。开合与循环"
               "在caller，故不是返回spellID的完整modal。",
    }, {
        "target": "evidence", "was": "按旧伪C的void和七个参数推断选择器ABI或纯查询",
        "now": "保留原sub_1D51D物理符号/伪C类型；raw1d68a..1d6c1补全MP与"
               "0/-1/1路径，共享16f04恢复尾。16c57会消费UI RNG，不能声称"
               "取消或列表等待不推进共享随机流。",
        "why": "strictLE object1 offd51d/file43531全427B连续窗口相等；"
               "SHA bf6c1838f60e40612e1af48ee194a468a4edd0442c9971ddd18168d709949d71。"
               "运行仅证实首项MP8/8/cost02的打开、Esc与重开；完整导航和低MP门"
               "本轮仍是静态证据，不冒称原版已执行。未新建或写入IDB。",
    }],
    0x3231B: [{
        "target": "evidence",
        "was": "完整序章脚本已接入，最后对白后的handoff可直接代表自然开场终态",
        "now": "stage0最后fragment2之后还须134e4复位全部方向、12d7b聚焦"
               "actor0，再清资源值。SDL遗漏这段会把最后说话人raw11的焦点和"
               "镜头交给战场；部署调试入口跳过script0也不等于自然开场。",
        "why": "权威3231b尾和strictLE script0/1/2/5写集；新无窗口完整"
               "32→31→0回归不读取START.SAV。原版自然START逐117个资源等待门"
               "及UI Save实测12站位、全方向0、focus(7,14)/camera(0,13)。",
    }],
    0x134E4: [{
        "target": "evidence",
        "was": "全体方向复位尚未登记自然开场的独立收尾写集",
        "now": "field_all_actor_directions_reset只逐actor的record+3写0，"
               "随后delay20；不写frame_phase或walk_frames。原版3231b在"
               "最后对白后、聚焦主角之前调用。",
        "why": "权威134e4及原始函数写集；不是13512的acted标记函数。"
               "新scene收尾复用已有focus helper，不按捕获相机值直接居中。",
    }],
    0x12CEA: [{
        "target": "evidence",
        "was": "左／上达到相对格2就卷动；已列控制夹具足以证明聚焦镜头",
        "now": "field_focus_move_to先X后Y逐格调用四方向。移动前旧relative"
               "左／上严格<2、右>=11、下>=6时才同步卷动；相机受地图边界限制，"
               "target已等于focus时不动，不是固定居中或只夹到可见范围。",
        "why": "权威12cea与原始11b62/11c73的jge、11c19/11bba的jle；"
               "13×8格、每格24px和4px边。旧SDL<=2提前一格，Z直接赋值又"
               "跳过逐格路径；scene已有严格<2版本，不能用测试安全导航替代边缘验收。",
    }],
    0x12D7B: [{
        "target": "evidence",
        "was": "聚焦actor只需使其进入可见范围，Z可直接更新坐标后夹相机",
        "now": "field_focus_move_to_actor读取raw actor的xy后调用12cea；"
               "117e7按flags&85==0且side2选中后，先完成聚焦再推进轮询owner。"
               "END、攻击取消和March的已确认调用复用同一坐标路径。",
        "why": "权威117e7/12d7b及四方向原始分支。cam(5,11)、focus(8,15)"
               "到actor(2,5)最终cam(1,4)，到(20,22)最终cam(9,16)；"
               "地图边缘或same-target不承诺额外一格空白。运行对照另行登记。",
    }],
    0x205B4: [{
        "target": "evidence",
        "was": "stage0结果handler仅有静态调用和SDL分派证据",
        "now": "保留中性sub_205B4：先置53ECC=2，有可见side0则清0，"
               "raw0 hidden覆盖为1；原版不查HP。Continue后由玩家controller"
               "后置评估；受控START只改hidden位可分别到达胜利／败北出口。",
        "why": "权威205b4及117e7后置链；DOSBox WGC的Z→Enter→选择取消"
               "触发22ef6首对白或22e5c败北资源。SDL从同派生快照独立返回"
               "胜／败exit2，不声称输入时点、结算或整关实战相同。",
    }],
    0x22E5C: [{
        "target": "evidence",
        "was": "中性败北callback未登记独立资源门",
        "now": "保留中性sub_22E5C：停音乐、淡出并清屏，以opaque方式画"
               "FDOTHER79 frame0，淡入并等9tick，再画frame1、等36tick。"
               "结果门校验使用完整320×200资源，不据此开放SDL败北后续。",
        "why": "权威22e5c→2eb9f(header+9)及本地嵌套资源；受控败北"
               "快照在原版controller后置到达该页。局部运行与B5后续分开登记。",
    }],
    0x22EF6: [{
        "target": "evidence",
        "was": "stage0胜利callback只作为准备页迁移调用依据",
        "now": "保留中性sub_22EF6：先呈现FDTXT[1]fragment9，之后11506"
               "同步持久表再写stage1。WGC仅校验首说话人4的两行和对话顶边，"
               "没有推进对白证明持久同步或下一关。",
        "why": "权威22ef6及严格LE结果callback表；受控胜利快照经原版"
               "controller取消后匹配2794字形／阴影像素和620顶边像素。"
               "这补本关结果分支证据，整关战斗和结算仍留B5／V1。",
    }],
    0x15311: [{
        "target": "evidence",
        "was": "静态复核未发现actor+0x44扣除，record+5仅作为MP可用性门",
        "now": "本体不直接写MP，但map-effect表的damage/heal/status wrapper在"
               "逐目标循环前调用1ca89扣一次；ID17的226ea实际传cost ID18。"
               "动画路径2ff01→30e9d按caster FIGANI每个part+4==1调用，不保证"
               "所有actor均恰一次。SDL当前修正范围仅为已确认无演出效果分支。",
        "why": "权威1ca89明确word(actor+44)-=record+5；原始1cab7..1cac1复核。"
               "51d01的0..27 relocation表、21227/21b18/226ea等wrapper与"
               "30f80..30f92证实真实调用。START unit9回退FIGANI28仅part6为1；"
               "不能用AI本体浅查或SDL旧mp不变测试推翻下层实际写集。",
    }],
    0x1CA89: [{
        "target": "evidence",
        "was": "中性sub_1CA89的MP消费写集未登记，AI消费被误当成只有gate",
        "now": "保留中性名；输入actor index和cost-record ID，只把"
               "actor+0x44的u16减去magic record+5的u8后写回；不夹0、不重新"
               "检查MP，不消费RNG。map-effect wrapper在target循环前调用，"
               "miss/免疫/状态无变化仍扣；目标选择取消尚未进入该helper。",
        "why": "权威伪C和原始1ca89..1cac7逐指令一致，唯一单位写指令为"
               "1cac1的mov word[actor+44],bx。15311和1cff0共用wrapper，"
               "动画30e9d另由part标记触发；静态写集不等于完整实机施法验收。",
    }],
    0x29300: [{
        "target": "evidence",
        "was": "普通战场的四槽手工存取dispatcher，可作为stage0菜单入口",
        "now": "本函数属于城镇准备页的酒店，selector0为持久成员详情、"
               "1为2968d手工Save、2为2986f手工Load；不替换战场secondary19df7"
               "活动存取。实际可见入口与出阵恢复归B5，B1只完成归属研究。",
        "why": "26152→2670e的selector0→29300；FDTXT495..499为酒店/武器店/"
               "出口/道具店/教会，513为进入战场确认。原始293fb/2940e/29424"
               "分别调用29620/2968d/2986f；未冒称该准备页完整运行验收。",
    }],
    0x29620: [{
        "target": "semantic_name",
        "was": "field_manual_slot_save",
        "now": "sub_29620",
        "why": "撤回旧误名，采用中性标识以覆盖历史save语义；完整109字节只"
               "设置53F5E并循环27d33成员选择→17aed详情，无文件读写、checksum"
               "或四槽picker。真正writer入口是2968d，不是Watcom序言/body拆分。",
    }, {
        "target": "evidence",
        "was": "选择并写入四槽；依据却指code0x1968d",
        "now": "准备页的持久roster成员选择与详情循环；取消退出，原save注释"
               "挂错入口，不能沿用。保留原误名供追溯，不据此保存新的猜测ABI。",
        "why": "已发布29620..2968d调用27d33/17aed；原始293fb的selector0调用。"
               "2968d另有独立Watcom序言和完整writer写集，地址必须分别登记。",
    }],
    0x2968D: [{
        "target": "evidence",
        "was": "中性sub_2968D未独立登记，writer语义误挂29620",
        "now": "保留IDA中性sub_2968D：真正四槽手工writer，读取完整SAV后经"
               "29bcb选择，复制准备owner整张0xa00持久表并写meta0..9、checksum"
               "和XOR；未使用roster尾与未知meta保留，不从active表重建。",
        "why": "2968d独立入口、2940e/26331调用；原始29757..29791写当前stage、"
               "roster_count、资源和camera/effect/music/SFX，成功FDTXT660。"
               "22ef6先11506同步持久表再stage=1，main25e2a随后进入准备页，"
               "正常首个四槽保存阶段是1；没有stage0正式生成链或实机样本。",
    }],
    0x2986F: [{
        "target": "evidence",
        "was": "恢复手工slot后直接重建当前战场",
        "now": "只恢复城镇准备owner的完整持久表、stage/资源/配置，重建"
               "FDICON缓存及DS:3f56；出阵后才调用stage entry重建地图、回合和"
               "事件状态。仅stageFF判空，再按523e7表拒不兼容章节。",
        "why": "原始29928..29941的stageFF/表门与完整2986f写集，函数不调用"
               "10010/1088d/205da；FDTXT478成功、479为不能在商店内读取。"
               "slot没有active、turn或camera快照，坏count/checksum原版安全拒绝未证。",
    }],
    0x29BCB: [{
        "target": "evidence",
        "was": "picker(buffer,initial_slot)，第二参是初始槽",
        "now": "选择来自共享53C57，三种caller首次均先置0；第二参选择"
               "16c57规范化等待读键或直接BIOS读键，不写初始index。上下限0..3，"
               "Enter/Space确认、Esc取消；SDL自有initial_slot不是原版ABI。",
        "why": "完整29bcb分支只测试第二参以选择读键路径；25ebb、2968d、2986f"
               "在调用前清53C57。29ab2按119+19*i绘四行，使用FDTXT549、514、"
               "514+stage和550+stage。保留现函数名，不保存猜测参数原型。",
    }],
    0x10010: [{
        "target": "evidence",
        "was": "活动Load后的焦点轮询起点可能沿旧session继续",
        "now": "成功活动Load尾把53AE9置0，下一次Z从raw0开始扫描；"
               "不是SAV字段，失败加载不交接owner。SDL同时在交接前检查可见actor"
               "完整sprite帧组，并复用现有纯胜败查询，避免恢复门重复漂移。",
        "why": "权威sub_10010及原始10602清53AE9；真实START与坏ID、成功/失败"
               "原子边界回归。sprite预检是SDL显式安全边界，不冒称原版坏ID安全拒绝。",
    }],
    0x11019: [{
        "target": "evidence",
        "was": "snapshot角色图形只需当前idle帧可用",
        "now": "fdicon_cache_append_unit按unit ID构造完整12帧组；SDL普通renderer"
               "只跳hidden和越界帧，不使用F1的unit121/race10过滤。可见记录需完整"
               "12帧资源范围；hidden未知ID继续按原保留策略保存。",
        "why": "原始1106a乘12、110fa/11193循环12次与真实FDICON1680帧。"
               "合法边界139、坏ID140/255、hidden/race10/121区别有回归；不改decoder owner。",
    }],
    0x2000A: [{
        "target": "evidence",
        "was": "F1/PageUp辅助页可能是部队列表或另一种战况总览",
        "now": "独立的全场缩放地形图，使用原始terrain cache；开7帧、稳态标记、"
               "关6帧后普通重绘。F1/PageUp直接返回，无玩家controller提交。"
               "页面期间不推进water，尾部普通重绘恢复一次；当前stage0门为SDL能力限制。",
        "why": "权威sub_2000A调用链、117e7扫描码3b/49、原版完整WGC研究页；"
               "1399c清缓存后RLE解码、1f558地形采样、1f6ef标记及尾11cac(0)。"
               "不声明精确开合时间、首次未初始化tick或完整BIOS队列等价。",
    }],
    0x1F558: [{
        "target": "evidence",
        "was": "F1缩放采样、黑边与动画中心未完整登记",
        "now": "tile跨度0xc00、像素余数右移7；清64000后仅画(4,4)的312×192。"
               "开phase1..7中心系数(phase+1)/8，关6..1，不补phase0采样；"
               "stage0最终step768、地图96×96，固定原始tile而非动画替代帧。",
        "why": "已发布field_view_render_scaled及独立原始tile oracle，负坐标floor、"
               "黑边/零色/全64000索引与真实START回归；terrain owner不替换。",
    }],
    0x1F6EF: [{
        "target": "evidence",
        "was": "F1单位标记门、颜色和脉冲尚未登记",
        "now": "stage0每tile4px，标记3×3；按raw顺序排hidden/unit121/race10，"
               "不加HP/acted/AI门。side0/1/2基色32/80/72，focus灰24最后覆盖。"
               "phase7..0,0..7,7端点重复，原BIOS差2，SDL约110ms且不推进玩法。",
        "why": "原始object2表5208a和sub_1F6EF循环、原版空格/actor0叠加页；"
               "F1 marker过滤不推广为普通actor renderer规则。",
    }],
    0x4E310: [{
        "target": "evidence",
        "was": "未登记BIOS时钟读取的实际宽度",
        "now": "中性sub_4E310读取BDA46c低16位tick，不按伪C MEMORY宏猜32位ABI。",
        "why": "已发布入口及局部原始xor eax,eax; mov esi,46c; lodsw。"
               "保留IDA中性名；SDL使用55ms宿主近似，不宣称硬件精确时钟。",
    }],
    0x4E31C: [{
        "target": "evidence",
        "was": "未登记水色DAC更新，可能把FDOTHER初始bank当运行中cycle0",
        "now": "unsigned低16位差>=2才先递增phase，16回0；固定RGB6滑窗只写"
               "DAC224..239，结束重读last，一次调用不追赶。状态60000/60002"
               "属于app视觉owner，Load不重置；F1本体暂停，尾普通重绘恢复一次。",
        "why": "原始4e323/4e32e/4e330/4e359/4e373及全部fixup引用。"
               "严格LE canonical60003有效93字节，后45恰重复首15个RGB，"
               "16个滑窗与C循环表一致；FDOTHER初始各分量少1，不能冒记cycle0。"
               "当前接普通等待、总览稳定及步态；剧情/通用对话/timed wait未全覆盖。",
    }],
    0x4E390: [{
        "target": "evidence",
        "was": "起点邻敌时也按ZOC禁止展开",
        "now": "起点直接写预算并进入递归，允许先离开起点；只有进入的邻格"
               "按ZOC位停止继续扩展。友方终点阻断与可经过规则不变。",
        "why": "原始4e422写起点预算、4e424无条件调用4e42c；"
               "进入格门4e4be在4e4ea..4e4f0按bit80清剩余预算。"
               "SDL双方分组回归及无副作用检查；对应近敌原版运行仍待单独取证。",
    }],
    0x4E4F6: [{
        "target": "evidence",
        "was": "普通路径起点与进入格是否同受ZOC门尚未区分",
        "now": "初始化起点后直接递归，起点不经进入格ZOC门；"
               "normal/mode1的SDL实现已有该行为，不额外更改。mode2 hostile witness独立。",
        "why": "原始初始化/递归调用与4e680进入格门；合成回归检查normal、"
               "long route、witness和非法起点拒绝。公开API另补坐标/三倍缓冲分配界检查，"
               "不据此声明原版完整ABI或全函数机器执行通过。",
    }],
    0x4E8A5: [{
        "target": "evidence",
        "was": "580字节成本表来自file0x7a64a，已与历史runtime线性0x1b3646一致",
        "now": "getter operand0x4e8b4的LE fixup指向object3+0x1646，"
               "canonical0x61646..0x6188a共29×20字节；严格LE页映射"
               "对应file0x7a65a。旧表提前16字节，175/580字节不同。"
               "正确SHA256=4002a997ddd7c1e036461c7ee2b915560587f1653aae6c5978bd47d84f533935。",
        "why": "已发布field_movement_profile_get的乘数20、原始operand/fixup、"
               "既有parse_le/parse_objects/build_relbase_image及独立全表比对。"
               "本轮未取新selector/base/runtime dump，撤回旧表已通过运行时比对的结论；"
               "不重新开启已通过的全局静态LE fixup验收。",
    }],
    0x14B78: [{
        "target": "evidence",
        "was": "先排序合法终点再探路；long route遇不可达即停，tie为abs(dx-abs(dy))",
        "now": "先normal probe原请求anchor，失败后以预算28/mode1仍探该anchor；"
               "完整扫描long route，以未做友方终点阻断的distance记录最后可达格，"
               "未命中继续。随后才range/friendly block/sort/final path。"
               "tie为abs(abs(dx)-abs(dy))，相等保留扫描先项。局部profile先immune→19，"
               "再text_id28→1，预算0合法且无HP门，不修改原actor profile。",
        "why": "权威伪C及原始局部指令14c3a/14c7d/14cd7/14d17/14d47。"
               "14e0c push使ESP=S-4，14e15 [esp+8]覆盖S+4为abs(dx)，"
               "14e19才弹参；14e27读回该值，旧非对称解释漏算ESP变化。"
               "新SDL合成回归与真实START四玩家几何对照，不等于整函数机器执行或March运行验收。",
    }],
    0x127E0: [{
        "target": "evidence",
        "was": "未区分战场单actor绘制与全表绘制、COMMAND末尾遮挡顺序",
        "now": "只按传入index画一个actor，位置相对camera且上移6px，"
               "frame由direction/cache class/idle或walk组成。acted调用4e1a6映色。"
               "COMMAND四图标后仅再画focus actor，不重铺terrainoverlay；"
               "raw26另有横向抖动与frame门，本SDL切片仍未实现该视觉分支。",
        "why": "已发布sub_127E0无全表循环、stride456和dest常量30168，"
               "sub_127A9负责全表与遮挡；179d5末尾call127e0。真实原版"
               "COMMAND截图上图标底6行由FDICON actor0 frame1覆盖，完整"
               "20行资源合成476像素bad0、delta<=3；不是截图缺失。",
    }],
    0x4E1A6: [{
        "target": "evidence",
        "was": "未登记已行动actor的专用RLE映色",
        "now": "对实际写入pixel映为(color&7)+24，RLE skip保留底图；"
               "不能把cursor一同映色，或把合法opaque0当skip。当前真实FDICON"
               "1680帧没有opaque0，SDL可在临时栈buffer映非0缓存，不改bank。",
        "why": "已发布sub_4E1A6、127e0按flags signbit选择它与normal4e22a；"
               "原始RLE独立审计严格区分repeat/alternate/literal写0与skip，"
               "全部帧span/end校验通过，配套可再生产审计与回归，不新增mask框架。",
    }],
    0x179D5: [{
        "target": "evidence",
        "was": "COMMAND图标可以作为actor之后的最后一层",
        "now": "四向图标后调用sub_127E0重画当前focus actor；actor头部"
               "覆盖上方图标底6行。只重画一个actor，不重新画全表或地形遮挡。",
        "why": "已发布field_command_menu_draw调用链、单actor绘制与原版"
               "移动后COMMAND WGC；14行presence与完整20行遮挡绘制分别验收。",
    }],
    0x115B6: [{
        "target": "evidence",
        "was": "target轮换可以沿用browse的持久focus索引或筛除unit121",
        "now": "本次selector局部索引入口为0；非空候选先读list[0]，Z／keypad5"
               "先递增并回绕后读对应槽。unit121仍占轮询槽，只跳过focus helper，"
               "保留当前镜头／焦点；无候选时不轮换。普通法术0／13的确认检查"
               "cast mask及filter目标，方向移动本身不限制在mask内。",
        "why": "已发布115b6和strictLE完整561字节SHA "
               "d8e63ca0aaba31c73d2efd6ad8b74f626464f908862a3a68c24ef086ff015fd4；"
               "115cf清ESI、115ee..11624首槽、1176b..11788递增回绕／121门。"
               "2026-11-05 controls原版WGC另证普通13首目标与frame1；"
               "Z及121分支本轮仍仅静态／SDL回归，不冒称原版运行。",
    }],
    0x18890: [{
        "target": "evidence",
        "was": "玩家路径逐格执行cell handler，行动尾先格事件后acted",
        "now": "入口清pending及物品交接标志53C53；完整路径只收集match0，"
               "随后COMMAND。下级取消返回0重开COMMAND并保留pending；普通COMMAND"
               "取消恢复起点并return0，由controller重入入口清pending。"
               "成功COMMAND先acted，再查询最终match1，返回controller后置。",
        "why": "已发布玩家函数及原raw 0x188b3/0x188bd入口清零、"
               "0x18a52调用共享路径、0x18b0c/0x18b66最终lookup；"
               "controller 0x11942..0x1194d重入。53C53=1只确认成功物品交接"
               "路径@0x1bfa7，未扩为普通攻击或默认状态。",
    }, {
        "target": "evidence",
        "was": "START角色9可移动四格后在同一回合直接施法",
        "now": "非零且非ff路径播放后，unit ID不为18／19／34时将disabled[1]置1，"
               "因此MAGIC禁止；零步分支不写此门。旧四步施法预研路线撤回。",
        "why": "已发布18890与raw18a38..18a7c：18a5f读unit[7]，"
               "18a63/68/6d比较三个例外，18a72写[ESP+4]=1，18a7c传同一disabled表；"
               "18d8c的空法术／+27状态也写此槽。0x18a38..0x18a8c的84字节"
               "SHA 95150fbbe7d8341b73f691ca9ecbbc5cbfb8172d0f11033f003bb719ac1c4fa6。"
               "SDL列表状态回归分别检查零步、例外及相邻ID；不从合成能力推断自然学习。",
    }],
    0x18D8C: [{
        "target": "evidence",
        "was": "成功玩家动作的acted与cell处理时点尚未区分",
        "now": "WAIT仅零步时做原地恢复，再独立cell-action、acted；移动后WAIT"
               "跳过恢复。普通攻击成功先战斗／败亡等已确认尾，再acted。"
               "攻击目标取消返回0并保留移动及pending，普通COMMAND取消返回-1。",
        "why": "已发布command与raw 0x19077路径长度门、0x19082恢复、"
               "0x1908b独立cell-action、0x19094 acted、0x18ff3攻击acted、"
               "0x18f97..0x18fae目标取消；不以pending替代尚未实现的特殊cell-action。",
    }],
    0x117E7: [{
        "target": "evidence",
        "was": "玩家acted／格事件／stage与自动phase可按通用动作尾提前分派",
        "now": "玩家函数返回后render→经验→stage→13565必要整轮phase→读取当前"
               "共享pending并dispatch→清pending。phase内AI可清空／覆盖同一slot，"
               "没有跨phase保护玩家事件的私有队列。selection取消也进入该后置门。",
        "why": "已发布controller及raw 0x11951 render、0x1196d经验、"
               "0x1197b stage、0x11985 phase、0x1199c dispatch、0x119a6 clear；"
               "AI outer handler返回后不清slot，只在下个raw actor起点清，"
               "最终值可能回到本controller。B2/B3/B4未实现语义仍单独登记。",
    }],
    0x13565: [{
        "target": "evidence",
        "was": "自动换phase可以用带HP过滤的remaining_units判断",
        "now": "仍存在(flags&0x81)==0、side==2、record+0x26==0的actor就"
               "不换phase，否则调用turn-cycle；没有HP或bit0x04过滤。",
        "why": "已发布sub_13565与原始函数四条件，controller在pending分派前"
               "调用；原地取消也需经过同一后置检查，不借AI可行动门替代。",
    }],
    0x13A44: [{
        "target": "evidence",
        "was": "AI 不查沿途格事件，仅在 common tail 查询最终格；lookup 可视为立即 handler",
        "now": "只在命中时写单个 pending code（DS:0x1a8f），未命中保留旧值；"
               "不调用 handler、不追加事件完成记录。四方向单格尾都以 match0 "
               "调用，AI common tail 另以 match1 查询最终格，命中才覆盖沿途pending。",
        "why": "已发布伪 C 的 lookup 与共享播放器，raw唯一pending写点 "
               "0x13a95；0x13167..0x1317a push0/focus_y/focus_x/call13a44，"
               "四方向和镜头分支均汇入该尾，未匹配路径直接ret。"
               "修正历史假设，不变更原资源或数据库；原版具体格事件运行另验。",
    }],
    0x13488: [{
        "target": "evidence",
        "was": "可把 AI 路径视为 event-free，玩家路径每步立即分派格事件",
        "now": "共享方向播放器完整走路径，每步更新match0 pending；"
               "循环不检查pending、不分派handler，不在第一次命中停步。"
               "最后一次匹配保留至调用者后置分派，后续未匹配不清空。",
        "why": "已发布 field_actor_path_play 与四方向调用；raw down "
               "0x13008→0x13167，up 0x13310→0x12ff0 后同尾，"
               "right 0x1345b→0x1314f 后同尾。@0x14b78 selector1"
               "选择side/ZOC/path策略，不切换到另一个无lookup播放器。",
    }],
    0x13A9F: [{
        "target": "evidence",
        "was": "AI cell lookup只会产生最终格事件，物理／法术／物品事务可自行提前分派",
        "now": "normal common tail顺序为最终格match1 lookup→acted→全表方向reset；"
               "side outer随后按当前pending调用handler。最终未命中仍可保留沿途match0。"
               "behavior8显式early return继续跳过common tail。",
        "why": "已发布AI函数与side1/side0 outer，raw 0x13e6c..0x13e88 "
               "确认尾顺序；side1 0x1d85d..0x1d876同样传selector1，"
               "与行军共享移动播放器。未知handler只在真实分派边界冻结，"
               "不把已提交动作或外部RNG回滚成未发生。",
    }],
    0x1AF1E: [{
        "target": "evidence", "was": "未登记总览主体关闭相位",
        "now": "从静态源(75,37)复制170×117；phase<5时目标x=75-50*(4-phase)，其余为75，负x裁切时同步推进源。",
        "why": "已发布 sub_1AF1E 与 sub_1B1E7 反向11..0调用链；只读静态矩形复制，无关卡或玩法转移。",
    }],
    0x1AF99: [{
        "target": "evidence", "was": "未登记总览主体打开相位",
        "now": "从静态源(75,37)复制170×117；phase<=4时目标x=75+50*(4-phase)，其余为75，右侧按屏幕裁切。",
        "why": "已发布 sub_1AF99 与 sub_1B1E7 正向0..11调用链，FDOTHER[5] frame134；不是详情页或关卡转场。",
    }],
    0x1B019: [{
        "target": "evidence", "was": "未登记总览顶部相位",
        "now": "102×17顶部栏phase<3不绘，3..7目标y=19-6*(7-phase)，其余y19；上边裁切同步推进源。",
        "why": "已发布 sub_1B019、开合两处调用与frame133尺寸，源固定(109,19)；精确时序另验。",
    }],
    0x1B0AD: [{
        "target": "evidence", "was": "未登记总览人数栏相位",
        "now": "170×16人数栏phase<5不绘，5..9目标y=155+9*(9-phase)，其余155；原phase5复制高度-9，signed循环不复制。",
        "why": "已发布 sub_1B0AD 与已确认原指令的负高度循环，源(75,155)、frame135；不能通用裁切补成理想可见行。",
    }],
    0x1B14B: [{
        "target": "evidence", "was": "未登记总览资源栏相位",
        "now": "63×15资源栏phase<8不绘，8..11目标y=172+4*(12-phase)；phase8高度-12不复制，phase11仍在176，稳定页回172。",
        "why": "已发布 sub_1B14B 及object1 0x1b19e lea高度、0x1b1de/0x1b1e0 signed cmp/jl；源(129,172)、frame136。",
    }],
    0x1B1E7: [{
        "target": "evidence",
        "was": "战况总览尚未登记开合、背景和关闭键边界",
        "now": "双64000字节缓冲保存最初干净战场与四块静态页面；"
               "打开0..11、关闭11..0，稳定页继续地形／actor动画，"
               "不画光标／信息面板、不提交玩法；任意普通键关闭并清缓冲键。",
        "why": "已发布 sub_1B1E7 及五个矩形合成 helper 的调用链、"
               "FDOTHER[5] 133..136 资源；0x1b14b 原始指令 "
               "0x1b19e/0x1b1de/0x1b1e0 确认资源 phase8 高度为负且 "
               "signed jl 不复制，人数 phase5 同样不复制。保留中性名，"
               "未改数据库或权威伪 C；精确帧时序与完整画面另验。",
    }],
    0x4E381: [{
        "target": "semantic_name",
        "was": "vsync_wait（等待垂直同步）",
        "now": "bios_keyboard_buffer_clear",
        "why": "原15字节 56 be1a040000 66 8b06 66 894602 5e c3 "
               "将 word[BDA 0x41a] head 复制至 word[BDA 0x41c] tail；"
               "无端口读取或等待。@0x1b1e7 在关闭前后调用它清键盘队列。"
               "原登记 FUN_0004bac9／历史0x73595 仅用于追溯，不保存猜测ABI。",
    }],
    0x1B41D: [{
        "target": "evidence",
        "was": "未登记总览内容，旧 secondary 说明将其描述为逐 actor 属性列表",
        "now": "战况与胜败条件总览绘制：FDOTHER[5] 的 133..136 四底板，"
               "关卡、回合、八位资源、side 0/2/1 人数及 FDTXT[0] 的 "
               "597+2*stage／598+2*stage 条件。stage 0 为敵全滅／索爾死亡。"
               "SDL 此切片只开放 stage 0 的纯绘制，完整预检后才写画面。",
        "why": "已发布 sub_1B41D 伪 C 与 sub_1B5F1 调用，FDOTHER[5] "
               "原始四帧尺寸、数字 bank 31/42 与 FDTXT[0] 两段只读解码；"
               "ui_number_draw @0x187d6 八位分支取格式化结果前八字符。"
               "保留中性函数名，未修改数据库或权威伪 C；原版页面 WGC 另验。",
    }],
    0x1B5F1: [{
        "target": "evidence",
        "was": "未登记总览三阵营计数的过滤门",
        "now": "逐 active actor 统计指定 side，排除 hidden bit 0、unit_id "
               "121、record +0x1f 为 10；不按 HP、acted、AI 门或持久 roster "
               "过滤。真实 START 的 side 0/2/1 为 7/4/0。",
        "why": "已发布 sub_1B5F1 的完整四条件与 sub_1B41D 三处调用，"
               "只读 START 的 12 条 active 数据（其中 actor9 hidden）独立核对；"
               "SDL 计数、布局和错误无画面写入回归。未开放其他关卡或重命名 IDB。",
    }],
    0x4ED7A: [{
        "target": "evidence",
        "was": "SDL 字形阴影向右／正下，逐像素填背景，并以 shadow=0 关闭阴影",
        "now": "原版背景参数低 byte 为 0 时保留底图，非零时先预清 "
               "16×16；glyph 10 在可选预清后跳过字模。每个置位像素写 "
               "fg(x,y)、shadow(x-1,y+1)、shadow(x,y+1)，0 色也写入。"
               "SDL 保留 bg<0 透明、bg>=0 显式预填的既有接口；"
               "原版 background_arg=0 的调用应传 SDL bg=-1。",
        "why": "已发布伪 C 52187..52203 与原始 object1 指令："
               "VA 0x4edc0 条件跳过预清、0x4edeb 跳过 glyph 10，"
               "0x4ee01/0x4ee03 令 ebx=stride-1，0x4ee1e/0x4ee20/0x4ee23 "
               "依次写 fg 与两处阴影。九处现有生产调用均为 "
               "shadow=0x4c、bg=-1；未重命名数据库或改写权威伪 C。",
    }],
    0x112A5: [{
        "target": "evidence",
        "was": "持久玩家构造与活动 stage 模板构造尚未区分完整初始字段",
        "now": "本函数在独立 roster 末尾调用 sub_1145A @0x1145a；"
               "从 object3 0x61da1 的 32 条 24-byte base 与 0x620a1 的 "
               "11-byte growth 读取初始等级、4-byte 法术位图、装备及库存。"
               "合计写 record 58 字节，保留 00..04、17、19、28..30、"
               "32..36、3d；3b 为 movement，确实写入。SDL 仅成功时提交记录。",
        "why": "已发布权威伪 C sub_112A5（661..729 行）与 sub_1145A"
               "（734..770 行），由 sub_4E838/sub_4E821 取表；"
               "LE page map 只读提取并逐行复核全部 32 条原表。"
               "派生函数只对 flags &0x40 的装备累计 AP/DP/HIT/EV；"
               "初始状态已清零，可复用现有装备派生。保留中性函数名，"
               "未新建 IDB 或修改权威伪 C。",
    }],
    0x37910: [{
        "target": "semantic_name",
        "was": "vga_clear（清屏 memset 0xa0000）",
        "now": "memset",
        "why": "IDA FLIRT『Watcom v9-*1.5 32bit DOS runtime』直接命名为 memset；"
               "调用点传入任意目标缓冲，不限于 0xa0000 显存。",
    }],
    0x3706E: [{"target": "semantic_name", "was": "mem_alloc", "now": "malloc",
               "why": "Watcom runtime FLIRT 符号"}],
    0x3776E: [{"target": "semantic_name", "was": "mem_free", "now": "free",
               "why": "Watcom runtime FLIRT 符号"}],
    0x37324: [{"target": "semantic_name", "was": "file_open", "now": "fopen",
               "why": "Watcom runtime FLIRT 符号"}],
    0x373CA: [{"target": "semantic_name", "was": "file_read", "now": "fread",
               "why": "Watcom runtime FLIRT 符号"}],
    0x37940: [{"target": "semantic_name", "was": "file_seek", "now": "fseek",
               "why": "Watcom runtime FLIRT 符号"}],
    0x3759C: [{"target": "semantic_name", "was": "file_close", "now": "fclose",
               "why": "Watcom runtime FLIRT 符号"}],
    0x16F55: [{
        "target": "evidence",
        "was": "selector 3 已确认结束回合，但确认框按钮资源与布局尚未登记",
        "now": "selector 3 使用通用 sub_19953 @0x19953：默认选择索引 0，"
               "Left 选 0、Right 选 1，Up/Down 无分支。object2 VA 0x51eed/0x51ef1 "
               "的 dword 为 16/17，对应 FDOTHER[2] 的 YES 与 NO!；普通帧 "
               "48/51、高亮帧 49/52 均为 24×16。四步展开后，320×200 屏幕"
               "按钮左上角分别为 (232,168)/(264,168)。仅选中项叠加 "
               "dword_53C13/2，计数每 2 BIOS tick 在 0..3 循环；非选中项"
               "始终使用普通帧。sub_197E5 @0x197e5 使用同值表 0x51ee5/0x51ee9 "
               "收回按钮。",
        "why": "复用已发布权威伪 C 的 @0x16f55 调用链与 sub_19953 "
               "（6916..7028 行）及 published fd2-functions.tsv 边界；"
               "只读按现有 LE parser 提取原版 object2 表并核对 FDOTHER[2] "
               "raw 像素。屏幕坐标由 buffer 偏移 0x1a599+3±16、view 源 "
               "32904/stride 456 与目标屏幕 (4,4) 换算。保留中性 helper 名，"
               "未新建 IDB、改写伪 C 或发布新的 IDA 逆向基线。",
    }, {
        "target": "evidence",
        "was": "selector1批量行军尚未确认，可能只是结束回合或自动战斗",
        "now": "417确认、418提示后保留绝对focus为固定anchor，按raw顺序仅处理"
               "flags&0x85为0且side2的actor，无HP门。每人focus→清pending→"
               "14b78(anchor,index,1)→dispatch当前match0→全表direction0→acted，"
               "无最终match1，无路也走尾。整批后才turncycle，无自动Save。",
        "why": "权威伪C及17104/1712d/17137/1713d/17154写序；"
               "17092 push0xc8→17097 call3790a→FLIRT __delay3e01d确认名义200ms，"
               "不声明宿主/DOS精确时长。51A83/51AAC在March及整轮phase关闭，"
               "SDL用私有呈现参数和play局部续行恢复，不增session/pending队列。",
    }],
    0x19DF7: [{
        "target": "evidence",
        "was": "selector 0 总览逐行显示 actor 属性",
        "now": "selector 0 调用 sub_1B1E7 @0x1b1e7，显示胜败条件、"
               "关卡／回合、资源与三阵营人数，不列出 actor 属性；"
               "静态内容、开合输入与原版运行分别验收。",
        "why": "已发布 sub_1B41D @0x1b41d 的四块绘制与 sub_1B5F1 "
               "计数门，FDOTHER[5] 133..136 和 FDTXT[0] 597/598 资源，"
               "纠正历史说明，不手改生成文件或伪 C。",
    }, {
        "target": "evidence",
        "was": "SDL 活动快照只覆盖单位、cell state 和 meta，未登记开头 metadata owner",
        "now": "原版 Save 先把 dword_53A55 的 2211（0x8a3）字节 metadata "
               "写到 FD2.SAV 明文开头，再把独立的 2560 字节持久 roster 写到 "
               "+0x8a3。活动恢复 sub_10010 @0x10010 原样读回 metadata，并用 "
               "metadata[0]*2 选择 FDSHAP。stage 0 的 FDFIELD[1] 已用长度为 "
               "937（0x83+31*26）字节，首三字节为 00 04 1e。SDL 导出须从"
               "当前 session 序列化头部、turn/cell 表和完整模板字节，不能"
               "沿用旧 stage 的 prefix；未使用 metadata 尾由目标容器保留，"
               "完整 roster（含未用 raw 尾）由 session 恢复并整表写回。"
               "+1/+2 不赋予未经确认的玩法语义。",
        "why": "复用已发布权威伪 C 的 @0x19df7 writer（7143..7145 行）与 "
               "sub_10010（68..90 行）读回／shape 选择；FDFIELD.DAT[1] 原始"
               "长度和首部只读复核。原版 WGC 对照暴露旧 stage 29 prefix "
               "1d 14 46 与活动 stage 0 混用；未新建 IDB 或改写伪 C。",
    }, {
        "target": "evidence",
        "was": "活动 meta[9]（DS:0x3bfb）被当作当前 actor count 的副本",
        "now": "meta[1] 为当前战场 actor count（DS:0x3beb，格式容量 96）；"
               "meta[9] 为独立持久 roster count（DS:0x3bfb，0x8a3 起 "
               "0xa00 字节、32 条 0x50 记录）。二者不要求相等，也不以"
               "当前 side 2 actor 数量或 active count 为 roster 上限。",
        "why": "已发布伪 C：sub_112A5 @0x112a5 在 dword_53bf7 + "
               "80*dword_53bfb 追加记录并递增 roster count；sub_11506 "
               "@0x11506 对 active actor 遍历独立 roster 同步；@0x19df7 "
               "writer 分别写 meta[1]/meta[9]。只读 START.SAV 的两个值为 "
               "12/4，FD2.SAV 为 27/31，反证 count-copy 与 roster<=active "
               "假设。保留中性函数名，未新建 IDB 或修改权威伪 C。",
    }, {
        "target": "evidence",
        "was": "SDL 将 secondary 肯定离场的 -1 建模为返回标题",
        "now": "selector 3 肯定后呈现 FDTXT[416]，返回 -1，经 "
               "field_empty_focus_menu_execute @0x16f55 和控制器 @0x117e7 "
               "传至 main @0x25bf4，结束 DOS 游戏；不会重新进入标题。"
               "SDL 使用独立 LEAVE_GAME，与宿主关闭保留不同来源，均正常退出 0。",
        "why": "已发布伪 C 调用链与原始 object1 @0x1a301 的 "
               "b8 ff ff ff ff 返回值，区别于取消 @0x16fd8 的返回 1；"
               "2026-11-04 真实 START 的 WGC 离场输入与自然退出收据，"
               "肯定后不再发键、清理前已观察原版 exit 0。"
               "仅更新登记输入，未建库、重命名数据库或改写权威伪 C。",
    }],
    0x1A30B: [{
        "target": "evidence",
        "was": "现有登记只描述玩家结束后的 phase 顺序与状态 tick",
        "now": "首部仅恢复 side 2、flags & 0x81 == 0、record +0x25/+0x26 "
               "均为零且 HP != maxHP 的 actor：HP 增加 maxHP/5 并封顶，"
               "随后 OR acted；整数恢复量为零也标记。满 HP 或被 gate 排除者"
               "不统一标记 acted。空焦点菜单 selector 3 确认后直接调用本函数。"
               "末尾重置 dword_53AE9=0，执行 phase 2 事件与状态后，"
               "field_focus_move_to_actor(0) 逐 X 再 Y 恢复焦点和镜头。",
        "why": "复用已发布权威伪 C fd2-ida-decomp-all.c 的本函数首部"
               "（7282..7314 行）与 field_empty_focus_menu_execute @0x16f55 "
               "selector 3 调用（5250..5267 行），末尾 @0x1a30b 的调用链，"
               "2026-11-04 真实 START 原版肯定 END→round002→Save 运行证据；"
               "未重新建立 IDB 或发布逆向基线。",
    }],
    0x25977: [{
        "target": "evidence",
        "was": "其他值从 FDMUS handle `DS:0x1a79` 加载 entry",
        "now": "object2 `DS:0x1a79`（VA `0x51a79`）是字符串 `\"FDMUS.DAT\"`，"
               "不是 handle；`music_track_play` 把该文件名传给 "
               "`res_load @0x111ba` 取曲目记录。",
        "why": "IDA 字符串识别 `aFdmusDat` + res_load 调用形态",
    }],
    0x1F894: [{
        "target": "semantic_name",
        "was": "boot_intro_title / boot_intro_title_entry / title_action_menu 三条独立登记",
        "now": "boot_intro_title_menu（同一函数）",
        "why": "`0x44aa8 - 0x25214 = 0x1f894`，`0x44ab2 - 0x25214 = 0x1f89e` 即其 "
               "__chkstk body；IDA 只有一个函数 `0x1f894..0x1ff79`。",
    }, {
        "target": "evidence",
        "was": "corrected code0 `0x15db1` 在进入 `title_action_dispatch` 前调用 "
               "`music_track_play(18,0)`",
        "now": "该描述属于 `main @0x25bf4` 体内 `0x25db1`，不是本函数。",
        "why": "IDA 由 Watcom FLIRT 确认 `main @0x25bf4..0x25ebb`，"
               "其主循环为 music_track_play(18,0) -> title_action_dispatch @0x25ebb。",
    }, {
        "target": "evidence",
        "was": "标题为 new-game/exit 两项，有活动快照时在中间插入恢复项",
        "now": "基础标题为 START/LOAD；完整存档与活动 stage 校验通过时"
               "在末尾追加 CONTINUE。默认索引 0，三项标题按 Up 回绕到"
               "索引 2 CONTINUE；LOAD 与 CONTINUE 是不同分派。",
        "why": "复用已发布 boot_intro_title_menu @0x1f894 伪 C 的项数门、"
               "上下键回绕与 FDOTHER[7] 菜单资源；2026-11-04 原版 WGC "
               "Up→CONTINUE→stage 0 运行证据。未新建 IDB 或改写伪 C。",
    }],
    0x25EBB: [{
        "target": "evidence",
        "was": "标题只有新游戏与退出，或把 LOAD 与活动快照恢复合并",
        "now": "标题分派 branch 0 进入新游戏，branch 1 调用手工四槽 "
               "picker；branch 2 调用 sub_10010 @0x10010 恢复活动快照。"
               "LOAD 不等于 CONTINUE。",
        "why": "复用已发布 title_action_dispatch @0x25ebb 的分支与 xref；"
               "结合原版三项标题 WGC 的 Up→CONTINUE→stage 0 检查点，"
               "不修改现有函数名或发布新的 IDA 逆向基线。",
    }, {
        "target": "evidence",
        "was": "手工LOAD后续owner尚未确认，可能直接恢复当前战场",
        "now": "LOAD1恢复完整持久表、stage/资源/配置后进入城镇出阵准备26152，"
               "不直接恢复回合或焦点。START0直接新游戏，CONTINUE2才调用10010"
               "活动恢复；四槽可见恢复归B5，B1保留明确未支持门。",
        "why": "原始25ecf/25f3f/26130分支及26064..26098手工meta写集；"
               "260e5先26152、260f5再调DS:1d71。严格LE表51d71[0]指3231b，"
               "正常START32→31→0不经过准备页；原版阶段标签已有WGC，"
               "准备页及出阵恢复本轮仅静态取证，未冒称运行通过。",
    }],
    0x32975: [{
        "target": "semantic_name",
        "was": "field_actor_hide / field_ai_behavior7_hide_actor 两条独立登记",
        "now": "field_actor_hide（同一函数）",
        "why": "两条历史描述（写 actor[idx*0x50+5]=1）在 IDA 反编译上完全吻合。",
    }],
    0x111BA: [{
        "target": "address",
        "was": "0x463ce",
        "now": "VA 0x111ba / OFF 0x11ba",
        "why": "`FUN_0000e902 + 0x28b8`；机器码 fopen -> fseek(idx*4+6) -> fread，"
               "39 个调用点，`main` 用它加载 FDOTHER/FDTXT。",
    }],
}


def write_checksums(directory: Path) -> None:
    import hashlib

    sums = [f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}"
            for path in sorted(directory.iterdir())
            if path.name != "SHA256SUMS" and path.is_file()]
    (directory / "SHA256SUMS").write_text("\n".join(sums) + "\n",
                                        encoding="utf-8", newline="\n")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--audit", type=Path, default=OUT / "ida-name-audit.json")
    ap.add_argument("--functions", type=Path, default=OUT / "ida-functions.tsv")
    ap.add_argument("--annotate", type=Path, default=OUT / "ida-annotate-report.json")
    ap.add_argument("--json", type=Path,
                    default=ROOT / "docs" / "generated" / "ida-canonical"
                    / "fd2-ida-function-registry.json")
    ap.add_argument("--markdown", type=Path)
    ap.add_argument("--publish", type=Path, default=None,
                    help="把小体积证据产物复制到该目录（默认 docs/generated/ida-canonical）")
    ap.add_argument("--no-publish", action="store_true")
    ap.add_argument("--checksums-only", action="store_true",
                    help="仅刷新发布目录的 SHA256SUMS，不读取 IDA 中间导出或改写证据")
    args = ap.parse_args()
    if args.publish is None and not args.no_publish:
        args.publish = args.json.parent
    if args.checksums_only:
        if args.no_publish:
            ap.error("--checksums-only cannot be combined with --no-publish")
        write_checksums(args.publish)
        return 0

    audit = json.loads(args.audit.read_text(encoding="utf-8"))
    annotate = json.loads(args.annotate.read_text(encoding="utf-8"))

    funcs: dict[int, dict] = {}
    with args.functions.open(encoding="utf-8") as f:
        for row in csv.DictReader(f, delimiter="\t"):
            funcs[int(row["start_ea"], 16)] = row

    renamed = {int(r["va"], 16): r for r in annotate["naming"]["renamed"]}

    entries: dict[int, dict] = {}
    for row in audit["rows"]:
        va = row["canonical_va"]
        if va is None or row["resolution"] == "unresolved":
            continue
        e = entries.setdefault(va, {
            "va": va, "off": va - CODE0_BASE, "ida_name": "",
            "size": None, "legacy": [], "records": [],
        })
        e["records"].append({
            "semantic_name": row["semantic_name"],
            "purpose": row["purpose"],
            "evidence": row["evidence"],
            "legacy_address_cell": row["address_cell"],
            "legacy_ghidra_cell": row["ghidra_cell"],
            "resolution": row["resolution"],
            "legacy_body_va": row["legacy_body_va"],
            "source_line": row["line_no"],
        })
        e["legacy"].append(row["address_cell"])

    # 新确认的函数可能没有历史登记；仍须以已发布 IDA 入口校验并纳入登记。
    for va in CORRECTIONS:
        if va not in funcs:
            raise ValueError(f"correction VA is not an IDA function entry: {va:#x}")
        entries.setdefault(va, {
            "va": va, "off": va - CODE0_BASE, "ida_name": "",
            "size": None, "legacy": [], "records": [],
        })

    for va, e in entries.items():
        if va in CORRECTIONS:
            e["corrections"] = CORRECTIONS[va]
        fn = funcs.get(va)
        if fn:
            e["ida_name"] = fn["name"]
            e["size"] = int(fn["size"])
            e["end_va"] = int(fn["end_ea"], 16)
            e["is_lib"] = fn["is_lib"] == "1"
            e["prototype"] = fn["prototype"]
        names = sorted({r["semantic_name"] for r in e["records"]})
        e["historical_names"] = names
        e["aliases"] = [n for n in names if n != e["ida_name"]]
        e["merged_from_multiple_rows"] = len(e["records"]) > 1
        e["chkstk_body_rows"] = [r["source_line"] for r in e["records"]
                                 if r["resolution"] == "chkstk-body"]

    payload = {
        "schema": "fd2-ida-function-registry-v1",
        "oracle": "IDA Pro 9.4 idalib（loaders/lx.so + metapc + Watcom FLIRT + Hex-Rays）",
        "address_key": "LE relbase linear VA；OFF = VA - 0x10000（object1 内偏移）",
        "object1_range": audit["object1_range"],
        "legacy_rules": {
            "legacy_dual_minus": f"{audit['legacy_dual_shift']:#x}",
            "legacy_ghidra_plus": "0x28b8",
            "chkstk_body_offset": "0xa",
        },
        "source_rows": len(audit["rows"]),
        "canonical_functions": len(entries),
        "confirmed_anchors": annotate.get("confirmed_anchors", []),
        "entries": [entries[k] for k in sorted(entries)],
    }
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps(payload, indent=1, ensure_ascii=False),
                         encoding="utf-8", newline="\n")
    print(f"历史登记 {len(audit['rows'])} 行 -> {len(entries)} 个规范函数")
    print(f"其中由多行合并而来: {sum(1 for e in entries.values() if e['merged_from_multiple_rows'])}")
    print(f"含 __chkstk body 行: {sum(1 for e in entries.values() if e['chkstk_body_rows'])}")
    print(f"wrote {args.json}")

    if args.publish:
        import shutil
        args.publish.mkdir(parents=True, exist_ok=True)
        published = []
        for name in ("ida-program.json", "ida-verify.json", "ida-name-audit.json",
                     "ida-annotate-report.json", "ida-prototype-probe.json",
                     "ida-functions.tsv", "ida-names.tsv", "ida-strings.tsv"):
            src = OUT / name
            if not src.exists():
                continue
            dst = args.publish / ("fd2-" + name.removeprefix("ida-")
                                  if name.startswith("ida-") else name)
            shutil.copy(src, dst)
            published.append(dst.name)
        write_checksums(args.publish)
        print(f"published {len(published)} 个证据产物到 {args.publish}")

    if args.markdown:
        conflicts = [e for e in entries.values() if e["merged_from_multiple_rows"]]
        corr_rows = [
            "| VA | 规范函数名 | 项 | 历史说法 | 复核后 | 依据 |",
            "|----|-----------|----|---------|--------|------|",
        ]
        for va in sorted(entries):
            for c in entries[va].get("corrections", []):
                corr_rows.append(
                    f"| `{va:#x}` | `{entries[va]['ida_name']}` | {c['target']} | "
                    f"{c['was']} | {c['now']} | {c['why']} |")

        body = [
            "| VA | OFF | 规范函数名 | 大小 | 历史语义名 | 用途 | 确认依据 | 历史地址列 |",
            "|----|-----|-----------|------|-----------|------|---------|-----------|",
        ]
        for va in sorted(entries):
            e = entries[va]
            for r in e["records"]:
                legacy = f"`{r['legacy_address_cell']}`"
                if r["resolution"] == "chkstk-body":
                    legacy += f"（`{r['legacy_body_va']:#x}` 为 `__chkstk` body）"
                if r["legacy_ghidra_cell"].startswith("FUN_"):
                    legacy += f" / `{r['legacy_ghidra_cell']}`"
                body.append(
                    f"| `{va:#x}` | `{e['off']:#x}` | `{e['ida_name']}` | "
                    f"{e['size']} | `{r['semantic_name']}` | {r['purpose']} | "
                    f"{r['evidence']} | {legacy} |")

        header = f"""# 函数命名登记表（canonical VA 主键）

> 本表由 `python3 tools/ida/build_fd2_name_registry.py --markdown \\
> docs/reverse-engineering/function-names.md` 生成，请勿手工改写地址列。
> 新增或修订语义命名时，先改 `docs/generated/ida-canonical/fd2-ida-function-registry.json`
> 的上游来源（历史档案在 Git 历史中，判定账本为 `tools/ida-out/ida-name-audit.json`），
> 再重新生成本文件。

## 地址主键

唯一主键是 **LE relbase linear VA**；`OFF` 是 object1 内偏移，等于 `VA - 0x10000`。
object1 有效范围为 `VA 0x10000..0x4ef29`。函数入口、边界与大小由 IDA Pro 9.4
（`loaders/lx.so` + `metapc` + Watcom FLIRT）独立分析给出，复核过程见
[`ida-crosscheck.md`](ida-crosscheck.md)。

历史档案的地址列混用两套已废弃坐标系，换算规则如下（均已在本表中完成归约）：

| 旧坐标 | 换算到 canonical VA | 说明 |
|--------|--------------------|------|
| 旧 dual 镜像地址 | `VA = legacy - 0x25214` | 旧流程把 `LE+0x80 = 0x10e00` 当成文件偏移，真实 page 1 在 `0x25214 + 0x10e00 = 0x36014` |
| 旧 Ghidra 窗口地址（`FUN_xxxxxxxx`） | `VA = FUN_addr + 0x28b8` | `0x28b8` 是 embedded MZ 的 `e_lfanew` |
| 已修正的 `code0` | `VA = code0 + 0x10000` | object1 内偏移加 relbase |

Watcom 栈检查序言 `push imm32; call watcom_stack_check` 固定 10 字节，因此历史档案
里记在函数入口 `+0xa` 的「body」地址已全部归一化到入口。IDA 把 wrapper 与 body
视为同一个函数，`*_entry` / `*_core` / `*_runtime` 后缀属旧工具产物，不是原程序结构。

## 统计

| 项 | 值 |
|----|----|
| 历史登记行数 | {len(audit['rows'])} |
| 归约后的规范函数数 | {len(entries)} |
| 由多行历史记录合并而来 | {len(conflicts)} |
| 含 `__chkstk` body 登记行 | {sum(1 for e in entries.values() if e['chkstk_body_rows'])} |
| 机器可读版本 | `docs/generated/ida-canonical/fd2-ida-function-registry.json` |

同一 VA 出现多个历史语义名时，本表保留全部历史描述，规范函数名列给出裁决结果。

## 已推翻或澄清的历史结论

原始说法保留在下方登记表的「用途 / 确认依据」列，本节只记录复核结果。
完整依据见 [`ida-crosscheck.md`](ida-crosscheck.md) 第八节。

{chr(10).join(corr_rows)}

## 登记表

"""
        args.markdown.parent.mkdir(parents=True, exist_ok=True)
        args.markdown.write_text(header + "\n".join(body) + "\n",
                                 encoding="utf-8", newline="\n")
        print(f"wrote {args.markdown}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
