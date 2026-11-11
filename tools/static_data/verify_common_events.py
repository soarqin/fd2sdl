#!/usr/bin/env python3
"""G4通用事件冷定义的独立原来源与关键控制路径核对（不执行游戏）。

严格LE原页/EXE身份来自既有独立parser；不导入生产generator或C输出。
完整函数raw-unrelocated摘要与G1完整Domain读集是不同地址视图，明确分列；
本工具核有源参数和写点顺序，不把它当原机器码执行/C轨迹/原生WGC。
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
from verify_unit_tables import EXE_SHA256, source_range

ROOT = Path(__file__).resolve().parents[2]
TABLES = {
    'k_common_event_nested_descriptors': ('uint8_t', [4, 3], 0x52742, 12,
        '9f78d1a5186c0274ff429e8ae056473d0c843272739cc5263bb42a268837455e'),
    'k_common_event_cell_items': ('uint8_t', [5], 0x5274e, 5,
        'd3ac66045c2d94bfc0384170ba3b559469aa55986989e713edc20f576707eb02'),
}
FLOW_IDS={'common.state.once','common.inventory.once','common.turn.relative','common.random.pair',
    'common.item.collection','common.cell.grant','common.key.consume','common.loot.child.0',
    'common.loot.child.1','common.loot.child.2','common.loot.physical','common.map.replace','common.defeat.tail',
    'common.group.turn.source','common.story.state.source'}
LEGACY_BINDINGS={f'stage.{1 if a==6 else 0:03d}.action.{a:03d}':f'common.legacy.action.{a:03d}'
    for a in (0,1,2,3,4,6)}
FLOW_IDS|=set(LEGACY_BINDINGS.values())
# 完整原函数，不把反编译文本SHA或后续规范名称当字节身份。
RAW_FUNCTIONS = {
    0x348ea: (0x34940, 'e1952039817b884c914670244ba8bbe51715224949f5052da01a7f0dc5bf758c'),
    0x35123: (0x35191, '9ca295701b5335ddba2bd5188e3f81dcf7a5ad37702351a729caa5a7dc219503'),
    0x35e0e: (0x35e5b, 'c20615b884febc42b3fe1ce0778f56196e0ea2e21ed8cd8806a8c0eb5d27822e'),
    0x3623c: (0x362b0, 'f28022dfcaca70df12c2e75e2f3d9a0d5f96fbbd0fbc4d08ed3d6377f0602831'),
    0x35854: (0x35997, '9e832539a680d22e4950e19436b540193c81afeea8b8acc755e2587f34a4b341'),
    0x35a0d: (0x35b78, '753a922a9e47c257cd54b2c15cc844ae4f830b2a8c17d1220bcd0420155ebcc0'),
    0x352ca: (0x35321, 'fcfe8163e6e43a0630b52a9aa8dce877511f5e6f26a63117b914541414f12a55'),
    0x35346: (0x353b5, '12ff26e4bba5009d33095e29ca614467fcfc0ec5db4f76bd75ac8163d01fb152'),
    0x355f0: (0x35638, 'e78526c13fc9d1ed99605642c9d3f2cdf000a1e2233469402246bc935aed8354'),
    0x35730: (0x357dd, 'e831cb1134c78d71f26edd10752ab7f874fbba78021c2c21f03f08fff168b334'),
    0x240fa: (0x24336, 'c69f5f7349fe2e482361f2798d9ca887ffb5303486112c2af8d608a1e78dcc81'),
    0x24754: (0x24b14, '6234821b5799c27782bc070354822df0cb6fb22ff804d09ada54ceee0a4f91e5'),
    0x35f10: (0x35f48, 'c784b007ac99d8c14139501255455537a339af1fdf7b3de9287ef6108b1d5e83'),
    0x35677: (0x356b3, '314871da8571e828d6e6696d3ef63e80fb55b3966db7e70c9d41e0f60e197738'),
    0x4df4c: (0x4df84, '5c8abec62abd33158b2e19102543ae6ab3ea93f96a730d4dba85347729ee675f'),
}
# 动态值来源主span；本项只锚参数/宽度，不声称已执行这些剧情/共享尾。
OPERAND_SPANS = {
    0x35468: (0x3551c, '00295aa52ac814aae9ce4b721cd6c891c38f018c4f8897b6914b8da86b160d8b'),
    0x3553f: (0x355b7, '004ed38b17e8b4bb8fc38cc65ca28d82cac1eec579a96e6d9a1b865d10f36acc'),
    0x35638: (0x35677, 'd74d15cc0079555c3e749064bd52b7d0df53d376d43a9d8177299a5e4847cd18'),
    0x356b3: (0x35722, '8ce599d715f05231216f7c54765ef1916824b8afd55e91883d1c9bee1d872de4'),
    0x35833: (0x35854, '585b070605e85913b830ffa43572bbb88256eff2209957816f1b354cad5962d5'),
    0x362e8: (0x363de, '15cd7c12dc337c6a818b4439ea3d5fbd3012f61c3189664d7a40bc82bb27b7db'),
    0x10b4e: (0x10c50, '06ea347674b886510b0e6a5ccb07c9b732d913a1ffb2f326d63eef0795067330'),
    0x35b78: (0x35bee, 'e07aeed51654a8659008a4d7c1e1fac48315c6908cc11454e111937a8629cdad'),
}

def digest(data):
    return hashlib.sha256(data).hexdigest()

def validate_refresh_water_source(raw):
    # 原11cbc cmp [esp+8],0；11cc1 jnz +5跳过11cc3 call4e31c。
    if raw != bytes.fromhex('837c2408007505e854c60300'):
        raise ValueError('11cac水色原条件/调用点不同')
    return {'canonical_va':'0x11cbc','raw_hex':raw.hex(),
        'raw_argument_0_calls_water':True,'raw_argument_1_skips_water':True,
        'source_kind':'static original instructions; not CPU/native execution'}

def flatten(value, dimensions):
    if not dimensions:
        if type(value) is not int or not 0 <= value <= 255:
            raise ValueError('事件表byte越界/非整数')
        return bytes((value,))
    if not isinstance(value, list) or len(value) != dimensions[0]:
        raise ValueError('事件表维度不同')
    return b''.join(flatten(v, dimensions[1:]) for v in value)

def validate_tables(document, sources):
    rows = document.get('tables', [])
    if type(document.get('schema_version')) is not int or document['schema_version'] != 1 or len(rows) != 2 or {r.get('name') for r in rows} != set(TABLES):
        raise ValueError('事件唯一冷表集合不同')
    for row in rows:
        name = row['name']; c_type, dimensions, va, size, sha = TABLES[name]
        source = row.get('source', {})
        expected = sources[name]
        if (row.get('c_type'), row.get('dimensions')) != (c_type, dimensions) or len(expected) != size or digest(expected) != sha:
            raise ValueError('事件表类型/维度/原读集不同')
        if flatten(row.get('values'), dimensions) != expected:
            raise ValueError('事件冷值与原byte不同')
        if source.get('file') != 'FD2.EXE' or source.get('file_sha256') != EXE_SHA256 or source.get('canonical_va') != hex(va) or source.get('sha256') != sha or not source.get('evidence'):
            raise ValueError('事件表原来源身份不同')

def trace(flow, choices):
    nodes = {n['id']: n for n in flow['nodes']}
    if len(nodes) != len(flow['nodes']):
        raise ValueError('事件节点重复')
    pc = flow['entry']; result = []
    for _ in range(256):
        n = nodes[pc]; result.append((n['op'], n['args']))
        if not n['on']:
            return result
        port = choices.get(pc, 'next' if 'next' in n['on'] else 'ack')
        if port not in n['on']:
            raise ValueError('事件关键路径出口丢失')
        pc = n['on'][port]
    raise ValueError('事件关键路径未终止')

def validate_flow(document):
    if (type(document.get('schema_version')) is not int or document['schema_version'] != 1 or
        document.get('domain') != 'common_event' or document.get('bindings')!=LEGACY_BINDINGS):
        raise ValueError('机制夹具不得自动绑定stage/action；只允许原六个shipping绑定迁移')
    if document.get('operations',{}).get('refresh_map',{}).get('args') != [{'name':'skip_water','min':0,'max':1}]:
        raise ValueError('refresh_map须保原0推进/1跳过水色参数域')
    source = document.get('source', {})
    if source.get('file') != 'FD2.EXE' or source.get('file_sha256') != EXE_SHA256:
        raise ValueError('事件图原EXE身份不同')
    flows = {f['id']: f for f in document['flows']}
    if len(flows) != len(document['flows']) or set(flows) != FLOW_IDS:
        raise ValueError('未审事件机制flow缺失/重复/新增')
    checked = 0
    def check(identity, choices, wanted):
        nonlocal checked
        if trace(flows[identity], choices) != wanted:
            raise ValueError(f'原事件关键控制/提交顺序不同: {identity}')
        checked += 1
    end = [('end', [])]
    check('common.state.once', {'gate':'yes'}, [
        ('branch_event_byte',[16,0,0]),('unit_ai_write',[24,27,7]),('story',[3]),('event_byte_write',[16,16,1,0]),*end])
    check('common.state.once', {'gate':'no'}, [('branch_event_byte',[16,0,0]),*end])
    check('common.inventory.once', {'caller':'yes','capacity':'yes','gate':'yes','grant':'yes'}, [
        ('branch_caller',[0,0]),('branch_actor',[0,7,1,8]),('branch_event_byte',[16,0,0]),
        ('event_item_append',[0,89]),('story',[11]),('event_byte_write',[16,16,1,0]),*end])
    check('common.turn.relative', {'side':'yes','gate':'yes','enabled':'yes'}, [
        ('branch_actor',[-1,1,1,0]),('branch_event_byte',[17,0,0]),('branch_event_byte',[18,1,0]),
        ('turn_relative',[2,0]),('event_byte_write',[17,17,1,0]),*end])
    check('common.random.pair', {}, [('turn_relative',[2,1]),('mark_random_pair',[21]),*end])
    check('common.item.collection', {'count':'yes'}, [
        ('branch_item_occurrences',[0,15,209,214,0,6]),('event_item_consume',[0,15,209,214]),('grant_party_item',[100]),*end])
    prefix = [('clear_keys',[]),('notice_actor',[-1]),('branch_actor',[-1,7,0,8])]
    check('common.cell.grant', {'full':'yes'}, [*prefix,('message',[480,0,99,127]),('portrait_frame',[0]),('wait_key',[0]),('close_box',[]),*end])
    check('common.cell.grant', {'full':'no','item.grant':'yes'}, [*prefix,('cell_item_prepare',[]),('message',[422,0,99,127]),
        ('portrait_frame',[0]),('wait_key',[0]),('event_item_append',[-1,-1]),('close_box',[]),('event_byte_write',[0,4,1,0]),('completed_cells',[]),*end])
    prefix = [('branch_event_byte',[12,0,0]),('notice_actor',[-1]),('branch_item',[-1,208])]
    check('common.key.consume', {'gate':'yes','has.key':'no'}, [*prefix,('message',[2,1,95,119]),('portrait_frame',[0]),('wait_key',[0]),('close_box',[]),*end])
    check('common.key.consume', {'gate':'yes','has.key':'yes','consume':'yes'}, [*prefix,('event_item_remove',[-1,208]),
        ('message',[3,1,95,119]),('wait_key',[0]),('close_box',[]),('raw_animation',[45,0,59,2,36,151,-1]),
        ('event_byte_write',[12,12,1,0]),('completed_cells',[]),('append_group',[1,0]),('roster_append',[31]),('story',[4]),*end])
    for index in range(3):
        check(f'common.loot.child.{index}', {}, ([('story',[3])] if index==1 else []) +
            [('loot_child',[index,-1]),('story',[11]),*end])
    check('common.loot.physical', {'caller':'yes','gate':'yes','hidden':'yes'}, [
        ('branch_caller',[0,0]),('branch_event_byte',[0,0,0]),('story',[0]),('physical_exchange',[17,0]),
        ('finalize_defeated',[]),('branch_actor',[17,0,1,0]),('event_byte_write',[0,0,1,0]),
        ('completed_cells',[]),('refresh_map',[1]),('loot_child',[3,0]),('contribution_clear',[]),*end])
    check('common.loot.physical', {'caller':'no'}, [('branch_caller',[0,0]),('contribution_clear',[]),*end])
    check('common.map.replace', {}, [('map_replace',[69,23]),('map_reset_cells',[]),('background_rebuild',[]),('pan',[14,29]),
        ('brightness',[0]),('move',[73,0]),('pan',[14,14]),('move',[73,0]),('move',[73,0]),('story',[17]),*end])
    check('common.defeat.tail', {}, [('story',[5]),('unit_hp_zero_tail',[18]),('finalize_defeated',[]),*end])
    # 明确的原参数/调用片段，不把尾跳转或周围关卡编排伪装成已翻译。
    check('common.group.turn.source', {}, [('append_group_source',[0,0]),*end])
    check('common.story.state.source', {}, [('story_source',[9]),*end])
    for flow in flows.values():
        for node in flow['nodes']:
            if node['on'] and node['on'].get('error') != 'failed':
                raise ValueError('事件错误路径必须冻结，不能回entry重放提交')
    return {'source_control_paths_checked':checked,'flow_count':len(flows),'bindings_added':0,
        'existing_bindings_migrated':6,'legacy_full_call_order_requires':'verify_fd2_legacy_events.py independent CPU check'}

def verify(executable, root=ROOT):
    exe = Path(executable).read_bytes()
    if digest(exe) != EXE_SHA256:
        raise ValueError('FD2.EXE身份不同')
    refresh_water=validate_refresh_water_source(source_range(exe,0x11cbc,12))
    chunks = []
    for va, (end, sha) in RAW_FUNCTIONS.items():
        if digest(source_range(exe, va, end-va)) != sha:
            raise ValueError('完整原函数raw LE页不同')
        chunks.append({'canonical_va':hex(va),'end':hex(end),'raw_unrelocated_sha256':sha})
    operands=[]
    for va,(end,sha) in OPERAND_SPANS.items():
        if digest(source_range(exe,va,end-va)) != sha:
            raise ValueError('动态operand原来源span不同')
        operands.append({'canonical_va':hex(va),'end':hex(end),'raw_unrelocated_sha256':sha})
    sources = {name:source_range(exe,s[2],s[3]) for name,s in TABLES.items()}
    tables = json.loads((root/'data/static/tables/common_event.json').read_text(encoding='utf-8'))
    flows = json.loads((root/'data/static/flows/common_event.json').read_text(encoding='utf-8'))
    validate_tables(tables,sources); result=validate_flow(flows)
    if Path(executable).read_bytes() != exe:
        raise ValueError('核对期间原来源变化')
    return {'ok':True,'schema':'fd2-g4-common-events-source-v1',**result,'refresh_water_source':refresh_water,
        'executable_sha256':EXE_SHA256,'tables':len(TABLES),'original_table_bytes':17,
        'functions':chunks,'operand_source_spans':operands,
        'source_view':'strict LE initialized raw-unrelocated pages; canonical relbase VA',
        'Domain_context':'G1 scope-root-readset: complete roots/shared tail3530d/3531b, with separate relocated chunk hashes',
        'original_machine_code_executed':False,'C_implementation_compared':False,'native_WGC_acceptance':False,
        'scope':'finite-rule source fixtures;240fa item portion and24754 post-map portion are explicit excerpts, not complete stage binding'}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable',type=Path)
    parser.add_argument('--json',type=Path)
    args=parser.parse_args(); result=verify(args.executable)
    encoded=json.dumps(result,ensure_ascii=False,indent=2)+'\n'
    if args.json:
        args.json.parent.mkdir(parents=True,exist_ok=True);args.json.write_text(encoded,encoding='utf-8')
    print(encoded,end='')

if __name__=='__main__':
    main()
