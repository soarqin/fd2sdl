"""G2已知Item正式路径定向回归；复用B3原源有限结果，补独立cache/来源/显示守卫。"""
import argparse
import json
from pathlib import Path
from types import SimpleNamespace

import fd2_g2_battle_oracle as oracle
from fd2_display_requirement import windows_display_requirement
from run_g2_battle_check import freeze,unchanged,sha
from run_graphics_tests import decode_save
from run_original_battle_animation import owned_job_required
import run_original_item_list as item

SCENARIOS=("use-05","use-19","use-20")


def check_cache(source,measured,engine):
    oracle.valid_plain(source);oracle.valid_plain(measured)
    if engine not in ("original","sdl") or source[oracle.META+1]!=measured[oracle.META+1]:
        raise ValueError("G2 Item cache owner/count不符")
    classes=[];rows=[]
    for raw in range(source[oracle.META+1]):
        original=oracle.actor(source,raw);actual=oracle.actor(measured,raw)
        if original[7]!=actual[7]:
            raise ValueError("G2 Item夹具无身份变化，不能借cache豁免接受另一unit")
        if original[7] not in classes:classes.append(original[7])
        expected=classes.index(original[7]) if engine=="original" else original[2]
        rows.append({"raw":raw,"expected":expected,"measured":actual[2]})
    return {"ok":all(row["expected"]==row["measured"] for row in rows),"rows":rows,
        "basis":"original10010 current raw-ID ordered class rebuild; SDL preserves original80B and rebuilds side cache"}


def run(args):
    owned_job_required()
    root=args.root.resolve();original=(root/"original_game").resolve();output=args.artifacts.resolve()
    if output==original or output.is_relative_to(original) or output.exists():
        raise ValueError("G2 Item需要新的独立产物目录")
    binaries=[args.sdl.resolve(),args.driver.resolve(),args.dosbox.resolve()]
    inputs,before,sources=freeze(root,original,binaries)
    output.mkdir(parents=True)
    report={"ok":False,"scenario":args.scenario,"inputs":inputs,"source_files":sources,
        "original_before":before,"display_requirement":{},"full_animation_RGB_or_audio_equivalence_claimed":False}
    try:
        with windows_display_requirement(report["display_requirement"]):
            result=item.run(SimpleNamespace(root=root,driver=args.driver,dosbox=args.dosbox,sdl=args.sdl,
                scenario=args.scenario,artifacts=output/"paired"))
            report["paired_report"]=str(output/"paired/comparison.json")
            if not result["ok"]:
                raise AssertionError("G2 Item双端正式路径未通过："+str(result.get("error")))
            source=decode_save(output/"paired"/(args.scenario+".SAV"))
            report["cache_checks"]=[]
            for case in result["cases"]:
                snapshot=Path(case["completed_save"]["snapshot"])
                if sha(snapshot)!=case["completed_save"]["sha256"]:
                    raise AssertionError("G2 Item已核快照身份变化")
                check=check_cache(source,decode_save(snapshot),case["kind"])
                report["cache_checks"].append({"kind":case["kind"],**check})
                if not check["ok"]:raise AssertionError("G2 Item cache不符合独立来源")
            report["ok"]=len(report["cache_checks"])==2
    except BaseException as error:
        report["error"]={"type":type(error).__name__,"message":str(error)}
    finally:
        report["final_sources_unchanged"]=unchanged(inputs,original,before,root,sources)
        report["ok"] &= report["final_sources_unchanged"] and report["display_requirement"].get("released") is True
        (output/"comparison.json").write_text(json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ("root","sdl","driver","dosbox","artifacts"):parser.add_argument("--"+name,type=Path,required=True)
    parser.add_argument("--scenario",choices=SCENARIOS,required=True)
    args=parser.parse_args()
    report=run(args);print(json.dumps({"ok":report["ok"],"scenario":args.scenario},ensure_ascii=False))
    return 0 if report["ok"] else 1


if __name__=="__main__":raise SystemExit(main())
