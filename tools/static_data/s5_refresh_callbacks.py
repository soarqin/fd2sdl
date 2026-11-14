"""S5复用旧独立callback CPU/C工具，输出新F证据；不改签S2—S4原件。"""
from pathlib import Path
import argparse
import json
import sys

import convert_stage_callbacks as callbacks
from s5_output import validate_output


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--mode',choices=('caller','templates'),required=True)
    for name in ('original','ledger','domain','image','c-probe','output-dir'):p.add_argument('--'+name,type=Path,required=True)
    args=p.parse_args();output=validate_output(args.output_dir,args.original)
    wrapper={str(Path(__file__)):callbacks.digest(Path(__file__).read_bytes())}
    if args.mode=='caller':
        sys.path.insert(0,str(Path(__file__).parents[1]/'ida'))
        import s3_callback_binding_source as source
        report=source.build(args.original,args.ledger,args.domain,args.image,args.c_probe)
        report['inputs_sha256'].update(wrapper)
        output.mkdir(parents=True);(output/'comparison.json').write_text(callbacks.encode(report),encoding='utf8')
        print(json.dumps({'original_vectors':report['normal_original_vectors'],'actual_C':report['actual_C']}))
        if not report['actual_C']['verified']:return 1
    else:
        result,traces=callbacks.build(args.original,args.ledger,args.domain,args.image)
        comparison=callbacks.verify_c_probe(args.c_probe,args.original,traces)
        result['inputs_sha256'].update(wrapper)
        output.mkdir(parents=True);hashes={}
        for name,value in [('candidates.json',result),('original-traces.json',traces),('C-original-comparison.json',comparison)]:
            raw=callbacks.encode(value).encode();(output/name).write_bytes(raw);hashes[name]=callbacks.digest(raw)
        (output/'manifest.json').write_text(callbacks.encode({'schema':result['schema'],'files':hashes,'summary':result['summary']}),encoding='utf8')
        print(callbacks.encode({'source':result['summary'],'C_normal':comparison['normal_cases'],
            'C_wait':sum(r['mode'] in (1,2,3) for r in comparison['cases']),
            'C_capacity':comparison['capacity_refusal_cases']}),end='')
    return 0


if __name__=='__main__':sys.exit(main())
