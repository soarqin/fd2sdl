"""F1：把实际 DAT caller、已核来源流程和追加次数连到容量／raw 域。

只读审计，不执行／修改产品，也不把冷图生成成功当原版语义证明。原函数
来源由 source09 的完整 Domain 读集及 strict 镜像固定；流程语义继承 S5。
这里新增的是正式绑定与实时排程／一次性状态的人工核查 join。
"""
from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/static_data'))
from audit_stage_data import archive_entries, load_definitions, parse_metadata, SOURCE_HASHES
from verify_movement_scripts import source_scripts
from verify_f1_boundaries import verify_source

SOURCE_SHA = '18617904b37d73b066eea7a78889d6da64fc6dad895b9292ae279df4b8f47517'
ROSTER_SHA = 'ee8a0953e4c57a8f920b1924f8d0c55344678624979c3aaf617c7ef6fc30c31d'
REVIEWED_FLOWS_SHA = '94db0080f642eae96a9d9fdec7911f923d45d301ed37dbb7a2bef37251b57691'
REVIEWED_BINDINGS_SHA = 'e622889243944d762ad21bbfe8b61576feefc120197125971fa7f84cbbf77ec0'
CHANGERS = {'append_group', 'append_group_source', 'roster_append', 'map_replace'}
SOURCE_ONLY = {'common.map.replace', 'common.group.turn.source'}
DYNAMIC = {27, 31, 47, 49, 52, 54, 57, 74, 82}
# 原 source09 完整 owner/共享尾人工审过的实际状态／writer合同。不是开放任意
# callback 的运行许可。普通固定 turn 每个槽在同一自然回合只检查一次；active
# Save/Load保留 metadata、event32、active/turn，不重放入场或先前 phase。
RULES = {
  0: 'turn3/phase1；先于其后的turn4/5/6。未引用cell action12不增加caller',
  1: 'turn4/phase0一次；turn3事件正常前缀先完成',
  2: 'turn5/phase0一次；turn4事件正常前缀先完成',
  3: 'turn6/phase1一次；turn5事件正常前缀先完成',
  6: 'turn3/phase1一次',
  5: '各绑定各自唯一固定turn槽；action32与5共用完整共享尾，不混为一个caller',
  7: 'turn9/phase0一次；turn4/phase1 action5先完成',
  9: 'turn3/phase2一次，raw6可见才append；取append分支给安全上界',
  11: 'turn4/phase1一次', 15: 'turn4/phase1一次', 16: 'turn7/phase0一次',
  22: 'turn15/phase2一次，raw8可见才append；取append分支给安全上界',
  23: 'group0 template0掉落一次；turn<15才appendg2；hidden死者不再收集',
  25: 'turn10/phase0一次；state16==1才appendg2，末state17=1',
  27: '六个固定槽turn2..7/phase0；DWORD turn直接作group，各组至多一次',
  30: 'group0 template0掉落一次；slot0=T+1、slot1=T+2(均BYTE)，state16=2',
  31: '仅上述两个槽；自然turn<=254由30一次排程，state16=2→3→4；各appendg2/g3至多一次',
  35: 'turn1/phase0一次；appendg2之后移动42',
  37: 'group1 template1掉落一次；各appendg3/g4一次；提前掉落的raw读资格另列，不据此否定容量上界',
  38: 'turn7/phase0一次', 40: 'turn4/phase1一次', 42: 'turn8/phase0一次',
  46: 'turn6/phase1一次；appendg1与roster27同次',
  47: 'turn2/4/6/8 phase0，signed DWORD turn/2为group1..4',
  49: 'turn3/7 phase0，signed DWORD turn/2为group1/3',
  50: 'turn5/phase2一次；appendg2与roster20同次',
  52: 'turn13/15/18/22；AL BYTE表达式2*(turn-14)及+1；254/255、2/3、8/9、16/17互异',
  54: 'turn2/4/7/10 phase0，DWORD turn为group；各一次',
  56: 'turn6/phase1一次',
  57: 'turn2/4/6/8/10/12/15/16/17 phase0，DWORD turn为group；各一次',
  61: 'lookup2 match1，state12==0且库存208；先consume并置state12=1再append，永久一次',
  63: 'lookup0/action62仅state17==0写slot0=T+1并置1；一个槽最多排程一次',
  64: 'group0模板8/9/10三次掉落；state16初0且每次+1，只有旧值1分支appendg3/g4/g5一次；旧值2写HP尾并finalize终端',
  66: 'lookup0/action65仅state16==0写slot0=now并置1；一个槽最多排程一次',
  68: 'group1模板8掉落action67一次写slot1=now；appendg2之后state18=1',
  70: 'lookup1/action69仅caller side!=0、state17==0、state18!=0写slot2=now并置17=1；须先68完成，g3/g4/g5各一次',
  72: 'group8模板17掉落一次，各appendg2/g3一次',
  74: 'lookup1/action75仅state17==0且caller text9成功置17=1、16=4并排slot0；74只由state16!=7自排T+1再+1，故4/5/6/7各一次',
  76: '75一次置state17=1并排slot1=T+1；76非4时+1并自排，下次4仅appendg1一次且不再自排；state21=count-3指向刚构造的三条',
  82: '四个group0 boss掉落81各至多一次；state16初0严格递增至4，slot0每杀覆盖T+1；正常<=254只可能按1/2/3追加其组各至多一次，合并可跳组；state4无追加',
}


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def digest_json(value):
    return sha(json.dumps(value, sort_keys=True, separators=(',', ':')).encode())


def verify_reviewed_scope(changing, aliases):
    bindings = {key: target for key, target in aliases.items()
                if target in changing and key.startswith('stage.')}
    if (len(changing) != 42 or SOURCE_ONLY - changing.keys() or
            digest_json(changing) != REVIEWED_FLOWS_SHA or
            digest_json(bindings) != REVIEWED_BINDINGS_SHA):
        raise ValueError('reviewed dynamic flow/binding scope changed; re-review required')


def callback_id(flow):
    if flow == 'common.key.consume':
        return 61
    match = re.fullmatch(r'common\.(?:callback|legacy\.action)\.(\d+)', flow)
    if not match:
        raise ValueError('unknown dynamic flow identity: ' + flow)
    return int(match[1])


def script_actors(raw):
    if not raw:
        raise ValueError('empty movement source')
    at = 1
    actors = []
    for _ in range(raw[0]):
        if at + 2 > len(raw):
            raise ValueError('truncated movement group')
        mode, count = raw[at:at + 2]
        at += 2
        if count > 32 or at + count * 2 > len(raw):
            raise ValueError('truncated/oversized movement pairs')
        actors.extend(raw[at:at + count * 2:2])
        at += count * 2
    if at != len(raw):
        raise ValueError('movement source not completely consumed')
    return sorted(set(actors))


def paths_max_groups(flow):
    """每个group独立取DAG最大值；互斥分支不相加，容量和不声称可达极值。"""
    nodes = {n['id']: n for n in flow['nodes']}
    if len(nodes) != len(flow['nodes']):
        raise ValueError('duplicate flow node')
    pending, done = set(), {}
    def visit(key):
        if key in done:
            return done[key]
        if key in pending or key not in nodes:
            raise ValueError('cyclic or missing flow path')
        pending.add(key)
        node = nodes[key]
        value = Counter()
        for target in node.get('on', {}).values():
            other = visit(target)
            for group, count in other.items():
                value[group] = max(value[group], count)
        if node['op'] == 'append_group':
            value[node['args'][0]] += 1
        elif node['op'] == 'append_group_source':
            raise ValueError('dynamic group in static stage path')
        pending.remove(key)
        done[key] = value
        return value
    return visit(flow['entry'])


def source_value(source, turn):
    if source == 0:
        return turn
    if source == 2:
        return int(turn / 2)
    if source in (3, 4):
        return (2 * ((turn & 255) - 14) + (source == 4)) & 255
    raise ValueError('unsupported dynamic group source')


def group_bound(identity, flow, turns):
    """已审过的normal<=254 multiplicity；不是枚举任意callback外部调用。"""
    if identity == 31:
        return Counter({2: 1, 3: 1})
    if identity == 74:
        return Counter({i: 1 for i in range(4, 8)})
    if identity == 82:
        return Counter({1: 1, 2: 1, 3: 1})
    if identity == 64:
        return Counter({3: 1, 4: 1, 5: 1})
    if identity in DYNAMIC:
        result = Counter()
        for node in flow['nodes']:
            if node['op'] == 'append_group_source':
                for turn in turns:
                    result[source_value(node['args'][0], turn)] += 1
        return result
    return paths_max_groups(flow)


def callers(meta, action):
    result = []
    for slot, (turn, target, phase) in enumerate(meta['turn']):
        if target == action:
            result.append({'kind': 'turn', 'slot': slot, 'turn': turn, 'phase': phase})
    for slot, (target, match) in enumerate(meta['lookup']):
        if target == action:
            result.append({'kind': 'lookup', 'slot': slot, 'match': match})
    for slot, (mode, param) in enumerate(meta['actions']):
        if mode == 2 and param == action:
            result.append({'kind': 'cell_descriptor', 'slot': slot,
                           'qualification': 'descriptor only; map reachability separately checked'})
    for slot, template in enumerate(meta['templates'][:meta['unknown_02']]):
        if template[22] == 2 and int.from_bytes(template[23:25], 'little') == action:
            result.append({'kind': 'drop', 'template': slot, 'group': template[21]})
    return result


def raw_nodes(flow, scripts):
    result = []
    for node in flow['nodes']:
        op, args = node['op'], node['args']
        if op == 'move':
            result.append({'node': node['id'], 'op': op, 'script': args[0],
                           'source_raw_actors': script_actors(scripts[args[0]].data)})
        elif op in {'branch_actor', 'branch_item', 'event_item_remove', 'notice_actor', 'ai_anchor',
                    'unit_field_write', 'unit_ai_write', 'unit_ai_write_source',
                    'unit_hp_zero_tail', 'finalize_defeated', 'teleport_begin'}:
            result.append({'node': node['id'], 'op': op, 'args': args})
    return result


def qualify_raw_nodes(flow, nodes):
    """实参范围以原移动流/固定字段独立提取；min count为已审写序前缀。"""
    if flow in SOURCE_ONLY:
        return [dict(node, disposition='source_only_helper_no_independent_formal_trigger') for node in nodes]
    identity = callback_id(flow)
    minima = {0: 13, 1: 18, 2: 23, 3: 27, 6: 27, 7: 72, 9: 15,
              15: 48, 22: 33, 23: 34, 25: 44, 30: 34, 35: 33, 37: 39,
              56: 70, 70: 46, 76: 76}
    result = []
    for node in nodes:
        node = dict(node)
        op = node['op']
        if op in {'notice_actor','branch_item','event_item_remove'} and node['args'][0] == -1:
            node.update(disposition='normal_active_caller', proof='cell lookup caller is current active actor; no fabricated raw')
        elif identity == 82:
            node.update(disposition='capacity_write_only_or_initialized_original_group0',
                        proof='state16=0..4: reverse24-s is raw20..24; s4 AI raw20; s1..3 new teleports27..32 may be inactive but xy-only, capacity96; original CPU separate')
        elif op == 'unit_hp_zero_tail':
            node.update(disposition='bounded_by_live_active_count', proof='original35f10 tests actor<count before every HP WORD write; no read outside initialized active prefix')
        elif op == 'finalize_defeated':
            node.update(disposition='separate_viewport_death_buffer_obligation',
                        proof='1db65 loops live active prefix; actual124B local frame and temporary lifetime/readback need verification; pseudocode v19[30] alone is not an ABI bound')
        else:
            if identity not in minima:
                raise ValueError('unreviewed dynamic raw node: ' + flow + '/' + node['node'])
            count = minima[identity]
            if identity == 0 and node.get('script') == 8:
                count = 14
            if identity == 37 and node.get('script') == 44:
                count = 45
            if identity == 23 and op == 'unit_ai_write':
                count = 33
            if op == 'move':
                actors = node['source_raw_actors']
            elif op in {'unit_ai_write', 'unit_field_write', 'ai_anchor'}:
                actors = list(range(node['args'][0], node['args'][1] + 1))
            elif op == 'branch_actor':
                actors = [node['args'][0]]
            else:
                raise ValueError('unreviewed dynamic raw operation: ' + op)
            if not actors or min(actors) < 0 or max(actors) >= count:
                raise ValueError('raw prefix witness does not bound all original actors')
            node.update(initialized_prefix_min=count, source_raw_min=min(actors), source_raw_max=max(actors),
                        disposition='pending_early37_reachability' if identity == 37 else 'initialized_active_prefix',
                        proof='requires earlier turn1/phase0 callback35' if identity == 37 else
                              'source-ordered initial/entry/prior mandatory turn append and local group append prefix')
            if identity == 70:
                node['proof'] = 'state18!=0 in original69 requires68/g2 complete; g6/g7 entry14+deploy20+g1/g2 twelve gives minimum46'
        result.append(node)
    return result


def audit(original, source_path, image_path, roster_path):
    sys.setrecursionlimit(20000)
    observed = {}
    def read(path):
        raw = path.read_bytes()
        observed[path] = sha(raw)
        return raw
    read(Path(__file__).resolve())
    source_raw = read(source_path)
    if sha(source_raw) != SOURCE_SHA:
        raise ValueError('not independently reviewed Domain source09')
    source = json.loads(source_raw)
    image = read(image_path)
    verify_source(source, image)
    field_raw = read(original / 'FDFIELD.DAT')
    if sha(field_raw) != SOURCE_HASHES['FDFIELD.DAT']:
        raise ValueError('FDFIELD identity differs')
    entries = archive_entries(field_raw)
    metas = [parse_metadata(entries[3 * stage + 1][1]) for stage in range(33)]
    scripts = source_scripts(original / 'FD2.EXE')
    observed[original / 'FD2.EXE'] = SOURCE_HASHES['FD2.EXE']
    flows, aliases, provenance = load_definitions(ROOT / 'data/static')
    for path in (ROOT / 'data/static').rglob('*.json'):
        read(path)
    changing = {key: flow for key, flow in flows.items() if key.startswith('common.') and
                any(n['op'] in CHANGERS for n in flow['nodes'])}
    verify_reviewed_scope(changing, aliases)
    result = []
    contributions = {stage: Counter() for stage in range(30)}
    for key, flow in changing.items():
        bindings = sorted(k for k, target in aliases.items() if target == key and
                          re.fullmatch(r'stage\.\d+\.action\.\d+', k))
        if (not bindings) != (key in SOURCE_ONLY):
            raise ValueError('source-only/formal dynamic scope changed')
        row = {'flow': key, 'kind': 'source_only_helper' if not bindings else 'formal_consumer',
               'count_or_owner_changers': [n for n in flow['nodes'] if n['op'] in CHANGERS],
               'raw_and_movement_nodes': qualify_raw_nodes(key, raw_nodes(flow, scripts)), 'bindings': []}
        if bindings:
            identity = callback_id(key)
            if identity not in RULES:
                raise ValueError('no reviewed multiplicity policy')
            for alias in bindings:
                match = re.fullmatch(r'stage\.(\d+)\.action\.(\d+)', alias)
                stage, action = map(int, match.groups())
                caller = callers(metas[stage], action)
                if not caller:
                    raise ValueError('formal binding has no original DAT caller')
                turns = [c['turn'] for c in caller if c['kind'] == 'turn' and c['turn'] < 255]
                groups = group_bound(identity, flow, turns)
                contributions[stage].update(groups)
                target = next(t for t in source['tables'] if t['name'] == 'callback')['targets'][action]
                row['bindings'].append({'binding': alias, 'stage': stage, 'callback': action,
                    'target': hex(target), 'original_DAT_callers': caller,
                    'normal_multiplicity_proof': RULES[identity],
                    'group_append_upper_bound': dict(sorted(groups.items())),
                    'roster_appends': [n['args'][0] for n in flow['nodes'] if n['op'] == 'roster_append'],
                    'source_qualification': 'S5 complete original control-flow semantics inherited; this run joins current binding and DAT/Domain identity'})
        result.append(row)
    stages = []
    for stage in range(30):
        meta = metas[stage]
        group_counts = Counter(t[21] for t in meta['templates'][:meta['unknown_02']])
        groups = Counter(contributions[stage])
        initial = meta['unknown_01'] + group_counts[0]
        entry_groups, victory_groups = Counter(), Counter()
        if stage == 0:
            # 原3231b最终原0场景init0的4 persistent+首8模板；不再重复appendg1/2。
            initial = 12
        else:
            alias = f'stage.entry.{stage:03d}'
            entry_groups = paths_max_groups(flows[aliases.get(alias, alias)])
            groups.update(entry_groups)
            initial += sum(group_counts[g] * n for g, n in entry_groups.items())
        alias = f'stage.victory.{stage:03d}'
        if alias in flows or alias in aliases:
            victory_groups = paths_max_groups(flows[aliases.get(alias, alias)])
            groups.update(victory_groups)
        # initial中已含entry；其余每组独立max求和给安全上界，不称真实同时可达极值。
        upper = initial + sum(group_counts[g] * n for g, n in contributions[stage].items())
        upper += sum(group_counts[g] * n for g, n in victory_groups.items())
        if upper > 96:
            raise ValueError(f'stage{stage}: capacity bound exceeds96: {upper}')
        stages.append({'stage': stage, 'deployment_slots': meta['unknown_01'],
            'effective_templates': meta['unknown_02'], 'template_group_counts': dict(sorted(group_counts.items())),
            'initial_count_after_entry_upper': initial, 'entry_group_multiplicities': dict(entry_groups),
            'callback_group_multiplicities': dict(contributions[stage]),
            'victory_group_multiplicities': dict(victory_groups), 'normal_turn_le254_capacity_upper': upper,
            'through_first_U255_capacity_upper': max(upper, 62 if stage == 8 else 34 if stage == 29 else upper),
            'proof_kind': 'sum of separate path/group upper bounds, not a simultaneously reachable maximum'})
    roster_raw = read(roster_path)
    if sha(roster_raw) != ROSTER_SHA:
        raise ValueError('not independently qualified S5 normal roster bound04')
    roster = json.loads(roster_raw)
    if not roster.get('ok') or roster['initial_ids'] != [0, 4, 9, 30]:
        raise ValueError('roster proof shape differs')
    if len(set(roster['initial_ids'] + roster['later_recruit_id_set'])) != 32:
        raise ValueError('roster ID universe differs')
    # 下面是明确的有效前缀／调查债务，不偷换成所有动态未知或已证缺陷。
    obligations = [
      {'id': 'A05-RAW-11-EARLY37', 'flow': 'common.callback.037', 'nodes': ['n0010', 'n0005'],
       'target': '0x35022 ->0x1366a(script43/44)', 'condition': 'stage11 template1(group1)死亡在turn1/phase0 callback35之前',
       'read_domain': '无35时count25；appendg3后31而script43读raw33..37；appendg4后37而script44读raw39..44',
       'status': 'not established as normally reachable; no implementation defect claim',
       'next_observation': 'stage11-order04已执行entry及37/1366a/32230/1f183：0/A5 preload在322a0的29B脚步表(profile-1)首次越界，preload4绕过该表并完整返回count37；需实际DOS heap来源/保留合同，不将allocated raw读本身叫U。正常unit10 level30学23+强者先传送后击杀为明确受控前提，未称训练/自然输入已执行'},
      {'id': 'A05-VPREFIX-29-82', 'flow': 'common.callback.082', 'nodes': ['n0014','n0008','n0005'],
       'target': '0x362e8 ->0x22253', 'condition': '两个81在82排程消费前合并，state16=2/count27',
       'read_domain': '82 appendg2后count29但teleport raw29/30；完整原22253已证目标记录zero-read、仅xy BYTE写、都在96容量',
       'status': 'confirmed allocated write-only V prefix; production xy-only capacity gate fixed with actual C/original regression, new native representative pending',
       'evidence': 'callback82-order02.json; teleport-capacity03/comparison.json; s5-layered82-F1-02/execution.json; natural input setup is source-qualified, not newly GUI executed'},
      {'id': 'A03-U255-PREFIX', 'flow': 'field_turn_event_check', 'nodes': ['slot0..first_default'],
       'target': '0x1a813/0x1a85a', 'condition': 'first turn255 phase0',
       'read_domain': '51f8d has00050302, fetch02030500 outside all FD2 objects; earlier live slots must run before first default',
       'status': 'first U target proved by original CPU; exact natural long-game trace not executed',
       'prefix_cases': {'stage8_without30': '31 twice appendsg0 theng1 before first default; count62 upper, not normal<=254 bound48',
                        'stage29_without81': '82 state0 appendsg0 again; count34 then first default; normal<=254 upper33 remains separate',
                        'stage26_27_28': 'default real callbacks63/66/68/70/74/79 can execute before default255; do not suppress their valid writes'}},
      {'id':'A04-LOCAL30-FINALIZE', 'flow':'common.callback.064', 'nodes':['n0030','n0029'],
       'target':'0x35f10 ->0x1db65/0x1dc29',
       'condition':'state16旧值2分支从raw16..count写HP0后统一finalize；原active容量可达96',
       'read_domain':'全active循环及HP写在96内；1db65实际局部frame为124B，1dc29按屏内nonhidden HP0数写DWORD地址列表。伪C的v19[30]不是已确认ABI边界，须核临时槽活跃期复用及后续实际读取',
       'status':'candidate local storage/lifetime boundary only; neither first U operation nor normally reachable overflow proved',
       'next_observation':'先按完整原frame与写入/回读顺序确认可用槽及首个实际合同外操作，再按原viewport(cameraX-1..cameraX+13, cameraY-1..cameraY+9)和actor轨迹求正常上界或取最小反例；不能把可能80个HP0等同31个屏内死者，也不以第31写或伪C数组长度直接判U'},
    ]
    # 完整当前writer目录供复审；count/状态条件不从节点数推论。
    writers = []
    for alias, target in sorted(aliases.items()):
        if not re.fullmatch(r'stage\.\d+\.action\.\d+', alias):
            continue
        for node in flows[target]['nodes']:
            if node['op'] in {'event_byte_write','event_byte_from_count','turn_relative'}:
                writers.append({'binding': alias, 'flow': target, 'node': node['id'],
                                'op': node['op'], 'args': node['args']})
    for path, expected in observed.items():
        if sha(path.read_bytes()) != expected:
            raise ValueError('source/input changed during audit: ' + str(path))
    return {'schema': 'fd2-f1-dynamic-domain-join-v1', 'inputs': {str(p): h for p, h in observed.items()},
        'obligation_flows': len(result), 'formal_unique_flows': sum(bool(r['bindings']) for r in result),
        'formal_bindings': sum(len(r['bindings']) for r in result), 'source_only_helpers': sorted(SOURCE_ONLY),
        'flows': result, 'stage_capacity_bounds': stages, 'event_and_turn_writers': writers,
        'roster': {'source_receipt': str(roster_path), 'sha256': ROSTER_SHA, 'normal_max': 31,
                   'original_capacity': 32, 'reason': '32 unique IDs minus18/19 mutual exclusion; once-only provenance inherited from S5; no deletion creates capacity',
                   'external_32_record_save_is_not_a_normal32_roster_proof': True},
        'precise_followups_and_valid_prefixes': obligations,
        'claims': {'actual_original_resource_read': True, 'full_source09_bytes_reverified': True,
                   'new_original_CPU_executed': False, 'new_natural_input_executed': False,
                   'capacity_only_join_not_full_semantic_acceptance': True, 'all_raw_readers_closed': False,
                   'F1_complete': False}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--original', type=Path, required=True)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--image', type=Path, required=True)
    parser.add_argument('--roster-proof', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists() or args.output.resolve().is_relative_to(args.original.resolve()):
        raise ValueError('existing/original output forbidden')
    report = audit(args.original, args.source, args.image, args.roster_proof)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('x', encoding='utf8') as stream:
        stream.write(json.dumps(report, ensure_ascii=False, indent=2) + '\n')
    print(json.dumps({'output': str(args.output), 'sha256': sha(args.output.read_bytes()),
                      'flows': report['obligation_flows'], 'formal_bindings': report['formal_bindings'],
                      'capacity_max': max(r['normal_turn_le254_capacity_upper'] for r in report['stage_capacity_bounds']),
                      'full_gate': False}))


if __name__ == '__main__':
    main()
