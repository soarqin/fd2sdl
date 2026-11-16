"""G4共享文本/选择框写掩码源域，仅读原DAT，不导入SDL冷定义或运行窗口。

15f84的动态名固定UI0；11cac/4e22a选择框原RLE的SKIP与opaque0需区分。
本结果是资源结构/字节取证，不是原版运行或SDL呈现通过。
"""
import argparse
import hashlib
import json
import struct
from collections import Counter
from pathlib import Path

TEXT_SHA = "b47e39ee727482528da061a8705f5cdee8a9cf759447aeda298afe17d9dd8bcc"
OTHER_SHA = "882580dfa31bd22e069eb151b7a86a2b8dbcd8194ec755b00036c96a4a0e0667"
NAME_RANGES = ((1,136),(150,178),(181,395),(441,476))


def archive_entries(data):
    if len(data)<10 or data[:6]!=b"LLLLLL":
        raise ValueError("归档头截断")
    table_end=struct.unpack_from("<I",data,6)[0]
    if table_end<10 or table_end>=len(data):
        raise ValueError("归档偏移表截断")
    offsets=[]
    for at in range(6,table_end-3,4):
        offset=struct.unpack_from("<I",data,at)[0]
        if offset>=len(data) or (offsets and offset<=offsets[-1]):break
        if offset<table_end:raise ValueError("归档条目落入目录")
        offsets.append(offset)
    if not offsets:raise ValueError("归档目录为空")
    return [data[a:b] for a,b in zip(offsets,(*offsets[1:],len(data)))]


def text_fragments(data):
    if len(data)<2:
        raise ValueError("文本头截断")
    first=struct.unpack_from("<H",data)[0]
    if first<2 or first%2 or first>len(data):
        raise ValueError("文本偏移表非法")
    offsets=struct.unpack_from("<"+"H"*(first//2),data)
    if offsets[0]!=first or any(a>b for a,b in zip(offsets,offsets[1:])):
        raise ValueError("文本偏移逆序")
    ends=(*offsets[1:],len(data))
    result=[]
    for a,b in zip(offsets,ends):
        if b>len(data) or b<a or (b-a)%2:
            raise ValueError("文本片段越界")
        tokens=struct.unpack_from("<"+"h"*((b-a)//2),data,a)
        if -1 not in tokens:
            raise ValueError("文本缺终止符")
        result.append(tokens[:tokens.index(-1)+1])
    return result


def rle_writes(width,height,stream):
    """返回明确opaque写(mask)，0也是实写；不以解码后color0猜mask。"""
    if not 0<width<=4096 or not 0<height<=4096:
        raise ValueError("图像尺寸非法")
    at=0; writes=[]
    for y in range(height):
        x=0
        while x<width:
            if at>=len(stream):
                raise ValueError("RLE控制截断")
            control=stream[at];at+=1
            count=(control&63)+1;mode=control&192
            span=count*(2 if mode==64 else 1)
            if x+span>width:
                raise ValueError("RLE跨行")
            if mode==128:
                if at+count>len(stream):
                    raise ValueError("RLE literal截断")
                writes.extend((x+i,y,stream[at+i]) for i in range(count));at+=count
            elif mode<128:
                if at>=len(stream):
                    raise ValueError("RLE run截断")
                writes.extend((x+(2*i+1 if mode==64 else i),y,stream[at])
                              for i in range(count));at+=1
            x+=span
    if at!=len(stream):
        raise ValueError("RLE有多余尾部")
    return writes


def text_syntax_inventory(entries, glyph_count):
    """15f84语法参数账；speaker参数不当字形，不代caller可达性或呈现。"""
    if not glyph_count:
        raise ValueError("字形分母为空")
    rows=[];total=Counter()
    for entry,fragments in enumerate(entries):
        for fragment,tokens in enumerate(fragments):
            counts=Counter();speakers=[];glyphs=0;at=0;ended=False
            while at<len(tokens):
                token=tokens[at];at+=1
                if token>=0:
                    if token>=glyph_count:raise ValueError(f"字形越源：{entry}/{fragment}/{token}")
                    glyphs+=1;continue
                if token not in (-1,-2,-3,-4,-5,-6,-17,-18,-19,-20):
                    raise ValueError(f"未知文本控制：{entry}/{fragment}/{token}")
                counts[str(token)]+=1
                if token==-1:
                    if at!=len(tokens):raise ValueError("已解析片段的END后仍有token")
                    ended=True;break
                if token in (-17,-18,-19,-20):
                    if at>=len(tokens):raise ValueError("speaker缺参数")
                    speakers.append({'control':token,'argument':tokens[at]&65535,
                        'kind':'text_id' if token in (-17,-18) else 'raw_index'})
                    at+=1
            if not ended:raise ValueError("speaker参数不能代替片段END")
            total.update(counts)
            raw=struct.pack('<'+'h'*len(tokens),*tokens)
            rows.append({'entry':entry,'fragment':fragment,'token_sha256':hashlib.sha256(raw).hexdigest(),
                'token_count':len(tokens),'glyphs':glyphs,'controls':dict(counts),'speakers':speakers})
    return {'entries':len(entries),'fragments':len(rows),'glyph_count':glyph_count,
        'literal_glyph_tokens':sum(row['glyphs'] for row in rows),'controls':dict(total),'rows':rows,
        'scope':'First-END token syntax only; suffix bytes, dynamic values, caller reachability and pixels are separate',
        'runtime_or_pixel_equivalence_claimed':False}


def audit(original):
    text=(original/"FDTXT.DAT").read_bytes()
    other=(original/"FDOTHER.DAT").read_bytes()
    if hashlib.sha256(text).hexdigest()!=TEXT_SHA or hashlib.sha256(other).hexdigest()!=OTHER_SHA:
        raise ValueError("原资源SHA不匹配已证基线")
    entries=[text_fragments(entry) for entry in archive_entries(text)]
    dynamic=[]
    for entry,fragments in enumerate(entries):
        for fragment,tokens in enumerate(fragments):
            controls=[value for value in tokens if value in (-4,-5,-6)]
            if controls:dynamic.append({"entry":entry,"fragment":fragment,"controls":controls})
    names=[]
    for lo,hi in NAME_RANGES:
        for fragment in range(lo,hi+1):
            if any(value<0 and value!=-1 for value in entries[0][fragment]):
                raise ValueError(f"正常名称{fragment}含非终止控制符")
            names.append(fragment)
    other_entries=archive_entries(other)
    font=other_entries[4]
    if len(font)%32:raise ValueError("字模owner不是32B整倍数")
    syntax=text_syntax_inventory(entries,len(font)//32)
    cursor=other_entries[1]
    width,height,count=struct.unpack_from("<HHH",cursor)
    offsets=struct.unpack_from("<"+"I"*count,cursor,6)
    frames=[]
    for index,(a,b) in enumerate(zip(offsets,(*offsets[1:],len(cursor)))):
        writes=rle_writes(width,height,cursor[a:b])
        frames.append({"index":index,"source_span":[a,b],"opaque_count":len(writes),
                       "opaque_zero_count":sum(value==0 for _,_,value in writes)})
    return {"schema":"fd2.g4.render-source.v1","resource_sha256":{
        "FDTXT.DAT":TEXT_SHA,"FDOTHER.DAT":OTHER_SHA},"text_entry_count":len(entries),
        "dynamic_fragments":dynamic,"all_dynamic_in_ui0":all(x["entry"]==0 for x in dynamic),
        "text_syntax_inventory":syntax,
        "normal_name_ranges":NAME_RANGES,"plain_name_count":len(names),
        "cursor_dimensions":[width,height,count],"cursor_frames":frames,
        "cursor_decoded_zero_is_skip":all(x["opaque_zero_count"]==0 for x in frames),
        "runtime_or_pixel_equivalence_claimed":False}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original",type=Path,default=Path("original_game"))
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args()
    result=audit(args.original)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf8")
    print(json.dumps({"output":str(args.output),"dynamic_fragments":len(result["dynamic_fragments"]),
                      "cursor_zero_is_skip":result["cursor_decoded_zero_is_skip"]},ensure_ascii=False))


if __name__=="__main__":main()
