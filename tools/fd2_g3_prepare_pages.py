"""G3 准备服务的原资源页面判据；不读取 SDL 表、状态或日志。

272d0 商品、2810b 成员、2825b/28632 装备预览、2a0c2/2a694
教会候选及 1956b/16559/15f84 消息。只核声明 ROI，不声称全帧等价。
"""
from __future__ import annotations

import struct
from PIL import Image

import fd2_g3_prepare_oracle as model
from fd2_item_list_oracle import raw_icon, number as small_number
from fd2_magic_list_oracle import glyphs, fragment, number
from fd2_reward_oracle import common_tokens
from fd2_transition_oracle import resources as town_resources
from run_original_battle_menu import match_indexed
from run_original_comparison import archive_entry, decode_image
from run_original_end_turn import decode_lmi
from run_original_player import exact_tile


def member(sheet, index):
    if sheet[:6] == b'LLLLLL':
        return archive_entry(sheet, index)
    if sheet[:4] != b'LMI1' or len(sheet) < 6:
        raise ValueError('G3 原服务目录格式不同')
    count = struct.unpack_from('<H', sheet, 4)[0]
    if not 0 <= index < count or 6 + count * 4 > len(sheet):
        raise ValueError('G3 原服务资源索引越界')
    start = struct.unpack_from('<I', sheet, 6 + index * 4)[0]
    end = struct.unpack_from('<I', sheet, 6 + (index + 1) * 4)[0] if index + 1 < count else len(sheet)
    if not 6 + count * 4 <= start < end <= len(sheet):
        raise ValueError('G3 原服务资源边界不同')
    return sheet[start:end]


def _image(data, palette, *, lmi=False, size=None, raw=False):
    if raw:
        width, height = struct.unpack_from('<HH', data)
        if len(data) != 4 + width * height:
            raise ValueError('G3 raw 页长不同')
        result = Image.frombytes('P', (width, height), data[4:])
    else:
        result = (decode_lmi if lmi else decode_image)(data)
    if size and result.size != size:
        raise ValueError('G3 原资源尺寸不同')
    result.putpalette(palette)
    return result


def _paste(canvas, image, xy, *, zero=False):
    canvas.paste(image, xy, image.point(lambda v: 255 if v else 0, mode='L') if zero else None)


def load(original):
    assets = town_resources(original)
    other = (original / 'FDOTHER.DAT').read_bytes()
    assets.update(other=other, info=archive_entry(other, 5), icons=(original / 'FDICON.B24').read_bytes(),
        portraits=(original / 'DATO.DAT').read_bytes())
    executable = (original / 'FD2.EXE').read_bytes()
    assets['service_portraits'] = model.source_range(executable, 0x52387, 6)
    assets['message_table'] = model.source_range(executable, 0x5238d, 12)
    assets['buy_tables'] = model.source_range(executable, 0x52428, 48)
    assets['sell_tables'] = model.source_range(executable, 0x52458, 24)
    assets['services'] = {}
    for selector, bank in ((0,13),(1,12),(4,14)):
        sheet = archive_entry(other, bank)
        assets['services'][selector] = {'bank':bank, 'sheet':sheet,
            'background':_image(member(sheet,0),assets['palette'],size=(320,200)),
            'page':_image(member(sheet,16),assets['palette'],lmi=True,size=(310,86)),
            'commands':[_image(member(sheet,i),assets['palette'],raw=True,size=(24,20)) for i in range(3,11)],
            'price':_image(member(sheet,15),assets['palette'],raw=True,size=(6,8))}
    return assets


def text(canvas, assets, identity, xy, *, selected=False, dynamic=None, value=None):
    """15f84 本场景实际 -4/-6/-2/-3 控制；-3 为等待边界，不画字。"""
    x,y = xy; origin=x
    def expanded(identity, nested=False):
        for token in common_tokens(assets['text'],identity):
            if token == -1: return
            if token == -4 and dynamic is not None and not nested:
                yield from expanded(dynamic, True)
            elif token == -6 and value is not None and value >= 0:
                yield from (int(ch) for ch in str(value))
            elif token in (-2,-3) or token >= 0: yield token
            else: raise ValueError('G3 消息含未声明控制或动态字段')
    for token in expanded(identity):
        if token == -3: continue
        if token == -2: x=origin;y+=19;continue
        if not 1 <= x <= 304 or not 0 <= y <= 182:
            raise ValueError('G3 消息字形写出完整屏幕')
        glyphs(canvas,assets['font'],[token],x,y,201 if selected else 205)
        x+=16


def notice(assets, identity, *, selector=1, dynamic=None, value=None, xy=(12,119), portrait=None):
    canvas=Image.new('P',(320,200),0);canvas.putpalette(assets['palette'])
    text(canvas,assets,identity,xy,dynamic=dynamic,value=value)
    portrait = assets['service_portraits'][selector] if portrait is None else portrait
    origins=(4283,1707,3939,1398,3644)  # 原1956b switch；未从SDL冷表取期望。
    at=origins[portrait-128] if 128<=portrait<=132 else 36887
    frames=[];entry=archive_entry(assets['portraits'],portrait)
    for phase in (0,3):
        start=struct.unpack_from('<I',entry,4*phase)[0]
        end=struct.unpack_from('<I',entry,4*(phase+1))[0] if phase<3 else len(entry)
        image=_image(entry[start:end],assets['palette'],lmi=True)
        # 16559：普通人物 origin36887 向左；准备服务特殊 origin 正向。
        if at==36887:
            image=image.transpose(Image.Transpose.FLIP_LEFT_RIGHT);position=(at%320-image.width+1,at//320)
        else:position=(at%320,at//320)
        frames.append((image,position,phase))
    return {'text':canvas,'portrait_frames':frames,'fragment':identity,'dynamic':dynamic,'value':value,
        'origin':list(xy),'portrait':portrait,'source_cva':['0x15f84','0x1956b','0x16559']}


def notice_match(frame, expected, *, portrait=True):
    words=match_indexed(frame,expected['text'],(0,0))
    faces=[]
    if words['ok'] and portrait:
        for image,xy,phase in expected['portrait_frames']:
            hit=exact_tile(frame,image,xy)
            if hit:faces.append({'phase':phase,'position':list(xy),'size':list(image.size),**hit})
    if not words['ok'] or (portrait and not faces):return None
    return {k:v for k,v in expected.items() if k not in ('text','portrait_frames')}|{
        'kind':'prepare_notice','text_match':words,'portrait_match':faces[:1],
        'scope':'原字形非零已知点及完整原portrait0/3；未核全帧/逐字等待RNG'}


def menu_match(frame,assets,selector,selected,expected_notice):
    service=assets['services'][selector]
    hits=[match_indexed(frame,image,(201+26*i,169)) for i,image in
        enumerate(service['commands'][2*i+(i==selected)] for i in range(4))]
    # 无portrait/money/command覆盖的原底图ROI；与文字、四command同时成立。
    background=exact_tile(frame,service['background'].crop((240,40,312,88)),(240,40))
    message=notice_match(frame,expected_notice)
    if background and all(h['ok'] for h in hits) and message:
        return {'kind':'prepare_service_menu','selector':selector,'selected':selected,'bank':service['bank'],
            'commands':hits,'background_roi':[240,40,72,48],'notice':message,'source_cva':['0x279bc','0x29daa','0x26ce4']}
    return None


def _base(assets,selector):
    result=Image.new('P',(320,200),0);result.putpalette(assets['palette'])
    _paste(result,assets['services'][selector]['page'],(5,112));return result


def _arrows(canvas,assets,selector,count,first,visible,phase):
    """26eda→27079：完整LMI11/12上、13/14下、17清块；不是列表core的一部分。"""
    if phase not in (0,1):raise ValueError('G3 arrow53F52/2范围不同')
    sheet=assets['services'][selector]['sheet']
    for position,index in (((298,120),11+phase if first else 17),
        ((298,181),13+phase if first+visible<count else 17)):
        _paste(canvas,_image(member(sheet,index),assets['palette'],lmi=True,size=(8,9)),position)


def items(assets,t,ids,selected,*,selling=False,first=0,selector=1,arrow_phase=0):
    result=_base(assets,selector);service=assets['services'][selector];sheet=assets['info']
    for i,identity in enumerate(ids[first:first+6]):
        entry=model.item(t,identity);kind=entry[0];x=10+148*(i%2);y=26*(i//2)
        _paste(result,raw_icon(sheet,59+(0 if kind<21 else 1 if kind<32 else 2),(24,20),assets['palette']),(x,y+119))
        text(result,assets,181+identity,(x+28,y+122),selected=selected==first+i)
        effect=None
        if kind<21:effect=(64,struct.unpack_from('<h',entry,1)[0])
        elif kind<32:effect=(65,struct.unpack_from('<h',entry,5)[0])
        elif kind==32 and entry[13] in (5,11):effect=(66 if entry[13]==5 else 67,struct.unpack_from('<h',entry,14)[0])
        if effect:
            _paste(result,raw_icon(sheet,effect[0],(21,8),assets['palette']),(x+95,y+121))
            small_number(result,sheet,effect[1],x+118,y+121,42,3)
        else:
            _paste(result,_image(member(sheet,41),assets['palette']),(x+95,y+123),zero=True)
        _paste(result,service['price'],(x+95,y+131))
        price=model.word(entry,19);price=3*price//4 if selling else price
        number(result,sheet,price,x+104,y+131,119,5)
    _arrows(result,assets,selector,len(ids),first,6,arrow_phase)
    return result.crop((5,112,315,198))


def _sprite(assets,state,raw,phase):
    ids=list(dict.fromkeys(model.record(state,i)[7] for i in range(state['count'])))
    if raw>=len(ids):return None
    data=assets['icons'];index=12*ids[raw]+phase
    w,h,count=struct.unpack_from('<HHH',data)
    if (w,h)!=(24,24) or not 0<=index<count or 6+4*count>len(data):raise ValueError('G3 FDICON实际目录不同')
    start=struct.unpack_from('<I',data,6+4*index)[0]
    end=struct.unpack_from('<I',data,6+4*(index+1))[0] if index+1<count else len(data)
    if not 6+4*count<=start<end<=len(data):raise ValueError('G3 FDICON原帧边界不同')
    at=start;pixels=bytearray([0]*576)
    for row in range(24):
        col=0
        while col<24:
            if at>=end:raise ValueError('G3 原sprite截断')
            c=data[at];at+=1;n=(c&63)+1;mode=c&192;span=2*n if mode==64 else n
            if col+span>24 or at>end:raise ValueError('G3 原sprite超行')
            if mode==192:pixels[row*24+col:row*24+col+n]=bytes([73])*n
            elif mode==128:
                if at+n>end or 0 in data[at:at+n]:raise ValueError('G3 sprite literal截断或opaque0，不能借零mask')
                pixels[row*24+col:row*24+col+n]=data[at:at+n];at+=n
            elif mode==64:
                if at>=end or data[at]==0:raise ValueError('G3 sprite stride2截断或opaque0')
                for j in range(n):pixels[row*24+col+1+2*j]=data[at]
                at+=1
            else:
                if at>=end or data[at]==0:raise ValueError('G3 sprite run截断或opaque0')
                pixels[row*24+col:row*24+col+n]=bytes([data[at]])*n;at+=1
            col+=span
    if at!=end:raise ValueError('G3 原sprite完整RLE边界不同')
    image=Image.frombytes('P',(24,24),bytes(pixels));image.putpalette(assets['palette']);return image


def preview_values(unit,item_id,t):
    ap,dp,dx=(struct.unpack_from('<h',unit,at)[0] for at in (55,57,62));values=[ap,dp,dx,dx]
    incoming=model.item(t,item_id);effects=struct.unpack_from('<4h',incoming,1)
    for out,offset in enumerate((0,2,1,3)):values[out]+=effects[offset]
    for slot in range(8):
        if unit[10+2*slot]&64:
            old=model.item(t,unit[11+2*slot])
            if (old[0]<=20)!=(incoming[0]<=20):
                effects=struct.unpack_from('<4h',old,1)
                for out,offset in enumerate((0,2,1,3)):values[out]+=effects[offset]
    return values


def roster(assets,t,state,selected,*,kind='members',raws=None,item_id=None,target_id=None,selector=1):
    """当前13夹具只需首屏；scroll/重复ID未初始化目录不在本原生代表范围。"""
    raws=list(range(state['count'])) if raws is None else list(raws)
    if not raws or len(raws)>6 or (kind!='members' and len(raws)>3):
        # 本轮五人普通成员，最多首屏6；装备候选实际有4，首屏取3。
        if kind=='members' or not raws:raise ValueError('G3 原生代表成员页超过已声明首屏')
    result=[]
    for phase in (0,1,2):
        image=_base(assets,selector);service=assets['services'][selector]
        for i,raw in enumerate(raws[:6 if kind=='members' else 3]):
            unit=model.record(state,raw);x=132*(i%2) if kind=='members' else 0;y=26*(i//2) if kind=='members' else 26*i
            sprite=_sprite(assets,state,raw,phase)
            if sprite:_paste(image,sprite,(14+x,117+y),zero=True)
            text(image,assets,unit[8]+1,(40+x,121+y),selected=i==selected)
            if kind=='revive':
                text(image,assets,unit[31]+140,(130,121+y),selected=i==selected)
                text(image,assets,unit[32]+150,(175,121+y),selected=i==selected)
                # 2a0c2：sprintf最少五位，绘制循环只取前五字符，不绘第六位。
                fee=model.word(t['fees'],2*unit[32])*unit[33]
                _paste(image,service['price'],(220,125+y));number(image,assets['info'],int(f'{fee:05d}'[:5]),228,125+y,119,5)
            elif kind=='promote':
                for f,xx in ((unit[32]+150,130),(593,175),(t['promotion'][2*(target_id-32)]+150,239)):
                    text(image,assets,f,(xx,121+y),selected=i==selected)
            elif kind=='equipment':
                new=preview_values(unit,item_id,t);old=struct.unpack_from('<4H',unit,72)
                for j in range(4):
                    xx=122 if j<2 else 196;yy=120+y+(9 if j%2 else 0);bank=31 if old[j]==new[j] else 119 if old[j]>=new[j] else 42
                    _paste(image,_image(member(service['sheet'],18+j),assets['palette'],lmi=True),(xx,yy),zero=True)
                    _paste(image,_image(member(service['sheet'],22),assets['palette'],lmi=True),(xx+(35 if j<2 else 38),yy+1),zero=True)
                    small_number(image,assets['info'],old[j],xx+(15 if j<2 else 18),yy,bank,3)
                    small_number(image,assets['info'],new[j],xx+(43 if j<2 else 46),yy,bank,3)
        for arrow_phase in ((0,1) if len(raws)>(6 if kind=='members' else 3) else (0,)):
            composed=image.copy()
            _arrows(composed,assets,selector,len(raws),0,6 if kind=='members' else 3,arrow_phase)
            result.append(composed.crop((5,112,315,198)))
    return result


def page_match(frame,expected,*,kind,selected,source):
    variants=expected if isinstance(expected,list) else [expected]
    for variant,image in enumerate(variants):
        found=exact_tile(frame,image,(5,112))
        if found:
            phases={'arrow_phase':variant} if kind=='prepare_item_page' else {
                'sprite_phase':variant//2 if len(variants)==6 else variant,
                'arrow_phase':variant%2 if len(variants)==6 else 0}
            return {'kind':kind,'selected':selected,'source_variant':variant,**phases,
                'position':[5,112],'size':[310,86],'checked_pixels':26660,
                'opaque_black_included':True,'source_cva':source,**found}
    return None
