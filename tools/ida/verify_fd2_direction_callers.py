"""134e4完整函数、完整xref闭集与当前有限冷图的独立来源校验。

只使用既有Domain指令头/函数chunk读集和只读原EXE，不启动IDA或窗口。
23是20个parent中的静态callsite数，不是任意一次运行的调用次数。
不以当前SDL实现或JSON反推原语义，不声称完成CPU/呈现/时钟实测。
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

STATIC_DATA = Path(__file__).resolve().parents[1] / "static_data"
sys.path.insert(0, str(STATIC_DATA))
from verify_unit_tables import source_range

EXE_SHA = "bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd"
RESET_SHA = "dc6f3ac1946072a996c259f094f6bad5da08d74223ad1b35fa150204a655578a"
READSETS = {
    "build/g1/ida/runtime/domain-evidence.json": "b4594f91b31b957c962f1bf9bcea1a2e805b26f1b5bc11414f52781c070c3097",
    "build/g1/ida/preparation/scope-callee-closure.json": "67ecf2532928a0359c63e5770eeace5d5feecf56098d2fe4d72b2dc1b5fc2c75",
    "build/g1/ida/preparation/scope-root-readset.json": "7f5beff4f8c8eb55c95b142608de8c82d1cccda6702d6fbe5957b3066fafd62a",
    "build/g2/runtime-continuation-domain.json": "a3845664b68aa28bd0e7133242c2921bcfd431d994d04815c7fb0a835f0fdc86",
    "build/g5/direction-callers-domain.json": "befab483c5cb135aa86a21592d8e440d743d79e4e8e3664a1fc3c0a99b77b848",
}

# 已有Domain完整函数chunk与原raw-unrelocated字节。共享尾不是新函数；
# stage2的3313b只能从32e8c的真实tail chunk归属取得，不能只看primary range。
CALLERS = {
    0x13a9f: ("66d532cfd6a1914686cd63ff05e44e250b359d71c54144c71c1a6648b3679779", (0x13e88,)),
    0x15055: ("9efd8ffa124f39665035803b4313e224b54e5b69ae30bfdd03c9caaebc334674", (0x152f8,)),
    0x1548e: ("e8294fe178f1dc6c83f2468a876881b397e13cb99051a02b1c3ad4c5423fca97", (0x1559c, 0x15627)),
    0x16f55: ("6c92d5e648c40d98caff228d45b1c9c5c5acd82f5d6a4ac24cb4a071c24ec5bd", (0x17137,)),
    0x18890: ("fc66a590290645071d85ce76a3e4f7bb9dfe118878d3114fdb94f501ad42aab3", (0x18a5a,)),
    0x18d8c: ("de3399668bd11727fa0867ef09d299acfb7898a91c34d012f4190630da67e55e", (0x18fce,)),
    0x3231b: ("d3077563157b8422e265ef3e2f8b523fc125d2cd5900e06f6e35bff99fa63143", (0x3295a,)),
    0x32d18: ("7aa0e73a9b98cb9227df0754f371bb9b2f97c351367cc39bbe9713897d456f5a", (0x32e36, 0x32e69)),
    0x32e8c: ("db67f9c05fd0ccdc370a4e218ff48be6a319d715ab2709aa0299e483c7d5d515", (0x3313b,)),
    0x3327d: ("b9da0fc8a79389c6959e8db77eb1de643e9c2bc4d511adebb7cec53f5d1ea6db", (0x33324,)),
    0x333f5: ("2722015c0f98023ede05b3a2ef6e9f68cc57f5c34290003666a7fcd414aa91b2", (0x33448,)),
    0x336a0: ("f492fe4eb7b7905f81584eddfa9a423729a39b4134777dde592fedb61a492a7c", (0x338ba,)),
    0x33c9d: ("aac45e32bf1601a817346f36855653a47cebabcf4c254cf33523b6ae383f0eb1", (0x33d59,)),
    0x34531: ("40fe6618481b2bde268a4f1b776cebdcaf937fd80436de5c5d52c1864573c64a", (0x34604,)),
    0x3460b: ("6e210c27d6cf8973d6a49850a889bf16d1dcfb028e57c0e121c74938b3b12196", (0x34645,)),
    0x34673: ("4e854f5e0220b54bed858c95917bc3eac128dc4c39b6fc2c4ec206c7303792b9", (0x346ad,)),
    0x346cd: ("112da54826e81a2ac35f973effacf4128dd9a793c9c9c1b6d364ce1606bb691b", (0x34710,)),
    0x34984: ("b1adacaf18958b39cd35ca9ceccd3ace4f710dada2c839e8aa2dd5d0821dfe8d", (0x349c6,)),
    0x35022: ("c9be5ca6fb0bedce0a9bae1d98af554f365ef4d3734e8d2fc91f636b876dc7d6", (0x35081, 0x350b4)),
    0x350c8: ("2e1cc58fb6f7a88b22e994a579073ed772f6d468212174cbba345b76b9cfdbfa", (0x35100,)),
}
NO_RESET = {
    0x2e2b0: "a5722c91d28c4b886d23c97dd92620f059bc0e3d2d34be36f99d0d4f147f9380",
    0x34738: "7a3c46013acd8d979e1e317d7f3d49d6b2a4dbeb3fba03a43334f1a9ed37a42e",
    0x34778: "ee8937455de514693282efd0229699b82546e103d2c4b6143744e482dcf2c35d",
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def direct_target(image, address):
    raw = image[address:address + 5]
    return address + 5 + struct.unpack_from("<i", raw, 1)[0] if len(raw) == 5 and raw[0] == 0xe8 else None


def verify_source(exe, documents):
    if sha(exe) != EXE_SHA:
        raise ValueError("方向caller原EXE身份不同")
    image = bytes(0x10000) + source_range(exe, 0x10000, 0x3ef29)
    if sha(image[0x134e4:0x13512]) != RESET_SHA:
        raise ValueError("134e4完整46B不同")
    if image[0x13507:0x13509] != bytes.fromhex("6a14") or direct_target(image, 0x13509) != 0x3790a:
        raise ValueError("134e4尾等待不是20ms")
    functions = {}
    shared = set()
    for document in documents:
        for function in document["functions"]:
            functions[function["va"]] = function
        for tail in document.get("shared_tail_instructions", []):
            shared.update(row["ea"] for row in tail["instructions"])
    reset = functions.get(0x134e4)
    if not reset or set(reset["callers"]) != set(CALLERS):
        raise ValueError("134e4完整parent xref闭集不同")
    records = []
    calls_by_function = {}
    for address, expected_sha in list((va, data[0]) for va, data in CALLERS.items()) + list(NO_RESET.items()):
        function = functions.get(address)
        if not function:
            raise ValueError(f"缺少完整Domain函数 {address:#x}")
        chunks = function["chunks"]
        raw = b"".join(image[c["start"]:c["end"]] for c in chunks)
        if sha(raw) != expected_sha:
            raise ValueError(f"方向caller完整chunk不同 {address:#x}")
        heads = {row.get("ea", row.get("va")) for row in function["instructions"]}
        heads.update(ea for ea in shared if any(c["start"] <= ea < c["end"] for c in chunks))
        calls = sorted((ea, direct_target(image, ea)) for ea in heads if direct_target(image, ea) is not None)
        resets = tuple(ea for ea, target in calls if target == 0x134e4)
        if resets != (CALLERS[address][1] if address in CALLERS else ()):
            raise ValueError(f"方向reset静态callsite不同 {address:#x}")
        calls_by_function[address] = calls
        records.append({"parent": hex(address), "chunks": [{"start": hex(c["start"]), "end": hex(c["end"])} for c in chunks],
                        "raw_sha256": expected_sha, "reset_calls": [hex(ea) for ea in resets]})
    # 完整函数摘要以外的显式语义锚：SHORT分支绕过FULL后共享第二reset；
    # AI Item只有reset20后清53EC8，不能套13a9f的额外refresh入口。
    for at, target in ((0x13e80, 0x13512), (0x13e8f, 0x11cac), (0x152f0, 0x20c6f),
                       (0x1561f, 0x2e2b0), (0x1562f, 0x1b6b7),
                       (0x18fc6, 0x2e2b0), (0x18fd6, 0x1b6b7)):
        if direct_target(image, at) != target:
            raise ValueError(f"方向caller顺序锚不同 {at:#x}")
    if image[0x13e8d:0x13e8f] != bytes.fromhex("6a00") or image[0x15616:0x15618] != bytes.fromhex("eb0f"):
        raise ValueError("SHORT/FULL共尾或refresh0参数不同")
    if any(at > 0x152f8 and target == 0x11cac for at, target in calls_by_function[0x15055]):
        raise ValueError("AI Item reset后出现额外refresh")
    if image[0x18a38:0x18a3a] != bytes.fromhex("85ff") or image[0x18a3a:0x18a40] != bytes.fromhex("0f84e4000000"):
        raise ValueError("移动零path跳过reset的原分支不同")
    return {"function": {"va": "0x134e4", "end": "0x13512", "raw_sha256": RESET_SHA, "delay_ms": image[0x13508]},
            "parents": 20, "static_reset_calls": sum(len(row[1]) for row in CALLERS.values()),
            "complete_functions": records,
            "source_paths": {"physical_short": 2, "physical_full_caller": 1, "physical_full_body": 0,
                             "AI_physical_before_collect": 1, "AI_item_tail_refresh": False,
                             "stage_entry_001": 2, "stage_entry_002": 1,
                             "common.legacy.action.000": 1, "common.legacy.action.001": 1,
                             "common.legacy.action.002": 1, "common.legacy.action.003": 1,
                             "common.legacy.action.004": 0, "common.legacy.action.006": 0}}


def flow(document, identity):
    found = [f for f in document["flows"] if f["id"] == identity]
    if len(found) != 1:
        raise ValueError("缺少唯一冷图 " + identity)
    return found[0]


def traces(definition, entry=None):
    """仅列有限定义的成功边；原版源常量由verify_source独立取得。"""
    nodes = {node["id"]: node for node in definition["nodes"]}
    if len(nodes) != len(definition["nodes"]):
        raise ValueError("冷图节点重复")
    pending = [(entry or definition["entry"], [], set())]
    result = []
    while pending:
        pc, path, seen = pending.pop()
        if pc not in nodes or pc in seen or len(path) >= 256 or len(pending) > 256:
            raise ValueError("冷图成功路径缺口/循环")
        node = nodes[pc]
        path = path + [[node["op"], node["args"]]]
        edges = [value for port, value in node["on"].items() if port not in ("error", "cancel")]
        if not edges:
            result.append(path)
        for target in edges:
            pending.append((target, path, seen | {pc}))
    return result


def reset_pairs(definition, operation, wait_operation, count, delay):
    nodes = {node["id"]: node for node in definition["nodes"]}
    resets = [node for node in definition["nodes"] if node["op"] == operation]
    if len(resets) != count:
        raise ValueError("冷图reset数量不符原caller " + definition["id"])
    for node in resets:
        following = nodes.get(node["on"].get("next"))
        if node["args"] or not following or following["op"] != wait_operation or following["args"] != [delay]:
            raise ValueError("冷图reset后必须立即原20ms " + definition["id"])
    for path in traces(definition):
        if sum(op == operation for op, args in path) != count:
            raise ValueError("冷图成功路径绕过原reset " + definition["id"])


def compare_flows(documents, original):
    delay = original["function"]["delay_ms"]
    runtime = flow(documents["runtime"], "turncycle")
    expected = [["turn_reset_directions", []], ["turn_delay_ms", [delay]]]
    for entry, refresh in (("directions_reset", False), ("directions_reset_refresh", True)):
        paths = traces(runtime, runtime.get("entries", {}).get(entry, "missing"))
        wanted = expected + ([["turn_refresh", []]] if refresh else []) + [["turn_done", []]]
        if paths != [wanted]:
            raise ValueError("runtime reset入口与原caller顺序不同 " + entry)
    full = flow(documents["progress"], "physical.full")
    if any(n["op"] in ("action_reset_directions", "action_delay_ms") for n in full["nodes"]):
        raise ValueError("裸physical.full不能重复caller reset")
    caller = flow(documents["progress"], "physical.caller.full")
    if traces(caller) != [[["action_call_full", []], ["action_reset_directions", []],
                           ["action_delay_ms", [delay]], ["action_done", []]]]:
        raise ValueError("FULL caller reset/20ms顺序不同")
    short = flow(documents["progress"], "physical.short")
    reset_pairs(short, "action_reset_directions", "action_delay_ms", original["source_paths"]["physical_short"], delay)
    for path in traces(short):
        projection = [row for row in path if row[0] in ("action_reset_directions", "action_delay_ms", "action_sequence")]
        wanted = [["action_reset_directions", []], ["action_delay_ms", [delay]], ["action_sequence", [0]]]
        if any(row == ["action_sequence", [1]] for row in projection):
            wanted.append(["action_sequence", [1]])
        wanted += [["action_reset_directions", []], ["action_delay_ms", [delay]]]
        if projection != wanted:
            raise ValueError("SHORT reset/攻击顺序不同")
    finish = flow(documents["loot"], "action.finish")
    if any(n["op"] in ("action_reset_directions", "directions_reset", "reset_directions", "turn_reset_directions", "action_delay_ms") for n in finish["nodes"]):
        raise ValueError("action.finish重复原caller reset/等待")
    for domain, identity, operation, wait, count in (
        ("scene", "scene.opening.000", "reset_directions", "sleep_ms", 1),
        ("stage", "stage.entry.001", "reset_directions", "sleep_ms", original["source_paths"]["stage_entry_001"]),
        ("stage", "stage.entry.002", "reset_directions", "sleep_ms", original["source_paths"]["stage_entry_002"])):
        reset_pairs(flow(documents[domain], identity), operation, wait, count, delay)
    for identity, count in original["source_paths"].items():
        if identity.startswith("common.legacy."):
            reset_pairs(flow(documents["common_event"], identity), "directions_reset", "delay_ms", count, delay)
    return {"runtime_entries": 2, "progress_flows": 3, "finish_no_duplicate": True,
            "scene_stage_flows": 3, "common_bindings": 6}


def verify(root, exe):
    documents = []
    for filename, expected in READSETS.items():
        raw = (root / filename).read_bytes()
        if sha(raw) != expected:
            raise ValueError("Domain来源读集身份不同 " + filename)
        documents.append(json.loads(raw))
    original = verify_source(exe, documents)
    cold = {name: json.loads((root / "data/static/flows" / (name + ".json")).read_bytes())
            for name in ("runtime", "progress", "loot", "scene", "stage", "common_event")}
    comparison = compare_flows(cold, original)
    return {"schema": "fd2.g5.direction-callers-source.v1", "ok": True, "exe_sha256": EXE_SHA,
            "readsets": READSETS, "original": original, "cold_comparison": comparison,
            "scope": "20个直接parent/23静态callsite完整原chunk与xref；已支持冷caller的reset/20ms顺序。7个后期私有parent仅保来源，不开放关卡。C adapter接线、完整运行分支、像素/PCM/时钟实测另验。",
            "CPU_execution_claimed": False, "native_execution_claimed": False,
            "new_GUI_started": False, "database_modified": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument("--exe", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = verify(args.root, (args.exe or args.root / "original_game/FD2.EXE").read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"ok": True, "parents": 20, "static_reset_calls": result["original"]["static_reset_calls"],
                      "output": str(args.output)}, ensure_ascii=False))


if __name__ == "__main__":
    main()
