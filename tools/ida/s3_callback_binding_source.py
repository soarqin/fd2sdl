"""S3新增绑定的五个真实callback入口/共享尾到实际有限C差分。

复用已完整Domain读集，逐chunk/指令核严格镜像；原x86独立产生期望。
1aa1d的呈现/选择替身明确满包Cancel；库存业务另与真实Loot owner比对。
不修改IDA/原资源，不据共享尾函数名替换真实调用栈参数。
"""
from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"tools/static_data"))
import convert_stage_callbacks as converter
import audit_stage_data as audit
import stage_source_ledger as ledger_tool

SLOTS=(8,12,26,39,41)


def cases(metadata,slot):
    base=converter.mechanism_cases(metadata)[0]
    result=[]
    for caller in (0,1):
        for side in (0,1,2):
            for items in (0,7,8):
                for once in (0,1):
                    case=copy.deepcopy(base);case.update(caller=caller,count=80,
                        id=f"caller{caller}-side{side}-items{items}-state{once}")
                    case["state"][16]=once
                    for raw in range(80):case["units"][raw*80+6]=2
                    case["units"][caller*80+6]=side
                    for raw in (0,caller):
                        for i in range(8):
                            at=raw*80+10+i*2;case["units"][at]=0 if i<items else 128
                            case["units"][at+1]=(i+1) if i<items else 0xa5
                    result.append(case)
    return result


def verify_c(probe,original,rows):
    requests=[(row,0,0) for row in rows]
    for slot in SLOTS:
        row=next((r for r in rows if r["slot"]==slot and r["source_checkpoints"]),None)
        if row:
            cut=len(row["source_checkpoints"])-1
            requests.extend((row,mode,cut) for mode in (1,2,3))
    wire=[]
    for index,(row,mode,cut) in enumerate(requests):
        c=row["input"]
        wire.append(" ".join(map(str,(index,row["definition"],row["stage"],c["count"],c["caller"],
            c["turn"],c["display"],c["contribution"],mode,cut)))+" "+bytes(c["units"]).hex()+" "+
            bytes(c["state"]).hex()+" "+bytes(c["metadata"][3:51]).hex())
    probe=Path(probe).resolve();before=converter.digest(probe.read_bytes())
    p=subprocess.run([str(probe),str((Path(original)/"FDFIELD.DAT").resolve())],input="\n".join(wire)+"\n",
                     capture_output=True,text=True,timeout=180)
    if p.returncode:raise ValueError(f"actualC probe exited{p.returncode}: {p.stderr[-1000:]}")
    if converter.digest(probe.read_bytes())!=before:raise ValueError("actualC executable changed")
    actual=[json.loads(line,object_pairs_hook=audit.unique_keys) for line in p.stdout.splitlines()]
    if len(actual)!=len(requests):raise ValueError("actualC vectors missing/extra")
    for index,((row,mode,cut),observed) in enumerate(zip(requests,actual)):
        expected=row["source_checkpoints"] if not mode else row["source_checkpoints"][:cut+1]
        if observed["case_id"]!=index or observed["status"]!=0 or observed["begin_result"]!=0 or len(observed["checkpoints"])!=len(expected):
            raise ValueError(f"actualC begin/checkpoint/owner differs: {row['slot']}/{row['case_id']}/mode{mode}: {observed}")
        for point,source in zip(observed["checkpoints"],expected):
            point["args"]=point["args"][:len(source["args"])]
            if point!=source:raise ValueError(f"actualC leaf prefix differs: {row['slot']}/{row['case_id']}/mode{mode}: {point} != {source}")
        finish=row["source_finish"] if not mode else {k:v for k,v in expected[-1].items() if k not in ("op","args")}
        if observed["finish"]!=finish or (not mode and (observed["active"] or not observed["repeat_ack_guard"])) or (mode and not observed["guarded"]):
            raise ValueError(f"actualC finish/cancel/repeatedACK differs: {row['slot']}/{row['case_id']}/mode{mode}")
    return {"verified":True,"probe":str(probe),"probe_sha256":before,"normal_vectors":len(rows),
            "wrong_ACK_HOST_error_vectors":len(requests)-len(rows),"actualC_vectors":len(requests)}


def build(original,ledger,domain_path,image_path,c_probe=None):
    led,callbacks,domain,image,sources,snapshots=converter.load_inputs(original,ledger,domain_path,image_path)
    for path in [Path(__file__),ROOT/"src/tests/s2_callback_source_test.c",ROOT/"src/field_game.c",
                 ROOT/"src/field_common_event_runtime.h",ROOT/"src/field_common_event_projection.h",
                 ROOT/"src/field_loot_flow.h",ROOT/"src/field_event.c"]:
        snapshots[path]=path.read_bytes()
    docs=json.loads(snapshots[ROOT/"data/static/flows/common_event.json"],object_pairs_hook=audit.unique_keys)
    mapping={f"stage.{stage:03d}.action.{slot:03d}":target for stage,slot,target in
             [(12,8,"common.inventory.once"),(3,12,"common.state.once"),(6,26,"common.callback.026"),
              (14,39,"common.loot.child.0"),(16,41,"common.callback.041")]}
    if any(docs.get("bindings",{}).get(k)!=v for k,v in mapping.items()):
        raise ValueError("five exact stage/callback bindings differ")
    field=audit.archive_entries(sources["FDFIELD.DAT"]);rows=[];entries=[]
    for slot,(runtime,definition) in zip(SLOTS,mapping.items()):
        stage=int(runtime.split('.')[1]);metadata=field[stage*3+1][1]
        groups={t[21]:0 for t in audit.parse_metadata(metadata)["templates"]}
        source=domain["slots"][slot];owner=next(f for f in domain["functions"] if f["va"]==source["owner"])
        visited=set();edges={}
        for case in cases(metadata,slot):
            observed=converter.source_trace(image,domain,slot,case,groups)
            visited.update(observed["visited"])
            for branch,target in observed["conditional_edges"]:edges.setdefault(branch,set()).add(target)
            rows.append({"slot":slot,"source_id":f"dispatch:event_callback:{slot:03d}","definition":definition,
                "stage":stage,"case_id":case["id"],"input":case,"source_trace":observed["trace"],
                "source_checkpoints":observed["checkpoints"],"source_finish":observed["finish"]})
        expected=converter.source_cfg_branches(domain,slot)
        if any(sorted(edges.get(branch,()))!=targets for branch,targets in expected.items()):
            raise ValueError(f"original full caller branches not covered: {slot}")
        outside=[address for address in sorted(visited) if not any(c["start"]<=address<c["end"] for c in owner["chunks"])]
        entries.append({"source_id":f"dispatch:event_callback:{slot:03d}","runtime_id":runtime,"definition":definition,
            "target":source["target"],"owner":source["owner"],
            "complete_owner_chunks":[{k:c[k] for k in ("start","end","sha256")} for c in owner["chunks"]],
            "explicit_nonowner_instructions_executed":outside,
            "original_conditional_edges":{hex(k):[hex(t) for t in sorted(v)] for k,v in edges.items()},
            "all_original_conditionals_both_edges_executed":True})
    if not any(r["slot"]==41 and r["source_trace"]==[("story",[3]),("loot_child",[1,-1]),("story",[4])] for r in rows):
        raise ValueError("35346 complete original shared-tail argument4 not independently observed")
    actual_c={"verified":False}
    if c_probe:
        actual_c["probe_sha256"]=converter.digest(Path(c_probe).read_bytes())
        try:actual_c=verify_c(c_probe,original,rows)
        except ValueError as error:actual_c["error"]=str(error)
    result={"schema":"fd2-s3-callback-binding-source-v1","original_CPU_executed":True,
        "inputs_sha256":{str(p):converter.digest(raw) for p,raw in snapshots.items()},
        "sources":entries,"normal_original_vectors":len(rows),"cases":rows,
        "actual_C":actual_c,
        "S2_refinement":{"source_id":"dispatch:event_callback:041","previous_frozen_classification":"existing_definition_reference",
            "previous_definition":"common.loot.child.1","current_definition":"common.callback.041",
            "reason":"353ae pushes4 then jumps3530d; it never executes352ca's push11; old G mechanism graph remains unchanged"},
        "substitutes":["text/renderer/keyboard","1aa1d mode0 child present and full-bag choice=Cancel; actualC uses real Loot business",
            "1b8a6 raw inventory count and1bb8c first empty append","344f2 masked AI writer"],
        "claims":{"new_GUI_executed":False,"all_original_timing_RNG_pixels":False,"natural_stage_reachability":False}}
    if any(p.read_bytes()!=raw for p,raw in snapshots.items()):raise ValueError("source inputs changed during original/C comparison")
    return result


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--original",type=Path,required=True);p.add_argument("--ledger",type=Path,required=True)
    p.add_argument("--domain",type=Path,required=True);p.add_argument("--image",type=Path,required=True)
    p.add_argument("--c-probe",type=Path);p.add_argument("--output",type=Path,required=True);a=p.parse_args()
    try:
        output=ledger_tool.validate_output(a.output,ROOT,a.original)
        result=build(a.original,a.ledger,a.domain,a.image,a.c_probe)
        output.mkdir(parents=True);(output/"comparison.json").write_text(converter.encode(result),encoding="utf8")
        print(json.dumps({"output":str(output),"original_vectors":result["normal_original_vectors"],"actual_C":result["actual_C"]}))
        if a.c_probe and not result["actual_C"]["verified"]:return 1
    except (OSError,ValueError,KeyError,TypeError) as error:p.exit(1,f"S3 callback原caller对照失败：{error}\n")


if __name__=="__main__":sys.exit(main())
