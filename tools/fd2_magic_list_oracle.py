"""真实START raw1及其明示位图/坐标派生的法术列表资源，不运行进程或改写存档。

依据field_player_magic_command_execute @0x1cff0（原登记同名）、
field_unit_magic_list_draw @0x1ceed（原sub_1CEED，仅叠名字/费用）及
field_player_magic_list_select_step @0x1d51d（原sub_1D51D，处理选择）。
只构造列表/姓名/MP和受控0/13详情数值矩形，不模拟施法或成长，
不推断恢复术13自然学习、原版UI的RNG次数或SDL正式MAGIC已完成。
"""
import hashlib
import struct

from PIL import Image

from run_original_comparison import archive_entry, decode_image
from run_original_end_turn import UNITS, STRIDE, round_assets, validate_initial
from run_original_player import sheet_frame

# field_magic_record_get @0x4e866：canonical 0x619fd + id*7 的 record+5。
# 只服务真实单项和controls五项夹具，不是通用魔法表或施法支持声明。
SPELL_COSTS = {0:2,1:6,2:20,3:42,13:3}


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


def prepare(original,fixture,*,source=None):
    if source is None:
        source,_ = validate_initial(fixture,0)
    actor = source[UNITS+STRIDE:UNITS+2*STRIDE]
    ids = [8*byte+bit for byte in range(5) for bit in range(8) if actor[0x1a+byte]&(1<<bit)]
    if ((actor[7],actor[8],actor[0x20]) != (9,9,5) or
            (tuple(actor[:2]),ids) not in (((10,15),[0]),((10,15),[0,13]),((10,15),[0,1,2,3,13]),((7,16),[0]))):
        raise ValueError("magic list oracle requires START raw1/unit9 or an exact controls/damage fixture")
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
    spell_names = [fragment(text,441+magic_id) for magic_id in ids]
    names = fragment(text,actor[8]+1)
    result = {}
    for selected in (-1,*range(len(ids))):
        canvas = bottom.copy()
        for index,(magic_id,name) in enumerate(zip(ids,spell_names)):
            x,y = 18+100*(index//4),103+22*(index%4)
            glyphs(canvas,font,name,x-5,y-94,201 if selected==index else 205)
            canvas.paste(cost_icon,(x+50-5,y+5-94))  # 1685c→4ed0b完整raw复制，包含0。
            number(canvas,sheet,SPELL_COSTS[magic_id],x+73-5,y+5-94,42,2)
        key = "unselected" if selected==-1 else "selected" if selected==0 else f"selected_{selected}"
        result[key] = ((5,94),canvas)
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
                   "spell_tokens":list(spell_names[0]),"spell_fragments":[441+magic_id for magic_id in ids],
                   "costs":[SPELL_COSTS[magic_id] for magic_id in ids],
                   "actor_fragment":actor[8]+1,"actor_tokens":list(names),
                   "current_mp":mp,"max_mp":maximum,"cost":2,"selection_changed_pixels":changed,
                   "list_bottom":[5,94,310,99],"magic_text_title":None,
                   "source_sha256":hashlib.sha256(fixture.read_bytes()).hexdigest(),
                   "runtime_rng_ui_consumption":"16c57 shared-RNG reader confirmed statically; actual count/alignment unverified",
                   "acceptance":False}


def caster_values(original,fixture,source,*,mp=6,experience=42):
    """17fc0（field_unit_detail_stats_draw，原登记同名）：只构造MP/姓名/LV/EX ROI。

    受控unit9以ID0击杀LV2 enemy96：1c81f累计2*21，18d8c除LV1，
    恢复raw0则1c916累计22*40*LV1/42=20；1e292（原sub_1E292）
    由EX0写42/20，均不升级；不模拟整套成长或FIGANI。
    """
    expected = bytearray(source)
    actor = UNITS+STRIDE
    if expected[actor+7]!=9 or expected[actor+0x21]!=1 or expected[actor+0x3c]!=0:
        raise ValueError("施法详情仅支持已确认unit9 LV1/EX0起点")
    if (mp,experience) not in ((6,42),(5,20)):
        raise ValueError("仅构造已确认的ID0击杀/ID13恢复详情终值")
    struct.pack_into("<H",expected,actor+0x44,mp)
    expected[actor+0x3c] = experience
    templates,_ = prepare(original,fixture,source=expected)
    sheet = archive_entry((original/"FDOTHER.DAT").read_bytes(),5)
    top = sheet_frame(sheet,20,lmi=True)
    top.putpalette(templates["current_max_mp"][1].getpalette())
    number(top,sheet,1,157-92,33-7,42,2)
    number(top,sheet,experience,157-92,44-7,42,2)
    return {"actor_name":templates["actor_name"],"current_max_mp":templates["current_max_mp"],
            "level":((157,33),top.crop((157-92,33-7,169-92,41-7))),
            "experience":((157,44),top.crop((157-92,44-7,169-92,52-7)))}


def healed_recipient_values(original,source):
    """17fc0的姓名/HP矩形：受控raw0由20恢复到42，不模拟恢复RNG。"""
    unit = source[UNITS:UNITS+STRIDE]
    if unit[7:9]!=b"\0\0" or struct.unpack_from("<HH",unit,0x40)!=(20,42):
        raise ValueError("heal目标必须是明确受伤的START raw0")
    other = (original/"FDOTHER.DAT").read_bytes()
    sheet,font = archive_entry(other,5),archive_entry(other,4)
    text = archive_entry((original/"FDTXT.DAT").read_bytes(),0)
    top = sheet_frame(sheet,20,lmi=True)
    top.putpalette(round_assets(original)["palette"])
    names = fragment(text,unit[8]+1)
    glyphs(top,font,names,99-92,13-7,205)
    number(top,sheet,42,267-92,41-7,31,3)
    number(top,sheet,42,293-92,41-7,31,3)
    return {"actor_name":((98,13),top.crop((98-92,13-7,99-92+16*len(names),30-7))),
            "current_max_hp":((267,41),top.crop((267-92,41-7,311-92,49-7)))}
