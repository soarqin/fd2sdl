"""独立盘点冷图，按各自真实walker检查递归与每epoch／advance步数上界。

只读data/static源，不导入生成器或读取生成C作为expected。此门证明结构超集
不触及已登记预算，不证明分支可达性、领域叶内部循环或全游戏语义等价。
"""
from __future__ import annotations
import argparse
from collections import deque
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
BUDGET=65536
DEPTH=8
CALLS={
    'call_init':'stage.source.initialize','call_fade_in':'stage.source.fade.in',
    'call_fade_out':'stage.source.fade.out','call_teleport':'stage.source.teleport',
    'call_transition_finish':'stage.palette.transition.finish',
    'call_cinematic_ending':'stage.cinematic.ending','call_cinematic_credits':'stage.cinematic.credits',
    'call_cinematic_card_normal':'stage.cinematic.card_normal','call_cinematic_card_last':'stage.cinematic.card_last',
    'call_cinematic_card_first':'stage.cinematic.card_first','call_cinematic_montage_round':'stage.cinematic.montage_round'}
# 这里列的是审查过的不同热owner；不是把它们套进Scene的共享计数器。
DYNAMIC_OWNERS={
    ('loot','loot'):('src/field_loot_flow.h','loot_flow_run: local budget per call; collection/descriptor counters and emit/ready continuation'),
    ('preparation','services'):('src/field_prepare_play.c','prepare_service_execute: consumed input_epoch resets steps'),
    ('preparation','deployment'):('src/field_prepare_play.c','prepare_service_execute: consumed input_epoch resets steps'),
    ('progress','reward'):('src/field_reward_flow.h','fd2_reward_advance: local budget per advance; notice returns before ACK continuation'),
    ('progress','phase.status'):('src/field_game.c','status advance uses actor/status-byte counters and notice continuation'),
    ('runtime','choice'):('src/field_round.c','fd2_field_choice_run explicitly has unbounded input wait, no Scene step budget')}

def canonical(document):
    result={}
    for flow in document['flows']:
        if flow['id'] in result:raise ValueError('duplicate flow '+flow['id'])
        result[flow['id']]=flow
    for alias,target in document.get('bindings',{}).items():
        if alias in result or target not in result:raise ValueError('invalid flow binding '+alias)
    return result

def dag_order(nodes,cut=frozenset(),cut_edges=frozenset()):
    degree={key:0 for key in nodes};edges={}
    for key,node in nodes.items():
        edges[key]=[] if key in cut else list(dict.fromkeys(
            target for port,target in node['on'].items() if (key,port) not in cut_edges))
        for target in edges[key]:
            if target not in nodes:raise ValueError('unknown edge '+target)
            degree[target]+=1
    todo=deque(k for k,v in degree.items() if not v);order=[]
    while todo:
        key=todo.popleft();order.append(key)
        for target in edges[key]:
            degree[target]-=1
            if not degree[target]:todo.append(target)
    if len(order)!=len(nodes):raise ValueError('cycle without verified epoch boundary')
    return order,edges

def dag_longest(nodes,weights,roots,cut=frozenset(),cut_edges=frozenset()):
    """cut节点本身照计数，停止其出边；其后继作为下一epoch的入口。"""
    order,edges=dag_order(nodes,cut,cut_edges)
    score={};next_node={}
    for key in reversed(order):
        target=max(edges[key],key=lambda t:score[t],default=None)
        score[key]=weights[key]+(score[target] if target is not None else 0);next_node[key]=target
    if not roots or any(r not in nodes for r in roots):raise ValueError('invalid entry')
    first=max(roots,key=lambda k:score[k]);path=[];cursor=first
    while cursor is not None:path.append(cursor);cursor=next_node[cursor]
    return score[first],path

def cyclic_nodes(flow):
    """仅作结构盘点；Kahn剩余包含回边下游，不冒称精确SCC。"""
    nodes={n['id']:n for n in flow['nodes']};degree={k:0 for k in nodes}
    for node in nodes.values():
        for target in node['on'].values():degree[target]+=1
    todo=deque(k for k,v in degree.items() if not v)
    while todo:
        key=todo.popleft()
        for target in nodes[key]['on'].values():
            degree[target]-=1
            if not degree[target]:todo.append(target)
    return [k for k,v in degree.items() if v]

def scene_proof(documents):
    flows={};aliases={}
    for domain in ('scene','stage'):
        document=documents[domain]
        for identity,flow in canonical(document).items():flows[(domain,identity)]=flow
        aliases.update({(domain,a):(domain,t) for a,t in document.get('bindings',{}).items()})
    def lookup(domain,identity):
        key=(domain,identity);key=aliases.get(key,key)
        if key not in flows:raise ValueError('missing callee '+domain+':'+identity)
        return key
    def call(node):
        op=node['op']
        if op in CALLS:return lookup('stage',CALLS[op]),op.startswith('call_cinematic_')
        if op=='fade':
            if node['args'][:1] not in ([0],[1]):raise ValueError('unknown fade argument')
            return lookup('stage','stage.source.fade.in' if node['args'][0] else 'stage.source.fade.out'),False
        if op=='palette_fade_in':return lookup('stage','stage.source.fade.in'),False
        if op=='opening_stage':
            identity=f"scene.opening.{node['args'][0]:03d}"
            return lookup('scene' if ('scene',identity) in flows else 'stage',identity),False
        if op.startswith('call_'):raise ValueError('unknown Scene call '+op)
        return None
    calls={key:{n['id']:call(n) for n in f['nodes'] if call(n)} for key,f in flows.items()}
    memo={};active=set()
    def prove(key):
        if key in active:raise ValueError('recursive Scene call cycle '+str(key))
        if key in memo:return memo[key]
        active.add(key);flow=flows[key];nodes={n['id']:n for n in flow['nodes']}
        if len(nodes)!=len(flow['nodes']):raise ValueError('duplicate node '+str(key))
        epochs={k for k,n in nodes.items() if n['op']=='cin_credits_epoch'}
        if epochs and key!=('stage','stage.cinematic.credits'):raise ValueError('epoch outside credits')
        weights={n:1 for n in nodes};depth=1;children=[]
        for node,(target,local) in calls[key].items():
            child=prove(target);depth=max(depth,1+child['max_depth_frames'])
            if not local and child['has_shared_epoch']:raise ValueError('shared callee resets parent epoch without proof')
            if not local:weights[node]+=child['max_epoch_steps']
            children.append({'node':node,'op':nodes[node]['op'],'callee':list(target),
                'fresh_budget_restore_parent':local,'charged_to_parent':1 if local else 1+child['max_epoch_steps']})
        roots=[flow['entry'],*flow.get('entries',{}).values()]
        for epoch in epochs:roots.extend(nodes[epoch]['on'].values())
        maximum,path=dag_longest(nodes,weights,list(dict.fromkeys(roots)),epochs)
        if maximum>BUDGET:raise ValueError('Scene epoch exceeds step budget '+str(key)+f': {maximum}')
        if depth>DEPTH:raise ValueError('Scene stack exceeds depth '+str(key)+f': {depth}')
        value={'domain':key[0],'id':key[1],'nodes':len(nodes),'entries':len(set(roots)),
            'max_epoch_steps':maximum,'max_depth_frames':depth,'epochs':len(epochs),'has_shared_epoch':bool(epochs),
            'epoch_reset_timing':'node charged before reset; outgoing targets start next epoch' if epochs else None,
            'longest_local_path_nodes':len(path),'longest_local_path_head':path[:8],'longest_local_path_tail':path[-8:],
            'longest_local_path_sha256':hashlib.sha256(json.dumps(path,separators=(',',':')).encode()).hexdigest(),
            'calls':children,'terminal_waits':[n['id'] for n in nodes.values() if n['op']=='terminal_wait']}
        memo[key]=value;active.remove(key);return value
    for key in flows:prove(key)
    return {'proved':True,'scope':'all structural branch outcomes, not feasible-path/equivalence proof',
        'real_flows':len(flows),'bindings':len(aliases),'calls':sum(map(len,calls.values())),
        'max_epoch_steps':max(r['max_epoch_steps'] for r in memo.values()),
        'max_depth_frames':max(r['max_depth_frames'] for r in memo.values()),
        'flows':[memo[key] for key in sorted(memo)]}

def consumer_contracts(root,read_source=None):
    """固定C审阅依据；规则变更时失败并要求重新审查，不静默沿用旧模型。"""
    if read_source is None:read_source=lambda p:Path(p).read_bytes()
    contracts={
        'src/static_flow.h':['#define FD2_FLOW_STEP_BUDGET 65536u'],
        'src/scene.c':['while (c->flow_steps++ < FD2_FLOW_STEP_BUDGET)',
            'if (!c || c->flow_depth >= 8u) return -1;',
            'if (!c->flow_depth) {c->flow_steps=0;scene_render_error=0;}',
            'scene_flow_run(c,scene_stage_flow("scene.opening",(size_t)a[0]))',
            'return flow ? flow : fd2_static_flow_lookup("stage", id);'],
        'src/scene_cinematic_flow.h':['unsigned parent_steps=c->flow_steps;',
            'c->flow_steps=0;', 'c->flow_steps=parent_steps;',
            '} else if(!strcmp(op,"cin_credits_epoch")) {'],
        'src/field_prepare_play.c':['while(steps++<FD2_FLOW_STEP_BUDGET)',
            'if(s->input_epoch!=epoch){epoch=s->input_epoch;steps=0;}'],
        'src/field_prepare_service_present.h':['s->input_epoch++;return 1;',
            'if (prepare_service_take_key(ui,event)) break;', 'if (prepare_service_take_key(ui,event)) return 0;'],
        'src/field_prepare_service_pick.h':['if (!count) { *port=FD2_FLOW_CANCEL;return 1; }',
            's->input_epoch++;'],
        'src/field_loot_flow.h':['ctx->scan = ctx->pending.count = 0;', 'case 3: ctx->scan++; break;',
            'else l->cursor++;', 'ctx->game->loot.count > FD2_FIELD_MAX_UNITS'],
        'src/field_reward_flow.h':['cursor->growth_cursor = cursor->learn_cursor = 0;',
            'cursor->growth_cursor++;','cursor->learn_cursor++;'],
        'src/field_unit_base.c':['prepared.level = (uint8_t)(unit->level + 1u);',
            'for (size_t i = 0; i < 6u; i++)'],
        'src/field_unit.h':['#define FD2_FIELD_MAX_UNITS 96'],
        'src/field_game.c':['progress->status_cursor++; progress->status_byte = 0;',
            'progress->status_pass = 0; progress->status_cursor = progress->status_byte = 0;',
            'progress->status_pass = 1; progress->status_cursor = progress->status_byte = 0;',
            'progress->status_byte++;', 'if(strcmp(id,"physical.caller.full"))break;',
            'if(run_action_flow(ctx,"physical.full")!=0)return -1;'],
        'src/field_common_event_projection.h':['enum { MAX_PATHS=64 };',
            'size_t used=1;unsigned steps=0;int result=1;',
            'if(++steps>FD2_FLOW_STEP_BUDGET)', 'if(used==MAX_PATHS)',
            'pending[used]=p;pending[used++].pc=fd2_static_flow_edge(flow,source,FD2_FLOW_NO);']}
    # C所有固定call操作和目标都须仍存在；配对语义依据该源码的人工审阅。
    scene=read_source(root/'src/scene.c').decode('utf8')
    cin=read_source(root/'src/scene_cinematic_flow.h').decode('utf8')
    for op,target in CALLS.items():
        source=cin if op.startswith('call_cinematic_') else scene
        if f'"{op}"' not in source or f'"{target}"' not in source:raise ValueError('consumer call mapping changed '+op)
    result=[]
    for relative,fragments in contracts.items():
        source=read_source(root/relative).decode('utf8')
        for fragment in fragments:
            if fragment not in source:raise ValueError('consumer contract changed '+relative+': '+fragment)
        result.append({'file':relative,'required_fragments':fragments})
    return result

def counter_contract(nodes,yields,resets,guards):
    """不能把可反复reset或绕过边界检查的计数器当单调有限循环。"""
    for label,node in nodes.items():
        if node['op'] in resets:
            pending=list(node['on'].values());seen=set()
            while pending:
                current=pending.pop()
                if current==label:raise ValueError('counter reset repeats within one invocation')
                if current in seen:continue
                seen.add(current)
                if nodes[current]['op'] not in yields:pending.extend(nodes[current]['on'].values())
        if node['op'] in guards:
            guard_op,arguments=guards[node['op']]
            target=nodes.get(node['on'].get('next'))
            if not target or target['op']!=guard_op or target['args']!=arguments:
                raise ValueError('counter increment bypasses its finite guard')
        if node['op']=='status_byte_select':
            parents=[(p,port)for p in nodes.values()for port,target in p['on'].items()if target==label]
            if len(parents)!=1 or parents[0][0]['op']!='status_byte_available' or parents[0][1]!='yes' or parents[0][0]['args']!=[6]:
                raise ValueError('status byte increment bypasses its finite guard')

def other_budget_proof(documents):
    """不同热owner各自的单次advance／真实输入epoch，不共用Scene counter。

    切有C依据的yield、输入成功边或单调有限counter后须为DAG。若一个epoch
    至多执行N次counter切点，完整路径至多含N+1段，故L*(N+1)为保守上界。
    计数初值/重置与递增的语义来自下列固定consumer审阅，不从生成器取得。
    """
    rows=[]
    for domain,document in sorted(documents.items()):
        if domain in ('scene','stage'):continue
        for flow in document['flows']:
            identity=flow['id'];key=(domain,identity);nodes={n['id']:n for n in flow['nodes']}
            for n in nodes.values():
                if n['op'].startswith('call_') and (domain,n['op'])!=('common_event','call_teleport'):
                    raise ValueError('unknown cross-owner call '+domain+':'+n['op'])
            cut=set();cut_edges=set();increments=0;rule='acyclic local walker'
            counters={};extra={}
            if key==('runtime','choice'):
                rows.append({'domain':domain,'id':identity,'budget_rejection_exists':False,
                    'proof':'actual fd2_field_choice_run uses for (;;) input wait; V wait plus H quit pump, not an exhausted finite budget',
                    'finite_duration_claimed':False});continue
            if domain=='preparation':
                rule='prepare_service_execute resets only after actual input_epoch changed'
                definite={'menu_pick','pick_companion','wait_key','confirm'}
                picker={'pick_product','pick_item','pick_member','pick_equipment_slot'}
                # 空列表cancel不保证读键，不能借它切epoch；只有slot保证真实输入。
                for label,node in nodes.items():
                    for port in node['on']:
                        if port!='error' and (node['op'] in definite or node['op'] in picker and port=='slot'):
                            cut_edges.add((label,port))
                extra['input_cuts']=[{'node':n,'port':p}for n,p in sorted(cut_edges)]
                extra['empty_picker_cancel_is_not_reset']=True
            elif key==('loot','loot'):
                rule='loot_flow_run local budget per invocation; emit/ready yield; scan and descriptor cursor monotone'
                cut={label for label,node in nodes.items() if node['op'] in {'emit','ready','collect_next','descriptor_next'}}
                counters={'collect_next':96,'descriptor_next':96};increments=sum(counters.values())
                counter_contract(nodes,{'emit','ready'},{'collect_reset','collect_commit','cell_init'},
                    {'collect_next':('branch_collect_more',[1,0]),'descriptor_next':('branch_has_descriptor',[1,0])})
                extra['counter_contract']='scan reset once before collect, bounded by units.count<=96; descriptor cursor reset at most once by collect_commit/cell_init, bounded by count<=96; no wrap before exit'
            elif key==('progress','reward'):
                rule='fd2_reward_advance local budget per invocation; reward_notice always returns before ACK'
                cut={label for label,node in nodes.items() if node['op'] in {'reward_notice','reward_growth_next'}}
                counters={'reward_growth_next':5};increments=5
                counter_contract(nodes,{'reward_notice'},{'reward_cursors_reset','reward_total_init'},
                    {'reward_growth_next':('reward_growth_available',[5])})
                growth=[n for n in nodes.values() if n['op']=='reward_growth_available']
                if len(growth)!=1 or growth[0]['args']!=[5]:raise ValueError('reward growth counter contract changed')
                stop=[n for n in nodes.values() if n['op']=='reward_stop_level']
                if len(stop)!=1 or len(stop[0]['args'])!=5 or any(not 0<=v<=255 for v in stop[0]['args']):
                    raise ValueError('reward BYTE stop-level contract changed')
                # BYTE level increments mod256, the configured stop level is reached in at most256
                # iterations even for a noncanonical entry byte. Each level yields at most1+5+6 notices.
                extra.update(preflight_max_levels=256,preflight_max_notices=1+256*(1+5+6)+1,
                    preflight_max_advance_calls=1+256*(1+5+6)+1+1,
                    counter_contract='growth cursor reset once per level before <=5 increments; learn array has6 entries; every level hits level_notice before cursors reset, so no total/gain loop within one advance')
            elif key==('progress','phase.status'):
                rule='run_status_flow local budget per invocation; status_notice returns; two actor scans and6 bytes per actor'
                cut={label for label,node in nodes.items() if node['op'] in {'status_notice','status_actor_next','status_byte_select'}}
                counters={'status_actor_next':96*2,'status_byte_select':96*6};increments=sum(counters.values())
                counter_contract(nodes,{'status_notice'},{'status_poison_reset','status_timer_reset'},
                    {'status_actor_next':('status_actor_available',[])})
                size=[n for n in nodes.values() if n['op']=='status_byte_available']
                if len(size)!=1 or size[0]['args']!=[6]:raise ValueError('status byte counter contract changed')
                extra.update(preflight_max_notices=96*(1+6),preflight_max_advance_calls=96*(1+6)+1,
                    counter_contract='poison scan and timer scan reset once each; size_t actor cursor at most96 each; status_byte reset per actor and increments only0..6, no wrap')
            elif cyclic_nodes(flow):raise ValueError('unclassified non-Scene cycle '+str(key))
            length,path=dag_longest(nodes,{k:1 for k in nodes},list(nodes),cut,cut_edges)
            maximum=length*(increments+1)
            if maximum>BUDGET:raise ValueError('non-Scene counter bound exceeds budget '+str(key))
            for name in ('preflight_max_advance_calls',):
                if extra.get(name,0)>BUDGET:raise ValueError('preflight advance loop exceeds budget '+str(key))
            rows.append({'domain':domain,'id':identity,'budget_rejection_exists':True,'proof':rule,
                'segment_max_steps':length,'counter_visit_limits':counters,'epoch_max_steps_upper_bound':maximum,
                'cut_nodes':sorted(cut),'structural_bound_proved':True,**extra})
    action_calls=[]
    progress=canonical(documents.get('progress',{'flows':[]}))
    for identity,f in progress.items():
        for n in f['nodes']:
            if n['op']=='action_call_full':
                if identity!='physical.caller.full' or 'physical.full' not in progress:
                    raise ValueError('action_call_full outside fixed nonrecursive caller')
                action_calls.append({'caller':identity,'node':n['id'],'callee':'physical.full',
                    'fresh_local_budget':True,'maximum_stack_frames':2})
    return {'proved':True,'scope':'current source JSON and reviewed consumer counter/yield contracts; no original semantic or timing qualification',
        'flows':rows,'action_calls':action_calls,'separate_owner_call':{'domain':'common_event','op':'call_teleport',
            'call_nodes':sum(n['op']=='call_teleport'for f in documents['common_event']['flows']for n in f['nodes']),
            'callee':'stage.source.teleport','boundary':'Play leaf -> fd2_scene_play_teleport_scope creates zeroed scene_flow_context, fresh root budget/depth; caller common_event yields'}}

def projection_budget_proof(document):
    """预检工作栈的steps跨分支累计；YES立即走、NO暂存，不能用单路径代签。"""
    may_fork={'result_leader_dead','result_side_present','result_all_hidden','branch_hidden_count',
        'branch_item','branch_item_occurrences','branch_actor','event_item_append','event_item_remove'}
    rows=[]
    for flow in document['flows']:
        nodes={n['id']:n for n in flow['nodes']};order,edges=dag_order(nodes);cost={};pending={}
        for key in reversed(order):
            node=nodes[key];on=node['on']
            if node['op'] in may_fork and 'yes' in on and 'no' in on:
                yes,no=on['yes'],on['no']
                # 同一join尾会在两个投影各走一遍；不能去重访问数。
                cost[key]=1+cost[yes]+cost[no]
                pending[key]=max(1+pending[yes],pending[no])
            else:
                cost[key]=1+max((cost[t]for t in edges[key]),default=0)
                pending[key]=max((pending[t]for t in edges[key]),default=0)
        steps=cost[flow['entry']];peak=max(1,pending[flow['entry']])
        if steps>BUDGET or peak>64:raise ValueError('common projection worklist budget exceeded '+flow['id'])
        rows.append({'id':flow['id'],'expanded_total_steps_upper_bound':steps,'pending_slots_upper_bound':peak})
    return {'proved':True,'scope':'all potentially unknown predicates fork; even immutable actor fields conservatively fork; joins charged for each projection',
        'max_expanded_total_steps':max(r['expanded_total_steps_upper_bound']for r in rows),
        'max_pending_slots':max(r['pending_slots_upper_bound']for r in rows),'pending_slot_limit':64,'flows':rows}

def audit(root=ROOT):
    inputs={};documents={};snapshots={}
    def snapshot(path):
        key=str(path.resolve())
        if key not in snapshots:
            data=path.read_bytes();snapshots[key]=data
            inputs[key]={'size':len(data),'sha256':hashlib.sha256(data).hexdigest()}
        return snapshots[key]
    paths=[root/'data/static/manifest.json',Path(__file__),root/'tools/static_data/generate.py']
    paths+=sorted((root/'data/static/flows').glob('*.json'))
    for path in paths:
        data=snapshot(path)
        if path.parent.name=='flows':documents[path.stem]=json.loads(data)
    # 审阅文本、所记摘要及重复读取均借同一份bytes；晚守卫只验证、不覆盖身份。
    contracts=consumer_contracts(root,snapshot)
    for relative in {r['file'] for r in contracts}|{v[0] for v in DYNAMIC_OWNERS.values()}|{
            'src/field_prepare_service_pick.h','src/field_prepare_service_flow.h','src/field_prepare_service_present.h',
            'src/field_common_event_present.h','src/field_common_event_runtime.h','src/field_unit.h',
            'src/field_unit_base.h','src/field_unit_base.c','src/field_game.h'}:
        snapshot(root/relative)
    inventory=[];cycles=[]
    for domain,document in sorted(documents.items()):
        canonical(document)
        inventory.append({'domain':domain,'real_flows':len(document['flows']),
            'bindings':len(document.get('bindings',{})),'nodes':sum(len(f['nodes']) for f in document['flows'])})
        for flow in document['flows']:
            remaining=cyclic_nodes(flow)
            if remaining:
                owner=DYNAMIC_OWNERS.get((domain,flow['id']))
                cycles.append({'domain':domain,'id':flow['id'],'kahn_remaining_including_downstream':len(remaining),
                    'owner':owner,'classification':'Scene credits epoch' if (domain,flow['id'])==('stage','stage.cinematic.credits') else
                    'different owner, proved separately by counter/yield/input-epoch contract' if owner else 'unknown dynamic backedge: blocking'})
    result={'schema':'fd2-static-flow-budget-audit-v1','whole_F1_complete':False,
        'scope':'source JSON + manually reviewed current C consumer contracts; generator hashed as context, never imported or used as expected',
        'consumer_C_change_requires_manual_requalification':True,
        'inputs':inputs,'consumer_contracts':contracts,'inventory':inventory,'structural_cycles':cycles,
        'scene_budget':scene_proof(documents),'other_budgets':other_budget_proof(documents),
        'common_projection':projection_budget_proof(documents['common_event'])}
    result['unknown_dynamic_backedges']=[r for r in cycles if r['owner'] is None and r['classification']!='Scene credits epoch']
    result['sources_unchanged']=all(hashlib.sha256(Path(p).read_bytes()).hexdigest()==f['sha256'] for p,f in inputs.items())
    if not result['sources_unchanged']:raise ValueError('audit inputs changed during read')
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--root',type=Path,default=ROOT)
    parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    protected=(args.root/'original_game').resolve();output=args.output.resolve()
    if output==protected or output.is_relative_to(protected):raise ValueError('审查收据不能写入只读原资源目录')
    result=audit(args.root.resolve())
    with args.output.open('x',encoding='utf8') as stream:json.dump(result,stream,ensure_ascii=False,indent=2);stream.write('\n')
    print(json.dumps({'scene_budget_proved':result['scene_budget']['proved'],
        'max_epoch_steps':result['scene_budget']['max_epoch_steps'],'max_depth_frames':result['scene_budget']['max_depth_frames'],
        'unknown_dynamic_backedges':len(result['unknown_dynamic_backedges']),'whole_F1_complete':False}))
    return 1 if result['unknown_dynamic_backedges'] else 0
if __name__=='__main__':raise SystemExit(main())
