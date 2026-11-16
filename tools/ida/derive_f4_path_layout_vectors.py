#!/usr/bin/env python3
"""Existing full 4e4f6 family -> finite source-derived mode/output vectors.

No original CPU/C execution. Reuses the shallow source guard and initial map
writer; only adds the exact index32 mode alias and shared output/result order.
Does not model the 60181+ table or qualify original caller output extent.
"""
import argparse
import importlib.util
import json
from pathlib import Path

BASE=Path(__file__).with_name("derive_f4_movement_vectors.py")
spec=importlib.util.spec_from_file_location("movement_source_base",BASE)
base=importlib.util.module_from_spec(spec)
spec.loader.exec_module(base)


class OutputLimit(Exception):
    pass


def expected(case):
    w,h=case["width"],case["height"]
    if min(case["costs"])<1:
        raise ValueError("this finite source qualification requires positive costs")
    mode=case["mode"]&3
    cells=base.marked(dict(case,apply=0))
    if case["mode"]&4:
        cells=[(x&0x001f03ff)|0xff000000 for x in cells]
    _,sx,sy,_=case["units"][0]
    cells[sy*w+sx]=(cells[sy*w+sx]&0xffffff)|(case["budget"]<<24)
    output=[0xa5]*64
    code=255
    written=0
    route=[]
    max_frames=0
    aliases=[]
    writes=[]

    def write(offset,value,pc):
        nonlocal written
        if offset>=case["capacity"]:
            raise OutputLimit((pc,offset))
        output[offset]=value
        written=max(written,offset+1)
        writes.append([pc,offset,value])

    def target(x,y):
        nonlocal code
        if [x,y]!=case["target"]:
            return
        # 4e761 uses BYTE60077; 4e76f precedes any output-byte copy.
        length=len(route)&255
        if length>code:
            return
        code=length
        for i in range(length):
            write(i,route[i],"4e78a")

    def visit(x,y,remaining):
        nonlocal mode,code,max_frames
        max_frames=max(max_frames,len(route)+1)
        if len(route)==32:
            aliases.append(dict(depth=32,x=x,y=y,before=mode,after=y))
            mode=y  # 4e5ce writes DH at6017a; pop has no mode restore.
        for dx,dy,direction in base.NEIGHBORS:
            nx,ny=x+dx,y+dy
            if not (0<=nx<w and 0<=ny<h):
                continue
            k=ny*w+nx
            cell=cells[k]
            cost=case["costs"][case["classes"][cell&0x3ff]]
            if cost>remaining:
                continue
            value=remaining-cost
            route.append(direction)
            runs=1+sum(a!=b for a,b in zip(route,route[1:]))
            turn=(runs*4)&255
            old=cell>>24
            if base.signed(value)<base.signed(old) or (value==old and
                (mode!=1 or turn<=((cell>>8)&252))):
                route.pop()
                continue
            cell=(cell&~0xfc00)|(turn<<8)  # 4e6d2 before40 rejection.
            cells[k]=cell
            if mode==2:
                cells[k]=(cell&0xffffff)|(value<<24)
                if cell&0x400000:
                    write(0,nx,"4e711")
                    write(1,ny,"4e713")
                    code=1  # 4e716 follows both stores.
            else:
                if cell&0x400000:
                    route.pop()
                    continue
                if cell&0x800000:
                    value=0
                cells[k]=(cell&0xffffff)|(value<<24)
                target(nx,ny)  # 4e6fa BEFORE child's4e5ce mode store.
            visit(nx,ny,value)
            route.pop()

    status=0
    first_error=None
    try:
        target(sx,sy)  # 4e5b8 is unconditional, also requested mode2.
        visit(sx,sy,case["budget"])
    except OutputLimit as error:
        status=-2
        first_error=list(error.args[0])
    witness=(case["mode"]==2)
    typed_found=int(code!=255 and (not witness or written>=2))
    return dict(status=status,result=code,mode=mode,written=written,output=output,
                cells=cells,max_frames=max_frames,aliases=aliases,writes=writes,
                first_error=first_error,typed_found=typed_found,
                typed_length=0 if witness or code==255 else code,
                typed_xy=output[:2] if witness and typed_found else [-1,-1])


def cases():
    rows=[]
    def corridor(name,y,budget,mode,target,enemy=False,capacity=64):
        w=35
        h=max(2,y+1)
        cells=[int(row!=y) for row in range(h) for _ in range(w)]
        units=[[2,0,y,0]]+([[0,0,y-1,0]] if enemy else [])
        row=dict(name=name,width=w,height=h,budget=budget,mode=mode,target=target,
                 cells=cells,classes=[0,1],costs=[1,20],units=units,capacity=capacity)
        row["expected"]=expected(row)
        rows.append(row)
    for mode in (0,1,2):
        corridor(f"before32-{mode}",2,31,mode,[34,2],True)
        corridor(f"mode32-{mode}",2,32,mode,[34,2],True)
    corridor("target-before-mode-store",2,32,0,[32,2])
    corridor("directions-then-XY",2,32,0,[32,2],True)
    corridor("requested2-to-normal0",0,33,2,[33,0])
    corridor("requested2-to-equal1",1,33,2,[33,1])
    corridor("requested1-to-normal0",0,32,1,[32,0])
    corridor("requested0-to-other3",3,32,0,[32,3])
    corridor("fixed28-clearflags",1,28,5,[32,1])
    corridor("witness-H-capacity1",2,32,0,[34,2],True,1)
    corridor("target-H-capacity31",2,32,0,[32,2],False,31)
    root=dict(name="root-mode2-result0-no-bytes",width=1,height=1,budget=0,mode=2,
              target=[0,0],cells=[0],classes=[0],costs=[1,20],units=[[2,0,0,0]],capacity=64)
    root["expected"]=expected(root)
    rows.append(root)
    by={r["name"]:r["expected"] for r in rows}
    assert by["before32-0"]["result"]==255 and by["before32-0"]["mode"]==0
    assert by["mode32-0"]["result"]==1 and by["mode32-0"]["output"][:2]==[0,1]
    assert by["mode32-0"]["max_frames"]==33
    assert by["target-before-mode-store"]["result"]==32
    assert by["target-before-mode-store"]["output"][:32]==[3]*32
    assert by["directions-then-XY"]["result"]==1
    assert by["directions-then-XY"]["output"][:4]==[0,1,3,3]
    assert by["requested2-to-equal1"]["output"][:33]==[3]*33
    assert by["witness-H-capacity1"]["result"]==255
    assert by["target-H-capacity31"]["result"]==32 and by["target-H-capacity31"]["mode"]==0
    return rows


def header(rows):
    def nums(v):return "{"+",".join(str(x)+"u" for x in v)+"}"
    lines=["/* Source-derived by derive_f4_path_layout_vectors.py; no C/original execution. */",
           "#ifndef FD2_FIELD_PATH_LAYOUT_VECTORS_H","#define FD2_FIELD_PATH_LAYOUT_VECTORS_H",
           "typedef struct { const char *name; unsigned width,height,budget,mode,capacity,unit_count;",
           " uint8_t units[2][4]; uint32_t cells[160]; int tx,ty,status;",
           " unsigned result,final_mode,written; uint8_t output[64];",
           " int typed_found,wx,wy; unsigned typed_length; } field_path_layout_vector;",
           "static const field_path_layout_vector k_path_layout[] = {"]
    for r in rows:
        e=r["expected"]
        units="{"+",".join(nums(x) for x in r["units"])+"}"
        values=[f'"{r["name"]}"',r["width"],r["height"],r["budget"],r["mode"],r["capacity"],len(r["units"]),
                units,nums(r["cells"]),*r["target"],e["status"],e["result"],e["mode"],e["written"],
                nums(e["output"]),e["typed_found"],*e["typed_xy"],e["typed_length"]]
        lines.append(" {"+",".join(map(str,values))+"},")
    lines += ["};","#endif",""]
    return "\n".join(lines)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source",type=Path,required=True)
    parser.add_argument("--image",type=Path,required=True)
    parser.add_argument("--movement",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args()
    inputs=[args.source,args.image,args.movement,BASE,Path(__file__)]
    before=[base.input_id(p) for p in inputs]
    functions=base.source_guard(args.source,args.image)
    movement=json.loads(args.movement.read_text())["tables"][0]["values"]
    raw=bytes(x for row in movement for x in row)
    if len(raw)!=580 or raw!=args.image.read_bytes()[0x61646:0x6188a] or movement[1][:2]!=[1,20]:
        raise ValueError("fixed profile1 class0/1 source differs")
    rows=cases()
    args.output.mkdir(parents=True,exist_ok=False)
    (args.output/"vectors.json").write_text(json.dumps(dict(schema="fd2-path-mode-output-v1",vectors=rows),indent=2)+"\n",encoding="utf8")
    (args.output/"field_path_layout_vectors.h").write_text(header(rows),encoding="utf8",newline="\n")
    after=[base.input_id(p) for p in inputs]
    if before!=after:raise ValueError("late source identity differs")
    receipt=dict(ok=True,functions=functions,cases=len(rows),max_frames=max(r["expected"]["max_frames"] for r in rows),
        before=before,after=after,claims=dict(static_source_model=True,new_original_CPU=False,C_execution=False,
        natural_producer=False,original_output_extent=False,full_source_layout=False,global_table_collateral=False))
    (args.output/"receipt.json").write_text(json.dumps(receipt,indent=2)+"\n",encoding="utf8")
    print(json.dumps({k:v for k,v in receipt.items() if k not in ("before","after","functions")}))


if __name__=="__main__":main()
