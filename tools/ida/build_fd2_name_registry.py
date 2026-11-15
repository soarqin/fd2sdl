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

def ida_provenance() -> str:
    candidates = (OUT / "ida-program.json", ROOT / "docs/generated/ida-canonical/fd2-program.json")
    program = next((json.loads(path.read_text(encoding="utf-8")) for path in candidates if path.exists()), {})
    version = program.get("ida_version", "unknown")
    host = program.get("host_platform", "historical host")
    loader = program.get("loader_module", "LE loader")
    return f"IDA Pro {version} {host} idalib（{loader} + metapc + Watcom FLIRT + Hex-Rays）"


# 历史档案里被 IDA 复核推翻或需要澄清的结论。
# 原始说法保留在 `records[].purpose/evidence` 中，更正以独立字段附加，
# 便于追溯「谁在什么依据下改了什么」。详见 docs/reverse-engineering/ida-crosscheck.md。
CORRECTIONS: dict[int, list[dict]] = {
    0x4ed34: [{"target": "semantic_name", "was": "sub_4ED34",
               "now": "image_raw_zero_transparent_blit",
               "why": "完整27B wrapper/9个caller：设WORD stride627a3，在4ed47真实CALL4ed4f，随后popa/pop ebp/RET；并非fallthrough拆分，不推ABI。"}],
    0x4ed4f: [{"target": "semantic_name", "was": "sub_4ED4F",
               "now": "image_raw_zero_transparent_rows",
               "why": "完整43B/唯一caller4ed34/独立RET：读原WORD宽高、逐raw byte仅非0写目标、按共享WORD stride换行，两个JNZ控制列/行；不是RLE解码，不保存Hex-Rays参数猜测。"}],
    0x197e5: [{"target": "semantic_name", "was": "sub_197E5",
               "now": "field_choice_buttons_close",
               "why": "主366B及17e03..17e0b共享8B epilogue，12个原caller及FDOTHER5按钮：53a51>1才地图重画，保bottom页桥、四步收回与310x86恢复；不改变原owner/ABI。"}],
    0x19953: [{"target": "semantic_name", "was": "sub_19953",
               "now": "field_choice_buttons_wait",
               "why": "主1188B及13fcc..13fd4共享8B返回尾、12个原caller、16f55/1aa1d/准备服务链：默认selector0、snapshot/四步展开、先查键后tick，blink RNG+2/+10、共享phase/tick，确认/取消清phase；不推类型。"}],
    0x4df4c: [{"target": "semantic_name", "was": 'sub_4DF4C',
               "now": 'field_map_runtime_cell_bits_reset',
               "why": '完整56B；10010/1088d/24754及AI调用；逐cell byte3=ff/BYTE2&1f/BYTE1&3，不推ABI。'}],
    0x35f10: [{"target": "semantic_name", "was": 'sub_35F10',
               "now": 'field_actor_tail_defeat_apply',
               "why": '完整56B；2548c/35677/35c40/35ec1调用；从给定raw到count写HP WORD0后1db65统一败亡。'}],
    0x3623c: [{"target": "semantic_name", "was": 'sub_3623C',
               "now": 'field_event_turn_slot2_random_pair',
               "why": '完整116B/callback表；slot2=now+1后单次4ebe3，同v/v+1模3标记两个acted，不取两次RNG。'}],
    0x348ea: [{"target": "semantic_name", "was": 'sub_348EA',
               "now": 'field_event_state16_ai7_once',
               "why": '完整86B/callback表；state16为0时AI24..27 masked7、story3、state16=1。'}],
    0x35123: [{"target": "semantic_name", "was": 'sub_35123',
               "now": 'field_event_item89_grant_once',
               "why": '完整110B/callback表；caller raw0/库存非8/state16为0时先append89、story11、再state16=1。'}],
    0x35e0e: [{"target": "semantic_name", "was": 'sub_35E0E',
               "now": 'field_event_side_state_turn_slot2_once',
               "why": '完整77B/callback表；caller side!=0/state17=0/state18!=0时slot2=now、state17=1。'}],
    0x35854: [{"target": "semantic_name", "was": 'sub_35854',
               "now": 'field_event_cell_reward_complete_first_five',
               "why": '完整323B/callback表/5B5274e；完整cell BYTE2选物，wait0后append/close，再state0..4=1与12263。'}],
    0x35a0d: [{"target": "semantic_name", "was": 'sub_35A0D',
               "now": 'field_event_key208_recruit31',
               "why": '完整363B/callback表/FDOTHER45；state12门，移item208在文本/ACK前，59帧后完成格与group1/roster31，非整功能新API。'}],
    0x352ca: [{"target": "semantic_name", "was": 'sub_352CA',
               "now": 'field_event_item211_story11',
               "why": '完整87B/callback表/descriptor52742；调用1aa1d(count1)后story11；共享尾3530d/3531b。'}],
    0x35346: [{"target": "semantic_name", "was": 'sub_35346',
               "now": 'field_event_story3_item213_story11',
               "why": '完整主111B及352ca共享尾/callback表/descriptor52745；先story3再1aa1d(count1)后story11。'}],
    0x355f0: [{"target": "semantic_name", "was": 'sub_355F0',
               "now": 'field_event_item101_story11',
               "why": '完整主72B及352ca共享尾/callback表/descriptor52748；1aa1d(count1)后story11。'}],
    0x35730: [{"target": "semantic_name", "was": 'sub_35730',
               "now": 'field_event_actor17_exchange_item11_once',
               "why": '完整主173B及3531b epilogue/callback表/descriptor5274b；caller0与state0门，2e2b0(17,0)/finalize，hidden17后state0/tiles/refresh/loot raw0 item11，最后贡献0；pC nullptr实际integer0，不推指针ABI。'}],
    0x35677: [{"target": "semantic_name", "was": 'sub_35677',
               "now": 'field_event_story5_defeat_tail18',
               "why": '完整60B/callback表；story5后35f10(18)，无额外ACK/清键。'}],
    0x35638: [{"target": "semantic_name", "was": 'sub_35638',
               "now": 'field_event_turn_group_pair_camera_flash',
               "why": '完整63B/callback表；两次AL(turn-14)*2/+1后MOVZX，分别35b78(2,11)/(26,11)。'}],
    0x35b78: [{"target": "semantic_name", "was": 'field_camera_music_flash',
               "now": 'field_camera_group_append_flash',
               "why": '主118B及35722..35730共享14B尾，与33c9d/33dba/35638/35c1d/35c40/35d1e/35d9e/35e5b/35f48/35f88调用；pan→group(BYTEarg8)→300ms→pal255→200ms→pal0→11cac(0)→400ms，无music_track_play；纠正旧名music推断。'}],
    0x1f1cc: [{"target": "semantic_name", "was": 'sub_1F1CC',
               "now": 'field_phase_title_enter',
               "why": '完整318B/1a30b唯一caller与FDOTHER5标签80..82：7次滑入后保VGA到53a49 raw prefix；factor1..16马赛克、DAC16..255减暗0..15、每帧1tick，末BIOS清键，不推ABI。'}],
    0x1f30a: [{"target": "semantic_name", "was": 'sub_1F30A',
               "now": 'field_phase_title_leave',
               "why": '完整291B/1a30b唯一caller：factor17..1与DAC暗度16..0逐tick，11eee/127a9重画原页，5次滑出；不调用1297d、不推ABI。'}],
    0x1f42d: [{"target": "semantic_name", "was": 'sub_1F42D',
               "now": 'field_phase_title_slide_frame',
               "why": '主214B/1f1cc和1f30a callers及184ba共享6B epilogue：双标签在456stride页(85-offset,82)/(165+offset,82)，投影312x192、1tick后恢复两矩形。不把尾跳转误为新ABI。'}],
    0x4eb59: [{"target": "semantic_name", "was": 'sub_4EB59',
               "now": 'field_phase_mosaic_visible_copy',
               "why": '完整82B/1f1cc与1f30a callers，320stride(4,4)的312x192复制；DL/DH按factor分块重复采样、MUL取低WORD。原phase有效factor1..17，不推猜测签名。'}],
    0x13fd4: [{"target": "semantic_name", "was": "sub_13FD4",
               "now": "field_stationary_recover_present_apply",
               "why": "完整333B、13a9f/18d8c callers及13fd4/1da16/31号样本bank来源：单单位静止恢复，三个wait后才写HP；不推ABI/类型。"}],
    0x1e611: [{"target": "semantic_name", "was": "sub_1E611",
               "now": "field_short_counter_refresh",
               "why": "完整296B与1548e caller：重画地图、48号双面板、固定55及HP条；不是反击箭头。caller仅三实参而callee读第四项，未初始化读取单列安全边界，不补猜参。"}],
    0x1e739: [{"target": "semantic_name", "was": "sub_1E739",
               "now": "field_short_hp_bar_draw",
               "why": "完整189B、1e611/1e7f6/1e98c xref及FDOTHER5列23/24/25、29/30：按给定宽绘HP条，不推ABI/类型。"}],
    0x1e7f6: [{"target": "semantic_name", "was": "sub_1E7F6",
               "now": "field_short_hp_bar_scaled",
               "why": "完整96B与1e739/1eb05/1e611调用：非零HP按69*HP/maxHP+1绘条，零HP跳过；不推ABI/类型。"}],
    0x1e98c: [{"target": "semantic_name", "was": "sub_1E98C",
               "now": "field_short_strike_present",
               "why": "完整377B、1e856/1e739/1da16 callers与61955的21指针/84B序列：武器FX、命中/失误声、64色solid与逐列HP条，不推ABI/类型。"}],
    0x1eb05: [{"target": "semantic_name", "was": "sub_1EB05",
               "now": "field_short_panels_present",
               "why": "完整293B、1548e caller、1ec2a与FDOTHER6：39..48十帧面板，反击可用才有第二面板，25ms步进；1e7f6初始HP条只有VGA写，无独立wait/flip；SDL不为初条新增零时长上传，而在首STRIKE上传条+实体+FX并按原时长等待。不推ABI/类型。"}],
    0x1ec2a: [{"target": "semantic_name", "was": "sub_1EC2A",
               "now": "field_short_panels_layout",
               "why": "完整157B、1eb05唯一caller及地图坐标/镜头边界计算，确认SHORT面板布局，不推ABI/类型。"}],
    0x2cf30: [{
        "target": "semantic_name",
        "was": "sub_2CF30",
        "now": "field_derived_magic_full_execute",
        "why": "2ff01按24/28..31调用，完整Domain/caller/FIGANI fixed slot2和逐part写集核对；G2实际C核心与原1c81f差分另登记，未推ABI/类型。",
    }, {
        "target": "evidence",
        "was": "未区分派生物理量与普通profile倍率伤害",
        "now": "signed AP乘24:15/28:20/29:12/30..31:18后除10减signed DP，直接1c81f；保存signed旧HP、核心贡献先提交再恢复HP，caster每个effect part marker以BYTE计数写oldHP-damage*count/divisor（28除8，其余1），duration0仍处理marker。intro每marker费用，固定FIGANI unit*3+2无普通fallback。",
        "why": "G1完整battle读集、2cf30全部分支与原FIGANI资源；G2状态/晚失败/skip回归。原版运行范围另列，不将局部代码差分当完整演出验收。",
    }],
    0x2d80d: [{
        "target": "semantic_name",
        "was": "sub_2D80D",
        "now": "field_composite_magic_full_execute",
        "why": "2ff01对32..35调用；完整Domain/xref、FIGANI fixed slot1、FDOTHER65..68与bank91..94及回图全组写序共同确认，不推ABI/类型。",
    }, {
        "target": "evidence",
        "was": "复合效果的目标组与费用边界尚未完整区分",
        "now": "先完成fixed FIGANI unit*3+1及intro每marker费用，再专用FX/回地图；32走record32伤害；33先全组清25..27再全组恢复800；34全组AP→清浮字→DP→清浮字→HIT/EV；35全组毒26→清浮字→禁法22→清浮字→麻痹27。组内复用core，不重复wrapper费用；不得逐目标合并三次效果。",
        "why": "完整2ddca..2deec指令、原表/资源与G2冷图/组顺序回归，运行范围另列。",
    }],

    0x1297D: [{
        "target": "evidence", "was": "约每275ms推进phase",
        "now": "1297d只在signed BIOS tick差>4或负数时把53c0b加一并模4，"
               "53c0f直接重取当前tick，不补追全部经过周期。另53c07每次调用加一模4，"
               "与11eee的地形开关53a40不同；不能把它们当同一时钟。"
               "11cac先调用1297d再绘tile/actor/1acf3面板；21eb1尾11cac→200ms→"
               "1c4cc首11cac后冻结FX背景，共用唯一视觉相位/last锚。",
        "why": "既有canonical完整1297d(111B)/11cac/21eb1/1c4cc及validated relocated原字节。"
               "C4 heal13实际原/SDL同一FX59两页1678差异全部为idle1→2：actor1374+"
               "底栏304；各端整2346底栏源模型bad0/max3，无E0..EF。只作为视觉时钟差异"
               "与本轮图像归因，修复后的实机完整帧验收尚待；不改函数名/类型/ABI。",
    }],
    0x11EEE: [{
        "target": "evidence", "was": "地形phase的一般描述",
        "now": "函数首仅当BIOS当前tick!=53a00时翻转53a40一次并重取当前tick，"
               "不按经过周期奇偶补追。attrs08加2*53a40、10加53c0b/2、04加53a40；"
               "角色idle由1297d另管，范围LUT53c1f亦为独立门。",
        "why": "既有完整11eee(885B)与1297d及tile flag分支；本次私有修复仅将"
               "55/275ms宿主近似的视觉锚按原调用点单次推进，不宣称BIOS时钟、"
               "全局RNG、水DAC或所有地形动画已同步。保持field_view_render_tiles既名/类型。",
    }],
    0x18B84: [{
        "target": "semantic_name", "was": "sub_18B84",
        "now": "field_unit_selection_summary_wait",
        "why": "完整233B、18890唯一caller及115b6紧随调用：while(!input_check())逐BIOS tick重绘"
               "terrain/selector/actor，再18c6d与下方cell板，最后复制page到VGA。相对X>=7时"
               "上板(9,9)，否则(160,9)。发现下一键即返回而不读取/清键，由115b6消费同键；"
               "最初选择键已由上层消费，不是持续UNIT_SELECTED板。原版多场6258px完整板及"
               "首方向后板消失已独立重放；raw SHA ebabd1f40c050dff9b29b3bfc4a99f83b9a9988e938e65ee8c19537d56478326。",
    }],
    0x18C6D: [{
        "target": "semantic_name", "was": "sub_18C6D",
        "now": "field_unit_summary_draw",
        "why": "完整287B及18b84/2facd共享caller：先FDOTHER5 frame22不透明149x42板，再画"
               "HP/MP两条及数字、LV和actor.text_id+1名字。HP/MP及上限均signed16，LV为u8；"
               "条起点(21,22)/(21,31)，LV(132,4)，HP/MP数字(126,21)/(126,30)，名字(5,4)。"
               "1875d在current==max用bank31否则42；actor/cursor先画，板前景后覆盖，不裁掉"
               "头部就冒称完整actor。raw SHA fa85a60e551fb589d541b39ff772e2427cd38e0462f7cd291c0e894a7b08f2af。",
    }],
    0x16559: [{
        "target": "semantic_name", "was": "sub_16559",
        "now": "dialog_portrait_frame_draw",
        "why": "完整83B及15f84/164e8/16c57/1e292等xref：按DATO帧偏移重画立绘，"
               "origin53C67等于36887时以VGA右端(87,115)反向画，否则在该origin正向画。"
               "DATO0..135全部4帧80x80；4ec31先写再dec，普通页左界为8。"
               "raw SHA 18ab5a72d38d7240d0cf5af6c966c259f235a8ed4293d1cd74f6c9618080eac1。",
    }],
    0x16C57: [{
        "target": "semantic_name", "was": "sub_16C57",
        "now": "dialog_wait_key",
        "why": "完整461B与15f84/17aed/1b9de/1d51d等caller：读取一个BIOS键，"
               "可选18/19箭头；每次入口一次RNG%30+2，按2 BIOS tick门更新"
               "portrait0/3及重新取blink计时，并显式推进water DAC。"
               "正式map非NULL的bottom箭头为(191,181)，无map另分支为143。"
               "返回前读取并规范化扩展键；不猜测持久ABI。raw SHA "
               "9ee45fd0d376c2f05cedeff838be5f13f7fcc8724000b152f1ad17f44c5b865e。",
    }],
    0x1956B: [{
        "target": "semantic_name", "was": "sub_1956B",
        "now": "field_bottom_notice_open",
        "why": "完整352B及系统确认/1a866/1e292等caller：分配三个64000B缓冲，"
               "保存初始VGA并拼310x86的bottom底板，载入DATO初帧reverse，"
               "i5..0以1974c(13*i+112)滑入。不是奖励专用；128..132有特殊origin。"
               "raw SHA e5b8b359f8371ce6513766b7d0badb2189dd8ea58af75a4e8abda637d0733026。",
    }, {
        "target": "evidence",
        "was": "系统notice首帧/稳态/结果使用详情底板或占位底色；caption坐标未由literal核算",
        "now": "确认框/奖励/库存页共用notice owner：先完整保存初VGA，FDOTHER[5] tiles拼310x86 bottom，再DATO初帧reverse，非128..132由36887=(87,115)向左绘80列；特殊origin原表保留。三缓冲的静态初板用于关闭，不是detail_bottom(详情用99高帧)。原意无0xcd整块占位/独立palette切换。",
        "why": "既有完整canonical函数、caller/xref与validated relocated原字节；独立Domain确认首提示push0xA9F23及后提示0xAB6E3，扣VGA基址直接为(99,127)/(99,146)。C4旧实际窗口确认底色差异；本轮修复后的Windows完整页验收尚待，不以纯SDL回归代替原版。",
    }],
    0x196CB: [{
        "target": "semantic_name", "was": "sub_196CB",
        "now": "field_bottom_notice_close",
        "why": "完整129B与1956b配对caller：i1..5调用1974c滑出初始静态底板，"
               "恢复打开前VGA、释放三缓冲并11cac(0)刷新；关闭不以当前文字页作源。"
               "raw SHA 9ed5ae278e9018f35c52d1abc69d2f475dcfb56d1dfc09b7d6f5441276c766e5。",
    }],
    0x1974C: [{
        "target": "semantic_name", "was": "sub_1974C",
        "now": "field_bottom_notice_slide_draw",
        "why": "完整153B及1956b/196cb和底部选择页caller：先复制53C5F初始背景，"
               "再从源(5,112)逐行复制310像素到目标y，最多86行并裁到200，"
               "交接完整VGA后走既有同步尾；不移动camera或修改actor。"
               "raw SHA 86eee771d050f98da4a5a88143f434f340cc0e7e84c8a7f987c525527fb688c8。",
    }],
    0x1D79C: [{
        "target": "semantic_name", "was": "sub_1D79C",
        "now": "field_unit_magic_bit_set",
        "why": "完整111B、1e292唯一caller及能力位图数据：按magicID/8选择"
               "actor+1a起byte，并OR原1<<bit表；不校学习等级、不判重复已学。"
               "学习条件及消息在caller，不把此函数当完整学习流程。"
               "raw SHA d09104c23818299c04f1c1e5303b2b18db1c7d4ee6d5fd7172c575e6ed8fe54c。",
    }],
    0x1E529: [{
        "target": "semantic_name", "was": "sub_1E529",
        "now": "field_growth_stat_present_apply",
        "why": "完整151B及1e292/2ac7d调用：1e548的EB0E跳过RNG块，"
               "取growth pair第二byte到53AE1；零值不写不呈现，非零时必要先scroll，"
               "显示带-3的成长消息后才WORD+=delta并递增行号。"
               "raw SHA 9f5f76922f070cd37ea0d39c655f478c9d5ccd9de6a798695e74fbbc05e13f78。",
    }],
    0x1E5C0: [{
        "target": "semantic_name", "was": "sub_1E5C0",
        "now": "field_notice_wait_ticks",
        "why": "完整81B、1a866参数10/1e292参数11：循环显式water DAC与input_check，"
               "达到BIOS tick间隔、tick wrap或任意键即结束，最后清BIOS键队列。"
               "本体不调用field RNG，不把可按键提前结束写成固定完整时长。"
               "raw SHA e9edc6715a04c11222290f6108decd5f54409f1adc7f209963bd0a599ced8e4d。",
    }],
    0x4E7F2: [{
        "target": "semantic_name", "was": "sub_4E7F2",
        "now": "field_magic_learning_record_get",
        "why": "完整23B getter返回object3 VA626b3+12*index，唯一caller1e292；"
               "68条growth+10的非ff引用完整覆盖0..19，对应20x12B六对level/magicID。"
               "完整240B strictLE==Domain SHA "
               "839200667f837136d3426ab952e455b1a63b3bbba8b9175eb895d4408ddd6fb9；"
               "getter无自己的索引门，不把已证引用集当原版一般校验。",
    }],
    0x4E821: [{
        "target": "semantic_name", "was": "sub_4E821",
        "now": "field_growth_record_get",
        "why": "完整23B getter返回object3 VA620a1+11*index，"
               "由10c50/112a5/1e292/2ac7d读五对成长及学习selector。"
               "下一独立getter4e809指向6238d，完整68x11B strictLE==Domain SHA "
               "75bedc7308088625abc0b70c10115d94903008e7fb907a0acaadc22dc45af2a5。"
               "getter无内部边界，不把SDL玩家构造范围等同该静态读集。",
    }],
    0x17D6F: [{
        "target": "semantic_name", "was": "sub_17D6F",
        "now": "field_stat_bar_columns_draw",
        "why": "完整156B raw-unrelocated SHA "
               "8feeb8ffe6045be1a71a51091a2437372edc6d8f9553bc762ae508f666446595；"
               "唯一caller18795，按signed length分支调用1685c复制FDOTHER[5]的1x5列。"
               "length非0先head，i=1且signed i<length时middle，最后tail；"
               "负length只写head和x+1 tail，不把负数当0。length0在x+1..101画29、"
               "x+102画30，x本身不写；全部8帧23..30已只读核尺寸和raw像素。",
    }],
    0x18795: [{
        "target": "semantic_name", "was": "sub_18795",
        "now": "field_stat_bar_draw",
        "why": "完整65B raw-unrelocated SHA "
               "c665cfc068acf8b11f7e67d6a063754301dd508087e79bf8358744b7ea223f4f；"
               "callers17fc0详情和18c6d战斗信息均先movsx读取HP/MP，再传本函数。"
               "max为0不画；current0传length0，否则signed idiv得到"
               "trunc(101*current/max)+1后调用17d6f。length是参数，不是实际像素宽："
               "正length写length+1列，负length只写2列，0分支写x+1..102共102列。"
               "Domain指令字节与已验证relocated镜像一致，不据伪C固化ABI。",
    }],
    0x17FC0: [{
        "target": "evidence", "was": "详情HP/MP与属性WORD可按无符号直接显示",
        "now": "HP/MP当前和上限、DX、AP/DP/HIT/EV共9个WORD均movsx；"
               "LV/EX/MV为u8。HP/MP的signed值同时进入18795条形和1875d数字路径，"
               "不能只修数字而把u16下溢MP画成满条。",
        "why": "完整749B及17eef/2b4fb的xref，17feb..17ffe的movsx与"
               "后续各属性调用、18c6d同类读法；18795使用signed idiv，187d6"
               "负数显示0，2位大于99用frame93、3位大于999用起始bank+10。"
               "本轮只登记原始读法与绘制边界，MP扣费u16写集不改为夹0。",
    }],
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
    }, {
        "target": "evidence", "was": "恢复实际差一定非负，贡献可等到完整effect返回后再加",
        "now": "先提交HP，再把signed(newHP-oldHP)*40*weightedLV/maxHP加到DWORD53EC8。"
               "oldHP>maxHP时cap可产生负差，ADD按32位回绕而非夹0。MP恢复1c9dd"
               "在写MP后复用1c9c7的signed除法/ADD尾，ID<75才计算且不作profile加30。",
        "why": "Domain原字节1c990..1c9cf和1ca58..1ca84；有状态cure的heal贡献"
               "先于clear/额外贡献，负状态damage贡献更先于duration RNG。"
               "恢复helper自身不归一或写caster EX；异常除零/未知数据支持门另列。",
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
        "target": "semantic_name", "was": "sub_1E292",
        "now": "field_unit_experience_reward_execute",
        "why": "完整663B及117e7/1548e的xref：检查raw贡献/hidden/等值levelcap，"
               "显示488后逐级执行LEVEL、五成长、六pair学习和重算，"
               "最终wait11/关闭后才写EX并清53EC8。入口早退保留raw；"
               "level先于489，成长WORD在其-3 ACK后写，学习bit先于587。"
               "普通40仅入口等值门，循环只在新30或特殊ID30/31新99清余值；"
               "raw SHA 5d9de2950a3c2f9ce7f68142e139d94176a175328ea9867dd3654abe2d3ef75b。",
    }, {
        "target": "evidence", "was": "未升级动作可省略EX或用WAIT收尾代替",
        "now": "gain非零、actor未hidden且不在等值levelcap时执行奖励；EX+gain<100"
               "不成长，完成488及自动等待/关闭后才写+3c并清全局。玩家18d8c先"
               "按level+(profile>8?30:0)归一，117e7再cap99；AI1548e不作cap99。"
               "B4已静态复核完整逐级/成长/学习顺序，不把原B2的非升级支持门当原版门。",
        "why": "已发布1e292及117e7/18d8c，完整663B SHA "
               "5d9de2950a3c2f9ce7f68142e139d94176a175328ea9867dd3654abe2d3ef75b；"
               "levelcap为ID30/31等于99、其它等于40，不扩为>=。"
               "完整Domain与原字节/xref确认语义，原版提示/等待RNG运行验收另登记。",
    }, {
        "target": "evidence",
        "was": "只核对非负gain的奖励路径",
        "now": "gain及EX临时total按signed DWORD比较，gain非零负值仍先显示488；仅当signed total<100时跳过升级/重算，等待11tick及关闭后写EX低byte。负gain可将EX回绕为大于99，后续奖励仍按EXbyte+signed gain判断，不能以负号提前终止循环。正常尾清53EC8，入口早退保留；controller的99上限也是signed比较。",
        "why": "独立Domain/strictLE读117e7/1e292及18d8c的SAR/IDIV与JLE/JL锚；readset SHA25972023958fdcf06d8052424ea5e1241038a0bed93d3a054313ee96e015e7ee。保留原字段位型，不猜ABI。",
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
    }, {
        "target": "evidence", "was": "状态消息可在整批HP/计时/属性写入后补播",
        "now": "每名中毒actor先HP写入和focus，再bottom notice487，clear键、"
               "wait10、关闭；全部毒后才死亡清理及stage handler。每个+22..27"
               "归零后focus→481..486→clear/wait10/close→立即1b750，再处理下一byte。"
               "487的-2只换行、-6读53AE1毒伤，七个片段均无-3；自动等待无blink RNG。",
        "why": "完整439B Domain/strictLE字节相等；消息调用696099对应(99,127)，"
               "1956b/196cb及1e5c0(10)和FDTXT0的481..487完整token相互支持。"
               "逐字164e8可留下portrait0/1/2，不能套16c57等待页的0/3。",
    }],
    0x1ECC7: [{
        "target": "evidence", "was": "物理经验可把每击贡献相加或按实际HP差统一归一",
        "now": "仅attacker.side2且defenderID>=68写53EC8，以MOV覆盖而非累加。"
               "先enemyLV*coef除有效attackerLV；若最终HP非零再rawDamage*base除maxHP。"
               "profile9..24或textID28将u8等级副本加30，注意不是unitID28；"
               "maxHP等战斗WORD均movzx。乘法为32位imul后signed idiv，不能悄改u64公式。",
        "why": "完整899B及1e856唯一caller；1ed20、1efd2..1f03c逐指令，"
               "2f7b6的2fa4d..2fab9采用同两次截断/MOV规则。miss时存活者贡献0，"
               "其它side/ID门不写而保留旧全局；完整FIGANI与无演出RNG轨迹仍分开。",
    }],
    0x1B750: [{
        "target": "evidence", "was": "AP/DP状态倍率1.15使用x87 nearest-even取整",
        "now": "仅AP/DP在装备累加后乘原binary64倍率1.15；原__CHP临时设置RC向0，"
               "故先PC53乘法舍入，再向0截断。10→11、12→13、20→23，"
               "100→114、-100→-114，不能替换为整数115*v/100。AC/EV不乘倍率，"
               "+24状态只使两者各加15。",
        "why": "完整237B/xref与原指令VA1b7ef..1b802、1b80a..1b81d及"
               "__CHP37af4；启动CW127f，倍率原字节666666666666f23f。"
               "verify_fd2_x87_stat_scale.py复用原块/原__CHP，无patch或hook，"
               "两块各穷举-65536..65535共131072输入，等于强制binary64中间值再向0cast；"
               "每块向量SHA 55dafb59f3c3576c68a43d3f241cc735c5160a4b3eb48e362fb54315d0ea32c2。"
               "这是局部机器码与C模型复核，不代表原版运行或全演出时序验收。",
    }],
    0x1548E: [{
        "target": "evidence", "was": "AI物理尾只奖励仍存活的玩家反击者且先清贡献",
        "now": "exchange后方向reset→败亡描述符→刷新→死亡清理→掉落→刷新，"
               "随后无额外HP门调用1e292给原防御者。caller不清53EC8、不作99cap；"
               "1e292入口hidden/等值level门可早退，早退不消费残留全局。",
        "why": "完整496B、13a9f/14ef0两caller及15627..1566a指令；53EC8完整xref"
               "无1548e/1e856/13a9f/14ef0初始化。Load10010/secondary19df7/END1a30b"
               "亦无清零写，不能从后来118ee/18dc4选择门推出Load必已清贡献。",
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
    }, {
        "target": "evidence", "was": "20/21仅清状态无RNG，状态贡献可以动作末统一累计",
        "now": "20/21的22af6仅在状态非零时先1c916(target,10)耗一次spread RNG并"
               "写HP/累heal贡献，再清状态并加4*weightedLV。17/18/19首次buff加"
               "2*weightedLV；25只在acted真时清位并加8*weightedLV。22/26/27"
               "命中先1c81f写HP/累damage贡献，再duration RNG、写状态、加8*原LV。"
               "23在扣费后、迁移前加10*weightedLV。AI尾1546a及item21046仍清全局。",
        "why": "完整51d01表及22721/22866/22997/22af6/22c04/22d1b原始写xref；"
               "weightedLV仅target profile9..24加30。22dd6..22e0b锁duration RNG前"
               "已有damage贡献，不能只在整个effect后补写。buff增量仍沿本轮x87"
               "127f/PC53和u16全域局部执行证明的floor((old*15+100)/100)，非RNE结果。",
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
    }, {
        "target": "evidence",
        "was": "未记录当前身份与旧TMP cache在Load后的关系",
        "now": "1035c清53BDF；1036a..1039c按raw actor顺序读取+7当前unit ID调用11019，并于10391重写+2；103a9..103cb写新FD2.TMP。热身份变更可保留旧cache sprite，但Load会按当前ID重建。SDL保留原80B、以派生旁路表示新class，不承诺旧TMP保持。",
        "why": "2026-11-10 Domain窄读10010/11019/2548c/34dd0共2911B与strictLE一致；readset SHA eff244cac03080c8b394489275fbf10018b80b95e31d27f2d4239a3ea10ac84e。未改原型/边界。",
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
        "target": "semantic_name",
        "was": "sub_2000A",
        "now": "field_auxiliary_map_execute",
        "why": "完整947B、唯一117e7 caller与1399c/1f558/1f6ef/INT16调用链；"
               "正式F1/PageUp和既有stage0完整WGC两相位来源。只确认有限视图流程，不改ABI。",
    }, {
        "target": "evidence",
        "was": "F1/PageUp辅助页可能是部队列表或另一种战况总览",
        "now": "独立的全场缩放地形图，使用原始terrain cache；开7帧、稳态标记、"
               "关6帧后普通重绘。高>40选scale3/delta896，否则scale4/delta640，无stage/shape门。"
               "开后clear一次，稳态peek后202fa INT16 AH10读一键、余队不清，再关闭。"
               "F1直接返回无controller提交；期间不推进water，尾11cac(0)恢复。",
        "why": "2026-11-10 Domain三函数1434B与strictLE/fixup逐chunk一致；20055/2005e/20063"
               "尺寸分支、202e7设AH10/202f8传16h/202fa callint386。原33图最大50x64，"
               "stage1真实27x21/shape1/192帧、高图stage7为29x45/shape7/384帧；"
               "旧stage0限制属已登记SDL缺口，不声明新晚关/精确时序/首次undefined tick。",
    }],
    0x1F558: [{
        "target": "evidence",
        "was": "F1缩放采样、黑边与动画中心未完整登记",
        "now": "tile跨度0xc00、像素余数右移7；清App53A49 raw-prefix64000B，"
               "按320stride画(4,4)312×192，2000a各开/关帧再复制到VGA；非456stride72/72桥。"
               "开phase1..7中心系数(phase+1)/8，关6..1，不补phase0采样；"
               "高>40最终step1024，否则768；固定原始tile而非动画替代帧。",
        "why": "完整1f558含共享尾、1f5e5取53A49/1f5ea加504h/1f5fc memset64000，"
               "原2000a复制和尺寸分支；独立负坐标floor/黑边/opaque0/全64000源oracle。"
               "临时解码cache恢复，App页历史按实写保留；不改terrain热owner。",
    }],
    0x1F6EF: [{
        "target": "semantic_name",
        "was": "sub_1F6EF",
        "now": "field_auxiliary_marker_draw",
        "why": "完整80B由2000a两处调用；1f733以scale-1同时作memset宽和行界，"
               "320stride直接写VGA。命名仅指固色标记叶，不猜参数ABI。",
    }, {
        "target": "evidence",
        "was": "F1单位标记门、颜色和脉冲尚未登记",
        "now": "map高>40每tile3px/标记2×2，否则4px/3×3；原1f733取scale-1。"
               "caller按raw顺序排hidden/unit121/race10，"
               "不加HP/acted/AI门。side0/1/2基色32/80/72，focus灰24最后覆盖。"
               "phase7..0,0..7,7端点重复，原BIOS差2，SDL约110ms且不推进玩法。",
        "why": "完整Domain/strictLE1f6ef及2000a尺寸分支、原始object2表5208a和原版空格/actor0叠加页；"
               "F1 marker过滤不推广为普通actor renderer规则。",
    }],
    0x1F525: [{
        "target": "evidence",
        "was": "淡入暗度终点与等待次数未完整登记",
        "now": "palette_fade_in_light从darkness64到0逐次应用，共65次；每次原delay_ms(2)，"
               "包含darkness0的尾2ms。不是64帧或每次1BIOS tick。保留现规范名，不猜ABI。",
        "why": "G1 scope-callee-closure完整51B SHA1d887e704729a4bf4a69abae92cfb876a364a4d2aee0bbfcce7d8426b7533c39；"
               "1f530初值40h，1f551 dec/1f554 jge，循环内push2/call3790a。当前prepare_fade依此修正；"
               "source调用/次数不等于宿主硬件绝对2ms已测。",
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
    }, {
        "target": "evidence",
        "was": "raw26视觉仍未实现的旧切片状态",
        "now": "原127f1..12810比较BIOS低WORD与53A08，tick变化只翻53A04低byte一次；128e7..128ef在actor+26非零时加共享0/1px，12914后同条件强制frame0。不改业务坐标、状态或RNG，acted映色及原层序独立。SDL用App VGA视觉owner对应；运行精度按阶段收据，不把接口存在当原版整帧通过。",
        "why": "完整413B原指令 SHA68b24e0d28affff7c08ed4eb1df55f3a558021c170f77019191b5f0f8b5094e4；G1完整读集与G2 clock/render回归。",
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
    0x13460: [{
        "target": "semantic_name", "was": "sub_13460",
        "now": "field_movement_tick_edge_wait",
        "why": "完整13460..13488共40B及12eaa/1300d/13185/13315正常移动caller；"
               "1346f将BIOS[46c]低WORD符号扩展，与App DWORD53a0c相等才回读；"
               "不等后1347a..13482重新读当前tick并写anchor，不是每相位新鲜等1tick。"
               "既有Domain source09 SHA18617904b37d73b066eea7a78889d6da64fc6dad895b9292ae279df4b8f47517，"
               "完整chunk SHA1ffd7e099e8a1ba4a19f45f6a80825aeabad462024147e87f0b8032a677022de，"
               "逐字节匹配strict relocated；不保存参数/返回类型或猜测ABI。",
    }, {
        "target": "evidence", "was": "普通移动等待按55ms或每调用等待一个新tick理解",
        "now": "普通四方向每格六相位共用53a0c anchor；比较通过后重读并保存signed lowWORD。"
               "本函数不调用4e381、不清普通键队；anchor属于App，不随Scene/Game/Load重置。"
               "初始53a0c四字节0有strict文件及既有F1 loader/entry转储依据，"
               "不把这项初态观察扩为新postCRT时钟测量。",
        "why": "完整13460与四方向caller的既有Domain读集、strict镜像和F1运行时来源分账；"
               "F2 BIOS共享时钟/边界测试只验证C消费者，不替代正常原版时序与输入运行对照。",
    }],
    0x135DD: [{
        "target": "evidence", "was": "每格调用vsync_wait等待垂直同步",
        "now": "field_camera_pan_to先完成X再Y；每格11cac(0)后4e381清普通BIOS FIFO，"
               "没有17ms或显式tick等待。入口清51a83；零距离不执行逐格clear。",
        "why": "完整135dd..1366a共141B，X在13622刷新/1362a清键，Y在1365b刷新/13663清键；"
               "既有Domain source09完整chunk SHA60f0ad6883f634fc4d3eec83a5e32b0d2af9ed285ff0c934b3e68bfea7a158bd"
               "逐字节匹配strict relocated；旧vsync登记是语义误判，不据此新增渲染节流或声明真实时序已验。",
    }],
    0x1366A: [{
        "target": "evidence", "was": "脚本帧后4e381是vsync等待，可用名义17ms代替",
        "now": "field_movement_script_play正常每相位render→17aa9(1)→4e381 clear；"
               "high-bit非零等待同序；jump先wait1、临时上抬合成、wait2、11cac(0)、clear。"
               "1398c尾仍11cac(1)，不追加clear；4e381本身没有等待。",
        "why": "完整1366a..1399c共818B及138fd/13905、1380f/13817、136db/137ae/137b8/137c0、1398c原调用；"
               "既有Domain source09完整chunk SHA9fa0bc803cd6303e62a2c49bb86b1bfdd139affc15de18b84c21e3c141a10ddf"
               "逐字节匹配strict relocated。这里只更正普通输入/等待来源，不签脚本全域、像素、绝对时序或RNG全轨迹。",
    }],
    0x122DC: [{
        "target": "evidence", "was": "普通cursor指针或玩家interaction可代替所有51a83值",
        "now": "122dc仅对mode1..6分派；6清node而无sprite，其它值走无sprite的default。"
               "15311确实无条件以magic record+4加2聚焦；原magic10/11/12的area5/7/9产生mode7/9/11，"
               "12cea仍按非0且非6等待，不能以renderer只列到6拒绝这些原值。",
        "why": "既有Domain source09完整122dc..126f7 SHA866692f43bd2736ade9ce526425241d33af53a54064647104dc76f96317a3a55，"
               "15311..1548e SHA967b72e576507cea33c9ec1d085aa9af182acd3c7e7402348c5488223999050e；"
               "原EXE object3 MAGIC252B@619fd SHA d516dc85c98d06e860477701ff1c018ac3a3f0953d79d0a6022aaf7a98f014c3"
               "独立读字节核field4完整域0/1/2/3/5/7/9。只核该分派/等待域，不代签完整AI范围/FX。",
    }],
    0x4EBE3: [{
        "target": "evidence", "was": "loader初态7a18，按计时器低8位每战场推进",
        "now": "原文件与真实CRT后共享RNG WORD=0，Watcom rand DWORD=1；main25d90首次rand返回16838，"
               "内部状态41c67ea6，%256=198，25da7推进198次后共享WORD=5885。"
               "25d83的BIOS低WORD只初始化视觉clock，不seed rand；Game/Load不重建App流。",
        "why": "F1 startup-native02的CRT后/25d83/25db1原版RAM及startup-original-cpu五BIOS向量；"
               "原CPU报告SHA0794806103d24778492609290966f3439376fc1c6141700aa3af40b66c19cf37，"
               "原运行报告SHA56dd479b8e77dfb4c6d91264d109814485bc41904f5827e57494730dbc55b8cb；"
               "F2 bootstrap逐项比较198次调用前状态与末态。5885是推进后的值，不是原初值，"
               "不将启动资格扩为标题后全部caller或严格绝对时序。",
    }],
    0x4E381: [{
        "target": "semantic_name",
        "was": "vsync_wait（等待垂直同步）",
        "now": "bios_keyboard_buffer_clear",
        "why": "原15字节 56 be1a040000 66 8b06 66 894602 5e c3 "
               "将 word[BDA 0x41a] head 复制至 word[BDA 0x41c] tail；"
               "无端口读取或等待。@0x1b1e7 在关闭前后调用它清键盘队列。"
               "原登记 FUN_0004bac9／历史0x73595 仅用于追溯，不保存猜测ABI。",
    }, {
        "target": "evidence", "was": "135dd/1366a的4e381沿旧vsync名解释为呈现等待",
        "now": "bios_keyboard_buffer_clear只将BDA head复制至tail；12cea、135dd、1366a的各明确clear时点"
               "必须保留，不能用等待代替，也不能把普通四方向13460等待变成清键。",
        "why": "既有完整15B本体和source09的12cea/135dd/1366a/13185 caller交叉；"
               "按键队列、held/typematic与HOST为不同owner合同，当前命名更正不等于跨端全部输入资格。",
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
    }, {
        "target": "evidence",
        "was": "未区分原负数字形越界与合法字体索引",
        "now": "除glyph10特判外按font+32*index取字模；15f84的-6用%d格式，逐unsigned字符减48，负号得到index-3。font是111ba malloc整entry后fread并直接返回的base（25ce0存53a75），前96B不在该owner内。SDL负数反馈使用原FDOTHER[5]132负号资源安全投影；不读取owner外字节，不宣称与原负号未知像素等价。",
        "why": "15f84/4ed7a/111ba及25bf4 loader完整Domain与strictLE；FDOTHER4为58368B=1824*32、SHA3d9493f204254c58600f2b08beb47e372d2aa950d686a97f7ed73aa1f709c871。没有原版崩溃实测结论。",
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
    }, {
        "target": "evidence",
        "was": "系统notice首帧/稳态/结果使用详情底板或占位底色；caption坐标未由literal核算",
        "now": "selector1 March调用1956b(raw0.unit_id)，selector3 END调用1956b(75)，不是空焦点actor查找/默认Hero。普通FDTXT417/419首提示push0xA9F23，418/420/412后提示push0xAB6E3；VGA基址0xA0000，原位置分别(99,127)/(99,146)。先1956b原tile底框+reverse portrait，再15f84原glyph(205/76/bg74,linepitch19)，16559(0)，19953/197e5控制YES/NO，196cb恢复初背景。不可用detail_bottom详情frame替代，亦不可把0xcd字前景填满框。",
        "why": "既有完整canonical函数、caller/xref与validated relocated原字节；独立Domain确认首提示push0xA9F23及后提示0xAB6E3，扣VGA基址直接为(99,127)/(99,146)。C4旧实际窗口确认底色差异；本轮修复后的Windows完整页验收尚待，不以纯SDL回归代替原版。",
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
    }, {
        "target": "evidence",
        "was": "系统notice首帧/稳态/结果使用详情底板或占位底色；caption坐标未由literal核算",
        "now": "Save410/Load413/Leave415三条确认均1956b(75)，首提示同原dst0xA9F23；411/414/416或412追加同dst0xAB6E3，保原first prompt和portrait。410..420实际普通tokens不另含speaker控制；15f84/font4ed7a不减4/8坐标。",
        "why": "既有完整canonical函数、caller/xref与validated relocated原字节；独立Domain确认首提示push0xA9F23及后提示0xAB6E3，扣VGA基址直接为(99,127)/(99,146)。C4旧实际窗口确认底色差异；本轮修复后的Windows完整页验收尚待，不以纯SDL回归代替原版。",
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

    # B3：完整函数、调用链及新IDA Domain原始字节／xref复核。
    0x1BB8C: [{'target': 'semantic_name',
      'was': 'sub_1BB8C',
      'now': 'field_unit_inventory_item_append',
      'why': '完整80B与Give/loot/剧情等xref：按raw0..7找首flags80位为1的槽，整flag写0且写item '
             'ID，成功返回1，满8槽返回-1；无压槽/重算/acted/MP/EX。新9.5 Domain全函数与原始机器码独立复核。'}],
    0x1BFFE: [{'target': 'semantic_name',
      'was': 'sub_1BFFE',
      'now': 'field_unit_equipment_menu_execute',
      'why': 'EQT反复执行mode0库存选择；兼容时切装备并重算/重画，Esc关闭和释放缓冲；确认后仍同页同raw索引。 依据已发布完整canonical '
             'pC+xref及strictLE原始函数/表/fixup；仅profile1两场EQT运行校准，不推广全部职业。'}],
    0x1C142: [{'target': 'semantic_name',
      'was': 'sub_1C142',
      'now': 'field_unit_equipment_slot_set',
      'why': '按item ID<128/≥128分组清全flag为0，再selected flag=40；不做职业兼容或重算。 依据已发布完整canonical '
             'pC+xref及strictLE原始函数/表/fixup；仅profile1两场EQT运行校准，不推广全部职业。'}],
    0x1C1C3: [{'target': 'semantic_name',
      'was': 'sub_1C1C3',
      'now': 'field_unit_can_equip_item',
      'why': 'actor+20→7-byte profile row；item.category仅比前6B，返回1/0，不使用第7B。 依据已发布完整canonical '
             'pC+xref及strictLE原始函数/表/fixup；仅profile1两场EQT运行校准，不推广全部职业。'}],
    0x1C9DD: [{'target': 'semantic_name',
      'was': 'sub_1C9DD',
      'now': 'field_mp_heal_apply',
      'why': '完整172B及20c6f/code11的xref：先整数9*power/10，再一次RNG%100*power/1000，加oldMP并夹mp_max，满值也耗RNG；原caster '
             'MP不扣费，targetID<75时实际恢复差*40*targetLV/mp_max仅累全局53EC8，经item dispatcher清除，不改unit.EX。'}],
    0x20C6F: [{'target': 'semantic_name',
      'was': 'sub_20C6F',
      'now': 'field_item_use_effect_execute',
      'why': '完整1043B、玩家1bbdc与AI15055的xref和新9.5 '
             'Domain/raw：按item+0dh分派5..24全20个码；5/6/7/8/9/10/11/17/18/19耗槽，12/13/14/15/16/20/21/22/23/24不耗；23复用2218a扣magic23费用，其余不得泛化扣MP。末53EC8全局归0后收败亡/清理/掉落，不清unit.EX；acted在玩家caller。新功能运行验收与B6完整演出/RNG另列。'}],
    0x21082: [{'target': 'semantic_name',
      'was': 'sub_21082',
      'now': 'field_item_permanent_stat_apply',
      'why': '完整152B仅20c6f的xref：向首raw '
             'target的给定offset作WORD+=power(16位回绕)，浮字/清呈现后仅caster1b750重算，再1b8e7压一次源槽。code8/9/10/17/18分别offset37h/39h/3eh/42h/46h，code19 '
             'offset3bh跨MV/EX后由caller恢复EX。无随机或HP随max增补。'}],
    0x2218A: [{'target': 'semantic_name',
      'was': 'sub_2218A',
      'now': 'field_actor_teleport_execute',
      'why': '完整201B与15311/1cff0/20c6f的xref：只用targets首raw，先focus该actor，1ca89(caster,23)扣一次费用，先22253(target,255,255,oldxy)，再聚焦global51CF9目的地并22253(target,destxy,destxy)，display归1；临时全局贡献由调用侧处理，不是物品独占helper。22253共享栈布局仍以raw读写为准，不从猜测pC参数造ABI。'}],
    0x4E88E: [{'target': 'semantic_name',
      'was': 'sub_4E88E',
      'now': 'field_equipment_profile_get',
      'why': '返回object3 0x6188a+7*profile，不在getter中做边界或兼容检查。 依据已发布完整canonical '
             'pC+xref及strictLE原始函数/表/fixup；仅profile1两场EQT运行校准，不推广全部职业。'}],
}



# B5完整函数语义：源记录仍保留历史误名/说法，不反向改写历史证据。
# 2026-11-05在既有9.5 Domain库完整读取函数、caller/callee及指令，
# 原字节逐段严格LE映射复核；真实数据库应用/重导另有本阶段收据。
_B5_CONFIRMED_NAMES = {
    0x1145A: ("sub_1145A", "field_persistent_unit_combat_stats_recompute",
        "完整172B；112a5持久构造与11506结算同步两caller。从base+37h/39h/3eh读取AP/DP/"
        "命中基值，八槽仅按flag40累加item+1/+5/+3/+7，最终只写record+48h/4ah/4ch/4eh"
        "四WORD并保留16位回绕。hidden槽仍可参与，不检查装备种类、不套战场1b750的状态"
        "倍率、不动HP/MP或其它字节。"),
    0x11506: ("sub_11506", "persistent_roster_sync_from_battle",
        "完整176B；22ef6及后续结算caller。按active raw顺序嵌套roster按text_id同步整80B，"
        "text_id0的hidden active不覆盖；清六状态、flags仅留hidden、存活HP和全部MP恢复max，"
        "再1145a重算。不是按unit ID或side同步，未匹配和未用表尾不写。"),
    0x12263: ("sub_12263", "field_completed_chest_tiles_apply",
        "完整121B；10010活动Load与190ac特殊格完成caller。逐格只选terrain flags&60==20且"
        "completed[cell.byte2&31]非零，将整个cell WORD模加1、byte2清0、byte3保留；"
        "不改metadata或completed本身，不把40类宝藏也改图。"),
    0x190AC: ("sub_190AC", "field_special_cell_execute",
        "完整1215B；18d8c的WAIT后路径。flags20/40未完成格421/428确认；item分支422/429"
        "文字完成后append并按成功标completed，满槽可经423选择替换，旧ID写回metadata参数，"
        "425同时引用新名-4/旧名-5；money分支426/430或空参数427/431，param0保留既有"
        "53AE1并仍加资源。完成后12263；No/Esc只412取消，不消费特殊格。"),
    0x1AA1D: ("sub_1AA1D", "field_loot_descriptors_execute",
        "完整726B；玩家physical/Magic/Item与AI physical/Magic的明确caller尾。按raw队列处理"
        "3B descriptor：0文字432后append/满槽433替换，1文字435后加资源，2延时回调，3"
        "本stage片段。mode0/1 recipient非side2立即结束整个队列；不重算装备stats。"
        "0队列/早退共享22bbe普通epilogue，不是状态治愈调用；不固定Hex-Rays猜测ABI。"),
    0x1B6B7: ("sub_1B6B7", "field_defeated_loot_collect",
        "完整107B；物理、玩家Magic、Item尾caller。全active raw顺序收nonhidden、HP0、"
        "descriptor.mode!=ff的完整3B；返回count经1b64d共享尾，不从伪C的void误推无返回值。"),
    0x1B653: ("sub_1B653", "field_defeated_story_loot_collect",
        "完整100B；15311 AI Magic专属caller。仅收nonhidden HP0且mode3的完整3B，"
        "raw顺序与count共享尾1b64d；不能等同1b6b7收物品/金币/回调的完整集合。"),
    0x22EF6: ("sub_22EF6", "field_stage0_victory_execute",
        "完整65B；main的stage0结果分派。先完整呈现本关FDTXT[1]fragment9，再11506"
        "同步持久表，最后stage=1。不是呈现到首句就可交接，也不是完整所有关卡结算。"),
    0x22E5C: ("sub_22E5C", "field_defeat_game_over_present",
        "完整154B；main败退caller。stopmusic→tick1→fadeout→loadFDOTHER79→clearVGA0→"
        "frame0→fadein→tick9→frame1→tick36→释放。79两帧header+9 B24RLE保留skip，"
        "frame1前不再次清屏、不另载palette，不能把decoded0一般化opaque0。"),
    0x206C5: ("sub_206C5", "field_stage1_battle_result_check",
        "完整66B；117e7/1a866与两AI outer的stage结果表xref。先205b4普通胜败检查，"
        "再按raw5..10扫描hidden；六人全hidden时无条件结果设1败北，覆盖先前普通结果。"
        "其中任一仍可见即返回，不检查HP/side或把villagers当persistent roster。"),
    0x26152: ("sub_26152", "field_town_prepare_execute",
        "完整1178B；main结算和标题手工LOADcaller。释放活动资源、从persistent重建FDICON，"
        "按stage-1准备记录开五项town；NoTown另分支四槽/部署。type0的酒店/武器/出口/道具/"
        "教会五项，左右反向循环，26x31完整表与FDOTHER10/11/13及495..499文本独立复核。"),
    0x265EC: ("sub_265EC", "field_town_view_render",
        "完整290B；26152逐键/4tick刷新caller。大buffer456stride先复制town底图，局部"
        "244,162画62x26面板、252,168画495+selector，再首roster的FDICON phase0/1/2/1"
        "marker；312x192拷贝至VGA4,4。不是五项命令图标sheet，不修改roster。"),
    0x2670E: ("sub_2670E", "field_town_selection_execute",
        "完整648B；26152选择确认caller。selector2用DATO75/fragment513确认并处理部署容量，"
        "其它入口十步缩放/渐暗后分派酒店/店/教会，返回恢复town；复核全部分支但本B5"
        "运行范围不因此宣称商店/教会和>16部署实现完成。"),
    0x26996: ("sub_26996", "field_prepare_bottom_page_close",
        "完整119B；标题、酒店、商店等底部选择页caller。i1..5调用1974c滑出静态页面，"
        "恢复53C5F全屏初始背景并释放三个buffer；不是field_bottom_notice_close的战场刷新尾。"),
    0x29300: ("field_manual_slot_dispatch", "field_hotel_menu_execute",
        "完整800B；2670e酒店入口。13frame0背景、DATO129及585/586问候；四项分派"
        "29620持久成员详情、2968d手工Save、2986f手工Load、415/416离开确认。"
        "Esc回town，不把酒店退出项与此取消合并；全部26eda菜单输入与zoom-out尾已读。"),
    0x29620: ("sub_29620", "field_roster_detail_select",
        "完整109B；酒店29300/教会29daa的xref。设53F5E=roster_count，循环27d33成员选择"
        "→17aed详情，返回恢复portrait origin并重载DATO129；取消退出。无Save/文件写入，"
        "原误挂field_manual_slot_save已撤回，真正writer在2968d。"),
    0x2968D: ("sub_2968D", "field_manual_slot_save",
        "完整482B；26152/29300 caller。读完整22987B容器，经29bcb选择，将persistent整0xa00"
        "和meta0..9写选槽，重算checksum/XOR后写盘；成功660需显式ACK，再重开保留selected，"
        "Esc才结束writer。其余未知meta/其它slot保留，slot3尾4B与checksum交叠。"),
    0x29AB2: ("sub_29AB2", "field_manual_slot_rows_draw",
        "完整281B；29bcb四槽picker唯一caller。每行119+19*i，549 x10动态FONT数字，"
        "empty514 x88，否则514+stage x40及550+stage x130；selected201/普通205，"
        "shadow76/bg0。每行显式53AE1=i+1，整页后为4；不分配、不读取文件、不等键。"),
    0x32D18: ("sub_32D18", "field_stage1_entry_play",
        "完整1281B；main/标题的stage1入口。205da从camera/focus0初始化，PAN13,11，"
        "脚本9/10、FDTXT[2]片段0/1、group1/脚本11/片段2，重置方向PAN6,12，"
        "强制group2/脚本12/重置方向/片段3后focus actor0。25个已知等待及21几何由原始"
        "脚本/placement独立构造；此命名不将逻辑近似camera或SDL ctor当原版oracle。"),
    0x34778: ("sub_34778", "field_stage1_turn3_execute",
        "完整160B；stage1 metadata的turn3 action5表xref。PAN9,1、延时100、强制追加group3、"
        "脚本13、延时200、stage片段4，完整文字返回后raw5..10的AI锚点+35h/+36h设26/15。"
        "不更改这六人的behavior字段，不把锚点更新提前到提示之前。"),
    0x34738: ("sub_34738", "field_stage0_ally_side1_event",
        "完整64B；stage0 descriptor/cell回调4表xref，1aa1d/190ac及controller/AI派发caller。"
        "先写active raw13的side字节+6=1，再完整播放当前stage片段7；不追加persistent记录、"
        "不将该actor直接设玩家side2，也不重写其它80B字段。"),
}
for _va, (_was, _now, _why) in _B5_CONFIRMED_NAMES.items():
    CORRECTIONS.setdefault(_va, []).append({"target": "semantic_name", "was": _was,
        "now": _now, "why": _why + " 依据：2026-11-05既有IDA9.5 Domain完整函数/xref与strict LE原字节一致；运行范围另登记。"})
CORRECTIONS.setdefault(0x4E809, []).append({"target": "evidence",
    "was": "准备记录按stage*31或file旧偏移读取，可能推成29行",
    "now": "完整24B原码先DEC stage再乘31，fixup指向canonical6238d；stage1对应row0。"
           "直到下一独立getter626b3的完整806B为26行，安全query仅1..26；27..29由523e7"
           "NoTown门跳过本getter，不能据此多读三行。保留现中性名，不猜getter ABI。",
    "why": "4e809 raw 558bec8b450848ba1f000000f7e28d158d23000003c25dc3；"
           "Domain4e819 fixup→object3:238d与strict LE逐段来源一致。"})
CORRECTIONS.setdefault(0x12C60, []).append({"target": "evidence",
    "was": "只查首个匹配active记录，hidden时丢弃portrait来源；无active就直接用text_id作DATO编号",
    "now": "active按raw扫描，每次text_id匹配先保存全局record指针，首visible立即返回raw；"
           "若全为hidden返回-1但保留最后匹配active指针。只有active完全无匹配才扫描persistent，"
           "同样保留最后匹配。-17/-18据该指针+7取DATO；-1只阻止focus，不阻止portrait。"
           "39仍走原特殊portrait分支。",
    "why": "完整138B与15f84的-17/-18调用、已发布Domain指令；本B5Scene三类edge纯回归分别锁定"
           "首visible、最后hidden与persistent fallback，不把这些纯测试当作全部原版运行。名称保持原登记。"})
CORRECTIONS.setdefault(0x16C57, []).append({"target": "evidence",
    "was": "所有等键页都带18/19箭头，或任意非0参数都启用箭头",
    "now": "精确a1==1才首次画18、每3个2tick相位更新18/19，并在返回前用frame13擦除；"
           "a1=0不画/不擦箭头，仍执行RNG%30+2、portrait0/3、水DAC及BIOS读键。"
           "文字-3传1；190ac/1aa1d结果/获得消息的外部wait全部传0。",
    "why": "完整461B三处显式==1比较及190ac/1aa1d全部wait实参；本B5chest-empty原版"
           "实际422页累计文字5123px/portrait6400px均通过而无箭头，旧门失败收据保留；"
           "修后对原填充+累计文字的140px无箭头区域严格比较，不删除像素门。"})
for _va in (0x190AC, 0x1AA1D):
    CORRECTIONS.setdefault(_va, []).append({"target": "evidence",
        "was": "外部dialog_wait_key等同文字-3，或每个结果fragment都重新清空对话框",
        "now": "外部等键均dialog_wait_key(0)，不带18/19箭头；同一bottom框保留先前文字。"
               "190ac的421/428首行与422/429/426/430第二行必须累计，仅显式close→open才清页。",
        "why": "已发布完整函数的696099/702179文字目的偏移、open/close调用与wait实参；"
               "B5真实原版及对应SDLchest-empty/swap/list-cancel的资源门另在运行收据登记。"})
CORRECTIONS.setdefault(0x1B41D, []).append({"target": "evidence",
    "was": "当前SDL总览只开放stage0，stage1没有原资源内容合同",
    "now": "原函数关卡数字为stage+1，胜/败条件为597+2*stage及598+2*stage；"
           "stage1正常使用599/600，沿既有四底板/数字/资源/阵营人数owner。stage16另有私有分支，"
           "不据此宣称所有关卡总览已通用验收。SDL本B5直接扩已证stage1，不新增禁用透明图例外。",
    "why": "完整published sub_1B41D及1B5F1计数/原资源；C3准备链真实SDL在总览能力门处失败，"
           "因此保两端原secondary0默认协议并修生产stage索引。该说明不替代其后实际双端验收。"})

CORRECTIONS[0x1BBDC].append({'target': 'evidence',
 'was': 'Give可把side2候选直接当收方或交换保装备flag，变更后COMMAND取消仍回原格',
 'now': 'Give range1/minManhattan1/filter3排除整中心格，仅hidden/side门；确认后12c0d再取该格首可见raw(不重筛side)。空槽append '
        'flags0；满槽两侧各remove后append flags0；仅giver重算。成功53C53=1，零步/移动最终COMMAND '
        'Esc均markacted→lookup1，移动不回滚；取消子路sender仍重算。',
 'why': '完整1bbdc/1bb8c/1b8e7/18d8c/18890和新Domain指令/xref；1bbdc '
        'code23额外身份门读取text_id+8==24与mp_max+46h>=20，而非currentMP；首次目标后第二次selector6选空且可走目的地，display仍1/frame0。Use '
        'area确认14742为纯Manhattan，最终收集14818仍地形传播，两者不可合并。'})

CORRECTIONS.setdefault(0x4E4F6, []).append({"target": "evidence",
    "was": "mode2命中hostile后停止，或在剩余预算／已访问检查前覆盖witness；剩余0等同未访问",
    "now": "4e680先减地形成本并以signed比较剩余预算，map byte7初值FF；有效剩余0第二次等值访问"
           "也受mode!=1门拒绝。mode2通过该门后才由4e703记录hostile坐标，4e6e5明确CLC，"
           "4e5cc继续穿过hostile递归，不受40阻挡或80 ZOC停止。名称和持久ABI不变。",
    "why": "IDA9.5 Domain完整4e5cc/4e680/4e703原指令，4e6a5..b2及4e6de..e6的明确分支；"
           "两敌单通道和2x2零剩余反例检出旧实现。B5 stage1真实十二坐标差异另需14b78清图长路"
           "合同一并恢复，不能把局部修复单独称为整条路径原版通过。"})
CORRECTIONS.setdefault(0x14B78, []).append({"target": "evidence",
    "was": "预算28的mode1 long route仍从units重建占格／ZOC，因而不能到occupied请求anchor",
    "now": "normal失败后14c53先4df4c清40/80及byte7，14c7d直接预算28/mode1探原anchor，"
           "期间未调用145cd，所以长路没有单位阻挡／ZOC。14c9a再次clear后14ca6才重建145cd，"
           "14cc7计算本回合range并扫描完整长路取最后可达格，随后友方终点阻断／排序／普通路径不变。"
           "SDL clear-unit-flags组合位只表达此调用环境，不是新增原版mode。",
    "why": "完整14b78 Domain指令与14121/13a9f调用链；同一真实C10派生stage1文件三END，"
           "旧probe重现六终点偏差，两项修复后的六终点精确等于native的13,5/11,3/12,4/15,3/14,4/13,3，"
           "该headless probe RNG0且源文件不变。真实双端重验仍按后续GUI收据单列。"})

CORRECTIONS.setdefault(0x1548E, []).append({"target": "evidence",
    "was": "AI物理提交只需改cursor，或可在actor调度层统一补一次聚焦",
    "now": "完整exchange在154ad先12d7b聚焦攻击者，154c6才执行14b78移动，154de再12d7b"
           "聚焦所选防御者，然后进入交战；不能用直接写cursor代替12d7b→12cea的逐格镜头留边。"
           "13a9f没有统一动作聚焦，behavior8无动作也不能凭调度主动改焦点。名称与持久ABI不变。",
    "why": "IDA9.5 Domain完整496B、13a9f/14ef0调用xref与154ad/154c6/154de原指令；"
           "12d7b在12da3调用12cea。B5 C20实际原版完成、SDL turn11 quiet失败，81块地形共46656px"
           "唯一确定SDL camera5,1，而原版0043 camera5,0的422px资源门及Save11保留Hero15,0。"
           "这是原动作聚焦遗漏的依据，不改变oracle或把后续修复预记为实机通过。"})
CORRECTIONS.setdefault(0x15311, []).append({"target": "evidence",
    "was": "AI法术的聚焦可由统一调度或一次cursor赋值替代",
    "now": "评分门通过后15360先12d7b聚焦施术者，构造目标后153c2调用12cea聚焦53C27/53C2B"
           "选定中心，再清display进入效果。AOE中心不等于最后raw目标；不得把统一actor调度"
           "或直接cursor写入当作该调用序。名称与持久ABI保持。",
    "why": "IDA9.5 Domain完整381B、13a9f/14ef0的xref、15360与153c2原call字节及权威全函数"
           "伪C共同确认；12d7b/12cea分别负责actor坐标与镜头留边。此为静态合同，"
           "AI法术修复的运行范围按实际候选／图形收据另记，不借物理C20失败冒称法术实跑。"})
CORRECTIONS.setdefault(0x15055, []).append({"target": "evidence",
    "was": "AI物品提交只需给cursor赋值，或可在调度层统一聚焦",
    "now": "150ba先12d7b聚焦使用者；普通range小于16分支1515b调用12cea聚焦53C37/53C3B"
           "选定中心，随后152e5进入20c6f效果。range大于等于16另有15287缩放中心及152dd"
           "首target聚焦，不能抹成普通分支或不顾原调用时点统一聚焦。名称与持久ABI不变。",
    "why": "IDA9.5 Domain完整700B、14ef0调用xref、150ba/1515b/15287/152dd原call字节与"
           "完整伪C分支一致；静态字节逐函数等于已验relocated镜像，raw-unrelocated摘要独立保留。"
           "普通AI物品聚焦与后续效果/RNG是不同owner，不能以本轮修复预称完整演出或运行已验。"})

CORRECTIONS.setdefault(0x129EC, []).extend([{
    "target": "semantic_name", "was": "sub_129EC",
    "now": "field_actor_terrain_overlays_draw",
    "why": "IDA9.5 Domain完整218B与127a9/1366a/1db65/32999四caller，逐raw读取actor坐标、"
           "direction+3与walking+4，过滤hidden/1f183后才调用12ac6重绘局部遮挡。原中性名保留"
           "为历史登记，不据void伪C或旧自动fastcall类型猜ABI，也不修改持久类型。",
}, {
    "target": "evidence", "was": "terrain attr80可在actor绘制后全地图重画，连空格光标也应被覆盖",
    "now": "只遍历非hidden且1f183返回0的actor，恢复自身(x,y)和上格(x,y-1)。walking+4非0时，"
           "direction0另恢复(x,y+1)，1恢复(x-1,y)/(x-1,y-1)，2恢复(x,y-2)，其余恢复"
           "(x+1,y)/(x+1,y-1)。无HP/acted额外过滤。1f183对unit28强制返回0，其余profile19或"
           "race4/5跳过；没有actor的远处attr80格不会因本遍历盖住cursor。",
    "why": "完整原函数及12ac6/1f183的Domain指令/xref逐项闭合。B5 C22 actualSave12两端存活"
           "身份/位置同，world11,8附近无actor：原版裸cursor204px通过且拒cursor+281合成，"
           "SDL却以455px FDSHAP281及527px合成通过、裸cursor坏132px。这个运行反例证明全图"
           "overlay旧实现错误，不是oracle模板缺帧；修复候选的实际重验另行登记。",
}])
CORRECTIONS.setdefault(0x12AC6, []).append({"target": "evidence",
    "was": "map_tile_blit_visible可独立遍历全图并使用通用terrain动画规则",
    "now": "保留既名。它只绘caller指定格，接纳camera_x-1..camera_x+51A87及"
           "camera_y-1..camera_y+51A8B+1且y非负的范围；读取cell低10位terrain_id，"
           "仅attr08按53A40加2*phase，attr80才绘FDSHAP frame+1。cell.byte7==FF走原图，"
           "否则按51A97[53C1F]作同一格LUT；本helper不决定actor邻域或全图遍历。",
    "why": "IDA9.5 Domain完整327B及唯一caller129ec；12ac6原分支、sheet offset+10和byte7判断"
           "与11eee底图→selection→127a9 actor→129ec局部恢复顺序一致。C22空格cursor反例只支持"
           "已测455/527/204像素及局部owner，不宣称所有遮挡/动画/RNG或新修复已实机通过。"})

# B6：完整Domain指令/资源目录与局部原码向量；不是新的原版GUI验收。
CORRECTIONS.setdefault(0x22046, []).append({
    "target": "evidence", "was": "圆宽nearest且上半矩形radius/sqrt(2)，actor先于全部mask",
    "now": "两次219ad都传scale16，CHP向零截断：扫描半宽floor(sqrt(r²-dy²)*16/10)，"
           "严格cy-r<y<cy+r且水平[cx-half,cx+half)，裁剪312×192；上半矩形半宽"
           "trunc(r*binary64(1.6))。第一次完整椭圆后于22077调用127a9重画actor/前景，"
           "再绘下半椭圆及上半矩形，三者不能在已合成actor后统一叠加。",
    "why": "IDA9.5 Domain完整22046/219ad/37af4/3ca4c/2189a和常数501f0/50208；"
           "无hook无patch执行原sqrt/CHP，scale12/16各65537向量、rectangle4097向量、"
           "大半径1900向量及4RC×7 signed CHP/restored CW。读集SHA "
           "990a57735c76ca975f3054cec7de8de3a22f72b929b0a7e6de5c7fbe518f9648，"
           "向量报告SHA dc13930868b3a6ea792256e2da38e58f7118260eba42ce54846d0878aae3c0c5。"
           "已测r<=4096的整数模型circle=isqrt(64*n/25),rectangle=8*r/5；"
           "不推广2189a的scale12或所有runtime控制字。"})
CORRECTIONS.setdefault(0x2FF01, []).append({
    "target": "evidence", "was": "费用可固定扣一次或由纯effect再次扣除；演出不消费共享RNG",
    "now": "保留规范名。caster按unit_id*3先取idle，施法取+2，首WORD零才退+1；"
           "30e9d仅遍历intro_end，part+4精确1才调用1ca89费用。player0..67真实资源"
           "52种一次、7/39四次、12/28/49两次、7种零次，4种最终仍empty；empty是合法"
           "容器哨兵而非可伪造part。费用按WORD逐次减，可回绕。玩家1cff0的ID0"
           "始终调此wrapper；AI15311的mode分派另验，不把物理FULL配置套在玩家0。"
           "Magic0的2b996 phase5在hit时给7个impact，effect RNG后/下一target前各消费"
           "一次1-rand()%3；miss不消费impact，skip/静音不能改这些业务次数。",
    "why": "完整2ff01/30e9d/1cff0/2b996 Domain及408条目元数据独立分类；"
           "player68 caster读集SHA1954de8e636ce19ef144444092ebd3cec9c118f618ad37ec5674e35fa3a2965c。"
           "实际unit9 selected28 intro11仅part6一次；这只是该actor，不是通用默认。"
           "通用player资源读集不扩大正式B2仅unit9的Magic0资格。"
           "当前新增说明是静态/局部证据，完整帧序及音效仍须B6运行验收；"
           "旧entry/core记录只为历史alias，不支持把Watcom stack-check序言拆为独立函数。"})
CORRECTIONS.setdefault(0x1E856, []).append({
    "target": "evidence", "was": "所有物理播放模式最多两击",
    "now": "无演出1e856保留type3与3%追加机会合并、最多两击；完整2ebe1流程则"
           "独立的3%追加与type3各加一次，最多三击。type3追加在hit检查前确定，"
           "实际target HP0停止后续击；不能以跳过画帧来切换结算模式。",
    "why": "完整2ebe1/2f7b6及2ec34/2ecae/2ecc4/2ecd0/2ed0c/2ed14/2ed25/2ed2d"
           "和2f97f..984原指令读集；本次只确认同一RNG流的不同原caller规则，"
           "未将纯逻辑三击测试冒称原版已实际观测三击。"})

# B6只补已读raw/xref/资源字段的依据；这些函数仍用IDA中性名，未确认ABI。
_B6_NEUTRAL_EVIDENCE = {
    0x2EB9F: (
        "entry+8为part DWORD相对offset目录；part+0/+2是unsigned WORD绘制x/y，"
        "图像宽高及RLE从part+9开始，交给4e98d。保留sub_2EB9F，不把坐标当宽高。",
        "完整函数及2ebe1/2e9a8/30e9d等caller；真实408条目、2118part逐项边界/RLE复核。"
        "合法三字节00000a空哨兵、intro_end大于count及duration0不据此一律判结构损坏。"),
    0x2EBE1: (
        "保留sub_2EBE1。完整物理流程先消费一次3%机会，再于首次2f7b6的type3标记"
        "额外加一，最多三击；HP0中止后击。part+4非零分段写HP并夹0，+5为cue，"
        "+6为本part绘制tick数，+7 bit0与side共同决定双层前后；idle计时器为byte，"
        "不能把duration0偷换成统一1tick。split与普通布局走不同640/400stride。",
        "完整raw/xref、5266c/52684抖动表和2f7b6输出；以canonical VA定位原分支。"
        "输出结构只按原写集描述，不保存推断的C参数或结构类型；完整三击尚非原版运行验收。"),
    0x2E2B0: (
        "保留sub_2E2B0。完整物理场景按actor side及当前stage/terrain选择BG/TAI，"
        "加载两方FIGANI idle和攻击条目，按split字段走2e9a8/2ebe1，并在原反击门后"
        "交换双方继续；正常退出重取当前战场资源、重画地图和当前HP信息后渐亮。"
        "共享pop epilogue2e956不是独立入口。",
        "完整1707B、1548e/18d8c等caller及52470/526e3相关表；"
        "terrain immune/profile例外与unit28分支直接来自原指令，不把stage0捕获当通用默认。"),
    0x2F7B6: (
        "保留sub_2F7B6。计算完整物理单击的miss/critical/status/type3及raw damage输出，"
        "type3在命中前置标记；状态可在hit RNG前写入。玩家攻击敌方时按原门覆盖53ec8。"
        "本函数局部算HP余量用于贡献，但目标HP实写由2ebe1的part marker驱动。",
        "完整raw、唯一2ebe1 caller、装备effect2/3/4及1e856共享公式对照；"
        "raw damage与HP夹0后的delta分开，未把单次formula测试提升为原版音画通过。"),
    0x30E9D: (
        "保留sub_30E9D。仅循环caster clip的intro_end；part+4精确1触发1ca89费用和"
        "状态板重画，费用不是按目标或固定一次。part+6控制逐tick绘制，side决定"
        "caster/idle先后，idle byte计时器自然回绕；duration0不执行本part内帧循环。",
        "完整raw及2ff01 caller；68个player目录中52个一次、7个零次、3个两次、2个四次、"
        "4个最终empty，unit9空29退28且只part6标记一次。该目录结论不扩大B2资格。"),
    0x2B996: (
        "保留sub_2B996。Magic0特效dispatch的phase3初始化7条错开的-2*j状态，"
        "phase4/5按524ee层表分别绘制，phase5逐条加一，恰到9返回impact。"
        "2ff01调用方在命中effect之后、下一target之前为7个impact各消费一次1-rand()%3；"
        "RNG不在只读present层重抽。",
        "完整4454B函数与2ff01/31266 callers；仅确认Magic0所用case3/4/5合同，"
        "其它magic dispatch未据地址/函数大小命名或声称已实现。"),
    0x21AD9: (
        "保留sub_21AD9。恢复13入口先请求既有SFX11，再调用21eb1的(1,2)视觉序列，"
        "随后转21b18并固定magic_id13；不能把这些演出步骤当作第二次恢复/扣费。",
        "完整63B及1cff0调用分派、21eb1/21b18 callees；SFX是否实际播放仍经原25a96设置门。"),
    0x21B18: (
        "保留sub_21B18。清浮字队列53ec4，先收集/聚焦和actor flash，再于目标循环前"
        "调用1ca89一次；逐raw目标执行恢复及1e0db入队。恢复原始量与夹顶后的HPdelta"
        "分开，满HP仍可有非零浮字原量，不能重抽spread或重复提交业务。",
        "完整函数raw/xref、13记录和1c916/1e0db写集；跳向21190的共用尾不臆造独立ABI。"),
    0x21EB1: (
        "保留sub_21EB1。工作页先生成terrain，9到1 LUT逐帧调用22046；"
        "半径按caller步进，停200ms后用最后半径作3到9 LUT，尾部重画当前地图。"
        "22046内部夹入actor/前景，不能以旧nearest/sqrt2几何或actor后统一mask替代。",
        "完整405B、21ad9等caller及22046/219ad/CHP原码向量；只读合成不提交Game状态/RNG。"),
    0x1DF58: (
        "保留sub_1DF58。53ec4非零时保存工作页，22帧逐次恢复底页并绘制已入队glyph，"
        "位置来自actor世界格及5202c固定纵向轨迹，帧后2ms，末500ms；无队列不播放。"
        "此处消费显示队列，不重算伤害/恢复或消耗effect RNG。",
        "完整387B与20c6f/2111a等caller、1e0db四glyph队列写集和5202c数据xref；"
        "浮字轨迹与保存值/真实恢复净值分别登记。"),
    0x1E0DB: (
        "保留sub_1E0DB。仅按原视区条件为一个actor追加4个十进制glyph槽，"
        "各槽有actor raw、5*i+2横移和字形索引；空白槽为0，53ec4加4。"
        "数值是caller提供的原始量，不在此按HP变化重算。",
        "完整257B及21b18/物品等caller；sprintf长度及offset写集不等于确认原C ABI或扩展队列容量。"),
    0x219AD: (
        "保留sub_219AD。圆扫描线先原sqrt，再乘caller scale除binary6410.0，"
        "真实CHP临时RC向0并恢复CW；22046传16，2189a传12，两调用范围不能混为默认值。"
        "strict y端点和半开x跨度裁到312乘192；不是nearest整数圆。",
        "完整241B及22046/2189a caller，原37af4/3ca4c和501f0常数；"
        "readset990a5773及vectors dc139308包含两个scale各65537向量和signed/CW复核。"),
    0x2E9A8: (
        "保留sub_2E9A8。按attacker side选择640stride工作页半边，i/j8到0以10px位移"
        "引入idle/TAI，逐帧dac darkness6*i；普通与split层不同。2e956为共享返回尾，"
        "不将跳转地址登记为新函数。",
        "完整caller2e2b0与原指令，25a96音效门独立；逐通道减暗度不等于比例brightness。"),
    0x2F4D4: (
        "保留sub_2F4D4。split切换的一侧按9到0再9到0两段，使用BG0/1/2交替与"
        "640stride工作页、32*i窗口偏移，中间清页重画actor信息和idle0。",
        "完整函数及2ebe1 caller；与2f631方向/端点不同，不按相邻地址或大小推测ABI。"),
    0x2F631: (
        "保留sub_2F631。反向split切换按1到9再1到10两段，使用工作页另一半、"
        "32*i偏移，并于中间清页重画背景/TAI/actor信息/idle0。",
        "完整函数及2ebe1 caller，原首/末帧端点明确；不借2f4d4的10加10帧对称近似。"),
    0x314DE: (
        "保留sub_314DE。非NULL clip且header+4非0时，以selector-1索引6字节"
        "ASCII 012345表，再按所得48到53号FDOTHER条目加载bank；0返回NULL。"
        "合法已证selector1到6，原函数无额外上界门，重写预检不能越表猜bank。",
        "完整75B、2e2b0/2ff01等caller及真实408目录selector分类；未确认值明确拒绝，"
        "不根据bank文件大小邻近推断名称或音色。"),
}
for _va, (_now, _why) in _B6_NEUTRAL_EVIDENCE.items():
    CORRECTIONS.setdefault(_va, []).append({
        "target": "evidence", "was": "未发布本次B6完整读集所确认的有限字段/时序",
        "now": _now, "why": _why +
        " 本轮IDA9.5 Domain完整raw/调用读集SHA776a577fad2694ddc2dfcb006168c346c218c456226a5c5577ce21b252711067。"
        "仅静态及局部纯测试依据，原版B6图形/声音验收未由本次发布取得；中性名/持久类型不变。"})

# B6后续FULL10：1..6是2b996内的dispatch窗口，不建立独立函数或猜测ABI。
CORRECTIONS.setdefault(0x25B45, []).extend([{
    "target": "semantic_name", "was": "pal_mask_set", "now": "sfx_play_secondary",
    "why": "完整175B及2cafc/2ccf4/2ce1a等xref：与25a96相同的三门及"
           "bank+6/+10取样流程，但固定AIL sample handle为53ee8而非53ee4。"
           "先ail_end_sample，index非-1再init/address/loop/start，完全没有palette写入；"
           "原pal_mask_set仅保留历史alias，不按函数大小或调用邻近命名。"}, {
    "target": "evidence", "was": "调色板遮罩设置",
    "now": "第二AIL handle的数字样本replace/stop。保留原53ef1/51e62/54133三门，"
           "index=-1只停止；其它index按嵌套bank offset取u8无头样本并设置loop。"
           "与primary独立，不能因为名称曾含pal而转为DAC操作。",
    "why": "本次Domain完整25b45/25a96指令、字节及callers对照；参考data handle"
           "53ee8/53ee4与AIL callees直接确认。仅更正函数语义名，不写类型/推断ABI。"}])
_B6_FULL_FX_EVIDENCE = {
    0x15311: (
        "15311在id<10且byte53af9==0时进入2ff01完整演出，其余走effect表；"
        "这不等于玩家1cff0的ID0配置分派。FULL费用沿caster FIGANI intro标记，"
        "FX、effect、jitter仍共用原RNG；AI contribution只在1546a尾清零。",
        "完整15311及2ff01/1cff0/1ca89读集，原153f1附近分支和1546a写点；"
        "先前‘SDL仅无演出分支’是历史支持范围，不能当原版FULL无费用证据。"),
    0x2FF01: (
        "保留field_magic_battle_animation。524c6按id选择十个phase0..8 handler，"
        "phase0/3/6返回循环数，1/2、4/5、7/8配对。状态跨全部target及31266九tick"
        "过渡保留；不能逐target重新init。effect后保存after并恢复before，只有raw伤害非零"
        "且phase5返回1才按526bc分母推进signed HP；达到分母后不再写HP，但每个后续"
        "impact仍消费1-rand()%3。ID8后项可effect有伤害而没有impact，HP仍为before。",
        "完整2ff01与31266原写序；526bc十字节为7/8/6/13/6/6/5/5/16/20，"
        "不同于首target impact7/8/6/14/6/9/11/7/16/27。526c6只有side非零9项，"
        "526cf为side0十项；ID9用真实44号31part，后续target会请求part31+，"
        "SDL因此明确拒绝side非零9及9多target，不把越界draw stub当合法资源证据。"),
    0x2B996: (
        "保留sub_2B996及原函数边界。524c6的ID0..6窗口分别为2b996/2bb33/2bd6c/"
        "2bfd9/2c217/2c441/2c67d，IDA均归此owner；已核全部phase0..8，不能再写仅Magic0。"
        "ID4/5 phase0各消费6个原16bit RNG，首target main各4个，过渡及后target继续"
        "同state并消费FX RNG；它们与effect hit/spread和impact jitter是不同调用点，"
        "skip、emit=NULL和静音不能省略。",
        "十dispatch原raw窗口与29组真实FX时长/原RNG/math局部机器向量；0..6"
        "首impact为7/8/6/14/6/9/11，3/5/6后target分别可20/11/12。"
        "2c67d使用原sin/cos/CHP常数及非对称位置交换，不凭伪C转换猜ABI。"),
    0x2CAFC: (
        "保留sub_2CAFC，524c6的ID7 owner。phase0设三条实际处理的age/slot及"
        "共享flip/next；phase2/5/8每两次推进，slot按5261e十位置轮换。phase6设stop"
        "只阻新slot，并继续16tick尾；main32tick，第一target7次impact、后续8次。"
        "primary/secondary cue分别走25a96/25b45，没有FX RNG。",
        "完整504B、524c6 xref及原phase窗口向量；方向由side调整x+130，"
        "状态跨target/九tick过渡，不把返回impact次数误当HP分母5。"),
    0x2CCF4: (
        "保留sub_2CCF4，524c6的ID8 owner。phase0初始化16个-2*j age；phase2与5"
        "绘制52646的part offset并推进age，恰++age==4返回impact。phase3仅返回34，"
        "不会重置age；前导3tick/首target后状态保持，后target可0个impact。"
        "age原值0/4分别提交primary1/secondary2 cue，无FX RNG。",
        "完整294B及真实side0/2多target原kernel；首target16impact，后两target均0，"
        "不能用effect raw after覆盖未由原impact写入的实际HP。"),
    0x2CE1A: (
        "保留sub_2CE1A，524c6的ID9 owner。phase0 frame=1，phase3只返回60；"
        "phase5绘part(frame>>1)后递增byte，16<newframe<44返回impact，共27次；"
        "frame6/36分别提交primary1/secondary2 cue。前导/收尾各20tick并切换blink，"
        "不含FX RNG；不得把impact27当HP插值分母20。",
        "完整278B与真实side0 FX44/31part局部向量；side非零表缺第十项、九tick"
        "过渡后的多target会越真实目录，相关stub失败原件保留且不列为合法29向量。"),
}
for _va, (_now, _why) in _B6_FULL_FX_EVIDENCE.items():
    CORRECTIONS.setdefault(_va, []).append({
        "target": "evidence", "was": "前批B6只发布Magic0或尚未发布FULL十handler的写序依据",
        "now": _now, "why": _why +
        " 原Domain dispatch读集SHA2cc7a9b08c1341395805477d07d9e7c6ddae68a876e51fd715266bcd9f05abed；"
        "29向量SHAedb866e44dda28deb224b73787264fa5a4e78dcfe047600e0faf701cb21715a6。"
        "局部机器执行使用draw/audio stub，未覆盖真实输入、全帧/声音；Game FULL自测另记，"
        "不借SDL测试确认原版图形，也不创建1..6新函数或保存猜测类型。"})


# G2追加：原机器指令与真实Game逐RNG差分，更新历史B6安全门的当前处理。
_G2_MAP_ACTION_EVIDENCE = {
    0x13FD4: "HP==max或raw25/26非零立即返回0，早退不写mode/focus/HP。其余mode0/focus/wait1，FDOTHER31 cue4，1da16 mode2闪烁/copy/wait1，mode0正常/copy/wait1，然后HP=min(old+max/5,max)、mode1、返回1。max/5可为0但成功仍封顶，max0且oldHP非0写HP0；无RNG或浮字。12cea总调用11cac(0)，信息板由51AAB选项及caller的51AAC上下文共同决定：玩家WAIT保留、AI END关闭，不能一概使用纯背景。",
    0x18D8C: "玩家物理分支直接2e2b0，未读53af9，算法及呈现均始终FULL；仅AI1548e按53af9选择1e856/1e98c SHORT或2e2b0 FULL。不能仅改renderer而保玩家SHORT攻击/RNG算法。",
    0x1548E: "SHORT先face主动者、mode0/map刷新/mode1，按初始合法反击face目标；1eb05后重置方向再1e856。目标存活且仍可反击才双方face、1e611、反击1e856，最后重置方向。155f3..155fb只push三实参，15600仅清0xc；1e611读arg_C不是已提供第四参。",
    0x1E611: "重画地图及普通actor、FDOTHER6 frame48面板，再1e739固定宽55；接着1e6a8读取caller未提供第四实参并用作单位索引。SDL最小安全扩展仅省这一次未定义索引HP条，保固定55与明确arg8第二面板HP条，不把未知栈值猜成actor。其余顺序保留，不称原未定义像素等价。1e640直call11eee会推进独立地形53A40门，1e648的127a9推进App raw26抖动；不调用1297d或水DAC门，不应把所有视觉时钟一起推进或全部冻结。",
    0x1E98C: "武器类别21指针落到6段有界84B序列，每段(frame,cue)对。hit首帧1da16 mode2在stride320上产生solid64、第二帧正常；miss无solid且声4，cue255无声。FX80ms保存/恢复背景；oldWidth=70*oldHP/oldmax，逐列递减到69*newHP/oldmax+1，8ms一步，无伤害浮字。原SHORT样本bank64由1a7bd加载，非Magic bank80。",
    0x1EB05: "FDOTHER6面板39..48共10次25ms，前9次保存/恢复底图，末次保留；其后1e7f6初始HP条直接写VGA，无额外等待或flip。SDL先写CPU HP条，和首次STRIKE的solid64及FX6合并上传，保80ms，不添加零时长中间present。",
    0x1DA16: "24x24专用actor绘制使用已缓存sprite与direction*3，idle/walk phase3映为1（不同于普通127e0的3映2），walking位移乘4；不套raw26抖动或acted灰色。solid实际使用stride低字节：456为200、SHORT VGA320为64，第四色参被4e127忽略。已证静止direction5以class*12+15+phase访问下一cache class的frame3+phase，必须按真实cache映射，不以unit ID加1；SHORT旧5在face/reset前不能被资源预检误拒。",
    0x2FF01: "G2取消历史ID9多target整动作拒绝：原目标收集逐actor，不按格去重。完整2ff01目标循环、完整31266九tick过渡、2ce1a和真实4ebe3已与public Game逐RNG快照差分；effect/贡献先提交、HP恢复后仅impact插值，后续无impact不可补写coreAfter。原局部指针数组域比较限1/2/5/8目标，C95仅容量边界。非side0的FULL9缺selector仍单列域限制；正常9来源十个模板全side0，玩家9本来走MAP。",
    0x2CE1A: "BYTE frame跨目标及九tick过渡延续：首target60tick为1到61、27impact；过渡到70，第二target核心与贡献照常但60tick无impact；后续BYTE回绕可再有impact。实际FDOTHER44仅31part，SDL只省beam越目录的DRAW，保全部计数/cue/impact与RNG，不伪造缺图或禁用多target。有效目录/其它FX/有效DRAW失败仍严格拒绝。",
}
for _va, _now in _G2_MAP_ACTION_EVIDENCE.items():
    CORRECTIONS.setdefault(_va, []).append({
        "target": "evidence", "was": "历史SHORT/恢复反馈未接线或FULL9多target整动作安全门",
        "now": _now,
        "why": "完整17函数Domain指令/CFG/xref与严格LE字节等同，读集SHAa3845664b68aa28bd0e7133242c2921bcfd431d994d04815c7fb0a835f0fdc86；FDOTHER/FIGANI及原分发表独立读取。verify_fd2_full9_wrapper.py执行原目标循环/完整过渡/真实RNG，与实际C FINISH80B及每次RNG前HP/MP/贡献比较；显存/声/等待仍明确stub，不冒称DOSBox运行或全帧声音等价。不改函数边界/类型。",
    })

# B6 C5真实地图演出核对：显示门由caller拥有，不把普通战斗返回页套到法术内部。
_B6_EFFECT_DISPLAY_EVIDENCE = {
    0x1CFF0: "成功目标确认后先令51A83=0并11cac(0)，随后进入FULL或map handler；死亡及loot处理后才恢复1。不能在恢复13的57..63或浮字底页强制画普通光标。",
    0x2FF01: "正常尾部恢复地图时调用11cac(1)，该参数只控制刷新等待，不把51A83强制改1。玩家1cff0及AI15311均以51A83=0进入，故FULL法术返回页保持无cursor；物理2e2b0的显示上下文不同。",
    0x15311: "聚焦施法中心后明确51A83=0，再分派2ff01或map effect；法术内部11cac返回页应沿caller的无cursor上下文，不能复用物理模式1返回页冒充。",
    0x1548E: "物理caller在攻击目标聚焦前设置51A83=1，2e2b0正常尾11cac(1)因而保留普通cursor。玩家18d8c亦沿18890的1进入，不能为修法术内部多余cursor而全局删除物理返回cursor。",
}
for _va, _now in _B6_EFFECT_DISPLAY_EVIDENCE.items():
    CORRECTIONS.setdefault(_va, []).append({
        "target": "evidence", "was": "战斗返回页统一强制普通cursor，未区分物理与法术caller显示上下文",
        "now": _now,
        "why": "完整1cff0/15311/1548e/18d8c/2ff01及122dc显示switch共同确认；真实C5治疗13中SDL目标源720px仅cursor开启模型匹配，原版关闭模型匹配。原资源全地图模型对9个纯实际帧各64000px/max3为0坏点，另1张原版57/58混帧不作完整帧通过；idle相位差另列，不以改色或mask掩盖。只读诊断收据SHA4567c02844a8e3b2b4f54572b58d3fff4dfce7f49b1256203f115be892781044；本条不新增函数名/ABI/类型，也不预称后续修复原版配对通过。"})


# B7：结算、下一关真实表入口与普通cursor状态，只登记已核raw和数据读集。
# 32e8c不是32d18的stack-check后主体；两个入口均有独立十字节序言和stage表fixup。
_B7_STAGE_NAMES = {
    0x22F37: ("sub_22F37", "field_stage1_victory_play",
        "完整443B与main结果表51de9[1]。raw5..10任一hidden播本stage片段7；全部可见则"
        "先完整片段6，再1c220(198)。PAN14,2、追加group4、delay100、script14、片段8；"
        "之后mode0/delay200/script15/片段9，再mode0/delay200/PAN14,1/delay200/script16/"
        "delay200/片段10。末尾112a5(8)先构造persistent，再11506按active同步，最后stage2。"
        "首片段6/7调用前继承caller的51A83；两片首控制码均为-19/raw6，不筛hidden，"
        "经165ac非零位置参数在165c4置0、165d6聚焦、165de置1。随后22fd4调用135dd，"
        "135f2无条件置0；group4与script14不写该字段，所以片段8入口为0，不能称继承caller。"
        "片段8内部可见说话人仍经165ac写0/1，返回后23021显式置0；片段9后23069再次置0，"
        "再PAN/script16至片段10入口0。mode写入不等于立即重画VGA；165de之后无11cac，"
        "16b43仅恢复缓存并移动marker，不写mode/重画战场，不能将逻辑1等同对话背景已有cursor。"),
    0x1C220: ("sub_1C220", "field_party_item_append",
        "完整73B，仅按active raw升序筛side2，不筛hidden/HP/acted；逐个调用1bb8c追加指定item，"
        "遇首个非负返回就停。全满不写、不弹替换/取消；调用者22f37忽略返回。"
        "奖励198在全部村民可见分支片段6完整返回后才发生，不是固定全队奖励或金额。"),
    0x32E8C: ("sub_32E8C", "field_stage2_entry_play",
        "stage入口表51d71[2]的fixup源51d79明确指向32e8c；独立push28/call3702f序言。"
        "主块294B，尾部共享3312d..33140与33140..3314b，不猜ABI。205da后PAN3,17/"
        "delay200/片段0；mode0/script18/group1/PAN3,6/delay200/script17/片段1；"
        "mode0/script19/片段2；mode0/PAN3,17/片段3一次，再全方向reset、focus actor0。"
        "FDFIELD[7]头2,6,40、group0空，六部署槽加group1九人得到15active。"
        "此初始化不等于stage2 turn3 action9或整关已实现/验收。"),
}
for _va, (_was, _name, _why) in _B7_STAGE_NAMES.items():
    CORRECTIONS.setdefault(_va, []).append({"target": "semantic_name", "was": _was,
        "now": _name, "why": _why +
        " 依据：B7隔离IDA Domain完整指令/xref、原LE fixup、FDFIELD/FDTXT与脚本14..19只读数据；"
        "静态取证不替代后续原版实际流程。未写函数猜测原型。"})

_B7_STAGE_EVIDENCE = {
    0x32D18: ("1281B区间全属于单一stage1入口",
        "旧32d18..33219误含六个有独立十字节序言和stage表fixup的入口。stage1主块止32e8c，"
        "保共享33206文字尾与33140聚焦尾；32e8c为独立stage2入口，其它32fb2/33049/3314b/"
        "33169仅修真实边界、保中性名。共同尾按实际jump连接，不造entry/core wrapper或改ABI。"),
    0x34778: ("stage1 turn3的action索引为5",
        "原FDFIELD[4]唯一回合记录是3,6,1，action表51b91[6]对应34778；此前说明中的5是"
        "登记笔误，当前业务case6正确。六村民behavior4/锚点26,15与已确认script13语义不变。"),
    0x26152: ("stage2可复用stage1的town底图及marker坐标",
        "stage2准备记录623ac首byte2；原52405三资源为11/61/62，故使用FDOTHER62。"
        "NoTown[2]=0，town曲10；stage2战场entry返回后main才请求primary19，alternate为1。"),
    0x265EC: ("town五项marker总使用type0坐标",
        "按准备记录type索引52363/52375的6B行；type2五项X为59/10/59/130/242、Y为"
        "26/144/163/150/31，拷入VGA内区后各加4。底部62x26 panel及495+selector沿旧owner。"),
    0x11506: ("新增unit8的persistent可以只用112a5默认值",
        "22f37先112a5(8)，再11506按非零text8从active raw27整80B覆盖该新persistent；"
        "清状态/flags、恢复HP/MP并重算。10c50在武器非FF的unit8模板下仅28..30九B未定义，"
        "cache2另属运行值，二者须按各端active来源继承，不把新allocator或SDL零当原版固定值。"),
    0x13A9F: ("AI纯移动和common tail总会恢复普通cursor1",
        "common tail无51A83写。behavior3找到text目标并移动/恢复后强写0；4/7在移动前0，"
        "5/10在tryattack失败后0；9移动保继承。14121/13e9c各自有0..1包裹，"
        "13fd4仅实际恢复才0..1，不能让14b78通用几何helper无条件改模式。"),
    0x14121: ("witness查询或同起点也写显示模式",
        "只有实际存在不同格witness的分支先mode0再focus/move，正常返回恢复1；无witness或"
        "同起点不写。正常零步移动也经过原恢复1，不据是否改坐标猜显示状态。"),
    0x13E9C: ("nearest目标缺失或同起点也改显示模式",
        "找到非自身坐标的最近敌目标后，mode0→focus actor→14b78→mode1；无目标或同起点"
        "保持原模式。该nearest扫描不新增hidden/HP/acted过滤。"),
    0x13FD4: ("AI恢复helper总会改显示模式",
        "HP已满或record25/26状态非零直接返回且不写模式；仅实际恢复分支0→focus/HP恢复→1。"),
    0x1CFF0: ("玩家Magic的演出末mode0会直接传至胜利回调",
        "演出/loot阶段先mode0，但普通成功尾1d4b1明确恢复1再返回。"
        "不得把Magic呈现FINISH页模式当成整个player caller完成后的持久普通模式。"),
    0x13565: ("自动换phase早胜会保留最后AI动作模式",
        "满足自动换轮条件后先mode0，调用1a30b返回后135ca无条件恢复1，即使AI阶段已提前胜败。"),
    0x16F55: ("所有END/March返回都统一恢复普通cursor1",
        "March分支17159恢复1；显式END分支调用turncycle后只恢复51AAC、不写51A83，"
        "因此按真实AI分支或turncycle返回模式继承。main胜利亦不另写，22f37首片段据此呈现。"),
}
for _va, (_was, _now) in _B7_STAGE_EVIDENCE.items():
    CORRECTIONS.setdefault(_va, []).append({"target": "evidence", "was": _was, "now": _now,
        "why": "B7隔离IDA Domain完整raw/调用链与原stage数据读集；32e8c与共享尾的边界修正"
               "另有独立table fixup和Watcom十字节序言证据。只修已证事实，既有9类型与3702f中性无型保护不变。"})

CORRECTIONS[0x13A9F].append({
    "target": "evidence",
    "was": "behavior11施法成功后可直接结束，或behavior5仅置完成位并保留behavior高四位",
    "now": "behavior11在13e1c调用15311后仍于13e26重新评估physical；priority>=6才13e39调用1548e，"
           "否则13e45调用14121，仅其未实际移动才13e52调用13fd4；不能因施法成功跳过后续。"
           "behavior5在tryattack失败后先于13c39调用12d7b聚焦caster，再检查completed和查找箱格，"
           "完成位已置或无箱格的fallback也保留该前置聚焦。实际到格后13d05写completed=1，"
           "13d1b调用12263即时将所有对应flags20地形WORD+1、清byte2并保留byte3，"
           "13d20再将actor+34整个byte写07。满库存不阻止完成位、开箱和behavior整byte写回；"
           "仍沿既有common tail执行最终格match1、acted和方向归零。",
    "why": "原13a9f完整1021B、13e09..13e52和13c34..13d24原指令及12263写集；"
           "major隔离回归保留AI11五项旧红/两no-op、AI5真实FDSHAP闭248/开249七项旧红，"
           "修后同一field_ai_commit新构建通过。此处只补已存在函数证据，不新增ABI/类型/边界，"
           "不将headless回归提升为原版运行对照。",
})



# Major：只更正标题自有BIOS循环；不引入未来A7八函数命名。
CORRECTIONS[0x1F894].append({'target': 'evidence', 'was': '再调用通用 six-entry selector `0x17d33`', 'now': '标题菜单自带 BIOS INT 16h/AH=10h 读取循环：scan 48h/50h 上下回绕，ASCII 0Dh/20h 或 scan E0h/52h 确认；其余键继续循环，ESC 没有取消或返回分支。完整 0x1f894..0x1ff79 不调用 six-entry selector；历史 code0 17d33 对应 canonical 27d33 的 shop selector，不能把其 ESC 取消推广至标题', 'why': '完整1765B反编译与原始指令/完整callee集合双证；自有BIOS菜单循环的全部key compare/branch均保留在只读readset。只更正证据comment，不改原name/type，不新增GUI或声称实机ESC验收。'})

# A7：完整AIL sequence原函数；八项已证语义，不写猜测ABI或16位driver地址。
_A7_SEQUENCE_NAMES = {275056: ('sub_43270', 'ail_mdi_sequences_timer_service', '完整1830B，原bytes==validated relocated。完整MDI timer遍历playing sequences；每callback +30计数后按tempo累计subtick，先32duration槽升序到期，再delay/EVNT；EOT保无限loop槽并跳本callback beat/volume/tempo fade；每8callback按缓存CC7刷新。43ad0把本函数注册到120Hz timer。全部15曲单次原kernel的事件和active-slot状态与私有执行器匹配；只声明当前资源默认100%tempo分支，不声称其余hook/宿主IO已恢复。 raw SHA c9ea914870d760db4bc8b1f86a55ac6c9c7d6d4d7126b227b63312fe589355a4。'), 281056: ('sub_449E0', 'ail_sequence_volume_fade_set', '完整128B，原bytes==validated relocated。先写sequence target；current相等不刷新；duration0立即置current，否则acc=0、step_us=(1000*duration)/abs(diff)，随后43230重发当前音量。25977普通曲用0/0再127/2000，16/17直接127，-1用0/4000。原kernel证到0保持status4，不补endpoint flush。 raw SHA c864f750f39071e654d4c1311e33f3039b5ad55b56b919cba93cf6ab29a573d0。'), 274992: ('sub_43230', 'ail_sequence_volume_controls_refresh', '完整61B，原bytes==validated relocated。升序扫描16个logical channel的raw CC7缓存+350h，只在!=−1时经42980重发controller7；不为未缓存channel生成默认。449e0和43270的8callback门调用，映射由sequence+90h决定。 raw SHA b6ed14e0ce03becafd7a7c4a57f6fab95f6ab574454bb2b9783bc36368274096。'), 272768: ('sub_42980', 'ail_sequence_midi_message_dispatch', '完整1094B，原bytes==validated relocated。完整sequence MIDI分派：program/control/bend先427c0缓存原值，controller7乘sequence+34h后signed除127并夹0..127，经logical-to-driver映射及channel owner门送422c0。还含AIL控制hook/锁/分支；本FDMUS已用CC仅1/7/64，不把未用分支算实现或运行验收。 raw SHA b3fad1dcbc2b713bde3eeb65fab8333c43b1bcb74b924b3a6d71ba9ccc63a4ce。'), 274784: ('sub_43160', 'ail_sequence_event_state_reset', '完整171B，原bytes==validated relocated。初始化16个channel恒等映射、1088B控制缓存为−1、loop stack与32duration channel槽为−1；重置pending count/delay/beat/+30callback计数。443d0和44790调用，不重装MDI driver默认值/RR/OPL。 raw SHA f0e658360d604620060397164bc27f427e796a57a7cb914ca39045d2789a97fc。'), 271536: ('sub_424B0', 'ail_xmidi_vlq_read', '完整45B，原bytes==validated relocated。完整VLQ读取：从传入游标逐byte累积7bit并前移，高位清零或达到4byte即返回；43270用于note duration及meta长度。实际全部FDMUS原字段已扫描；私有预检不接受未证更长编码或越界，不保存猜测参数类型。 raw SHA 4adfb87d832de5a180da5d5d3a8c2887d03abb6413d6b785ba1662cb37d010ad。'), 280528: ('sub_447D0', 'ail_sequence_playback_stop', '完整193B，原bytes==validated relocated。仅status4时改8，调用42dd0清本sequence pending槽，再按自身cache清pedal/锁/112。不是统一硬重置全部driver controller/voice，status2的重复stop不再发送释放；driver硬件release仍可继续。 raw SHA 676c29375b7ccbe454d2db4ac46bd067ac3f6917e4a930eac9fadbceb2d1ce6a。'), 273872: ('sub_42DD0', 'ail_sequence_pending_notes_release', '完整198B，原bytes==validated relocated。升序遍历32个非−1槽，通过42980发送各channel/note的noteoff，清槽和pending count并flush；再按3845e条件请求383f1(3)。重复note不折bitmap，32满时第33个noteon已发后由43270调stop释放旧32；宿主delay/OPL IO未作运行保真声明。 raw SHA 9449c603edf4c4787545ea7b242ad52a47debd06d2252ef32b8fa7a6ada9af8f。')}
for _va, (_was, _name, _why) in _A7_SEQUENCE_NAMES.items():
    CORRECTIONS.setdefault(_va, []).append({"target": "semantic_name", "was": _was, "now": _name, "why": _why})


# A7：原sample默认volume/pan缩放表；保留sub_40240历史名，不写ABI。
CORRECTIONS.setdefault(0x40240, []).append({'target': 'semantic_name', 'was': 'sub_40240', 'now': 'ail_sample_volume_pan_tables_rebuild', 'why': '完整626B/40240..404b2，初始化pan表5360c[63/64]均128。先夹sample volume/pan0..127，非零volume+1；按输入有/无符号和driver output kind构建256项mono或左右缩放表，使用原SAR7整型顺序。FD2 startup3fca9仅一次经37f70→3f937置preference5=100；39521→414e0读此值并设rate11025/pan64，两FD2 SFX wrappers25a96/25b45只end/init/address/loop/start，无volume/pan/rate覆盖；volume/pan setters的另caller为45880内部数字MIDI分支。原函数无patch/无stub执行：U8mono、volume100/pan64、output2/3的L/R各256项都精确(u8-128)*202，即名义sample gain101/128；127与0为独立控制。只读sample驱动ptr/format/volume/pan、driver+24 output-kind和原pan表，不读DIG master127。C10六WAV十二个既有onset/661源点窗显示SDL单位幅度与原版约0.785..0.789差异，低能80/11及残差保留；该幅度观察不作为公式或通过阈值。driver/DAC/混音/重采样、最终PCM保真仍未由此关闭。rawSHA 7af1f72ed139c68aa2b347be39c514dc1497f4f82f72a9adabd9dd0fb353b4e1。'})


def write_checksums(directory: Path) -> None:
    import hashlib

    sums = [f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}"
            for path in sorted(directory.iterdir())
            if path.name != "SHA256SUMS" and path.is_file()]
    (directory / "SHA256SUMS").write_text("\n".join(sums) + "\n",
                                        encoding="utf-8", newline="\n")


CORRECTIONS.setdefault(0x34F38, []).append({
    "target": "evidence", "was": "只有5B前缀，Hex-Rays无法反编译",
    "now": "event_callback[32]的入口34f38是push28h并自然落入34f3d..34f74共享尾；"
           "与350be的push28h/jmp34f3d共用stack_check、append_group(1)、text(fragment1)和ret。"
           "不能据旧5B主chunk判为no-op；补共享tail引用但保留350be的原owner，不扩大成吞并其它入口的函数。",
    "why": "G1 Domain逐指令/现chunkowner与strict LE/fixup表51c11→34f38双证；"
           "完整60B SHA256 cf7734a4bca09f70340a01c1b01bfc9ae3a80002b7f58c8f1aca7329dffc5ab0；"
           "原返回清栈与350be同尾证据见common-event-semantics.md及本地34f38-boundary.json。",
})

# G1：完整 Domain 指令／CFG／xref 与独立 LE 原字节交叉确认；不登记猜测 ABI。
# 取消与提交细节见 common-preparation-semantics.md、common-battle-semantics.md。
for _va, _old, _name, _contract in (
    (0x279BC, "sub_279BC", "field_shop_menu_execute",
     "2670e的武器/道具/隐藏店caller；四项依次购买、出售、装备、准备态转交，保留选择"),
    (0x2872B, "sub_2872B", "field_shop_buy_execute",
     "商品价格取23B record+19；确认/资金/收件/容量门后追加物品，可选装备，最后扣钱"),
    (0x28CBD, "sub_28CBD", "field_shop_sell_execute",
     "成员与原槽选择，价格floor(3*price/4)；YES后先加钱再移槽、重算送方"),
    (0x28F65, "sub_28F65", "field_prepare_item_give_execute",
     "商店/教会共同caller；无相邻门，满包拒绝不交换；先移槽再追加，仅重算送方"),
    (0x29DAA, "sub_29DAA", "field_church_menu_execute",
     "2670e selector4；四项成员详情、准备态转交、复活、转职，非额外治疗服务"),
    (0x2A43E, "sub_2A43E", "field_church_revive_execute",
     "hidden候选；费用word(52397+2*profile)*level；YES先扣钱再flags整byte清0、HP=max，MP保留"),
    (0x2AA00, "sub_2AA00", "field_church_promote_execute",
     "2ae0e选唯一目标，确认后删所需道具、演出、写profile/id、重建cache、2ac7d增量；无扣钱"),
    (0x2AF28, "sub_2AF28", "field_deployment_select_execute",
     "stage<27选15同伴否则19，raw0保留；选满先稳定重排，再必选成员门/前置重排/最终确认；最终NO不撤销排序"),
    (0x2B777, "sub_2B777", "persistent_roster_selection_reorder",
     "完整0xa00临时副本；raw0保留，选中raw1..稳定前置，其余稳定后置，不减人数、不改80B记录"),
    (0x2AE0E, "sub_2AE0E", "field_promotion_candidates_build",
     "level>=20且ID<18并!=7；目标ID+32，持指定物改+50，unit9持90优先52，候选顺raw"),
    (0x2AC7D, "sub_2AC7D", "field_promotion_growth_execute",
     "新ID成长pair第二byte，AP/DP/DX/maxHP/maxMP逐项提示ACK后写，移动BYTE增量；末重算/level1/EX0/HPMP满，不重学法术"),
    (0x4E7DD, "sub_4E7DD", "field_promotion_record_get",
     "返回615fe+2*(target_id-32)，至61646恰36条2B；profile与移动增量，非玩家基础表"),
):
    CORRECTIONS.setdefault(_va, []).append({
        "target": "semantic_name", "was": _old, "now": _name,
        "why": "G1 IDA9.5 Domain完整函数/CFG/xref及独立strict LE合法fixup逐字节相同；" +
               _contract + "。来源与有效域见docs/reverse-engineering/common-preparation-semantics.md及common-battle-semantics.md；静态取证不冒充原版运行。",
    })


CORRECTIONS.setdefault(0x1C2DA, []).append({
    "target": "evidence", "was": "caller颜色表值等于最终实体flash色",
    "now": "caller仍传51f15[effectID]，但4e127实际忽略第四个颜色参数；"
           "map stride456低BYTE被复制到AH并填全部RLE写点，所以此路径的有效色恒为200。"
           "30B原表13=192/17=146/18=72/19=216/22=35是未生效参数，不能用它替代最终色。",
    "why": "完整Domain1c2da caller及4e127原callee交叉复核；"
           "原callee读取arg8为EDX步幅，4e13b读取同一arg8为EAX，4e13e mov ah,al，"
           "后续mov al,ah而未读argC。原34连续WGC实体完整写mask的blue色与palette200一致，"
           "推翻只按caller推断的146/72/216。原51f15..51f33 SHA256 "
           "c09f95968698a256f582dab5bf428e1b495d54a8be6ce0df20406248477dcf0c保留为来源，"
           "不保留无真实消费者的新增冷表；有效200另作有证呈现常量。",
})
CORRECTIONS.setdefault(0x4E127, []).append({
    "target": "evidence", "was": "按caller第四个palette index填非透明像素",
    "now": "真实原指令同时以arg8作行步幅和填色低BYTE，不读取argC。"
           "repeat/alternate/literal的原写点均填stride&255，skip只推进；"
           "本地图caller步幅0x1c8，故有效色200，不是51f15参数或SDL自身surface步幅。",
    "why": "Domain完整127B手写解码器及1c2da/1da16全部caller；"
           "原伪C将a3同时用于rowstride与color的异常由真实指令确认，未改猜测ABI/签名。"
           "完整127B SHA256 a466fe4c6e5b855925dc413ea0adb227686799bfe0aa2c186a8d6237bd3dcb09；局部机器码及定向原件证据范围另列G2，非完整全帧/音频验收。",
})


# G3：准备领域完整Domain指令/CFG/xref与strict LE逐chunk相同；这里只登记
# 名称和证据，不写猜测ABI/Hex-Rays类型，不触发数据库或发布输出。
_G3_PREPARATION_NAMES = (
    (0x26A0D, "sub_26A0D", "field_shop_stock_copy",
     "完整109B，279bc唯一caller。selector1从31B准备行+3复制至多12项，3从+15/5从+23至多8项，ff停；不是新库存owner。"),
    (0x26A7A, "sub_26A7A", "field_prepare_money_add_present",
     "完整279B，28cbd caller；先格式化旧signed DWORD precision8，再26ab5模2^32加钱，随后逐列九切片滚动。47例局部原码/C门含完整正值循环和有界DWORD回绕写点；负号越目录像素不作等价。"),
    (0x26B91, "sub_26B91", "field_prepare_money_subtract_present",
     "完整266B，2872b/2a43e caller；26bcc先模2^32扣钱，再逐列递减滚动；共享26b8a返回尾。复活真实入口证明扣钱早于flags整byte清0和HP=max，不推UI等待RNG等价。"),
    (0x26C9B, "sub_26C9B", "field_prepare_money_strip_draw",
     "完整73B，26a7a/26b91两caller；从服务归档raw frame2读6字节步进的数字切片，再复制9行到指定stride目的页，不是通用数字格式化。"),
    (0x26CE4, "sub_26CE4", "field_prepare_menu_transition",
     "完整340B，商店/酒店/教会三caller；保存64000页，清201,169的104x20带，四图标按原四offset及四相位缩放位移进入/退出；不选择命令、不改roster。"),
    (0x26E38, "sub_26E38", "field_prepare_menu_select",
     "完整162B，279bc/29300/29daa；每轮26eda(0)，左右将53c57在0..3循环并cue0，Enter/Space确认、Esc返回；不是无RNG查询。"),
    (0x26EDA, "sub_26EDA", "field_prepare_picker_wait_key",
     "完整415B，菜单/商品/成员/购买候选/复活/转职picker六caller；入口一次共享RNG%50+8，highlight初2，按2 BIOS tick推进27079及立绘0/3，blink后RNG%30+2；读BIOS键并规范扩展确认/取消。"),
    (0x27079, "sub_27079", "field_prepare_picker_tick_draw",
     "完整599B，26eda唯一caller；highlight为奇数才推53f72并模4，按mode0菜单选中图、1列表箭头、2三行候选sprite、3完整成员sprite绘制。raw cache目录按原索引，phase3取1；不读键、不耗RNG。"),
    (0x272D0, "sub_272D0", "field_shop_items_draw",
     "完整790B，275e6/27738 caller；两列三行商品/库存，item+181名字、分类/属性图及bank42主值，服务frame15钱图及bank119五位售价；a5非零为floor3price/4。不是库存写入。"),
    (0x275E6, "sub_275E6", "field_shop_item_select",
     "完整338B，买/卖/转交caller；26eda(1)等待，两列六项窗口及上下两行滚动，边界不循环；确认/取消通过原53c57返回，不把压缩显示行重新映射到raw库存槽。"),
    (0x27738, "sub_27738", "field_shop_item_page_open",
     "完整222B，买/卖/转交caller；分配三个64000B页面，服务frame16底板与272d0内容，1974c的5..0六相位入场；实际导航由275e6，不是整购买handler。"),
    (0x27816, "sub_27816", "field_prepare_rows_shift_up",
     "完整209B，五类列表picker共享；284x80区域内容连续三次上移6像素/10ms，再上移8像素并清73，合一行26px；名字按像素方向，不混淆选项向下。"),
    (0x278E7, "sub_278E7", "field_prepare_rows_shift_down",
     "完整213B，与27816同五caller；反向拷贝保护重叠，内容三次下移6像素/10ms，再下移8像素并清73；不改first/选择索引。"),
    (0x27D33, "sub_27D33", "field_prepare_member_select",
     "完整535B，买/卖/装备/转交/酒店与教会详情caller；重置first/selection0，分配并滑入成员页，26eda(3)共享等待，两列raw roster导航，Enter确认/Esc取消。"),
    (0x27F4A, "sub_27F4A", "field_shop_equipment_recipient_select",
     "完整449B，2872b唯一caller；三行兼容候选及2825b四属性预览，26eda(2)，上下单行滚动且有效移动cue0；不装备、不扣钱。"),
    (0x2810B, "sub_2810B", "field_prepare_members_draw",
     "完整336B，27079/27d33；两列至多六名，cache目录raw_index*12+共享idle相位，4e29c将sprite skip填73，姓名按text+1并高亮；不按当前unitID另建cache。"),
    (0x2825B, "sub_2825B", "field_shop_equipment_recipients_draw",
     "完整943B，27f4a唯一caller；三行候选raw映射，28632出AP/DP/HIT/EV四DWORD预览，与现派生WORD比较选数字bank，服务frames18..22标签/箭头，不写live属性。"),
    (0x28632, "sub_28632", "field_shop_item_stats_preview",
     "完整249B及26e30共享返回尾，2825b唯一caller；signed基础AP/DP/DX加新物品效果，再只保留record.kind<=20/>20相反组旧flags40装备，依次输出四DWORD。不同于1b750状态缩放和ID128装备分组。"),
    (0x28B41, "sub_28B41", "field_prepare_service_animate",
     "完整380B，购买/出售/复活caller；selector1五帧、3单帧、4九帧与原渐白、5七帧，读取各服务归档23起帧，保固定BIOS等待/末清键；不在本函数扣钱或写actor。"),
    (0x28EFE, "sub_28EFE", "field_prepare_equipment_execute",
     "完整103B及28efc共享返回，279bc caller；循环27d33选持久成员→26996关页→1bffe装备，保存恢复portrait origin并重载当前服务头像；Esc退出。不把整个循环作为SDL native业务叶。"),
    (0x2A07A, "sub_2A07A", "field_revive_candidates_build",
     "完整72B，2a43e唯一caller；逐raw调用34894，仅flags bit0为1收索引，不加HP0/side/acted门。原码/C五组count0/1/7/16/32及hidden且HP非零输出一致。"),
    (0x2A0C2, "sub_2A0C2", "field_revive_candidates_draw",
     "完整475B及26e30共享返回，2a29d唯一caller；三行候选cache sprite、名字/种族/profile和费用，profile-1引用52399与52397+2*profile同地址；候选布局本身不清flags/HP。"),
    (0x2A29D, "sub_2A29D", "field_revive_candidate_select",
     "完整417B，2a43e唯一caller；三缓冲与六步滑入，26eda(2)等待，三行上下/滚动及确认/Esc，无有效移动cue0调用，不在picker恢复单位。"),
    (0x2A694, "sub_2A694", "field_promotion_candidates_draw",
     "完整451B，2a857唯一caller；三行旧身份sprite/text/profile、593及4e7dd目标profile+150，目标由已建候选数组传入；不构造新单位，不写身份。"),
    (0x2A857, "sub_2A857", "field_promotion_candidate_select",
     "完整425B，2aa00唯一caller；三行候选上下边界和滚动，26eda(2)共享RNG等待，确认/取消；与复活列表一样未播放移动cue0，不发明普通/特殊二选一。"),
    (0x2AEDB, "sub_2AEDB", "field_unit_item_slot_find_prefix",
     "完整77B，转职候选/耗物及若干stage根共享；先1b8a6数可见槽，再1b722直接查raw[0..count-1]同ID，返回首raw槽或-1。不能改成扫全8槽跳hidden；非仅转职服务。"),
    (0x2B439, "sub_2B439", "field_deployment_required_member_check",
     "完整194B，2af28 caller；扫描已排前缀raw1..15/19的text ID，缺失才头像75/动态名字/657与16c57(0)，返回有无；不自动补选、不回滚此前排序。"),
    (0x2B4FB, "sub_2B4FB", "field_deployment_selection_draw",
     "完整388B及2b434共享返回，2af28/2b67f；复制底页，画名额/剩余数、1297d共享idle、当前raw+1详情和10列同伴网格。已选普通sprite下移3px、未选灰阶；不改变选择位图。"),
    (0x2B67F, "sub_2B67F", "field_deployment_selection_wait_key",
     "完整202B，2af28唯一caller；输入未就绪时BIOS tick变化才2b4fb重绘并复制页，读取键后规范Enter/Space及扩展确认、Delete取消；本体不调用业务RNG。"),
    (0x2B749, "sub_2B749", "field_deployment_selection_count",
     "完整46B及26ed6共享返回，2af28/2b4fb；数count-1个同伴选择byte的非零项，不含raw0、不判断名额/必须成员；返回由caller比较15/19。"),
    (0x2B843, "sub_2B843", "persistent_roster_text_front",
     "完整339B，2af28 caller；扫描raw1..count-1取最后匹配text ID，以完整a00副本把它稳定移到raw1，其余相对顺序保留，随后按新UID序重建FDICON。47例原码/C门包含重复text，修正SDL先匹配即返回；原缺失匹配的未定义写域不开放。"),
    (0x2FB2C, "sub_2FB2C", "field_promotion_visual_execute",
     "完整744B，2aa00唯一caller；保存画面/fadeout/wait1，旧FIGANI idle的9帧入场/16tick循环，20步渐白，新idle首帧offset200/24tick，再fadeout还原/fadein。BG0..2为320x100，工作页stride640；不写单位、不扣物/钱、不调用业务RNG或音效。"),
    (0x2FE14, "sub_2FE14", "field_promotion_idle_cycle",
     "完整237B，2fb2c唯一caller；局部BG先加一mod3，每tick绘当前idle part，按原duration推进并在BYTE count回0，wait1；duration0不补1，age非BYTE回绕。正常16→48新idle为三字节empty，SDL仅略未定义sprite目录/时长读取，保BG/等待，非原未定义像素等价。"),
)
for _va, _old, _name, _contract in _G3_PREPARATION_NAMES:
    CORRECTIONS.setdefault(_va, []).append({
        "target": "semantic_name", "was": _old, "now": _name,
        "why": "G3只读IDA9.5 Domain完整函数/CFG/xref与strict LE原字节一致；" + _contract +
               " 完整33函数命名读集630d6b7dee0731278a4af07bd3595538c87f536eb72912586fd8f1161b7fbfda，"
               "来源/适用域见common-preparation-semantics.md；局部原码与原生运行范围分别登记，不写推测类型/ABI。",
    })


# G4：原调用指令与局部CPU栈捕获纠正Hex-Rays寄存器伪参，不写猜测类型。
CORRECTIONS.setdefault(0x1CAC7, []).append({
    "target": "evidence", "was": "伪C把第二次actor绘制误分成另外一组count/list及寄存器实参",
    "now": "保留中性sub_1CAC7。完整205B在LMI74与75两次调用1cb94时读取同一组原栈槽："
           "1cad5/1cad9与1cafc/1cb00均为ff74241c，依次传page/count/list/74或75。"
           "本函数不读取EBP/EDI的入值；2111a及213b7各显式push四个栈参数，不据伪C保存六参或寄存器ABI。",
    "why": "G1 Domain完整chunk/xref严格原镜像等同，函数SHA1210d0b96be5275f3c893984bfe3d908a20c309b62187c2d352f76b036654741；"
           "G4 effect-page-owner-source.json的count0/1/16/96四组原Unicorn调用捕获，两次callee同count/list，"
           "EBP/EDI poisoned入值保持。替身仅capture绘制/等待/资源，不证明图像或玩法native，不改名/类型/边界。",
})
CORRECTIONS.setdefault(0x1CB94, []).append({
    "target": "evidence", "was": "据调用者伪C寄存器变量推断第二次角色绘制使用其他队伍",
    "now": "保留map_scene_render_actors。自身prologue从栈加载count/list；1cac7以同一count/list依次传LMI74/75，"
           "不能把伪C的EBP/EDI变量当另一组队伍或另一个原owner。相同actor列表的两种绘制由真实page调用者安排。",
    "why": "完整387B Domain读集及strict LE SHA7bf45203e80d5f3002b3c2a562aca93bbd0f52eefbd04c194a09b40464f239c6；"
           "1cac7两处push/call和2111a/213b7原xref/四栈参一致；G4四组局部CPU捕获仅核调用与栈，"
           "绘制被capture，未证明完整像素或正式触发。不改原名/持久类型或函数边界。",
})


_G4_WATER_CALLER_EVIDENCE = {
    0x11CAC: "保留中性sub_11CAC。11cbc cmp原实参0，11cc1 JNZ跳过11cc3 call4e31c；"
              "故raw0推进water，非0跳过。参数不是普通显示51A83开关，也不是刷新等待；"
              "其余1297d、11eee、122dc、127a9、1acf3及VGA投影仍按原序。",
    0x135DD: "pan每个镜头格都调用11cac(0)，包括两方向移动；共享工作页和water随该原caller刷新，"
              "不把所有Scene render统一套skip-water，也不在纯delay虚构刷新。",
    0x1366A: "脚本相位的11cac实参按当前53AFB选择：0或64时传0，非0非64传1；"
              "渐暗可在同脚本内到64而切回0。高位wait及上抬恢复分支传0；1398a脚本尾固定传1。"
              "尾仍重画一次但跳water，不能把它当普通相位刷新或省掉。",
    0x205DA: "保留中性sub_205DA。独立entry完整边界205da..2067d，初始化后首次11cac明确传1，"
              "该刷新跳water。局部原CPU验证只执行到此次调用，constructor/memset为替身，"
              "不扩大为完整入场业务或像素验收。",
    0x2FF01: "纠正前批‘11cac参数只控制刷新等待’：FULL法术正常尾确实调用11cac(1)，"
              "原1跳water，原0才调用4e31c。该参数不写51A83；玩家1cff0与AI15311以mode0进入，"
              "因此正常返回仍沿caller无cursor环境。物理和其它caller须分别按源值接水色，"
              "不能通过强制mode1或每present更新water替代。",
    0x3231B: "new_game_opening_play既有cold j_delay只是原等待；不能为sleep_ms或每次呈现额外调用11cac。"
              "本条仅补既有caller的等待边界说明，不宣称新提取整函数或新增开场运行证据。",
}
for _va, _now in _G4_WATER_CALLER_EVIDENCE.items():
    CORRECTIONS.setdefault(_va, []).append({
        "target": "evidence", "was": "共享Scene刷新未区分原11cac水色参数或把它误称刷新等待",
        "now": _now,
        "why": "完整11cac/135dd/1366a既有Domain读集与strict relocated原字节逐chunk一致；"
               "205da按已发布入口边界核完整原字节；verify_fd2_scene_water.py执行原指令10例，"
               "核raw0/1、pan和脚本fade/wait/尾实参。绘制、DAC、脚步、等待、getter与constructor为明确替身；"
               "只证明原条件和调用参数，不证明图像、真实时钟、业务或DOSBox原生，也不改名/类型/边界。",
    })



_G4_FINAL_OWNER_EVIDENCE = {
    0x1548E: "完整1548e..1567e原指令中SHORT15616与FULL15624共同汇15627：1562f调用1b6b7收集descriptor，1563b以实参0调用11cac，15643调用1db65清死亡，15652调用1aa1d，1565c再次以0调用11cac，最后1566a调用1e292奖励。两次外围水色刷新适用于全部AI物理，不仅SHORT；它们不代替2e2b0 FULL演出内部的raw1尾。空descriptor count0同样经过1aa1d返回后的第二次刷新。",
    0x32999: "完整32999..32d18及FDOTHER9实际12part独立执行：153216B整页snapshot后每帧restore、LMI写53A49再投影；frame6投影后上抬新actor8px并FG存下一base，frame7投影后11eee并上抬5px存base，frame8投影后11eee/127a9存base；尾保frame11工作页。该caller不新增11cac/1297d/water/info，不能把12帧简化为6帧或把最后页回滚成到达前base。"
}
for _va, _now in _G4_FINAL_OWNER_EVIDENCE.items():
    CORRECTIONS.setdefault(_va, []).append({
        "target": "evidence", "was": "G4共享页／刷新caller顺序待逐条闭合", "now": _now,
        "why": "1548e复用G2完整Domain原读集，whole chunk SHA bb8f00be9623a99c0221c23831e79f659bcfddd5507ce6b27735b2f6543915d0；32999复用G1完整Domain原读集，whole chunk SHA 15a88f6d53a73f1644f046cdef06a09247cabd207f48638eebb93f180e2b69a3，永久verify_fd2_arrival_owner.py实际12次原caller投影。arrival的绘图/PCM/分配/拷贝/append均显式替身，只核调用顺序/页目标及参数；不升级为像素、业务、原版WGC或ABI等价，不改名称或猜测类型。"})


_G5_DIRECTION_ARRIVAL_EVIDENCE = {
    0x134E4: {
        "now": "完整134e4..13512的46B只遍历全raw表写record+3=0；13507 push20、13509调用3790a等待后返回。"
               "20个直接parent共23个静态callsite（不是每次运行23次）：13a9f正常尾在acted后reset20再11cac(0)；"
               "15055末20c6f后只有reset20、清53EC8返回，无额外11cac；16f55逐actor在acted前，18890仅非零非FF路径后。"
               "1548e SHORT两处reset20、FULL在裸2e2b0返回后一次，18d8c在collect前一次；裸2e2b0自身无reset。"
               "32d18两处，32e8c的一处位于共享尾3313b。六个已绑定legacy handler中34531/3460b/34673/346cd各一次，"
               "34738/34778没有reset；其余后期私有caller仅登记来源，不开放关卡。原语不写frame_phase、walk、输入或RNG。",
        "why": "verify_fd2_direction_callers.py独立核原EXE、5个既有Domain闭集读集SHA、完整parent chunks/xref/指令头及冷图次序；"
               "原46B raw-unrelocated SHA dc6f3ac1946072a996c259f094f6bad5da08d74223ad1b35fa150204a655578a，"
               "loader-relocated SHA 1b3991fcb227ba71e5c2de0525206f8051787eb9c98c625366245ec31051554a分列。"
               "静态callsite与分支证据不替代C adapter、完整CPU路径、真实等待或WGC；规范名/边界/类型不变。",
    },
    0x32999: {
        "now": "32999原到达效果每帧project后，frame6/7/8先完成各自postbase，随后统一到32ba2调用4e381清键队、"
               "32ba7 push1、32ba9调用17aa9等待，再递增帧号；正常组及空追加组均12次project/clear/wait。"
               "UI95/9加载、153216B快照先于10b4e追加；95/0/loop1 cue在frame1投影之前，尾不补11cac/1297d/水色/info。",
        "why": "verify_fd2_arrival_owner.py复用完整32999..32d18（895B）loader-relocated SHA "
               "15a88f6d53a73f1644f046cdef06a09247cabd207f48638eebb93f180e2b69a3与strict镜像；"
               "正常/空组两次原CPU caller控制流、实参和runtime.arrival 53叶独立比较。"
               "绘图、资源/PCM句柄、分配/拷贝、append和等待是显式替身，不声称业务、像素、声音或原生运行等价；不改名或猜ABI。",
    },
}
for _va, _facts in _G5_DIRECTION_ARRIVAL_EVIDENCE.items():
    CORRECTIONS.setdefault(_va, []).append({
        "target": "evidence", "was": "通用reset尾等待及arrival逐帧清队的完整caller范围待补核",
        "now": _facts["now"], "why": _facts["why"],
    })


_S2_RESULT_NAMES = {
    0x20707: ("leaders50_51", "先CALL205be普通主体，再raw50/51任一hidden写败北"),
    0x2073D: ("leader14", "先CALL205be普通主体，再raw14 hidden写败北"),
    0x20765: ("villagers15_26_turn59", "先CALL205be；raw15..26全hidden时先写败北再story10；其后signed turn>5且raw59 hidden再写败北、story2，两段可同次依序发生"),
    0x20822: ("leader64", "先CALL205be普通主体，再raw64 hidden写败北"),
    0x2084A: ("leader65", "先CALL205be普通主体，再raw65 hidden写败北"),
    0x20872: ("roster18_actor52", "先CALL205be，再33499(18)；roster text18缺席且raw52 hidden时先story2、返回后才写败北"),
    0x208CF: ("latched_actor52", "不调用普通主体、不初始化53ECC；先raw0/16/17任hidden写败北，再raw52 hidden写胜利覆盖；均不满足时保留入口锁存，没有胜利早返"),
    0x20926: ("turn6_actor64", "20930明确CALL205be（205b4内普通主体入口），再signed turn>6且raw64 hidden时写败北"),
    0x20957: ("allies53_60_enemies", "先CALL205be；raw53..60全hidden先写败北再story10；其后raw0/52任hidden写败北；末raw36..51和61..82均全hidden再写胜利覆盖，包括先前raw0败北"),
    0x20A51: ("leaders16_17", "先CALL205be普通主体，再raw16/17任一hidden写败北"),
    0x20A87: ("leader1", "先CALL205be普通主体，再raw1 hidden写败北；stage21/26/27三个原槽保留各自来源，不因同target提权"),
    0x20AAF: ("latched_actor18", "不调用普通主体、不初始化53ECC；先raw0/1/16/17任hidden写败北，再raw18 hidden写胜利覆盖；均不满足时保留入口锁存，没有胜利早返"),
    0x20B14: ("leader16", "先CALL205be普通主体，再raw16 hidden写败北"),
    0x20B3C: ("leaders1_2", "先CALL205be普通主体，再raw1/2任一hidden写败北"),
    0x20B72: ("latched_state18_20", "不初始化53ECC；state18/19/20三个BYTE全非0先写胜利；随后raw0 hidden写败北；随后raw1 hidden先story9、返回后写败北；无命中保留入口锁存"),
    0x20BF5: ("latched_actor20", "不初始化53ECC；raw20 hidden先写胜利；随后raw0 hidden写败北；随后raw1 hidden先story7、返回后写败北；无命中保留入口锁存"),
}
for _va, (_suffix, _meaning) in _S2_RESULT_NAMES.items():
    CORRECTIONS.setdefault(_va, []).append({
        "target": "semantic_name", "was": f"sub_{_va:X}",
        "now": f"field_battle_result_{_suffix}_check",
        "why": "S2完整30槽/18target及205b4/34894/33499读集，所有shared chunk逐字节核strict relocated；"
               + _meaning + "。205be是205b4内既有共享入口/主体，不拆新函数；hidden只取record+5 bit0，不查HP。"
               "原结果码0进行/1败北/2胜利，与SDL enum显式映射；不推断参数类型。"
               "verify_s2_results.py独立执行原指令832例，完整124块/158边及实际C等待前缀核验；"
               "仅Watcom栈探测和文本呈现为明确替身，34894/33499查询实际执行；不等同原生、全游戏或缺raw越界域。",
    })
CORRECTIONS.setdefault(0x33499, []).append({
    "target": "semantic_name", "was": "sub_33499", "now": "field_roster_has_text",
    "why": "S2完整64B、7个真实caller xref及20872实参18；扫描53BF7独立roster的53BFB条80B记录，"
           "record+8 BYTE零扩展后与完整栈DWORD比较，参数不截BYTE；不查HP/hidden，不是active身份查找；不保存猜测ABI。",
})
for _va, _meaning in {
    0x11DF2: "palette BYTE零扩展加有符号DWORD偏移，signed大于63才夹63，first..last含两端逐DAC端口写；逆区间为空、未改基色；本轮consumer仅接基色0..63/add0..255，不推广负数DAC或DWORD溢出硬件域；不是乘性brightness",
    0x17AA9: "signed WORD BIOS tick先减，负差加65536，与signed DWORD goal比较；<=0仍读一轮，53A2C scratch无外部reader；不清键/不推进水色",
    0x35B78: "pan→append group(BYTEarg8)→300ms→palette add255→200ms→add0→11cac(0)→400ms，无music",
    0x361B0: "palette add0..63每次8ms，400ms，再add62..0每次8ms；完整循环顺序不是fade-out brightness",
}.items():
    CORRECTIONS.setdefault(_va, []).append({
        "target": "evidence", "was": "S2新增palette/tick消费域未登记完整caller/WORD边界", "now": _meaning,
        "why": "s2_visual_source.py完整4函数494B含1951f共享4B尾及全部scratch xref；"
               "verify_s2_visual.py独立原码与真实VGA/Play叶346例，另记两个完整flash caller源轨迹。"
               "outp/时钟为明确替身；名义SDL/55不声明硬件相位/绝对时序。",
    })


_S2_FADE_EVIDENCE = {
    0x11D40: ("dac_apply_darkness", "完整11d40..11df2及1951f..19523共享返回尾；每个6bit基色分量减"
              "有符号DWORD darkness，负值夹0后写DAC，不改base palette。S2新source消费只接dark0..64；"
              "不能用旧宿主base*(64-dark)/64的乘法缩放作本原义oracle，旧已验brightness caller行为未在此重写"),
    0x1F882: ("palette_fade_out_dark", "主18B与1f503..1f525共享34B为同一完整函数域：先dark0，"
              "按0..63共64次11d40(0,255,dark)后各delay_ms(2)，包含dark63后的尾2ms；"
              "无清键、BIOS等待、RNG或Game热字段写。共享tail不是独立fade函数"),
    0x1F525: ("palette_fade_in_light", "完整51B从dark64递减至0，共65次11d40(0,255,dark)后各"
              "delay_ms(2)，包含dark0尾2ms；与fade-out的64次不同。1f82e位于1f81e内部，"
              "不能将其误登记成新fade-in入口；旧Scene17级/12ms近似不作为新source冷序列的依据"),
}
for _va, (_name, _meaning) in _S2_FADE_EVIDENCE.items():
    CORRECTIONS.setdefault(_va, []).append({
        "target": "evidence", "was": "S2完整fade/减法DAC尚未与历史宿主近似消费域分列",
        "now": _name + "：" + _meaning,
        "why": "S2 Domain完整六函数/CFG/xref及1951f显式4B尾逐chunk等strict relocated；"
               "本地fade-source-02/domain.json SHA256 b334003f6e657028582790ef62ab732f6982da8bd80b82cdeeb7d7363d0b47d3。"
               "verify_s2_fades.py执行完整11d40并和实际VGA C比85向量，含全部64基色×65暗度组合；"
               "原1f882/1f525各128/130步调用轨迹与共享冷子图stage.source.fade.out/in独立逐节点相等。"
               "栈检查、outp捕获和delay调用为明确替身；不称原生DAC/WGC或宿主绝对2ms已测，不改name/type。",
    })


# S2只记录source→唯一冷图的审阅关联；不复制节点/raw、不改名/类型/函数边界。
# 入口位于shared chunk或函数内部时登记到原owner；不创建mid-entry函数。
_S2_CALLBACK_SOURCE_BINDINGS = (
    (0x34818, '0x34818..0x34894', 'dispatch:event_callback:009 target=0x34818 (function_entry)→common.callback.009'),
    (0x348BB, '0x348bb..0x348ea', 'dispatch:event_callback:011 target=0x348bb (function_entry)→common.callback.011'),
    (0x34940, '0x34940..0x34984', 'dispatch:event_callback:014 target=0x34940 (function_entry)→common.callback.014'),
    (0x34984, '0x34984..0x349ec', 'dispatch:event_callback:015 target=0x34984 (function_entry)→common.callback.015'),
    (0x349EC, '0x349ec..0x34a1e', 'dispatch:event_callback:016 target=0x349ec (function_entry)→common.callback.016'),
    (0x34A1E, '0x34a1e..0x34a6c', 'dispatch:event_callback:017 target=0x34a1e (function_entry)→common.callback.017'),
    (0x34A6C, '0x34a6c..0x34b07', 'dispatch:event_callback:019 target=0x34a6c (function_entry)→common.callback.019'),
    (0x34B07, '0x34b07..0x34b2f', 'dispatch:event_callback:020 target=0x34b07 (function_entry)→common.callback.020'),
    (0x34B2F, '0x34b2f..0x34b6f', 'dispatch:event_callback:021 target=0x34b2f (function_entry)→common.callback.021'),
    (0x34B6F, '0x34b6f..0x34c7a,0x35f6e..0x35f78', 'dispatch:event_callback:022 target=0x34b6f (function_entry)→common.callback.022; dispatch:event_callback:023 target=0x34b9a (explicit_shared_chunk_entry)→common.callback.023; dispatch:event_callback:024 target=0x34c52 (explicit_shared_chunk_entry)→common.callback.024'),
    (0x34C7A, '0x34c7a..0x34cf1', 'dispatch:event_callback:025 target=0x34c7a (function_entry)→common.callback.025'),
    (0x34CF1, '0x34cf1..0x34d2f', 'dispatch:event_callback:026 target=0x34cf1 (function_entry)→common.callback.026'),
    (0x34D2F, '0x34d2f..0x34d64', 'dispatch:event_callback:027 target=0x34d2f (function_entry)→common.callback.027'),
    (0x34D64, '0x34d64..0x34d92', 'dispatch:event_callback:028 target=0x34d64 (function_entry)→common.callback.028'),
    (0x34D92, '0x34d92..0x34dd0', 'dispatch:event_callback:029 target=0x34d92 (function_entry)→common.callback.029'),
    (0x34DD0, '0x34dd0..0x34eb3', 'dispatch:event_callback:030 target=0x34dd0 (function_entry)→common.callback.030'),
    (0x34EB3, '0x34eb3..0x34f38', 'dispatch:event_callback:031 target=0x34eb3 (function_entry)→common.callback.031'),
    (0x34F38, '0x34f38..0x34f3d,0x34f3d..0x34f74', 'dispatch:event_callback:032 target=0x34f38 (function_entry)→common.callback.005'),
    (0x34F74, '0x34f74..0x34fc2', 'dispatch:event_callback:033 target=0x34f74 (function_entry)→common.callback.033'),
    (0x34FC2, '0x34fc2..0x34fcc', 'dispatch:event_callback:034 target=0x34fc2 (function_entry)→common.callback.024'),
    (0x34FCC, '0x34fcc..0x35009', 'dispatch:event_callback:035 target=0x34fcc (function_entry)→common.callback.035'),
    (0x35009, '0x35009..0x35022', 'dispatch:event_callback:036 target=0x35009 (function_entry)→common.callback.036'),
    (0x35022, '0x35022..0x350be,0x35f6e..0x35f78', 'dispatch:event_callback:037 target=0x35022 (function_entry)→common.callback.037'),
    (0x350BE, '0x350be..0x350c8,0x34f3d..0x34f74', 'dispatch:event_callback:005 target=0x350be (function_entry)→common.callback.005'),
    (0x350C8, '0x350c8..0x35123', 'dispatch:event_callback:007 target=0x350c8 (function_entry)→common.callback.007'),
    (0x35191, '0x35191..0x351e6', 'dispatch:event_callback:010 target=0x35191 (function_entry)→common.callback.010'),
    (0x351E6, '0x351e6..0x35258', 'dispatch:event_callback:013 target=0x351e6 (function_entry)→common.callback.013'),
    (0x35258, '0x35258..0x35298', 'dispatch:event_callback:018 target=0x35258 (function_entry)→common.callback.018'),
    (0x35298, '0x35298..0x352ca', 'dispatch:event_callback:038 target=0x35298 (function_entry)→common.callback.038'),
    (0x35321, '0x35321..0x35346', 'dispatch:event_callback:040 target=0x35321 (function_entry)→common.callback.040'),
    (0x353B5, '0x353b5..0x353e7', 'dispatch:event_callback:042 target=0x353b5 (function_entry)→common.callback.042'),
    (0x353E7, '0x353e7..0x353fa', 'dispatch:event_callback:043 target=0x353e7 (function_entry)→common.callback.043'),
    (0x353FA, '0x353fa..0x3540f', 'dispatch:event_callback:044 target=0x353fa (function_entry)→common.callback.044'),
    (0x3540F, '0x3540f..0x35422', 'dispatch:event_callback:045 target=0x3540f (function_entry)→common.callback.045'),
    (0x35422, '0x35422..0x35468', 'dispatch:event_callback:046 target=0x35422 (function_entry)→common.callback.046'),
    (0x35468, '0x35468..0x3551c', 'dispatch:event_callback:047 target=0x35468 (function_entry)→common.callback.047'),
    (0x3551C, '0x3551c..0x3553f', 'dispatch:event_callback:048 target=0x3551c (function_entry)→common.callback.048'),
    (0x3553F, '0x3553f..0x355b7,0x35d76..0x35d85', 'dispatch:event_callback:049 target=0x3553f (function_entry)→common.callback.049'),
    (0x355B7, '0x355b7..0x355f0', 'dispatch:event_callback:050 target=0x355b7 (function_entry)→common.callback.050'),
    (0x35638, '0x35638..0x35677', 'dispatch:event_callback:052 target=0x35638 (function_entry)→common.callback.052'),
    (0x356B3, '0x356b3..0x35722', 'dispatch:event_callback:054 target=0x356b3 (function_entry)→common.callback.054'),
    (0x357DD, '0x357dd..0x35833', 'dispatch:event_callback:056 target=0x357dd (function_entry)→common.callback.056'),
    (0x35833, '0x35833..0x35854', 'dispatch:event_callback:057 target=0x35833 (function_entry)→common.callback.057'),
    (0x35997, '0x35997..0x359cb', 'dispatch:event_callback:059 target=0x35997 (function_entry)→common.callback.059'),
    (0x359CB, '0x359cb..0x35a0d', 'dispatch:event_callback:060 target=0x359cb (function_entry)→common.callback.060'),
    (0x35BEE, '0x35bee..0x35c1d', 'dispatch:event_callback:062 target=0x35bee (function_entry)→common.callback.062'),
    (0x35C1D, '0x35c1d..0x35c40', 'dispatch:event_callback:063 target=0x35c1d (function_entry)→common.callback.063'),
    (0x35C40, '0x35c40..0x35cf1', 'dispatch:event_callback:064 target=0x35c40 (function_entry)→common.callback.064'),
    (0x35CF1, '0x35cf1..0x35d1e', 'dispatch:event_callback:065 target=0x35cf1 (function_entry)→common.callback.065'),
    (0x35D1E, '0x35d1e..0x35d76', 'dispatch:event_callback:066 target=0x35d1e (function_entry)→common.callback.066'),
    (0x35D85, '0x35d85..0x35d9e', 'dispatch:event_callback:067 target=0x35d85 (function_entry)→common.callback.067'),
    (0x35D9E, '0x35d9e..0x35e0e', 'dispatch:event_callback:068 target=0x35d9e (function_entry)→common.callback.068'),
    (0x35E5B, '0x35e5b..0x35ec1', 'dispatch:event_callback:070 target=0x35e5b (function_entry)→common.callback.070'),
    (0x35EC1, '0x35ec1..0x35f10', 'dispatch:event_callback:071 target=0x35ec1 (function_entry)→common.callback.071'),
    (0x35F48, '0x35f48..0x35f6e', 'dispatch:event_callback:072 target=0x35f48 (function_entry)→common.callback.072'),
    (0x35F79, '0x35f79..0x35f88', 'dispatch:event_callback:073 target=0x35f79 (function_entry)→common.callback.073'),
    (0x35F88, '0x35f88..0x35fcf', 'dispatch:event_callback:074 target=0x35f88 (function_entry)→common.callback.074'),
    (0x35FCF, '0x35fcf..0x360b6', 'dispatch:event_callback:075 target=0x35fcf (function_entry)→common.callback.075'),
    (0x360B6, '0x360b6..0x361b0', 'dispatch:event_callback:076 target=0x360b6 (function_entry)→common.callback.076'),
    (0x36214, '0x36214..0x36228', 'dispatch:event_callback:077 target=0x36214 (function_entry)→common.callback.077'),
    (0x36228, '0x36228..0x3623c', 'dispatch:event_callback:078 target=0x36228 (function_entry)→common.callback.078'),
    (0x362B0, '0x362b0..0x362c5', 'dispatch:event_callback:080 target=0x362b0 (function_entry)→common.callback.080'),
    (0x362C5, '0x362c5..0x362e8', 'dispatch:event_callback:081 target=0x362c5 (function_entry)→common.callback.081'),
    (0x363DE, '0x363de..0x36416', 'dispatch:event_callback:083 target=0x363de (function_entry)→common.callback.083'),
    (0x36416, '0x36416..0x3642e', 'dispatch:event_callback:084 target=0x36416 (function_entry)→common.callback.084'),
    (0x3642E, '0x3642e..0x36439', 'dispatch:event_callback:085 target=0x3642e (function_entry)→common.callback.085'),
    (0x36439, '0x36439..0x36440', 'dispatch:event_callback:086 target=0x36439 (function_entry)→common.callback.085'),
    (0x36440, '0x36440..0x36447', 'dispatch:event_callback:087 target=0x36440 (function_entry)→common.callback.085'),
    (0x36447, '0x36447..0x3644e', 'dispatch:event_callback:088 target=0x36447 (function_entry)→common.callback.085'),
    (0x3644E, '0x3644e..0x36455', 'dispatch:event_callback:089 target=0x3644e (function_entry)→common.callback.085'),
)
for _va, _chunks, _bindings in _S2_CALLBACK_SOURCE_BINDINGS:
    CORRECTIONS.setdefault(_va, []).append({
        "target": "evidence", "was": "S2批量callback完整来源与唯一冷图关联未登记",
        "now": "实际owner " + hex(_va) + " 完整chunks[" + _chunks + "]；" + _bindings +
               "。保留原中性名称；条件、顺序、等待和取消只维护于data/static/flows/common_event.json，不把候选视为新绑定。",
        "why": "convert_stage_callbacks.py的callback04完整103-owner Domain读集SHA256 "
               "299fd2d5f5a9b250c58db1b0a1bc31acc528880228c228e55c9bf926152f5306；strict relocated逐chunk等原码。"
               "72源caller的1944独立x86/实际Game begin-peek-reply C逐叶前缀、141 wrong-ACK/HOST/error及1容量拒绝。"
               "constructor新增区、stats、renderer/text/keyboard/delay/DAC为显式替身，既有helper写集单列；不签自然可达、"
               "全部原生/RNG等价。turn/lookup为BYTE、descriptor为完整WORD；每个新stage/caller/参数/resource绑定仍须S3。"
               "原callback82未闭合teleport ABI不在本映射；全部17旧图引用与未知去向由S2来源账另列，不向未决提权。",
    })

_S2_TRANSITION_SOURCE_BINDINGS = (
    (0x230F2, '0x230f2..0x23296', 'dispatch:stage_victory:002 target=0x230f2→stage.source.victory.002; dispatch:stage_victory:003 target=0x231bc→stage.source.victory.003; dispatch:stage_victory:004 target=0x231f9→stage.source.victory.004'),
    (0x23296, '0x23296..0x232e8', 'dispatch:stage_victory:005 target=0x23296→stage.source.victory.005'),
    (0x232E8, '0x232e8..0x233c6', 'dispatch:stage_victory:006 target=0x232e8→stage.source.victory.006'),
    (0x234BB, '0x234bb..0x235bc', 'dispatch:stage_victory:007 target=0x234bb→stage.source.victory.007'),
    (0x235BC, '0x235bc..0x235f9', 'dispatch:stage_victory:008 target=0x235bc→stage.source.victory.008'),
    (0x235F9, '0x235f9..0x23790', 'dispatch:stage_victory:009 target=0x235f9→stage.source.victory.009'),
    (0x23790, '0x23790..0x237d5', 'dispatch:stage_victory:010 target=0x23790→stage.source.victory.010'),
    (0x237D5, '0x237d5..0x239bd', 'dispatch:stage_victory:011 target=0x237d5→stage.source.victory.011; dispatch:stage_victory:012 target=0x2389f→stage.source.victory.012; dispatch:stage_victory:013 target=0x238dc→stage.source.victory.013'),
    (0x239BD, '0x239bd..0x23a0a', 'dispatch:stage_victory:014 target=0x239bd→stage.source.victory.014'),
    (0x23A0A, '0x23a0a..0x23b5f', 'dispatch:stage_victory:015 target=0x23a0a→stage.source.victory.015'),
    (0x23B5F, '0x23b5f..0x23cd5', 'dispatch:stage_victory:016 target=0x23b5f→stage.source.victory.016'),
    (0x23CD5, '0x23cd5..0x23e39', 'dispatch:stage_victory:017 target=0x23cd5→stage.source.victory.017'),
    (0x23E39, '0x23e39..0x23e74', 'dispatch:stage_victory:018 target=0x23e39→stage.source.victory.018'),
    (0x23E74, '0x23e74..0x240fa', 'dispatch:stage_victory:019 target=0x23e74→stage.source.victory.019'),
    (0x24DF2, '0x24df2..0x24e80', 'dispatch:stage_victory:024 target=0x24df2→stage.source.victory.024'),
    (0x24E80, '0x24e80..0x25052', 'dispatch:stage_victory:025 target=0x24e80→stage.source.victory.025'),
    (0x25464, '0x25464..0x2548c', 'dispatch:stage_victory:027 target=0x25464→stage.source.victory.027'),
    (0x32D18, '0x32d18..0x32e8c,0x33140..0x3314b,0x33206..0x33219', 'dispatch:stage_entry:001 target=0x32d18→stage.source.entry.001'),
    (0x32E8C, '0x32e8c..0x32fb2,0x3312d..0x33140,0x33140..0x3314b', 'dispatch:stage_entry:002 target=0x32e8c→stage.source.entry.002'),
    (0x32FB2, '0x32fb2..0x33049,0x3312d..0x33140', 'dispatch:stage_entry:003 target=0x32fb2→stage.source.entry.003'),
    (0x33049, '0x33049..0x3312d,0x3312d..0x33140,0x33140..0x3314b', 'dispatch:stage_entry:004 target=0x33049→stage.source.entry.004'),
    (0x3314B, '0x3314b..0x33169,0x3344d..0x3346b', 'dispatch:stage_entry:005 target=0x3314b→stage.source.entry.005'),
    (0x33169, '0x33169..0x33206,0x33140..0x3314b,0x33206..0x33219', 'dispatch:stage_entry:006 target=0x33169→stage.source.entry.006'),
    (0x33219, '0x33219..0x3327d', 'dispatch:stage_entry:007 target=0x33219→stage.source.entry.007'),
    (0x3327D, '0x3327d..0x3332b', 'dispatch:stage_entry:008 target=0x3327d→stage.source.entry.008'),
    (0x3332B, '0x3332b..0x33367,0x33206..0x33219,0x3344d..0x3346b', 'dispatch:stage_entry:009 target=0x3332b→stage.source.entry.009'),
    (0x33367, '0x33367..0x333f5', 'dispatch:stage_entry:010 target=0x33367→stage.source.entry.010'),
    (0x333F5, '0x333f5..0x3344d', 'dispatch:stage_entry:011 target=0x333f5→stage.source.entry.011'),
    (0x3346B, '0x3346b..0x3347c,0x3344d..0x3346b', 'dispatch:stage_entry:012 target=0x3346b→stage.source.entry.012'),
    (0x3347C, '0x3347c..0x33499,0x3344d..0x3346b', 'dispatch:stage_entry:013 target=0x3347c→stage.source.entry.013'),
    (0x334D9, '0x334d9..0x335a0', 'dispatch:stage_entry:014 target=0x334d9→stage.source.entry.014'),
    (0x335A0, '0x335a0..0x335aa', 'dispatch:stage_entry:015 target=0x335a0→stage.source.entry.015 (source-only alias→stage.source.entry.012)'),
    (0x335AA, '0x335aa..0x335da,0x3344d..0x3346b', 'dispatch:stage_entry:016 target=0x335aa→stage.source.entry.016'),
    (0x335DA, '0x335da..0x33674', 'dispatch:stage_entry:017 target=0x335da→stage.source.entry.017'),
    (0x33674, '0x33674..0x3367e', 'dispatch:stage_entry:018 target=0x33674→stage.source.entry.018 (source-only alias→stage.source.entry.012); dispatch:stage_entry:019 target=0x33674→stage.source.entry.019 (source-only alias→stage.source.entry.012); dispatch:stage_entry:020 target=0x33674→stage.source.entry.020 (source-only alias→stage.source.entry.012)'),
    (0x3367E, '0x3367e..0x336a0', 'dispatch:stage_entry:021 target=0x3367e→stage.source.entry.021'),
    (0x338C4, '0x338c4..0x3396a', 'dispatch:stage_entry:023 target=0x338c4→stage.source.entry.023'),
    (0x33AAE, '0x33aae..0x33af1', 'dispatch:stage_entry:025 target=0x33aae→stage.source.entry.025'),
    (0x33DBA, '0x33dba..0x33e3c', 'dispatch:stage_entry:028 target=0x33dba→stage.source.entry.028'),
)
for _va, _chunks, _bindings in _S2_TRANSITION_SOURCE_BINDINGS:
    CORRECTIONS.setdefault(_va, []).append({
        "target": "evidence", "was": "S2入场/胜利完整caller来源与source冷图关联未登记",
        "now": "实际owner " + hex(_va) + " 完整chunks[" + _chunks + "]及该表入口可达切片；" + _bindings +
               "。原ID、branch及确切共享尾保持；流程仅在data/static/flows/stage.json编辑，不改中性名/类型/边界。",
        "why": "convert_stage_transitions.py的完整80-owner Domain/CFG/xref读集SHA256 "
               "ac3849db2e2892117d56d37145eb9f75687781053a2982c3fd356b599fe602d6；每chunk/instruction等strict relocated。"
               "candidate15的45表入口独立原caller CPU161向量和14完整predicate向量；叶、实时等待、绘制/构造为显式替身。"
               "原233c6完整写集与13536/shared tail独立核，fade原码/实际VGA C85向量及共享冷子图序列另证；"
               "实际Scene逐叶C/probe门由独立最终收据登记，本注释不提前签仍未跑的C或原生结果。"
               "45caller复用41唯一图（4个全图同义source-only alias）及stage.source.initialize/fade.in/fade.out；共用现Scene深度/预算，无新hot stack，所有新caller资源绑定留S3，"
               "12未决目标不因附近CALL或已知部分写集开放。",
    })

CORRECTIONS.setdefault(0x24618, []).append({
    "target": "evidence",
    "was": "field_stage_transition_effect：播放9帧区域转场，等待500ms后渐暗",
    "now": "完整24618尾在原9帧/释放临时页后delay_ms(500)，随后j=0..62、step2共32次"
           "pal_partial_set(0,255,j) @0x11df2，各有尾delay_ms(4)。11df2对基色逐分量加j并夹63，"
           "所以是加色变白，不是11d40减色或1f882淡出；保留原入口名、边界和业务owner。",
    "why": "S4完整六caller Domain读集build/s4/fade-caller-source01/domain.json SHA256 "
           "b94010fe3b42eef5a9ed2c4b50aeaaee6c21c03a8a5303133d3b694e97a87043，"
           "含24618..24754全316B/完整CFG、xref及callee；六函数3885B逐chunk与strict relocated镜像一致。"
           "本条只澄清完整原控制流和调用参数；不把已有SDL比例近似当原义，"
           "也不提前签尚未执行的C/原码oracle或原生图像/绝对4ms时序。",
})

CORRECTIONS[0x35346].append({
    "target": "semantic_name",
    "was": "field_event_story3_item213_story11",
    "now": "field_event_story3_item213_story4",
    "why": "S3复核完整owner35346..353b5（111B，strict relocated SHA256 "
           "033ae1e66dabfd9187eadac8ab3e72de78a27e6ddc353929391e3add158cd65f）及真实共享尾入口："
           "353ae push4后跳3530d，执行3530d/35313/35318/3531b/3531e/3531f/35320，"
           "绕过352ca路径的3530b push11。因此callback41真实顺序为story3→1aa1d(count1，item213)→story4，"
           "不能按共享tail所在owner名推成story11。"
           "build/s3/callback-binding-source02-red/comparison.json（SHA256 "
           "e18663a532e725584231b8638eb996c590e21c39f26fee576a75b7d4298b17b5）"
           "独立执行5个原caller各36、合计180个x86向量，全部41轨迹均到story4；"
           "该red收据actualC=false，C失败属于另一个caller39的child owner，不据此称C或原生通过。"
           "旧错误命名及当时依据保留历史，本条只改正35346名称/注释，不改边界、指令、原型或共享尾owner。",
})

CORRECTIONS[0x1B41D].extend([{
    "target": "semantic_name", "was": "sub_1B41D",
    "now": "field_battle_overview_draw",
    "why": "S3完整1b41d..1b5f1（468B）CFG和真实19df7→1b1e7 caller："
           "本函数只绘制四底板、关卡/回合/资源/三阵营计数与两段条件，不拥有输入或modal生命周期。"
           "完整Domain读集build/s3/ida-overview-source02/domain.json SHA256 "
           "d0a1f4f023ae9b02f88723573302913fb2a48269570363906723bf1744ee7352，"
           "含1b41d/1b5f1/33499/34894共669B，每个chunk及连续指令与strict relocated一致。"
           "verify_s3_overview.py独立执行完整四函数242向量，绘制/文本/栈检查为显式替身；"
           "只确认语义名，不保存猜测原型，也不据此签真实字体、原版WGC或尚未运行的实际C门。",
}, {
    "target": "evidence",
    "was": "旧总览切片只开放stage0/1；stage16私有败条件未绑定，普通条件598+2*stage不足以描述该分支",
    "now": "胜条件始终597+2*stage。stage16先画胜条件629，再调用field_roster_has_text @0x33499(18)："
           "持久roster存在text18时败条件630，缺席时628；active中的text18不参与，roster的HP/hidden/side均不滤。"
           "SDL本轮以field_info冷表投影原常量/条件，借现Game持久roster；普通stage不要求这个owner。"
           "旧未绑记载保留当时事实，不能当作当前剩余阻塞。",
    "why": "完整1b41d CFG及33499 fullDWORD参数18、独立53bf7/53bfb roster读取；"
           "原码242向量覆盖stage0..29、roster0/1/32、首尾text18、HP0/hidden/side噪声，"
           "并检查原active/roster无写。资料缺失/容量越界是SDL安全拒绝域，不送原码越界执行；"
           "真实资源渲染、实际C、原版运行范围分别登记，不把纯query或源CPU当作整场景通过。",
}])


# S5只给完整原入口登记来源/消费边界；冷图仍以data/static为唯一可编辑输入。
# 本清单不是第二套剧情表，也不把读集中全部owner都当作语义已确认。
_S5_SOURCE_EVIDENCE = (
    (0x336A0, 'dispatch:stage_entry:022→stage.source.entry.022',
     '初始化后raw0..15置hidden；24618相对focus(+6,+5,10,8)；仅HP WORD非0者显现/方向2；追加group1及故事0..4，末方向复位20ms/focus0。'),
    (0x3396A, 'dispatch:stage_entry:024→stage.source.entry.024',
     '借FDOTHER88、sample1/loop1四次；清App整153216B并按20/20/20/60次24b4d帧编排；1d512 push53B13再跳1a80a共享free切片，不能套owner上游53B0F条件。'),
    (0x33AF1, 'dispatch:stage_entry:026→stage.source.entry.026',
     '保item100分支、script76/81、三次不同focus-relative24618及共享3312d故事7入口；条件、原序和reset/20ms均在冷定义。'),
    (0x33C9D, 'dispatch:stage_entry:027→stage.source.entry.027',
     'raw0..19先hidden、HP WORD非0者恢复；三次script85、group6/7闪现和显式方向/20ms；不以HP_MAX代HP。'),
    (0x33E3C, 'dispatch:stage_entry:029→stage.source.entry.029',
     'script87、七次33f78/22253确切actor/destination/focus序与361b0脉冲；相同传送冷序只存唯一stage.source.teleport。'),
    (0x240FA, 'dispatch:stage_victory:020→stage.source.victory.020',
     '完整233c6写集后计首16 raw中209..214物品出现数；六件匹配才消费/授100并走24336；不匹配故事6。两臂后持久追加raw24/23、11506并stage21。'),
    (0x244B6, 'dispatch:stage_victory:021→stage.source.victory.021',
     '布局、故事4..6/scripts65/66后24618相对(0,+3,10,8)；保白页、原淡出、11506和共享stage22尾。'),
    (0x24754, 'dispatch:stage_victory:022→stage.source.victory.022',
     'item100与持久roster text18两条件；248b5读取signed DWORD53BEF turn并与15比较，绝非53BFB roster_count。stage先增23后仍用原FDTXT23故事14..17；circle/shake与map69/shape23替换顺序保持。'),
    (0x24C1E, 'dispatch:stage_victory:023→stage.source.victory.023',
     'scroll speed2..9各30次refresh(1)/tick1，10..14各12次darkness/refresh(0)/tick1；复用已有312×192背景owner和tick门，不把300 refresh当300实际scroll。'),
    (0x250CC, 'dispatch:stage_victory:026→stage.source.victory.026',
     '首16 raw的item100保两臂；正常stage27，缺失则raw1传送255/255、恢复roster、31529后25462原不返循环。1c2da实COUNT1/list52326=[1]只闪raw1，arg19不是raw范围/调色板。ending pair自然caller的HP栈cell末写/有效域待复核，仍显式阻塞，不提前称整来源可运行。'),
    (0x2548C, 'dispatch:stage_victory:028→stage.source.victory.028',
     '35f10/1db65只有败亡反馈，无loot/reward collector；raw20只写ID/text126、不伪造cache重建。三次361b0各自返回后都有delay300（25687/25699/256ab push12c）；其后故事15/淡出/11506/stage29。'),
    (0x25757, 'dispatch:stage_victory:029→stage.source.victory.029',
     'stage++到30后先1088d(30)再FDTXT31故事0/1、pan/script89；31c49内再构造30一次。31529返回后25975为原不返尾；30仅内部终章资源，不是普通battle/preparation capability。'),
    (0x362E8, 'dispatch:event_callback:082→common.callback.082',
     'STATE16=0..4完整caller：故事s+2、reverse24-s、追加group s及25+2s/26+2s传送；s4改raw20 AI低域11。末display1，不递增STATE16；共享35f6e由完整34b6f/35022 owner承担，不能称unowned或新函数。'),
)
_S5_SOURCE_WHY = (
    'S5源基底保持S2完整80-owner读集SHAac3849db2e2892117d56d37145eb9f75687781053a2982c3fd356b599fe602d6；'
    '新增transition domain05 SHA8d8d8a71301bce4e2ce57b70b8fe031532d4ef9b194b41950a4794d8deb2be0f、'
    'victory domain04 SHAa86a98ed7ca90031edef0eb80cce2af1028fb0cf56a809ece3d620178670bdac、'
    'teleport完整44-owner domain05 SHA683ec0e987b4fff9d515677df1fc0d9d1d77d9e95ab11f816135fd2bc06cdbb2，'
    '每chunk/instruction/xref/CFG保实际owner且逐字节等strict relocated；完整域非附近CALL推断。'
    '来源/候选为F:/fd2sdl-evidence/s5-20261114/transition-candidate08/conversion.json '
    'SHA37fa5dbdb5a5142a2169eebd68f97be3a1752656c862e21fb9010aea7d70d3c9。'
    '13原分派ID与真实目标保留；有分支/等待/取消/写点的编排在唯一冷定义，C仅有限领域叶。'
    '完整来源/冷图不是运行支持许可；当前绑定、C及原生范围另按development/实际收据，不由此静态注释签核。'
)
for _va, _source, _meaning in _S5_SOURCE_EVIDENCE:
    CORRECTIONS.setdefault(_va, []).append({'target':'evidence','was':'S2该原入口保持隔离或部分语义未闭合',
        'now':_source+'。'+_meaning+' 保持原中性名、完整边界/共享chunk和持久类型。', 'why':_S5_SOURCE_WHY})

_S5_HELPER_EVIDENCE = {
    0x24618:'原五参数读取focus-relative cells，九帧LUT9..1按radius+i*step增长；后delay500及32次11df2加色/尾4ms，不是减色。八个实际callsite四参数各保来源，单叶不隐藏循环。',
    0x22253:'五参actor/dstX/dstY/drawX/drawY；114..124逐sprite/tick、sample由两组xy相等选择；icon124；两次tick后先写raw.xy再copybase/mask，不先present；dy==0擦18行否则24行，各10ms，expand0..9各tick。借原App/page/Game owner，无SAV新状态。',
    0x22470:'sprite原frame114..124共11帧，每帧BIOS tick1；不把终帧125误加进LMI资源域。',
    0x22547:'icon124后contract LUT5..0，每步10ms；shared tail返回原LUT0/mult0。原row与focus-relative中心读取保留，不靠draw坐标反算。',
    0x22656:'固定expand LUT0..9每帧tick1；完整owner/共享尾单列，不生成通用脚本VM。',
    0x24336:'FDOTHER34完整101part；frame0叠snapshot，1..68写真实App raw前64000后到VGA，water/tick3/clear键；ANI(0,15,0)及palette时序后69..100仍tick3/clear但无water。资源字段只引用原archive。',
    0x2189A:'一次terrain快照后十次circle12/127a9投影；半径、LUT次序及无额外等待按完整caller冷展开。',
    0x24B4D:'terrain首13×9与11cac(0)准备后交替两行/20ms，循环次数由原caller冷定义；不在renderer重采输入或追加16ms。',
    0x1C2DA:'250cc四push为(0,19,COUNT1,&52326byte1)；真实第三参是count、第四参是BYTE列表，不是raw0..19。只闪raw1，solid200及原有限定时表沿G4；原未用参数不猜新义。',
    0x31BDF:'普通通知origin84,119/background74/pitch19；clear/open，phase5..0 slide，clear/text/portrait0/wait0，phase1..5 slide，close/clear；1974c/16f04共享尾无暗加tick或16ms。',
    0x31529:'终章固定序列含FDOTHER54/60/58/57/59、通知、40宽页帧/200帧/credits及20组pair。31c49前WORK原free、BASE最后读取后仅现代owned清理不冒原free ABI。2ebe1未自行初始化的HP栈cell影响3%重复，但完整自然caller链的末写/有效域尚待复核；当前首pair显式fail，不能签整终章等价。',
    0x31C49:'1088d参数30不写热Game.stage；FDTXT31 fragment44滚500帧、raw j=0/1互换、intro8..0无等待、20idle帧、byte2个part按part+6等tick、220/440卡帧。输入改原j=1而不break内帧，末卡可重播；RNG借现Game。class/movement_profile是raw+20h而非level+21h；textID+12先BYTE wrap再查片段。',
    0x26152:'NoTown先于任何(stage-1)*31城镇记录读取。原523e7完整30B中22..24及27..29非0，26为0；27..29不能越界延长只有26行的城镇表。普通prepare域1..29与内部credits资源30分离。',
    0x2EBE1:'S5 callee受控240向量仅证marker分支读取未自行初始化的局部HP栈cell并影响RNG相关3%重复。完整31529→2e2b0及prior2e9a8→memmove链的实际末写/有效域尚待复核；未自初始化不等于自然caller的HP任意，不提前断言无法恢复或作安全差异取舍。当前pair保持阻塞，不改ABI类型。',
}
for _va, _meaning in _S5_HELPER_EVIDENCE.items():
    CORRECTIONS.setdefault(_va, []).append({'target':'evidence','was':'S5新caller/资源/失败前缀适用边界尚未归档',
        'now':_meaning,'why':_S5_SOURCE_WHY+' 仅复核上述已证有限合同，不把读集中其他owner一并提升语义确认。'})

CORRECTIONS.setdefault(0x355F0, []).extend([{
    'target':'semantic_name','was':'field_event_item101_story11','now':'sub_355F0',
    'why':'撤销错误语义名并保中性入口。完整355f0..35638（72B）在35631原6a03 push3、35633跳3530d，'
          '不执行352ca自己的push11；callback51真实loot_child[2,-1]后story3。旧G common.loot.child.2仍保原已测story11组合，'
          'S5新common.callback.051与stage021精确binding修正旧S2误归属，不改签旧报告。'}, {
    'target':'evidence','was':'S2将callback51视为common.loot.child.2已有引用，未核共享尾实际push',
    'now':'355f0原入口/352ca真实共享尾与52748的3B descriptor保留；side0/1/2、库存0/7/8、满包Cancel均先1aa1d后story3。'
          '原72B main与shared tail不能据owner旧名猜成story11；不改函数边界、chunk、签名或指令。',
    'why':'F:/fd2sdl-evidence/s5-20261114/callback51-old-mapping-red01/comparison.json原CPU36、旧C13实际story11红件保留；'
          'callback51-source-c15-01/comparison.json独立原CPU36与真实C39（3错ACK/HOST/error）通过，'
          'probe SHA6a2ec667649b7baac8300b511a9467956918ba0ae1f78af15cb7065af00b8867。'
          '该门为真实Game/Loot有限consumer，渲染/键盘及原child呈现为明确替身，不证明原生、全RNG/时钟或完整游戏。'}])


# publication02当时的未决边界保留；本块只追加后来完整原caller与有限C证据。
# 这些记录不是第二份ending定义，不把sourceavailable改称整终章runtime通过。
_S5_MONTAGE_CLOSURE_WHY = (
    '完整自然栈读集F:/fd2sdl-evidence/s5-20261114/victory-stack02/domain07.json '
    'SHA4cd565bc6e1c245ab5e6bc1d7d2ee45901c47b987bcf92490d4378c184d02a46，20个完整owner/8880B；'
    'natural-stack-cpu03.json SHAeb63210f61b62676e040f7973d11cc84a7fa0c1c0b4e935945575b1df70f81e2 '
    '分列60原intro、40同CPU连接first/counter-query/second、80派生WORD body。'
    '完整render/memmove/stackprobe/query/25a96实际执行；40连接例设备和SFX选项开、两bank故意未映射，'
    'AIL若到达即拒；RNG0/50、BIOSdelay和硬件OUT仍显式替身。'
    'natural-stack-contract02.json SHA817967cb5bd900c57d37bb4450b140ebd384614b61c8991afad8583aa45ae079 '
    '登记53A45热owner、credits之后时点、WORD写及音频门。'
    'montage-actualC18-01/comparison.json SHA688b62ed183e0c6c207f29eff3903efe663ae4e3916194756cfe535797cf5c50 '
    '核20pair×RNG0/50共40例、3596原PC操作、2779完整64000B索引帧及HP/RNG/branch；'
    '实际有限C叶由原PC事件驱动，80次RNG/2419次BIOS调用/199次原SFX门另计。'
    '该门不签完整31529/credits/冷walker/自然App/GUI或硬件时钟音色；'
    'source10的13源完整候选和315来源可用与实际C、正式绑定、原生门分别登记。'
    'publication02、旧240个poison callee向量及37040探索误入口拒绝均保原身份；'
    '正确stackprobe入口为37042，不改3702f中性无原型。'
)
_S5_MONTAGE_CLOSURE_EVIDENCE = {
    0x250CC: 'publication02中的ending自然栈待复核已由完整caller闭合；item100好臂仍11506→stage27→25089，'
        '坏臂25089→31529后保持原25462不返。坏臂热stage26不被credits资源30构造强写为30；'
        'montage在31c49返回后才发生，不能用旧stage26两名战场actor或独立render假actor代替53A45。'
        '当前来源候选可用不是本注释给坏结局、任意SAV或整终章原生批准。',
    0x25757: 'stage++30后原1088d(30)、FDTXT31故事0/1和script89顺序保持；'
        '31529内部31c49再构造resource30，故后来pair写的是该时点的53A45热active。'
        '原25975不返尾不变，30仍仅终章资源/终端域，不能据sourceavailable新增普通battlecap30。',
    0x31529: '撤销publication02首pair仍fail的当前推断，仅保留其历史。20组原526e9/526fd/52711参数在31c49返回后，'
        '逐组写53A45 raw0/1的side(+6)/unitID(+7)，不改cache(+2)、textID(+8)或roster/SAV。'
        '63680的marker HP写也属于这些真实active；不是malloc视觉页的两个临时actor。'
        '每组两次RNG：首侧3%最多两轮且idle/抖动不重置，第二侧仍取RNG但最后marker整part后返回一轮。'
        'overlay58时53A65切到FDOTHER57，组间与末尾恢复保存的base palette；不凭临时palette改永久owner。'
        '分支、重复、HP写、逐帧和等待放唯一冷图，C仅有限资源/单帧叶。',
    0x31C49: '资源constructor参数30不写热stage；返回31529之前已重建53A45 active和相关资源，'
        '随后montage沿这一热owner写raw0/1。26坏结局热stage26与29终章热stage30均可到此，但不开放battle30。'
        '既有500滚页、j0/1互换、220/440卡帧、BYTE(textID+12)和raw+20h职业文本合同保持，'
        '不把31529之后的actor覆盖倒推成此前credits已使用这些改写身份。',
    0x2E2B0: '仅31529这20组非split场景：borrow原53A45 raw0/1，54133取原scene override；'
        '首侧完整2ebe1及真实counter-query仍执行，override使普通反击/伤害分派路径受限；随后override=1执行第二侧。'
        '两方FIGANI和BG/TAI从真实资源读取，冷图只用有界22part/10tick族；'
        '不将此montage合同推广到1548e/18d8c等普通物理caller或旧B6未验域，不修改持久ABI类型。',
    0x2E9A8: '31529非split实际资源下完整原intro尾经11eb0→3771c。令P=2e2b0入口ESP-64，'
        '最后memmove入口P-108，push ESI写P-112；下一2ebe1入口P-36，其var4C正是(P-36)-0x4c=P-112。'
        'ESI是VGA末行0xa0000+199*320=0xaf8c0；多种旧栈poison/heap布局均被这个实际末写覆盖，'
        '不能将callee未自初始化误报自然HP任意，也不能给别的caller假定相同栈深。',
    0x2EBE1: '本函数未自行初始化的局部HP读不等于31529自然值未知：原2ef80按signed DWORD读719040，'
        'override将damage清0，2ef9e向真实target+40h只写低WORD63680，每个marker即使duration0也写。'
        '首侧正DWORD保3%第二轮；第二侧override1在最后marker完整part之后早返，仍消耗其一次RNG。'
        'typed HP/条件/轮次/等待在冷图，marker为非0不等于固定图像帧数；'
        '旧240poison向量仍证明受控callee依赖，不改签为自然域、全物理战斗或完整C/GUI。',
    0x25A96: 'S5 montage调用仍遵守完整device&&sfx_option&&54133==0门。首侧override>1、第二侧1，'
        '在读bank或AIL stop/sample之前返回；因此不能依据clip的bank_selector加载额外音库。'
        '原CPU将设备/选项置开且两bank设未映射仍完整通过，AIL到达硬拒；该结论仅此override域，'
        '不改变其他正常caller的sample/loop行为或用户混音器静音状态。',
    0x3771C: '保FLIRT memmove原名/类型。仅在上述完整31529自然栈链，入口push ESI的保存槽成为下一2ebe1局部HP cell末写；'
        '值0xaf8c0由上层VGA最后scanline给出。这是已证caller栈别名，不把通用memmove重命名成HP初始化函数。',
}
for _va, _meaning in _S5_MONTAGE_CLOSURE_EVIDENCE.items():
    CORRECTIONS.setdefault(_va, []).append({'target':'evidence',
        'was':'publication02阶段自然栈/热owner及完整C帧范围尚待闭合；旧记录保历史',
        'now':_meaning,'why':_S5_MONTAGE_CLOSURE_WHY})


# F2逐读取点合同：只追加来源说明，不将C回归或代表运行提升为全时序资格。
_F2_CLOCK_READ_CONTRACTS = {
    0x1297D: "12987先读signed差>4，失败才1299a另读负差；成功推进idle后129c5重新读钟存53c0f，cursor每调用推进。不复用一次宿主时间覆盖这些读取点。",
    0x11EEE: "11eff比较terrain tick，xor后11f16重读锚；随后实际wave/scroll背景更新及其fresh锚，1210f/12122的range门和1214b fresh锚，12164才选LUT并绘tile。固定51a93值直接写range phase而不取range clock。",
    0x127A9: "127c2先筛hidden，非hidden按raw递增在127cf逐个调用127e0，全部actor后才127d9调用129ec前景；空表及全hidden不调用127e0。",
    0x127E0: "127f1/127f6在camera/raw26之前读BIOS低WORD；不等则12801翻共享抖动phase，12808/1280d重新取锚。相机外及raw26=0也经过该门，direct本体不含batch的hidden筛选。",
    0x4E31C: "低WORD unsigned差>=2才单步推进水色，完成16色DAC写后4e373再次4e310取锚，不复用比较值或追赶。",
    0x11AA8: "先peek，空队4e31c，低WORD与App539f2不同才11cac(0)，返回后重新读WORD写539f2；早键不刷新，锚不随每次读键/Load重置。",
    0x12DAC: "115b6的ESI候选索引传入本次局部等待；先peek，空队water，signed低WORD不同才11cac(0)，返回后fresh读ESI。读一返回结束局部生命，不能借539f2跨调用保存。",
    0x17898: "每个空FIFO循环water→共享四tick高亮→1297d→11eee→127a9→1acf3→179d5→project；无BIOS-change外门、122dc或RNG。高亮phase写后重新读signed低WORD存53c17。",
    0x19953: "19b1e比较高亮门，19b49在phase写后重读signed低WORD存53c17；重开眼先画portrait0再19be0抽RNG%30+10。不把实际portrait绘制后移到RNG之后。",
    0x26EDA: "入口只置App53f52=2而不清53f72；do tick门后才peek/read，成功tick后fresh写局部锚。普通画像只在close/open事件绘制，不每轮覆盖portrait0。",
    0x27079: "每mode均按共享53f52奇偶推进53f72，仅++恰等4归0；名单/成员/复活/转职借53f72，菜单/箭头借53f52。城镇26152也写53f52，不能用三份局部phase代替两App DWORD。",
    0x2B4FB: "数字/selection count后无条件1297d，再详情/网格。实际App phase写发生于后续绘图之前，不能以成功绘制后提交的视觉副本替代原前缀。",
    0x2B67F: "先查键；空队比较App53f46，变化时重新读钟写锚再2b4fb及copy。后续宿主绘图失败不回滚已经执行的53f46写；此H规则不冒称原版坏资源oracle。",
    0x18B84: "18bab peek；空则water一次，完整ESI与signed低WORD不等才第二次water→1297d→11eee→122dc→127a9→summary→info→project，最后18c65 fresh存ESI。summary侧由入口屏内focus X锁定。ESI来自18890所选actor地址，首次比较的全正常启动地址域仍未闭合，不用0/入口tick/宿主C指针代替。",
    0x1DB65: "1dcd4/1dd81分别调用完整11eee，仍为terrain→实际背景→range→tile的独立读取点；1dd1b逐raw直接127e0，1dd95经127a9批量调用。不能将死亡演出的terrain/background/range或actor成功重读锚冻结在一次now采样。预检advance0只借页不推进clock；第31锚别名及caller保存值资格另账，不由这些读取点代签。",
}
for _va, _contract in _F2_CLOCK_READ_CONTRACTS.items():
    CORRECTIONS.setdefault(_va, []).append({
        "target": "evidence", "was": "共享时钟的一次采样或局部相位投影尚未按全部原读点扩签",
        "now": _contract,
        "why": "F2既有完整Domain/strict读集source09（SHA256 18617904b37d73b066eea7a78889d6da64fc6dad895b9292ae279df4b8f47517）；"
               "11aa8/12dac/18b84/18890另取input-wait-source01的完整函数，source01 SHA256 6f112a51399bdae47e82d208393a27ca9b4f78c451e71aeb21bc254ba6a248c1。"
               "17898/26eda/27079/2b4fb/2b67f的来源资格沿既有完整G3 Domain及canonical登记，不冒称它们全在source09。"
               "4e31c沿已发布水色函数与strict原字节读集，不列作source09包含项。"
               "新增live组件、实际工作页和正式代表分别登记F2报告；不是新原始程序运行、全共享RNG或绝对CPU/PIC时序证明。不改函数名、边界、类型或ABI。",
    })


# F2正常输入和菜单开合：完整原指令核序，ABI不从伪C参数表推定。
_F2_MENU_WAIT_CONTRACTS = {
    0x1F894: "1fe37绘制菜单后1fe48..1fe60以INT16/AH10阻塞读一，方向或无映射键返回1fe2c后再绘；确认直接进入原四轮normal/highlight各80ms。没有批量取完FIFO后的50ms。宿主空等pump不属于原显式等待。",
    0x1741C: "1749d..174e7仅入口一次1297d→11eee→127a9→info→175a9；四相位各17643恢复72x72、四图标、可见focus的direct127e0、project。无water、122dc、每相位全图重画或显式16ms。focus raw由入口lookup锁定；五个正常菜单caller共用两张四项表，不由伪C多实参猜ABI。",
    0x175A9: "原53a71跨open/wait/close保存同一菜单底图：先free旧buffer再malloc5184，每行18个DWORD、共72行，从当前focus附近的456-stride工作页复制72x72。不是按每个开合函数重新生成的底图。",
    0x17643: "按当前focus对应地址，将53a71的72行连续72B恢复到456-stride工作页；只写页，不刷新terrain/actor或投影。原调用者在图标和direct127e0之前调用它。",
    0x176B4: "入口仅SFX及可见focus lookup，没有1297d/11eee/127a9/info；四相位各恢复原72x72、画四图标、direct127e0、project。177ed再恢复一次工作页而不project，177f2才4e381清FIFO。HOST/H失败保已执行前缀，不伪造成功尾clear。",
}
_F2_MENU_HELPER_NAMES = {
    0x175A9: ("sub_175A9", "field_command_menu_background_save"),
    0x17643: ("sub_17643", "field_command_menu_background_restore"),
}
for _va, (_old, _new) in _F2_MENU_HELPER_NAMES.items():
    CORRECTIONS.setdefault(_va, []).append({
        "target": "semantic_name", "was": _old, "now": _new,
        "why": "完整154B/113B及1741c、176b4两caller和53a71读写证明72x72菜单底图保存/恢复；不按邻近或函数大小猜名。",
    })
for _va, _contract in _F2_MENU_WAIT_CONTRACTS.items():
    CORRECTIONS.setdefault(_va, []).append({
        "target": "evidence", "was": "正常标题读键与四向菜单开合尚未按全部原读点扩签",
        "now": _contract,
        "why": "F2 menu-wait-source01/source01.json SHA256 53f1e84db5e2973f2774cdaadc148ac4083ec70a024b012ea2cc404207571952，"
               "10完整函数含四叶和五caller，各chunk连续指令/strict字节匹配，源DB/镜像SHA及mtime不变，无类型保存或原版运行。"
               "C回归、正式窗口及全帧/绝对CPU时序分别登记，不由本静态合同代签。",
    })


# F2有限UI提交不含等待；稳态clock门与额外caller尾另行登记。
_F2_UI_TRANSITION_CONTRACTS = {
    0x17E0B: "详情打开以18409提交相位11..0，共12次，17ee8尾清键；有限相位无16ms或BIOS等待。",
    0x18409: "详情单相位裁切/拷贝后提交，不自行等clock；开关相位及额外snapshot恢复由caller分别决定。",
    0x17AED: "整页关闭0..11后另恢复完整64000B snapshot；法术下页0..6滑出、独立空底条提交、6..0滑入。两处任意键16c57与有限相位分账。",
    0x1956B: "底框按Y177/164/151/138/125/112提交六次，没有各相位16ms。入口和逐字等待仍由原caller执行。",
    0x196CB: "底框按Y125/138/151/164/177提交五次，随后整页snapshot恢复，释放三个buffer，最后11cac(0)。不是六次关闭，也不把释放后移至尾刷新之后。",
    0x1974C: "底框单相位只裁切/提交，不设置16ms或BIOS等待；26996的准备页尾与196cb的战场尾不同。",
    0x197E5: "选择框单相位不等待；19953打开4/8/12/16、关闭12/8/4/0，各四次。正常高亮与画像的2tick稳态门不属于有限开合。",
    0x1B1E7: "总览打开0..11、关闭11..0均无显式等待；稳态1b2fb/1b2fe将signed BIOS低WORD与继承EBP比较，相等跳过actor/water/tile/units块。该函数不写EBP，不可猜成每轮fresh锚；正常入口EBP资格另记。",
    0x2000A: "辅助图打开1..7、关闭6..1无有限相位等待；开尾清队、退出读一、尾11cac(0)。稳态先键/clock门再marker，首次比较字节源于前一int386的FS/GS，非2000a初始化clock；正常selector返回合同另记，不用常量替代。",
}
for _va, _contract in _F2_UI_TRANSITION_CONTRACTS.items():
    CORRECTIONS.setdefault(_va, []).append({
        "target": "evidence", "was": "有限UI相位、稳态与caller尾合同尚未分账",
        "now": _contract,
        "why": "F2 ui-transition-source01/source01.json SHA256 9aaac5b34cb503ceacb54a1f5bf97e45b2352c2b237db9f925847381a1fc4902，"
               "14完整函数；1b1e7另见source02/source01.json SHA256 a13af3e357f12eacfbcaabcfbfcdc085580e9a2ce225746fe88a7d05d13978b4。"
               "2000a沿input-wait-source01及overview-initial-time-review01的完整来源。逐chunk连续指令/strict bytes匹配，源DB与镜像身份守卫不变。"
               "这是静态功能复现合同，不代签初值全域、WGC可见帧、绝对CPU时刻或整阶段原生等价；不改类型或ABI。",
    })


_F2_MAP_CAST_PREAMBLE_WHY = (
    "F2 battle-wait-source01/source01.json SHA256 fbba710a24749ed5ca31081fbfde76e4c9381faf6dd9181f7e383bc6418f8584，"
    "完整1d6c8的212B、连续原指令/strict bytes及1cff0唯一caller相交；源DB/镜像SHA与mtime守卫不变。"
    "完整1cff0确认23的两次selector均在共用1d3f3/1d459之前，2218a的1ca89是MP helper而非selector。"
    "不从伪C参数猜ABI，不以C回归代签原版运行或绝对时序。"
)
CORRECTIONS.setdefault(0x1D6C8, []).extend([
    {"target": "semantic_name", "was": "sub_1D6C8",
     "now": "field_map_magic_cast_flash", "why": _F2_MAP_CAST_PREAMBLE_WHY},
    {"target": "evidence", "was": "玩家MAP费用之前的公共声画前缀尚未独立登记",
     "now": "入口先sfx_play(53b13,0,1)，四轮DAC0写51aad/51ad1/51af5[id]→wait1→DAC0黑→wait1。"
            "1cff0仅id9..27且非24在双selector成功、mode0/11cac(0)/bank准备之后调用，随后才分派51d01 handler及其MP成本；"
            "AI/Item/FULL不是此唯一caller域。23取消第二目的地不进入该前缀，不能提前闪烁。",
     "why": _F2_MAP_CAST_PREAMBLE_WHY},
])


_F2_RAY_CONTRACTS = {
    0x149F8: ("sub_149F8", "field_line_targets_collect",
              "七实参的arg_C/10为扫描起点，arg_0/4只决定方向；先将focus写起点，每轮先步进再lookup并按raw顺序收录，最后恢复旧focus。"
              "15055及1567e传当前actor坐标为起点、selected为方向点；1cff0同源。不能反向从selected朝actor扫描。"
              "原SP=S的[esp+2c/30]在本体固定栈深1c下对应[S+10/14]，不是按伪C参数名猜测。"),
    0x2DFC8: ("sub_2DFC8", "field_ai_ray_item_cast_present",
              "唯一直接caller15055的高射程Item路径；EDI为149f8返回的目标数，1511c写入并经face/11cac/17aa9/2e9a8正常保存。"
              "本函数不初始化EDI，从该part起点至part_count逐项演出并等待，不能从0重播；起点不小于part_count时自然零part，仍执行尾wait6。"
              "临时资源释放及地图cache恢复在尾wait6之前；随后fadeout/清VGA/11cac(1)/停止源声/fadein。"
              "15055的2189a、200ms、渐白及远端focus/phase循环在caller，20c6f效果只执行一次，不能替换成另一套FULL业务。"),
}
for _va, (_old, _new, _contract) in _F2_RAY_CONTRACTS.items():
    _why = ("F2完整Domain/strict原源：149f8的286B及三个真实caller沿source09 SHA256 "
            "18617904b37d73b066eea7a78889d6da64fc6dad895b9292ae279df4b8f47517；"
            "2dfc8的744B沿battle-wait-source01/source01.json SHA256 fbba710a24749ed5ca31081fbfde76e4c9381faf6dd9181f7e383bc6418f8584。"
            "完整caller实push、SP变化及EDI保存叶交叉核对，不用伪C数组/原型自证。源DB/EXE/镜像身份守卫不变；"
            "C范围、像素、正常可达性和原生时序分别登记，不由静态合同合签。")
    CORRECTIONS.setdefault(_va, []).extend([
        {"target": "semantic_name", "was": _old, "now": _new, "why": _why},
        {"target": "evidence", "was": "射线起点/调用ABI及演出返回合同尚未完整登记",
         "now": _contract, "why": _why},
    ])


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
        "oracle": ida_provenance(),
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
object1 有效范围为 `VA 0x10000..0x4ef29`。本次函数入口、边界与大小来源：{ida_provenance()}。历史复核过程见
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
