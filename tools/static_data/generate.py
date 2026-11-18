"""校验冷定义并生成 C11 数据；只使用 Python 标准库。"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
SCHEMA_VERSION = 2
INTEGER_TYPES = {
    "uint8_t": (0, 255), "int8_t": (-128, 127),
    "uint16_t": (0, 65535), "int16_t": (-32768, 32767),
    "uint32_t": (0, 4294967295), "int32_t": (-2147483648, 2147483647),
    "int": (-2147483648, 2147483647),
}
STRUCT_TYPES = {"fd2_field_stage_music": ["uint8_t", "uint8_t"]}
PORTS = ("next", "yes", "no", "cancel", "ack", "text_ready", "slot", "error")
S5_CINEMATIC_IDS = {"stage.cinematic."+name for name in
    ("ending","credits","card_normal","card_last","card_first","montage_round")}
S5_SOURCE_WIDE_IDS = {f"stage.source.{kind}.{stage:03d}" for kind,stages in (
    ("entry",(24,29)), ("victory",(20,22,23,26,28,29))) for stage in stages}


def reject_duplicates(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_json(path: Path):
    return decode_json(path.read_bytes())


def decode_json(raw):
    return json.loads(raw.decode("utf-8"), object_pairs_hook=reject_duplicates,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)))


def integer(value, c_type):
    lo, hi = INTEGER_TYPES[c_type]
    if type(value) is not int or not lo <= value <= hi:
        raise ValueError(f"{c_type} value outside [{lo}, {hi}]: {value!r}")


def validate_values(values, dimensions, c_type):
    if dimensions:
        if not isinstance(values, list) or len(values) != dimensions[0]:
            raise ValueError(f"array extent mismatch: expected {dimensions}")
        for value in values:
            validate_values(value, dimensions[1:], c_type)
    elif c_type in STRUCT_TYPES:
        fields = STRUCT_TYPES[c_type]
        if not isinstance(values, list) or len(values) != len(fields):
            raise ValueError(f"{c_type}: wrong field count")
        for value, field in zip(values, fields):
            integer(value, field)
    else:
        integer(values, c_type)


def validate_document(document):
    if not isinstance(document, dict) or set(document) != {"schema_version", "tables"}:
        raise ValueError("table document requires schema_version and tables only")
    if type(document["schema_version"]) is not int or document["schema_version"] != SCHEMA_VERSION:
        raise ValueError("unsupported static schema_version")
    if not isinstance(document["tables"], list) or not document["tables"]:
        raise ValueError("nonempty tables required")
    names = set()
    for table in document["tables"]:
        if not isinstance(table, dict) or set(table) != {"name", "c_type", "dimensions", "values"}:
            raise ValueError("unknown/missing table field")
        name, c_type, dimensions = table["name"], table["c_type"], table["dimensions"]
        if not isinstance(name, str) or not re.fullmatch(r"[a-zA-Z][a-zA-Z0-9_]*", name) or name in names:
            raise ValueError(f"invalid/duplicate C table identifier: {name}")
        names.add(name)
        if c_type not in INTEGER_TYPES and c_type not in STRUCT_TYPES:
            raise ValueError(f"unsupported C type: {c_type}")
        if not isinstance(dimensions, list) or not 1 <= len(dimensions) <= 3 or any(
                type(n) is not int or not 1 <= n <= 65536 for n in dimensions):
            raise ValueError(f"invalid dimensions: {dimensions}")
        validate_values(table["values"], dimensions, c_type)
    return document


def initializer(value):
    if isinstance(value, list):
        return "{" + ", ".join(initializer(item) for item in value) + "}"
    # uint32 最大值也应按无符号常量生成，避免 MSVC 的 decimal signed warning。
    return str(value) + ("u" if value > 2147483647 else "")


def validate_references(tables):
    """只校验已确认的跨表引用，不把未解码 byte 猜成引用。"""
    required = {"k_profiles", "k_player_base_records", "k_enemy_records", "ITEM_RECORDS", "k_growth_records", "k_learning_records", "MAGIC_RECORDS"}
    if not required <= tables.keys():
        raise ValueError(f"missing referenced tables: {sorted(required-tables.keys())}")
    if "k_field_range_sequence" not in tables or len(tables["k_field_range_sequence"])!=20 or any(
            not 0<=frame<23 for frame in tables["k_field_range_sequence"]):
        raise ValueError("invalid original range LUT reference")
    def reference(value, count, label, sentinel=None, first=0):
        if value != sentinel and not first <= value < first + count:
            raise ValueError(f"invalid {label} reference: {value}")
    profiles = len(tables["k_profiles"])
    for row in tables["k_player_base_records"] + tables["k_enemy_records"]:
        reference(row[1], profiles, "movement profile", first=1)
    for row in tables["k_player_base_records"]:
        for item in row[12:18]:
            reference(item, len(tables["ITEM_RECORDS"]), "initial item", sentinel=255)
    for row in tables["k_growth_records"]:
        reference(row[10], len(tables["k_learning_records"]), "learning selector", sentinel=255)
    for row in tables["k_learning_records"]:
        for magic in row[1::2]:
            reference(magic, len(tables["MAGIC_RECORDS"]), "learned magic", sentinel=255)


def preparation_views(tables):
    """只从唯一冷源派生费用引用mask及重叠的第六项返回提示。"""
    names = {"k_prepare_revive_coefficients", "k_prepare_promotion_records",
             "k_prepare_special_items", "k_prepare_service_return_prefix",
             "k_prepare_town_background_entries", "k_prepare_town_service_entries",
             "k_prepare_deployment_roster_gate"}
    if not names <= tables.keys():
        raise ValueError("missing preparation projection inputs")
    coefficients = tables["k_prepare_revive_coefficients"]
    prefix = tables["k_prepare_service_return_prefix"]
    if len(coefficients) != 29 or len(prefix) != 5:
        raise ValueError("preparation projection source extent differs")
    valid = [0] * len(coefficients)
    for profile in ([row[1] for row in tables["k_player_base_records"]] +
                    [row[0] for row in tables["k_prepare_promotion_records"]]):
        if not 1 <= profile < len(valid):
            raise ValueError("invalid preparation revive profile reference")
        valid[profile] = 1
    item_count = len(tables["ITEM_RECORDS"])
    for item in tables["k_prepare_special_items"]:
        if item != 255 and not 0 <= item < item_count:
            raise ValueError("invalid preparation promotion item reference")
    for row in tables["k_stage_prepare"]:
        if not 0 <= row[0] < len(tables["k_prepare_town_background_entries"]):
            raise ValueError("invalid preparation town type reference")
        for item in row[3:]:
            if item != 255 and not 0 <= item < item_count:
                raise ValueError("invalid preparation stock item reference")
    if len(tables["k_prepare_town_service_entries"]) != 6 or tables["k_prepare_town_service_entries"][2] != 255:
        raise ValueError("departure selector cannot load a preparation service asset")
    for first, last, slot, text in tables["k_prepare_deployment_roster_gate"]:
        if first > last or not 0 <= slot < 96:
            raise ValueError("invalid preparation deployment gate reference")
    return [
        {"name": "k_prepare_revive_profile_valid", "c_type": "uint8_t", "dimensions": [29],
         "values": valid},
        {"name": "k_prepare_service_return_fragments", "c_type": "uint16_t", "dimensions": [6],
         "values": prefix + coefficients[:1]},
    ]


def render_tables(tables):
    lines = ["/* 由 tools/static_data/generate.py 生成，禁止手改。 */"]
    for table in tables:
        dimensions = "".join(f"[{n}]" for n in table["dimensions"])
        lines.append(f"static const {table['c_type']} {table['name']}{dimensions} = {{")
        lines.extend("    " + initializer(row) + "," for row in table["values"])
        lines.extend(["};", ""])
    return "\n".join(lines).encode("utf-8")


def read_movement_scripts(document):
    if (not isinstance(document, dict) or set(document) != {"schema_version", "scripts"} or
            type(document["schema_version"]) is not int or document["schema_version"] != SCHEMA_VERSION or
            not isinstance(document["scripts"], dict) or not document["scripts"]):
        raise ValueError("movement script document requires schema_version and nonempty scripts")
    scripts = {}
    for identity, data in document["scripts"].items():
        if not re.fullmatch(r"0|[1-9][0-9]{0,2}", identity) or int(identity) > 255:
            raise ValueError(f"invalid movement script ID: {identity}")
        if not isinstance(data, list) or not data:
            raise ValueError(f"movement script {identity} must be a nonempty byte stream")
        for value in data:
            integer(value, "uint8_t")
        at = 1
        for _ in range(data[0]):
            if at + 2 > len(data):
                raise ValueError(f"movement script {identity}: truncated group")
            mode, count = data[at:at+2]
            at += 2
            end = at + count * 2
            if count > 32 or end > len(data):
                raise ValueError(f"movement script {identity}: invalid actor pair count")
            for pos in range(at, end, 2):
                actor, direction = data[pos:pos+2]
                if actor >= 96 or (direction > 3 and not (mode == 0x80 and direction == 5)):
                    raise ValueError(f"movement script {identity}: invalid actor/direction pair")
            at = end
        if at != len(data):
            raise ValueError(f"movement script {identity}: stream not completely consumed")
        scripts[int(identity)] = data
    return scripts


def render_movement_scripts(scripts):
    tables = []
    lines = ["static const fd2_static_movement_script k_movement_scripts[] = {"]
    for identity, data in sorted(scripts.items()):
        name = f"k_movement_script_{identity:02x}"
        tables.append({"name": name, "c_type": "uint8_t", "dimensions": [len(data)], "values": data})
        lines.append(f"    {{{identity}, {name}, sizeof({name})}},")
    lines.extend(["};", ""])
    return render_tables(tables) + "\n".join(lines).encode("utf-8")


def constructor_alias_records(tables):
    """原getter的两条实际调用窗；值仅投影既有唯一冷源，不再编辑一份byte。"""
    views = tables["k_player_base_alias_views"]
    if views != [[48, 0, 384], [66, 1, 68]]:
        raise ValueError("constructor alias reference domain differs from reviewed original callers")
    sources = [tables["k_growth_records"], tables["k_stage_prepare"]]
    flattened = [[byte for row in rows for byte in row] for rows in sources]
    result = []
    for identity, kind, offset in views:
        if kind >= len(flattened) or offset < 0 or offset + 24 > len(flattened[kind]):
            raise ValueError("constructor alias reference out of source extent")
        result.append(flattened[kind][offset:offset + 24])
    return result


def validate_flows(document):
    """有限图结构和固定参数契约；不会执行脚本或推断未实现的领域语义。"""
    required = {"schema_version", "domain", "operations", "flows"}
    if not isinstance(document, dict) or not required <= document.keys() or set(document) - required - {"bindings"}:
        raise ValueError("unknown/missing flow document field")
    if type(document["schema_version"]) is not int or document["schema_version"] != SCHEMA_VERSION:
        raise ValueError("unsupported flow schema")
    if not isinstance(document["domain"], str) or not re.fullmatch(r"[a-z][a-z0-9_]*", document["domain"]):
        raise ValueError("invalid flow domain")
    operations = document["operations"]
    if not isinstance(operations, dict) or not operations:
        raise ValueError("operation contracts required")
    for name, spec in operations.items():
        if not re.fullmatch(r"[a-z][a-z0-9_]*", name) or not isinstance(spec, dict) or set(spec) != {"args"}:
            raise ValueError(f"invalid operation contract: {name}")
        if not isinstance(spec["args"], list) or len(spec["args"]) > 8:
            raise ValueError(f"invalid operation argument count: {name}")
        for argument in spec["args"]:
            if not isinstance(argument, dict) or set(argument) != {"name", "min", "max"} or not re.fullmatch(r"[a-z][a-z0-9_]*", argument["name"]):
                raise ValueError(f"invalid argument contract: {name}")
            if any(type(argument[k]) is not int for k in ("min", "max")) or not -2147483648 <= argument["min"] <= argument["max"] <= 2147483647:
                raise ValueError(f"invalid argument range: {name}")
    if not isinstance(document["flows"], list) or not document["flows"]:
        raise ValueError("nonempty flows required")
    flow_ids = set()
    for flow in document["flows"]:
        if not isinstance(flow, dict) or not {"id", "entry", "nodes"} <= flow.keys() or set(flow) - {"id", "entry", "entries", "nodes"}:
            raise ValueError("invalid flow fields")
        if not isinstance(flow["id"], str) or not re.fullmatch(r"[a-z0-9_.-]+", flow["id"]) or flow["id"] in flow_ids:
            raise ValueError("invalid/duplicate flow ID")
        flow_ids.add(flow["id"])
        nodes = flow["nodes"]
        # 只有common_event的现有宽PC热owner消费S2长序列；reward等仍是BYTE游标。
        maximum_nodes = 32767 if (document['domain'] == 'common_event' or
            document['domain'] == 'stage' and flow['id'] in S5_CINEMATIC_IDS|S5_SOURCE_WIDE_IDS) else 254
        if not isinstance(nodes, list) or not 1 <= len(nodes) <= maximum_nodes:
            raise ValueError("flow node capacity exceeded")
        ids = set()
        for node in nodes:
            if not isinstance(node, dict) or not {"id", "op", "args", "on"} <= node.keys() or set(node) - {"id", "op", "args", "on", "views"}:
                raise ValueError("unknown/missing node fields")
            if not isinstance(node["id"], str) or not re.fullmatch(r"[a-z0-9_.-]+", node["id"]) or node["id"] in ids:
                raise ValueError("invalid/duplicate node ID")
            ids.add(node["id"])
            if node["op"] not in operations:
                raise ValueError(f"unregistered operation: {node['op']}")
            arguments = operations[node["op"]]["args"]
            if not isinstance(node["args"], list) or len(node["args"]) != len(arguments) or any(
                    type(v) is not int or not a["min"] <= v <= a["max"] for v, a in zip(node["args"], arguments)):
                raise ValueError(f"invalid node arguments: {node['id']}")
            if document["domain"] in {"common_event", "stage"}:
                op,a=node["op"],node["args"]
                if op in {"branch_hidden_count","unit_field_write","unit_ai_write","event_byte_write",
                        "event_item_consume","branch_item_occurrences"} and a[0]>a[1]:
                    raise ValueError("invalid common event actor/state interval")
                if op in {"event_item_consume","branch_item_occurrences"} and a[2]>a[3]:
                    raise ValueError("invalid common event item interval")
                if op=="unit_field_write" and a[2]<18 and a[4]>255:
                    raise ValueError("invalid common event BYTE field value")
            if not isinstance(node["on"], dict) or set(node["on"]) - set(PORTS):
                raise ValueError(f"invalid node ports: {node['id']}")
            if node['op']=='terminal_wait' and (node['on'] or document['domain']!='stage' or
                    flow['id'] not in {'stage.source.victory.026','stage.source.victory.029'}):
                raise ValueError('original terminal loop cannot have a fabricated return port/caller')
            views = node.get("views", ["logic", "present"])
            if not isinstance(views, list) or not views or len(set(views)) != len(views) or set(views) - {"logic", "present"}:
                raise ValueError("invalid node execution views")
        entries = flow.get("entries", {})
        if not isinstance(entries, dict) or any(not isinstance(k, str) or not re.fullmatch(r"[a-z0-9_.-]+", k) for k in entries):
            raise ValueError("invalid external entry labels")
        roots = [flow["entry"], *entries.values()]
        if any(not isinstance(n, str) or n not in ids for n in roots):
            raise ValueError("unknown entry node")
        if any(not isinstance(edge, str) or edge not in ids for node in nodes for edge in node["on"].values()):
            raise ValueError("unknown edge target")
        seen, pending = set(), list(roots)
        by_id = {n["id"]: n for n in nodes}
        while pending:
            current = pending.pop()
            if current not in seen:
                seen.add(current)
                pending.extend(by_id[current]["on"].values())
        if seen != ids:
            raise ValueError(f"unreachable flow nodes: {sorted(ids-seen)}")
        # 循环可表示原来的遍历；消费端每次推进均受不可配置预算限制。
        # 全图必须有明确出口，不能把闭合循环误当成功结束。
        terminals = {n["id"] for n in nodes if not n["on"]}
        can_finish, pending = set(terminals), list(terminals)
        parents = {identity: [] for identity in ids}
        for node in nodes:
            for target in node["on"].values():parents[target].append(node["id"])
        while pending:
            for parent in parents[pending.pop()]:
                if parent not in can_finish:
                    can_finish.add(parent);pending.append(parent)
        if ids - can_finish:
            raise ValueError(f"flow has no reachable exit: {sorted(ids-can_finish)}")
    bindings = document.get("bindings", {})
    if not isinstance(bindings, dict):
        raise ValueError("flow bindings must be an object")
    for alias, target in bindings.items():
        if not isinstance(alias, str) or not re.fullmatch(r"[a-z0-9_.-]+", alias) or alias in flow_ids:
            raise ValueError("invalid/duplicate flow binding ID")
        # 只指向同文件的真实流程；不允许别名链、循环或悬空引用。
        if not isinstance(target, str) or target not in flow_ids:
            raise ValueError("flow binding must target a real flow")
    return document


def flow_index(document):
    """别名保留同一个节点对象；仅增加稳定运行ID，不复制可编辑定义。"""
    flows = {flow["id"]: flow for flow in document["flows"]}
    for target in document.get("bindings", {}).values():
        if target not in flows:
            raise ValueError(f"required runtime flow binding target missing: {target}")
    return {**flows, **{alias: flows[target] for alias, target in document.get("bindings", {}).items()}}


def render_flows(document):
    domain = document["domain"]
    lines = ["/* 冷流程生成定义，禁止手改；原语语义由领域 C 模块提供。 */"]
    for i, flow in enumerate(document["flows"]):
        ids = {n["id"]: j for j, n in enumerate(flow["nodes"])}
        lines.append(f"static const fd2_static_flow_node k_{domain}_nodes_{i}[] = {{")
        for node in flow["nodes"]:
            args = node["args"] + [0] * (8-len(node["args"]))
            edges = [ids[node["on"][port]] if port in node["on"] else -1 for port in PORTS]
            views = sum({"logic": 1, "present": 2}[v] for v in node.get("views", ["logic", "present"]))
            lines.append("    {" + json.dumps(node["id"]) + ", " + json.dumps(node["op"]) + ", " + initializer(args) + ", " + initializer(edges) + f", {views}" + "},")
        lines.append("};")
    lines.append(f"static const fd2_static_flow k_{domain}_flows[] = {{")
    for i, flow in enumerate(document["flows"]):
        entry = next(j for j, node in enumerate(flow["nodes"]) if node["id"] == flow["entry"])
        lines.append("    {" + json.dumps(flow["id"]) + f", k_{domain}_nodes_{i}, {len(flow['nodes'])}, {entry}" + "},")
    indices = {flow["id"]: i for i, flow in enumerate(document["flows"])}
    for alias, target in sorted(document.get("bindings", {}).items()):
        i = indices[target]
        flow = document["flows"][i]
        entry = next(j for j, node in enumerate(flow["nodes"]) if node["id"] == flow["entry"])
        lines.append("    {" + json.dumps(alias) + f", k_{domain}_nodes_{i}, {len(flow['nodes'])}, {entry}" + "},")
    lines.extend(["};", f"#define FD2_{domain.upper()}_FLOW_COUNT (sizeof(k_{domain}_flows)/sizeof(k_{domain}_flows[0]))", ""])
    return "\n".join(lines).encode("utf-8")


def validate_stage_source_calls(document):
    """固定有源callee白名单；普通source保原总预算，credits按原局部卡片域。"""
    if document['domain'] != 'stage': return
    calls = {'call_init': 'stage.source.initialize', 'call_fade_in': 'stage.source.fade.in',
             'call_fade_out': 'stage.source.fade.out',
             'call_teleport': 'stage.source.teleport',
             'call_transition_finish': 'stage.palette.transition.finish',
             'call_cinematic_ending': 'stage.cinematic.ending',
             'call_cinematic_credits': 'stage.cinematic.credits',
             'call_cinematic_card_normal': 'stage.cinematic.card_normal',
             'call_cinematic_card_last': 'stage.cinematic.card_last',
             'call_cinematic_card_first': 'stage.cinematic.card_first',
             'call_cinematic_montage_round': 'stage.cinematic.montage_round'}
    cinematic_ids = {value for key,value in calls.items() if key.startswith('call_cinematic_')}
    flows = {flow['id']: flow for flow in document['flows']}
    for operation in calls:
        if operation in document['operations'] and document['operations'][operation] != {'args': []}:
            raise ValueError('fixed stage source call ABI differs')
    cinematic_callers = {
        'call_cinematic_ending': {'stage.source.victory.026','stage.source.victory.029'},
        'call_cinematic_credits': {'stage.cinematic.ending'},
        'call_cinematic_card_normal': {'stage.cinematic.credits'},
        'call_cinematic_card_last': {'stage.cinematic.credits'},
        'call_cinematic_card_first': {'stage.cinematic.card_normal','stage.cinematic.card_last'},
        'call_cinematic_montage_round': {'stage.cinematic.ending'}}
    for flow in document['flows']:
        for node in flow['nodes']:
            if node['op']=='cin_animation' and node['args'][0] not in (0,2):
                raise ValueError('cinematic ANI outside confirmed 0/2 caller domain')
            if node['op'] in cinematic_callers and flow['id'] not in cinematic_callers[node['op']]:
                raise ValueError('fixed cinematic call outside original caller domain')
            if node['op']=='cin_credits_epoch' and flow['id']!='stage.cinematic.credits':
                raise ValueError('credits epoch outside original outer loop')
        if flow['id'] in cinematic_ids:
            # 每条视觉回边必须穿过credits原outer边界；其它固定callee不能
            # 借epoch预算复位形成新脚本循环。cut后的图须为DAG。
            nodes={n['id']:n for n in flow['nodes']};degree={key:0 for key in nodes}
            for n in nodes.values():
                if n['op']=='cin_credits_epoch': continue
                for target in n['on'].values():degree[target]+=1
            pending=[key for key,value in degree.items() if not value];visited=0
            while pending:
                current=nodes[pending.pop()];visited+=1
                if current['op']=='cin_credits_epoch': continue
                for target in current['on'].values():
                    degree[target]-=1
                    if not degree[target]:pending.append(target)
            if visited!=len(nodes):raise ValueError('cinematic cycle lacks original credits epoch')
    memo = {}
    def flow_cost(identity, ancestry=()):
        if identity in ancestry or len(ancestry) >= 8:
            raise ValueError('stage source helper cycle/depth exceeds Scene contract')
        if identity in memo: return memo[identity]
        if identity not in flows: raise ValueError('fixed stage source helper missing')
        flow = flows[identity]; nodes = {node['id']: node for node in flow['nodes']}
        if identity in cinematic_ids:
            # 原credits的有限outer/card状态及输入skip可回到卡片。逐次callee
            # 仍受Scene预算/深度约束；不把32张卡的累计量伪作一张卡的PC容量。
            total=len(nodes)
            for node in flow['nodes']:
                if node['op'] in calls:
                    callee_cost=flow_cost(calls[node['op']],ancestry+(identity,))
                    if calls[node['op']] not in cinematic_ids: total+=callee_cost
            if len(nodes)>32767 or total>65536:
                raise ValueError('cinematic local graph exceeds Scene budget')
            memo[identity]=total
            return total
        visited, active, pending = {}, set(), [(flow['entry'], False)]
        while pending:
            label, exiting = pending.pop()
            if label in visited: continue
            node = nodes[label]
            if exiting:
                cost = 1 + (flow_cost(calls[node['op']], ancestry + (identity,)) if node['op'] in calls else 0)
                visited[label] = cost + max((visited[target] for target in node['on'].values()), default=0)
                active.remove(label)
            else:
                if label in active: raise ValueError('stage source graph cycle')
                active.add(label); pending.append((label, True))
                pending.extend((target, False) for target in node['on'].values() if target not in visited)
        total = visited[flow['entry']]
        if total > 65536: raise ValueError('expanded stage source path exceeds Scene budget')
        memo[identity] = total
        return total
    for flow in document['flows']:
        if any(node['op'] in calls for node in flow['nodes']):
            if not flow['id'].startswith('stage.source.') and flow['id'] not in cinematic_ids:
                raise ValueError('fixed source calls cannot change existing stage routes')
            flow_cost(flow['id'])


def validate_flow_contract(document, contract, script_ids, all_flows):
    validate_stage_source_calls(document)
    required = {"operation_abi_sha256", "ports", "required_ids"}
    if not isinstance(contract, dict) or not required <= contract.keys() or set(contract) - required - {"required_labels"}:
        raise ValueError("invalid consumer flow contract")
    digest = hashlib.sha256(json.dumps(document["operations"], sort_keys=True).encode("utf-8")).hexdigest()
    if contract["operation_abi_sha256"] != digest:
        raise ValueError("flow operation ABI differs from consumer contract")
    if not isinstance(contract["required_ids"], list) or not contract["required_ids"] or len(set(contract["required_ids"])) != len(contract["required_ids"]) or any(not isinstance(v, str) for v in contract["required_ids"]):
        raise ValueError("required flow IDs must be a nonempty unique list")
    flows = flow_index(document)
    if not set(contract["required_ids"]) <= flows.keys():
        raise ValueError("required runtime flow missing")
    labels = contract.get("required_labels", {})
    if not isinstance(labels, dict):
        raise ValueError("required node labels must be an object")
    for identity, names in labels.items():
        if not isinstance(identity, str) or identity not in flows:
            raise ValueError("required node labels reference unknown flow")
        if not isinstance(names, list) or not names or any(
                not isinstance(name, str) or not re.fullmatch(r"[a-z0-9_.-]+", name) for name in names) or len(set(names)) != len(names):
            raise ValueError("required node labels must be a nonempty unique ID list")
        actual = {node["id"] for node in flows[identity]["nodes"]}
        for name in names:
            if name not in actual:
                raise ValueError(f"required external node label missing: {identity}/{name}")
    for flow in document["flows"]:
        if document["domain"] == "progress" or flow["id"].startswith("stage.result."):
            if any(n.get("views", ["logic", "present"]) != ["logic"] for n in flow["nodes"]):
                raise ValueError("progress/result nodes must use logic view")
        for node in flow["nodes"]:
            if document["domain"] == "loot" and "logic" not in node.get("views", ["logic", "present"]):
                raise ValueError("loot nodes must include logic view")
            if document["domain"] == "scene" and flow["id"] == "scene.init.000":
                logic_ops = {"init_player", "init_templates", "end"}
                logic = "logic" in node.get("views", ["logic", "present"])
                if node["op"] in logic_ops and not logic:
                    raise ValueError("Game initialization nodes must include logic view")
                if logic and node["op"] not in logic_ops:
                    raise ValueError("unsupported Game initialization logic operation")
            variants = contract["ports"].get(node["op"], [])
            if sorted(node["on"]) not in variants:
                raise ValueError(f"{flow['id']}/{node['id']}: ports violate primitive contract")
            if node["op"] == "move" and node["args"][0] not in script_ids:
                raise ValueError("unknown movement script reference")
            if node["op"] == "opening_stage":
                stage = node["args"][0]
                if any(f"scene.{kind}.{stage:03d}" not in all_flows for kind in ("init", "opening")):
                    raise ValueError("opening stage references missing flows")
            if node["op"] in {"actor_hidden", "set_ai_target", "branch_any_hidden", "result_all_hidden"} and node["args"][0] > node["args"][1]:
                raise ValueError("invalid actor range")
        if flow["id"].startswith("stage.victory."):
            app_ops = {"preflight_before_music", "victory_music"}
            by_id = {n["id"]: n for n in flow["nodes"]}
            current, prefix = flow["entry"], set()
            while by_id[current]["op"] in app_ops:
                if current in prefix:
                    raise ValueError("cyclic App prefix")
                prefix.add(current)
                n = by_id[current]
                if n.get("views") != ["logic"] or set(n["on"]) != {"next"}:
                    raise ValueError("App prefix must be a linear logic declaration")
                current = n["on"]["next"]
            app_nodes = [n for n in flow["nodes"] if n["op"] in app_ops]
            if {n["id"] for n in app_nodes} != prefix or sum(n["op"] == "victory_music" for n in app_nodes) != 1 or sum(n["op"] == "preflight_before_music" for n in app_nodes) > 1:
                raise ValueError("victory flow must declare one music request in its App prefix")
        if flow["id"].startswith("stage.capability."):
            n = flow["nodes"][0]
            if len(flow["nodes"]) != 1 or n["op"] != "stage_capability" or n.get("views") != ["logic"] or n["on"]:
                raise ValueError("stage capability must be a single logic declaration")
            battle, minimum, exact, slots = n["args"]
            if exact and (exact < minimum or exact < slots):
                raise ValueError("stage capability unit bounds inconsistent")
            identity = flow["id"].rsplit(".", 1)[1]
            entry = all_flows.get("stage.entry." + identity)
            source_entry = all_flows.get("stage.source.entry." + identity)
            source_bound = entry is not None and entry is source_entry
            if identity != "000" and not slots and not source_bound:
                raise ValueError("source capability lacks an explicit entry binding")
            if source_bound:
                entry_node = next((node for node in entry["nodes"] if node["id"] == entry["entry"]), None)
                if slots or entry_node is None or entry_node["op"] != "call_init":
                    raise ValueError("source capability must reference metadata deployment via shared initialization")
            elif slots:
                setup = [n for n in entry["nodes"] if n["op"] == "stage_setup"] if entry else []
                if len(setup) != 1 or setup[0]["args"][1] != slots:
                    raise ValueError("capability/entry preparation slots inconsistent")
            if battle:
                victory = all_flows.get("stage.victory." + identity)
                if source_bound:
                    result = all_flows.get("stage.result." + identity)
                    effect_result = all_flows.get("stage." + identity + ".result")
                    if ((result is None) == (effect_result is None) or
                            (effect_result is not None and not effect_result["id"].startswith("common.result.source_")) or
                            victory is None or victory is not all_flows.get("stage.source.victory." + identity) or
                            "stage.victory.source.prefix" not in all_flows):
                        raise ValueError("source battle capability lacks unique result/victory/App prefix binding")
                elif any("stage." + kind + "." + identity not in all_flows for kind in ("result", "victory", "settlement")):
                    raise ValueError("battle capability lacks result/victory/settlement flows")
        if flow["id"].startswith("stage.result."):
            queries = {"branch_event_byte", "branch_turn", "branch_actor", "branch_hidden_count",
                       "branch_roster_text", "branch_item", "branch_item_occurrences"}
            terminals = {"result_return", "result_keep", "result_invalid"}
            if any((not n["op"].startswith("result_") and n["op"] not in queries) or
                   (not n["on"] and n["op"] not in terminals) for n in flow["nodes"]):
                raise ValueError("result flow requires result predicates and explicit result terminals")
            by_id = {n["id"]: n for n in flow["nodes"]}
            done, visiting = set(), set()
            def visit(identity):
                if identity in visiting:
                    raise ValueError("result flow cycle is not a valid query")
                if identity in done:
                    return
                visiting.add(identity)
                for target in by_id[identity]["on"].values():
                    visit(target)
                visiting.remove(identity)
                done.add(identity)
            visit(flow["entry"])


def generate(root: Path, output: Path, check=False):
    if output.resolve().is_relative_to((root / "original_game").resolve()) or output.resolve().is_relative_to((root / "data").resolve()):
        raise ValueError("output must not overwrite source definitions or original_game")
    manifest_path = root / "data/static/manifest.json"
    manifest_bytes = manifest_path.read_bytes()
    manifest = decode_json(manifest_bytes)
    inputs = {manifest_path: manifest_bytes}
    if (not isinstance(manifest, dict) or set(manifest) != {"schema_version", "table_contracts", "flow_contracts"} or
            type(manifest["schema_version"]) is not int or manifest["schema_version"] != SCHEMA_VERSION):
        raise ValueError("invalid static manifest")
    movement_file = "movement_scripts.json"
    table_contracts = manifest["table_contracts"]
    flow_contracts = manifest["flow_contracts"]
    tables_dir = root / "data/static/tables"
    flows_dir = root / "data/static/flows"
    if not isinstance(table_contracts, dict) or not table_contracts or movement_file in table_contracts:
        raise ValueError("invalid table contracts")
    table_files = set(table_contracts) | {movement_file}
    actual = {p.name for p in tables_dir.glob("*.json")}
    if actual != table_files:
        raise ValueError(f"table manifest coverage mismatch: missing={table_files-actual}, extra={actual-table_files}")
    actual_flows = {p.name for p in flows_dir.glob("*.json")}
    if not isinstance(flow_contracts, dict) or not flow_contracts or set(flow_contracts) != actual_flows:
        raise ValueError("flow manifest coverage mismatch")
    expected_outputs = {Path(name).stem + ".inc" for name in table_files}
    expected_outputs |= {Path(name).stem + "_flows.inc" for name in flow_contracts}
    extras = {path.name for path in output.glob("*.inc")} - expected_outputs
    if extras:
        raise ValueError(f"obsolete generated headers must be removed explicitly: {sorted(extras)}")
    outputs = {}
    tables = {}
    for name, contracts in sorted(table_contracts.items()):
        if not re.fullmatch(r"[a-z][a-z0-9_]*\.json", name):
            raise ValueError(f"unsafe table file: {name}")
        path = tables_dir / name
        raw = path.read_bytes()
        inputs[path] = raw
        records = validate_document(decode_json(raw))["tables"]
        if not isinstance(contracts, dict) or set(contracts) != {t["name"] for t in records}:
            raise ValueError(f"{name}: ABI table coverage differs from manifest")
        for table in records:
            symbol = table["name"]
            shape = {"c_type": table["c_type"], "dimensions": table["dimensions"]}
            if json.dumps(contracts[symbol], sort_keys=True) != json.dumps(shape, sort_keys=True):
                raise ValueError(f"{name}/{symbol}: consumer ABI contract mismatch")
            if symbol in tables:
                raise ValueError(f"duplicate global table name: {symbol}")
            tables[symbol] = table["values"]
        outputs[Path(name).stem + ".inc"] = render_tables(records)
    validate_references(tables)
    aliases = constructor_alias_records(tables)
    outputs["unit_base.inc"] += b"\n" + render_tables([{
        "name": "k_player_base_alias_records", "c_type": "uint8_t",
        "dimensions": [len(aliases), 24], "values": aliases}])
    outputs["preparation.inc"] += b"\n" + render_tables(preparation_views(tables))
    path = tables_dir / movement_file
    inputs[path] = path.read_bytes()
    scripts = read_movement_scripts(decode_json(inputs[path]))
    outputs["movement_scripts.inc"] = render_movement_scripts(scripts)
    global_ids = set()
    flow_documents = {}
    for name in sorted(flow_contracts):
        if not re.fullmatch(r"[a-z][a-z0-9_]*\.json", name):
            raise ValueError("invalid flow file name")
        path = flows_dir / name
        raw = path.read_bytes()
        inputs[path] = raw
        document = validate_flows(decode_json(raw))
        if document["domain"] != path.stem:
            raise ValueError("flow domain/file mismatch")
        flow_documents[name] = document
        for identity in flow_index(document):
            if identity in global_ids:
                raise ValueError("duplicate global flow ID")
            global_ids.add(identity)
        outputs[path.stem + "_flows.inc"] = render_flows(document)
    all_flows = {identity: flow for d in flow_documents.values() for identity, flow in flow_index(d).items()}
    for name, document in flow_documents.items():
        validate_flow_contract(document, flow_contracts[name], scripts.keys(), all_flows)
    # 全部输入验证成功后才更新任何输出，坏数据不能留下半套新定义。
    if any(path.read_bytes() != data for path, data in inputs.items()):
        raise ValueError("static inputs changed during generation")
    for name, data in outputs.items():
        path = output / name
        if path.exists() and path.read_bytes() == data:
            continue
        if check:
            raise ValueError(f"generated file stale/missing: {path}")
        output.mkdir(parents=True, exist_ok=True)
        temporary = path.with_suffix(path.suffix + ".tmp")
        temporary.write_bytes(data)
        temporary.replace(path)
    return len(tables) + len(scripts)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    try:
        count = generate(args.root, args.output, args.check)
    except (ValueError, OSError, TypeError) as error:
        parser.exit(1, f"static data: {error}\n")
    print(f"static data: {count} validated data arrays")


if __name__ == "__main__":
    main()
