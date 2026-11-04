"""真实START raw1/ID0法术列表的纯资源构造，不运行进程或改写存档。

依据field_player_magic_command_execute @0x1cff0（原登记同名）、
field_unit_magic_list_draw @0x1ceed（原sub_1CEED，仅叠名字/费用）及
field_player_magic_list_select_step @0x1d51d（原sub_1D51D，处理选择）。
只生成已校准的列表/姓名/MP矩形，
不推断玩家施法、恢复术13、原版UI的RNG次数或SDL正式MAGIC已完成。
"""
import hashlib
import struct

from PIL import Image

from run_original_comparison import archive_entry, decode_image
from run_original_end_turn import UNITS, STRIDE, round_assets, validate_initial
from run_original_player import sheet_frame


def fragment(text,index):
    start,end = struct.unpack_from("<HH",text,index*2)
    if not 0 <= start < end <= len(text) or (end-start)%2:
        raise ValueError("text fragment bounds")
    tokens = struct.unpack("<"+"h"*((end-start)//2),text[start:end])
    if tokens[-1] != -1 or any(token<0 for token in tokens[:-1]):
        raise ValueError("only ordinary glyphs and final terminator are supported")
    return tokens[:-1]


def glyphs(canvas,font,tokens,x,y,foreground):
    # 15f84参数a6/a7/a8进入4ed7a：前景205/201，shadow76，背景0不预清。
    for token in tokens:
        if (token+1)*32 > len(font):
            raise ValueError("font bounds")
        if token != 10:
            glyph = font[token*32:(token+1)*32]
            for row in range(16):
                bits = int.from_bytes(glyph[row*2:row*2+2],"big")
                for column in range(16):
                    if bits & (0x8000>>column):
                        px,py = x+column,y+row
                        canvas.putpixel((px,py),foreground)
                        canvas.putpixel((px-1,py+1),76)
                        canvas.putpixel((px,py+1),76)
        x += 16


def digit(sheet,index):
    """沿既有RLE decoder；仅核实所用数字无opaque0，才能以decoded0作skip。"""
    count = struct.unpack_from("<H",sheet,4)[0]
    start = struct.unpack_from("<I",sheet,6+index*4)[0]
    end = struct.unpack_from("<I",sheet,6+(index+1)*4)[0] if index+1<count else len(sheet)
    data = sheet[start:end]
    image = decode_image(data)
    cursor = 4
    for _ in range(image.height):
        x = 0
        while x < image.width:
            control = data[cursor]
            cursor += 1
            mode,length = control&192,(control&63)+1
            if mode != 192:
                size = length if mode==128 else 1
                if 0 in data[cursor:cursor+size]:
                    raise ValueError("digit contains opaque0; nonzero mask would be invalid")
                cursor += size
            x += length*2 if mode==64 else length
    if cursor != len(data) or image.width>6 or image.height!=8:
        raise ValueError("digit dimensions or RLE length")
    return image


def number(canvas,sheet,value,x,y,bank,width):
    for index,character in enumerate(f"{value:0{width}d}"):
        image = digit(sheet,bank+int(character))
        canvas.paste(image,(x+index*6,y),image.point(lambda c:255 if c else 0,mode="L"))


def prepare(original,fixture):
    source,_ = validate_initial(fixture,0)
    actor = source[UNITS+STRIDE:UNITS+2*STRIDE]
    ids = [8*byte+bit for byte in range(5) for bit in range(8) if actor[0x1a+byte]&(1<<bit)]
    if (actor[7],actor[8],actor[0x20],tuple(actor[:2]),ids) != (9,9,5,(10,15),[0]):
        raise ValueError("magic list oracle requires real START raw1/unit9 with only ID0")
    assets = round_assets(original)
    other = (original / "FDOTHER.DAT").read_bytes()
    sheet,font = archive_entry(other,5),archive_entry(other,4)
    text = archive_entry((original / "FDTXT.DAT").read_bytes(),0)
    bottom,top = sheet_frame(sheet,21,lmi=True),sheet_frame(sheet,20,lmi=True)
    if bottom.size != (310,99) or top.size != (223,86) or 0 in bottom.getdata():
        raise ValueError("detail rectangle geometry/opacity changed")
    bottom.putpalette(assets["palette"])
    top.putpalette(assets["palette"])
    count = struct.unpack_from("<H",sheet,4)[0]
    begin,end = struct.unpack_from("<II",sheet,6+92*4)
    raw = sheet[begin:end]
    if count<=93 or struct.unpack_from("<HH",raw)!=(20,8) or len(raw)!=164:
        raise ValueError("MP icon92 raw format changed")
    cost_icon = Image.frombytes("P",(20,8),raw[4:])
    cost_icon.putpalette(assets["palette"])
    name = fragment(text,441)
    names = fragment(text,actor[8]+1)
    result = {}
    for selected in (-1,0):
        canvas = bottom.copy()
        glyphs(canvas,font,name,18-5,103-94,201 if selected==0 else 205)
        canvas.paste(cost_icon,(68-5,108-94))  # 1685c→4ed0b完整raw复制，包含0。
        number(canvas,sheet,2,91-5,108-94,42,2)
        result["selected" if selected==0 else "unselected"] = ((5,94),canvas)
    # 不重建其余动态属性，只返回两个明确、无其它读集覆盖的完整ROI。
    glyphs(top,font,names,99-92,13-7,205)
    mp,maximum = struct.unpack_from("<HH",actor,0x44)
    number(top,sheet,mp,267-92,59-7,31 if mp==maximum else 42,3)
    number(top,sheet,maximum,293-92,59-7,31,3)
    result["actor_name"] = ((98,13),top.crop((98-92,13-7,99-92+16*len(names),13-7+17)))
    result["current_max_mp"] = ((267,59),top.crop((267-92,59-7,311-92,67-7)))
    selected,unselected = result["selected"][1],result["unselected"][1]
    changed = sum(a!=b for a,b in zip(selected.getdata(),unselected.getdata()))
    if not changed or any(a not in (201,205) or b not in (201,205)
                          for a,b in zip(selected.getdata(),unselected.getdata()) if a!=b):
        raise AssertionError("selection must only recolor glyph foreground")
    return result,{"actor":1,"unit_id":9,"ids":ids,"spell_fragment":441,
                   "spell_tokens":list(name),"actor_fragment":actor[8]+1,"actor_tokens":list(names),
                   "current_mp":mp,"max_mp":maximum,"cost":2,"selection_changed_pixels":changed,
                   "list_bottom":[5,94,310,99],"magic_text_title":None,
                   "source_sha256":hashlib.sha256(fixture.read_bytes()).hexdigest(),
                   "runtime_rng_ui_consumption":"16c57 shared-RNG reader confirmed statically; actual count/alignment unverified",
                   "acceptance":False}
