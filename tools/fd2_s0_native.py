"""S0受控结果夹具与只读owner观察，不修改生产代码或原始资源。

依据完整205b4/206c5的hidden/side谓词、10010活动恢复和117e7→1197b
后置查询；复用既有RPC原码锚/fixup与CDB/PDB owned读取，不扫描单位内容。
"""
from __future__ import annotations

import hashlib
import json
import re
import struct
from pathlib import Path

import fd2_cdb_prepare_snapshot as cdb_api
import fd2_dosbox_snapshot as rpc_api
from rebuild_fd2_analysis import parse_le, parse_objects, parse_fixups
from run_g4_runtime_check import encode
from run_graphics_tests import decode_save
from run_original_end_turn import META, UNITS, STRIDE
from static_data.verify_unit_tables import EXE_SHA256

SCENARIOS = ("leader-hp0-visible", "hostile-hp0-visible", "hidden-defeat-priority", "hidden-victory",
             "stage1-villager-priority", "active-load-hidden-defeat")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def original_result(plain):
    """仅正常stage0/1源域；不模拟空表/缺raw0/村民越界等原版未定义读取。"""
    count, stage = plain[META + 1:META + 3]
    if stage not in (0, 1) or not 1 <= count <= 96 or stage == 1 and count < 11:
        raise ValueError("S0结果oracle要求完整stage0/1正常源域")
    units = [plain[UNITS + raw * STRIDE:UNITS + (raw + 1) * STRIDE] for raw in range(count)]
    if any(len(row) != STRIDE for row in units):
        raise ValueError("活动记录截断")
    result = 0 if any(row[6] == 0 and not row[5] & 1 for row in units) else 1
    if units[0][5] & 1 or stage == 1 and all(row[5] & 1 for row in units[5:11]):
        result = 2
    return result


def derive(source, scenario):
    if scenario not in SCENARIOS:
        raise ValueError("未知S0场景")
    if len(source) != 0x59CB or sum(source[:-4]) != int.from_bytes(source[-4:], "little"):
        raise ValueError("来源容器长度或checksum不同")
    stage = 1 if scenario == "stage1-villager-priority" else 0
    if source[META + 2] != stage or not 11 <= source[META + 1] <= 96:
        raise ValueError("S0夹具stage或active数量不符")
    plain = bytearray(source)
    allowed = set()

    def put(raw, field, payload):
        at = UNITS + raw * STRIDE + field
        plain[at:at + len(payload)] = payload
        allowed.update(range(at, at + len(payload)))

    if scenario == "leader-hp0-visible":
        put(0, 5, bytes((source[UNITS + 5] & ~1,)))
        put(0, 0x40, b"\0\0")
    elif scenario == "hostile-hp0-visible":
        for raw in range(source[META + 1]):
            if source[UNITS + raw * STRIDE + 6] == 0:
                put(raw, 5, bytes((source[UNITS + raw * STRIDE + 5] & ~1,)))
                put(raw, 0x40, b"\0\0")
    else:
        for raw in range(source[META + 1]):
            if source[UNITS + raw * STRIDE + 6] == 0:
                put(raw, 5, bytes((source[UNITS + raw * STRIDE + 5] | 1,)))
        if stage == 1:
            for raw in range(5, 11):
                put(raw, 5, bytes((source[UNITS + raw * STRIDE + 5] | 1,)))
        elif scenario != "hidden-victory":
            put(0, 5, bytes((source[UNITS + 5] | 1,)))
    expected = original_result(plain)
    if expected != (0 if scenario.endswith("hp0-visible") else 1 if scenario == "hidden-victory" else 2):
        raise ValueError("受控初态没有构成声明的原义结果")
    plain[-4:] = sum(plain[:-4]).to_bytes(4, "little")
    changes = [{"offset": at, "before": a, "after": b} for at, (a, b) in enumerate(zip(source, plain)) if a != b]
    if any(row["offset"] not in allowed and row["offset"] < len(source) - 4 for row in changes):
        raise AssertionError("夹具越出声明的HP/hidden写集")
    if scenario == "leader-hp0-visible":
        # 原117e7按flags/side进入controller；HP0可见主角必须实际选中验证。
        actor = 0 if plain[UNITS + 6] == 2 and not plain[UNITS + 5] & 0x85 else None
    else:
        actor = next((raw for raw in range(1, source[META + 1])
                      if plain[UNITS + raw * STRIDE + 6] == 2 and not plain[UNITS + raw * STRIDE + 5] & 0x85
                      and int.from_bytes(plain[UNITS + raw * STRIDE + 0x40:UNITS + raw * STRIDE + 0x42], "little") > 0), None)
    if actor is None:
        raise ValueError("夹具没有符合声明flags/side的controller玩家")
    return bytes(plain), encode(plain), {"scenario": scenario, "synthetic_controlled_state": True,
        "natural_state_claimed": False, "source_plain_sha256": digest(source), "derived_plain_sha256": digest(plain),
        "changed_plain_bytes": changes, "expected_post_controller_result": expected,
        "controller_actor": actor, "result_before_controller_query_expected": 0,
        "prior_snapshot_result_not_serialized": True}


def check_active(actual, plain, *, expected_cached_result=0):
    expected = []
    for raw in range(plain[META + 1]):
        row = plain[UNITS + raw * STRIDE:UNITS + (raw + 1) * STRIDE]
        expected.append({"raw": raw, "flags": row[5], "side": row[6], "unit_id": row[7],
                         "hp": int.from_bytes(row[0x40:0x42], "little")})
    return {"ok": actual["stage"] == plain[META + 2] and actual["count"] == len(expected)
            and actual["actors"] == expected and actual["cached_result"] == expected_cached_result,
            "expected_cached_result": expected_cached_result, "actual": actual,
            "expected_actors": expected, "scope": "all active flags/side/unit-id/HP, stage/count and cached result; not all runtime/RNG"}


def active_fields(data, count):
    if len(data) != count * STRIDE or not 1 <= count <= 96:
        raise ValueError("active owner数量或完整长度不符")
    return [{"raw": raw, "flags": data[raw * STRIDE + 5], "side": data[raw * STRIDE + 6],
             "unit_id": data[raw * STRIDE + 7], "hp": struct.unpack_from("<H", data, raw * STRIDE + 0x40)[0]}
            for raw in range(count)]


def original_active(memory, original, descriptor):
    executable = (Path(original) / "FD2.EXE").read_bytes()
    if digest(executable) != EXE_SHA256 or len(memory) != rpc_api.RAM_SIZE:
        raise ValueError("原EXE身份或RPC单次完整RAM长度不同")
    le = parse_le(executable)
    fixups = parse_fixups(executable, le, parse_objects(executable, le))
    anchors = rpc_api._anchors(executable, fixups)
    matches = [rpc_api.locate(memory, anchor) for anchor in anchors]
    bases = [at - (anchor["cva"] - 0x10000) for at, anchor in zip(matches, anchors)]
    if len(set(bases)) != 1 or bases[0] < 0:
        raise ValueError("两原码锚未定位同一object1")
    code = bases[0]
    proofs = []
    for fixup in fixups:
        if fixup.target_object == 2 and any(lo <= fixup.source_addr_relbase < hi for lo, hi in
                ((0x26152, 0x265EC), (0x2B777, 0x2B843), (0x2968D, 0x2986F))):
            value = struct.unpack_from("<I", memory, code + fixup.source_addr_relbase - 0x10000)[0]
            proofs.append({"source_cva": hex(fixup.source_addr_relbase), "target_offset": fixup.target_offset,
                           "base_DS_offset": value - fixup.target_offset})
    if len(proofs) < 2 or len({row["base_DS_offset"] for row in proofs}) != 1:
        raise ValueError("本轮object2合法fixup基址不一致")
    data = proofs[0]["base_DS_offset"]

    def u32(offset):
        if not 0 <= data + offset <= len(memory) - 4:
            raise ValueError("原活动owner字段越界")
        return struct.unpack_from("<I", memory, data + offset)[0]

    pointer, count, stage, result = (u32(offset) for offset in (0x3A45, 0x3BEB, 0x3C03, 0x3ECC))
    if stage not in (0, 1) or not 1 <= count <= 96 or pointer <= 0 or pointer + count * STRIDE > len(memory):
        raise ValueError("原活动owner不在S0正常源域")
    state = {"stage": stage, "count": count, "cached_result": result,
             "actors": active_fields(memory[pointer:pointer + count * STRIDE], count)}
    return state, {"object1_DS_offset": code, "object2_DS_offset": data,
                   "object2_linear_base": descriptor["base"] + data, "fixup_proofs": proofs,
                   "active_owner_cva": "0x53a45", "count_cva": "0x53beb", "result_cva": "0x53ecc",
                   "active_pointer_DS_offset": pointer, "actor_content_scan_used": False}


def snapshot_retry_reason(requests, error):
    """只识别dump后guest暂离PM；真实descriptor/owner/短读错误仍拒绝。"""
    if not isinstance(error, ValueError) or str(error) != "RPC前后protected descriptor改变；不拼接不同视图" or len(requests) != 6:
        return None
    ev = "EXEC EV " + " ".join(rpc_api.REGISTERS)
    if ([row.get("command") for row in requests[:3]] != [ev, "EXEC SELINFO CS", "EXEC SELINFO DS"]
            or not requests[3].get("command", "").startswith("EXEC MEMDUMPBIN ")
            or requests[4].get("command") != ev or any(row.get("ok") is not True for row in requests)):
        return None
    try:
        before = rpc_api.parse_registers(requests[0]["response"])
        after = rpc_api.parse_registers(requests[4]["response"])
        ds = rpc_api.parse_selector(requests[2]["response"])
        ds_after = rpc_api.parse_selector(requests[5]["response"])
    except (KeyError, TypeError, ValueError):
        return None
    if (requests[5].get("command") != f'EXEC SELINFO {before["DS"]:X}'
            or not before["CR0"] & 1 or before["EFLAGS"] & (1 << 17) or not before["DS"]
            or any(ds[key] != ds_after[key] for key in ("base", "limit", "type", "dpl", "flags"))
            or after["CR0"] & 1 and not after["EFLAGS"] & (1 << 17)):
        return None
    return {"reason": "guest_left_protected_mode_after_dump", "registers_before": before, "registers_after": after,
            "unchanged_descriptor": {key: ds[key] for key in ("base", "limit", "type", "dpl", "flags")}}


def wrapped_address_retry_reason(memory, error):
    """GetAddress的16bit wrap有精确64KiB周期；不将普通owner错误当瞬态。"""
    if (not isinstance(error, ValueError) or not str(error).startswith("原码anchor在本轮snapshot没有唯一match：")
            or not isinstance(memory, bytes) or len(memory) != rpc_api.RAM_SIZE or not any(memory)):
        return None
    period = 65536
    if memory != memory[:period] * (len(memory) // period):
        return None
    return {"reason": "exact_16bit_segment_offset_wrap", "period_bytes": period,
            "whole_dump_bytes": len(memory), "identical_periods": len(memory) // period,
            "nonzero_payload": True, "bridge_snapshot_passed": True, "owner_anchor_validated": False,
            "source_basis": "DOSBox-X2026.10.01 debug.cpp Is16BitSegment/GetAddress/SaveMemoryBin; offsets wrap before seg<<4"}


def original_snapshot(rpc, work, label, record, *, owner_decoder=None):
    """最多3个完整独立attempt；生产调用须在同attempt内通过owner/状态验证。"""
    attempts = record.setdefault("rpc_snapshot_attempts", [])
    record["rpc_snapshot_attempt_limit"] = 3
    for index in range(1, 4):
        directory = Path(work) / f"attempt-{index:02d}"
        directory.mkdir()
        start = len(rpc.record["requests"])
        attempt = {"attempt": index, "directory": str(directory), "accepted": False}
        attempts.append(attempt)
        receipt = None
        memory = None
        try:
            memory, receipt = rpc.snapshot(directory, label)
            if owner_decoder is not None:
                attempt["owner_validation_attempted"] = True
                decoded_owner = owner_decoder(memory, receipt)
        except BaseException as error:
            requests = rpc.record["requests"][start:]
            raw = json.dumps(requests, ensure_ascii=False, indent=2).encode()
            path = directory / "rpc-requests.json"
            path.write_bytes(raw)
            attempt.update(error={"type": type(error).__name__, "message": str(error)},
                           requests={"path": str(path), "sha256": digest(raw), "count": len(requests)})
            dump = directory / (label + "-ram.bin")
            if dump.is_file():
                attempt["discarded_dump"] = {"path": str(dump), "size": dump.stat().st_size,
                                              "sha256": digest(dump.read_bytes()), "decoded_or_reused": False}
            if receipt is not None:
                attempt["bridge_snapshot"] = receipt
                reason = wrapped_address_retry_reason(memory, error)
            else:
                reason = snapshot_retry_reason(requests, error)
            attempt["retry_classification"] = reason
            attempt["retry_scheduled"] = reason is not None and index < 3
            (directory / "attempt.json").write_text(json.dumps(attempt, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
            if not attempt["retry_scheduled"]:
                raise
        else:
            requests = rpc.record["requests"][start:]
            raw = json.dumps(requests, ensure_ascii=False, indent=2).encode()
            path = directory / "rpc-requests.json"
            path.write_bytes(raw)
            attempt.update(accepted=True, snapshot=receipt, owner_validated=owner_decoder is not None,
                           requests={"path": str(path), "sha256": digest(raw), "count": len(requests)})
            (directory / "attempt.json").write_text(json.dumps(attempt, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
            if owner_decoder is not None:
                record["validated_owner"] = decoded_owner
            return memory, receipt
    raise AssertionError("快照attempt预算耗尽后不得继续")


def battle_frame(lines, pid, module):
    current = None
    matches = []
    for line in lines:
        banner = re.search(r"^\s*[.#]?\s*[0-9]+\s+Id:\s*([0-9a-f]+)\.([0-9a-f]+)\b", line, re.I)
        if banner:
            if int(banner[1], 16) != pid:
                raise ValueError("CDB栈线程不是本轮owned PID")
            current = int(banner[2], 16)
        frame = re.search(r"^\s*([0-9a-f]+)\s+[0-9a-f`]+\s+[0-9a-f`]+\s+" + re.escape(module)
                          + r"!fd2_field_play_run_context(?:\+\S+)?(?:\s|$)", line, re.I)
        if frame:
            if current is None:
                raise ValueError("battle栈帧缺少线程归属")
            matches.append({"thread_id": current, "frame": int(frame[1], 16), "line": line})
    if len(matches) != 1:
        raise ValueError("需要唯一fd2_field_play_run_context栈帧，不猜RSP或扫描game")
    return matches[0]


def sdl_active(cdb, target, exe, pdb, work, record):
    work = Path(work)
    record["symbols"] = cdb_api.symbols(exe, pdb, Path(cdb).parent / "dbghelp.dll")
    with cdb_api.CDB(cdb, target, work, record) as debugger:
        debugger.request(".sympath " + str(Path(pdb).resolve().parent))
        debugger.request(".reload /f " + Path(exe).name)
        frame = battle_frame(debugger.request("~*kn 0n128"), target.pid, Path(exe).stem)
        debugger.request(f"~~[{frame['thread_id']:x}]s")
        debugger.request(f".frame /r {frame['frame']:x}")
        local = debugger.request("dv /t /v game")
        if not any("game" in line and "fd2_field_game" in line for line in local):
            raise ValueError("CDB没有本轮栈帧game真实类型")

        def value(expression):
            return cdb_api.cpp_integer(debugger.request("?? " + expression))

        address, size = value("&game"), value("sizeof(game)")
        if not address or not 7680 <= size <= 1024 * 1024:
            raise ValueError("PDB game地址或sizeof越界")
        offsets = {name: value("(char *)&game." + name + " - (char *)&game")
                   for name in ("stage", "units.count", "units.items", "battle_result")}
        widths = {name: value("sizeof(game." + name + ")") for name in ("stage", "units.count", "battle_result")}
        data, read = cdb_api.read_owned_memory(target, address, size, exe)

        def field(name):
            at, width = offsets[name], widths[name]
            if width not in (4, 8) or not 0 <= at <= len(data) - width:
                raise ValueError("PDB game标量布局越界")
            return int.from_bytes(data[at:at + width], "little")

        stage, count, result = field("stage"), field("units.count"), field("battle_result")
        at = offsets["units.items"]
        if stage not in (0, 1) or not 1 <= count <= 96 or not 0 <= at <= len(data) - count * STRIDE:
            raise ValueError("PDB活动owner不在S0正常源域")
        state = {"stage": stage, "count": count, "cached_result": result,
                 "actors": active_fields(data[at:at + count * STRIDE], count)}
        path = work / "sdl-game.bin"
        if path.exists():
            raise ValueError("CDB快照输出已存在")
        path.write_bytes(data)
        record.update(frame=frame, layout={"address": address, "size": size, "offsets": offsets, "widths": widths},
                      memory_read=read, snapshot={"path": str(path), "sha256": digest(data), "size": size},
                      one_stopped_attach=True, one_ReadProcessMemory=True)
    if digest(Path(exe).read_bytes()) != record["symbols"]["exe_sha256"] or digest(Path(pdb).read_bytes()) != record["symbols"]["pdb_sha256"]:
        raise ValueError("读取期间App/PDB身份改变")
    return state, record
